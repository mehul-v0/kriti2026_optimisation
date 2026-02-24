#ifndef VRP_VALIDATORS_H
#define VRP_VALIDATORS_H

#include "vrp_types.h"
#include "vrp_config.h"
#include <cmath>
#include <algorithm>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

// ============================================================================
// SHARED ROUTE SIMULATION --- Single source of truth for time/distance calculations
// Used by: validators, ALNS cost function, force_insert, lateness_aware_insert
// ============================================================================

struct RouteSimResult {
    double total_distance;
    int office_arrival;
    int curr_time_at_last_pickup;
};

inline RouteSimResult simulate_route(const std::vector<int>& route,
                                      const Vehicle& veh,
                                      const std::vector<Employee>& emps) {
    RouteSimResult result = {0.0, veh.available_from, veh.available_from};
    if (route.empty()) return result;
    
    int curr_time = veh.available_from;
    int curr_node = veh.start_node;
    
    for (int e : route) {
        double d = dist_matrix[curr_node][emps[e].node_idx];
        result.total_distance += d;
        int travel = (int)std::round((d / veh.speed_kmph) * 60.0);
        int arrival = curr_time + travel;
        curr_time = std::max(arrival, emps[e].earliest_pickup);
        curr_node = emps[e].node_idx;
    }
    result.curr_time_at_last_pickup = curr_time;
    
    double d_off = dist_matrix[curr_node][OFFICE_NODE];
    result.total_distance += d_off;
    int travel_off = (int)std::round((d_off / veh.speed_kmph) * 60.0);
    result.office_arrival = curr_time + travel_off;
    
    return result;
}

// ============================================================================
// SHARED VIOLATION COUNTING --- Single source of truth
// Returns {hard_violations, pref_violations}
// ============================================================================

struct ViolationCount {
    int hard_time_violations;       // time window breaches (HIGHEST PRIORITY)
    int pref_violations;            // sharing + vehicle preference (lower priority)
    int total_lateness;             // sum of minutes late (raw)
    double priority_weighted_lateness;  // lateness weighted by employee priority
    int max_lateness;               // worst single lateness (for minimax)
};

// Weighted violation score for local search move decisions
// Priority: hard violations >> pref violations >> total lateness
// Pref violations are important soft constraints that should be avoided
inline long long weighted_violation_score(const ViolationCount& vc) {
    // Hard violations: 10M weight (so 1 hard > any amount of lateness up to 10K min)
    // Pref violations: 100K weight (vehicle preference is important to respect)
    // Total lateness: 1000 weight per minute
    return (long long)vc.hard_time_violations * 10000000LL
         + (long long)vc.pref_violations * 100000LL
         + (long long)vc.total_lateness * 1000LL;
}

// ============================================================================
// MULTI-TRIP SIMULATION: Properly handles capacity constraints
// When route.size() > capacity, vehicle makes multiple trips
// Returns per-employee arrival times for accurate violation counting
// ============================================================================
struct MultiTripResult {
    double total_distance;
    std::vector<int> employee_arrivals;  // arrival time for each employee (indexed by position in route)
    int last_office_arrival;             // final office arrival time (for backward compat)
};

inline MultiTripResult simulate_route_multitrip(const std::vector<int>& route,
                                                 const Vehicle& veh,
                                                 const std::vector<Employee>& emps) {
    MultiTripResult result;
    result.total_distance = 0.0;
    result.employee_arrivals.resize(route.size(), veh.available_from);
    result.last_office_arrival = veh.available_from;
    
    if (route.empty()) return result;
    
    int cap = veh.capacity;
    int n = (int)route.size();
    int num_trips = (n + cap - 1) / cap;  // ceiling division
    
    int next_avail = veh.available_from;
    int emp_offset = 0;
    
    for (int t = 0; t < num_trips; t++) {
        int trip_start = emp_offset;
        int trip_end = std::min(emp_offset + cap, n);
        
        // First trip starts from depot, subsequent from office
        int start_node = (t == 0) ? veh.start_node : OFFICE_NODE;
        int curr_time = next_avail;
        int curr_node = start_node;
        
        for (int i = trip_start; i < trip_end; i++) {
            int e = route[i];
            double d = dist_matrix[curr_node][emps[e].node_idx];
            result.total_distance += d;
            int travel = (int)std::round((d / veh.speed_kmph) * 60.0);
            int arrival = curr_time + travel;
            curr_time = std::max(arrival, emps[e].earliest_pickup);
            curr_node = emps[e].node_idx;
        }
        
        // Return to office
        double d_off = dist_matrix[curr_node][OFFICE_NODE];
        result.total_distance += d_off;
        int travel_off = (int)std::round((d_off / veh.speed_kmph) * 60.0);
        int office_arrival = curr_time + travel_off;
        
        // All employees on this trip arrive at office_arrival
        for (int i = trip_start; i < trip_end; i++) {
            result.employee_arrivals[i] = office_arrival;
        }
        
        next_avail = office_arrival;  // Next trip starts after this one returns
        emp_offset = trip_end;
    }
    
    result.last_office_arrival = next_avail;
    return result;
}

// Count violations using MULTI-TRIP aware simulation
// This correctly handles vehicles that need multiple trips
inline ViolationCount count_route_violations_multitrip(const std::vector<int>& route,
                                                        const Vehicle& veh,
                                                        const std::vector<Employee>& emps) {
    ViolationCount vc = {0, 0, 0, 0.0, 0};
    if (route.empty()) return vc;
    
    MultiTripResult mtr = simulate_route_multitrip(route, veh, emps);
    int n = (int)route.size();
    int cap = veh.capacity;
    
    for (int i = 0; i < n; i++) {
        int e = route[i];
        int emp_arrival = mtr.employee_arrivals[i];
        
        // Time window violations
        if (emp_arrival > emps[e].latest_arrival_deadline) {
            vc.hard_time_violations++;
            int lateness = emp_arrival - emps[e].latest_arrival_deadline;
            vc.total_lateness += lateness;
            if (lateness > vc.max_lateness) vc.max_lateness = lateness;
            
            double priority_weight = 1.0;
            switch (emps[e].priority) {
                case 1: priority_weight = 10.0; break;
                case 2: priority_weight = 5.0; break;
                case 3: priority_weight = 3.0; break;
                case 4: priority_weight = 2.0; break;
                default: priority_weight = 1.0; break;
            }
            vc.priority_weighted_lateness += lateness * priority_weight;
        }
        
        // Sharing preference violations - use trip size for this employee
        int trip_idx = i / cap;
        int trip_start = trip_idx * cap;
        int trip_end = std::min(trip_start + cap, n);
        int trip_size = trip_end - trip_start;
        if (emps[e].sharing_pref < trip_size) vc.pref_violations++;
        
        // Vehicle preference violations
        if (emps[e].vehicle_pref == 1 && veh.category != 1) vc.pref_violations++;
        if (emps[e].vehicle_pref == 2 && veh.category == 1) vc.pref_violations++;
    }
    return vc;
}

// Legacy function: redirects to multi-trip aware version
inline ViolationCount count_route_violations(const std::vector<int>& route,
                                              const Vehicle& veh,
                                              const std::vector<Employee>& emps,
                                              int /*office_arrival_IGNORED*/) {
    // IMPORTANT: We ignore the passed office_arrival and use multi-trip simulation
    // This correctly handles vehicles that need multiple trips due to capacity constraints
    return count_route_violations_multitrip(route, veh, emps);
}

// ============================================================================
// SHARED SOLUTION COMPARISON --- Single source of truth
// Returns true if sol_a is strictly better than sol_b
// ============================================================================

// Returns true if sol_a is strictly better than sol_b
// Priority: fewer time violations > fewer soft violations > lower cost
inline bool is_solution_better(int hard_a, int soft_a, double cost_a,
                                int hard_b, int soft_b, double cost_b) {
    if (hard_a < hard_b) return true;
    if (hard_a > hard_b) return false;
    // At same hard violation count, prefer fewer soft violations
    if (soft_a < soft_b) return true;
    if (soft_a > soft_b) return false;
    return cost_a < cost_b;
}

// Extended comparison with lateness as explicit factor
inline bool is_solution_better_ext(int hard_a, int lateness_a, int soft_a, double cost_a,
                                    int hard_b, int lateness_b, int soft_b, double cost_b) {
    // 1st: fewer time window violations
    if (hard_a < hard_b) return true;
    if (hard_a > hard_b) return false;
    // 2nd: less total lateness (minimize delay)
    if (lateness_a < lateness_b) return true;
    if (lateness_a > lateness_b) return false;
    // 3rd: fewer pref violations
    if (soft_a < soft_b) return true;
    if (soft_a > soft_b) return false;
    // 4th: lower cost
    return cost_a < cost_b;
}

// Lateness-aware comparison: PRIORITY ORDER is:
//   1. Fewer hard violations (time window breaches)
//   2. Lower total lateness (minimize delay as much as possible)
//   3. Lower dollar cost (secondary to lateness)
inline bool is_solution_better_lateness(int hard_a, int lateness_a, double cost_a,
                                         int hard_b, int lateness_b, double cost_b) {
    if (hard_a < hard_b) return true;
    if (hard_a > hard_b) return false;
    // SAME violation count: prefer less total lateness
    if (lateness_a < lateness_b) return true;
    if (lateness_a > lateness_b) return false;
    // Same lateness: prefer lower cost
    return cost_a < cost_b;
}

// ============================================================================
// CAPACITY AND PREFERENCE VALIDATION
// ============================================================================

inline bool is_capacity_valid(const std::vector<int>& route, int pos, int emp_idx,
                               const Vehicle& veh, const std::vector<Employee>& emps,
                               bool enforce_soft) {
    std::vector<int> temp = route;
    temp.insert(temp.begin() + pos, emp_idx);
    
    if ((int)temp.size() > veh.capacity) return false;
                                
    if (enforce_soft) {
        // Check sharing preference
        int max_allowed = veh.capacity;
        for (int e : temp) if (emps[e].sharing_pref < max_allowed) max_allowed = emps[e].sharing_pref;
        if ((int)temp.size() > max_allowed) return false;
        
        // Check vehicle preference
        for (int e : temp) {
            if (emps[e].vehicle_pref == 1 && veh.category != 1) return false;
            if (emps[e].vehicle_pref == 2 && veh.category == 1) return false;
        }
    }
    return true;
}

inline bool is_time_window_valid(const std::vector<int>& route, int pos, int emp_idx,
                                  const Vehicle& veh, const std::vector<Employee>& emps, const Metadata& /*meta*/) {
    std::vector<int> temp = route;
    temp.insert(temp.begin() + pos, emp_idx);
    
    // For later trips (starting from office), check earliest pickup feasibility
    if (veh.start_node == OFFICE_NODE) {
        for (int e : temp) {
            if (emps[e].earliest_pickup < veh.available_from - 30) {
                return false;
            }
        }
    }
    
    // Use shared simulation
    RouteSimResult sim = simulate_route(temp, veh, emps);
    
    for (int e : temp) {
        if (sim.office_arrival > emps[e].latest_arrival_deadline) return false;
    }
    
    return true;
}

inline double calculate_delta_cost(const std::vector<int>& route, int pos, int emp_idx,
                                    int start_node, const std::vector<Employee>& emps) {
    int emp_node = emps[emp_idx].node_idx;
    
    if (route.empty())
        return dist_matrix[start_node][emp_node] + dist_matrix[emp_node][OFFICE_NODE];
    
    int prev = (pos == 0) ? start_node : emps[route[pos-1]].node_idx;
    int next = (pos >= (int)route.size()) ? OFFICE_NODE : emps[route[pos]].node_idx;
    
    return dist_matrix[prev][emp_node] + dist_matrix[emp_node][next] - dist_matrix[prev][next];
}

inline bool validate_full_route(const std::vector<int>& route, const Vehicle& veh,
                                 const std::vector<Employee>& emps, int& hard_v, int& soft_v,
                                 bool enforce_soft, const Metadata& /*meta*/,
                                 bool allow_hard_violations = false) {
    if (route.empty()) return true;
    if ((int)route.size() > veh.capacity) { hard_v++; return false; }
    
    if (enforce_soft) {
        // Check sharing preference
        int max_allowed = veh.capacity;
        for (int e : route) if (emps[e].sharing_pref < max_allowed) max_allowed = emps[e].sharing_pref;
        if ((int)route.size() > max_allowed) {
            soft_v++;
            return false;
        }
        
        // Check vehicle preference violations
        for (int e : route) {
            if (emps[e].vehicle_pref == 1 && veh.category != 1) {
                soft_v++;
                return false;
            }
            if (emps[e].vehicle_pref == 2 && veh.category == 1) {
                soft_v++;
                return false;
            }
        }
    }
    
    // Use shared simulation
    RouteSimResult sim = simulate_route(route, veh, emps);
    
    for (int e : route) {
        if (sim.office_arrival > emps[e].latest_arrival_deadline) {
            hard_v++;
            if (!allow_hard_violations) return false;
        }
    }
    return true;
}

#endif
