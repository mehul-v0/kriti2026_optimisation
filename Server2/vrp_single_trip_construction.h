#ifndef VRP_SINGLE_TRIP_CONSTRUCTION_H
#define VRP_SINGLE_TRIP_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <iostream>
#include <algorithm>
#include <limits>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

// Helper function to calculate route cost
static double route_cost(const std::vector<int>& route, int start,
                        const std::vector<Employee>& emps, const Vehicle& veh) {
    if (route.empty()) return 0;
    double dist = 0;
    int curr = start;
    for (int e : route) {
        int next = emps[e].node_idx;
        dist += dist_matrix[curr][next];
        curr = next;
    }
    dist += dist_matrix[curr][OFFICE_NODE];
    return dist * veh.cost_per_km;
}

// Build solution with single-employee trips (aggressive trip splitting)
class SingleTripConstruction {
public:
    static void build(std::vector<std::vector<int>>& routes,
                      const std::vector<Employee>& emps,
                      const std::vector<Vehicle>& virt_vehs,
                      const ConstraintEngine& cp, bool enforce_soft, const Metadata& meta) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SINGLE-TRIP CONSTRUCTION (Minimize Violations)\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        int trips_per_vehicle = 4;
        
        routes.clear();
        routes.resize(n_veh);
        
        // Pre-detect inherently infeasible employees —
        // those who can't meet deadline on ANY trip 1 vehicle
        std::vector<bool> is_infeasible(n_emp, false);
        for (int i = 0; i < n_emp; i++) {
            bool any_feasible = false;
            for (int v = 0; v < n_veh; v++) {
                if (v % trips_per_vehicle != 0) continue; // Only trip 1
                std::vector<int> test = {i};
                int h = 0, s = 0;
                validate_full_route(test, virt_vehs[v], emps, h, s, false, meta, true);
                RouteSimResult sim = simulate_route(test, virt_vehs[v], emps);
                if (sim.office_arrival <= emps[i].latest_arrival_deadline) {
                    any_feasible = true;
                    break;
                }
            }
            if (!any_feasible) {
                is_infeasible[i] = true;
                std::cout << "   DETECTED: " << emps[i].employee_id 
                          << " is inherently infeasible (deadline too tight)\n";
            }
        }
        
        // Sort employees: feasible by deadline first, infeasible at the end
        std::vector<int> emp_order(n_emp);
        for (int i = 0; i < n_emp; i++) emp_order[i] = i;
        std::sort(emp_order.begin(), emp_order.end(), [&emps, &is_infeasible](int a, int b) {
            if (is_infeasible[a] != is_infeasible[b]) return !is_infeasible[a]; // feasible first
            return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
        });
        
        std::cout << "Employee order (infeasible last): ";
        for (int e : emp_order) std::cout << emps[e].employee_id 
                    << (is_infeasible[e] ? "*" : "") << " ";
        std::cout << std::endl;
        
        // Try to assign each employee to their own trip (minimize sharing)
        std::vector<bool> routed(n_emp, false);
        
        for (int e : emp_order) {
            double best_score = 1e9;
            int best_v = -1;
            int best_h = 999, best_s = 999;
            
            // Try all available trips
            for (int v = 0; v < n_veh; v++) {
                // Skip if trip already used
                if (!routes[v].empty()) continue;
                
                // Check if employee can use this vehicle
                if (!cp.employee_vars[e].is_vehicle_valid(v)) continue;
                
                // Try single-employee assignment
                std::vector<int> test_route = {e};
                int h = 0, s = 0;
                
                if (validate_full_route(test_route, virt_vehs[v], emps, h, s, enforce_soft, meta)) {
                    // Prioritize: violations first, then cost
                    double score = h * 10000 + s * 100 + 
                                   route_cost(test_route, virt_vehs[v].start_node, emps, virt_vehs[v]);
                    
                    if (h < best_h || (h == best_h && s < best_s) || 
                        (h == best_h && s == best_s && score < best_score)) {
                        best_score = score;
                        best_v = v;
                        best_h = h;
                        best_s = s;
                    }
                }
            }
            
            if (best_v >= 0) {
                routes[best_v].push_back(e);
                routed[e] = true;
                std::cout << "   " << emps[e].employee_id << " -> " << virt_vehs[best_v].vehicle_id 
                          << " (H:" << best_h << " S:" << best_s << ")\n";
            } else {
                // Infeasible employee — place on the LATEST available trip
                // to avoid cascading delays to other trips on the same vehicle
                if (is_infeasible[e]) {
                    std::cout << "   " << emps[e].employee_id 
                              << " -> Placing infeasible employee on latest trip...\n";
                    
                    // Find the latest empty trip on a compatible vehicle
                    // Prefer vehicles that already have earlier trips used (so this
                    // employee goes as the last trip, not poisoning future trips)
                    int best_late_v = -1;
                    int best_trip_idx = -1;
                    double best_late_score = 1e18;
                    
                    for (int v = n_veh - 1; v >= 0; v--) {
                        if (!routes[v].empty()) continue;
                        if (!cp.employee_vars[e].is_vehicle_valid(v)) continue;
                        
                        int trip_idx = v % trips_per_vehicle;
                        int phys_base = v - trip_idx;
                        
                        // Check if there's at least one earlier trip used on this vehicle
                        bool has_earlier = false;
                        for (int t = 0; t < trip_idx; t++) {
                            if (!routes[phys_base + t].empty()) { has_earlier = true; break; }
                        }
                        
                        // Check no LATER trips are used (don't insert before used trips)
                        bool has_later = false;
                        for (int t = trip_idx + 1; t < trips_per_vehicle; t++) {
                            if (phys_base + t < n_veh && !routes[phys_base + t].empty()) {
                                has_later = true; break;
                            }
                        }
                        if (has_later) continue;
                        
                        // Simulate to get lateness for this placement
                        std::vector<int> test = {e};
                        RouteSimResult sim = simulate_route(test, virt_vehs[v], emps);
                        int lateness = std::max(0, sim.office_arrival - emps[e].latest_arrival_deadline);
                        
                        // Score: prefer vehicles with earlier trips used (infeasible goes last),
                        // minimize lateness, prefer later trip indices
                        double score = lateness * 1000.0 - trip_idx * 100.0 + (has_earlier ? -5000.0 : 0.0);
                        
                        if (score < best_late_score || best_late_v == -1) {
                            best_late_score = score;
                            best_late_v = v;
                            best_trip_idx = trip_idx;
                        }
                    }
                    
                    if (best_late_v >= 0) {
                        routes[best_late_v].push_back(e);
                        routed[e] = true;
                        std::cout << "   " << emps[e].employee_id << " -> " 
                                  << virt_vehs[best_late_v].vehicle_id 
                                  << " (infeasible, placed on late trip " << (best_trip_idx+1) << ")\n";
                    } else {
                        // Absolute last resort for infeasible
                        for (int v = n_veh - 1; v >= 0; v--) {
                            if (routes[v].empty() || (int)routes[v].size() < virt_vehs[v].capacity) {
                                routes[v].push_back(e);
                                routed[e] = true;
                                std::cout << "   " << emps[e].employee_id 
                                          << " -> FORCED (last resort) on " << virt_vehs[v].vehicle_id << "\n";
                                break;
                            }
                        }
                    }
                } else {
                    // Feasible employee that couldn't find an empty trip
                    std::cout << "   " << emps[e].employee_id << " -> Looking for shared trip...\n";
                    bool placed = false;
                    
                    for (int v = 0; v < n_veh && !placed; v++) {
                        if ((int)routes[v].size() >= virt_vehs[v].capacity) continue;
                        if (!cp.employee_vars[e].is_vehicle_valid(v)) continue;
                        
                        // Check compatibility
                        bool compat = true;
                        for (int ex : routes[v]) {
                            if (!cp.are_compatible(e, ex)) {
                                compat = false;
                                break;
                            }
                        }
                        
                        if (compat) {
                            // Try adding
                            std::vector<int> test = routes[v];
                            test.push_back(e);
                            int h=0, s=0;
                            if (validate_full_route(test, virt_vehs[v], emps, h, s, enforce_soft, meta)) {
                                routes[v] = test;
                                routed[e] = true;
                                placed = true;
                                std::cout << "   " << emps[e].employee_id << " -> " << virt_vehs[v].vehicle_id 
                                          << " (shared, H:" << h << " S:" << s << ")\n";
                            }
                        }
                    }
                    
                    if (!placed) {
                        // Force on trip 1 vehicle with available capacity
                        for (int v = 0; v < n_veh; v++) {
                            if (v % trips_per_vehicle == 0 && (int)routes[v].size() < virt_vehs[v].capacity) {
                                routes[v].push_back(e);
                                routed[e] = true;
                                std::cout << "   " << emps[e].employee_id << " -> FORCED on " 
                                          << virt_vehs[v].vehicle_id << "\n";
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        int used = 0;
        for (const auto& r : routes) if (!r.empty()) used++;
        
        std::cout << "\nSingle-trip solution: " << n_emp << " employees in " 
                  << used << " trips\n";
        std::cout << std::string(60, '=') << std::endl;
    }
};

#endif
