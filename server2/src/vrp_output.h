#ifndef VRP_OUTPUT_H
#define VRP_OUTPUT_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_simulation.h"
#include "json.hpp"
#include <sstream>
#include <numeric>
#include <climits>
#include <cmath>
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
        
        SimulationResult sim = simulate_full_solution(routes, virt_vehs, phys_vehs, emps, meta);
        
        Solution sol;
        sol.solution_type = sol_type;
        sol.total_cost = sim.total_cost;
        sol.total_time = sim.total_time;
        sol.hard_violations = sim.hard_violations;
        sol.soft_violations = sim.soft_violations;
        sol.score = sim.score;
        
        for (const auto& pvr : sim.vehicles) {
            VehicleSolution vs;
            vs.vehicle_id = pvr.vehicle_id;
            vs.physical_id = pvr.physical_id;
            vs.total_cost = pvr.total_cost;
            vs.total_distance = pvr.total_distance;
            vs.total_time = pvr.total_time;
            
            for (const auto& tr : pvr.trips) {
                Trip trip;
                trip.trip_number = tr.trip_number;
                trip.employee_indices = tr.employee_indices;
                trip.total_distance = tr.total_distance;
                trip.total_cost = tr.total_cost;
                trip.total_time = tr.total_time;
                
                for (const auto& sd : tr.stops) {
                    Stop s;
                    s.node = sd.node;
                    s.employee_idx = sd.employee_idx;
                    s.location_name = sd.location_name;
                    s.arrival_time = sd.arrival_time;
                    s.departure_time = sd.departure_time;
                    s.wait_time = sd.wait_time;
                    s.distance_from_prev = sd.distance_from_prev;
                    trip.stops.push_back(s);
                }
                vs.trips.push_back(trip);
            }
            sol.vehicles.push_back(vs);
        }
        
        for (const auto& er : sim.employee_results) {
            if (er.lateness > 0) {
                std::cerr << "LATE: " << emps[er.employee_idx].employee_id
                         << " arrives at " << er.dropoff_time
                         << " > deadline " << emps[er.employee_idx].latest_arrival_deadline
                         << " (late by " << er.lateness << " min)" << std::endl;
            }
        }
        
        return sol;
    }
    
    static json to_json(const Solution& sol) {
        // Backward-compatible: no employee details or sim_result
        return to_json_rich(sol, {}, nullptr);
    }
    
    static json to_json_rich(const Solution& sol, const std::vector<Employee>& emps,
                              const SimulationResult* sim_result) {
        json out;
        out["solution_type"] = sol.solution_type;
        out["score"] = sol.score;
        out["cost"] = sol.total_cost;
        out["total_time"] = sol.total_time;
        out["stats"] = {
            {"cost", sol.total_cost}, 
            {"time", sol.total_time},
            {"hard_violations", sol.hard_violations}, 
            {"soft_violations", sol.soft_violations}
        };
        
        json vehs_json = json::array();
        double total_distance = 0;
        for (const auto& vs : sol.vehicles) {
            json v;
            v["vehicle_id"] = vs.vehicle_id;
            v["total_cost"] = vs.total_cost;
            v["total_distance"] = vs.total_distance;
            v["total_time"] = vs.total_time;
            total_distance += vs.total_distance;
            
            json trips = json::array();
            for (const auto& t : vs.trips) {
                json tr;
                tr["trip_number"] = t.trip_number;
                tr["total_distance"] = t.total_distance;
                tr["total_cost"] = t.total_cost;
                tr["total_time"] = t.total_time;
                
                if (!emps.empty()) {
                    json emp_ids = json::array();
                    for (int e : t.employee_indices) {
                        if (e >= 0 && e < (int)emps.size())
                            emp_ids.push_back(emps[e].employee_id);
                    }
                    tr["employees"] = emp_ids;
                }
                
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
            vehs_json.push_back(v);
        }
        out["vehicles"] = vehs_json;
        
        // Per-employee assignments
        if (sim_result && !emps.empty()) {
            json emp_assignments = json::array();
            for (const auto& er : sim_result->employee_results) {
                if (er.employee_idx < 0 || er.employee_idx >= (int)emps.size()) continue;
                const auto& emp = emps[er.employee_idx];
                json ea;
                ea["employee_id"] = emp.employee_id;
                for (const auto& pvr : sim_result->vehicles) {
                    if (pvr.physical_id == er.vehicle_physical_id) {
                        ea["vehicle_id"] = pvr.vehicle_id;
                        break;
                    }
                }
                ea["trip_number"] = er.trip_number;
                ea["pickup_time"] = min_to_time(er.pickup_time);
                ea["dropoff_time"] = min_to_time(er.dropoff_time);
                ea["ride_time_minutes"] = er.ride_time;
                ea["delay_minutes"] = er.lateness;
                ea["priority"] = emp.priority;
                ea["pickup_lat"] = emp.pickup_lat;
                ea["pickup_lng"] = emp.pickup_lng;
                ea["drop_lat"] = emp.drop_lat;
                ea["drop_lng"] = emp.drop_lng;
                emp_assignments.push_back(ea);
            }
            out["employee_assignments"] = emp_assignments;
        }
        
        // Baseline cost comparison
        if (!emps.empty()) {
            BaselineCost bc = compute_baseline_cost(emps, sol.total_cost);
            out["baseline_comparison"] = {
                {"ola_estimate", bc.ola_total},
                {"uber_estimate", bc.uber_total},
                {"rapido_estimate", bc.rapido_total},
                {"average_market_cost", bc.average_total},
                {"optimized_cost", bc.optimized_cost},
                {"savings_absolute", bc.savings_absolute},
                {"savings_percent", bc.savings_percent}
            };
        }
        
        // Aggregate metrics
        json metrics;
        metrics["total_operational_cost"] = sol.total_cost;
        metrics["total_travel_time_minutes"] = sol.total_time;
        metrics["total_distance_km"] = total_distance;
        metrics["vehicles_used"] = (int)sol.vehicles.size();
        metrics["hard_violations"] = sol.hard_violations;
        metrics["soft_violations"] = sol.soft_violations;
        if (sim_result) {
            metrics["employees_served"] = (int)sim_result->employee_results.size();
            metrics["total_lateness_minutes"] = sim_result->total_lateness;
            metrics["total_wait_with_passengers_minutes"] = sim_result->total_wait_with_pax;
            int n = (int)sim_result->employee_results.size();
            if (n > 0) {
                double avg_ride = 0, avg_lateness = 0;
                for (const auto& er : sim_result->employee_results) {
                    avg_ride += er.ride_time;
                    avg_lateness += er.lateness;
                }
                metrics["avg_ride_time_minutes"] = avg_ride / n;
                metrics["avg_lateness_minutes"] = avg_lateness / n;
            }
        }
        out["aggregate_metrics"] = metrics;
        
        return out;
    }
};

#endif
