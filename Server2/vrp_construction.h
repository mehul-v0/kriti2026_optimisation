#ifndef VRP_CONSTRUCTION_H
#define VRP_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <iostream>
#include <limits>
#include <algorithm>
#include <random>

// Different ordering strategies for construction heuristic
enum OrderingStrategy {
    EARLIEST_DEADLINE,      // Original: tightest deadline first
    LATEST_DEADLINE,        // Relaxed deadlines first (counter-intuitive but may work)
    GEOGRAPHIC_CLUSTER,     // Nearest to office first
    PRIORITY_BASED,         // By employee priority
    RANDOM_ORDER            // Random for diversity
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
            case RANDOM_ORDER:
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(emp_order.begin(), emp_order.end(), g);
                break;
        }
        
        std::cout << "Employee order: ";
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
