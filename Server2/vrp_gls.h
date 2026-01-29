#ifndef VRP_GLS_H
#define VRP_GLS_H

#include "vrp_types.h"
#include "vrp_local_search.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include <chrono>
#include <iostream>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

class GuidedLocalSearch {
    std::vector<std::vector<int>> penalties;
    double LAMBDA = 0.5;
    int n_nodes;
    const ConstraintEngine* cp_ptr = nullptr;  // Pointer to constraint engine
    
public:
    GuidedLocalSearch(int n) : n_nodes(n) {
        penalties.resize(n, std::vector<int>(n, 0));
    }
    
    void set_constraint_engine(const ConstraintEngine* cp) {
        cp_ptr = cp;
    }
    
    // Check if all employees in a route are compatible with each other
    bool route_compatible(const std::vector<int>& route) const {
        if (!cp_ptr) return true;  // No constraint engine, assume compatible
        for (size_t i = 0; i < route.size(); i++) {
            for (size_t j = i+1; j < route.size(); j++) {
                if (!cp_ptr->are_compatible(route[i], route[j])) return false;
            }
        }
        return true;
    }
    
    double gls_cost(int i, int j) const {
        return dist_matrix[i][j] + LAMBDA * penalties[i][j];
    }
    
    double route_cost_real(const std::vector<int>& route, int start,
                           const std::vector<Employee>& emps, const Vehicle& veh) const {
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
    
    void penalize_worst(const std::vector<std::vector<int>>& routes,
                        const std::vector<Vehicle>& vehs, const std::vector<Employee>& emps) {
        int wu = -1, wv = -1;
        double max_util = -1;
        
        for (size_t vidx = 0; vidx < routes.size(); vidx++) {
            if (routes[vidx].empty()) continue;
            int curr = vehs[vidx].start_node;
            
            for (int e : routes[vidx]) {
                int next = emps[e].node_idx;
                double util = dist_matrix[curr][next] / (1.0 + penalties[curr][next]);
                if (util > max_util) { max_util = util; wu = curr; wv = next; }
                curr = next;
            }
            
            double util = dist_matrix[curr][OFFICE_NODE] / (1.0 + penalties[curr][OFFICE_NODE]);
            if (util > max_util) { max_util = util; wu = curr; wv = OFFICE_NODE; }
        }
        
        if (wu >= 0 && wv >= 0) penalties[wu][wv]++;
    }
    
    // Try splitting a multi-employee route into separate trips
    bool try_split_route(std::vector<std::vector<int>>& routes, size_t v_idx,
                         const std::vector<Vehicle>& vehs, const std::vector<Employee>& emps,
                         const Metadata& meta, bool enforce_soft, double& current_cost) {
        
        if (routes[v_idx].size() < 2) return false;  // Can't split single-employee routes
        
        // Find empty trip slots for the same physical vehicle
        int phys_veh_id = v_idx / 3;  // Which physical vehicle (V1, V2, V3, V4)
        std::vector<size_t> empty_trips;
        
        for (int trip = 0; trip < 3; trip++) {
            size_t trip_idx = phys_veh_id * 3 + trip;
            if (trip_idx < routes.size() && routes[trip_idx].empty() && trip_idx != v_idx) {
                empty_trips.push_back(trip_idx);
            }
        }
        
        if (empty_trips.empty()) return false;
        
        // Try moving each employee to an empty trip
        for (size_t emp_pos = 0; emp_pos < routes[v_idx].size() && !empty_trips.empty(); emp_pos++) {
            int emp = routes[v_idx][emp_pos];
            size_t target_trip = empty_trips.back();
            
            // Create trial configuration
            std::vector<int> new_source = routes[v_idx];
            new_source.erase(new_source.begin() + emp_pos);
            std::vector<int> new_target = {emp};
            
            // Validate both routes
            int h1=0, s1=0, h2=0, s2=0;
            if (validate_full_route(new_source, vehs[v_idx], emps, h1, s1, enforce_soft, meta) &&
                validate_full_route(new_target, vehs[target_trip], emps, h2, s2, enforce_soft, meta)) {
                
                // Calculate cost change
                double old_cost = route_cost_real(routes[v_idx], vehs[v_idx].start_node, emps, vehs[v_idx]);
                double new_cost = route_cost_real(new_source, vehs[v_idx].start_node, emps, vehs[v_idx]) +
                                  route_cost_real(new_target, vehs[target_trip].start_node, emps, vehs[target_trip]);
                
                // Count current violations in the original route
                int old_h=0, old_s=0;
                validate_full_route(routes[v_idx], vehs[v_idx], emps, old_h, old_s, enforce_soft, meta);
                
                // Accept if:
                // 1. It reduces violations (even if cost increases)
                // 2. Same violations but reduces cost
                // 3. Feasible and cost increase < 20%
                if ((h1+h2) < old_h || 
                    ((h1+h2) == old_h && new_cost < old_cost * 1.2)) {
                    routes[v_idx] = new_source;
                    routes[target_trip] = new_target;
                    empty_trips.pop_back();
                    current_cost = current_cost - old_cost + new_cost;
                    return true;
                }
            }
        }
        return false;
    }
    
    // Large Neighborhood Search: remove multiple employees and reinsert optimally
    bool try_destroy_repair(std::vector<std::vector<int>>& routes,
                            const std::vector<Vehicle>& vehs, const std::vector<Employee>& emps,
                            const Metadata& meta, bool enforce_soft, double& current_cost) {
        
        // Find a route with multiple employees
        std::vector<size_t> multi_routes;
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].size() >= 2) multi_routes.push_back(v);
        }
        
        if (multi_routes.empty()) return false;
        
        // Pick a random route and remove all but one employee
        size_t target = multi_routes[rand() % multi_routes.size()];
        std::vector<int> removed;
        
        while (routes[target].size() > 1) {
            removed.push_back(routes[target].back());
            routes[target].pop_back();
        }
        
        // Try to reinsert removed employees in best positions
        for (int emp : removed) {
            double best_delta = 1e9;
            size_t best_v = 0;
            size_t best_pos = 0;
            
            for (size_t v = 0; v < routes.size(); v++) {
                // Check capacity
                if ((int)routes[v].size() >= vehs[v].capacity) continue;
                
                // Check compatibility
                bool compat = true;
                for (int ex : routes[v]) {
                    if (cp_ptr && !cp_ptr->are_compatible(emp, ex)) {
                        compat = false;
                        break;
                    }
                }
                if (!compat) continue;
                
                for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                    std::vector<int> temp = routes[v];
                    temp.insert(temp.begin() + pos, emp);
                    
                    int h=0, s=0;
                    if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
                        double delta = route_cost_real(temp, vehs[v].start_node, emps, vehs[v]) -
                                       route_cost_real(routes[v], vehs[v].start_node, emps, vehs[v]);
                        if (delta < best_delta) {
                            best_delta = delta;
                            best_v = v;
                            best_pos = pos;
                        }
                    }
                }
            }
            
            // Insert at best position
            if (best_delta < 1e9) {
                routes[best_v].insert(routes[best_v].begin() + best_pos, emp);
                current_cost += best_delta;
            } else {
                // Forced insertion if no good position found
                routes[target].push_back(emp);
            }
        }
        
        return true;
    }

    void optimize(std::vector<std::vector<int>>& routes, std::vector<std::vector<int>>& best,
                  double& best_cost, const std::vector<Vehicle>& vehs,
                  const std::vector<Employee>& emps, const Metadata& meta,
                  bool enforce_soft, int time_limit = 10) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "GUIDED LOCAL SEARCH + ROUTE SPLITTING\n";
        std::cout << std::string(60, '=') << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        best_cost = 0;
        for (size_t v = 0; v < routes.size(); v++)
            best_cost += route_cost_real(routes[v], vehs[v].start_node, emps, vehs[v]);
        
        best = routes;
        double current_cost = best_cost;
        std::cout << "Initial cost: $" << best_cost << std::endl;
        
        int iters = 0, improvements = 0, no_improve = 0, splits = 0, repairs = 0;
        
        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= time_limit) {
                std::cout << "\nTime limit reached" << std::endl;
                break;
            }
            
            iters++;
            bool improved = false;
            
            // Local search
            for (size_t v = 0; v < routes.size(); v++) {
                auto& route = routes[v];
                if (route.size() < 2) continue;
                
                // Relocate
                for (size_t i = 0; i < route.size(); i++) {
                    for (size_t j = 0; j < route.size()+1; j++) {
                        if (i == j || i+1 == j) continue;
                        
                        auto delta = LocalSearchOps::relocate_delta(route, i, j, vehs[v].start_node, emps, vehs[v].speed_kmph);
                        if (delta.get_weighted_score(meta.cost_weight, meta.time_weight) < -0.001) {
                            std::vector<int> temp = route;
                            LocalSearchOps::apply_relocate(temp, i, j);
                            int h = 0, s = 0;
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
                                route = temp;
                                improved = true;
                                goto next_iter;
                            }
                        }
                    }
                }
                
                // Exchange
                for (size_t i = 0; i < route.size(); i++) {
                    for (size_t j = i+1; j < route.size(); j++) {
                        auto delta = LocalSearchOps::exchange_delta(route, i, j, vehs[v].start_node, emps, vehs[v].speed_kmph);
                        if (delta.get_weighted_score(meta.cost_weight, meta.time_weight) < -0.001) {
                            std::vector<int> temp = route;
                            LocalSearchOps::apply_exchange(temp, i, j);
                            int h = 0, s = 0;
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
                                route = temp;
                                improved = true;
                                goto next_iter;
                            }
                        }
                    }
                }
                
                // 2-Opt
                for (size_t i = 0; i < route.size(); i++) {
                    for (size_t j = i+1; j < route.size(); j++) {
                        auto delta = LocalSearchOps::twoopt_delta(route, i, j, vehs[v].start_node, emps, vehs[v].speed_kmph);
                        if (delta.get_weighted_score(meta.cost_weight, meta.time_weight) < -0.001) {
                            std::vector<int> temp = route;
                            LocalSearchOps::apply_2opt(temp, i, j);
                            int h = 0, s = 0;
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
                                route = temp;
                                improved = true;
                                goto next_iter;
                            }
                        }
                    }
                }
            }
            
            // ============================================================================
            // INTER-ROUTE OPERATORS (Move employees between vehicles)
            // ============================================================================
            
            // Inter-route relocate: Move employee from one vehicle to another
            for (size_t v1 = 0; v1 < routes.size(); v1++) {
                if (routes[v1].empty()) continue;
                for (size_t i = 0; i < routes[v1].size(); i++) {
                    int emp_to_move = routes[v1][i];
                    
                    for (size_t v2 = 0; v2 < routes.size(); v2++) {
                        if (v1 == v2) continue;
                        
                        // Try inserting into all positions in v2
                        for (size_t j = 0; j <= routes[v2].size(); j++) {
                            // Calculate cost change
                            double old_cost = route_cost_real(routes[v1], vehs[v1].start_node, emps, vehs[v1]) +
                                            route_cost_real(routes[v2], vehs[v2].start_node, emps, vehs[v2]);
                            
                            // Create temporary routes
                            std::vector<int> temp_v1 = routes[v1];
                            std::vector<int> temp_v2 = routes[v2];
                            
                            temp_v1.erase(temp_v1.begin() + i);
                            temp_v2.insert(temp_v2.begin() + j, emp_to_move);
                            
                            double new_cost = route_cost_real(temp_v1, vehs[v1].start_node, emps, vehs[v1]) +
                                            route_cost_real(temp_v2, vehs[v2].start_node, emps, vehs[v2]);
                            
                            if (new_cost < old_cost - 0.01) {
                                // Validate both routes and check compatibility using ConstraintEngine
                                int h1 = 0, s1 = 0, h2 = 0, s2 = 0;
                                if (validate_full_route(temp_v1, vehs[v1], emps, h1, s1, enforce_soft, meta) &&
                                    validate_full_route(temp_v2, vehs[v2], emps, h2, s2, enforce_soft, meta) &&
                                    route_compatible(temp_v1) && route_compatible(temp_v2)) {
                                    routes[v1] = temp_v1;
                                    routes[v2] = temp_v2;
                                    improved = true;
                                    goto next_iter;
                                }
                            }
                        }
                    }
                }
            }
            
            // Inter-route exchange: Swap employees between two vehicles
            for (size_t v1 = 0; v1 < routes.size(); v1++) {
                if (routes[v1].empty()) continue;
                for (size_t i = 0; i < routes[v1].size(); i++) {
                    
                    for (size_t v2 = v1 + 1; v2 < routes.size(); v2++) {
                        if (routes[v2].empty()) continue;
                        for (size_t j = 0; j < routes[v2].size(); j++) {
                            
                            // Calculate cost change
                            double old_cost = route_cost_real(routes[v1], vehs[v1].start_node, emps, vehs[v1]) +
                                            route_cost_real(routes[v2], vehs[v2].start_node, emps, vehs[v2]);
                            
                            // Create temporary routes with swap
                            std::vector<int> temp_v1 = routes[v1];
                            std::vector<int> temp_v2 = routes[v2];
                            
                            std::swap(temp_v1[i], temp_v2[j]);
                            
                            double new_cost = route_cost_real(temp_v1, vehs[v1].start_node, emps, vehs[v1]) +
                                            route_cost_real(temp_v2, vehs[v2].start_node, emps, vehs[v2]);
                            
                            if (new_cost < old_cost - 0.01) {
                                // Validate both routes and check compatibility
                                int h1 = 0, s1 = 0, h2 = 0, s2 = 0;
                                if (validate_full_route(temp_v1, vehs[v1], emps, h1, s1, enforce_soft, meta) &&
                                    validate_full_route(temp_v2, vehs[v2], emps, h2, s2, enforce_soft, meta) &&
                                    route_compatible(temp_v1) && route_compatible(temp_v2)) {
                                    routes[v1] = temp_v1;
                                    routes[v2] = temp_v2;
                                    improved = true;
                                    goto next_iter;
                                }
                            }
                        }
                    }
                }
            }
            
            // ============================================================================
            // ROUTE SPLITTING: Try to split multi-employee routes into single trips
            // ============================================================================
            if (iters % 20 == 0) {  // Try every 20 iterations (more frequent)
                for (size_t v = 0; v < routes.size(); v++) {
                    if (try_split_route(routes, v, vehs, emps, meta, enforce_soft, current_cost)) {
                        splits++;
                        improved = true;
                        // Don't break - try all routes
                    }
                }
            }
            
            // ============================================================================
            // DESTROY-REPAIR: Large neighborhood search for escaping local optima
            // ============================================================================
            if (no_improve > 500 && iters % 50 == 0) {  // Earlier trigger, more frequent
                if (try_destroy_repair(routes, vehs, emps, meta, enforce_soft, current_cost)) {
                    repairs++;
                    improved = true;
                }
            }
            
            next_iter:
            
            double curr_cost = 0;
            for (size_t v = 0; v < routes.size(); v++)
                curr_cost += route_cost_real(routes[v], vehs[v].start_node, emps, vehs[v]);
            
            if (improved) {
                no_improve = 0;
                if (curr_cost < best_cost) {
                    best_cost = curr_cost;
                    best = routes;
                    improvements++;
                    if (improvements % 10 == 0)
                        std::cout << "   Improvement #" << improvements << " - Total Cost: $" << best_cost << std::endl;
                }
            } else {
                no_improve++;
                if (no_improve >= 100) {
                    penalize_worst(routes, vehs, emps);
                    no_improve = 0;
                }
            }
            
            if (iters % 1000 == 0)
                std::cout << "   Iteration " << iters << ": $" << curr_cost << std::endl;
        }
        
        std::cout << "\nOptimization: " << iters << " iterations, " 
                  << improvements << " improvements\n";
        std::cout << "   Route splits: " << splits << ", Destroy-repairs: " << repairs << "\n";
        std::cout << "   Final cost: $" << best_cost << "\n";
        std::cout << std::string(60, '=') << std::endl;
        
        routes = best;
    }
};

#endif
