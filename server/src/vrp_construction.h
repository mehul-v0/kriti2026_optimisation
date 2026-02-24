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
    DEPOT_PROXIMITY         // Assign to physical vehicle with closest depot (uses more vehicles)
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
        }
        
        std::cout << "Employee order: ";
        for (int e : emp_order) std::cout << emps[e].employee_id << "(" << emps[e].latest_arrival_deadline << ") ";
        std::cout << std::endl;
        
        // Create vehicle ordering: by cost_per_km ascending for CHEAPEST_VEHICLE_FIRST and DOLLAR_COST_AWARE
        std::vector<int> veh_order(n_veh);
        for (int i = 0; i < n_veh; i++) veh_order[i] = i;
        if (strategy == CHEAPEST_VEHICLE_FIRST || strategy == DOLLAR_COST_AWARE || strategy == PREFERENCE_PRIORITY || strategy == GEO_CLUSTER_CONSOL) {
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
                        if (strategy == CHEAPEST_VEHICLE_FIRST || strategy == DOLLAR_COST_AWARE || strategy == PREFERENCE_PRIORITY || strategy == GEO_CLUSTER_CONSOL) {
                            cost = cost * virt_vehs[v].cost_per_km;
                            // Add penalty for opening new trip (consolidation preference) - use config value
                            if ((strategy == DOLLAR_COST_AWARE || strategy == PREFERENCE_PRIORITY || strategy == GEO_CLUSTER_CONSOL) && routes[v].empty()) {
                                cost += g_config.vehicle_activation_cost;
                            }
                            // Add penalty for vehicle preference violations
                            // Premium preference employee on normal vehicle, or normal preference on premium
                            if (emps[e].vehicle_pref == 1 && virt_vehs[v].category != 1) {
                                cost += g_config.pref_violation_penalty;  // Strong penalty for wrong vehicle type
                            }
                            if (emps[e].vehicle_pref == 2 && virt_vehs[v].category == 1) {
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

#endif
