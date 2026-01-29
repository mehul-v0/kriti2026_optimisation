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
        
        routes.clear();
        routes.resize(n_veh);
        
        // Sort employees by deadline (tightest first)
        std::vector<int> emp_order(n_emp);
        for (int i = 0; i < n_emp; i++) emp_order[i] = i;
        std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
            return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
        });
        
        std::cout << "Trying single-employee trips for each: ";
        for (int e : emp_order) std::cout << emps[e].employee_id << " ";
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
                // Fallback: add to an existing trip that can accommodate
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
                    // Last resort: force on first trip of first vehicle
                    for (int v = 0; v < n_veh; v++) {
                        if (v % 3 == 0) {  // Trip 1 only
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
        
        int used = 0;
        for (const auto& r : routes) if (!r.empty()) used++;
        
        std::cout << "\nSingle-trip solution: " << n_emp << " employees in " 
                  << used << " trips\n";
        std::cout << std::string(60, '=') << std::endl;
    }
};

#endif
