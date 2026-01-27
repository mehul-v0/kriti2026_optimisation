#ifndef VRP_CONSTRUCTION_H
#define VRP_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <iostream>
#include <limits>
#include <algorithm>

class ParallelCheapestInsertion {
public:
    static void build(std::vector<std::vector<int>>& routes,
                      const std::vector<Employee>& emps,
                      const std::vector<Vehicle>& virt_vehs,
                      const ConstraintEngine& cp, bool enforce_soft, const Metadata& meta) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PARALLEL CHEAPEST INSERTION\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        
        routes.clear();
        routes.resize(n_veh);
        
        // Sort employees by deadline (tighter deadlines first) for priority processing
        std::vector<int> emp_order(n_emp);
        for (int i = 0; i < n_emp; i++) emp_order[i] = i;
        std::sort(emp_order.begin(), emp_order.end(), [&emps](int a, int b) {
            return emps[a].latest_arrival_deadline < emps[b].latest_arrival_deadline;
        });
        
        std::cout << "Employee order by deadline: ";
        for (int e : emp_order) std::cout << emps[e].employee_id << "(" << emps[e].latest_arrival_deadline << ") ";
        std::cout << std::endl;
        
        std::vector<bool> routed(n_emp, false);
        int unrouted = n_emp;
        int iter = 0;
        
        while (unrouted > 0) {
            InsertionInfo best;
            
            // Process employees in deadline order
            for (int e : emp_order) {
                if (routed[e]) continue;
                
                for (int v = 0; v < n_veh; v++) {
                    if (!cp.employee_vars[e].is_vehicle_valid(v)) continue;
                    
                    bool compat = true;
                    for (int ex : routes[v]) {
                        if (!cp.are_compatible(e, ex)) { compat = false; break; }
                    }
                    if (!compat) continue;
                    
                    for (size_t p = 0; p <= routes[v].size(); p++) {
                        double cost = calculate_delta_cost(routes[v], p, e, virt_vehs[v].start_node, emps);
                        
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
                std::cerr << "⚠️ Cannot route remaining employees - forcing insertion" << std::endl;
                // Smart forced insertion: try to find a placement that minimizes violations
                // First preference: Trip 1 vehicles (don't have trip-start dependencies)
                for (int e = 0; e < n_emp; e++) {
                    if (!routed[e]) {
                        std::cerr << "   Trying to place " << emps[e].employee_id << std::endl;
                        bool placed = false;
                        
                        // First: try Trip 1 vehicles only (v%3 == 0)
                        for (int v = 0; v < n_veh && !placed; v++) {
                            if (v % 3 != 0) continue;  // Skip Trip 2/3
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
                                if (v % 3 != 0) continue;
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
