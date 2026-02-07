#ifndef VRP_ALNS_H
#define VRP_ALNS_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include "vrp_local_search.h"
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>
#include <unordered_set>
#include <map>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

/**
 * Adaptive Large Neighborhood Search (ALNS) for VRP
 * 
 * CRITICAL INVARIANT: ALL employees must ALWAYS be assigned.
 * No employee can ever be dropped from the solution.
 * Solutions with missing employees get massive penalty.
 */
class AdaptiveLargeNeighborhoodSearch {
private:
    enum DestroyOperator {
        RANDOM_REMOVAL = 0,
        WORST_REMOVAL = 1,
        SHAW_REMOVAL = 2,
        ROUTE_REMOVAL = 3,
        VIOLATION_REMOVAL = 4,
        CONSOLIDATION_REMOVAL = 5,
        CROSS_VEHICLE_REMOVAL = 6,
        VEHICLE_ELIMINATION = 7,
        NUM_DESTROY_OPS = 8
    };
    
    enum RepairOperator {
        GREEDY_INSERTION = 0,
        REGRET_INSERTION = 1,
        NEAREST_INSERTION = 2,
        BATCHING_INSERTION = 3,
        NUM_REPAIR_OPS = 4
    };
    
    std::vector<double> destroy_weights;
    std::vector<double> repair_weights;
    std::vector<int> destroy_attempts;
    std::vector<int> repair_attempts;
    std::vector<int> destroy_successes;
    std::vector<int> repair_successes;
    
    double start_temp = 10000.0;
    double cooling_rate = 0.99975;
    double min_destroy_pct = 0.15;
    double max_destroy_pct = 0.40;
    
    double sigma1 = 33.0;
    double sigma2 = 9.0;
    double sigma3 = 3.0;
    double decay_factor = 0.8;
    
    static constexpr double UNASSIGNED_PENALTY = 100000.0;
    static constexpr double SOFT_VIOLATION_PENALTY = 10000.0;
    
    int total_employees = 0;
    bool user_enforce_soft = true;  // The user's setting (preserved for output)
    
    std::mt19937 rng;
    const ConstraintEngine* cp_ptr = nullptr;
    
public:
    AdaptiveLargeNeighborhoodSearch() : rng(std::random_device{}()) {
        destroy_weights.resize(NUM_DESTROY_OPS, 1.0);
        // Give consolidation, cross-vehicle, and vehicle elimination higher initial weight
        destroy_weights[CONSOLIDATION_REMOVAL] = 2.0;
        destroy_weights[CROSS_VEHICLE_REMOVAL] = 1.5;
        destroy_weights[VEHICLE_ELIMINATION] = 2.5;
        repair_weights.resize(NUM_REPAIR_OPS, 1.0);
        repair_weights[BATCHING_INSERTION] = 2.0;  // Boost batching
        destroy_attempts.resize(NUM_DESTROY_OPS, 0);
        repair_attempts.resize(NUM_REPAIR_OPS, 0);
        destroy_successes.resize(NUM_DESTROY_OPS, 0);
        repair_successes.resize(NUM_REPAIR_OPS, 0);
    }
    
    void set_constraint_engine(const ConstraintEngine* cp) {
        cp_ptr = cp;
    }
    
    int count_assigned(const std::vector<std::vector<int>>& routes) const {
        int count = 0;
        for (const auto& route : routes) count += (int)route.size();
        return count;
    }
    
    // Count all violations across all routes using shared utilities
    // Returns {total_hard_violations (time + pref), total_pref_violations, total_lateness}
    struct AllViolations {
        int hard_time;
        int pref;
        int total_lateness;
        int total_hard() const { return hard_time + pref; }
    };
    
    // Cost function with PRIORITY ORDER:
    //   0. Unassigned employees (UNASSIGNED_PENALTY each)
    //   1. Hard violations (HARD_VIOLATION_PENALTY each + lateness penalty)
    //   2. Soft violations (SOFT_VIOLATION_PENALTY each)
    //   3. Actual score = cost_weight * distance_cost + time_weight * total_time
    static constexpr double HARD_VIOLATION_PENALTY = 50000.0;
    static constexpr double LATENESS_PENALTY_PER_MIN = 500.0;
    
    // Combined cost + violation calculation in a single pass (avoids double simulate_route)
    struct CostResult {
        double cost;
        int hard_violations;
    };
    
    // Per-route cost cache for delta evaluation (opt 7)
    struct RouteCostEntry {
        double dist_cost;     // distance * cost_per_km
        double time;          // office_arrival - available_from
        int hard_time;        // hard time violations
        int pref;             // preference violations
        int total_lateness;   // total lateness minutes
    };
    std::vector<RouteCostEntry> route_cache;
    
    CostResult calculate_cost_and_violations(const std::vector<std::vector<int>>& routes,
                                              const std::vector<Vehicle>& vehs,
                                              const std::vector<Employee>& emps,
                                              const Metadata& meta) const {
        double total_dist_cost = 0.0;
        double total_time = 0.0;
        int assigned = 0;
        AllViolations av = {0, 0, 0};
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            assigned += (int)routes[v].size();
            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
            total_dist_cost += sim.total_distance * vehs[v].cost_per_km;
            total_time += (sim.office_arrival - vehs[v].available_from);
            ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
            av.hard_time += vc.hard_time_violations;
            av.pref += vc.pref_violations;
            av.total_lateness += vc.total_lateness;
        }
        
        double score = meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
        int unassigned_count = total_employees - assigned;
        score += unassigned_count * UNASSIGNED_PENALTY;
        score += av.total_hard() * HARD_VIOLATION_PENALTY;
        score += av.total_lateness * LATENESS_PENALTY_PER_MIN;
        
        return {score, av.total_hard()};
    }
    
    // ======================== DELTA-BASED COST EVALUATION (OPT 7) ========================
    // Build the per-route cost cache (full simulation of all routes)
    void build_route_cache(const std::vector<std::vector<int>>& routes,
                           const std::vector<Vehicle>& vehs,
                           const std::vector<Employee>& emps) {
        route_cache.resize(routes.size());
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) {
                route_cache[v] = {0, 0, 0, 0, 0};
            } else {
                RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                route_cache[v] = {
                    sim.total_distance * vehs[v].cost_per_km,
                    (double)(sim.office_arrival - vehs[v].available_from),
                    vc.hard_time_violations,
                    vc.pref_violations,
                    vc.total_lateness
                };
            }
        }
    }
    
    // Compute total cost from route cache
    CostResult cost_from_cache(const Metadata& meta, int assigned) const {
        double total_dist_cost = 0, total_time = 0;
        AllViolations av = {0, 0, 0};
        for (size_t v = 0; v < route_cache.size(); v++) {
            total_dist_cost += route_cache[v].dist_cost;
            total_time += route_cache[v].time;
            av.hard_time += route_cache[v].hard_time;
            av.pref += route_cache[v].pref;
            av.total_lateness += route_cache[v].total_lateness;
        }
        double score = meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
        int unassigned_count = total_employees - assigned;
        score += unassigned_count * UNASSIGNED_PENALTY;
        score += av.total_hard() * HARD_VIOLATION_PENALTY;
        score += av.total_lateness * LATENESS_PENALTY_PER_MIN;
        return {score, av.total_hard()};
    }
    
    // Update cache for only the modified routes, then compute total cost
    CostResult delta_evaluate(const std::vector<std::vector<int>>& routes,
                              const std::vector<Vehicle>& vehs,
                              const std::vector<Employee>& emps,
                              const Metadata& meta,
                              const std::vector<int>& modified_vehicles) {
        int assigned = count_assigned(routes);
        for (int v : modified_vehicles) {
            if (routes[v].empty()) {
                route_cache[v] = {0, 0, 0, 0, 0};
            } else {
                RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                route_cache[v] = {
                    sim.total_distance * vehs[v].cost_per_km,
                    (double)(sim.office_arrival - vehs[v].available_from),
                    vc.hard_time_violations,
                    vc.pref_violations,
                    vc.total_lateness
                };
            }
        }
        return cost_from_cache(meta, assigned);
    }
    
    // ======================== CP-GUIDED FEASIBILITY CHECK (OPT 6) ========================
    // Quick check using ConstraintEngine data to prune obviously infeasible insertions.
    // Returns false if inserting `emp` into `route` on vehicle `v` is definitely infeasible.
    bool cp_quick_feasible(int emp, const std::vector<int>& route, int v,
                           const std::vector<Vehicle>& vehs,
                           const std::vector<Employee>& emps) const {
        if (!cp_ptr) return true; // No CP engine, skip pruning
        
        // Check vehicle preference compatibility
        if (emp < (int)cp_ptr->employee_vars.size()) {
            if (!cp_ptr->employee_vars[emp].is_vehicle_valid(v)) {
                return false; // CP pruned this vehicle for this employee
            }
        }
        
        // Check incompatibility with existing route members
        for (int existing : route) {
            if (!cp_ptr->are_compatible(emp, existing)) {
                return false; // Time-incompatible or sharing-pref violation
            }
        }
        
        // Quick capacity check
        if ((int)route.size() >= vehs[v].capacity) return false;
        
        return true;
    }
    
    // Backward-compatible wrapper
    double calculate_cost(const std::vector<std::vector<int>>& routes,
                         const std::vector<Vehicle>& vehs,
                         const std::vector<Employee>& emps,
                         const Metadata& meta) const {
        return calculate_cost_and_violations(routes, vehs, emps, meta).cost;
    }
    
    int select_operator(const std::vector<double>& weights) {
        double sum = 0;
        for (double w : weights) sum += w;
        std::uniform_real_distribution<> dis(0, sum);
        double r = dis(rng);
        double cumsum = 0;
        for (size_t i = 0; i < weights.size(); i++) {
            cumsum += weights[i];
            if (r <= cumsum) return (int)i;
        }
        return (int)weights.size() - 1;
    }
    
    void update_weights() {
        for (int i = 0; i < NUM_DESTROY_OPS; i++) {
            if (destroy_attempts[i] > 0) {
                double success_rate = (double)destroy_successes[i] / destroy_attempts[i];
                destroy_weights[i] = decay_factor * destroy_weights[i] + (1 - decay_factor) * success_rate * 10.0;
                destroy_weights[i] = std::max(0.1, destroy_weights[i]);
            }
        }
        for (int i = 0; i < NUM_REPAIR_OPS; i++) {
            if (repair_attempts[i] > 0) {
                double success_rate = (double)repair_successes[i] / repair_attempts[i];
                repair_weights[i] = decay_factor * repair_weights[i] + (1 - decay_factor) * success_rate * 10.0;
                repair_weights[i] = std::max(0.1, repair_weights[i]);
            }
        }
    }
    
    // ================================================================
    // FORCE INSERT: Guarantees ALL employees are placed.
    // Uses progressive constraint relaxation:
    //   1. Try feasible insertion (all constraints)
    //   2. Relax soft constraints
    //   3. Ignore time windows (capacity only)
    //   4. Absolute last resort: smallest route
    // ================================================================
    void force_insert_all(std::vector<std::vector<int>>& routes,
                         std::vector<int>& unassigned,
                         const std::vector<Vehicle>& vehs,
                         const std::vector<Employee>& emps,
                         const Metadata& meta,
                         bool enforce_soft) {
        
        while (!unassigned.empty()) {
            int emp = unassigned.back();
            
            // Level 1: Find cheapest feasible position (with soft constraints)
            {
                double best_cost = 1e18;
                int best_v = -1, best_pos = -1;
                
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        routes[v].insert(routes[v].begin() + pos, emp);
                        int h = 0, s = 0;
                        bool valid = validate_full_route(routes[v], vehs[v], emps, h, s, enforce_soft, meta);
                        routes[v].erase(routes[v].begin() + pos);
                        if (valid) {
                            int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                            int curr = emps[emp].node_idx;
                            int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                            double cost = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                            if (cost < best_cost) {
                                best_cost = cost;
                                best_v = (int)v;
                                best_pos = (int)pos;
                            }
                        }
                    }
                }
                if (best_v >= 0) {
                    routes[best_v].insert(routes[best_v].begin() + best_pos, emp);
                    unassigned.pop_back();
                    continue;
                }
            }
            
            // Level 2: Relax soft constraints (hard only)
            if (enforce_soft) {
                double best_cost = 1e18;
                int best_v = -1, best_pos = -1;
                
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        routes[v].insert(routes[v].begin() + pos, emp);
                        int h = 0, s = 0;
                        bool valid = validate_full_route(routes[v], vehs[v], emps, h, s, false, meta);
                        routes[v].erase(routes[v].begin() + pos);
                        if (valid) {
                            int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                            int curr = emps[emp].node_idx;
                            int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                            double cost = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                            if (cost < best_cost) {
                                best_cost = cost;
                                best_v = (int)v;
                                best_pos = (int)pos;
                            }
                        }
                    }
                }
                if (best_v >= 0) {
                    routes[best_v].insert(routes[best_v].begin() + best_pos, emp);
                    unassigned.pop_back();
                    continue;
                }
            }
            
            // Level 3: Allow hard violations, minimize lateness
            // This is critical for inherently infeasible employees (e.g., E10 in TC04)
            {
                double best_cost = 1e18;
                int best_v = -1, best_pos = -1;
                
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        routes[v].insert(routes[v].begin() + pos, emp);
                        int h = 0, s = 0;
                        // Allow hard violations — just measure how bad
                        validate_full_route(routes[v], vehs[v], emps, h, s, false, meta, true);
                        
                        // Use shared simulation + violation counting
                        RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                        ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                        routes[v].erase(routes[v].begin() + pos);
                        
                        // Cost = heavy penalty per violation + lateness + distance cost
                        double cost = h * HARD_VIOLATION_PENALTY;
                        cost += vc.pref_violations * HARD_VIOLATION_PENALTY;
                        cost += vc.total_lateness * LATENESS_PENALTY_PER_MIN;
                        cost += sim.total_distance * vehs[v].cost_per_km;
                        
                        if (cost < best_cost) {
                            best_cost = cost;
                            best_v = (int)v;
                            best_pos = (int)pos;
                        }
                    }
                }
                if (best_v >= 0) {
                    routes[best_v].insert(routes[best_v].begin() + best_pos, emp);
                    unassigned.pop_back();
                    continue;
                }
            }
            
            // Level 4: Absolute last resort — put in smallest route
            {
                size_t min_v = 0;
                int min_size = (int)routes[0].size();
                for (size_t v = 1; v < routes.size(); v++) {
                    if ((int)routes[v].size() < min_size) {
                        min_size = (int)routes[v].size();
                        min_v = v;
                    }
                }
                routes[min_v].push_back(emp);
                unassigned.pop_back();
            }
        }
    }
    
    // ======================== LATENESS-AWARE INSERT ========================
    // Used as a second pass when strict validation rejects employees.
    // Tries every position with allow_hard_violations=true,
    // picks the position that minimizes lateness (NOT just random append).
    // Critical for inherently infeasible employees like E10 in TC04 who
    // physically cannot arrive on time regardless of placement.
    void lateness_aware_insert(std::vector<std::vector<int>>& routes,
                               std::vector<int>& unassigned,
                               const std::vector<Vehicle>& vehs,
                               const std::vector<Employee>& emps,
                               const Metadata& meta) {
        
        int safety = (int)unassigned.size() * 3 + 10;
        int iter = 0;
        
        while (!unassigned.empty() && iter < safety) {
            iter++;
            double best_score = 1e18;
            int best_emp_idx = -1, best_veh = -1, best_pos = -1;
            
            for (size_t ue = 0; ue < unassigned.size(); ue++) {
                int emp = unassigned[ue];
                
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        routes[v].insert(routes[v].begin() + pos, emp);
                        int h = 0, s = 0;
                        // ALLOW hard violations — just measure them
                        validate_full_route(routes[v], vehs[v], emps, h, s, false, meta, true);
                        
                        // Use shared simulation + violation counting
                        RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                        ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                        routes[v].erase(routes[v].begin() + pos);
                        
                        double dist_cost = 0;
                        {
                            int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos > 0 ? pos-1 : 0]].node_idx;
                            int curr_n = emps[emp].node_idx;
                            int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                            if (pos == 0 && routes[v].empty()) {
                                prev = vehs[v].start_node;
                                next = OFFICE_NODE;
                            }
                            dist_cost = (dist_matrix[prev][curr_n] + dist_matrix[curr_n][next]) * vehs[v].cost_per_km;
                        }
                        
                        // Priority: minimize hard violations + pref violations → minimize lateness → minimize cost
                        double score = h * HARD_VIOLATION_PENALTY + vc.pref_violations * HARD_VIOLATION_PENALTY
                                     + s * SOFT_VIOLATION_PENALTY + vc.total_lateness * LATENESS_PENALTY_PER_MIN + dist_cost;
                        
                        if (score < best_score) {
                            best_score = score;
                            best_emp_idx = (int)ue;
                            best_veh = (int)v;
                            best_pos = (int)pos;
                        }
                    }
                }
            }
            
            if (best_emp_idx >= 0) {
                routes[best_veh].insert(routes[best_veh].begin() + best_pos, unassigned[best_emp_idx]);
                unassigned.erase(unassigned.begin() + best_emp_idx);
            } else {
                break; // No positions with capacity — force_insert_all handles
            }
        }
    }
    
    // ======================== DESTROY OPERATORS ========================
    
    std::vector<int> destroy_random(std::vector<std::vector<int>>& routes, int num_remove) {
        std::vector<int> removed;
        std::vector<std::pair<int, int>> all_positions;
        
        for (size_t v = 0; v < routes.size(); v++) {
            for (size_t i = 0; i < routes[v].size(); i++) {
                all_positions.push_back({(int)v, (int)i});
            }
        }
        if (all_positions.empty()) return removed;
        
        std::shuffle(all_positions.begin(), all_positions.end(), rng);
        num_remove = std::min(num_remove, (int)all_positions.size());
        
        std::vector<std::pair<int, int>> to_remove(
            all_positions.begin(), all_positions.begin() + num_remove);
        std::sort(to_remove.begin(), to_remove.end(),
                 [](const auto& a, const auto& b) {
                     return a.first != b.first ? a.first > b.first : a.second > b.second;
                 });
        
        for (auto& [v, pos] : to_remove) {
            removed.push_back(routes[v][pos]);
            routes[v].erase(routes[v].begin() + pos);
        }
        return removed;
    }
    
    std::vector<int> destroy_worst(std::vector<std::vector<int>>& routes,
                                   int num_remove,
                                   const std::vector<Vehicle>& vehs,
                                   const std::vector<Employee>& emps) {
        std::vector<int> removed;
        // Store {saving, employee_id} instead of positions to avoid index invalidation
        std::vector<std::pair<double, int>> costs;
        
        for (size_t v = 0; v < routes.size(); v++) {
            for (size_t i = 0; i < routes[v].size(); i++) {
                int emp = routes[v][i];
                int prev = (i == 0) ? vehs[v].start_node : emps[routes[v][i-1]].node_idx;
                int curr = emps[emp].node_idx;
                int next = (i == routes[v].size()-1) ? OFFICE_NODE : emps[routes[v][i+1]].node_idx;
                double saving = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                costs.push_back({saving, emp});
            }
        }
        if (costs.empty()) return removed;
        
        std::sort(costs.begin(), costs.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
        
        num_remove = std::min(num_remove, (int)costs.size());
        int pool = std::min((int)costs.size(), std::max(num_remove, num_remove * 2));
        std::shuffle(costs.begin(), costs.begin() + pool, rng);
        
        // Collect employee IDs to remove
        std::unordered_set<int> to_remove_set;
        for (int i = 0; i < num_remove; i++) {
            to_remove_set.insert(costs[i].second);
        }
        
        // Remove by employee ID (safe regardless of index shifts)
        for (auto& route : routes) {
            for (int i = (int)route.size() - 1; i >= 0; i--) {
                if (to_remove_set.count(route[i])) {
                    removed.push_back(route[i]);
                    route.erase(route.begin() + i);
                }
            }
        }
        return removed;
    }
    
    std::vector<int> destroy_shaw(std::vector<std::vector<int>>& routes,
                                  int num_remove,
                                  const std::vector<Vehicle>& /*vehs*/,
                                  const std::vector<Employee>& emps) {
        std::vector<int> removed;
        std::vector<int> all_emps;
        for (const auto& route : routes)
            for (int e : route) all_emps.push_back(e);
        
        if (all_emps.empty()) return removed;
        
        std::uniform_int_distribution<> dis(0, (int)all_emps.size() - 1);
        int seed = all_emps[dis(rng)];
        
        std::vector<std::pair<double, int>> similarities;
        for (int emp : all_emps) {
            if (emp == seed) continue;
            double dist_sim = dist_matrix[emps[seed].node_idx][emps[emp].node_idx];
            double time_sim = std::abs(emps[seed].earliest_pickup - emps[emp].earliest_pickup);
            similarities.push_back({dist_sim + time_sim * 0.01, emp});
        }
        std::sort(similarities.begin(), similarities.end());
        
        std::vector<int> to_remove_emps = {seed};
        for (int i = 0; i < num_remove - 1 && i < (int)similarities.size(); i++) {
            to_remove_emps.push_back(similarities[i].second);
        }
        
        std::unordered_set<int> to_remove_set(to_remove_emps.begin(), to_remove_emps.end());
        for (auto& route : routes) {
            for (int i = (int)route.size() - 1; i >= 0; i--) {
                if (to_remove_set.count(route[i])) {
                    removed.push_back(route[i]);
                    route.erase(route.begin() + i);
                }
            }
        }
        return removed;
    }
    
    std::vector<int> destroy_route(std::vector<std::vector<int>>& routes, int num_remove) {
        std::vector<int> removed;
        std::vector<int> non_empty;
        for (size_t v = 0; v < routes.size(); v++)
            if (!routes[v].empty()) non_empty.push_back((int)v);
        
        if (non_empty.empty()) return removed;
        
        std::shuffle(non_empty.begin(), non_empty.end(), rng);
        int num_routes = std::min(2, (int)non_empty.size());
        
        for (int i = 0; i < num_routes; i++) {
            int v = non_empty[i];
            for (int emp : routes[v]) {
                removed.push_back(emp);
                if ((int)removed.size() >= num_remove) break;
            }
            routes[v].clear();
            if ((int)removed.size() >= num_remove) break;
        }
        return removed;
    }
    
    // VIOLATION-TARGETED REMOVAL: Removes employees that are causing soft violations
    // This directs the search toward violation reduction
    std::vector<int> destroy_violation(std::vector<std::vector<int>>& routes,
                                       int num_remove,
                                       const std::vector<Vehicle>& vehs,
                                       const std::vector<Employee>& emps) {
        std::vector<int> removed;
        
        // Find all employees involved in soft violations
        std::vector<std::pair<int, int>> violation_emps;  // (violation_count, emp_index_in_route)
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
            int sz = (int)routes[v].size();
            for (size_t i = 0; i < routes[v].size(); i++) {
                int e = routes[v][i];
                int violations = 0;
                // Sharing pref violation
                if (emps[e].sharing_pref < sz) violations++;
                // Vehicle pref violation  
                if (emps[e].vehicle_pref == 1 && vehs[v].category != 1) violations++;
                if (emps[e].vehicle_pref == 2 && vehs[v].category == 1) violations++;
                // Time window violation
                if (sim.office_arrival > emps[e].latest_arrival_deadline) violations++;
                if (violations > 0) {
                    violation_emps.push_back({violations, e});
                }
            }
        }
        
        if (violation_emps.empty()) {
            // No violations — fall back to random removal
            return destroy_random(routes, num_remove);
        }
        
        // Sort by violation count (most violations first)
        std::sort(violation_emps.begin(), violation_emps.end(),
                 [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Remove violation-causing employees first
        std::unordered_set<int> to_remove_set;
        for (const auto& [viol, emp] : violation_emps) {
            to_remove_set.insert(emp);
            if ((int)to_remove_set.size() >= num_remove) break;
        }
        
        // Also remove some neighbors of violating employees to create room
        if ((int)to_remove_set.size() < num_remove) {
            for (auto& route : routes) {
                for (int e : route) {
                    if (to_remove_set.count(e)) {
                        // Add adjacent employees in the same route
                        for (int e2 : route) {
                            if (!to_remove_set.count(e2)) {
                                to_remove_set.insert(e2);
                                if ((int)to_remove_set.size() >= num_remove) break;
                            }
                        }
                    }
                    if ((int)to_remove_set.size() >= num_remove) break;
                }
                if ((int)to_remove_set.size() >= num_remove) break;
            }
        }
        
        // Remove from routes
        for (auto& route : routes) {
            for (int i = (int)route.size() - 1; i >= 0; i--) {
                if (to_remove_set.count(route[i])) {
                    removed.push_back(route[i]);
                    route.erase(route.begin() + i);
                }
            }
        }
        return removed;
    }
    
    // CONSOLIDATION REMOVAL: Removes ALL employees from trips belonging to 
    // a random physical vehicle, so they can be re-batched more efficiently.
    // This is the key operator for finding solutions like "put 3 employees
    // in a single trip instead of 3 separate trips."
    std::vector<int> destroy_consolidation(std::vector<std::vector<int>>& routes,
                                           int /*num_remove*/,
                                           const std::vector<Vehicle>& vehs) {
        std::vector<int> removed;
        
        // Find physical vehicles that have employees across multiple trips
        std::map<int, std::vector<int>> phys_to_virt;
        for (size_t v = 0; v < routes.size(); v++) {
            if (!routes[v].empty()) {
                phys_to_virt[vehs[v].physical_id].push_back((int)v);
            }
        }
        
        // Collect all physical vehicles with employees (or just pick random ones)
        std::vector<int> phys_ids;
        for (auto& [pid, virts] : phys_to_virt) phys_ids.push_back(pid);
        
        if (phys_ids.empty()) return removed;
        
        // Randomly select 1-2 physical vehicles and remove ALL their employees
        std::shuffle(phys_ids.begin(), phys_ids.end(), rng);
        int num_phys = std::min(2, (int)phys_ids.size());
        
        for (int i = 0; i < num_phys; i++) {
            int pid = phys_ids[i];
            for (int virt_idx : phys_to_virt[pid]) {
                for (int e : routes[virt_idx]) {
                    removed.push_back(e);
                }
                routes[virt_idx].clear();
            }
        }
        
        return removed;
    }
    
    // CROSS-VEHICLE REMOVAL: Removes employees that might be better served by a 
    // different vehicle (e.g., currently on expensive V03 but could go on cheaper V02).
    // Targets employees on high cost-per-km vehicles.
    std::vector<int> destroy_cross_vehicle(std::vector<std::vector<int>>& routes,
                                            int num_remove,
                                            const std::vector<Vehicle>& vehs,
                                            const std::vector<Employee>& emps) {
        std::vector<int> removed;
        
        // Score each employee by: (cost on current vehicle) vs (cheapest possible vehicle)
        // Higher score = more benefit from moving
        std::vector<std::tuple<double, int, int>> move_benefit; // (benefit, veh_idx, pos)
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            double curr_cpm = vehs[v].cost_per_km;
            
            for (size_t i = 0; i < routes[v].size(); i++) {
                int emp = routes[v][i];
                // Calculate insertion cost at current position
                int prev = (i == 0) ? vehs[v].start_node : emps[routes[v][i-1]].node_idx;
                int curr = emps[emp].node_idx;
                int next = (i == routes[v].size()-1) ? OFFICE_NODE : emps[routes[v][i+1]].node_idx;
                double delta_dist = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                double curr_cost = delta_dist * curr_cpm;
                
                // Find cheapest alternative vehicle's cost_per_km
                double min_alt_cpm = 1e9;
                for (size_t v2 = 0; v2 < routes.size(); v2++) {
                    if (v2 == v) continue;
                    if (vehs[v2].cost_per_km < min_alt_cpm) {
                        min_alt_cpm = vehs[v2].cost_per_km;
                    }
                }
                double alt_cost = delta_dist * min_alt_cpm;
                double benefit = curr_cost - alt_cost;
                
                move_benefit.push_back({benefit, (int)v, (int)i});
            }
        }
        
        if (move_benefit.empty()) return removed;
        
        // Sort by benefit (highest first)
        std::sort(move_benefit.begin(), move_benefit.end(),
                 [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
        
        // Remove top candidates using employee-ID-based removal (avoids index invalidation)
        num_remove = std::min(num_remove, (int)move_benefit.size());
        
        std::unordered_set<int> to_remove_set;
        for (int i = 0; i < num_remove; i++) {
            int v_idx = std::get<1>(move_benefit[i]);
            int pos = std::get<2>(move_benefit[i]);
            if (pos < (int)routes[v_idx].size()) {
                to_remove_set.insert(routes[v_idx][pos]);
            }
        }
        
        for (auto& route : routes) {
            for (int i = (int)route.size() - 1; i >= 0; i--) {
                if (to_remove_set.count(route[i])) {
                    removed.push_back(route[i]);
                    route.erase(route.begin() + i);
                }
            }
        }
        return removed;
    }
    
    // VEHICLE ELIMINATION: Removes ALL employees from the most expensive active vehicle.
    // Forces redistribution to cheaper vehicles, reducing total cost.
    // This is the key operator for finding solutions that use fewer vehicles.
    std::vector<int> destroy_vehicle_elimination(std::vector<std::vector<int>>& routes,
                                                  int /*num_remove*/,
                                                  const std::vector<Vehicle>& vehs) {
        std::vector<int> removed;
        
        // Find all active physical vehicles and their total cost
        struct PhysVehInfo {
            int phys_id;
            double total_cost;
            int total_emps;
            std::vector<int> virt_indices;
        };
        std::map<int, PhysVehInfo> phys_info;
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            int pid = vehs[v].physical_id;
            if (phys_info.find(pid) == phys_info.end()) {
                phys_info[pid] = {pid, 0.0, 0, {}};
            }
            phys_info[pid].virt_indices.push_back((int)v);
            phys_info[pid].total_emps += (int)routes[v].size();
            // Estimate cost
            for (size_t vi = 0; vi < route_cache.size() && vi == (size_t)v; ) {
                phys_info[pid].total_cost += route_cache[v].dist_cost;
                break;
            }
        }
        
        if (phys_info.size() <= 1) return removed; // Need at least 2 active vehicles
        
        // Strategy: Pick vehicle to eliminate based on cost-effectiveness
        // Higher cost_per_km and fewer employees = better candidate for elimination
        std::vector<std::pair<double, int>> candidates;
        for (auto& [pid, info] : phys_info) {
            // Score: higher = more worth eliminating
            // Use cost_per_km of the physical vehicle as proxy
            double cpm = 0;
            for (int vi : info.virt_indices) cpm = std::max(cpm, vehs[vi].cost_per_km);
            double score = cpm / std::max(1, info.total_emps); // expensive per employee = eliminate
            candidates.push_back({score, pid});
        }
        std::sort(candidates.begin(), candidates.end(),
                 [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Sometimes pick the most expensive, sometimes random for diversity
        int pick = 0;
        std::uniform_real_distribution<> d01(0, 1);
        if (d01(rng) < 0.3 && candidates.size() > 1) {
            std::uniform_int_distribution<> di(0, (int)candidates.size() - 1);
            pick = di(rng);
        }
        int target_pid = candidates[pick].second;
        
        // Remove ALL employees from this physical vehicle
        for (int vi : phys_info[target_pid].virt_indices) {
            for (int e : routes[vi]) removed.push_back(e);
            routes[vi].clear();
        }
        
        return removed;
    }
    
    // ======================== REPAIR OPERATORS ========================
    // Every repair operator ends with force_insert_all() to guarantee
    // that ALL employees are assigned. No employee is ever dropped.
    
    void repair_greedy(std::vector<std::vector<int>>& routes,
                      std::vector<int>& unassigned,
                      const std::vector<Vehicle>& vehs,
                      const std::vector<Employee>& emps,
                      const Metadata& meta,
                      bool enforce_soft) {
        
        int safety = (int)unassigned.size() * 3 + 10;
        int iter = 0;
        
        while (!unassigned.empty() && iter < safety) {
            iter++;
            double best_cost = 1e18;
            int best_emp = -1, best_veh = -1, best_pos = -1;
            
            for (size_t ue = 0; ue < unassigned.size(); ue++) {
                int emp = unassigned[ue];
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    // CP-guided pruning (opt 6)
                    if (!cp_quick_feasible(emp, routes[v], (int)v, vehs, emps)) continue;
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        int curr = emps[emp].node_idx;
                        int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                        double delta_dist = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                        double delta_dollars = delta_dist * vehs[v].cost_per_km;
                        double delta_time = (delta_dist / vehs[v].speed_kmph) * 60.0;
                        double cost = meta.cost_weight * delta_dollars + meta.time_weight * delta_time;
                        
                        if (cost < best_cost) {
                            // In-place insert, validate, then erase — avoids temp vector copy
                            routes[v].insert(routes[v].begin() + pos, emp);
                            int h = 0, s = 0;
                            bool valid = validate_full_route(routes[v], vehs[v], emps, h, s, enforce_soft, meta);
                            routes[v].erase(routes[v].begin() + pos);
                            if (valid) {
                                best_cost = cost;
                                best_emp = (int)ue;
                                best_veh = (int)v;
                                best_pos = (int)pos;
                            }
                        }
                    }
                }
            }
            
            if (best_emp >= 0) {
                routes[best_veh].insert(routes[best_veh].begin() + best_pos, unassigned[best_emp]);
                unassigned.erase(unassigned.begin() + best_emp);
            } else {
                break; // No feasible found — try with hard violations allowed
            }
        }
        
        // Second pass: Allow hard violations but minimize lateness
        // This handles inherently infeasible employees (e.g., E10 in TC04)
        if (!unassigned.empty()) {
            lateness_aware_insert(routes, unassigned, vehs, emps, meta);
        }
        
        // MANDATORY: place ALL remaining employees
        if (!unassigned.empty()) {
            force_insert_all(routes, unassigned, vehs, emps, meta, enforce_soft);
        }
    }
    
    void repair_regret(std::vector<std::vector<int>>& routes,
                      std::vector<int>& unassigned,
                      const std::vector<Vehicle>& vehs,
                      const std::vector<Employee>& emps,
                      const Metadata& meta,
                      bool enforce_soft) {
        
        const int K = 3;
        int safety = (int)unassigned.size() * 3 + 10;
        int iter = 0;
        
        while (!unassigned.empty() && iter < safety) {
            iter++;
            double best_regret = -1e18;
            int best_emp = -1, best_veh = -1, best_pos = -1;
            
            for (size_t ue = 0; ue < unassigned.size(); ue++) {
                int emp = unassigned[ue];
                std::vector<double> costs;
                std::vector<std::pair<int, int>> positions;
                
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    // CP-guided pruning (opt 6)
                    if (!cp_quick_feasible(emp, routes[v], (int)v, vehs, emps)) continue;
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        int curr = emps[emp].node_idx;
                        int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                        double cost = (dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next]) * vehs[v].cost_per_km;
                        
                        // In-place insert, validate, erase — avoids temp vector copy
                        routes[v].insert(routes[v].begin() + pos, emp);
                        int h = 0, s = 0;
                        bool valid = validate_full_route(routes[v], vehs[v], emps, h, s, enforce_soft, meta);
                        routes[v].erase(routes[v].begin() + pos);
                        if (valid) {
                            costs.push_back(cost);
                            positions.push_back({(int)v, (int)pos});
                        }
                    }
                }
                
                if (costs.empty()) continue;
                
                std::vector<size_t> indices(costs.size());
                std::iota(indices.begin(), indices.end(), 0);
                std::sort(indices.begin(), indices.end(),
                         [&costs](size_t a, size_t b) { return costs[a] < costs[b]; });
                
                double regret = 0;
                for (int k = 1; k < K && k < (int)costs.size(); k++) {
                    regret += costs[indices[k]] - costs[indices[0]];
                }
                if (costs.size() == 1) regret = 1e9; // Must insert now!
                
                if (regret > best_regret) {
                    best_regret = regret;
                    best_emp = (int)ue;
                    best_veh = positions[indices[0]].first;
                    best_pos = positions[indices[0]].second;
                }
            }
            
            if (best_emp >= 0) {
                routes[best_veh].insert(routes[best_veh].begin() + best_pos, unassigned[best_emp]);
                unassigned.erase(unassigned.begin() + best_emp);
            } else {
                break; // No feasible found — try with hard violations allowed
            }
        }
        
        // Second pass: Allow hard violations but minimize lateness
        if (!unassigned.empty()) {
            lateness_aware_insert(routes, unassigned, vehs, emps, meta);
        }
        
        // MANDATORY: place ALL remaining employees
        if (!unassigned.empty()) {
            force_insert_all(routes, unassigned, vehs, emps, meta, enforce_soft);
        }
    }
    
    void repair_nearest(std::vector<std::vector<int>>& routes,
                       std::vector<int>& unassigned,
                       const std::vector<Vehicle>& vehs,
                       const std::vector<Employee>& emps,
                       const Metadata& meta,
                       bool enforce_soft) {
        
        int safety = (int)unassigned.size() * 3 + 10;
        int iter = 0;
        
        while (!unassigned.empty() && iter < safety) {
            iter++;
            double best_dist = 1e18;
            int best_emp = -1, best_veh = -1, best_pos = -1;
            
            for (size_t ue = 0; ue < unassigned.size(); ue++) {
                int emp = unassigned[ue];
                int emp_node = emps[emp].node_idx;
                
                for (size_t v = 0; v < routes.size(); v++) {
                    if ((int)routes[v].size() >= vehs[v].capacity) continue;
                    // CP-guided pruning (opt 6)
                    if (!cp_quick_feasible(emp, routes[v], (int)v, vehs, emps)) continue;
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        int prev_node = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        double d = dist_matrix[prev_node][emp_node];
                        
                        if (d < best_dist) {
                            // In-place insert, validate, erase — avoids temp vector copy
                            routes[v].insert(routes[v].begin() + pos, emp);
                            int h = 0, s = 0;
                            bool valid = validate_full_route(routes[v], vehs[v], emps, h, s, enforce_soft, meta);
                            routes[v].erase(routes[v].begin() + pos);
                            if (valid) {
                                best_dist = d;
                                best_emp = (int)ue;
                                best_veh = (int)v;
                                best_pos = (int)pos;
                            }
                        }
                    }
                }
            }
            
            if (best_emp >= 0) {
                routes[best_veh].insert(routes[best_veh].begin() + best_pos, unassigned[best_emp]);
                unassigned.erase(unassigned.begin() + best_emp);
            } else {
                break; // No feasible found — try with hard violations allowed
            }
        }
        
        // Second pass: Allow hard violations but minimize lateness
        if (!unassigned.empty()) {
            lateness_aware_insert(routes, unassigned, vehs, emps, meta);
        }
        
        // MANDATORY: place ALL remaining employees
        if (!unassigned.empty()) {
            force_insert_all(routes, unassigned, vehs, emps, meta, enforce_soft);
        }
    }
    
    // BATCHING INSERTION: Sorts unassigned by geographic proximity, then inserts
    // clusters of nearby employees into the same route to maximize batching.
    // This is the key operator for finding solutions where multiple employees
    // share a single trip instead of getting separate trips.
    void repair_batching(std::vector<std::vector<int>>& routes,
                        std::vector<int>& unassigned,
                        const std::vector<Vehicle>& vehs,
                        const std::vector<Employee>& emps,
                        const Metadata& meta,
                        bool enforce_soft) {
        
        if (unassigned.empty()) return;
        
        // Sort unassigned by distance to office (closest first - they're along the way)
        std::sort(unassigned.begin(), unassigned.end(), [&](int a, int b) {
            return dist_matrix[emps[a].node_idx][OFFICE_NODE] < dist_matrix[emps[b].node_idx][OFFICE_NODE];
        });
        
        // Find the cheapest vehicles (lowest cost_per_km) that have capacity
        std::vector<size_t> veh_order(routes.size());
        std::iota(veh_order.begin(), veh_order.end(), 0);
        std::sort(veh_order.begin(), veh_order.end(), [&](size_t a, size_t b) {
            return vehs[a].cost_per_km < vehs[b].cost_per_km;
        });
        
        // Try to batch as many employees as possible into each vehicle, cheapest first
        for (size_t vi = 0; vi < veh_order.size() && !unassigned.empty(); vi++) {
            int v = (int)veh_order[vi];
            
            for (int attempt = 0; attempt < (int)unassigned.size() && !unassigned.empty(); ) {
                if ((int)routes[v].size() >= vehs[v].capacity) break;
                
                int emp = unassigned[attempt];
                
                // Try to find the best position in this route
                double best_cost = 1e18;
                int best_pos = -1;
                
                for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                    routes[v].insert(routes[v].begin() + pos, emp);
                    int h = 0, s = 0;
                    if (validate_full_route(routes[v], vehs[v], emps, h, s, enforce_soft, meta)) {
                        int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        int curr = emps[emp].node_idx;
                        int next = (pos + 1 >= routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos+1]].node_idx;
                        double delta = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                        double cost = meta.cost_weight * delta * vehs[v].cost_per_km +
                                      meta.time_weight * (delta / vehs[v].speed_kmph) * 60.0;
                        if (cost < best_cost) {
                            best_cost = cost;
                            best_pos = (int)pos;
                        }
                    }
                    routes[v].erase(routes[v].begin() + pos);
                }
                
                if (best_pos >= 0) {
                    routes[v].insert(routes[v].begin() + best_pos, emp);
                    unassigned.erase(unassigned.begin() + attempt);
                    // Don't increment attempt — next employee now at same index
                } else {
                    attempt++;
                }
            }
        }
        
        // Second pass: Allow hard violations but minimize lateness
        if (!unassigned.empty()) {
            lateness_aware_insert(routes, unassigned, vehs, emps, meta);
        }
        
        // MANDATORY: place ALL remaining employees
        if (!unassigned.empty()) {
            force_insert_all(routes, unassigned, vehs, emps, meta, enforce_soft);
        }
    }
    
    // ======================== INTRA-ROUTE LOCAL SEARCH ========================
    // Applies relocate, exchange, and 2-opt within each route to improve ordering.
    // Uses first-improvement strategy for speed.
    void apply_local_search(std::vector<std::vector<int>>& routes,
                            const std::vector<Vehicle>& vehs,
                            const std::vector<Employee>& emps,
                            const Metadata& /*meta*/) {
        for (size_t v = 0; v < routes.size(); v++) {
            if ((int)routes[v].size() < 2) continue;
            
            bool improved = true;
            int passes = 0;
            while (improved && passes < 3) {
                improved = false;
                passes++;
                
                // Relocate
                for (int from = 0; from < (int)routes[v].size() && !improved; from++) {
                    for (int to = 0; to <= (int)routes[v].size() && !improved; to++) {
                        if (from == to || from + 1 == to) continue;
                        MoveDelta md = LocalSearchOps::relocate_delta(
                            routes[v], from, to, vehs[v].start_node, emps, vehs[v].speed_kmph);
                        if (md.dist_cost < -1e-6) {
                            LocalSearchOps::apply_relocate(routes[v], from, to);
                            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                            ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                            if (vc.hard_time_violations == 0) {
                                improved = true;
                            } else {
                                // Undo: reverse the relocate
                                int actual_to = (from < to) ? (to - 1) : to;
                                LocalSearchOps::apply_relocate(routes[v], actual_to, from);
                            }
                        }
                    }
                }
                if (improved) continue;
                
                // Exchange
                for (int a = 0; a < (int)routes[v].size() && !improved; a++) {
                    for (int b = a + 1; b < (int)routes[v].size() && !improved; b++) {
                        MoveDelta md = LocalSearchOps::exchange_delta(
                            routes[v], a, b, vehs[v].start_node, emps, vehs[v].speed_kmph);
                        if (md.dist_cost < -1e-6) {
                            LocalSearchOps::apply_exchange(routes[v], a, b);
                            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                            ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                            if (vc.hard_time_violations == 0) {
                                improved = true;
                            } else {
                                LocalSearchOps::apply_exchange(routes[v], a, b); // undo swap
                            }
                        }
                    }
                }
                if (improved) continue;
                
                // 2-opt
                for (int i = 0; i < (int)routes[v].size() - 1 && !improved; i++) {
                    for (int j = i + 1; j < (int)routes[v].size() && !improved; j++) {
                        MoveDelta md = LocalSearchOps::twoopt_delta(
                            routes[v], i, j, vehs[v].start_node, emps, vehs[v].speed_kmph);
                        if (md.dist_cost < -1e-6) {
                            LocalSearchOps::apply_2opt(routes[v], i, j);
                            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                            ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                            if (vc.hard_time_violations == 0) {
                                improved = true;
                            } else {
                                LocalSearchOps::apply_2opt(routes[v], i, j); // undo reverse
                            }
                        }
                    }
                }
                if (improved) continue;
                
                // Or-opt: move segments of 2 or 3 consecutive employees (opt 8)
                for (int seg_len = 2; seg_len <= std::min(3, (int)routes[v].size()) && !improved; seg_len++) {
                    for (int from = 0; from + seg_len - 1 < (int)routes[v].size() && !improved; from++) {
                        for (int to = 0; to <= (int)routes[v].size() - seg_len && !improved; to++) {
                            if (to >= from && to <= from + seg_len) continue; // skip overlap
                            MoveDelta md = LocalSearchOps::oropt_delta(
                                routes[v], from, seg_len, to, vehs[v].start_node, emps, vehs[v].speed_kmph);
                            if (md.dist_cost < -1e-6) {
                                // Save route for undo
                                auto saved = routes[v];
                                LocalSearchOps::apply_oropt(routes[v], from, seg_len, to);
                                RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                                if (vc.hard_time_violations == 0) {
                                    improved = true;
                                } else {
                                    routes[v] = saved; // undo
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ======================== INTER-ROUTE MOVES ========================
    // Tries moving an employee from one route to another, or swapping between routes.
    // Cost-aware: accounts for different cost_per_km between vehicles.
    // First-improvement strategy.
    void apply_inter_route_moves(std::vector<std::vector<int>>& routes,
                                  const std::vector<Vehicle>& vehs,
                                  const std::vector<Employee>& emps,
                                  const Metadata& /*meta*/) {
        bool improved = true;
        int passes = 0;
        while (improved && passes < 3) {
            improved = false;
            passes++;
            
            // Inter-route relocate: move employee from route v1 to route v2
            for (size_t v1 = 0; v1 < routes.size() && !improved; v1++) {
                if (routes[v1].empty()) continue;
                for (int i = 0; i < (int)routes[v1].size() && !improved; i++) {
                    int emp = routes[v1][i];
                    int emp_node = emps[emp].node_idx;
                    
                    // Count current violations on v1
                    RouteSimResult sim_v1_before = simulate_route(routes[v1], vehs[v1], emps);
                    ViolationCount vc_v1_before = count_route_violations(routes[v1], vehs[v1], emps, sim_v1_before.office_arrival);
                    int before_violations = vc_v1_before.hard_time_violations + vc_v1_before.pref_violations;
                    
                    // Full cost of having this employee on v1 route
                    int prev1 = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                    int next1 = (i == (int)routes[v1].size()-1) ? OFFICE_NODE : emps[routes[v1][i+1]].node_idx;
                    double remove_saving = (dist_matrix[prev1][emp_node] + dist_matrix[emp_node][next1]
                                            - dist_matrix[prev1][next1]) * vehs[v1].cost_per_km;
                    
                    for (size_t v2 = 0; v2 < routes.size() && !improved; v2++) {
                        if (v1 == v2) continue;
                        if ((int)routes[v2].size() >= vehs[v2].capacity) continue;
                        
                        // Count current violations on v2
                        int v2_before_v = 0;
                        if (!routes[v2].empty()) {
                            RouteSimResult sim_v2_b = simulate_route(routes[v2], vehs[v2], emps);
                            ViolationCount vc_v2_b = count_route_violations(routes[v2], vehs[v2], emps, sim_v2_b.office_arrival);
                            v2_before_v = vc_v2_b.hard_time_violations + vc_v2_b.pref_violations;
                        }
                        int total_before_v = before_violations + v2_before_v;
                        
                        for (size_t pos = 0; pos <= routes[v2].size() && !improved; pos++) {
                            int prev2 = (pos == 0) ? vehs[v2].start_node : emps[routes[v2][pos-1]].node_idx;
                            int next2 = (pos == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][pos]].node_idx;
                            double insert_cost = (dist_matrix[prev2][emp_node] + dist_matrix[emp_node][next2]
                                                  - dist_matrix[prev2][next2]) * vehs[v2].cost_per_km;
                            
                            double net = insert_cost - remove_saving;
                            if (net < -1e-6 || total_before_v > 0) {
                                // Try the move: remove from v1, insert into v2
                                routes[v1].erase(routes[v1].begin() + i);
                                routes[v2].insert(routes[v2].begin() + pos, emp);
                                
                                // Validate both routes — check ALL violations
                                int v1_after_v = 0;
                                if (!routes[v1].empty()) {
                                    RouteSimResult sim1 = simulate_route(routes[v1], vehs[v1], emps);
                                    ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, sim1.office_arrival);
                                    v1_after_v = vc1.hard_time_violations + vc1.pref_violations;
                                }
                                RouteSimResult sim2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, sim2.office_arrival);
                                int v2_after_v = vc2.hard_time_violations + vc2.pref_violations;
                                int total_after_v = v1_after_v + v2_after_v;
                                
                                // Accept if: reduces violations, OR same violations and saves cost
                                bool accept_move = false;
                                if (total_after_v < total_before_v) {
                                    accept_move = true; // fewer violations — always good
                                } else if (total_after_v == total_before_v && net < -1e-6) {
                                    accept_move = true; // same violations, lower cost
                                } else if (total_after_v == 0 && net < -1e-6) {
                                    accept_move = true; // no violations, saves cost
                                }
                                
                                if (accept_move) {
                                    improved = true;
                                } else {
                                    // Undo
                                    routes[v2].erase(routes[v2].begin() + pos);
                                    routes[v1].insert(routes[v1].begin() + i, emp);
                                }
                            }
                        }
                    }
                }
            }
            if (improved) continue;
            
            // Inter-route swap: swap employee from v1 with employee from v2
            // Fixed: properly compare before/after violations
            for (size_t v1 = 0; v1 < routes.size() && !improved; v1++) {
                if (routes[v1].empty()) continue;
                // Compute v1 violations BEFORE swap
                RouteSimResult sim1_b = simulate_route(routes[v1], vehs[v1], emps);
                ViolationCount vc1_b = count_route_violations(routes[v1], vehs[v1], emps, sim1_b.office_arrival);
                int v1_before = vc1_b.hard_time_violations + vc1_b.pref_violations;
                
                for (int i = 0; i < (int)routes[v1].size() && !improved; i++) {
                    for (size_t v2 = v1 + 1; v2 < routes.size() && !improved; v2++) {
                        if (routes[v2].empty()) continue;
                        // Compute v2 violations BEFORE swap
                        RouteSimResult sim2_b = simulate_route(routes[v2], vehs[v2], emps);
                        ViolationCount vc2_b = count_route_violations(routes[v2], vehs[v2], emps, sim2_b.office_arrival);
                        int v2_before = vc2_b.hard_time_violations + vc2_b.pref_violations;
                        int before_v = v1_before + v2_before;
                        
                        for (int j = 0; j < (int)routes[v2].size() && !improved; j++) {
                            int e1 = routes[v1][i], e2 = routes[v2][j];
                            int n1 = emps[e1].node_idx, n2 = emps[e2].node_idx;
                            
                            int p1 = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                            int x1 = (i == (int)routes[v1].size()-1) ? OFFICE_NODE : emps[routes[v1][i+1]].node_idx;
                            double d1 = (-dist_matrix[p1][n1] - dist_matrix[n1][x1]
                                         + dist_matrix[p1][n2] + dist_matrix[n2][x1]) * vehs[v1].cost_per_km;
                            
                            int p2 = (j == 0) ? vehs[v2].start_node : emps[routes[v2][j-1]].node_idx;
                            int x2 = (j == (int)routes[v2].size()-1) ? OFFICE_NODE : emps[routes[v2][j+1]].node_idx;
                            double d2 = (-dist_matrix[p2][n2] - dist_matrix[n2][x2]
                                         + dist_matrix[p2][n1] + dist_matrix[n1][x2]) * vehs[v2].cost_per_km;
                            
                            double net = d1 + d2;
                            // Try swap if cost improves OR violations exist
                            if (net < -1e-6 || before_v > 0) {
                                std::swap(routes[v1][i], routes[v2][j]);
                                
                                RouteSimResult sim1 = simulate_route(routes[v1], vehs[v1], emps);
                                ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, sim1.office_arrival);
                                RouteSimResult sim2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, sim2.office_arrival);
                                int after_v = vc1.hard_time_violations + vc1.pref_violations
                                            + vc2.hard_time_violations + vc2.pref_violations;
                                
                                // Accept if: fewer violations, OR same violations + lower cost
                                bool accept = false;
                                if (after_v < before_v) accept = true;
                                else if (after_v == before_v && net < -1e-6) accept = true;
                                
                                if (accept) {
                                    improved = true;
                                    // Update cached v1 violations for next iteration
                                    v1_before = vc1.hard_time_violations + vc1.pref_violations;
                                } else {
                                    std::swap(routes[v1][i], routes[v2][j]); // undo
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ======================== MAIN ALNS LOOP ========================
    
    void optimize(std::vector<std::vector<int>>& routes,
                 std::vector<std::vector<int>>& best_routes,
                 double& best_cost,
                 const std::vector<Vehicle>& vehs,
                 const std::vector<Employee>& emps,
                 const Metadata& meta,
                 bool enforce_soft,
                 int time_limit = 30) {
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "ADAPTIVE LARGE NEIGHBORHOOD SEARCH (ALNS)\n";
        std::cout << std::string(60, '=') << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // CRITICAL: Track total employee count
        total_employees = (int)emps.size();
        user_enforce_soft = enforce_soft;
        
        // STRATEGY: Always use enforce_soft=false for ALNS validators.
        // Soft-violating moves are ALLOWED but heavily penalized in cost function.
        // This lets ALNS explore the full search space and find solutions with
        // fewer total soft violations, rather than being blocked by rigid validation.
        bool alns_enforce_soft = false;
        
        // Verify initial solution has ALL employees
        int initial_assigned = count_assigned(routes);
        std::cout << "Employees: " << total_employees << " total, "
                  << initial_assigned << " initially assigned" << std::endl;
        
        if (initial_assigned < total_employees) {
            std::cout << "WARNING: Initial solution missing " 
                      << (total_employees - initial_assigned) << " employees! Fixing..." << std::endl;
            
            std::unordered_set<int> assigned_set;
            for (const auto& route : routes)
                for (int e : route) assigned_set.insert(e);
            
            std::vector<int> missing;
            for (int e = 0; e < total_employees; e++)
                if (assigned_set.find(e) == assigned_set.end())
                    missing.push_back(e);
            
            force_insert_all(routes, missing, vehs, emps, meta, alns_enforce_soft);
            std::cout << "Fixed: Now " << count_assigned(routes) << "/" << total_employees << " assigned" << std::endl;
        }
        
        best_routes = routes;
        CostResult init_cr = calculate_cost_and_violations(routes, vehs, emps, meta);
        best_cost = init_cr.cost;
        int best_hard_v = init_cr.hard_violations;
        double current_cost = best_cost;
        int current_hard_v = best_hard_v;
        auto current_routes = routes;
        
        // Build initial route cache for delta evaluation (opt 7)
        build_route_cache(current_routes, vehs, emps);
        
        double temperature = start_temp;
        int iteration = 0;
        int accept_count = 0, improve_count = 0;
        int segment_iter = 0;
        
        // Adaptive reheating state (opt 5)
        int iters_since_best = 0;
        int reheat_count = 0;
        const int REHEAT_THRESHOLD = 500;   // reheat after this many iterations without improvement
        const int MAX_REHEATS = 5;          // limit total reheats
        const double REHEAT_FACTOR = 0.5;   // reheat to this fraction of start_temp
        
        std::cout << "Initial cost: $" << best_cost 
                  << " (" << count_assigned(routes) << "/" << total_employees << " assigned"
                  << ", hard_v=" << best_hard_v << ")" << std::endl;
        std::cout << "Temperature: " << temperature << ", Cooling: " << cooling_rate << "\n";
        std::cout << "OPTIMIZATION PRIORITY: fewer hard violations > lower cost\n\n";
        
        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= time_limit) {
                std::cout << "\n✓ Time limit reached (" << time_limit << "s)" << std::endl;
                break;
            }
            
            iteration++;
            segment_iter++;
            
            if (segment_iter >= 100) {
                update_weights();
                segment_iter = 0;
                
                std::cout << "\n--- Iteration " << iteration << " ---" << std::endl;
                std::cout << "Best: $" << best_cost << " | Current: $" << current_cost
                         << " | Temp: " << temperature
                         << " | Assigned: " << count_assigned(best_routes) << "/" << total_employees
                         << " | HardV: " << best_hard_v << std::endl;
                std::cout << "Accepts: " << accept_count << " | Improves: " << improve_count << std::endl;
                
                std::fill(destroy_attempts.begin(), destroy_attempts.end(), 0);
                std::fill(repair_attempts.begin(), repair_attempts.end(), 0);
                std::fill(destroy_successes.begin(), destroy_successes.end(), 0);
                std::fill(repair_successes.begin(), repair_successes.end(), 0);
            }
            
            // ===== DESTROY =====
            int destroy_op = select_operator(destroy_weights);
            destroy_attempts[destroy_op]++;
            
            auto temp_routes = current_routes;
            // Snapshot which routes are non-empty before destroy (for tracking modifications)
            std::vector<int> pre_route_sizes(temp_routes.size());
            for (size_t vi = 0; vi < temp_routes.size(); vi++)
                pre_route_sizes[vi] = (int)temp_routes[vi].size();
            
            int total_in_routes = count_assigned(temp_routes);
            
            std::uniform_real_distribution<> pct_dis(min_destroy_pct, max_destroy_pct);
            int num_remove = std::max(1, (int)(total_in_routes * pct_dis(rng)));
            
            std::vector<int> removed;
            switch (destroy_op) {
                case RANDOM_REMOVAL: removed = destroy_random(temp_routes, num_remove); break;
                case WORST_REMOVAL:  removed = destroy_worst(temp_routes, num_remove, vehs, emps); break;
                case SHAW_REMOVAL:   removed = destroy_shaw(temp_routes, num_remove, vehs, emps); break;
                case ROUTE_REMOVAL:  removed = destroy_route(temp_routes, num_remove); break;
                case VIOLATION_REMOVAL: removed = destroy_violation(temp_routes, num_remove, vehs, emps); break;
                case CONSOLIDATION_REMOVAL: removed = destroy_consolidation(temp_routes, num_remove, vehs); break;
                case CROSS_VEHICLE_REMOVAL: removed = destroy_cross_vehicle(temp_routes, num_remove, vehs, emps); break;
                case VEHICLE_ELIMINATION: removed = destroy_vehicle_elimination(temp_routes, num_remove, vehs); break;
            }
            
            if (removed.empty()) continue;
            
            // ===== REPAIR =====
            int repair_op = select_operator(repair_weights);
            repair_attempts[repair_op]++;
            
            switch (repair_op) {
                case GREEDY_INSERTION:  repair_greedy(temp_routes, removed, vehs, emps, meta, alns_enforce_soft); break;
                case REGRET_INSERTION:  repair_regret(temp_routes, removed, vehs, emps, meta, alns_enforce_soft); break;
                case NEAREST_INSERTION: repair_nearest(temp_routes, removed, vehs, emps, meta, alns_enforce_soft); break;
                case BATCHING_INSERTION: repair_batching(temp_routes, removed, vehs, emps, meta, alns_enforce_soft); break;
            }
            
            // SAFETY: If repair left anything in removed, force insert
            if (!removed.empty()) {
                force_insert_all(temp_routes, removed, vehs, emps, meta, alns_enforce_soft);
            }
            
            // DOUBLE-CHECK: Ensure all employees present
            int new_assigned = count_assigned(temp_routes);
            if (new_assigned < total_employees) {
                std::unordered_set<int> assigned_set;
                for (const auto& route : temp_routes)
                    for (int e : route) assigned_set.insert(e);
                std::vector<int> missing;
                for (int e = 0; e < total_employees; e++)
                    if (assigned_set.find(e) == assigned_set.end())
                        missing.push_back(e);
                if (!missing.empty())
                    force_insert_all(temp_routes, missing, vehs, emps, meta, alns_enforce_soft);
            }
            
            // ===== LOCAL SEARCH on modified routes =====
            apply_local_search(temp_routes, vehs, emps, meta);
            
            // ===== INTER-ROUTE MOVES =====
            apply_inter_route_moves(temp_routes, vehs, emps, meta);
            
            // ===== DELTA EVALUATION (opt 7) =====
            // Only re-simulate routes that changed (comparing sizes with pre-destroy snapshot)
            std::vector<int> modified_vehicles;
            for (size_t vi = 0; vi < temp_routes.size(); vi++) {
                if ((int)temp_routes[vi].size() != pre_route_sizes[vi]) {
                    modified_vehicles.push_back((int)vi);
                } else {
                    // Size same but contents might differ — check
                    bool same = true;
                    if (!temp_routes[vi].empty()) {
                        for (size_t ei = 0; ei < temp_routes[vi].size(); ei++) {
                            if (temp_routes[vi][ei] != current_routes[vi][ei]) {
                                same = false;
                                break;
                            }
                        }
                    }
                    if (!same) modified_vehicles.push_back((int)vi);
                }
            }
            
            // Save current cache, update only modified routes
            auto saved_cache = route_cache;
            CostResult cr = delta_evaluate(temp_routes, vehs, emps, meta, modified_vehicles);
            double new_cost = cr.cost;
            double delta = new_cost - current_cost;
            
            bool accept = false;
            int new_hard_v = cr.hard_violations;
            
            // Use shared comparison function
            bool is_new_best = is_solution_better(new_hard_v, 0, new_cost,
                                                   best_hard_v, 0, best_cost);
            
            if (is_new_best) {
                accept = true;
                best_cost = new_cost;
                best_hard_v = new_hard_v;
                best_routes = temp_routes;
                improve_count++;
                iters_since_best = 0; // reset reheat counter (opt 5)
                std::cout << "  ★ NEW BEST: $" << best_cost
                         << " (" << count_assigned(best_routes) << "/" << total_employees
                         << " assigned, hard_v=" << best_hard_v
                         << ", iter " << iteration << ")" << std::endl;
            } else if (new_hard_v > current_hard_v) {
                // NEVER accept a solution with MORE hard violations than current
                // This is the #1 priority: minimize violations first
                accept = false;
            } else if (new_hard_v < current_hard_v) {
                // ALWAYS accept a solution with FEWER hard violations
                accept = true;
                improve_count++;
            } else if (delta < 0) {
                // Same violation count, lower cost — accept
                accept = true;
                improve_count++;
            } else {
                // Same violation count, higher cost — SA probability
                double prob = std::exp(-delta / temperature);
                std::uniform_real_distribution<> accept_dis(0.0, 1.0);
                if (accept_dis(rng) < prob) {
                    accept = true;
                }
            }
            
            if (accept) {
                current_routes = temp_routes;
                current_cost = new_cost;
                current_hard_v = new_hard_v;
                // route_cache already updated by delta_evaluate
                accept_count++;
                destroy_successes[destroy_op]++;
                repair_successes[repair_op]++;
            } else {
                // Restore cache to pre-evaluation state
                route_cache = saved_cache;
            }
            
            temperature *= cooling_rate;
            iters_since_best++;
            
            // ===== ADAPTIVE REHEATING (opt 5) =====
            if (iters_since_best >= REHEAT_THRESHOLD && reheat_count < MAX_REHEATS) {
                double new_temp = start_temp * REHEAT_FACTOR;
                std::cout << "  🔥 REHEAT #" << (reheat_count + 1) 
                         << ": temp " << temperature << " → " << new_temp
                         << " (stagnated " << iters_since_best << " iters)" << std::endl;
                temperature = new_temp;
                iters_since_best = 0;
                reheat_count++;
                // Return to best known solution to diversify from there
                current_routes = best_routes;
                current_cost = best_cost;
                current_hard_v = best_hard_v;
                build_route_cache(current_routes, vehs, emps);
            }
        }
        
        // FINAL SAFETY: Ensure best solution has ALL employees
        int final_assigned = count_assigned(best_routes);
        if (final_assigned < total_employees) {
            std::cerr << "FINAL FIX: Best solution missing "
                      << (total_employees - final_assigned) << " employees" << std::endl;
            std::unordered_set<int> assigned_set;
            for (const auto& route : best_routes)
                for (int e : route) assigned_set.insert(e);
            std::vector<int> missing;
            for (int e = 0; e < total_employees; e++)
                if (assigned_set.find(e) == assigned_set.end())
                    missing.push_back(e);
            force_insert_all(best_routes, missing, vehs, emps, meta, alns_enforce_soft);
            best_cost = calculate_cost(best_routes, vehs, emps, meta);
        }
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "ALNS COMPLETE" << std::endl;
        std::cout << "Total iterations: " << iteration << std::endl;
        std::cout << "Final best score (weighted): $" << best_cost << std::endl;
        std::cout << "Hard violations: " << best_hard_v << std::endl;
        std::cout << "Employees assigned: " << count_assigned(best_routes) << "/" << total_employees << std::endl;
        std::cout << "Total accepts: " << accept_count << " ("
                 << (iteration > 0 ? (100.0 * accept_count / iteration) : 0) << "%)" << std::endl;
        std::cout << "Total improves: " << improve_count << " ("
                 << (iteration > 0 ? (100.0 * improve_count / iteration) : 0) << "%)" << std::endl;
        std::cout << "Reheats: " << reheat_count << "/" << MAX_REHEATS << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // ======================== POST-ALNS EXHAUSTIVE OPTIMIZATION ========================
        // Best-improvement inter-route local search on final best solution.
        // Tries ALL possible swaps and relocations, picks single best move per pass.
        post_alns_optimize(best_routes, vehs, emps, meta);
        best_cost = calculate_cost(best_routes, vehs, emps, meta);
        
        routes = best_routes;
    }
    
    // ======================== POST-ALNS EXHAUSTIVE OPTIMIZATION ========================
    void post_alns_optimize(std::vector<std::vector<int>>& routes,
                            const std::vector<Vehicle>& vehs,
                            const std::vector<Employee>& emps,
                            const Metadata& meta) {
        
        std::cout << "\n--- Post-ALNS Exhaustive Optimization ---\n";
        
        bool improved = true;
        int pass = 0;
        
        while (improved && pass < 100) {
            improved = false;
            pass++;
            
            double best_improvement = 0; // positive = saving
            int best_type = -1; // 0=relocate, 1=swap
            size_t best_v1 = 0, best_v2 = 0;
            int best_i = 0, best_j = 0;
            int best_viol_delta = 0;
            
            // Pre-compute violations for all non-empty routes
            std::vector<int> viols(routes.size(), 0);
            for (size_t v = 0; v < routes.size(); v++) {
                if (routes[v].empty()) continue;
                RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                viols[v] = vc.hard_time_violations + vc.pref_violations;
            }
            
            // === BEST-IMPROVEMENT RELOCATIONS ===
            for (size_t v1 = 0; v1 < routes.size(); v1++) {
                if (routes[v1].empty()) continue;
                for (int i = 0; i < (int)routes[v1].size(); i++) {
                    int emp = routes[v1][i];
                    int emp_node = emps[emp].node_idx;
                    
                    int prev1 = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                    int next1 = (i == (int)routes[v1].size()-1) ? OFFICE_NODE : emps[routes[v1][i+1]].node_idx;
                    double remove_saving = (dist_matrix[prev1][emp_node] + dist_matrix[emp_node][next1]
                                            - dist_matrix[prev1][next1]) * vehs[v1].cost_per_km;
                    
                    for (size_t v2 = 0; v2 < routes.size(); v2++) {
                        if (v1 == v2) continue;
                        if ((int)routes[v2].size() >= vehs[v2].capacity) continue;
                        
                        int total_before = viols[v1] + viols[v2];
                        
                        for (size_t pos = 0; pos <= routes[v2].size(); pos++) {
                            int prev2 = (pos == 0) ? vehs[v2].start_node : emps[routes[v2][pos-1]].node_idx;
                            int next2 = (pos == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][pos]].node_idx;
                            double insert_cost = (dist_matrix[prev2][emp_node] + dist_matrix[emp_node][next2]
                                                  - dist_matrix[prev2][next2]) * vehs[v2].cost_per_km;
                            double net = insert_cost - remove_saving;
                            
                            // Try move
                            routes[v1].erase(routes[v1].begin() + i);
                            routes[v2].insert(routes[v2].begin() + pos, emp);
                            
                            int v1_after = 0, v2_after = 0;
                            if (!routes[v1].empty()) {
                                RouteSimResult s1 = simulate_route(routes[v1], vehs[v1], emps);
                                ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, s1.office_arrival);
                                v1_after = vc1.hard_time_violations + vc1.pref_violations;
                            }
                            {
                                RouteSimResult s2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, s2.office_arrival);
                                v2_after = vc2.hard_time_violations + vc2.pref_violations;
                            }
                            int total_after = v1_after + v2_after;
                            int viol_delta = total_after - total_before;
                            
                            double improvement = -net + (-viol_delta) * 100000.0;
                            
                            if (viol_delta < 0 || (viol_delta == 0 && net < -1e-6)) {
                                if (improvement > best_improvement) {
                                    best_improvement = improvement;
                                    best_type = 0;
                                    best_v1 = v1; best_i = i;
                                    best_v2 = v2; best_j = (int)pos;
                                    best_viol_delta = viol_delta;
                                }
                            }
                            
                            // Undo
                            routes[v2].erase(routes[v2].begin() + pos);
                            routes[v1].insert(routes[v1].begin() + i, emp);
                        }
                    }
                }
            }
            
            // === BEST-IMPROVEMENT SWAPS ===
            for (size_t v1 = 0; v1 < routes.size(); v1++) {
                if (routes[v1].empty()) continue;
                for (int i = 0; i < (int)routes[v1].size(); i++) {
                    for (size_t v2 = v1 + 1; v2 < routes.size(); v2++) {
                        if (routes[v2].empty()) continue;
                        
                        int total_before = viols[v1] + viols[v2];
                        
                        for (int j = 0; j < (int)routes[v2].size(); j++) {
                            int e1 = routes[v1][i], e2 = routes[v2][j];
                            int n1 = emps[e1].node_idx, n2 = emps[e2].node_idx;
                            
                            int p1 = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                            int x1 = (i == (int)routes[v1].size()-1) ? OFFICE_NODE : emps[routes[v1][i+1]].node_idx;
                            double d1 = (-dist_matrix[p1][n1] - dist_matrix[n1][x1]
                                         + dist_matrix[p1][n2] + dist_matrix[n2][x1]) * vehs[v1].cost_per_km;
                            
                            int p2 = (j == 0) ? vehs[v2].start_node : emps[routes[v2][j-1]].node_idx;
                            int x2 = (j == (int)routes[v2].size()-1) ? OFFICE_NODE : emps[routes[v2][j+1]].node_idx;
                            double d2 = (-dist_matrix[p2][n2] - dist_matrix[n2][x2]
                                         + dist_matrix[p2][n1] + dist_matrix[n1][x2]) * vehs[v2].cost_per_km;
                            
                            double net = d1 + d2;
                            
                            // Try swap
                            std::swap(routes[v1][i], routes[v2][j]);
                            
                            RouteSimResult s1 = simulate_route(routes[v1], vehs[v1], emps);
                            ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, s1.office_arrival);
                            RouteSimResult s2 = simulate_route(routes[v2], vehs[v2], emps);
                            ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, s2.office_arrival);
                            int total_after = vc1.hard_time_violations + vc1.pref_violations
                                            + vc2.hard_time_violations + vc2.pref_violations;
                            int viol_delta = total_after - total_before;
                            
                            double improvement = -net + (-viol_delta) * 100000.0;
                            
                            if (viol_delta < 0 || (viol_delta == 0 && net < -1e-6)) {
                                if (improvement > best_improvement) {
                                    best_improvement = improvement;
                                    best_type = 1;
                                    best_v1 = v1; best_i = i;
                                    best_v2 = v2; best_j = j;
                                    best_viol_delta = viol_delta;
                                }
                            }
                            
                            // Undo
                            std::swap(routes[v1][i], routes[v2][j]);
                        }
                    }
                }
            }
            
            // Apply best move found
            if (best_type == 0) {
                int emp = routes[best_v1][best_i];
                routes[best_v1].erase(routes[best_v1].begin() + best_i);
                routes[best_v2].insert(routes[best_v2].begin() + best_j, emp);
                improved = true;
                std::cout << "  Relocate emp " << emp << ": route " << best_v1
                         << " -> " << best_v2 << " (save=$" << best_improvement
                         << ", viols:" << best_viol_delta << ")\n";
            } else if (best_type == 1) {
                std::cout << "  Swap route " << best_v1 << "[" << best_i << "] <-> route "
                         << best_v2 << "[" << best_j << "] (save=$" << best_improvement
                         << ", viols:" << best_viol_delta << ")\n";
                std::swap(routes[best_v1][best_i], routes[best_v2][best_j]);
                improved = true;
            }
        }
        std::cout << "Post-optimization: " << pass << " passes\n";
    }
};

#endif
