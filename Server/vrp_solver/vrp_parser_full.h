/*
 * VRP Parser - Full Constraints Version
 * 
 * Parses input file with ALL constraints matching solver_ortools_full.py
 * 
 * Input format:
 * Line 1: cost_weight time_weight priority1_delay priority2_delay priority3_delay priority4_delay priority5_delay
 * Line 2: office_lat office_lon
 * Line 3: num_employees
 * Next num_employees lines: id pickup_lat pickup_lng drop_lat drop_lng earliest_pickup latest_drop priority vehicle_pref sharing_pref service_time
 * Next line: num_vehicles
 * Next num_vehicles lines: id capacity speed cost_per_km start_lat start_lng available_from category
 */

#ifndef VRP_PARSER_FULL_H
#define VRP_PARSER_FULL_H

#include "vrp_types_full.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <iomanip>

namespace vrp {

// ============================================================================
// UTILITIES
// ============================================================================

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

inline std::vector<std::string> split(const std::string& s, char delimiter = ' ') {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        std::string t = trim(token);
        if (!t.empty()) tokens.push_back(t);
    }
    return tokens;
}

inline int parseInt(const std::string& s, int default_val = 0) {
    if (s.empty()) return default_val;
    try {
        return std::stoi(s);
    } catch (...) {
        return default_val;
    }
}

inline double parseDouble(const std::string& s, double default_val = 0.0) {
    if (s.empty()) return default_val;
    try {
        return std::stod(s);
    } catch (...) {
        return default_val;
    }
}

// ============================================================================
// DISTANCE CALCULATION
// ============================================================================

inline double haversine(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = lat1 * PI / 180.0;
    double phi2 = lat2 * PI / 180.0;
    double dphi = (lat2 - lat1) * PI / 180.0;
    double dlambda = (lon2 - lon1) * PI / 180.0;
    
    double a = std::sin(dphi/2) * std::sin(dphi/2) +
               std::cos(phi1) * std::cos(phi2) *
               std::sin(dlambda/2) * std::sin(dlambda/2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
    
    return EARTH_RADIUS_KM * c;
}

// ============================================================================
// PROBLEM PARSING
// ============================================================================

inline ProblemInstance parseInputFile(const std::string& filename) {
    ProblemInstance problem;
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open input file: " + filename);
    }
    
    std::string line;
    
    // Line 1: Metadata (cost_weight time_weight priority_delays)
    std::getline(file, line);
    auto parts = split(line);
    if (parts.size() < 7) {
        throw std::runtime_error("Invalid metadata line (expected 7 values)");
    }
    
    problem.config.cost_weight = parseDouble(parts[0], 0.6);
    problem.config.time_weight = parseDouble(parts[1], 0.4);
    problem.config.priority_max_delays.clear();
    for (int i = 2; i < 7 && i < (int)parts.size(); i++) {
        problem.config.priority_max_delays.push_back(parseInt(parts[i], 15));
    }
    
    // Line 2: Office location
    std::getline(file, line);
    parts = split(line);
    if (parts.size() < 2) {
        throw std::runtime_error("Invalid office location line");
    }
    problem.config.office_lat = parseDouble(parts[0]);
    problem.config.office_lon = parseDouble(parts[1]);
    
    // Line 3: Number of employees
    std::getline(file, line);
    int num_employees = parseInt(trim(line));
    
    // Employee lines
    for (int i = 0; i < num_employees; i++) {
        std::getline(file, line);
        parts = split(line);
        if (parts.size() < 11) {
            std::cerr << "Warning: Employee line " << i+1 << " incomplete (expected 11 fields, got " 
                      << parts.size() << ")" << std::endl;
            continue;
        }
        
        Employee emp;
        emp.id = parts[0];
        emp.pickup_lat = parseDouble(parts[1]);
        emp.pickup_lon = parseDouble(parts[2]);
        emp.drop_lat = parseDouble(parts[3]);
        emp.drop_lon = parseDouble(parts[4]);
        emp.earliest_pickup = parseInt(parts[5], 480);  // 8:00 default
        emp.latest_drop = parseInt(parts[6], 1080);     // 18:00 default
        emp.priority = parseInt(parts[7], 3);
        emp.vehicle_pref = parseInt(parts[8], 0);
        emp.sharing_pref = parseInt(parts[9], 3);
        emp.service_time = parseInt(parts[10], 2);
        
        // Compute direct distance to office
        emp.dist_pickup_to_office = haversine(
            emp.pickup_lat, emp.pickup_lon,
            problem.config.office_lat, problem.config.office_lon
        );
        
        problem.employees.push_back(emp);
    }
    
    // Number of vehicles
    std::getline(file, line);
    int num_vehicles = parseInt(trim(line));
    
    // Vehicle lines
    for (int i = 0; i < num_vehicles; i++) {
        std::getline(file, line);
        parts = split(line);
        if (parts.size() < 8) {
            std::cerr << "Warning: Vehicle line " << i+1 << " incomplete" << std::endl;
            continue;
        }
        
        Vehicle veh;
        veh.id = parts[0];
        veh.capacity = parseInt(parts[1], 4);
        veh.speed_kmh = parseDouble(parts[2], 40.0);
        veh.cost_per_km = parseDouble(parts[3], 1.0);
        veh.start_lat = parseDouble(parts[4]);
        veh.start_lon = parseDouble(parts[5]);
        veh.available_from = parseInt(parts[6], 480);  // 8:00 default
        veh.category = parseInt(parts[7], 2);          // Default to normal
        
        problem.vehicles.push_back(veh);
    }
    
    file.close();
    
    // Compute distance matrix and incompatibilities
    problem.computeDistanceMatrix();
    problem.computeIncompatibilities();
    
    return problem;
}

// ============================================================================
// DISTANCE MATRIX COMPUTATION
// ============================================================================

inline void ProblemInstance::computeDistanceMatrix() {
    int n = employees.size();
    int v = vehicles.size();
    int total = 1 + n + v;  // office + employees + vehicle starts
    
    distance_matrix.resize(total, std::vector<double>(total, 0.0));
    
    // Build location list: [office, employees..., vehicles...]
    std::vector<std::pair<double, double>> locations;
    locations.push_back({config.office_lat, config.office_lon});
    
    for (const auto& emp : employees) {
        locations.push_back({emp.pickup_lat, emp.pickup_lon});
    }
    
    for (const auto& veh : vehicles) {
        locations.push_back({veh.start_lat, veh.start_lon});
    }
    
    // Compute all pairwise distances
    for (int i = 0; i < total; i++) {
        for (int j = 0; j < total; j++) {
            if (i != j) {
                distance_matrix[i][j] = haversine(
                    locations[i].first, locations[i].second,
                    locations[j].first, locations[j].second
                );
            }
        }
    }
}

// ============================================================================
// INCOMPATIBILITY COMPUTATION
// ============================================================================

inline void ProblemInstance::computeIncompatibilities() {
    incompatible_pairs.clear();
    
    int n = employees.size();
    double avg_speed = 0.0;
    for (const auto& v : vehicles) avg_speed += v.speed_kmh;
    avg_speed /= vehicles.size();
    
    for (int i = 0; i < n; i++) {
        const auto& emp_i = employees[i];
        
        // Calculate when employee i must leave their pickup to reach office on time
        // (considering priority-based delay allowance)
        int max_delay_i = (emp_i.priority >= 1 && emp_i.priority <= 5) 
                          ? config.priority_max_delays[emp_i.priority - 1] 
                          : 15;
        int adjusted_latest_i = emp_i.latest_drop + max_delay_i;
        double travel_time_i = (emp_i.dist_pickup_to_office / avg_speed) * 60;
        int latest_pickup_i = adjusted_latest_i - (int)travel_time_i;
        
        for (int j = i + 1; j < n; j++) {
            const auto& emp_j = employees[j];
            
            int max_delay_j = (emp_j.priority >= 1 && emp_j.priority <= 5) 
                              ? config.priority_max_delays[emp_j.priority - 1] 
                              : 15;
            int adjusted_latest_j = emp_j.latest_drop + max_delay_j;
            double travel_time_j = (emp_j.dist_pickup_to_office / avg_speed) * 60;
            int latest_pickup_j = adjusted_latest_j - (int)travel_time_j;
            
            // If i's latest pickup < j's earliest pickup, they're incompatible
            if (latest_pickup_i < emp_j.earliest_pickup) {
                incompatible_pairs.push_back({i, j});
            }
            // Check reverse
            else if (latest_pickup_j < emp_i.earliest_pickup) {
                incompatible_pairs.push_back({j, i});
            }
        }
    }
    
    if (!incompatible_pairs.empty()) {
        std::cout << "Found " << incompatible_pairs.size() 
                  << " incompatible employee pairs due to time windows" << std::endl;
    }
}

// ============================================================================
// TIME FORMATTING
// ============================================================================

inline std::string formatTime(int minutes) {
    int h = minutes / 60;
    int m = minutes % 60;
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return std::string(buf);
}

// ============================================================================
// SOLUTION OUTPUT (JSON)
// Matches format from solver_ortools_full.py exactly
// ============================================================================

inline std::string solutionToJson(const Solution& solution, const ProblemInstance& problem) {
    std::ostringstream json;
    
    // Build assignment map: vehicle_id -> [employee_ids]
    json << "{\n";
    json << "  \"assignment\": {\n";
    bool first_assignment = true;
    for (size_t v = 0; v < solution.vehicle_trips.size(); v++) {
        const auto& trips = solution.vehicle_trips[v];
        const auto& vehicle = problem.vehicles[v];
        
        std::vector<std::string> all_employees;
        for (const auto& trip : trips) {
            for (int emp_idx : trip.employee_indices) {
                all_employees.push_back(problem.employees[emp_idx].id);
            }
        }
        
        if (!first_assignment) json << ",\n";
        first_assignment = false;
        json << "    \"" << vehicle.id << "\": [";
        for (size_t e = 0; e < all_employees.size(); e++) {
            if (e > 0) json << ", ";
            json << "\"" << all_employees[e] << "\"";
        }
        json << "]";
    }
    json << "\n  },\n";
    
    // Score (matching Python formula)
    double baseline_cost = 500.0;
    double baseline_time = 100.0;
    double norm_cost = solution.total_cost / (baseline_cost + 1e-6);
    double norm_time = solution.total_time / (baseline_time + 1e-6);
    double score = problem.config.cost_weight * norm_cost + 
                   problem.config.time_weight * norm_time + 
                   solution.penalty / 10000.0;
    
    json << "  \"score\": " << std::fixed << std::setprecision(4) << score << ",\n";
    
    // Stats (matching Python format)
    json << "  \"stats\": {\n";
    json << "    \"cost\": " << std::fixed << std::setprecision(2) << solution.total_cost << ",\n";
    json << "    \"time\": " << std::fixed << std::setprecision(2) << solution.total_time << ",\n";
    json << "    \"penalty\": " << std::fixed << std::setprecision(2) << solution.penalty << ",\n";
    json << "    \"hard_violations\": " << solution.hard_violations << ",\n";
    json << "    \"soft_violations\": " << solution.soft_violations << "\n";
    json << "  },\n";
    
    // Details array (route information)
    json << "  \"details\": [\n";
    
    // Build details array matching solver_ortools_full.py format EXACTLY
    bool first_vehicle = true;
    for (size_t v = 0; v < solution.vehicle_trips.size(); v++) {
        const auto& trips = solution.vehicle_trips[v];
        const auto& vehicle = problem.vehicles[v];
        
        if (!first_vehicle) json << ",\n";
        first_vehicle = false;
        
        json << "    {\n";
        json << "      \"vehicle\": \"" << vehicle.id << "\",\n";
        json << "      \"vehicle_id\": \"" << vehicle.id << "\",\n";
        
        // Collect all employees across all trips
        json << "      \"employees\": [";
        bool first_emp = true;
        for (const auto& trip : trips) {
            for (int emp_idx : trip.employee_indices) {
                if (!first_emp) json << ", ";
                json << "\"" << problem.employees[emp_idx].id << "\"";
                first_emp = false;
            }
        }
        json << "],\n";
        
        json << "      \"num_trips\": " << trips.size() << ",\n";
        
        // Calculate total cost for this vehicle
        double total_vehicle_cost = 0.0;
        for (const auto& trip : trips) {
            total_vehicle_cost += trip.total_cost;
        }
        json << "      \"cost\": " << total_vehicle_cost << ",\n";
        
        json << "      \"trip_routes\": [\n";
        
        // Output each trip
        for (size_t t = 0; t < trips.size(); t++) {
            const auto& trip = trips[t];
            if (t > 0) json << ",\n";
            
            json << "        {\n";
            json << "          \"trip_number\": " << (t + 1) << ",\n";
            
            json << "          \"employees\": [";
            for (size_t e = 0; e < trip.employee_indices.size(); e++) {
                if (e > 0) json << ", ";
                json << "\"" << problem.employees[trip.employee_indices[e]].id << "\"";
            }
            json << "],\n";
            
            json << "          \"distance_km\": " << trip.total_distance << ",\n";
            json << "          \"cost\": " << trip.total_cost << ",\n";
            
            // Calculate office arrival time (last stop)
            int office_arrival_minutes = 0;
            if (!trip.stops.empty()) {
                office_arrival_minutes = trip.stops.back().arrival_time;
            }
            
            json << "          \"detailed_stops\": [\n";
            
            double cumulative_distance = 0.0;
            for (size_t s = 0; s < trip.stops.size(); s++) {
                const auto& stop = trip.stops[s];
                if (s > 0) json << ",\n";
                
                json << "            {\n";
                json << "              \"stop_number\": " << s << ",\n";
                json << "              \"label\": \"";
                
                if (stop.type == Stop::DEPOT_START) {
                    // Show "Office" for trips 2, 3, etc. that start from office
                    json << (trip.starts_from_office ? "Office" : "Depot");
                } else if (stop.type == Stop::PICKUP) {
                    json << problem.employees[stop.employee_idx].id;
                } else if (stop.type == Stop::OFFICE_DROP) {
                    json << "Office";
                }
                
                json << "\",\n";
                json << "              \"type\": \"";
                
                if (stop.type == Stop::DEPOT_START) {
                    // Use "office" type for trips that start from office
                    json << (trip.starts_from_office ? "office_start" : "depot");
                } else if (stop.type == Stop::PICKUP) {
                    json << "employee";
                } else if (stop.type == Stop::OFFICE_DROP) {
                    json << "office";
                }
                
                json << "\",\n";
                json << "              \"time\": \"" << formatTime(stop.arrival_time) << "\",\n";
                json << "              \"time_minutes\": " << stop.arrival_time << ",\n";
                json << "              \"distance_to_next\": " << stop.distance_from_prev << ",\n";
                json << "              \"cumulative_distance\": " << cumulative_distance;
                
                // Add employee-specific fields
                if (stop.type == Stop::PICKUP) {
                    const auto& emp = problem.employees[stop.employee_idx];
                    int max_delay = problem.config.priority_max_delays[emp.priority - 1];
                    int adjusted_latest = emp.latest_drop + max_delay;
                    
                    json << ",\n";
                    json << "              \"time_window\": \"[" << formatTime(emp.earliest_pickup) 
                         << " - " << formatTime(emp.latest_drop) << "]\",\n";
                    json << "              \"adjusted_window\": \"[" << formatTime(emp.earliest_pickup) 
                         << " - " << formatTime(adjusted_latest) << "]\",\n";
                    json << "              \"earliest_pickup\": \"" << formatTime(emp.earliest_pickup) << "\",\n";
                    json << "              \"latest_drop\": \"" << formatTime(emp.latest_drop) << "\",\n";
                    json << "              \"latest_drop_minutes\": " << emp.latest_drop << ",\n";
                    json << "              \"adjusted_latest_minutes\": " << adjusted_latest << ",\n";
                    json << "              \"priority\": " << emp.priority << ",\n";
                    json << "              \"max_delay\": " << max_delay << ",\n";
                    json << "              \"est_office_arrival\": \"" << formatTime(office_arrival_minutes) << "\",\n";
                    json << "              \"est_office_arrival_minutes\": " << office_arrival_minutes;
                }
                
                json << "\n            }";
                
                cumulative_distance += stop.distance_from_prev;
            }
            
            json << "\n          ]\n";
            json << "        }";
        }
        
        json << "\n      ]\n";
        json << "    }";
    }
    
json << "\n  ]\n";
    json << "}\n";
    
    return json.str();
}

} // namespace vrp

#endif // VRP_PARSER_FULL_H
