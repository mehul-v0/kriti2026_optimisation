#ifndef VRP_CONSTRUCTION_H
#define VRP_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <iostream>
#include <limits>

class ParallelCheapestInsertion {
public:
    static void build(std::vector<std::vector<int>>& routes,
                      const std::vector<Employee>& emps,
                      const std::vector<Vehicle>& virt_vehs,
                      const ConstraintEngine& cp, bool enforce_soft) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PARALLEL CHEAPEST INSERTION\n";
        std::cout << std::string(60, '=') << std::endl;
        
        int n_emp = emps.size();
        int n_veh = virt_vehs.size();
        
        routes.clear();
        routes.resize(n_veh);
        
        std::vector<bool> routed(n_emp, false);
        int unrouted = n_emp;
        int iter = 0;
        
        while (unrouted > 0) {
            InsertionInfo best;
            
            for (int e = 0; e < n_emp; e++) {
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
                            is_time_window_valid(routes[v], p, e, virt_vehs[v], emps)) {
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
        
        std::cout << "\nInitial solution: " << n_emp << " employees in " 
                  << used << " vehicle trips\n";
        std::cout << std::string(60, '=') << std::endl;
    }
};

#endif
