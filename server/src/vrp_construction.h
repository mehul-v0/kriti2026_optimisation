#ifndef VRP_CONSTRUCTION_H
#define VRP_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <iostream>
#include <limits>
#include <cmath>
#include <algorithm>
#include <random>

// Different ordering strategies for construction heuristic
enum OrderingStrategy {
    EARLIEST_DEADLINE,      // Original: tightest deadline first
    LATEST_DEADLINE,        // Relaxed deadlines first (counter-intuitive but may work)
    GEOGRAPHIC_CLUSTER,     // Nearest to office first
    PRIORITY_BASED,         // By employee priority
    RANDOM_ORDER,           // Random for diversity
    CHEAPEST_VEHICLE_FIRST, // Maximize trips on cheapest vehicles
    DOLLAR_COST_AWARE,      // Direct dollar cost minimization (inspired by OR-Tools)
    PREFERENCE_PRIORITY,    // Vehicle preference first, then cheapest
    GEO_CLUSTER_CONSOL,     // cluster employees by proximity, assign clusters to cheapest vehicles
    DEPOT_PROXIMITY,        // Assign to physical vehicle with closest depot (uses more vehicles)
    ANGULAR_SECTOR          // Proper angular sector partitioning for many-to-one (Vansteenwegen 5.2.1.3)
};

class ParallelCheapestInsertion {
public:
    static void build(std::vector<std::vector<int>>& routes,
                      const std::vector<Employee>& emps,
                      const std::vector<Vehicle>& virt_vehs,
                      const ConstraintEngine& cp, bool enforce_soft, const Metadata& meta,
                      OrderingStrategy strategy = EARLIEST_DEADLINE) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PARALLEL CHEAPEST INSERTION (";
        switch(strategy) {
            case EARLIEST_DEADLINE: std::cout << "EARLIEST_DEADLINE"; break;
            case LATEST_DEADLINE: std::cout << "LATEST_DEADLINE"; break;
            case GEOGRAPHIC_CLUSTER: std::cout << "GEOGRAPHIC_CLUSTER"; break;
            case PRIORITY_BASED: std::cout << "PRIORITY_BASED"; break;
            case RANDOM_ORDER: std::cout << "RANDOM_ORDER"; break;
            case CHEAPEST_VEHICLE_FIRST: std::cout << "CHEAPEST_VEHICLE_FIRST"; break;
            case DOLLAR_COST_AWARE: std::cout << "DOLLAR_COST_AWARE"; break;
            case PREFERENCE_PRIORITY: std::cout << "PREFERENCE_PRIORITY"; break;
            case GEO_CLUSTER_CONSOL: std::cout << "GEO_CLUSTER_CONSOL"; break;
            case DEPOT_PROXIMITY: std::cout << "DEPOT_PROXIMITY"; break;
            case ANGULAR_SECTOR: std::cout << "ANGULAR_SECTOR"; break;
        }
        std::cout << ")\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        
        routes.clear();
        routes.resize(n_veh);
        
        // Create employee order based on strategy
        std::vector<int> emp_order(n_emp);
        for (int i = 0; i < n_emp; i++) emp_order[i] = i;
        
        switch(strategy) {
            case EARLIEST_DEADLINE:
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
                });
                break;
            case LATEST_DEADLINE:
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    return emps[a].latest_arrival_deadline > emps[b].latest_arrival_deadline;
                });
                break;
            case GEOGRAPHIC_CLUSTER:
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    double dist_a = haversine_km(emps[a].pickup_lat, emps[a].pickup_lng, 
                                              emps[a].drop_lat, emps[a].drop_lng);
                    double dist_b = haversine_km(emps[b].pickup_lat, emps[b].pickup_lng, 
                                              emps[b].drop_lat, emps[b].drop_lng);
                    return dist_a < dist_b;
                });
                break;
            case PRIORITY_BASED:
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    return emps[a].priority < emps[b].priority;
                });
                break;
            case RANDOM_ORDER: {
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(emp_order.begin(), emp_order.end(), g);
                break;
            }
            case CHEAPEST_VEHICLE_FIRST:
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
                });
                break;
            case DOLLAR_COST_AWARE:
                // Sort by distance to office (farthest first --- they're most expensive)
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    double dist_a = haversine_km(emps[a].pickup_lat, emps[a].pickup_lng, 
                                              emps[a].drop_lat, emps[a].drop_lng);
                    double dist_b = haversine_km(emps[b].pickup_lat, emps[b].pickup_lng, 
                                              emps[b].drop_lat, emps[b].drop_lng);
                    return dist_a > dist_b;  // Farthest first
                });
                break;
            case PREFERENCE_PRIORITY:
                // Premium preference employees first, then normal, then any
                // Within each group: single sharing first (need own trip), then by tightest deadline
                std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
                    auto pref_rank = [](int p) { return p == 1 ? 0 : (p == 2 ? 1 : 2); };
                    int ra = pref_rank(emps[a].vehicle_pref);
                    int rb = pref_rank(emps[b].vehicle_pref);
                    if (ra != rb) return ra < rb;
                    if (emps[a].sharing_pref != emps[b].sharing_pref)
                        return emps[a].sharing_pref < emps[b].sharing_pref;
                    return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
                });
                break;
            case GEO_CLUSTER_CONSOL:
                // Cluster employees by angular position relative to office, then by distance.
                // This groups geographically close employees together so they get assigned
                // to the same vehicle, creating shorter routes.
                {
                    // Compute centroid of all employees as reference
                    double off_lat = emps[0].drop_lat;  // All go to same office
                    double off_lng = emps[0].drop_lng;
                    
                    // Sort by angle from office to pickup, then by distance
                    std::sort(emp_order.begin(), emp_order.end(), [&emps, off_lat, off_lng](int a, int b) {
                        double angle_a = std::atan2(emps[a].pickup_lat - off_lat, emps[a].pickup_lng - off_lng);
                        double angle_b = std::atan2(emps[b].pickup_lat - off_lat, emps[b].pickup_lng - off_lng);
                        if (std::abs(angle_a - angle_b) > 0.3) return angle_a < angle_b; // ~17 degree sectors
                        // Within same sector, sort by distance from office (farthest first for efficient routing)
                        double dist_a = haversine_km(emps[a].pickup_lat, emps[a].pickup_lng, off_lat, off_lng);
                        double dist_b = haversine_km(emps[b].pickup_lat, emps[b].pickup_lng, off_lat, off_lng);
                        return dist_a > dist_b;
                    });
                }
                break;
            case DEPOT_PROXIMITY:
                // Sort employees by their pickup's distance to the NEAREST vehicle depot
                // This tends to use more vehicles since employees go to nearest depot
                std::sort(emp_order.begin(), emp_order.end(), [&emps, &virt_vehs](int a, int b) {
                    // Find nearest depot distance for employee a
                    double min_a = 1e9;
                    for (const auto& v : virt_vehs) {
                        double d = haversine_km(emps[a].pickup_lat, emps[a].pickup_lng, v.current_lat, v.current_lng);
                        if (d < min_a) min_a = d;
                    }
                    // Find nearest depot distance for employee b
                    double min_b = 1e9;
                    for (const auto& v : virt_vehs) {
                        double d = haversine_km(emps[b].pickup_lat, emps[b].pickup_lng, v.current_lat, v.current_lng);
                        if (d < min_b) min_b = d;
                    }
                    // Sort by nearest depot distance (closest to depot first = assigns to their nearest vehicle)
                    return min_a < min_b;
                });
                break;
            case ANGULAR_SECTOR:
                // Proper angular sector partitioning for many-to-one structure
                // (Vansteenwegen Section 5.2.1.3; Li & Quadrifoglio 2009; Kim et al. 2019)
                // Partitions employees into angular sectors around the office, then within each
                // sector sorts by distance (farthest first) for radial route construction.
                {
                    double off_lat = emps[0].drop_lat;
                    double off_lng = emps[0].drop_lng;
                    
                    // Compute angles and distances for all employees
                    struct EmpAngle { int idx; double angle; double dist; };
                    std::vector<EmpAngle> angles(n_emp);
                    for (int i = 0; i < n_emp; i++) {
                        angles[i].idx = i;
                        angles[i].angle = std::atan2(emps[i].pickup_lat - off_lat, emps[i].pickup_lng - off_lng);
                        angles[i].dist = haversine_km(emps[i].pickup_lat, emps[i].pickup_lng, off_lat, off_lng);
                    }
                    
                    // Sort by angle to group geographically similar employees
                    std::sort(angles.begin(), angles.end(), [](const EmpAngle& a, const EmpAngle& b) {
                        return a.angle < b.angle;
                    });
                    
                    // Determine sector size based on average vehicle capacity
                    int total_cap = 0;
                    for (const auto& v : virt_vehs) total_cap += v.capacity;
                    int avg_cap = (n_veh > 0) ? std::max(2, total_cap / n_veh) : 3;
                    int num_sectors = std::max(1, (n_emp + avg_cap - 1) / avg_cap);
                    int per_sector = (n_emp + num_sectors - 1) / num_sectors;
                    
                    // Within each sector, sort farthest first (radial from Shi & Gao 2020)
                    for (int s = 0; s < num_sectors; s++) {
                        int start = s * per_sector;
                        int end = std::min(start + per_sector, n_emp);
                        if (start >= n_emp) break;
                        std::sort(angles.begin() + start, angles.begin() + end,
                                  [](const EmpAngle& a, const EmpAngle& b) {
                                      return a.dist > b.dist;
                                  });
                    }
                    
                    // Build emp_order from sector-sorted angles
                    for (int i = 0; i < n_emp; i++) {
                        emp_order[i] = angles[i].idx;
                    }
                }
                break;
        }
        
        std::cout << "Employee order: ";
        for (int e : emp_order) std::cout << emps[e].employee_id << "(" << emps[e].latest_arrival_deadline << ") ";
        std::cout << std::endl;
        
        // Create vehicle ordering: by cost_per_km ascending for CHEAPEST_VEHICLE_FIRST and DOLLAR_COST_AWARE
        std::vector<int> veh_order(n_veh);
        for (int i = 0; i < n_veh; i++) veh_order[i] = i;
        if (strategy == CHEAPEST_VEHICLE_FIRST || strategy == DOLLAR_COST_AWARE || strategy == PREFERENCE_PRIORITY || strategy == GEO_CLUSTER_CONSOL || strategy == ANGULAR_SECTOR) {
            std::sort(veh_order.begin(), veh_order.end(), [&virt_vehs](int a, int b) {
                return virt_vehs[a].cost_per_km < virt_vehs[b].cost_per_km;
            });
        }
        
        std::vector<bool> routed(n_emp, false);
        int unrouted = n_emp;
        int iter = 0;
        
        while (unrouted > 0) {
            InsertionInfo best;
            
            // Process employees in deadline order
            for (int e : emp_order) {
                if (routed[e]) continue;
                
                for (int vi = 0; vi < n_veh; vi++) {
                    int v = veh_order[vi];
                    if (!cp.employee_vars[e].is_vehicle_valid(v)) continue;
                    
                    bool compat = true;
                    for (int ex : routes[v]) {
                        if (!cp.are_compatible(e, ex)) { compat = false; break; }
                    }
                    if (!compat) continue;
                    
                    for (size_t p = 0; p <= routes[v].size(); p++) {
                        double cost = calculate_delta_cost(routes[v], p, e, virt_vehs[v].start_node, emps);
                        // For CHEAPEST_VEHICLE_FIRST and DOLLAR_COST_AWARE, use dollar cost
                        // to strongly prefer cheap vehicles (like OR-Tools cost callback)
                        if (strategy == CHEAPEST_VEHICLE_FIRST || strategy == DOLLAR_COST_AWARE || strategy == PREFERENCE_PRIORITY || strategy == GEO_CLUSTER_CONSOL || strategy == ANGULAR_SECTOR) {
                            cost = cost * virt_vehs[v].cost_per_km;
                            // Capacity-aware penalty for opening a new trip —
                            // low-capacity vehicles pay more since they can't amortize deadheading
                            if (routes[v].empty()) {
                                double deadhead = (dist_matrix[OFFICE_NODE][emps[e].node_idx]
                                                 + dist_matrix[emps[e].node_idx][OFFICE_NODE])
                                                * virt_vehs[v].cost_per_km;
                                cost += g_config.new_trip_penalty
                                      + deadhead * (1.0 - 1.0 / std::max(1, virt_vehs[v].capacity));
                            }
                            // Add penalty for vehicle preference violations
                            // Premium preference employee on normal vehicle, or normal preference on premium
                            if (emps[e].vehicle_pref == 1 && virt_vehs[v].category != 1) {
                                cost += g_config.pref_violation_penalty;  // Strong penalty for wrong vehicle type
                            }
                            if (emps[e].vehicle_pref == 2 && virt_vehs[v].category == 1) {
                                cost += g_config.pref_violation_penalty;
                            }
                            // Add penalty for sharing preference violations
                            // If employee wants single ride but trip already has people (or vice versa)
                            if (emps[e].sharing_pref < (int)(routes[v].size() + 1)) {
                                cost += g_config.pref_violation_penalty;
                            }
                            // Also penalize existing riders whose sharing pref would be violated
                            for (int ex : routes[v]) {
                                if (emps[ex].sharing_pref < (int)(routes[v].size() + 1)) {
                                    cost += g_config.pref_violation_penalty;
                                }
                            }
                        }
                        
                        if (cost < best.delta_cost &&
                            is_capacity_valid(routes[v], p, e, virt_vehs[v], emps, enforce_soft) &&
                            is_time_window_valid(routes[v], p, e, virt_vehs[v], emps, meta)) {
                            best.employee_idx = e;
                            best.vehicle_idx = v;
                            best.position = p;
                            best.delta_cost = cost;
                        }
                    }
                }
            }
            
            if (best.employee_idx >= 0) {
                routes[best.vehicle_idx].insert(routes[best.vehicle_idx].begin() + best.position, 
                                                 best.employee_idx);
                routed[best.employee_idx] = true;
                unrouted--;
                iter++;
                
                if (iter % 10 == 0 || unrouted == 0)
                    std::cout << "   Iteration " << iter << ": " 
                              << (n_emp - unrouted) << "/" << n_emp << " routed" << std::endl;
            } else {
                std::cerr << " Cannot route remaining employees - forcing insertion" << std::endl;
                // Smart forced insertion: try to find a placement that minimizes violations
                // First preference: Trip 1 vehicles (don't have trip-start dependencies)
                for (int e = 0; e < n_emp; e++) {
                    if (!routed[e]) {
                        std::cerr << "   Trying to place " << emps[e].employee_id << std::endl;
                        bool placed = false;
                        
                        // First: try Trip 1 vehicles only (v%3 == 0)
                        for (int v = 0; v < n_veh && !placed; v++) {
                            if (v % TRIPS_PER_VEHICLE != 0) continue;  // Skip non-Trip-1
                            if ((int)routes[v].size() >= virt_vehs[v].capacity) continue;
                            
                            // Check compatibility - prefer compatible placements
                            bool compat = true;
                            for (int ex : routes[v]) {
                                if (!cp.are_compatible(e, ex)) { compat = false; break; }
                            }
                            if (compat) {
                                routes[v].push_back(e);
                                routed[e] = true;
                                unrouted--;
                                placed = true;
                                std::cerr << "   -> Placed on " << virt_vehs[v].vehicle_id << " (compatible)" << std::endl;
                            }
                        }
                        
                        // Second: try any Trip 1 (even if incompatible)
                        if (!placed) {
                            for (int v = 0; v < n_veh && !placed; v++) {
                                if (v % TRIPS_PER_VEHICLE != 0) continue;
                                if ((int)routes[v].size() >= virt_vehs[v].capacity) continue;
                                routes[v].push_back(e);
                                routed[e] = true;
                                unrouted--;
                                placed = true;
                                std::cerr << "   -> Forced on " << virt_vehs[v].vehicle_id << std::endl;
                            }
                        }
                        
                        // Last resort: any vehicle
                        if (!placed) {
                            for (int v = 0; v < n_veh && !placed; v++) {
                                if ((int)routes[v].size() >= virt_vehs[v].capacity) continue;
                                routes[v].push_back(e);
                                routed[e] = true;
                                unrouted--;
                                placed = true;
                                std::cerr << "   -> Last resort on " << virt_vehs[v].vehicle_id << std::endl;
                            }
                        }
                        break;  // Only process one employee at a time
                    }
                }
            }
        }
        
        int used = 0;
        for (const auto& r : routes) if (!r.empty()) used++;
        
        std::cout << "\nInitial solution: " << n_emp << " employees in " 
                  << used << " vehicle trips\n";
        
        // Debug: show initial routes before GLS
        std::cout << "Initial routes:\n";
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            std::cout << "  " << virt_vehs[v].vehicle_id << ": ";
            for (int e : routes[v]) std::cout << emps[e].employee_id << " ";
            std::cout << "\n";
        }
        
        std::cout << std::string(60, '=') << std::endl;
    }
};

// ============================================================================
// PreferenceFirstConstruction: Guarantees preference satisfaction for strict employees
// Phase 1: Directly assign each premium+single employee to an empty premium trip (1 per trip)
// Phase 2: Cheapest insertion for remaining employees with pref+sharing penalties
// ============================================================================
class PreferenceFirstConstruction {
public:
    static void build(std::vector<std::vector<int>>& routes,
                      const std::vector<Employee>& emps,
                      const std::vector<Vehicle>& virt_vehs,
                      const ConstraintEngine& cp, bool enforce_soft, const Metadata& meta) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PREFERENCE-FIRST CONSTRUCTION\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        
        routes.clear();
        routes.resize(n_veh);
        
        std::vector<bool> routed(n_emp, false);
        int unrouted = n_emp;
        
        // ---- Phase 1: Direct assignment for preference-strict employees ----
        // Identify employees who want premium + single (vehicle_pref==1, sharing_pref==1)
        // Sort by tightest deadline first (they need the earliest trip slots)
        std::vector<int> strict_emps;
        for (int i = 0; i < n_emp; i++) {
            if (emps[i].vehicle_pref == 1 && emps[i].sharing_pref == 1) {
                strict_emps.push_back(i);
            }
        }
        std::sort(strict_emps.begin(), strict_emps.end(), [&emps](int a, int b) {
            return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
        });
        
        // Find empty premium trips, sorted by trip index (Trip 1 starts from depot, others from office)
        // Prefer Trip 1 slots since they start earliest
        std::vector<int> premium_trips;
        for (int v = 0; v < n_veh; v++) {
            if (virt_vehs[v].category == 1) {  // premium vehicle
                premium_trips.push_back(v);
            }
        }
        // Sort: Trip 1 first (v % TRIPS_PER_VEHICLE == 0), then by cost_per_km ascending
        std::sort(premium_trips.begin(), premium_trips.end(), [&virt_vehs](int a, int b) {
            int trip_a = a % TRIPS_PER_VEHICLE;
            int trip_b = b % TRIPS_PER_VEHICLE;
            if (trip_a != trip_b) return trip_a < trip_b;  // earlier trip slots first
            return virt_vehs[a].cost_per_km < virt_vehs[b].cost_per_km;
        });
        
        int premium_idx = 0;
        std::cout << "  Phase 1: Assigning " << strict_emps.size() << " premium+single employees\n";
        for (int e : strict_emps) {
            bool placed = false;
            // Find first available empty premium trip
            while (premium_idx < (int)premium_trips.size() && !placed) {
                int v = premium_trips[premium_idx];
                if (routes[v].empty() && 
                    is_mode_compatible(emps[e].priority, emps[e].vehicle_pref, virt_vehs[v].vehicle_mode)) {
                    // Check compatibility with CP
                    bool valid = true;
                    if (cp.employee_vars.size() > 0) {
                        if (!cp.employee_vars[e].is_vehicle_valid(v)) valid = false;
                    }
                    if (valid) {
                        routes[v].push_back(e);
                        routed[e] = true;
                        unrouted--;
                        placed = true;
                        std::cout << "    " << emps[e].employee_id << " -> " << virt_vehs[v].vehicle_id 
                                  << " (premium trip, deadline=" << emps[e].latest_arrival_deadline << ")\n";
                    }
                }
                premium_idx++;
            }
            if (!placed) {
                std::cout << "    WARNING: " << emps[e].employee_id << " - no empty premium trip available\n";
            }
        }
        
        // ---- Also handle normal-preference+single employees (vehicle_pref==2, sharing_pref==1) ----
        std::vector<int> normal_single_emps;
        for (int i = 0; i < n_emp; i++) {
            if (!routed[i] && emps[i].sharing_pref == 1 && emps[i].vehicle_pref != 1) {
                normal_single_emps.push_back(i);
            }
        }
        std::sort(normal_single_emps.begin(), normal_single_emps.end(), [&emps](int a, int b) {
            return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
        });
        
        // Find empty normal trips
        std::vector<int> normal_trips;
        for (int v = 0; v < n_veh; v++) {
            if (routes[v].empty() && virt_vehs[v].category != 1) {  // non-premium
                normal_trips.push_back(v);
            }
        }
        std::sort(normal_trips.begin(), normal_trips.end(), [&virt_vehs](int a, int b) {
            int trip_a = a % TRIPS_PER_VEHICLE;
            int trip_b = b % TRIPS_PER_VEHICLE;
            if (trip_a != trip_b) return trip_a < trip_b;
            return virt_vehs[a].cost_per_km < virt_vehs[b].cost_per_km;
        });
        
        int normal_idx = 0;
        for (int e : normal_single_emps) {
            bool placed = false;
            while (normal_idx < (int)normal_trips.size() && !placed) {
                int v = normal_trips[normal_idx];
                if (routes[v].empty() &&
                    is_mode_compatible(emps[e].priority, emps[e].vehicle_pref, virt_vehs[v].vehicle_mode)) {
                    bool valid = true;
                    if (cp.employee_vars.size() > 0) {
                        if (!cp.employee_vars[e].is_vehicle_valid(v)) valid = false;
                    }
                    if (valid) {
                        routes[v].push_back(e);
                        routed[e] = true;
                        unrouted--;
                        placed = true;
                        std::cout << "    " << emps[e].employee_id << " -> " << virt_vehs[v].vehicle_id 
                                  << " (normal single trip)\n";
                    }
                }
                normal_idx++;
            }
        }
        
        std::cout << "  Phase 1 complete: " << (n_emp - unrouted) << "/" << n_emp << " placed\n";
        
        // ---- Phase 2: Cheapest insertion for remaining employees ----
        // Sort remaining by deadline (tightest first)
        std::vector<int> remaining;
        for (int i = 0; i < n_emp; i++) {
            if (!routed[i]) remaining.push_back(i);
        }
        std::sort(remaining.begin(), remaining.end(), [&emps](int a, int b) {
            return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
        });
        
        // Vehicle order: cheapest first
        std::vector<int> veh_order(n_veh);
        for (int i = 0; i < n_veh; i++) veh_order[i] = i;
        std::sort(veh_order.begin(), veh_order.end(), [&virt_vehs](int a, int b) {
            return virt_vehs[a].cost_per_km < virt_vehs[b].cost_per_km;
        });
        
        std::cout << "  Phase 2: Cheapest insertion for " << unrouted << " remaining employees\n";
        
        while (unrouted > 0) {
            InsertionInfo best;
            
            for (int e : remaining) {
                if (routed[e]) continue;
                
                for (int vi = 0; vi < n_veh; vi++) {
                    int v = veh_order[vi];
                    if (!cp.employee_vars.empty() && !cp.employee_vars[e].is_vehicle_valid(v)) continue;
                    
                    bool compat = true;
                    for (int ex : routes[v]) {
                        if (!cp.are_compatible(e, ex)) { compat = false; break; }
                    }
                    if (!compat) continue;
                    
                    for (size_t p = 0; p <= routes[v].size(); p++) {
                        double cost = calculate_delta_cost(routes[v], p, e, virt_vehs[v].start_node, emps);
                        cost = cost * virt_vehs[v].cost_per_km;
                        
                        if (routes[v].empty()) {
                            double deadhead = (dist_matrix[OFFICE_NODE][emps[e].node_idx]
                                             + dist_matrix[emps[e].node_idx][OFFICE_NODE])
                                            * virt_vehs[v].cost_per_km;
                            cost += g_config.new_trip_penalty
                                  + deadhead * (1.0 - 1.0 / std::max(1, virt_vehs[v].capacity));
                        }
                        
                        // Vehicle preference penalty
                        if (emps[e].vehicle_pref == 1 && virt_vehs[v].category != 1) {
                            cost += g_config.pref_violation_penalty;
                        }
                        if (emps[e].vehicle_pref == 2 && virt_vehs[v].category == 1) {
                            cost += g_config.pref_violation_penalty;
                        }
                        // Sharing preference penalty
                        if (emps[e].sharing_pref < (int)(routes[v].size() + 1)) {
                            cost += g_config.pref_violation_penalty;
                        }
                        for (int ex : routes[v]) {
                            if (emps[ex].sharing_pref < (int)(routes[v].size() + 1)) {
                                cost += g_config.pref_violation_penalty;
                            }
                        }
                        
                        if (cost < best.delta_cost &&
                            is_capacity_valid(routes[v], p, e, virt_vehs[v], emps, enforce_soft) &&
                            is_time_window_valid(routes[v], p, e, virt_vehs[v], emps, meta)) {
                            best.employee_idx = e;
                            best.vehicle_idx = v;
                            best.position = p;
                            best.delta_cost = cost;
                        }
                    }
                }
            }
            
            if (best.employee_idx >= 0) {
                routes[best.vehicle_idx].insert(routes[best.vehicle_idx].begin() + best.position, 
                                                 best.employee_idx);
                routed[best.employee_idx] = true;
                unrouted--;
            } else {
                // Forced insertion for remaining employees
                for (int e = 0; e < n_emp; e++) {
                    if (!routed[e]) {
                        for (int v = 0; v < n_veh; v++) {
                            if ((int)routes[v].size() < virt_vehs[v].capacity) {
                                routes[v].push_back(e);
                                routed[e] = true;
                                unrouted--;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        int used = 0;
        for (const auto& r : routes) if (!r.empty()) used++;
        
        std::cout << "  Result: " << n_emp << " employees in " << used << " trips\n";
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            std::cout << "    " << virt_vehs[v].vehicle_id << ": ";
            for (int e : routes[v]) std::cout << emps[e].employee_id << " ";
            std::cout << "\n";
        }
        std::cout << std::string(60, '=') << std::endl;
    }
};

#endif
