#ifndef VRP_OUTPUT_H
#define VRP_OUTPUT_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_validators.h"
#include "json.hpp"
#include <sstream>
#include <numeric>
#include <climits>
#include <algorithm>

using json = nlohmann::json;

extern std::vector<std::vector<double>> dist_matrix;
extern int OFFICE_NODE;

class OutputFormatter {
public:
    static Solution format(const std::vector<std::vector<int>>& routes,
                          const std::vector<Vehicle>& virt_vehs, const std::vector<Vehicle>& phys_vehs,
                          const std::vector<Employee>& emps, const Metadata& meta,
                          bool /*enforce_soft*/, const std::string& sol_type) {
        
        Solution sol;
        sol.solution_type = sol_type;
        sol.total_cost = sol.total_time = sol.score = 0;
        sol.hard_violations = sol.soft_violations = 0;
        
        // Step 1: Collect employee lists per physical vehicle
        // Each element is a "trip" (list of employee indices)
        std::vector<std::vector<std::vector<int>>> phys_emp_lists(phys_vehs.size());
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            int phys_id = virt_vehs[v].physical_id;
            phys_emp_lists[phys_id].push_back(routes[v]);
        }
        
        // Step 2: For each physical vehicle, find optimal trip ordering and build output
        for (size_t p = 0; p < phys_vehs.size(); p++) {
            auto& emp_lists = phys_emp_lists[p];
            if (emp_lists.empty()) continue;
            
            int K = (int)emp_lists.size();
            const auto& pv = phys_vehs[p];
            
            // Lambda: build complete trip sequence for a given permutation
            // Returns {trips, time_violations, total_lateness, total_cost}
            // CRITICAL: Trip 0 starts from vehicle depot, Trip 1+ start from office
            auto build_trip_sequence = [&](const std::vector<int>& order) 
                -> std::tuple<std::vector<Trip>, int, int, double> {
                
                std::vector<Trip> trips;
                int time_violations = 0;
                int total_lateness = 0;
                double total_cost = 0;
                int next_avail = pv.available_from;
                
                for (int ti = 0; ti < K; ti++) {
                    const auto& emp_list = emp_lists[order[ti]];
                    Trip trip;
                    trip.trip_number = ti + 1;
                    trip.employee_indices = emp_list;
                    trip.total_distance = 0;
                    trip.total_cost = 0;
                    trip.total_time = 0;
                    
                    // First trip starts from vehicle depot, subsequent from office
                    int start_node = (ti == 0) ? pv.start_node : OFFICE_NODE;
                    int curr_time = next_avail;
                    int curr_node = start_node;
                    
                    Stop start_stop = {curr_node, -1, 
                        (ti == 0) ? "Vehicle Depot" : "Office",
                        curr_time, curr_time, 0, 0.0};
                    trip.stops.push_back(start_stop);
                    
                    for (int e : emp_list) {
                        const auto& emp = emps[e];
                        double dist = dist_matrix[curr_node][emp.node_idx];
                        int travel = (int)std::round((dist / pv.speed_kmph) * 60.0);
                        int arrival = curr_time + travel;
                        int wait = std::max(0, emp.earliest_pickup - arrival);
                        int depart = std::max(arrival, emp.earliest_pickup);
                        
                        Stop s = {emp.node_idx, e, emp.employee_id + " Pickup",
                                  arrival, depart, wait, dist};
                        trip.stops.push_back(s);
                        trip.total_distance += dist;
                        
                        curr_time = depart;
                        curr_node = emp.node_idx;
                    }
                    
                    // Return to office
                    double dist_off = dist_matrix[curr_node][OFFICE_NODE];
                    int travel_off = (int)std::round((dist_off / pv.speed_kmph) * 60.0);
                    int off_arr = curr_time + travel_off;
                    
                    Stop off_stop = {OFFICE_NODE, -1, "Office (Drop-off)",
                                     off_arr, off_arr, 0, dist_off};
                    trip.stops.push_back(off_stop);
                    trip.total_distance += dist_off;
                    
                    // Check deadline violations and accumulate lateness
                    for (int e : emp_list) {
                        if (off_arr > emps[e].latest_arrival_deadline) {
                            time_violations++;
                            total_lateness += (off_arr - emps[e].latest_arrival_deadline);
                        }
                    }
                    
                    trip.total_cost = trip.total_distance * pv.cost_per_km;
                    trip.total_time = off_arr - next_avail;
                    total_cost += trip.total_cost;
                    
                    next_avail = off_arr;
                    trips.push_back(trip);
                }
                
                return {trips, time_violations, total_lateness, total_cost};
            };
            
            // Find best permutation - prioritize: violations > lateness > cost
            std::vector<int> perm(K);
            std::iota(perm.begin(), perm.end(), 0);
            
            int best_violations = INT_MAX;
            int best_lateness = INT_MAX;
            double best_cost_local = 1e18;
            std::vector<int> best_perm = perm;
            
            if (K <= 7) {
                // Exact: try all K! permutations
                do {
                    auto [trips, viols, lateness, cost] = build_trip_sequence(perm);
                    bool better = (viols < best_violations) ||
                                  (viols == best_violations && lateness < best_lateness) ||
                                  (viols == best_violations && lateness == best_lateness && cost < best_cost_local);
                    if (better) {
                        best_violations = viols;
                        best_lateness = lateness;
                        best_cost_local = cost;
                        best_perm = perm;
                    }
                } while (std::next_permutation(perm.begin(), perm.end()));
            } else {
                // Heuristic: try short-trips-first and long-trips-first
                auto perm1 = perm;
                std::sort(perm1.begin(), perm1.end(), [&](int a, int b) {
                    double da = 0, db = 0;
                    for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                    for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                    return da < db;
                });
                auto [t1, v1, l1, c1] = build_trip_sequence(perm1);
                best_violations = v1; best_lateness = l1; best_cost_local = c1; best_perm = perm1;
                
                auto perm2 = perm;
                std::sort(perm2.begin(), perm2.end(), [&](int a, int b) {
                    double da = 0, db = 0;
                    for (int e : emp_lists[a]) da += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                    for (int e : emp_lists[b]) db += dist_matrix[emps[e].node_idx][OFFICE_NODE];
                    return da > db;
                });
                auto [t2, v2, l2, c2] = build_trip_sequence(perm2);
                bool better2 = (v2 < best_violations) ||
                               (v2 == best_violations && l2 < best_lateness) ||
                               (v2 == best_violations && l2 == best_lateness && c2 < best_cost_local);
                if (better2) {
                    best_violations = v2; best_lateness = l2; best_cost_local = c2; best_perm = perm2;
                }
            }
            
            // Build final trips with best permutation
            auto [final_trips, final_time_violations, final_lateness, final_cost] = build_trip_sequence(best_perm);
            
            // Log time violations
            for (const auto& trip : final_trips) {
                int off_arr = trip.stops.back().arrival_time;
                for (int e : trip.employee_indices) {
                    if (off_arr > emps[e].latest_arrival_deadline) {
                        std::cerr << "HARD VIOLATION: " << emps[e].employee_id 
                                 << " office arrival " << off_arr 
                                 << " > deadline " << emps[e].latest_arrival_deadline << std::endl;
                    }
                }
            }
            sol.hard_violations += final_time_violations;
            
            // Count preference violations (doesn't depend on ordering)
            for (int ti = 0; ti < K; ti++) {
                const auto& emp_list = emp_lists[best_perm[ti]];
                int sz = (int)emp_list.size();
                for (int e : emp_list) {
                    if (emps[e].sharing_pref < sz) sol.soft_violations++;
                    if (emps[e].vehicle_pref == 1 && pv.category != 1) sol.soft_violations++;
                    if (emps[e].vehicle_pref == 2 && pv.category == 1) sol.soft_violations++;
                }
            }
            
            // Build vehicle solution
            VehicleSolution vs;
            vs.vehicle_id = pv.vehicle_id;
            vs.physical_id = p;
            vs.trips = final_trips;
            vs.total_cost = vs.total_distance = vs.total_time = 0;
            for (const auto& t : final_trips) {
                vs.total_cost += t.total_cost;
                vs.total_distance += t.total_distance;
                vs.total_time += t.total_time;
            }
            
            sol.total_cost += vs.total_cost;
            sol.total_time += vs.total_time;
            sol.vehicles.push_back(vs);
        }
        
        sol.score = meta.cost_weight * sol.total_cost + meta.time_weight * sol.total_time;
        return sol;
    }
    
    static json to_json(const Solution& sol) {
        json out;
        out["solution_type"] = sol.solution_type;
        out["score"] = sol.score;
        out["cost"] = sol.total_cost;
        out["total_time"] = sol.total_time;
        out["stats"] = {{"cost", sol.total_cost}, {"time", sol.total_time},
                        {"hard_violations", sol.hard_violations}, {"soft_violations", sol.soft_violations}};
        
        json vehs = json::array();
        for (const auto& vs : sol.vehicles) {
            json v;
            v["vehicle_id"] = vs.vehicle_id;
            v["total_cost"] = vs.total_cost;
            v["total_distance"] = vs.total_distance;
            v["total_time"] = vs.total_time;
            
            json trips = json::array();
            for (const auto& t : vs.trips) {
                json tr;
                tr["trip_number"] = t.trip_number;
                tr["total_distance"] = t.total_distance;
                tr["total_cost"] = t.total_cost;
                tr["total_time"] = t.total_time;
                
                json stops = json::array();
                for (const auto& s : t.stops) {
                    json st;
                    st["location"] = s.location_name;
                    st["arrival_time"] = min_to_time(s.arrival_time);
                    st["departure_time"] = min_to_time(s.departure_time);
                    st["wait_time"] = s.wait_time;
                    st["distance_from_prev"] = s.distance_from_prev;
                    stops.push_back(st);
                }
                tr["stops"] = stops;
                trips.push_back(tr);
            }
            v["trips"] = trips;
            vehs.push_back(v);
        }
        out["vehicles"] = vehs;
        return out;
    }
};

#endif
