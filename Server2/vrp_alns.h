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
        NUM_DESTROY_OPS = 7
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
        // Give consolidation and cross-vehicle higher initial weight
        destroy_weights[CONSOLIDATION_REMOVAL] = 2.0;
        destroy_weights[CROSS_VEHICLE_REMOVAL] = 1.5;
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
    
    AllViolations count_all_violations(const std::vector<std::vector<int>>& routes,
                                       const std::vector<Vehicle>& vehs,
                                       const std::vector<Employee>& emps) const {
        AllViolations av = {0, 0, 0};
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
            ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
            av.hard_time += vc.hard_time_violations;
            av.pref += vc.pref_violations;
            av.total_lateness += vc.total_lateness;
        }
        return av;
    }
    
    // Cost function with PRIORITY ORDER:
    //   0. Unassigned employees (UNASSIGNED_PENALTY each)
    //   1. Hard violations (HARD_VIOLATION_PENALTY each + lateness penalty)
    //   2. Soft violations (SOFT_VIOLATION_PENALTY each)
    //   3. Actual score = cost_weight * distance_cost + time_weight * total_time
    static constexpr double HARD_VIOLATION_PENALTY = 50000.0;
    static constexpr double LATENESS_PENALTY_PER_MIN = 500.0;
    
    double calculate_cost(const std::vector<std::vector<int>>& routes,
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
        
        // Score = cost_weight * cost + time_weight * time
        double score = meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
        
        // PRIORITY 0: Missing employees get highest penalty
        int unassigned_count = total_employees - assigned;
        score += unassigned_count * UNASSIGNED_PENALTY;
        
        // PRIORITY 1: Hard violations (time + preference) get very heavy penalty
        score += av.total_hard() * HARD_VIOLATION_PENALTY;
        score += av.total_lateness * LATENESS_PENALTY_PER_MIN;
        
        return score;
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
                        std::vector<int> temp = routes[v];
                        temp.insert(temp.begin() + pos, emp);
                        int h = 0, s = 0;
                        if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
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
                        std::vector<int> temp = routes[v];
                        temp.insert(temp.begin() + pos, emp);
                        int h = 0, s = 0;
                        if (validate_full_route(temp, vehs[v], emps, h, s, false, meta)) {
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
                        std::vector<int> temp = routes[v];
                        temp.insert(temp.begin() + pos, emp);
                        int h = 0, s = 0;
                        // Allow hard violations — just measure how bad
                        validate_full_route(temp, vehs[v], emps, h, s, false, meta, true);
                        
                        // Use shared simulation + violation counting
                        RouteSimResult sim = simulate_route(temp, vehs[v], emps);
                        ViolationCount vc = count_route_violations(temp, vehs[v], emps, sim.office_arrival);
                        
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
                        std::vector<int> temp = routes[v];
                        temp.insert(temp.begin() + pos, emp);
                        int h = 0, s = 0;
                        // ALLOW hard violations — just measure them
                        validate_full_route(temp, vehs[v], emps, h, s, false, meta, true);
                        
                        // Use shared simulation + violation counting
                        RouteSimResult sim = simulate_route(temp, vehs[v], emps);
                        ViolationCount vc = count_route_violations(temp, vehs[v], emps, sim.office_arrival);
                        
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
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        int curr = emps[emp].node_idx;
                        int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                        double delta_dist = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                        // Use weighted score: cost_weight * $ + time_weight * time
                        double delta_dollars = delta_dist * vehs[v].cost_per_km;
                        double delta_time = (delta_dist / vehs[v].speed_kmph) * 60.0;
                        double cost = meta.cost_weight * delta_dollars + meta.time_weight * delta_time;
                        
                        if (cost < best_cost) {
                            std::vector<int> temp = routes[v];
                            temp.insert(temp.begin() + pos, emp);
                            int h = 0, s = 0;
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
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
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        int curr = emps[emp].node_idx;
                        int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                        double cost = (dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next]) * vehs[v].cost_per_km;
                        
                        std::vector<int> temp = routes[v];
                        temp.insert(temp.begin() + pos, emp);
                        int h = 0, s = 0;
                        if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
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
                    for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                        int prev_node = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        double d = dist_matrix[prev_node][emp_node];
                        
                        if (d < best_dist) {
                            std::vector<int> temp = routes[v];
                            temp.insert(temp.begin() + pos, emp);
                            int h = 0, s = 0;
                            if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
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
                    std::vector<int> temp = routes[v];
                    temp.insert(temp.begin() + pos, emp);
                    int h = 0, s = 0;
                    if (validate_full_route(temp, vehs[v], emps, h, s, enforce_soft, meta)) {
                        int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                        int curr = emps[emp].node_idx;
                        int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                        double delta = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                        double cost = meta.cost_weight * delta * vehs[v].cost_per_km +
                                      meta.time_weight * (delta / vehs[v].speed_kmph) * 60.0;
                        if (cost < best_cost) {
                            best_cost = cost;
                            best_pos = (int)pos;
                        }
                    }
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
        best_cost = calculate_cost(routes, vehs, emps, meta);
        AllViolations init_av = count_all_violations(routes, vehs, emps);
        int best_hard_v = init_av.total_hard();
        double current_cost = best_cost;
        auto current_routes = routes;
        
        double temperature = start_temp;
        int iteration = 0;
        int accept_count = 0, improve_count = 0;
        int segment_iter = 0;
        
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
            
            // ===== EVALUATE =====
            double new_cost = calculate_cost(temp_routes, vehs, emps, meta);
            double delta = new_cost - current_cost;
            
            bool accept = false;
            
            AllViolations new_av = count_all_violations(temp_routes, vehs, emps);
            int new_hard_v = new_av.total_hard();
            
            // Use shared comparison function
            bool is_new_best = is_solution_better(new_hard_v, 0, new_cost,
                                                   best_hard_v, 0, best_cost);
            
            if (is_new_best) {
                accept = true;
                best_cost = new_cost;
                best_hard_v = new_hard_v;
                best_routes = temp_routes;
                improve_count++;
                std::cout << "  ★ NEW BEST: $" << best_cost
                         << " (" << count_assigned(best_routes) << "/" << total_employees
                         << " assigned, hard_v=" << best_hard_v
                         << ", iter " << iteration << ")" << std::endl;
            } else if (delta < 0) {
                accept = true;
                improve_count++;
            } else {
                double prob = std::exp(-delta / temperature);
                std::uniform_real_distribution<> accept_dis(0.0, 1.0);
                if (accept_dis(rng) < prob) {
                    accept = true;
                }
            }
            
            if (accept) {
                current_routes = temp_routes;
                current_cost = new_cost;
                accept_count++;
                destroy_successes[destroy_op]++;
                repair_successes[repair_op]++;
            }
            
            temperature *= cooling_rate;
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
        std::cout << std::string(60, '=') << std::endl;
        
        routes = best_routes;
    }
};

#endif
