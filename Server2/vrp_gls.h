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
    
    void optimize(std::vector<std::vector<int>>& routes, std::vector<std::vector<int>>& best,
                  double& best_cost, const std::vector<Vehicle>& vehs,
                  const std::vector<Employee>& emps, const Metadata& meta,
                  bool enforce_soft, int time_limit = 10) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "GUIDED LOCAL SEARCH\n";
        std::cout << std::string(60, '=') << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        best_cost = 0;
        for (size_t v = 0; v < routes.size(); v++)
            best_cost += route_cost_real(routes[v], vehs[v].start_node, emps, vehs[v]);
        
        best = routes;
        std::cout << "Initial cost: $" << best_cost << std::endl;
        
        int iters = 0, improvements = 0, no_improve = 0;
        
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
        std::cout << "   Final cost: $" << best_cost << "\n";
        std::cout << std::string(60, '=') << std::endl;
        
        routes = best;
    }
};

#endif
