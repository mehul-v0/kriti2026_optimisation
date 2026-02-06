#ifndef VRP_OUTPUT_H
#define VRP_OUTPUT_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_validators.h"
#include "json.hpp"
#include <sstream>

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
        
        std::vector<std::vector<Trip>> phys_trips(phys_vehs.size());
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            
            const auto& vv = virt_vehs[v];
            int phys_id = vv.physical_id;
            int trip_num = (v % 4) + 1;
            
            Trip trip;
            trip.trip_number = trip_num;
            trip.employee_indices = routes[v];
            trip.total_distance = trip.total_cost = 0;
            trip.total_time = 0;
            
            int curr_time = vv.available_from;
            int curr_node = vv.start_node;
            
            Stop start = {curr_node, -1, (trip_num==1)?"Vehicle Depot":"Office", curr_time, curr_time, 0, 0};
            trip.stops.push_back(start);
            
            for (int e : routes[v]) {
                const auto& emp = emps[e];
                double dist = dist_matrix[curr_node][emp.node_idx];
                int travel = (int)std::round((dist/vv.speed_kmph)*60.0);
                int arrival = curr_time + travel;
                int wait = std::max(0, emp.earliest_pickup - arrival);
                int depart = std::max(arrival, emp.earliest_pickup);
                
                Stop s = {emp.node_idx, e, emp.employee_id+" Pickup", arrival, depart, wait, dist};
                trip.stops.push_back(s);
                trip.total_distance += dist;
                
                curr_time = depart;
                curr_node = emp.node_idx;
            }
            
            double dist_off = dist_matrix[curr_node][OFFICE_NODE];
            int travel_off = (int)std::round((dist_off/vv.speed_kmph)*60.0);
            int off_arr = curr_time + travel_off;
            
            Stop off = {OFFICE_NODE, -1, "Office (Drop-off)", off_arr, off_arr, 0, dist_off};
            trip.stops.push_back(off);
            trip.total_distance += dist_off;
            
            // Don't check violations here - will check after trip sequencing
            
            trip.total_cost = trip.total_distance * vv.cost_per_km;
            trip.total_time = off_arr - vv.available_from;
            
            sol.total_cost += trip.total_cost;
            sol.total_time += trip.total_time;
            
            phys_trips[phys_id].push_back(trip);
        }
        
        for (size_t p = 0; p < phys_vehs.size(); p++) {
            if (phys_trips[p].empty()) continue;
            
            // Sort trips by trip number
            std::sort(phys_trips[p].begin(), phys_trips[p].end(), 
                     [](const Trip& a, const Trip& b) { return a.trip_number < b.trip_number; });
            
            // Properly sequence trips - each trip starts when previous trip ends
            int next_available_time = phys_vehs[p].available_from;
            for (auto& trip : phys_trips[p]) {
                // Calculate time offset needed to start this trip at correct time
                int original_start = trip.stops[0].departure_time;
                
                // Trip can start when previous trip ends OR at its original planned time
                // whichever is LATER (can't start earlier than planned - would violate time windows)
                int actual_start = std::max(next_available_time, original_start);
                int time_offset = actual_start - original_start;
                
                if (time_offset != 0) {
                    // Adjust all times in this trip
                    for (auto& stop : trip.stops) {
                        stop.arrival_time += time_offset;
                        stop.departure_time += time_offset;
                    }
                }
                
                // Recalculate trip duration based on adjusted times
                trip.total_time = trip.stops.back().arrival_time - trip.stops.front().departure_time;
                
                // Check for time window violations with adjusted times
                int office_arrival = trip.stops.back().arrival_time;
                for (int emp_idx : trip.employee_indices) {
                    const auto& emp = emps[emp_idx];
                    
                    // Check if pickup time is too early
                    for (size_t s = 1; s < trip.stops.size() - 1; s++) {
                        if (trip.stops[s].employee_idx == emp_idx) {
                            if (trip.stops[s].departure_time < emp.earliest_pickup) {
                                std::cerr << "HARD VIOLATION: " << emp.employee_id << " pickup at " 
                                         << trip.stops[s].departure_time << " < earliest " 
                                         << emp.earliest_pickup << std::endl;
                                sol.hard_violations++;
                            }
                            break;
                        }
                    }
                    
                    // Check if office arrival exceeds deadline (with priority buffer)
                    if (office_arrival > emp.latest_arrival_deadline) {
                        std::cerr << "HARD VIOLATION: " << emp.employee_id << " office arrival " 
                                 << office_arrival << " > deadline " 
                                 << emp.latest_arrival_deadline << std::endl;
                        sol.hard_violations++;
                    }
                }
                
                // Next trip can start when this trip ends (arrives at office)
                next_available_time = trip.stops.back().arrival_time;
            }
            
            VehicleSolution vs;
            vs.vehicle_id = phys_vehs[p].vehicle_id;
            vs.physical_id = p;
            vs.trips = phys_trips[p];
            vs.total_cost = vs.total_distance = vs.total_time = 0;
            
            for (const auto& t : phys_trips[p]) {
                vs.total_cost += t.total_cost;
                vs.total_distance += t.total_distance;
                vs.total_time += t.total_time;
            }
            
            sol.vehicles.push_back(vs);
        }
        
        // Count preference violations as HARD violations using shared utility
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            // Use the already-sequenced trip times for violation counting
            // Preference violations (sharing + vehicle) don't depend on timing
            const auto& vv = virt_vehs[v];
            int phys_id = vv.physical_id;
            const auto& pv = phys_vehs[phys_id];
            int sz = (int)routes[v].size();
            for (int e : routes[v]) {
                if (emps[e].sharing_pref < sz) sol.hard_violations++;
                if (emps[e].vehicle_pref == 1 && pv.category != 1) sol.hard_violations++;
                if (emps[e].vehicle_pref == 2 && pv.category == 1) sol.hard_violations++;
            }
        }
        
        sol.score = meta.cost_weight * sol.total_cost + meta.time_weight * sol.total_time;
        return sol;
    }
    
    static json to_json(const Solution& sol) {
        json out;
        out["solution_type"] = sol.solution_type;
        out["score"] = sol.total_cost;
        out["total_time"] = sol.total_time;
        out["stats"] = {{"cost", sol.score}, {"time", sol.total_time},
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
