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
        int max_allowed = veh.capacity;
        for (int e : temp) if (emps[e].sharing_pref < max_allowed) max_allowed = emps[e].sharing_pref;
        if ((int)temp.size() > max_allowed) return false;
    }
    return true;
}

inline bool is_time_window_valid(const std::vector<int>& route, int pos, int emp_idx,
                                  const Vehicle& veh, const std::vector<Employee>& emps) {
    std::vector<int> temp = route;
    temp.insert(temp.begin() + pos, emp_idx);
    
    int curr_time = veh.available_from;
    int curr_node = veh.start_node;
    
    for (int e : temp) {
        double dist = dist_matrix[curr_node][emps[e].node_idx];
        int travel = std::round((dist / veh.speed_kmph) * 60.0);
        int arrival = curr_time + travel;
        int depart = std::max(arrival, emps[e].earliest_pickup);
        if (depart > emps[e].latest_arrival_deadline) return false;
        curr_time = depart;
        curr_node = emps[e].node_idx;
    }
    
    double dist_off = dist_matrix[curr_node][OFFICE_NODE];
    int travel_off = std::round((dist_off / veh.speed_kmph) * 60.0);
    int final_arr = curr_time + travel_off;
    
    for (int e : temp)
        if (final_arr > emps[e].latest_arrival_deadline) return false;
    
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
                                 bool enforce_soft) {
    if (route.empty()) return true;
    if ((int)route.size() > veh.capacity) { hard_v++; return false; }
    
    if (enforce_soft) {
        int max_allowed = veh.capacity;
        for (int e : route) if (emps[e].sharing_pref < max_allowed) max_allowed = emps[e].sharing_pref;
        if ((int)route.size() > max_allowed) soft_v++;
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
        if (final_arr > emps[e].latest_arrival_deadline) {
            hard_v++;
            return false;
        }
    }
    return true;
}

#endif
