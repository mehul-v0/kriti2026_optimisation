#ifndef VRP_OUTPUT_H
#define VRP_OUTPUT_H

#include "vrp_types.h"
#include "vrp_utils.h"
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
                          bool enforce_soft, const std::string& sol_type) {
        
        Solution sol;
        sol.solution_type = sol_type;
        sol.total_cost = sol.total_time = sol.score = 0;
        sol.hard_violations = sol.soft_violations = 0;
        
        std::vector<std::vector<Trip>> phys_trips(phys_vehs.size());
        
        for (size_t v = 0; v < routes.size(); v++) {
            if (routes[v].empty()) continue;
            
            const auto& vv = virt_vehs[v];
            int phys_id = vv.physical_id;
            int trip_num = (v % 3) + 1;
            
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
                int travel = std::round((dist/vv.speed_kmph)*60.0);
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
            int travel_off = std::round((dist_off/vv.speed_kmph)*60.0);
            int off_arr = curr_time + travel_off;
            
            Stop off = {OFFICE_NODE, -1, "Office (Drop-off)", off_arr, off_arr, 0, dist_off};
            trip.stops.push_back(off);
            trip.total_distance += dist_off;
            
            for (int e : routes[v])
                if (off_arr > emps[e].latest_arrival_deadline) sol.hard_violations++;
            
            trip.total_cost = trip.total_distance * vv.cost_per_km;
            trip.total_time = off_arr - vv.available_from;
            
            sol.total_cost += trip.total_cost;
            sol.total_time += trip.total_time;
            
            phys_trips[phys_id].push_back(trip);
        }
        
        for (size_t p = 0; p < phys_vehs.size(); p++) {
            if (phys_trips[p].empty()) continue;
            
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
        
        if (enforce_soft) {
            for (const auto& route : routes) {
                if (route.empty()) continue;
                int sz = route.size();
                for (int e : route)
                    if (emps[e].sharing_pref < sz) sol.soft_violations++;
            }
        }
        
        sol.score = meta.cost_weight * sol.total_cost + meta.time_weight * sol.total_time;
        return sol;
    }
    
    static json to_json(const Solution& sol) {
        json out;
        out["solution_type"] = sol.solution_type;
        out["total_cost"] = sol.total_cost;
        out["total_time"] = sol.total_time;
        out["score"] = sol.score;
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
