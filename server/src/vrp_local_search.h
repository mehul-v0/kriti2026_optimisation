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
    
    // ======================== OR-OPT MOVES ========================
    // Move a segment of `seg_len` consecutive employees from position `from` to position `to`.
    // Returns the distance delta (negative = improvement).
    static MoveDelta oropt_delta(const std::vector<int>& route, int from, int seg_len, int to,
                                  int start, const std::vector<Employee>& emps, double speed) {
        if (route.empty() || from == to || (int)route.size() < seg_len + 1) return {0, 0};
        int seg_end = from + seg_len - 1;
        if (seg_end >= (int)route.size()) return {0, 0};
        // `to` is the insertion position in the route *after* the segment is removed
        // Skip if target overlaps with segment
        if (to >= from && to <= seg_end + 1) return {0, 0};
        
        int seg_first = emps[route[from]].node_idx;
        int seg_last = emps[route[seg_end]].node_idx;
        
        // Cost of removing the segment
        int prev_seg = (from == 0) ? start : emps[route[from - 1]].node_idx;
        int after_seg = (seg_end == (int)route.size() - 1) ? OFFICE_NODE : emps[route[seg_end + 1]].node_idx;
        
        double remove_cost = -dist_matrix[prev_seg][seg_first] - dist_matrix[seg_last][after_seg]
                             + dist_matrix[prev_seg][after_seg];
        
        // Cost of inserting the segment at `to` (adjusted for removal)
        // After removal, indices shift. We need the neighbor nodes at the insertion point.
        // Build a conceptual route without the segment, find neighbors at position `to_adj`.
        int to_adj = (to > seg_end) ? (to - seg_len) : to;
        // The route without segment has size = route.size() - seg_len
        int new_sz = (int)route.size() - seg_len;
        
        // Neighbor before insertion point in the stripped route
        int prev_ins, after_ins;
        if (to_adj == 0) {
            prev_ins = start;
        } else {
            // Map to_adj-1 back to original index
            int orig_idx = (to_adj - 1 < from) ? (to_adj - 1) : (to_adj - 1 + seg_len);
            if (orig_idx < 0 || orig_idx >= (int)route.size()) return {0, 0};
            prev_ins = emps[route[orig_idx]].node_idx;
        }
        if (to_adj >= new_sz) {
            after_ins = OFFICE_NODE;
        } else {
            int orig_idx = (to_adj < from) ? to_adj : (to_adj + seg_len);
            if (orig_idx < 0 || orig_idx >= (int)route.size()) return {0, 0};
            after_ins = emps[route[orig_idx]].node_idx;
        }
        
        double insert_cost = dist_matrix[prev_ins][seg_first] + dist_matrix[seg_last][after_ins]
                             - dist_matrix[prev_ins][after_ins];
        
        double delta = remove_cost + insert_cost;
        return {delta, (delta / speed) * 60.0};
    }
    
    static void apply_oropt(std::vector<int>& route, int from, int seg_len, int to) {
        // Extract segment
        std::vector<int> segment(route.begin() + from, route.begin() + from + seg_len);
        route.erase(route.begin() + from, route.begin() + from + seg_len);
        // Adjust `to` for the removal
        int to_adj = (to > from + seg_len) ? (to - seg_len) : to;
        if (to_adj > from) to_adj = std::min(to_adj, (int)route.size());
        route.insert(route.begin() + to_adj, segment.begin(), segment.end());
    }
};

#endif
