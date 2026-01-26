#ifndef VRP_LOCAL_SEARCH_H
#define VRP_LOCAL_SEARCH_H

#include "vrp_types.h"
#include <algorithm>

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

class LocalSearchOps {
public:
    static MoveDelta relocate_delta(const std::vector<int>& route, int from, int to,
                                     int start, const std::vector<Employee>& emps, double speed) {
        if (from == to || from+1 == to || route.empty()) return {0,0};
        
        int node = emps[route[from]].node_idx;
        int prev_f = (from == 0) ? start : emps[route[from-1]].node_idx;
        int next_f = (from == (int)route.size()-1) ? OFFICE_NODE : emps[route[from+1]].node_idx;
        int prev_t = (to == 0) ? start : emps[route[to-1]].node_idx;
        int next_t = (to >= (int)route.size()) ? OFFICE_NODE : emps[route[to]].node_idx;
        
        double delta = -dist_matrix[prev_f][node] - dist_matrix[node][next_f] + dist_matrix[prev_f][next_f]
                       -dist_matrix[prev_t][next_t] + dist_matrix[prev_t][node] + dist_matrix[node][next_t];
        
        return {delta, (delta/speed)*60.0};
    }
    
    static MoveDelta exchange_delta(const std::vector<int>& route, int a, int b,
                                     int start, const std::vector<Employee>& emps, double speed) {
        if (a == b || route.size() < 2) return {0,0};
        if (a > b) std::swap(a, b);
        
        int na = emps[route[a]].node_idx;
        int nb = emps[route[b]].node_idx;
        double delta = 0;
        
        if (a+1 == b) {
            int prev = (a==0) ? start : emps[route[a-1]].node_idx;
            int next = (b==(int)route.size()-1) ? OFFICE_NODE : emps[route[b+1]].node_idx;
            delta = -dist_matrix[prev][na] - dist_matrix[na][nb] - dist_matrix[nb][next]
                    +dist_matrix[prev][nb] + dist_matrix[nb][na] + dist_matrix[na][next];
        } else {
            int prev_a = (a==0) ? start : emps[route[a-1]].node_idx;
            int next_a = emps[route[a+1]].node_idx;
            int prev_b = emps[route[b-1]].node_idx;
            int next_b = (b==(int)route.size()-1) ? OFFICE_NODE : emps[route[b+1]].node_idx;
            delta = -dist_matrix[prev_a][na] - dist_matrix[na][next_a]
                    -dist_matrix[prev_b][nb] - dist_matrix[nb][next_b]
                    +dist_matrix[prev_a][nb] + dist_matrix[nb][next_a]
                    +dist_matrix[prev_b][na] + dist_matrix[na][next_b];
        }
        
        return {delta, (delta/speed)*60.0};
    }
    
    static MoveDelta twoopt_delta(const std::vector<int>& route, int i, int j,
                                   int start, const std::vector<Employee>& emps, double speed) {
        if (i >= j || route.size() < 2) return {0,0};
        
        int ni = emps[route[i]].node_idx;
        int nj = emps[route[j]].node_idx;
        int prev = (i==0) ? start : emps[route[i-1]].node_idx;
        int next = (j==(int)route.size()-1) ? OFFICE_NODE : emps[route[j+1]].node_idx;
        
        double delta = -dist_matrix[prev][ni] - dist_matrix[nj][next]
                       +dist_matrix[prev][nj] + dist_matrix[ni][next];
        
        return {delta, (delta/speed)*60.0};
    }
    
    static void apply_relocate(std::vector<int>& route, int from, int to) {
        if (from == to || from+1 == to) return;
        int e = route[from];
        route.erase(route.begin() + from);
        int pos = (from < to) ? (to-1) : to;
        route.insert(route.begin() + pos, e);
    }
    
    static void apply_exchange(std::vector<int>& route, int a, int b) {
        if (a != b) std::swap(route[a], route[b]);
    }
    
    static void apply_2opt(std::vector<int>& route, int i, int j) {
        std::reverse(route.begin() + i, route.begin() + j + 1);
    }
};

#endif
