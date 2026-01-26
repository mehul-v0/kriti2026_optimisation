#ifndef VRP_GLS_H
#define VRP_GLS_H

#include "vrp_types.h"
#include "vrp_local_search.h"
#include "vrp_validators.h"
#include <chrono>
#include <iostream>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

class GuidedLocalSearch {
    std::vector<std::vector<int>> penalties;
    double LAMBDA = 0.5;
    int n_nodes;
    
public:
    GuidedLocalSearch(int n) : n_nodes(n) {
        penalties.resize(n, std::vector<int>(n, 0));
    }
    
    double gls_cost(int i, int j) const {
        return dist_matrix[i][j] + LAMBDA * penalties[i][j];
    }
    
    double route_cost_real(const std::vector<int>& route, int start,
                           const std::vector<Employee>& emps) const {
        if (route.empty()) return 0;
        double cost = 0;
        int curr = start;
        for (int e : route) {
            int next = emps[e].node_idx;
            cost += dist_matrix[curr][next];
            curr = next;
        }
        cost += dist_matrix[curr][OFFICE_NODE];
        return cost;
    }
    
    void penalize_worst(const std::vector<std::vector<int>>& routes,
                        const std::vector<Vehicle>& vehs, const std::vector<Employee>& emps) {
        int wu = -1, wv = -1;
        double max_util = -1;
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            int curr = vehs[v].start_node;
            
            for (int e : routes[v]) {
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
            best_cost += route_cost_real(routes[v], vehs[v].start_node, emps);
        
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
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft)) {
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
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft)) {
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
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft)) {
                                route = temp;
                                improved = true;
                                goto next_iter;
                            }
                        }
                    }
                }
            }
            
            next_iter:
            
            if (improved) {
                no_improve = 0;
                double curr_cost = 0;
                for (size_t v = 0; v < routes.size(); v++)
                    curr_cost += route_cost_real(routes[v], vehs[v].start_node, emps);
                
                if (curr_cost < best_cost) {
                    best_cost = curr_cost;
                    best = routes;
                    improvements++;
                    if (improvements % 10 == 0)
                        std::cout << "   Improvement #" << improvements << ": $" << best_cost << std::endl;
                }
            } else {
                no_improve++;
                if (no_improve >= 100) {
                    penalize_worst(routes, vehs, emps);
                    no_improve = 0;
                }
            }
            
            if (iters % 1000 == 0)
                std::cout << "   Iteration " << iters << ", Best: $" << best_cost << std::endl;
        }
        
        std::cout << "\nOptimization: " << iters << " iterations, " 
                  << improvements << " improvements\n";
        std::cout << "   Final cost: $" << best_cost << "\n";
        std::cout << std::string(60, '=') << std::endl;
        
        routes = best;
    }
};

#endif
