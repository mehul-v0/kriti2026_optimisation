#ifndef VRP_SIMULATION_H
#define VRP_SIMULATION_H

#include "vrp_types.h"
#include "vrp_config.h"
#include "vrp_utils.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>
#include <map>
#include <climits>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

// ============================================================================
// UNIFIED SIMULATION ENGINE
// This is the SINGLE SOURCE OF TRUTH for all time, cost, and violation
// calculations. Every part of the solver (construction, ALNS, local search,
// output) MUST use these functions. This eliminates the trip-sequencing
// inconsistency bug where ALNS, cache, and output used different orderings.
// ============================================================================

// ---------- Per-Employee Result ----------
struct EmployeeResult {
    int employee_idx;
    int vehicle_physical_id;
    int trip_number;            // 1-based trip number on the physical vehicle
    int pickup_time;            // actual time vehicle arrives / service begins
    int dropoff_time;           // office arrival for this trip
    int ride_time;              // dropoff - pickup - service_time
    int lateness;               // max(0, dropoff - latest_arrival_deadline)
    int wait_with_pax;          // time vehicle waited at this stop with passengers aboard
    double distance_from_prev;
};

// ---------- Per-Trip Result ----------
struct TripResult {
    int trip_number;
    std::vector<int> employee_indices;
    double total_distance;
    double total_cost;
    int start_time;
    int end_time;               // office arrival
    int total_time;
    // Per-stop detail for output
    struct StopDetail {
        int node;
        int employee_idx;       // -1 for depot/office
        std::string location_name;
        int arrival_time;
        int departure_time;
        int wait_time;
        double distance_from_prev;
    };
    std::vector<StopDetail> stops;
};

// ---------- Per-Physical-Vehicle Result ----------
struct PhysVehicleResult {
    int physical_id;
    std::string vehicle_id;
    std::vector<TripResult> trips;
    double total_cost;
    double total_distance;
    int total_time;
};

// ---------- Full Simulation Result ----------
struct SimulationResult {
    std::vector<PhysVehicleResult> vehicles;
    std::vector<EmployeeResult> employee_results; // indexed by employee order encountered
    
    // Aggregate metrics
    double total_cost;
    double total_time;
    int hard_violations;        // time window breaches
    int soft_violations;        // pref violations (sharing + vehicle type)
    int total_lateness;         // sum of minutes late
    double priority_weighted_lateness;
    int total_wait_with_pax;    // total minutes passengers waited aboard stationary vehicle
    int vehicles_used;
    double total_distance;
    
    // Phase 3 metrics
    int ride_time_violations;       // max ride time breaches (Parragh constraint 18)
    int excess_ride_time;           // sum of excess ride time minutes
    double priority_weighted_ride;  // priority-weighted total ride time (Portell 4.3)
    
    // Scoring
    double score;
};

// ============================================================================
// simulate_single_trip: Simulate one trip (pickup sequence → office)
// This is the atomic building block. Everything else calls this.
// ============================================================================
inline TripResult simulate_single_trip(
    const std::vector<int>& emp_list,
    int trip_number,
    int start_node,
    int start_time,
    double speed_kmph,
    double cost_per_km,
    const std::vector<Employee>& emps)
{
    TripResult tr;
    tr.trip_number = trip_number;
    tr.employee_indices = emp_list;
    tr.total_distance = 0;
    tr.start_time = start_time;
    
    int curr_time = start_time;
    int curr_node = start_node;
    
    // Start stop (depot or office)
    TripResult::StopDetail start_stop;
    start_stop.node = curr_node;
    start_stop.employee_idx = -1;
    start_stop.location_name = (start_node == OFFICE_NODE) ? "Office" : "Vehicle Depot";
    start_stop.arrival_time = curr_time;
    start_stop.departure_time = curr_time;
    start_stop.wait_time = 0;
    start_stop.distance_from_prev = 0.0;
    tr.stops.push_back(start_stop);
    
    // Pickup each employee
    for (int e : emp_list) {
        double dist = dist_matrix[curr_node][emps[e].node_idx];
        int travel = (int)std::round((dist / speed_kmph) * 60.0);
        int arrival = curr_time + travel;
        int wait = std::max(0, emps[e].earliest_pickup - arrival);
        int depart = std::max(arrival, emps[e].earliest_pickup);
        
        TripResult::StopDetail s;
        s.node = emps[e].node_idx;
        s.employee_idx = e;
        s.location_name = emps[e].employee_id + " Pickup";
        s.arrival_time = arrival;
        s.departure_time = depart;
        s.wait_time = wait;
        s.distance_from_prev = dist;
        tr.stops.push_back(s);
        
        tr.total_distance += dist;
        curr_time = depart;
        curr_node = emps[e].node_idx;
    }
    
    // Return to office
    double dist_off = dist_matrix[curr_node][OFFICE_NODE];
    int travel_off = (int)std::round((dist_off / speed_kmph) * 60.0);
    int off_arr = curr_time + travel_off;
    
    TripResult::StopDetail off_stop;
    off_stop.node = OFFICE_NODE;
    off_stop.employee_idx = -1;
    off_stop.location_name = "Office (Drop-off)";
    off_stop.arrival_time = off_arr;
    off_stop.departure_time = off_arr;
    off_stop.wait_time = 0;
    off_stop.distance_from_prev = dist_off;
    tr.stops.push_back(off_stop);
    
    tr.total_distance += dist_off;
    tr.total_cost = tr.total_distance * cost_per_km;
    tr.end_time = off_arr;
    tr.total_time = off_arr - start_time;
    
    return tr;
}

// ============================================================================
// find_best_trip_permutation: For a physical vehicle's trips, find the
// ordering that minimizes violations > lateness > cost.
// This is the CANONICAL ordering logic used everywhere.
// ============================================================================
struct TripPermResult {
    std::vector<int> best_perm;
    int violations;
    int lateness;
    double cost;
};

inline TripPermResult find_best_trip_permutation(
    const std::vector<std::vector<int>>& emp_lists,
    const Vehicle& pv,
    const std::vector<Employee>& emps)
{
    int K = (int)emp_lists.size();
    if (K == 0) return {{}, 0, 0, 0.0};
    
    // Lambda: simulate a full trip sequence for a given permutation
    auto evaluate_perm = [&](const std::vector<int>& perm) 
        -> std::tuple<int, int, double> 
    {
        int time_violations = 0;
        int total_lateness = 0;
        double total_cost = 0;
        int next_avail = pv.available_from;
        
        for (int ti = 0; ti < K; ti++) {
            const auto& emp_list = emp_lists[perm[ti]];
            int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
            int curr_time = next_avail;
            int curr_node = start_node;
            double route_dist = 0.0;
            
            for (int e : emp_list) {
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
            
            for (int e : emp_list) {
                if (office_arrival > emps[e].latest_arrival_deadline) {
                    time_violations++;
                    total_lateness += (office_arrival - emps[e].latest_arrival_deadline);
                }
            }
            
            total_cost += route_dist * pv.cost_per_km;
            next_avail = office_arrival;
        }
        return {time_violations, total_lateness, total_cost};
    };
    
    std::vector<int> perm(K);
    std::iota(perm.begin(), perm.end(), 0);
    
    int best_violations = INT_MAX;
    int best_lateness = INT_MAX;
    double best_cost = 1e18;
    std::vector<int> best_perm = perm;
    
    if (K <= 7) {
        // Exact: try all K! permutations (max 5040 for K=7)
        do {
            auto [viols, lateness, cost] = evaluate_perm(perm);
            if (viols < best_violations ||
                (viols == best_violations && lateness < best_lateness) ||
                (viols == best_violations && lateness == best_lateness && cost < best_cost)) {
                best_violations = viols;
                best_lateness = lateness;
                best_cost = cost;
                best_perm = perm;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));
    } else {
        // Heuristic: try earliest-deadline-first and latest-deadline-first
        auto perm1 = perm;
        std::sort(perm1.begin(), perm1.end(), [&](int a, int b) {
            int min_a = INT_MAX, min_b = INT_MAX;
            for (int e : emp_lists[a]) min_a = std::min(min_a, emps[e].latest_arrival_deadline);
            for (int e : emp_lists[b]) min_b = std::min(min_b, emps[e].latest_arrival_deadline);
            return min_a < min_b;
        });
        {
            auto [v1, l1, c1] = evaluate_perm(perm1);
            best_violations = v1; best_lateness = l1; best_cost = c1; best_perm = perm1;
        }
        
        // Also try: short distance trips first, long distance trips first
        auto perm2 = perm;
        std::sort(perm2.begin(), perm2.end(), [&](int a, int b) {
            double da = 0, db = 0;
            for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            return da < db;
        });
        {
            auto [v2, l2, c2] = evaluate_perm(perm2);
            if (v2 < best_violations || (v2 == best_violations && l2 < best_lateness) ||
                (v2 == best_violations && l2 == best_lateness && c2 < best_cost)) {
                best_violations = v2; best_lateness = l2; best_cost = c2; best_perm = perm2;
            }
        }
        
        auto perm3 = perm;
        std::sort(perm3.begin(), perm3.end(), [&](int a, int b) {
            double da = 0, db = 0;
            for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            return da > db;
        });
        {
            auto [v3, l3, c3] = evaluate_perm(perm3);
            if (v3 < best_violations || (v3 == best_violations && l3 < best_lateness) ||
                (v3 == best_violations && l3 == best_lateness && c3 < best_cost)) {
                best_violations = v3; best_lateness = l3; best_cost = c3; best_perm = perm3;
            }
        }
    }
    
    return {best_perm, best_violations, best_lateness, best_cost};
}

// ============================================================================
// simulate_full_solution: THE master simulation function.
// Groups routes by physical vehicle, finds optimal trip ordering,
// computes per-employee results, violations, costs, everything.
// Used by ALNS cost function AND output formatter — guarantees consistency.
// ============================================================================
inline SimulationResult simulate_full_solution(
    const std::vector<std::vector<int>>& routes,
    const std::vector<Vehicle>& virt_vehs,
    const std::vector<Vehicle>& phys_vehs,
    const std::vector<Employee>& emps,
    const Metadata& meta,
    int total_employees = -1)  // pass -1 to auto-detect
{
    SimulationResult result;
    result.total_cost = 0;
    result.total_time = 0;
    result.hard_violations = 0;
    result.soft_violations = 0;
    result.total_lateness = 0;
    result.priority_weighted_lateness = 0;
    result.total_wait_with_pax = 0;
    result.vehicles_used = 0;
    result.total_distance = 0;
    result.ride_time_violations = 0;
    result.excess_ride_time = 0;
    result.priority_weighted_ride = 0;
    
    if (total_employees < 0) total_employees = (int)emps.size();
    
    // Step 1: Group routes by physical vehicle
    std::map<int, std::vector<std::vector<int>>> phys_emp_lists;
    std::map<int, std::vector<size_t>> phys_route_indices;
    
    int assigned = 0;
    for (size_t v = 0; v < routes.size(); v++) {
        if (routes[v].empty()) continue;
        int phys_id = virt_vehs[v].physical_id;
        phys_emp_lists[phys_id].push_back(routes[v]);
        phys_route_indices[phys_id].push_back(v);
        assigned += (int)routes[v].size();
    }
    
    // Step 2: For each physical vehicle, find optimal trip ordering & simulate
    for (auto& [phys_id, emp_lists] : phys_emp_lists) {
        if (emp_lists.empty()) continue;
        
        const Vehicle& pv = (phys_id < (int)phys_vehs.size()) ? phys_vehs[phys_id] : virt_vehs[phys_route_indices[phys_id][0]];
        
        // Find best trip permutation (same logic for ALNS and output)
        TripPermResult tpr = find_best_trip_permutation(emp_lists, pv, emps);
        
        PhysVehicleResult pvr;
        pvr.physical_id = phys_id;
        pvr.vehicle_id = pv.vehicle_id;
        pvr.total_cost = 0;
        pvr.total_distance = 0;
        pvr.total_time = 0;
        
        int next_avail = pv.available_from;
        int K = (int)emp_lists.size();
        
        for (int ti = 0; ti < K; ti++) {
            const auto& emp_list = emp_lists[tpr.best_perm[ti]];
            int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
            
            TripResult tr = simulate_single_trip(
                emp_list, ti + 1, start_node, next_avail,
                pv.speed_kmph, pv.cost_per_km, emps);
            
            int office_arrival = tr.end_time;
            
            // Count violations for employees on this trip
            int trip_size = (int)emp_list.size();
            int pax_aboard = 0;
            
            for (size_t si = 1; si < tr.stops.size() - 1; si++) { // skip depot and office stops
                int e = tr.stops[si].employee_idx;
                if (e < 0) continue;
                
                // Per-employee results
                EmployeeResult er;
                er.employee_idx = e;
                er.vehicle_physical_id = phys_id;
                er.trip_number = ti + 1;
                er.pickup_time = tr.stops[si].departure_time;
                er.dropoff_time = office_arrival;
                er.ride_time = office_arrival - tr.stops[si].departure_time;
                // Delay = how late arriving past the hard deadline (latest_drop + priority_delay)
                // Only counts as late when past the full allowed window
                er.lateness = std::max(0, office_arrival - emps[e].latest_arrival_deadline);
                er.distance_from_prev = tr.stops[si].distance_from_prev;
                
                // Waiting with passengers aboard
                er.wait_with_pax = 0;
                if (pax_aboard > 0 && tr.stops[si].wait_time > 0) {
                    er.wait_with_pax = tr.stops[si].wait_time;
                    result.total_wait_with_pax += tr.stops[si].wait_time * pax_aboard;
                }
                pax_aboard++;
                
                result.employee_results.push_back(er);
                
                // Hard violations: time window
                if (office_arrival > emps[e].latest_arrival_deadline) {
                    result.hard_violations++;
                    int lateness = office_arrival - emps[e].latest_arrival_deadline;
                    result.total_lateness += lateness;
                    double pw = g_config.get_priority_weight(emps[e].priority);
                    result.priority_weighted_lateness += lateness * pw;
                }
                
                // Max ride time constraint (Phase 3A - Parragh constraint 18, Portell constraint 11)
                {
                    int max_rt = g_config.get_max_ride_time(emps[e].priority);
                    if (er.ride_time > max_rt) {
                        result.ride_time_violations++;
                        result.excess_ride_time += er.ride_time - max_rt;
                    }
                    double pw3 = g_config.get_priority_weight(emps[e].priority);
                    result.priority_weighted_ride += er.ride_time * pw3;
                }
                
                // Soft violations: sharing preference
                if (emps[e].sharing_pref < trip_size) result.soft_violations++;
                
                // Soft violations: vehicle preference
                if (emps[e].vehicle_pref == 1 && pv.category != 1) result.soft_violations++;
                if (emps[e].vehicle_pref == 2 && pv.category == 1) result.soft_violations++;
                
                // Soft violations: vehicle mode compatibility (Phase 3B)
                if (!is_mode_compatible(emps[e].priority, emps[e].vehicle_pref, pv.vehicle_mode))
                    result.soft_violations++;
            }
            
            pvr.trips.push_back(tr);
            pvr.total_cost += tr.total_cost;
            pvr.total_distance += tr.total_distance;
            pvr.total_time += tr.total_time;
            
            next_avail = office_arrival;
        }
        
        result.vehicles.push_back(pvr);
        result.total_cost += pvr.total_cost;
        result.total_time += pvr.total_time;
        result.total_distance += pvr.total_distance;
        result.vehicles_used++;
    }
    
    // Compute score (same formula everywhere)
    double score = 0.0;
    int unassigned = total_employees - assigned;
    score += unassigned * g_config.unassigned_penalty;
    score += result.hard_violations * g_config.time_violation_penalty;
    score += result.total_lateness * g_config.lateness_per_min_penalty;
    score += result.priority_weighted_lateness * g_config.priority_lateness_multiplier;
    score += result.soft_violations * g_config.pref_violation_penalty;
    score += meta.cost_weight * result.total_cost + meta.time_weight * result.total_time;
    score += result.vehicles_used * g_config.vehicle_activation_cost;
    // Waiting-with-passengers penalty theta (Parragh 2011, Objective Function 1)
    score += result.total_wait_with_pax * g_config.wait_with_pax_penalty;
    // Max ride time violation penalty (Phase 3A)
    score += result.ride_time_violations * g_config.ride_time_violation_penalty;
    score += result.excess_ride_time * g_config.lateness_per_min_penalty * 0.5;
    // Priority-weighted ride time (Phase 3D - Portell Section 4.3)
    score += result.priority_weighted_ride * g_config.priority_ride_time_weight;
    
    result.score = score;
    return result;
}

// ============================================================================
// PER-PHYSICAL-VEHICLE COST COMPONENTS (Phase 2C - Incremental Delta Evaluation)
// Stores all additive cost components for one physical vehicle, enabling
// incremental updates: only re-evaluate affected vehicles after destroy/repair.
// ============================================================================
struct PhysVehicleCostComponents {
    double dist_cost = 0;
    double time = 0;
    int assigned = 0;
    int hard_time = 0;
    int pref_v = 0;
    int total_lateness = 0;
    double priority_weighted_lateness = 0;
    int total_wait_pax = 0;
    int ride_time_violations = 0;
    int excess_ride_time = 0;
    double priority_weighted_ride = 0;
    bool active = false;  // has any assigned employees
};

// Evaluate a single physical vehicle's trips and return its cost components.
// emp_lists[i] = employee indices for trip i. pv = the physical vehicle.
inline PhysVehicleCostComponents evaluate_physical_vehicle(
    const std::vector<std::vector<int>>& emp_lists,
    const Vehicle& pv,
    const std::vector<Employee>& emps)
{
    PhysVehicleCostComponents c;
    int K = (int)emp_lists.size();
    if (K == 0) return c;
    c.active = true;
    
    // Find best trip permutation (must match find_best_trip_permutation logic)
    // Lambda: evaluate a given permutation (violations > lateness > cost)
    auto eval_perm_fast = [&](const std::vector<int>& p) -> std::tuple<int, int, double> {
        int tv = 0, tl = 0; double tc = 0;
        int na = pv.available_from;
        for (int ti = 0; ti < K; ti++) {
            const auto& el = emp_lists[p[ti]];
            int sn = (ti == 0) ? pv.start_node : OFFICE_NODE;
            int ct = na, cn = sn; double rd = 0;
            for (int e : el) {
                double d = dist_matrix[cn][emps[e].node_idx];
                rd += d;
                int tr = (int)std::round((d / pv.speed_kmph) * 60.0);
                ct = std::max(ct + tr, emps[e].earliest_pickup);
                cn = emps[e].node_idx;
            }
            double doff = dist_matrix[cn][OFFICE_NODE];
            rd += doff;
            int oa = ct + (int)std::round((doff / pv.speed_kmph) * 60.0);
            for (int e : el) {
                if (oa > emps[e].latest_arrival_deadline) {
                    tv++; tl += oa - emps[e].latest_arrival_deadline;
                }
            }
            tc += rd * pv.cost_per_km;
            na = oa;
        }
        return {tv, tl, tc};
    };
    auto is_better = [](int v1, int l1, double c1, int v2, int l2, double c2) {
        return v1 < v2 || (v1 == v2 && l1 < l2) || (v1 == v2 && l1 == l2 && c1 < c2);
    };
    
    std::vector<int> perm(K);
    std::iota(perm.begin(), perm.end(), 0);
    
    if (K <= 6) {
        // Exact search (up to 720 perms for K=6) — matches find_best_trip_permutation
        int best_v = INT_MAX, best_l = INT_MAX;
        double best_c = 1e18;
        std::vector<int> best_p = perm;
        do {
            auto [tv, tl, tc] = eval_perm_fast(perm);
            if (is_better(tv, tl, tc, best_v, best_l, best_c)) {
                best_v = tv; best_l = tl; best_c = tc; best_p = perm;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));
        perm = best_p;
    } else {
        // Heuristic for K>6: try 3 orderings (same as find_best_trip_permutation)
        int best_v = INT_MAX, best_l = INT_MAX;
        double best_c = 1e18;
        std::vector<int> best_p = perm;
        // 1. Earliest deadline first
        auto p1 = perm;
        std::sort(p1.begin(), p1.end(), [&](int a, int b) {
            int ma = INT_MAX, mb = INT_MAX;
            for (int e : emp_lists[a]) ma = std::min(ma, emps[e].latest_arrival_deadline);
            for (int e : emp_lists[b]) mb = std::min(mb, emps[e].latest_arrival_deadline);
            return ma < mb;
        });
        { auto [v1, l1, c1] = eval_perm_fast(p1); best_v = v1; best_l = l1; best_c = c1; best_p = p1; }
        // 2. Short distance trips first
        auto p2 = perm;
        std::sort(p2.begin(), p2.end(), [&](int a, int b) {
            double da = 0, db = 0;
            for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            return da < db;
        });
        { auto [v2, l2, c2] = eval_perm_fast(p2); if (is_better(v2, l2, c2, best_v, best_l, best_c)) { best_v = v2; best_l = l2; best_c = c2; best_p = p2; } }
        // 3. Long distance trips first
        auto p3 = perm;
        std::sort(p3.begin(), p3.end(), [&](int a, int b) {
            double da = 0, db = 0;
            for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
            return da > db;
        });
        { auto [v3, l3, c3] = eval_perm_fast(p3); if (is_better(v3, l3, c3, best_v, best_l, best_c)) { best_v = v3; best_l = l3; best_c = c3; best_p = p3; } }
        perm = best_p;
    }
    
    // Simulate with best permutation
    int next_avail = pv.available_from;
    for (int ti = 0; ti < K; ti++) {
        const auto& route = emp_lists[perm[ti]];
        int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
        int curr_time = next_avail;
        int curr_node = start_node;
        double route_dist = 0.0;
        int pax_aboard = 0;
        
        int pickup_times_arr[64];
        int pt_idx = 0;
        
        for (int e : route) {
            double d = dist_matrix[curr_node][emps[e].node_idx];
            route_dist += d;
            int travel = (int)std::round((d / pv.speed_kmph) * 60.0);
            int arrival = curr_time + travel;
            int wait = std::max(0, emps[e].earliest_pickup - arrival);
            if (pax_aboard > 0 && wait > 0) c.total_wait_pax += wait * pax_aboard;
            curr_time = std::max(arrival, emps[e].earliest_pickup);
            pickup_times_arr[pt_idx++] = curr_time;
            curr_node = emps[e].node_idx;
            pax_aboard++;
        }
        
        double d_off = dist_matrix[curr_node][OFFICE_NODE];
        route_dist += d_off;
        int travel_off = (int)std::round((d_off / pv.speed_kmph) * 60.0);
        int office_arrival = curr_time + travel_off;
        
        c.dist_cost += route_dist * pv.cost_per_km;
        c.time += (office_arrival - next_avail);
        
        int sz = (int)route.size();
        c.assigned += sz;
        for (int idx = 0; idx < sz; idx++) {
            int e = route[idx];
            double pw = g_config.get_priority_weight(emps[e].priority);
            
            if (office_arrival > emps[e].latest_arrival_deadline) {
                c.hard_time++;
                int lat = office_arrival - emps[e].latest_arrival_deadline;
                c.total_lateness += lat;
                c.priority_weighted_lateness += lat * pw;
            }
            
            int ride_time = office_arrival - pickup_times_arr[idx];
            int max_rt = g_config.get_max_ride_time(emps[e].priority);
            if (ride_time > max_rt) {
                c.ride_time_violations++;
                c.excess_ride_time += ride_time - max_rt;
            }
            c.priority_weighted_ride += ride_time * pw;
            
            if (emps[e].sharing_pref < sz) c.pref_v++;
            if (emps[e].vehicle_pref == 1 && pv.category != 1) c.pref_v++;
            if (emps[e].vehicle_pref == 2 && pv.category == 1) c.pref_v++;
            if (!is_mode_compatible(emps[e].priority, emps[e].vehicle_pref, pv.vehicle_mode))
                c.pref_v++;
        }
        
        next_avail = office_arrival;
    }
    
    return c;
}

// Compute final score from accumulated cost component sums
inline double compute_score_from_components(
    double sum_dist_cost, double sum_time, int sum_assigned,
    int sum_hard_time, int sum_pref_v, int sum_total_lateness,
    double sum_priority_weighted_lateness, int sum_total_wait_pax,
    int sum_ride_time_violations, int sum_excess_ride_time,
    double sum_priority_weighted_ride, int vehicles_used,
    const Metadata& meta, int total_employees)
{
    double score = 0.0;
    int unassigned = total_employees - sum_assigned;
    score += unassigned * g_config.unassigned_penalty;
    score += sum_hard_time * g_config.time_violation_penalty;
    score += sum_total_lateness * g_config.lateness_per_min_penalty;
    score += sum_priority_weighted_lateness * g_config.priority_lateness_multiplier;
    score += sum_pref_v * g_config.pref_violation_penalty;
    score += meta.cost_weight * sum_dist_cost + meta.time_weight * sum_time;
    score += vehicles_used * g_config.vehicle_activation_cost;
    score += sum_total_wait_pax * g_config.wait_with_pax_penalty;
    score += sum_ride_time_violations * g_config.ride_time_violation_penalty;
    score += sum_excess_ride_time * g_config.lateness_per_min_penalty * 0.5;
    score += sum_priority_weighted_ride * g_config.priority_ride_time_weight;
    return score;
}

// ============================================================================
// FAST cost-only evaluation for ALNS (no stop details, no employee results)
// Same logic as simulate_full_solution but stripped down for speed.
// ============================================================================
struct FastCostResult {
    double score;
    int hard_violations;
    int total_lateness;
    double raw_cost;
    int soft_violations;
};

inline FastCostResult fast_evaluate_solution(
    const std::vector<std::vector<int>>& routes,
    const std::vector<Vehicle>& vehs,
    const std::vector<Employee>& emps,
    const Metadata& meta,
    int total_employees)
{
    double total_dist_cost = 0.0;
    double total_time = 0.0;
    int assigned = 0;
    int hard_time = 0, pref_v = 0, total_lateness = 0;
    double priority_weighted_lateness = 0.0;
    int vehicles_used = 0;
    int total_wait_pax = 0;
    int ride_time_violations = 0;
    int excess_ride_time = 0;
    double priority_weighted_ride = 0.0;
    
    // Group by physical vehicle
    std::map<int, std::vector<size_t>> phys_to_routes;
    for (size_t v = 0; v < routes.size(); v++) {
        if (!routes[v].empty()) {
            phys_to_routes[vehs[v].physical_id].push_back(v);
            assigned += (int)routes[v].size();
        }
    }
    
    for (auto& [phys_id, route_indices] : phys_to_routes) {
        if (route_indices.empty()) continue;
        vehicles_used++;
        
        const Vehicle& pv = vehs[route_indices[0]];
        int K = (int)route_indices.size();
        
        // Collect emp_lists for permutation search
        std::vector<std::vector<int>> emp_lists;
        emp_lists.reserve(K);
        for (size_t vi : route_indices) emp_lists.push_back(routes[vi]);
        
        // Find best permutation using canonical logic
        // Must match find_best_trip_permutation: exact for K<=6, multi-heuristic for K>6
        auto eval_perm_fe = [&](const std::vector<int>& p) -> std::tuple<int, int, double> {
            int tv = 0, tl = 0; double tc = 0;
            int na = pv.available_from;
            for (int ti = 0; ti < K; ti++) {
                const auto& el = emp_lists[p[ti]];
                int sn = (ti == 0) ? pv.start_node : OFFICE_NODE;
                int ct = na, cn = sn; double rd = 0;
                for (int e : el) {
                    double d = dist_matrix[cn][emps[e].node_idx];
                    rd += d;
                    int tr = (int)std::round((d / pv.speed_kmph) * 60.0);
                    ct = std::max(ct + tr, emps[e].earliest_pickup);
                    cn = emps[e].node_idx;
                }
                double doff = dist_matrix[cn][OFFICE_NODE];
                rd += doff;
                int oa = ct + (int)std::round((doff / pv.speed_kmph) * 60.0);
                for (int e : el) {
                    if (oa > emps[e].latest_arrival_deadline) {
                        tv++; tl += oa - emps[e].latest_arrival_deadline;
                    }
                }
                tc += rd * pv.cost_per_km;
                na = oa;
            }
            return {tv, tl, tc};
        };
        auto is_better_fe = [](int v1, int l1, double c1, int v2, int l2, double c2) {
            return v1 < v2 || (v1 == v2 && l1 < l2) || (v1 == v2 && l1 == l2 && c1 < c2);
        };
        
        std::vector<int> perm(K);
        std::iota(perm.begin(), perm.end(), 0);
        
        if (K <= 6) {
            // Exact search (up to 720 perms for K=6)
            int best_v = INT_MAX, best_l = INT_MAX;
            double best_c = 1e18;
            std::vector<int> best_p = perm;
            do {
                auto [tv, tl, tc] = eval_perm_fe(perm);
                if (is_better_fe(tv, tl, tc, best_v, best_l, best_c)) {
                    best_v = tv; best_l = tl; best_c = tc; best_p = perm;
                }
            } while (std::next_permutation(perm.begin(), perm.end()));
            perm = best_p;
        } else {
            // Heuristic for K>6: try 3 orderings
            int best_v = INT_MAX, best_l = INT_MAX;
            double best_c = 1e18;
            std::vector<int> best_p = perm;
            auto p1 = perm;
            std::sort(p1.begin(), p1.end(), [&](int a, int b) {
                int ma = INT_MAX, mb = INT_MAX;
                for (int e : emp_lists[a]) ma = std::min(ma, emps[e].latest_arrival_deadline);
                for (int e : emp_lists[b]) mb = std::min(mb, emps[e].latest_arrival_deadline);
                return ma < mb;
            });
            { auto [v1, l1, c1] = eval_perm_fe(p1); best_v = v1; best_l = l1; best_c = c1; best_p = p1; }
            auto p2 = perm;
            std::sort(p2.begin(), p2.end(), [&](int a, int b) {
                double da = 0, db = 0;
                for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                return da < db;
            });
            { auto [v2, l2, c2] = eval_perm_fe(p2); if (is_better_fe(v2, l2, c2, best_v, best_l, best_c)) { best_v = v2; best_l = l2; best_c = c2; best_p = p2; } }
            auto p3 = perm;
            std::sort(p3.begin(), p3.end(), [&](int a, int b) {
                double da = 0, db = 0;
                for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                return da > db;
            });
            { auto [v3, l3, c3] = eval_perm_fe(p3); if (is_better_fe(v3, l3, c3, best_v, best_l, best_c)) { best_v = v3; best_l = l3; best_c = c3; best_p = p3; } }
            perm = best_p;
        }
        
        // Simulate with best permutation
        int next_avail = pv.available_from;
        for (int ti = 0; ti < K; ti++) {
            const auto& route = emp_lists[perm[ti]];
            int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
            int curr_time = next_avail;
            int curr_node = start_node;
            double route_dist = 0.0;
            int pax_aboard = 0;
            
            // Track pickup times for per-employee ride time (Phase 3A)
            int pickup_times_arr[64];
            int pt_idx = 0;
            
            for (int e : route) {
                double d = dist_matrix[curr_node][emps[e].node_idx];
                route_dist += d;
                int travel = (int)std::round((d / pv.speed_kmph) * 60.0);
                int arrival = curr_time + travel;
                int wait = std::max(0, emps[e].earliest_pickup - arrival);
                if (pax_aboard > 0 && wait > 0) total_wait_pax += wait * pax_aboard;
                curr_time = std::max(arrival, emps[e].earliest_pickup);
                pickup_times_arr[pt_idx++] = curr_time;
                curr_node = emps[e].node_idx;
                pax_aboard++;
            }
            
            double d_off = dist_matrix[curr_node][OFFICE_NODE];
            route_dist += d_off;
            int travel_off = (int)std::round((d_off / pv.speed_kmph) * 60.0);
            int office_arrival = curr_time + travel_off;
            
            total_dist_cost += route_dist * pv.cost_per_km;
            total_time += (office_arrival - next_avail);
            
            int sz = (int)route.size();
            for (int idx = 0; idx < sz; idx++) {
                int e = route[idx];
                double pw = g_config.get_priority_weight(emps[e].priority);
                
                if (office_arrival > emps[e].latest_arrival_deadline) {
                    hard_time++;
                    int lat = office_arrival - emps[e].latest_arrival_deadline;
                    total_lateness += lat;
                    priority_weighted_lateness += lat * pw;
                }
                
                // Max ride time constraint (Phase 3A)
                int ride_time = office_arrival - pickup_times_arr[idx];
                int max_rt = g_config.get_max_ride_time(emps[e].priority);
                if (ride_time > max_rt) {
                    ride_time_violations++;
                    excess_ride_time += ride_time - max_rt;
                }
                priority_weighted_ride += ride_time * pw;
                
                if (emps[e].sharing_pref < sz) pref_v++;
                if (emps[e].vehicle_pref == 1 && pv.category != 1) pref_v++;
                if (emps[e].vehicle_pref == 2 && pv.category == 1) pref_v++;
                // Vehicle mode compatibility (Phase 3B)
                if (!is_mode_compatible(emps[e].priority, emps[e].vehicle_pref, pv.vehicle_mode))
                    pref_v++;
            }
            
            next_avail = office_arrival;
        }
    }
    
    double score = 0.0;
    int unassigned = total_employees - assigned;
    score += unassigned * g_config.unassigned_penalty;
    score += hard_time * g_config.time_violation_penalty;
    score += total_lateness * g_config.lateness_per_min_penalty;
    score += priority_weighted_lateness * g_config.priority_lateness_multiplier;
    score += pref_v * g_config.pref_violation_penalty;
    score += meta.cost_weight * total_dist_cost + meta.time_weight * total_time;
    score += vehicles_used * g_config.vehicle_activation_cost;
    // Waiting-with-passengers penalty theta (Parragh 2011)
    score += total_wait_pax * g_config.wait_with_pax_penalty;
    // Max ride time violations (Phase 3A)
    score += ride_time_violations * g_config.ride_time_violation_penalty;
    score += excess_ride_time * g_config.lateness_per_min_penalty * 0.5;
    // Priority-weighted ride time (Phase 3D)
    score += priority_weighted_ride * g_config.priority_ride_time_weight;
    
    return {score, hard_time + pref_v, total_lateness, total_dist_cost, pref_v};
}

// ============================================================================
// BASELINE COST CALCULATOR
// Computes what it would cost if every employee took individual cab rides
// Required by the problem statement for "net cost savings"
// ============================================================================
struct BaselineCost {
    double ola_total;
    double uber_total;
    double rapido_total;
    double average_total;
    double optimized_cost;
    double savings_percent;
    double savings_absolute;
};

inline BaselineCost compute_baseline_cost(
    const std::vector<Employee>& emps,
    double optimized_cost)
{
    // Indian ride-hailing market rates (approximate per km)
    const double OLA_BASE = 50.0;    // base fare
    const double OLA_PER_KM = 14.0;
    const double UBER_BASE = 45.0;
    const double UBER_PER_KM = 13.0;
    const double RAPIDO_BASE = 25.0; // auto
    const double RAPIDO_PER_KM = 8.0;
    
    double ola_total = 0, uber_total = 0, rapido_total = 0;
    
    for (const auto& emp : emps) {
        double direct_dist = dist_matrix[emp.node_idx][OFFICE_NODE];
        
        ola_total += std::max(OLA_BASE, OLA_BASE + direct_dist * OLA_PER_KM);
        uber_total += std::max(UBER_BASE, UBER_BASE + direct_dist * UBER_PER_KM);
        rapido_total += std::max(RAPIDO_BASE, RAPIDO_BASE + direct_dist * RAPIDO_PER_KM);
    }
    
    double avg = (ola_total + uber_total + rapido_total) / 3.0;
    double savings_pct = (avg > 0) ? ((avg - optimized_cost) / avg * 100.0) : 0.0;
    
    return {ola_total, uber_total, rapido_total, avg, optimized_cost, savings_pct, avg - optimized_cost};
}

#endif
