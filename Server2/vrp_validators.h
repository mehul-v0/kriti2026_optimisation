#ifndef VRP_VALIDATORS_H
#define VRP_VALIDATORS_H

#include "vrp_types.h"
#include <cmath>
#include <algorithm>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

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
            // 0=any, 1=premium, 2=normal
            if (emps[e].vehicle_pref == 1 && veh.category != 1) return false; // Premium required but not premium
            if (emps[e].vehicle_pref == 2 && veh.category == 1) return false; // Normal required but premium given
        }
    }
    return true;
}

inline bool is_time_window_valid(const std::vector<int>& route, int pos, int emp_idx,
                                  const Vehicle& veh, const std::vector<Employee>& emps, const Metadata& meta) {
    std::vector<int> temp = route;
    temp.insert(temp.begin() + pos, emp_idx);
    
    int curr_time = veh.available_from;
    int curr_node = veh.start_node;
    
    // For later trips (starting from office), we need to be careful about timing
    // But we should allow some flexibility - Trip 1 might finish early
    // Allow employees whose earliest_pickup is within 30 min before stagger
    if (veh.start_node == OFFICE_NODE) {
        for (int e : temp) {
            // Allow some buffer (30 min) before the stagger time
            // This accounts for Trip 1 potentially finishing early
            if (emps[e].earliest_pickup < veh.available_from - 30) {
                return false;  // Too early - would likely miss deadline
            }
        }
    }
    
    for (int e : temp) {
        double dist = dist_matrix[curr_node][emps[e].node_idx];
        int travel = std::round((dist / veh.speed_kmph) * 60.0);
        int arrival = curr_time + travel;
        int depart = std::max(arrival, emps[e].earliest_pickup);
        // latest_arrival_deadline already includes priority max delay
        if (depart > emps[e].latest_arrival_deadline) return false;
        curr_time = depart;
        curr_node = emps[e].node_idx;
    }
    
    double dist_off = dist_matrix[curr_node][OFFICE_NODE];
    int travel_off = std::round((dist_off / veh.speed_kmph) * 60.0);
    int final_arr = curr_time + travel_off;
    
    for (int e : temp) {
        // latest_arrival_deadline already includes priority max delay
        if (final_arr > emps[e].latest_arrival_deadline) return false;
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
                                 bool enforce_soft, const Metadata& meta) {
    if (route.empty()) return true;
    if ((int)route.size() > veh.capacity) { hard_v++; return false; }
    
    if (enforce_soft) {
        // Check sharing preference
        int max_allowed = veh.capacity;
        for (int e : route) if (emps[e].sharing_pref < max_allowed) max_allowed = emps[e].sharing_pref;
        if ((int)route.size() > max_allowed) {
            soft_v++;
            return false;  // Reject moves that violate sharing preference in Stage 1
        }
        
        // Check vehicle preference violations
        for (int e : route) {
            // 0=any, 1=premium, 2=normal
            if (emps[e].vehicle_pref == 1 && veh.category != 1) {
                soft_v++;
                return false;  // Reject moves that violate vehicle preference in Stage 1
            }
            if (emps[e].vehicle_pref == 2 && veh.category == 1) {
                soft_v++;
                return false;  // Reject moves that violate vehicle preference in Stage 1
            }
        }
    }
    
    int curr_time = veh.available_from;
    int curr_node = veh.start_node;
    
    for (int e : route) {
        double dist = dist_matrix[curr_node][emps[e].node_idx];
        int travel = std::round((dist / veh.speed_kmph) * 60.0);
        int arrival = curr_time + travel;
        int depart = std::max(arrival, emps[e].earliest_pickup);
        curr_time = depart;
        curr_node = emps[e].node_idx;
    }
    
    double dist_off = dist_matrix[curr_node][OFFICE_NODE];
    int travel_off = std::round((dist_off / veh.speed_kmph) * 60.0);
    int final_arr = curr_time + travel_off;
    
    for (int e : route) {
        // latest_arrival_deadline already includes priority max delay
        if (final_arr > emps[e].latest_arrival_deadline) {
            hard_v++;
            return false;
        }
    }
    return true;
}

#endif
