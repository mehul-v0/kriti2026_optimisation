#ifndef VRP_VALIDATORS_H
#define VRP_VALIDATORS_H

#include "vrp_types.h"
#include <cmath>
#include <algorithm>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

// ============================================================================
// SHARED ROUTE SIMULATION — Single source of truth for time/distance calculations
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
// SHARED VIOLATION COUNTING — Single source of truth
// Returns {hard_violations, pref_violations}
// ============================================================================

struct ViolationCount {
    int hard_time_violations;   // time window breaches
    int pref_violations;        // sharing + vehicle preference
    int total_lateness;         // sum of minutes late
};

inline ViolationCount count_route_violations(const std::vector<int>& route,
                                              const Vehicle& veh,
                                              const std::vector<Employee>& emps,
                                              int office_arrival) {
    ViolationCount vc = {0, 0, 0};
    if (route.empty()) return vc;
    
    int sz = (int)route.size();
    for (int e : route) {
        // Time window violations
        if (office_arrival > emps[e].latest_arrival_deadline) {
            vc.hard_time_violations++;
            vc.total_lateness += (office_arrival - emps[e].latest_arrival_deadline);
        }
        // Sharing preference violations
        if (emps[e].sharing_pref < sz) vc.pref_violations++;
        // Vehicle preference violations
        if (emps[e].vehicle_pref == 1 && veh.category != 1) vc.pref_violations++;
        if (emps[e].vehicle_pref == 2 && veh.category == 1) vc.pref_violations++;
    }
    return vc;
}

// ============================================================================
// SHARED SOLUTION COMPARISON — Single source of truth
// Returns true if sol_a is strictly better than sol_b
// Priority: fewer hard violations > fewer soft violations > lower cost
// ============================================================================

inline bool is_solution_better(int hard_a, int soft_a, double cost_a,
                                int hard_b, int soft_b, double cost_b) {
    if (hard_a < hard_b) return true;
    if (hard_a > hard_b) return false;
    if (soft_a < soft_b) return true;
    if (soft_a > soft_b) return false;
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
