#ifndef VRP_ALNS_H
#define VRP_ALNS_H

#include "vrp_types.h"
#include "vrp_validators.h"
#include "vrp_constraints.h"
#include "vrp_local_search.h"
#include "vrp_config.h"
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>
#include <unordered_set>
#include <set>
#include <map>
#include <functional>

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
        EXPENSIVE_ARC_REMOVAL = 8,    // NEW: targets most expensive edges
        STRING_REMOVAL = 9,           // NEW: removes geographically close emps from different vehicles
        LATENESS_TARGETED_REMOVAL = 10, // NEW: targets routes with highest lateness
        NUM_DESTROY_OPS = 11
    };
    
    enum RepairOperator {
        GREEDY_INSERTION = 0,
        REGRET_INSERTION = 1,
        NEAREST_INSERTION = 2,
        BATCHING_INSERTION = 3,
        CHEAPEST_VEHICLE_INSERTION = 4,  // NEW: fill cheap vehicles first
        NUM_REPAIR_OPS = 5
    };
    
    std::vector<double> destroy_weights;
    std::vector<double> repair_weights;
    std::vector<int> destroy_attempts;
    std::vector<int> repair_attempts;
    std::vector<int> destroy_successes;
    std::vector<int> repair_successes;
    
    double start_temp = g_config.start_temperature;
    double cooling_rate = g_config.cooling_rate;
    double min_destroy_pct = g_config.min_destroy_pct;
    double max_destroy_pct = g_config.max_destroy_pct;
    
    double sigma1 = 33.0;
    double sigma2 = 9.0;
    double sigma3 = 3.0;
    double decay_factor = 0.8;
    
    
    int total_employees = 0;
    bool user_enforce_soft = true;  // The user's setting (preserved for output)
    
    // GLS-like penalty mechanism (inspired by OR-Tools Guided Local Search)
    // Penalizes frequently-used costly features to escape local optima
    std::map<std::pair<int,int>, double> edge_penalties;  // (from_node, to_node) -> penalty
    double gls_lambda = 0.1;  // GLS penalty weight (auto-calibrated)
    int gls_update_interval = 200;  // How often to update penalties
    
    std::mt19937 rng;
    const ConstraintEngine* cp_ptr = nullptr;
    const NeighborList* nlist_ptr = nullptr;  // Precomputed neighbor lists
    
    // Active vehicle indices (skip empties for speed)
    std::vector<int> active_vehicles;
    void rebuild_active_vehicles(const std::vector<std::vector<int>>& routes) {
        active_vehicles.clear();
        for (size_t v = 0; v < routes.size(); v++)
            if (!routes[v].empty()) active_vehicles.push_back((int)v);
    }
    
    // Solution pool for multi-start diversification
    struct PoolEntry {
        std::vector<std::vector<int>> routes;
        double cost;
        int hard_violations;
    };
    std::vector<PoolEntry> solution_pool;
    static constexpr int MAX_POOL_SIZE = 5;
    
    // Late Acceptance Hill Climbing (LAHC) history
    std::vector<double> lahc_history;
    static constexpr int LAHC_LENGTH = 500;
    
public:
    AdaptiveLargeNeighborhoodSearch() : rng(std::random_device{}()) {
        destroy_weights.resize(NUM_DESTROY_OPS, 1.0);
        // Give consolidation, cross-vehicle, and vehicle elimination higher initial weight
        destroy_weights[CONSOLIDATION_REMOVAL] = 2.0;
        destroy_weights[CROSS_VEHICLE_REMOVAL] = 1.5;
        destroy_weights[VEHICLE_ELIMINATION] = 2.5;
        destroy_weights[EXPENSIVE_ARC_REMOVAL] = 2.0;  // Directly targets cost
        destroy_weights[STRING_REMOVAL] = 1.5;          // Geographic consolidation
        destroy_weights[LATENESS_TARGETED_REMOVAL] = 3.0;  // HIGH: directly targets delay reduction
        destroy_weights[LATENESS_TARGETED_REMOVAL] = 3.0;   // Directly targets lateness reduction
        repair_weights.resize(NUM_REPAIR_OPS, 1.0);
        repair_weights[BATCHING_INSERTION] = 2.0;  // Boost batching
        repair_weights[CHEAPEST_VEHICLE_INSERTION] = 2.0; // Boost cheap vehicle preference
        destroy_attempts.resize(NUM_DESTROY_OPS, 0);
        repair_attempts.resize(NUM_REPAIR_OPS, 0);
        destroy_successes.resize(NUM_DESTROY_OPS, 0);
        repair_successes.resize(NUM_REPAIR_OPS, 0);
    }
    
    void set_constraint_engine(const ConstraintEngine* cp) {
        cp_ptr = cp;
    }
    
    void set_neighbor_list(const NeighborList* nl) {
        nlist_ptr = nl;
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
        double priority_weighted_lateness;
        int total_hard() const { return hard_time + pref; }
    };
    

    // ======================== CRITICAL INVARIANT CHECKER ========================
    // Verifies no employee is duplicated and none is missing.
    // Call after EVERY route modification to catch bugs immediately.
    bool verify_solution_integrity(const std::vector<std::vector<int>>& routes,
                                    const std::string& context = "") const {
        std::vector<int> emp_count(total_employees, 0);
        for (size_t v = 0; v < routes.size(); v++) {
            for (int e : routes[v]) {
                if (e < 0 || e >= total_employees) {
                    std::cerr << "INTEGRITY VIOLATION [" << context << "]: invalid employee index " << e << "\n";
                    return false;
                }
                emp_count[e]++;
            }
        }
        bool ok = true;
        for (int e = 0; e < total_employees; e++) {
            if (emp_count[e] == 0) {
                std::cerr << "INTEGRITY VIOLATION [" << context << "]: employee " << e << " MISSING\n";
                ok = false;
            } else if (emp_count[e] > 1) {
                std::cerr << "INTEGRITY VIOLATION [" << context << "]: employee " << e << " appears " << emp_count[e] << " times\n";
                ok = false;
            }
        }
        return ok;
    }
    
    // Fix a corrupted solution: remove duplicates, re-insert missing employees
    void repair_integrity(std::vector<std::vector<int>>& routes,
                           const std::vector<Vehicle>& vehs,
                           const std::vector<Employee>& emps,
                           const Metadata& meta,
                           bool enforce_soft) {
        std::vector<bool> seen(total_employees, false);
        
        // Pass 1: Remove duplicates (keep first occurrence)
        for (size_t v = 0; v < routes.size(); v++) {
            auto it = routes[v].begin();
            while (it != routes[v].end()) {
                int e = *it;
                if (e < 0 || e >= total_employees || seen[e]) {
                    it = routes[v].erase(it);  // Remove duplicate
                } else {
                    seen[e] = true;
                    ++it;
                }
            }
        }
        
        // Pass 2: Find and re-insert missing employees
        std::vector<int> missing;
        for (int e = 0; e < total_employees; e++) {
            if (!seen[e]) missing.push_back(e);
        }
        
        if (!missing.empty()) {
            std::cerr << "REPAIR: Re-inserting " << missing.size() << " missing employees\n";
            force_insert_all(routes, missing, vehs, emps, meta, enforce_soft);
        }
    }
    // Cost function with PRIORITY ORDER:
    //   0. Unassigned employees (g_config.unassigned_penalty each)
    //   1. Hard violations (g_config.time_violation_penalty each + lateness penalty)
    //   2. Soft violations (g_config.pref_violation_penalty each)
    //   3. Actual score = cost_weight * distance_cost + time_weight * total_time
    // PRIORITY ORDER: minimize violations > minimize total lateness > minimize cost
    // All penalty values come from g_config (loaded from solver_config.json)
    
    // Combined cost + violation calculation in a single pass (avoids double simulate_route)
    struct CostResult {
        double cost;
        int hard_violations;
        int total_lateness;       // total minutes late across all employees
    };
    
    // Per-route cost cache for delta evaluation (opt 7)
    struct RouteCostEntry {
        double dist_cost;     // distance * cost_per_km
        double time;          // office_arrival - available_from
        int hard_time;        // hard time violations
        int pref;             // preference violations
        int total_lateness;   // total lateness minutes
        double priority_weighted_lateness;  // priority-weighted lateness
    };
    std::vector<RouteCostEntry> route_cache;
    
    // CRITICAL: Calculate violations by simulating trip sequences per physical vehicle
    // This matches OutputFormatter's behavior, ensuring ALNS and output agree
    CostResult calculate_cost_and_violations(const std::vector<std::vector<int>>& routes,
                                              const std::vector<Vehicle>& vehs,
                                              const std::vector<Employee>& emps,
                                              const Metadata& meta) const {
        double total_dist_cost = 0.0;
        double total_time = 0.0;
        int assigned = 0;
        AllViolations av = {0, 0, 0, 0.0};
        int vehicles_used = 0;
        
        // Step 1: Group routes by physical vehicle ID
        std::map<int, std::vector<size_t>> phys_to_routes;
        for (size_t v = 0; v < routes.size(); v++) {
            if (!routes[v].empty()) {
                phys_to_routes[vehs[v].physical_id].push_back(v);
                assigned += (int)routes[v].size();
                vehicles_used++;
            }
        }
        
        // Step 2: For each physical vehicle, simulate trips in sequence
        for (auto& [phys_id, route_indices] : phys_to_routes) {
            if (route_indices.empty()) continue;
            
            // Get physical vehicle info from first route's vehicle
            const Vehicle& pv = vehs[route_indices[0]];
            
            // Sort routes by their trip index (v % trips_per_vehicle) to maintain trip ordering
            // This ensures trip1 goes before trip2, etc.
            std::vector<size_t> sorted_indices = route_indices;
            std::sort(sorted_indices.begin(), sorted_indices.end(), [](size_t a, size_t b) {
                return (a % TRIPS_PER_VEHICLE) < (b % TRIPS_PER_VEHICLE);
            });
            
            // Simulate trips in sequence
            int next_avail = pv.available_from;
            for (size_t ti = 0; ti < sorted_indices.size(); ti++) {
                size_t v = sorted_indices[ti];
                const auto& route = routes[v];
                
                // First trip starts from depot, subsequent from office
                int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
                int curr_time = next_avail;
                int curr_node = start_node;
                double route_dist = 0.0;
                
                // Simulate pickup sequence
                for (int e : route) {
                    double d = dist_matrix[curr_node][emps[e].node_idx];
                    route_dist += d;
                    int travel = (int)std::round((d / pv.speed_kmph) * 60.0);
                    int arrival = curr_time + travel;
                    curr_time = std::max(arrival, emps[e].earliest_pickup);
                    curr_node = emps[e].node_idx;
                }
                
                // Return to office
                double d_off = dist_matrix[curr_node][OFFICE_NODE];
                route_dist += d_off;
                int travel_off = (int)std::round((d_off / pv.speed_kmph) * 60.0);
                int office_arrival = curr_time + travel_off;
                
                // Accumulate costs
                total_dist_cost += route_dist * pv.cost_per_km;
                total_time += (office_arrival - next_avail);
                
                // Count violations for this trip
                int sz = (int)route.size();
                for (int e : route) {
                    if (office_arrival > emps[e].latest_arrival_deadline) {
                        av.hard_time++;
                        int lateness = office_arrival - emps[e].latest_arrival_deadline;
                        av.total_lateness += lateness;
                        double priority_weight = 1.0;
                        switch (emps[e].priority) {
                            case 1: priority_weight = 10.0; break;
                            case 2: priority_weight = 5.0; break;
                            case 3: priority_weight = 3.0; break;
                            case 4: priority_weight = 2.0; break;
                            default: priority_weight = 1.0; break;
                        }
                        av.priority_weighted_lateness += lateness * priority_weight;
                    }
                    if (emps[e].sharing_pref < sz) av.pref++;
                    if (emps[e].vehicle_pref == 1 && pv.category != 1) av.pref++;
                    if (emps[e].vehicle_pref == 2 && pv.category == 1) av.pref++;
                }
                
                // Next trip starts when this one returns
                next_avail = office_arrival;
            }
        }
        
        // PRIORITY ORDER: unassigned > hard violations > total lateness > dollar cost
        // Lateness minimization DOMINATES cost minimization when violations exist.
        double score = 0.0;
        
        int unassigned_count = total_employees - assigned;
        score += unassigned_count * g_config.unassigned_penalty;
        // Level 1 (critical): time window violations
        score += av.hard_time * g_config.time_violation_penalty;
        // Level 2 (high): minimize delay
        score += av.total_lateness * g_config.lateness_per_min_penalty;
        score += av.priority_weighted_lateness * g_config.priority_lateness_multiplier;
        // Level 3: pref violations
        score += av.pref * g_config.pref_violation_penalty;
        
        // Dollar cost is tertiary - only matters when lateness is equal
        score += meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
        
        // Vehicle activation cost (encourages consolidation like OR-Tools)
        score += vehicles_used * g_config.vehicle_activation_cost;   // Strong consolidation incentive
        
        // Detour ratio penalty: penalize routes that are much longer than direct distances
        // Encourages "along the way" routing
        if (nlist_ptr && !nlist_ptr->direct_to_office.empty()) {
            for (size_t v = 0; v < routes.size(); v++) {
                if (routes[v].empty()) continue;
                double sum_direct = 0;
                for (int e : routes[v]) {
                    if (e < (int)nlist_ptr->direct_to_office.size())
                        sum_direct += nlist_ptr->direct_to_office[e];
                }
                if (sum_direct > 0) {
                    RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                    double ratio = sim.total_distance / sum_direct;
                    if (ratio > g_config.detour_ratio_threshold) {
                        score += (ratio - g_config.detour_ratio_threshold) * g_config.detour_penalty_multiplier * vehs[v].cost_per_km;
                    }
                }
            }
        }
        
        return {score, av.total_hard(), av.total_lateness};
    }
    
    // ======================== DELTA-BASED COST EVALUATION (OPT 7) ========================
    // Build the per-route cost cache (full simulation of all routes)
    void build_route_cache(const std::vector<std::vector<int>>& routes,
                           const std::vector<Vehicle>& vehs,
                           const std::vector<Employee>& emps) {
        route_cache.resize(routes.size());
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) {
                route_cache[v] = {0, 0, 0, 0, 0, 0.0};
            } else {
                // Use multi-trip simulation for accurate results
                MultiTripResult mtr = simulate_route_multitrip(routes[v], vehs[v], emps);
                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, 0);
                route_cache[v] = {
                    mtr.total_distance * vehs[v].cost_per_km,
                    (double)(mtr.last_office_arrival - vehs[v].available_from),
                    vc.hard_time_violations,
                    vc.pref_violations,
                    vc.total_lateness,
                    vc.priority_weighted_lateness
                };
            }
        }
    }
    
    // Compute total cost from route cache - WITH PHYSICAL VEHICLE GROUPING
    // This matches calculate_cost_and_violations by properly sequencing trips
    CostResult cost_from_cache_grouped(const std::vector<std::vector<int>>& routes,
                                        const std::vector<Vehicle>& vehs,
                                        const std::vector<Employee>& emps,
                                        const Metadata& meta, int assigned) const {
        double total_dist_cost = 0, total_time = 0;
        AllViolations av = {0, 0, 0, 0.0};
        int vehicles_used = 0;
        
        // Group routes by physical vehicle ID
        std::map<int, std::vector<size_t>> phys_to_routes;
        for (size_t v = 0; v < routes.size(); v++) {
            if (!routes[v].empty()) {
                phys_to_routes[vehs[v].physical_id].push_back(v);
                vehicles_used++;
            }
        }
        
        // For each physical vehicle, simulate trips in sequence
        for (auto& [phys_id, route_indices] : phys_to_routes) {
            if (route_indices.empty()) continue;
            
            const Vehicle& pv = vehs[route_indices[0]];
            
            // Sort routes by earliest deadline (trips with tighter deadlines first)
            std::vector<size_t> sorted_indices = route_indices;
            std::sort(sorted_indices.begin(), sorted_indices.end(), [&](size_t a, size_t b) {
                int min_a = INT_MAX, min_b = INT_MAX;
                for (int e : routes[a]) min_a = std::min(min_a, emps[e].latest_arrival_deadline);
                for (int e : routes[b]) min_b = std::min(min_b, emps[e].latest_arrival_deadline);
                return min_a < min_b;
            });
            
            // Simulate trips in sequence
            int next_avail = pv.available_from;
            for (size_t ti = 0; ti < sorted_indices.size(); ti++) {
                size_t v = sorted_indices[ti];
                const auto& route = routes[v];
                
                int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
                int curr_time = next_avail;
                int curr_node = start_node;
                double route_dist = 0.0;
                
                for (int e : route) {
                    double d = dist_matrix[curr_node][emps[e].node_idx];
                    route_dist += d;
                    int travel = (int)std::round((d / pv.speed_kmph) * 60.0);
                    int arrival = curr_time + travel;
                    curr_time = std::max(arrival, emps[e].earliest_pickup);
                    curr_node = emps[e].node_idx;
                }
                
                double d_off = dist_matrix[curr_node][OFFICE_NODE];
                route_dist += d_off;
                int travel_off = (int)std::round((d_off / pv.speed_kmph) * 60.0);
                int office_arrival = curr_time + travel_off;
                
                total_dist_cost += route_dist * pv.cost_per_km;
                total_time += (office_arrival - next_avail);
                
                // Count violations
                int sz = (int)route.size();
                for (int e : route) {
                    if (office_arrival > emps[e].latest_arrival_deadline) {
                        av.hard_time++;
                        int lateness = office_arrival - emps[e].latest_arrival_deadline;
                        av.total_lateness += lateness;
                        double priority_weight = 1.0;
                        switch (emps[e].priority) {
                            case 1: priority_weight = 10.0; break;
                            case 2: priority_weight = 5.0; break;
                            case 3: priority_weight = 3.0; break;
                            case 4: priority_weight = 2.0; break;
                            default: priority_weight = 1.0; break;
                        }
                        av.priority_weighted_lateness += lateness * priority_weight;
                    }
                    if (emps[e].sharing_pref < sz) av.pref++;
                    if (emps[e].vehicle_pref == 1 && pv.category != 1) av.pref++;
                    if (emps[e].vehicle_pref == 2 && pv.category == 1) av.pref++;
                }
                
                next_avail = office_arrival;
            }
        }
        
        double score = 0.0;
        int unassigned_count = total_employees - assigned;
        score += unassigned_count * g_config.unassigned_penalty;
        score += av.hard_time * g_config.time_violation_penalty;
        score += av.total_lateness * g_config.lateness_per_min_penalty;
        score += av.priority_weighted_lateness * g_config.priority_lateness_multiplier;
        score += av.pref * g_config.pref_violation_penalty;
        score += meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
        score += vehicles_used * g_config.vehicle_activation_cost;
        return {score, av.total_hard(), av.total_lateness};
    }
    
    // Original cost_from_cache for compatibility (DEPRECATED - use cost_from_cache_grouped)
    CostResult cost_from_cache(const Metadata& meta, int assigned) const {
        double total_dist_cost = 0, total_time = 0;
        AllViolations av = {0, 0, 0, 0.0};
        int vehicles_used = 0;
        for (size_t v = 0; v < route_cache.size(); v++) {
            total_dist_cost += route_cache[v].dist_cost;
            total_time += route_cache[v].time;
            av.hard_time += route_cache[v].hard_time;
            av.pref += route_cache[v].pref;
            av.total_lateness += route_cache[v].total_lateness;
            av.priority_weighted_lateness += route_cache[v].priority_weighted_lateness;
            if (route_cache[v].dist_cost > 0) vehicles_used++;
        }
        double score = 0.0;
        int unassigned_count = total_employees - assigned;
        score += unassigned_count * g_config.unassigned_penalty;
        // Level 1 (critical): time window violations
        score += av.hard_time * g_config.time_violation_penalty;
        // Level 2 (high): minimize delay
        score += av.total_lateness * g_config.lateness_per_min_penalty;
        score += av.priority_weighted_lateness * g_config.priority_lateness_multiplier;
        // Level 3: pref violations
        score += av.pref * g_config.pref_violation_penalty;
        score += meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
        // Vehicle activation cost (must match calculate_cost_and_violations)
        score += vehicles_used * g_config.vehicle_activation_cost;
        return {score, av.total_hard(), av.total_lateness};
    }
    
    // Update cache for only the modified routes, then compute total cost
    // IMPORTANT: Uses grouped evaluation to properly sequence trips within same physical vehicle
    CostResult delta_evaluate(const std::vector<std::vector<int>>& routes,
                              const std::vector<Vehicle>& vehs,
                              const std::vector<Employee>& emps,
                              const Metadata& meta,
                              const std::vector<int>& /*modified_vehicles*/) {
        int assigned = count_assigned(routes);
        // Use full grouped evaluation for accuracy with physical vehicle sequencing
        return cost_from_cache_grouped(routes, vehs, emps, meta, assigned);
    }
    
    // ======================== CP-GUIDED FEASIBILITY CHECK (OPT 6) ========================
    // Quick check using ConstraintEngine data to prune obviously infeasible insertions.
    // Returns false if inserting `emp` into `route` on vehicle `v` is definitely infeasible.
    bool cp_quick_feasible(int emp, const std::vector<int>& route, int v,
                           const std::vector<Vehicle>& vehs,
                           const std::vector<Employee>& /*emps*/) const {
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
    
    // GLS: Calculate augmented cost with edge penalties (escape local optima)
    double calculate_gls_penalty(const std::vector<std::vector<int>>& routes,
                                  const std::vector<Vehicle>& vehs,
                                  const std::vector<Employee>& emps) const {
        double penalty = 0;
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            int prev = vehs[v].start_node;
            for (int e : routes[v]) {
                int curr = emps[e].node_idx;
                auto key = std::make_pair(prev, curr);
                auto it = edge_penalties.find(key);
                if (it != edge_penalties.end()) penalty += it->second;
                prev = curr;
            }
            auto key = std::make_pair(prev, OFFICE_NODE);
            auto it = edge_penalties.find(key);
            if (it != edge_penalties.end()) penalty += it->second;
        }
        return penalty * gls_lambda;
    }
    
    // GLS: Update penalties on edges in the current local optimum
    void update_gls_penalties(const std::vector<std::vector<int>>& routes,
                               const std::vector<Vehicle>& vehs,
                               const std::vector<Employee>& emps) {
        // Find the edge with maximum utility = cost / (1 + penalty)
        double max_utility = -1;
        std::pair<int,int> max_edge = {-1, -1};
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            int prev = vehs[v].start_node;
            for (int e : routes[v]) {
                int curr = emps[e].node_idx;
                double cost = dist_matrix[prev][curr] * vehs[v].cost_per_km;
                auto key = std::make_pair(prev, curr);
                double pen = 0;
                auto it = edge_penalties.find(key);
                if (it != edge_penalties.end()) pen = it->second;
                double utility = cost / (1.0 + pen);
                if (utility > max_utility) {
                    max_utility = utility;
                    max_edge = key;
                }
                prev = curr;
            }
            // Last edge to office
            double cost = dist_matrix[prev][OFFICE_NODE] * vehs[v].cost_per_km;
            auto key = std::make_pair(prev, OFFICE_NODE);
            double pen = 0;
            auto it = edge_penalties.find(key);
            if (it != edge_penalties.end()) pen = it->second;
            double utility = cost / (1.0 + pen);
            if (utility > max_utility) {
                max_utility = utility;
                max_edge = key;
            }
        }
        
        if (max_edge.first >= 0) {
            edge_penalties[max_edge] += 1.0;
        }
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
                        // Allow hard violations --- just measure how bad
                        validate_full_route(routes[v], vehs[v], emps, h, s, false, meta, true);
                        
                        // Use shared simulation + violation counting
                        RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                        ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                        routes[v].erase(routes[v].begin() + pos);
                        
                        // Cost = heavy penalty per violation + lateness + distance cost
                        double cost = h * g_config.time_violation_penalty;
                        cost += vc.pref_violations * g_config.pref_violation_penalty;  // FIXED: was using wrong penalty
                        cost += vc.total_lateness * g_config.lateness_per_min_penalty;
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
            
            // Level 4: Absolute last resort --- put in smallest route
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
                        // ALLOW hard violations --- just measure them
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
                        
                        // Priority: minimize hard violations > lateness > pref violations > cost
                        // Note: s and vc.pref_violations measure same thing, use only one
                        double score = h * g_config.time_violation_penalty
                                     + vc.total_lateness * g_config.lateness_per_min_penalty
                                     + vc.pref_violations * g_config.pref_violation_penalty + dist_cost;
                        
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
                break; // No positions with capacity --- force_insert_all handles
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
        // Use neighbor list for fast similarity lookup if available
        if (nlist_ptr && seed < (int)nlist_ptr->neighbors.size()) {
            for (const auto& [dist_sim, emp] : nlist_ptr->neighbors[seed]) {
                double time_sim = std::abs(emps[seed].earliest_pickup - emps[emp].earliest_pickup);
                // Also add cost impact similarity
                double cost_sim = std::abs(dist_matrix[emps[seed].node_idx][OFFICE_NODE] - 
                                           dist_matrix[emps[emp].node_idx][OFFICE_NODE]);
                similarities.push_back({dist_sim + time_sim * 0.01 + cost_sim * 0.005, emp});
            }
            // Add remaining employees not in neighbor list
            std::unordered_set<int> in_neighbors;
            for (const auto& [d, e] : nlist_ptr->neighbors[seed]) in_neighbors.insert(e);
            for (int emp : all_emps) {
                if (emp == seed || in_neighbors.count(emp)) continue;
                double dist_sim = dist_matrix[emps[seed].node_idx][emps[emp].node_idx];
                similarities.push_back({dist_sim, emp});
            }
        } else {
            for (int emp : all_emps) {
                if (emp == seed) continue;
                double dist_sim = dist_matrix[emps[seed].node_idx][emps[emp].node_idx];
                double time_sim = std::abs(emps[seed].earliest_pickup - emps[emp].earliest_pickup);
                double cost_sim = std::abs(dist_matrix[emps[seed].node_idx][OFFICE_NODE] - 
                                           dist_matrix[emps[emp].node_idx][OFFICE_NODE]);
                similarities.push_back({dist_sim + time_sim * 0.01 + cost_sim * 0.005, emp});
            }
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
            // No violations --- fall back to random removal
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
            // Estimate cost from cache
            if (v < route_cache.size()) {
                phys_info[pid].total_cost += route_cache[v].dist_cost;
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
    

    // LATENESS-TARGETED REMOVAL: Removes employees who are late, prioritizing
    // high-priority employees. Also removes some routemates to allow re-routing
    // the late employee to a faster vehicle/route.
    std::vector<int> destroy_lateness(std::vector<std::vector<int>>& routes,
                                       int num_remove,
                                       const std::vector<Vehicle>& vehs,
                                       const std::vector<Employee>& emps) {
        std::vector<int> removed;
        
        // Find all late employees with their lateness and priority
        struct LateInfo {
            double score;  // priority_weight * lateness - higher = more urgent to fix
            int emp_id;
            int veh_idx;
        };
        std::vector<LateInfo> late_emps;
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
            for (int e : routes[v]) {
                if (sim.office_arrival > emps[e].latest_arrival_deadline) {
                    int lateness = sim.office_arrival - emps[e].latest_arrival_deadline;
                    double pw = 1.0;
                    switch (emps[e].priority) {
                        case 1: pw = 10.0; break;
                        case 2: pw = 5.0; break;
                        case 3: pw = 3.0; break;
                        case 4: pw = 2.0; break;
                    }
                    late_emps.push_back({pw * lateness, e, (int)v});
                }
            }
        }
        
        if (late_emps.empty()) {
            return destroy_random(routes, num_remove);
        }
        
        // Sort by urgency (highest priority * lateness first)
        std::sort(late_emps.begin(), late_emps.end(),
                 [](const LateInfo& a, const LateInfo& b) { return a.score > b.score; });
        
        // Remove the late employees + their routemates
        std::unordered_set<int> to_remove_set;
        for (const auto& li : late_emps) {
            to_remove_set.insert(li.emp_id);
            // Also remove routemates (they contribute to the delay)
            for (int e : routes[li.veh_idx]) {
                to_remove_set.insert(e);
                if ((int)to_remove_set.size() >= num_remove) break;
            }
            if ((int)to_remove_set.size() >= num_remove) break;
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
    
    // EXPENSIVE ARC REMOVAL: Finds the most expensive edges in the solution
    // and removes employees at both ends. Directly targets cost reduction.
    std::vector<int> destroy_expensive_arc(std::vector<std::vector<int>>& routes,
                                            int num_remove,
                                            const std::vector<Vehicle>& vehs,
                                            const std::vector<Employee>& emps) {
        std::vector<int> removed;
        
        // Collect all edges with their dollar costs
        struct ArcInfo {
            double cost;       // dollar cost of this arc
            int emp_id;        // employee at the "to" end of arc
            int veh_idx;
        };
        std::vector<ArcInfo> arcs;
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            int prev = vehs[v].start_node;
            for (size_t i = 0; i < routes[v].size(); i++) {
                int emp = routes[v][i];
                int curr = emps[emp].node_idx;
                double arc_cost = dist_matrix[prev][curr] * vehs[v].cost_per_km;
                arcs.push_back({arc_cost, emp, (int)v});
                prev = curr;
            }
            // Last arc to office - attribute to last employee
            if (!routes[v].empty()) {
                int last_emp = routes[v].back();
                int last_node = emps[last_emp].node_idx;
                double arc_cost = dist_matrix[last_node][OFFICE_NODE] * vehs[v].cost_per_km;
                arcs.push_back({arc_cost, last_emp, (int)v});
            }
        }
        
        if (arcs.empty()) return removed;
        
        // Sort by cost (most expensive first)
        std::sort(arcs.begin(), arcs.end(), [](const ArcInfo& a, const ArcInfo& b) {
            return a.cost > b.cost;
        });
        
        // Remove employees involved in expensive arcs
        std::unordered_set<int> to_remove_set;
        for (const auto& arc : arcs) {
            to_remove_set.insert(arc.emp_id);
            if ((int)to_remove_set.size() >= num_remove) break;
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
    
    // STRING REMOVAL: Removes geographically close employees from DIFFERENT vehicles
    // so they can be consolidated onto fewer routes. Unlike Shaw removal which picks
    // similar employees, this specifically targets cross-vehicle clusters.
    std::vector<int> destroy_string(std::vector<std::vector<int>>& routes,
                                     int num_remove,
                                     const std::vector<Vehicle>& vehs,
                                     const std::vector<Employee>& emps) {
        std::vector<int> removed;
        
        // Pick a random seed employee
        std::vector<int> all_emps;
        for (const auto& route : routes)
            for (int e : route) all_emps.push_back(e);
        if (all_emps.empty()) return removed;
        
        std::uniform_int_distribution<> dis(0, (int)all_emps.size() - 1);
        int seed = all_emps[dis(rng)];
        int seed_node = emps[seed].node_idx;
        
        // Find which vehicle the seed is on
        int seed_veh = -1;
        for (size_t v = 0; v < routes.size(); v++)
            for (int e : routes[v])
                if (e == seed) { seed_veh = (int)v; break; }
        
        // Score all employees by distance to seed, BUT prefer those on DIFFERENT vehicles
        std::vector<std::pair<double, int>> scored;
        for (int emp : all_emps) {
            if (emp == seed) continue;
            double dist = dist_matrix[seed_node][emps[emp].node_idx];
            // Find which vehicle this employee is on
            int emp_veh = -1;
            for (size_t v = 0; v < routes.size(); v++)
                for (int e : routes[v])
                    if (e == emp) { emp_veh = (int)v; break; }
            // Heavily discount distance for employees on different vehicles
            // (we WANT to remove cross-vehicle clusters for consolidation)
            double score = dist;
            if (emp_veh != seed_veh) score *= 0.5; // prefer different-vehicle employees
            scored.push_back({score, emp});
        }
        std::sort(scored.begin(), scored.end());
        
        std::vector<int> to_remove_emps = {seed};
        for (int i = 0; i < num_remove - 1 && i < (int)scored.size(); i++) {
            to_remove_emps.push_back(scored[i].second);
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
    

    // LATENESS-TARGETED REMOVAL: Removes employees from the routes with highest lateness.
    // Directly targets the #1 priority: minimizing delay.
    std::vector<int> destroy_lateness_targeted(std::vector<std::vector<int>>& routes,
                                                int num_remove,
                                                const std::vector<Vehicle>& vehs,
                                                const std::vector<Employee>& emps) {
        std::vector<int> removed;
        std::unordered_set<int> removed_set;  // Track to prevent duplicates
        
        // Score each route by total lateness (highest first)
        std::vector<std::pair<int, int>> route_lateness; // (lateness, route_idx)
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
            ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
            if (vc.total_lateness > 0) {
                route_lateness.push_back({vc.total_lateness, (int)v});
            }
        }
        
        if (route_lateness.empty()) return destroy_random(routes, num_remove);
        
        // Sort by lateness descending
        std::sort(route_lateness.begin(), route_lateness.end(),
                 [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Remove employees from routes with highest lateness
        for (const auto& [late, v] : route_lateness) {
            if ((int)removed.size() >= num_remove) break;
            for (int e : routes[v]) {
                if (removed_set.find(e) == removed_set.end()) {
                    removed.push_back(e);
                    removed_set.insert(e);
                }
            }
            routes[v].clear();
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
                        // Use DOLLAR cost directly (like OR-Tools arc cost evaluator)
                        double delta_dollars = delta_dist * vehs[v].cost_per_km;
                        // Small time penalty to break ties
                        double delta_time = (delta_dist / vehs[v].speed_kmph) * 60.0 * 0.1;
                        double cost = delta_dollars + delta_time;
                        
                        // Time-window aware: add penalty for lateness this insertion would cause
                        {
                            int travel_min = (int)std::round((delta_dist / vehs[v].speed_kmph) * 60.0);
                            int est_arrival = vehs[v].available_from + travel_min;
                            if (!routes[v].empty()) {
                                // Rough estimate of added delay to office
                                est_arrival = vehs[v].available_from + 
                                    (int)std::round(((dist_matrix[vehs[v].start_node][emps[emp].node_idx] + 
                                     dist_matrix[emps[emp].node_idx][OFFICE_NODE]) / vehs[v].speed_kmph) * 60.0);
                            }
                            int lateness = std::max(0, est_arrival - emps[emp].latest_arrival_deadline);
                            cost += lateness * g_config.lateness_per_min_penalty;  // Configurable lateness penalty
                        }
                        
                        // Penalty for using a new vehicle trip (encourages consolidation)
                        if (routes[v].empty()) cost += 25.0;  // Stronger consolidation incentive
                        
                        if (cost < best_cost) {
                            // In-place insert, validate, then erase --- avoids temp vector copy
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
                break; // No feasible found --- try with hard violations allowed
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
                        // Use DOLLAR cost (like OR-Tools arc cost evaluator)
                        double cost = (dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next]) * vehs[v].cost_per_km;
                        // Add small penalty for opening new trip (consolidation preference)
                        if (routes[v].empty()) cost += 25.0;  // Stronger consolidation incentive
                        
                        // In-place insert, validate, erase --- avoids temp vector copy
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
                break; // No feasible found --- try with hard violations allowed
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
                            // In-place insert, validate, erase --- avoids temp vector copy
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
                break; // No feasible found --- try with hard violations allowed
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
    // Inspired by OR-Tools' preference for consolidation through cost optimization.
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
        // This matches OR-Tools' natural preference for cheaper vehicles
        std::vector<size_t> veh_order(routes.size());
        std::iota(veh_order.begin(), veh_order.end(), 0);
        std::sort(veh_order.begin(), veh_order.end(), [&](size_t a, size_t b) {
            // Prefer vehicles that already have employees (consolidation)
            // then by cost_per_km (cheaper first)
            bool a_has = !routes[a].empty();
            bool b_has = !routes[b].empty();
            if (a_has != b_has) return a_has > b_has;  // non-empty first
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
                        // Use direct dollar cost (like OR-Tools)
                        double cost = delta * vehs[v].cost_per_km;
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
                    // Don't increment attempt --- next employee now at same index
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
    
    // CHEAPEST VEHICLE FIRST INSERTION: Exclusively fills cheaper vehicles to capacity
    // before using expensive ones. Strong cost-reduction repair operator.
    void repair_cheapest_vehicle(std::vector<std::vector<int>>& routes,
                                  std::vector<int>& unassigned,
                                  const std::vector<Vehicle>& vehs,
                                  const std::vector<Employee>& emps,
                                  const Metadata& meta,
                                  bool enforce_soft) {
        if (unassigned.empty()) return;
        
        // Lambda to insert employees with given filter
        auto insert_to_vehicles = [&](const std::vector<size_t>& veh_order, 
                                       const std::function<bool(int)>& emp_filter) {
            for (size_t vi = 0; vi < veh_order.size() && !unassigned.empty(); vi++) {
                int v = (int)veh_order[vi];
                
                bool inserted_any = true;
                while (inserted_any && !unassigned.empty()) {
                    inserted_any = false;
                    if ((int)routes[v].size() >= vehs[v].capacity) break;
                    
                    double best_cost = 1e18;
                    int best_emp_idx = -1, best_pos = -1;
                    
                    for (size_t ue = 0; ue < unassigned.size(); ue++) {
                        int emp = unassigned[ue];
                        if (!emp_filter(emp)) continue;
                        if (!cp_quick_feasible(emp, routes[v], v, vehs, emps)) continue;
                        
                        for (size_t pos = 0; pos <= routes[v].size(); pos++) {
                            int prev = (pos == 0) ? vehs[v].start_node : emps[routes[v][pos-1]].node_idx;
                            int curr = emps[emp].node_idx;
                            int next = (pos == routes[v].size()) ? OFFICE_NODE : emps[routes[v][pos]].node_idx;
                            double delta_dist = dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next];
                            double cost = delta_dist * vehs[v].cost_per_km;
                            
                            if (cost < best_cost) {
                                routes[v].insert(routes[v].begin() + pos, emp);
                                int h = 0, s = 0;
                                bool valid = validate_full_route(routes[v], vehs[v], emps, h, s, enforce_soft, meta);
                                routes[v].erase(routes[v].begin() + pos);
                                if (valid) {
                                    best_cost = cost;
                                    best_emp_idx = (int)ue;
                                    best_pos = (int)pos;
                                }
                            }
                        }
                    }
                    
                    if (best_emp_idx >= 0) {
                        routes[v].insert(routes[v].begin() + best_pos, unassigned[best_emp_idx]);
                        unassigned.erase(unassigned.begin() + best_emp_idx);
                        inserted_any = true;
                    }
                }
            }
        };
        
        // PASS 1: Assign premium-preference employees to premium vehicles first
        std::vector<size_t> premium_vehs, normal_vehs;
        for (size_t v = 0; v < vehs.size(); v++) {
            if (vehs[v].category == 1) premium_vehs.push_back(v);
            else normal_vehs.push_back(v);
        }
        // Sort each group by cost_per_km ascending
        std::sort(premium_vehs.begin(), premium_vehs.end(), [&](size_t a, size_t b) {
            return vehs[a].cost_per_km < vehs[b].cost_per_km;
        });
        std::sort(normal_vehs.begin(), normal_vehs.end(), [&](size_t a, size_t b) {
            return vehs[a].cost_per_km < vehs[b].cost_per_km;
        });
        
        // Pass 1a: premium-preference employees -> premium vehicles
        insert_to_vehicles(premium_vehs, [&](int emp) {
            return emps[emp].vehicle_pref == 1;
        });
        
        // Pass 1b: normal-preference employees -> normal vehicles  
        insert_to_vehicles(normal_vehs, [&](int emp) {
            return emps[emp].vehicle_pref == 2;
        });
        
        // PASS 2: Assign remaining employees by cheapest vehicle
        std::vector<size_t> all_vehs(routes.size());
        std::iota(all_vehs.begin(), all_vehs.end(), 0);
        std::sort(all_vehs.begin(), all_vehs.end(), [&](size_t a, size_t b) {
            return vehs[a].cost_per_km < vehs[b].cost_per_km;
        });
        
        insert_to_vehicles(all_vehs, [&](int) { return true; });  // All remaining
        
        // Fall back to greedy for remaining
        if (!unassigned.empty()) {
            repair_greedy(routes, unassigned, vehs, emps, meta, enforce_soft);
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
                    
                    // Count current violations on v1 (use weighted score: hard >> lateness >> pref)
                    RouteSimResult sim_v1_before = simulate_route(routes[v1], vehs[v1], emps);
                    ViolationCount vc_v1_before = count_route_violations(routes[v1], vehs[v1], emps, sim_v1_before.office_arrival);
                    long long before_violations = weighted_violation_score(vc_v1_before);
                    
                    // Full cost of having this employee on v1 route
                    int prev1 = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                    int next1 = (i == (int)routes[v1].size()-1) ? OFFICE_NODE : emps[routes[v1][i+1]].node_idx;
                    double remove_saving = (dist_matrix[prev1][emp_node] + dist_matrix[emp_node][next1]
                                            - dist_matrix[prev1][next1]) * vehs[v1].cost_per_km;
                    
                    for (size_t v2 = 0; v2 < routes.size() && !improved; v2++) {
                        if (v1 == v2) continue;
                        if ((int)routes[v2].size() >= vehs[v2].capacity) continue;
                        
                        // Count current violations on v2 (use weighted score)
                        long long v2_before_v = 0;
                        if (!routes[v2].empty()) {
                            RouteSimResult sim_v2_b = simulate_route(routes[v2], vehs[v2], emps);
                            ViolationCount vc_v2_b = count_route_violations(routes[v2], vehs[v2], emps, sim_v2_b.office_arrival);
                            v2_before_v = weighted_violation_score(vc_v2_b);
                        }
                        long long total_before_v = before_violations + v2_before_v;
                        
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
                                
                                // Validate both routes (use weighted violation score)
                                long long v1_after_v = 0;
                                if (!routes[v1].empty()) {
                                    RouteSimResult sim1 = simulate_route(routes[v1], vehs[v1], emps);
                                    ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, sim1.office_arrival);
                                    v1_after_v = weighted_violation_score(vc1);
                                }
                                RouteSimResult sim2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, sim2.office_arrival);
                                long long v2_after_v = weighted_violation_score(vc2);
                                long long total_after_v = v1_after_v + v2_after_v;
                                
                                // Accept if: reduces weighted violations, OR same violations and saves cost
                                bool accept_move = false;
                                if (total_after_v < total_before_v) {
                                    accept_move = true; // better weighted score --- always good
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
            if (improved) {
                // Verify integrity after relocate
                if (!verify_solution_integrity(routes, "inter-route-relocate")) {
                    std::cerr << "BUG: inter-route relocate corrupted solution\n";
                }
                continue;
            }
            
            // Inter-route swap: swap employee from v1 with employee from v2
            // Fixed: properly compare before/after violations using weighted score
            for (size_t v1 = 0; v1 < routes.size() && !improved; v1++) {
                if (routes[v1].empty()) continue;
                // Compute v1 violations BEFORE swap
                RouteSimResult sim1_b = simulate_route(routes[v1], vehs[v1], emps);
                ViolationCount vc1_b = count_route_violations(routes[v1], vehs[v1], emps, sim1_b.office_arrival);
                long long v1_before = weighted_violation_score(vc1_b);
                
                for (int i = 0; i < (int)routes[v1].size() && !improved; i++) {
                    for (size_t v2 = v1 + 1; v2 < routes.size() && !improved; v2++) {
                        if (routes[v2].empty()) continue;
                        // Compute v2 violations BEFORE swap
                        RouteSimResult sim2_b = simulate_route(routes[v2], vehs[v2], emps);
                        ViolationCount vc2_b = count_route_violations(routes[v2], vehs[v2], emps, sim2_b.office_arrival);
                        long long v2_before = weighted_violation_score(vc2_b);
                        long long before_v = v1_before + v2_before;
                        
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
                                long long after_v = weighted_violation_score(vc1) + weighted_violation_score(vc2);
                                
                                // Accept if: lower weighted score, or same score + lower cost
                                bool accept = false;
                                if (after_v < before_v) {
                                    accept = true;
                                } else if (after_v == before_v && net < -1e-6) {
                                    accept = true;
                                }
                                
                                if (accept) {
                                    improved = true;
                                    // Update cached v1 violations for next iteration
                                    v1_before = weighted_violation_score(vc1);
                                } else {
                                    std::swap(routes[v1][i], routes[v2][j]); // undo
                                }
                            }
                        }
                    }
                }
            }
            if (improved) continue;
            
            // Or-opt 2: move a block of 2 consecutive employees from v1 to v2
            for (size_t v1 = 0; v1 < routes.size() && !improved; v1++) {
                if ((int)routes[v1].size() < 2) continue;
                
                RouteSimResult sim1_b = simulate_route(routes[v1], vehs[v1], emps);
                ViolationCount vc1_b = count_route_violations(routes[v1], vehs[v1], emps, sim1_b.office_arrival);
                long long v1_before_v = weighted_violation_score(vc1_b);
                
                for (int i = 0; i < (int)routes[v1].size() - 1 && !improved; i++) {
                    int e_a = routes[v1][i], e_b = routes[v1][i+1];
                    int n_a = emps[e_a].node_idx, n_b = emps[e_b].node_idx;
                    int prev = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                    int next = (i+2 < (int)routes[v1].size()) ? emps[routes[v1][i+2]].node_idx : OFFICE_NODE;
                    double remove_saving = (dist_matrix[prev][n_a] + dist_matrix[n_a][n_b] + dist_matrix[n_b][next]
                                            - dist_matrix[prev][next]) * vehs[v1].cost_per_km;
                    
                    for (size_t v2 = 0; v2 < routes.size() && !improved; v2++) {
                        if (v1 == v2) continue;
                        if ((int)routes[v2].size() + 2 > vehs[v2].capacity) continue;
                        
                        long long v2_before_v = 0;
                        if (!routes[v2].empty()) {
                            RouteSimResult sim2_b = simulate_route(routes[v2], vehs[v2], emps);
                            ViolationCount vc2_b = count_route_violations(routes[v2], vehs[v2], emps, sim2_b.office_arrival);
                            v2_before_v = weighted_violation_score(vc2_b);
                        }
                        long long total_bv = v1_before_v + v2_before_v;
                        
                        for (size_t pos = 0; pos <= routes[v2].size() && !improved; pos++) {
                            int prev2 = (pos == 0) ? vehs[v2].start_node : emps[routes[v2][pos-1]].node_idx;
                            int next2 = (pos == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][pos]].node_idx;
                            double insert_cost = (dist_matrix[prev2][n_a] + dist_matrix[n_a][n_b] + dist_matrix[n_b][next2]
                                                  - dist_matrix[prev2][next2]) * vehs[v2].cost_per_km;
                            
                            double net = insert_cost - remove_saving;
                            if (net < -1e-6 || total_bv > 0) {
                                // Try the block move
                                routes[v1].erase(routes[v1].begin() + i, routes[v1].begin() + i + 2);
                                routes[v2].insert(routes[v2].begin() + pos, e_b);
                                routes[v2].insert(routes[v2].begin() + pos, e_a);
                                
                                long long v1_aft_v = 0;
                                if (!routes[v1].empty()) {
                                    RouteSimResult s1 = simulate_route(routes[v1], vehs[v1], emps);
                                    ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, s1.office_arrival);
                                    v1_aft_v = weighted_violation_score(vc1);
                                }
                                RouteSimResult s2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, s2.office_arrival);
                                long long v2_aft_v = weighted_violation_score(vc2);
                                long long total_av = v1_aft_v + v2_aft_v;
                                
                                bool accept = false;
                                if (total_av < total_bv) accept = true;
                                else if (total_av == total_bv && net < -1e-6) accept = true;
                                
                                if (accept) {
                                    improved = true;
                                } else {
                                    routes[v2].erase(routes[v2].begin() + pos, routes[v2].begin() + pos + 2);
                                    routes[v1].insert(routes[v1].begin() + i, e_a);
                                    routes[v1].insert(routes[v1].begin() + i + 1, e_b);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ======================== CROSS-EXCHANGE INTER-ROUTE ========================
    // Swaps segments between two routes (e.g., 2 from A with 1 from B).
    // Can find improvements that single relocations/swaps cannot.
    void apply_cross_exchange(std::vector<std::vector<int>>& routes,
                               const std::vector<Vehicle>& vehs,
                               const std::vector<Employee>& emps,
                               const Metadata& meta) {
        bool improved = true;
        int passes = 0;
        while (improved && passes < 2) {
            improved = false;
            passes++;
            
            for (size_t v1 = 0; v1 < routes.size() && !improved; v1++) {
                if (routes[v1].size() < 2) continue;
                for (size_t v2 = v1 + 1; v2 < routes.size() && !improved; v2++) {
                    if (routes[v2].empty()) continue;
                    
                    // Pre-compute costs for both routes
                    RouteSimResult s1b = simulate_route(routes[v1], vehs[v1], emps);
                    RouteSimResult s2b = simulate_route(routes[v2], vehs[v2], emps);
                    double cost_before = s1b.total_distance * vehs[v1].cost_per_km
                                       + s2b.total_distance * vehs[v2].cost_per_km;
                    ViolationCount vc1b = count_route_violations(routes[v1], vehs[v1], emps, s1b.office_arrival);
                    ViolationCount vc2b = count_route_violations(routes[v2], vehs[v2], emps, s2b.office_arrival);
                    long long viols_before = weighted_violation_score(vc1b) + weighted_violation_score(vc2b);
                    
                    // Try swapping segment of size 2 from v1 with segment of size 1 from v2
                    for (int i1 = 0; i1 < (int)routes[v1].size() - 1 && !improved; i1++) {
                        for (int i2 = 0; i2 < (int)routes[v2].size() && !improved; i2++) {
                            // Check capacity: v1 loses 2, gains 1 = -1; v2 loses 1, gains 2 = +1
                            if ((int)routes[v2].size() + 1 > vehs[v2].capacity) continue;
                            
                            // Extract segments
                            int e1a = routes[v1][i1], e1b = routes[v1][i1+1];
                            int e2 = routes[v2][i2];
                            
                            // Perform swap: remove 2 from v1, insert e2 at i1; remove e2 from v2, insert 2
                            auto saved_v1 = routes[v1];
                            auto saved_v2 = routes[v2];
                            
                            routes[v1].erase(routes[v1].begin() + i1, routes[v1].begin() + i1 + 2);
                            routes[v1].insert(routes[v1].begin() + i1, e2);
                            
                            routes[v2].erase(routes[v2].begin() + i2);
                            int ins_pos = std::min(i2, (int)routes[v2].size());
                            routes[v2].insert(routes[v2].begin() + ins_pos, e1b);
                            routes[v2].insert(routes[v2].begin() + ins_pos, e1a);
                            
                            // Evaluate
                            RouteSimResult s1a = simulate_route(routes[v1], vehs[v1], emps);
                            RouteSimResult s2a = simulate_route(routes[v2], vehs[v2], emps);
                            double cost_after = s1a.total_distance * vehs[v1].cost_per_km
                                              + s2a.total_distance * vehs[v2].cost_per_km;
                            ViolationCount vc1a = count_route_violations(routes[v1], vehs[v1], emps, s1a.office_arrival);
                            ViolationCount vc2a = count_route_violations(routes[v2], vehs[v2], emps, s2a.office_arrival);
                            long long viols_after = weighted_violation_score(vc1a) + weighted_violation_score(vc2a);
                            
                            bool accept = false;
                            if (viols_after < viols_before) accept = true;
                            else if (viols_after == viols_before && cost_after < cost_before - 0.01) accept = true;
                            
                            if (accept) {
                                improved = true;
                            } else {
                                routes[v1] = saved_v1;
                                routes[v2] = saved_v2;
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
        int best_lateness = init_cr.total_lateness;
        double current_cost = best_cost;
        int current_hard_v = best_hard_v;
        int current_lateness = best_lateness;
        auto current_routes = routes;
        
        // Build initial route cache for delta evaluation (opt 7)
        build_route_cache(current_routes, vehs, emps);
        
        // Adaptive temperature: scale based on initial cost for problem-proportional SA
        double adaptive_start = std::max(start_temp, best_cost * 0.5);
        double temperature = adaptive_start;
        int iteration = 0;
        int accept_count = 0, improve_count = 0;
        int segment_iter = 0;
        
        // Initialize LAHC history with current cost
        lahc_history.assign(LAHC_LENGTH, best_cost);
        
        // Initialize solution pool with initial solution
        solution_pool.clear();
        solution_pool.push_back({routes, best_cost, best_hard_v});
        
        // Adaptive reheating state (opt 5)
        int iters_since_best = 0;
        int reheat_count = 0;
        const int REHEAT_THRESHOLD = g_config.reheat_threshold;
        const int MAX_REHEATS = g_config.max_reheats;
        const double REHEAT_FACTOR = 0.4;     // reheat to this fraction of start_temp
        
        // GLS calibration: lambda = 0.1 * average_route_cost / num_employees
        {
            CostResult init_check = calculate_cost_and_violations(routes, vehs, emps, meta);
            double avg_cost = (init_check.cost > 0 && total_employees > 0) ?
                              init_check.cost / total_employees : 100.0;
            gls_lambda = 0.1 * avg_cost / std::max(1, total_employees);
            edge_penalties.clear();
        }
        int gls_counter = 0;
        
        std::cout << "Initial cost: $" << best_cost 
                  << " (" << count_assigned(routes) << "/" << total_employees << " assigned"
                  << ", hard_v=" << best_hard_v << ")" << std::endl;
        std::cout << "Temperature: " << temperature << ", Cooling: " << cooling_rate << "\n";
        std::cout << "OPTIMIZATION PRIORITY: fewer hard violations > lower cost\n\n";
        
        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed >= time_limit) {
                std::cout << "\n Time limit reached (" << time_limit << "s)" << std::endl;
                break;
            }
            
            iteration++;
            segment_iter++;
            
            if (segment_iter >= 500) {
                update_weights();
                segment_iter = 0;
                
                std::cout << "\n--- Iteration " << iteration << " ---" << std::endl;
                std::cout << "Best: $" << best_cost << " | Current: $" << current_cost
                         << " | Temp: " << temperature
                         << " | Assigned: " << count_assigned(best_routes) << "/" << total_employees
                         << " | HardV: " << best_hard_v << " | Lateness: " << best_lateness << "min" << std::endl;
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
            
            // Adaptive destroy size: larger when stagnated, smaller when improving
            double adaptive_min = min_destroy_pct;
            double adaptive_max = max_destroy_pct;
            if (iters_since_best > 500) {
                adaptive_min = 0.30;  // Force larger destroys when stuck
                adaptive_max = 0.65;
            } else if (iters_since_best < 50) {
                adaptive_min = 0.08;  // Smaller destroys when recently improving
                adaptive_max = 0.35;
            }
            std::uniform_real_distribution<> pct_dis(adaptive_min, adaptive_max);
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
                case EXPENSIVE_ARC_REMOVAL: removed = destroy_expensive_arc(temp_routes, num_remove, vehs, emps); break;
                case STRING_REMOVAL: removed = destroy_string(temp_routes, num_remove, vehs, emps); break;
                case LATENESS_TARGETED_REMOVAL: removed = destroy_lateness_targeted(temp_routes, num_remove, vehs, emps); break;
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
                case CHEAPEST_VEHICLE_INSERTION: repair_cheapest_vehicle(temp_routes, removed, vehs, emps, meta, alns_enforce_soft); break;
            }
            
            // SAFETY: If repair left anything in removed, force insert
            if (!removed.empty()) {
                force_insert_all(temp_routes, removed, vehs, emps, meta, alns_enforce_soft);
            }
            
            // CRITICAL: Remove duplicates AND re-insert missing employees
            // This catches any bugs in destroy/repair operators
            {
                std::vector<int> emp_count(total_employees, 0);
                for (auto& route : temp_routes)
                    for (int e : route) {
                        if (e >= 0 && e < total_employees) emp_count[e]++;
                    }
                
                // Remove duplicates (keep first occurrence)
                bool had_dups = false;
                for (auto& route : temp_routes) {
                    auto it = route.begin();
                    while (it != route.end()) {
                        int e = *it;
                        if (e >= 0 && e < total_employees && emp_count[e] > 1) {
                            emp_count[e]--;
                            it = route.erase(it);
                            had_dups = true;
                        } else {
                            ++it;
                        }
                    }
                }
                
                // Re-insert missing
                std::vector<int> missing;
                for (int e = 0; e < total_employees; e++) {
                    if (emp_count[e] == 0) missing.push_back(e);
                }
                if (!missing.empty()) {
                    force_insert_all(temp_routes, missing, vehs, emps, meta, alns_enforce_soft);
                }
            }
            
            // ===== LOCAL SEARCH on modified routes =====
            apply_local_search(temp_routes, vehs, emps, meta);
            
            // ===== INTER-ROUTE MOVES =====
            apply_inter_route_moves(temp_routes, vehs, emps, meta);
            
            // ===== CROSS-EXCHANGE (every 5th iteration to save time) =====
            if (iteration % 5 == 0) {
                auto ce_now = std::chrono::high_resolution_clock::now();
                auto ce_elapsed = std::chrono::duration_cast<std::chrono::seconds>(ce_now - start_time).count();
                if (ce_elapsed < time_limit) {
                    apply_cross_exchange(temp_routes, vehs, emps, meta);
                }
            }
            
            // INTEGRITY CHECK after destroy-repair cycle
            if (!verify_solution_integrity(temp_routes, "destroy-repair")) {
                // Solution corrupted - skip this iteration
                temp_routes = current_routes;
                route_cache = route_cache;  // keep current cache
                continue;
            }
            
            // ===== DELTA EVALUATION (opt 7) =====
            // Only re-simulate routes that changed (comparing sizes with pre-destroy snapshot)
            std::vector<int> modified_vehicles;
            for (size_t vi = 0; vi < temp_routes.size(); vi++) {
                if ((int)temp_routes[vi].size() != pre_route_sizes[vi]) {
                    modified_vehicles.push_back((int)vi);
                } else {
                    // Size same but contents might differ --- check
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
            
            // Use fast delta evaluation for most iterations
            // Full calculation is only done periodically for sync and at the end
            CostResult cr = delta_evaluate(temp_routes, vehs, emps, meta, modified_vehicles);
            // Add GLS penalty to augmented cost for acceptance decision
            double gls_pen = calculate_gls_penalty(temp_routes, vehs, emps);
            double new_cost = cr.cost + gls_pen;
            double delta = new_cost - (current_cost + calculate_gls_penalty(current_routes, vehs, emps));
            
            bool accept = false;
            int new_hard_v = cr.hard_violations;
            int new_lateness = cr.total_lateness;
            
            // Use RAW cost (without GLS) for best-solution tracking
            double raw_new_cost = cr.cost;
            
            // Use shared comparison function --- compare against best using raw cost
            bool is_new_best = is_solution_better_lateness(new_hard_v, new_lateness, raw_new_cost,
                                                            best_hard_v, best_lateness, best_cost);
            
            if (is_new_best) {
                accept = true;
                best_cost = raw_new_cost;
                best_hard_v = new_hard_v;
                best_routes = temp_routes;
                improve_count++;
                iters_since_best = 0; // reset reheat counter (opt 5)
                best_lateness = new_lateness;
                std::cout << "   NEW BEST: $" << best_cost
                         << " (" << count_assigned(best_routes) << "/" << total_employees
                         << " assigned, hard_v=" << best_hard_v
                         << ", iter " << iteration << ")" << std::endl;
                
                // Update solution pool
                bool pool_has = false;
                for (auto& pe : solution_pool) {
                    if (std::abs(pe.cost - raw_new_cost) < 0.01) { pool_has = true; break; }
                }
                if (!pool_has) {
                    if ((int)solution_pool.size() < MAX_POOL_SIZE) {
                        solution_pool.push_back({temp_routes, raw_new_cost, new_hard_v});
                    } else {
                        // Replace worst in pool
                        int worst_idx = 0;
                        for (int pi = 1; pi < (int)solution_pool.size(); pi++) {
                            if (is_solution_better(solution_pool[worst_idx].hard_violations, 0, solution_pool[worst_idx].cost,
                                                    solution_pool[pi].hard_violations, 0, solution_pool[pi].cost)) {
                                worst_idx = pi;
                            }
                        }
                        solution_pool[worst_idx] = {temp_routes, raw_new_cost, new_hard_v};
                    }
                }
            } else if (new_hard_v > current_hard_v) {
                // NEVER accept a solution with MORE hard violations than current
                accept = false;
            } else if (new_hard_v < current_hard_v) {
                // ALWAYS accept fewer hard violations
                accept = true;
                improve_count++;
            } else if (new_lateness < current_lateness) {
                // SAME violation count but LESS total lateness - ALWAYS accept
                // This is the #1 sub-priority: minimize delay
                accept = true;
                improve_count++;
            } else if (delta < 0) {
                // Same violation count, lower cost --- accept
                accept = true;
                improve_count++;
            } else {
                // Same violation count, higher cost: use LAHC + SA hybrid
                // LAHC: accept if better than solution LAHC_LENGTH iterations ago
                int lahc_idx = iteration % LAHC_LENGTH;
                double lahc_ref = lahc_history[lahc_idx];
                if (new_cost <= lahc_ref) {
                    accept = true;
                } else {
                    // SA probability as fallback
                    double prob = std::exp(-delta / temperature);
                    std::uniform_real_distribution<> accept_dis(0.0, 1.0);
                    if (accept_dis(rng) < prob) {
                        accept = true;
                    }
                }
            }
            
            // Update LAHC history
            {
                int lahc_idx = iteration % LAHC_LENGTH;
                lahc_history[lahc_idx] = current_cost;
            }
            
            if (accept) {
                current_routes = temp_routes;
                current_cost = raw_new_cost;  // Track raw cost (without GLS)
                current_hard_v = new_hard_v;
                current_lateness = new_lateness;
                // Rebuild cache for accepted solution
                build_route_cache(current_routes, vehs, emps);
                accept_count++;
                destroy_successes[destroy_op]++;
                repair_successes[repair_op]++;
            }
            // If rejected, no cache update needed (cache still reflects current_routes)
            
            temperature *= cooling_rate;
            iters_since_best++;
            
            // ===== GLS PENALTY UPDATE (inspired by OR-Tools Guided Local Search) =====
            gls_counter++;
            if (gls_counter >= gls_update_interval) {
                gls_counter = 0;
                update_gls_penalties(current_routes, vehs, emps);
            }
            
            // ===== ADAPTIVE REHEATING / RANDOM RESTART (opt 5) =====
            if (iters_since_best >= REHEAT_THRESHOLD && reheat_count < MAX_REHEATS) {
                // Every 3rd reheat: do a RANDOM RESTART (reconstruct from scratch)
                // This provides much stronger diversification than just temperature reheating
                if (reheat_count > 0 && reheat_count % 3 == 0) {
                    std::cout << "   RANDOM RESTART #" << (reheat_count + 1) 
                             << " (rebuilding from scratch, iter " << iteration << ")" << std::endl;
                    
                    // Pool-aware restart: sometimes start from a random pool solution
                    // instead of building from scratch - better diversification
                    std::vector<int> all_emps;
                    for (int e = 0; e < total_employees; e++) all_emps.push_back(e);
                    
                    if (solution_pool.size() > 1 && reheat_count % 2 == 0) {
                        // Start from a random pool solution (not the best)
                        std::uniform_int_distribution<> pool_dis(0, (int)solution_pool.size() - 1);
                        int pool_idx = pool_dis(rng);
                        current_routes = solution_pool[pool_idx].routes;
                        // Partially destroy and rebuild for diversity
                        int destroy_count = total_employees / 3;
                        auto destroyed = destroy_random(current_routes, destroy_count);
                        std::shuffle(destroyed.begin(), destroyed.end(), rng);
                        repair_greedy(current_routes, destroyed, vehs, emps, meta, alns_enforce_soft);
                        // Force-insert any remaining from destroyed
                        if (!destroyed.empty()) {
                            force_insert_all(current_routes, destroyed, vehs, emps, meta, alns_enforce_soft);
                        }
                    } else {
                        // Full restart from scratch
                        current_routes.clear();
                        current_routes.resize(vehs.size());
                        std::shuffle(all_emps.begin(), all_emps.end(), rng);
                        repair_greedy(current_routes, all_emps, vehs, emps, meta, alns_enforce_soft);
                        // Force-insert any remaining from all_emps
                        if (!all_emps.empty()) {
                            force_insert_all(current_routes, all_emps, vehs, emps, meta, alns_enforce_soft);
                        }
                    }
                    
                    // CRITICAL: Remove duplicates and re-insert missing after restart
                    repair_integrity(current_routes, vehs, emps, meta, alns_enforce_soft);
                    
                    // Apply local search to clean up
                    apply_local_search(current_routes, vehs, emps, meta);
                    apply_inter_route_moves(current_routes, vehs, emps, meta);
                    
                    CostResult restart_cr = calculate_cost_and_violations(current_routes, vehs, emps, meta);
                    current_cost = restart_cr.cost;
                    current_hard_v = restart_cr.hard_violations;
                    current_lateness = restart_cr.total_lateness;
                    build_route_cache(current_routes, vehs, emps);
                    
                    // Check if restart found something better
                    bool restart_better = is_solution_better_lateness(current_hard_v, current_lateness, current_cost,
                                                                   best_hard_v, best_lateness, best_cost);
                    if (restart_better) {
                        best_cost = current_cost;
                        best_hard_v = current_hard_v;
                        best_routes = current_routes;
                        std::cout << "   RESTART FOUND NEW BEST: $" << best_cost << std::endl;
                    }
                    
                    temperature = adaptive_start * REHEAT_FACTOR;
                } else {
                    double new_temp = adaptive_start * REHEAT_FACTOR;
                    std::cout << "   REHEAT #" << (reheat_count + 1) 
                             << ": temp " << temperature << " --- " << new_temp
                             << " (stagnated " << iters_since_best << " iters)" << std::endl;
                    temperature = new_temp;
                    // Return to best known solution to diversify from there
                    current_routes = best_routes;
                    current_cost = best_cost;
                    current_hard_v = best_hard_v;
                    current_lateness = best_lateness;
                    
                    // Perturbation: randomly swap 2-3 pairs between routes for structural escape
                    int num_perturb = 2 + (rng() % 2);
                    for (int pp = 0; pp < num_perturb; pp++) {
                        // Pick two random non-empty routes
                        std::vector<int> ne;
                        for (size_t vi = 0; vi < current_routes.size(); vi++)
                            if (!current_routes[vi].empty()) ne.push_back((int)vi);
                        if (ne.size() >= 2) {
                            std::shuffle(ne.begin(), ne.end(), rng);
                            int rv1 = ne[0], rv2 = ne[1];
                            if (!current_routes[rv1].empty() && !current_routes[rv2].empty()) {
                                std::uniform_int_distribution<> d1(0, (int)current_routes[rv1].size()-1);
                                std::uniform_int_distribution<> d2(0, (int)current_routes[rv2].size()-1);
                                std::swap(current_routes[rv1][d1(rng)], current_routes[rv2][d2(rng)]);
                            }
                        }
                    }
                    
                    build_route_cache(current_routes, vehs, emps);
                    CostResult perturb_cr = calculate_cost_and_violations(current_routes, vehs, emps, meta);
                    current_cost = perturb_cr.cost;
                    current_hard_v = perturb_cr.hard_violations;
                    current_lateness = perturb_cr.total_lateness;
                }
                iters_since_best = 0;
                reheat_count++;
                // Partially reset GLS penalties to allow revisiting old arcs
                for (auto& [key, pen] : edge_penalties) {
                    pen *= 0.5;  // Decay penalties on reheat for diversification
                }
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
        
        // POST-ALNS passes removed - solution should be found within ALNS itself
        // (post_alns_optimize, cross_exchange, reduce_lateness_pass, exact_tsp_small_routes, ejection_chain removed)
        
        // CRITICAL: Final integrity repair after all optimization
        // CRITICAL: ALWAYS repair integrity before output - catches any remaining bugs
        repair_integrity(best_routes, vehs, emps, meta, user_enforce_soft);
        if (!verify_solution_integrity(best_routes, "FINAL-OUTPUT")) {
            std::cerr << "CRITICAL: Solution still has integrity issues after repair!\n";
            // Try one more time
            repair_integrity(best_routes, vehs, emps, meta, user_enforce_soft);
        }
        
        best_cost = calculate_cost(best_routes, vehs, emps, meta);
        
        routes = best_routes;
    }
    


    // ======================== LATENESS REDUCTION PASS ========================
    // After main optimization, specifically try to reduce lateness for each late employee.
    // For each late employee: try every other route to find one that delivers them on time.
    // SKIP if lateness penalties are 0 (user doesn't care about lateness magnitude)
    void reduce_lateness_pass(std::vector<std::vector<int>>& routes,
                               const std::vector<Vehicle>& vehs,
                               const std::vector<Employee>& emps,
                               const Metadata& meta) {
        // Skip if user set lateness penalties to 0 (1 min late = 1 hour late)
        if (g_config.lateness_per_min_penalty <= 0 && g_config.priority_lateness_multiplier <= 0) {
            return;  // User doesn't care about lateness magnitude, skip this pass
        }
        
        std::cout << "  Lateness reduction pass...\n";
        int improvements = 0;
        
        // Find all late employees, sorted by priority (highest first) then lateness
        struct LateEmp {
            int emp_id;
            int veh_idx;
            int pos_in_route;
            int lateness;
            int priority;
        };
        std::vector<LateEmp> late_list;
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
            for (int i = 0; i < (int)routes[v].size(); i++) {
                int e = routes[v][i];
                if (sim.office_arrival > emps[e].latest_arrival_deadline) {
                    int lat = sim.office_arrival - emps[e].latest_arrival_deadline;
                    late_list.push_back({e, (int)v, i, lat, emps[e].priority});
                }
            }
        }
        
        // Sort: highest priority first, then most late
        std::sort(late_list.begin(), late_list.end(), [](const LateEmp& a, const LateEmp& b) {
            if (a.priority != b.priority) return a.priority < b.priority; // lower number = higher priority
            return a.lateness > b.lateness;
        });
        
        for (const auto& le : late_list) {
            int emp = le.emp_id;
            int curr_v = -1, curr_pos = -1;
            
            // Find current position (may have changed from earlier moves)
            for (size_t v = 0; v < routes.size(); v++) {
                for (int i = 0; i < (int)routes[v].size(); i++) {
                    if (routes[v][i] == emp) { curr_v = (int)v; curr_pos = i; break; }
                }
                if (curr_v >= 0) break;
            }
            if (curr_v < 0) continue;
            
            // Check if still late
            RouteSimResult curr_sim = simulate_route(routes[curr_v], vehs[curr_v], emps);
            if (curr_sim.office_arrival <= emps[emp].latest_arrival_deadline) continue; // Fixed already
            int curr_lateness = curr_sim.office_arrival - emps[emp].latest_arrival_deadline;
            
            // Calculate current TOTAL cost (including all penalties)
            CostResult cr_before = calculate_cost_and_violations(routes, vehs, emps, meta);
            
            // Try every other route: find one with LOWER TOTAL COST (not just less lateness!)
            int best_v = -1, best_pos = -1;
            double best_cost = cr_before.cost;
            
            for (size_t v2 = 0; v2 < routes.size(); v2++) {
                if ((int)v2 == curr_v) continue;
                if ((int)routes[v2].size() >= vehs[v2].capacity) continue;
                
                for (size_t pos = 0; pos <= routes[v2].size(); pos++) {
                    // Temporarily make the move
                    routes[v2].insert(routes[v2].begin() + pos, emp);
                    auto saved_curr = routes[curr_v];
                    routes[curr_v].erase(std::find(routes[curr_v].begin(), routes[curr_v].end(), emp));
                    
                    // Calculate TOTAL cost after move (includes lateness + preference penalties!)
                    CostResult cr_after = calculate_cost_and_violations(routes, vehs, emps, meta);
                    
                    // Only accept if TOTAL cost improved (accounts for preference violations!)
                    if (cr_after.cost < best_cost) {
                        best_cost = cr_after.cost;
                        best_v = (int)v2;
                        best_pos = (int)pos;
                    }
                    
                    // Restore routes
                    routes[curr_v] = saved_curr;
                    routes[v2].erase(routes[v2].begin() + pos);
                }
            }
            
            if (best_v >= 0 && best_cost < cr_before.cost) {
                // Execute the move - it actually improves total cost
                routes[curr_v].erase(routes[curr_v].begin() + curr_pos);
                routes[best_v].insert(routes[best_v].begin() + best_pos, emp);
                improvements++;
                
                // Re-optimize intra-route ordering
                apply_local_search(routes, vehs, emps, meta);
            }
        }
        
        if (improvements > 0) {
            std::cout << "  Lateness reduction: " << improvements << " employees moved to faster routes\n";
        }
    }
    // ======================== EXACT TSP FOR SMALL ROUTES ========================
    // For routes with <=5 employees, try all permutations to find optimal ordering.
    // This guarantees optimal intra-route ordering which local search may miss.
    void exact_tsp_small_routes(std::vector<std::vector<int>>& routes,
                                 const std::vector<Vehicle>& vehs,
                                 const std::vector<Employee>& emps,
                                 const Metadata& meta) {
        int improvements = 0;
        for (size_t v = 0; v < routes.size(); v++) {
            int sz = (int)routes[v].size();
            if (sz < 2 || sz > 6) continue;  // Only solve small routes exactly
            
            // Current cost
            RouteSimResult sim_best = simulate_route(routes[v], vehs[v], emps);
            ViolationCount vc_best = count_route_violations(routes[v], vehs[v], emps, sim_best.office_arrival);
            double best_cost = sim_best.total_distance * vehs[v].cost_per_km;
            long long best_score = weighted_violation_score(vc_best);
            auto best_order = routes[v];
            
            // Try all permutations
            auto perm = routes[v];
            std::sort(perm.begin(), perm.end());
            do {
                RouteSimResult sim = simulate_route(perm, vehs[v], emps);
                ViolationCount vc = count_route_violations(perm, vehs[v], emps, sim.office_arrival);
                double cost = sim.total_distance * vehs[v].cost_per_km;
                long long score = weighted_violation_score(vc);
                
                // LATENESS-FIRST: lower weighted score > lower cost
                bool perm_better = false;
                if (score < best_score) perm_better = true;
                else if (score == best_score && cost < best_cost - 0.01) perm_better = true;
                if (perm_better) {
                    best_cost = cost;
                    best_score = score;
                    best_order = perm;
                }
            } while (std::next_permutation(perm.begin(), perm.end()));
            
            if (best_order != routes[v]) {
                routes[v] = best_order;
                improvements++;
            }
        }
        if (improvements > 0) {
            std::cout << "  Exact TSP: " << improvements << " routes optimized\n";
        }
    }
    // ======================== EJECTION CHAIN OPTIMIZATION ========================
    // Removes an employee from vehicle A, inserts into B (displacing someone from B),
    // displaced employee goes to C, etc. Finds improvements single moves cannot.
    void ejection_chain_optimize(std::vector<std::vector<int>>& routes,
                                   const std::vector<Vehicle>& vehs,
                                   const std::vector<Employee>& emps,
                                   const Metadata& meta) {
        std::cout << "  Ejection chain phase...\n";
        int chain_improvements = 0;
        
        CostResult cr_before = calculate_cost_and_violations(routes, vehs, emps, meta);
        
        for (size_t v1 = 0; v1 < routes.size(); v1++) {
            if (routes[v1].empty()) continue;
            
            for (int i = 0; i < (int)routes[v1].size(); i++) {
                int emp1 = routes[v1][i];
                
                for (size_t v2 = 0; v2 < routes.size(); v2++) {
                    if (v1 == v2) continue;
                    if (routes[v2].empty()) continue;
                    if ((int)routes[v2].size() < vehs[v2].capacity) continue;
                    
                    for (int j = 0; j < (int)routes[v2].size(); j++) {
                        int emp2 = routes[v2][j];
                        
                        auto saved = routes;
                        
                        routes[v1].erase(routes[v1].begin() + i);
                        routes[v2].erase(routes[v2].begin() + j);
                        
                        double best_c1 = 1e18;
                        int best_p1 = -1;
                        for (size_t p = 0; p <= routes[v2].size(); p++) {
                            int prev = (p == 0) ? vehs[v2].start_node : emps[routes[v2][p-1]].node_idx;
                            int curr = emps[emp1].node_idx;
                            int next = (p == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][p]].node_idx;
                            double c = (dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next]) * vehs[v2].cost_per_km;
                            if (c < best_c1) { best_c1 = c; best_p1 = (int)p; }
                        }
                        if (best_p1 >= 0) routes[v2].insert(routes[v2].begin() + best_p1, emp1);
                        
                        double best_c2 = 1e18;
                        int best_v3 = -1, best_p2 = -1;
                        for (size_t v3 = 0; v3 < routes.size(); v3++) {
                            if (v3 == v2) continue;
                            if ((int)routes[v3].size() >= vehs[v3].capacity) continue;
                            for (size_t p = 0; p <= routes[v3].size(); p++) {
                                int prev = (p == 0) ? vehs[v3].start_node : emps[routes[v3][p-1]].node_idx;
                                int curr = emps[emp2].node_idx;
                                int next = (p == routes[v3].size()) ? OFFICE_NODE : emps[routes[v3][p]].node_idx;
                                double c = (dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next]) * vehs[v3].cost_per_km;
                                if (c < best_c2) { best_c2 = c; best_v3 = (int)v3; best_p2 = (int)p; }
                            }
                        }
                        
                        if (best_v3 >= 0) {
                            routes[best_v3].insert(routes[best_v3].begin() + best_p2, emp2);
                            
                            CostResult cr_after = calculate_cost_and_violations(routes, vehs, emps, meta);
                            
                            if (is_solution_better_lateness(cr_after.hard_violations, cr_after.total_lateness, cr_after.cost,
                                                                 cr_before.hard_violations, cr_before.total_lateness, cr_before.cost)) {
                                if (verify_solution_integrity(routes, "ejection-chain")) {
                                    chain_improvements++;
                                    cr_before = cr_after;
                                    i = -1;
                                    break;
                                }
                                // Integrity failed - restore
                                routes = saved;
                            }
                        }
                        
                        routes = saved;
                    }
                    if (i < 0) break;
                }
                if (i < 0) break;
            }
        }
        
        if (chain_improvements > 0) {
            std::cout << "  Ejection chains: " << chain_improvements << " improvements\n";
        }
    }
    
    // ======================== POST-ALNS EXHAUSTIVE OPTIMIZATION ========================
    void post_alns_optimize(std::vector<std::vector<int>>& routes,
                            const std::vector<Vehicle>& vehs,
                            const std::vector<Employee>& emps,
                            const Metadata& meta) {
        
        std::cout << "\n--- Post-ALNS Exhaustive Optimization ---\n";
        
        // Phase 1: Intra-route optimization (quick)
        apply_local_search(routes, vehs, emps, meta);
        
        bool improved = true;
        int pass = 0;
        
        while (improved && pass < 150) {
            improved = false;
            pass++;
            
            double best_improvement = 0; // positive = saving
            int best_type = -1; // 0=relocate, 1=swap
            size_t best_v1 = 0, best_v2 = 0;
            int best_i = 0, best_j = 0;
            long long best_viol_delta = 0;
            
            // Pre-compute weighted violations for all non-empty routes
            std::vector<long long> viols(routes.size(), 0);
            for (size_t v = 0; v < routes.size(); v++) {
                if (routes[v].empty()) continue;
                RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                viols[v] = weighted_violation_score(vc);
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
                        
                        long long total_before = viols[v1] + viols[v2];
                        
                        for (size_t pos = 0; pos <= routes[v2].size(); pos++) {
                            int prev2 = (pos == 0) ? vehs[v2].start_node : emps[routes[v2][pos-1]].node_idx;
                            int next2 = (pos == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][pos]].node_idx;
                            double insert_cost = (dist_matrix[prev2][emp_node] + dist_matrix[emp_node][next2]
                                                  - dist_matrix[prev2][next2]) * vehs[v2].cost_per_km;
                            double net = insert_cost - remove_saving;
                            
                            // Try move
                            routes[v1].erase(routes[v1].begin() + i);
                            routes[v2].insert(routes[v2].begin() + pos, emp);
                            
                            long long v1_after = 0, v2_after = 0;
                            if (!routes[v1].empty()) {
                                RouteSimResult s1 = simulate_route(routes[v1], vehs[v1], emps);
                                ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, s1.office_arrival);
                                v1_after = weighted_violation_score(vc1);
                            }
                            {
                                RouteSimResult s2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, s2.office_arrival);
                                v2_after = weighted_violation_score(vc2);
                            }
                            long long total_after = v1_after + v2_after;
                            long long viol_delta = total_after - total_before;
                            
                            double improvement = -net + (viol_delta < 0 ? -viol_delta / 1000.0 : 0);
                            
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
                        
                        long long total_before = viols[v1] + viols[v2];
                        
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
                            long long total_after = weighted_violation_score(vc1) + weighted_violation_score(vc2);
                            long long viol_delta = total_after - total_before;
                            
                            double improvement = -net + (viol_delta < 0 ? -viol_delta / 1000.0 : 0);
                            
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
            
            // Re-run intra-route optimization after each inter-route move
            if (improved) {
                apply_local_search(routes, vehs, emps, meta);
            }
        }
        
        // Phase 2: Try to consolidate routes by emptying expensive vehicles
        // Inspired by OR-Tools' natural tendency to minimize vehicle usage
        std::cout << "  Post-consolidation phase...\n";
        {
            // Find active vehicles sorted by cost-per-km (most expensive first)
            std::vector<size_t> active;
            for (size_t v = 0; v < routes.size(); v++)
                if (!routes[v].empty()) active.push_back(v);
            
            std::sort(active.begin(), active.end(), [&](size_t a, size_t b) {
                return vehs[a].cost_per_km > vehs[b].cost_per_km;
            });
            
            for (size_t ai = 0; ai < active.size(); ai++) {
                size_t v1 = active[ai];
                if (routes[v1].empty()) continue;
                if ((int)routes[v1].size() > 4) continue;  // Try emptying routes up to 4 employees
                
                // Try moving ALL employees from v1 to other routes
                // Try multiple random orderings to find one that works
                auto saved_routes = routes;
                std::vector<int> to_move = routes[v1];
                bool consolidated = false;
                
                int num_orderings = std::min(10, std::max(1, (int)to_move.size() * 2));
                for (int ord = 0; ord < num_orderings && !consolidated; ord++) {
                    routes = saved_routes;
                    routes[v1].clear();
                    
                    // Shuffle for all but the first attempt (first = original order)
                    if (ord > 0) {
                        std::shuffle(to_move.begin(), to_move.end(), rng);
                    }
                
                    bool all_placed = true;
                    for (int emp : to_move) {
                        double best_cost = 1e18;
                        int best_v = -1, best_pos = -1;
                        
                        for (size_t v2 = 0; v2 < routes.size(); v2++) {
                            if (v2 == v1) continue;
                            if ((int)routes[v2].size() >= vehs[v2].capacity) continue;
                            
                            for (size_t pos = 0; pos <= routes[v2].size(); pos++) {
                                routes[v2].insert(routes[v2].begin() + pos, emp);
                                int h = 0, s = 0;
                                bool valid = validate_full_route(routes[v2], vehs[v2], emps, h, s, false, meta);
                                routes[v2].erase(routes[v2].begin() + pos);
                                if (valid && h == 0) {
                                    int prev = (pos == 0) ? vehs[v2].start_node : emps[routes[v2][pos-1]].node_idx;
                                    int curr = emps[emp].node_idx;
                                    int next = (pos == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][pos]].node_idx;
                                    double cost = (dist_matrix[prev][curr] + dist_matrix[curr][next] - dist_matrix[prev][next]) * vehs[v2].cost_per_km;
                                    if (cost < best_cost) {
                                        best_cost = cost;
                                        best_v = (int)v2;
                                        best_pos = (int)pos;
                                    }
                                }
                            }
                        }
                        
                        if (best_v >= 0) {
                            routes[best_v].insert(routes[best_v].begin() + best_pos, emp);
                        } else {
                            all_placed = false;
                            break;
                        }
                    }
                    
                    if (all_placed) {
                        // Check if new solution is better cost-wise
                        CostResult cr_new = calculate_cost_and_violations(routes, vehs, emps, meta);
                        CostResult cr_old = calculate_cost_and_violations(saved_routes, vehs, emps, meta);
                        if (cr_new.hard_violations <= cr_old.hard_violations && cr_new.cost < cr_old.cost) {
                            std::cout << "  Consolidated route " << v1 << " (ordering " << ord 
                                     << ", saved $" << (cr_old.cost - cr_new.cost) << ")\n";
                            apply_local_search(routes, vehs, emps, meta);
                            consolidated = true;  // Break out of orderings loop
                        } else {
                            routes = saved_routes;
                        }
                    } else {
                        routes = saved_routes;
                    }
                }  // end orderings loop
            }
        }
        
        // Phase 3: Final re-optimization after consolidation may have opened new opportunities
        std::cout << "  Final re-optimization after consolidation...\n";
        apply_local_search(routes, vehs, emps, meta);
        
        // One more round of best-improvement inter-route moves
        bool final_improved = true;
        int final_pass = 0;
        while (final_improved && final_pass < 50) {
            final_improved = false;
            final_pass++;
            
            // Pre-compute weighted violations
            std::vector<long long> final_viols(routes.size(), 0);
            for (size_t v = 0; v < routes.size(); v++) {
                if (routes[v].empty()) continue;
                RouteSimResult sim = simulate_route(routes[v], vehs[v], emps);
                ViolationCount vc = count_route_violations(routes[v], vehs[v], emps, sim.office_arrival);
                final_viols[v] = weighted_violation_score(vc);
            }
            
            double best_save = 0;
            int best_type = -1;
            size_t bv1 = 0, bv2 = 0;
            int bi = 0, bj = 0;
            long long bvd = 0;
            
            // Relocations
            for (size_t v1 = 0; v1 < routes.size(); v1++) {
                if (routes[v1].empty()) continue;
                for (int i = 0; i < (int)routes[v1].size(); i++) {
                    int emp = routes[v1][i];
                    int emp_node = emps[emp].node_idx;
                    int prev1 = (i == 0) ? vehs[v1].start_node : emps[routes[v1][i-1]].node_idx;
                    int next1 = (i == (int)routes[v1].size()-1) ? OFFICE_NODE : emps[routes[v1][i+1]].node_idx;
                    double rem_sav = (dist_matrix[prev1][emp_node] + dist_matrix[emp_node][next1]
                                      - dist_matrix[prev1][next1]) * vehs[v1].cost_per_km;
                    
                    for (size_t v2 = 0; v2 < routes.size(); v2++) {
                        if (v1 == v2) continue;
                        if ((int)routes[v2].size() >= vehs[v2].capacity) continue;
                        long long tb = final_viols[v1] + final_viols[v2];
                        
                        for (size_t pos = 0; pos <= routes[v2].size(); pos++) {
                            int p2 = (pos == 0) ? vehs[v2].start_node : emps[routes[v2][pos-1]].node_idx;
                            int n2 = (pos == routes[v2].size()) ? OFFICE_NODE : emps[routes[v2][pos]].node_idx;
                            double ins_cost = (dist_matrix[p2][emp_node] + dist_matrix[emp_node][n2]
                                               - dist_matrix[p2][n2]) * vehs[v2].cost_per_km;
                            double net = ins_cost - rem_sav;
                            
                            routes[v1].erase(routes[v1].begin() + i);
                            routes[v2].insert(routes[v2].begin() + pos, emp);
                            long long va1 = 0, va2 = 0;
                            if (!routes[v1].empty()) {
                                RouteSimResult s1 = simulate_route(routes[v1], vehs[v1], emps);
                                ViolationCount vc1 = count_route_violations(routes[v1], vehs[v1], emps, s1.office_arrival);
                                va1 = weighted_violation_score(vc1);
                            }
                            {
                                RouteSimResult s2 = simulate_route(routes[v2], vehs[v2], emps);
                                ViolationCount vc2 = count_route_violations(routes[v2], vehs[v2], emps, s2.office_arrival);
                                va2 = weighted_violation_score(vc2);
                            }
                            long long ta = va1 + va2;
                            long long vd = ta - tb;
                            double imp = -net + (vd < 0 ? -vd / 1000.0 : 0);
                            
                            if (vd < 0 || (vd == 0 && net < -1e-6)) {
                                if (imp > best_save) {
                                    best_save = imp; best_type = 0;
                                    bv1 = v1; bi = i; bv2 = v2; bj = (int)pos; bvd = vd;
                                }
                            }
                            routes[v2].erase(routes[v2].begin() + pos);
                            routes[v1].insert(routes[v1].begin() + i, emp);
                        }
                    }
                }
            }
            
            if (best_type == 0) {
                int emp = routes[bv1][bi];
                routes[bv1].erase(routes[bv1].begin() + bi);
                routes[bv2].insert(routes[bv2].begin() + bj, emp);
                final_improved = true;
                apply_local_search(routes, vehs, emps, meta);
            }
        }
        if (final_pass > 1) std::cout << "  Final re-opt: " << final_pass << " passes\n";
        
        // Phase 4: Score-aware intra-route optimization
        // The standard local search optimizes distance only. This pass tries
        // pairwise swaps within each route using SCORE (cost + time weighted)
        // to reduce wait times and improve the time component.
        int score_improvements = 0;
        for (size_t v = 0; v < routes.size(); v++) {
            if ((int)routes[v].size() < 2) continue;
            
            bool route_improved = true;
            while (route_improved) {
                route_improved = false;
                
                // Compute current route weighted violation score
                RouteSimResult sim_before = simulate_route(routes[v], vehs[v], emps);
                ViolationCount vc_before = count_route_violations(routes[v], vehs[v], emps, sim_before.office_arrival);
                long long score_viols_before = weighted_violation_score(vc_before);
                double cost_before = sim_before.total_distance * vehs[v].cost_per_km;
                double time_before = sim_before.office_arrival - vehs[v].available_from;
                double score_before = meta.cost_weight * cost_before + meta.time_weight * time_before;
                
                for (int a = 0; a < (int)routes[v].size() && !route_improved; a++) {
                    for (int b = a + 1; b < (int)routes[v].size() && !route_improved; b++) {
                        std::swap(routes[v][a], routes[v][b]);
                        
                        RouteSimResult sim_after = simulate_route(routes[v], vehs[v], emps);
                        ViolationCount vc_after = count_route_violations(routes[v], vehs[v], emps, sim_after.office_arrival);
                        long long score_viols_after = weighted_violation_score(vc_after);
                        double cost_after = sim_after.total_distance * vehs[v].cost_per_km;
                        double time_after = sim_after.office_arrival - vehs[v].available_from;
                        double score_after = meta.cost_weight * cost_after + meta.time_weight * time_after;
                        
                        // Accept if: lower weighted violation score, or same and lower dollar score
                        bool accept = false;
                        if (score_viols_after < score_viols_before) accept = true;
                        else if (score_viols_after == score_viols_before && score_after < score_before - 0.01) accept = true;
                        
                        if (accept) {
                            route_improved = true;
                            score_improvements++;
                        } else {
                            std::swap(routes[v][a], routes[v][b]); // undo
                        }
                    }
                }
            }
        }
        if (score_improvements > 0) {
            std::cout << "  Score-aware phase: " << score_improvements << " time-reducing swaps\n";
        }
        
        std::cout << "Post-optimization: " << pass << " passes\n";
    }
};

#endif
