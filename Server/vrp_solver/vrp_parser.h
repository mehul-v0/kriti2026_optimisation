/*
 * VRP Input Parser
 * 
 * Parses the tab-separated input file format used by the VRP system.
 * 
 * Input format (Excel-style with tabs):
 * Line 1: num_employees num_vehicles
 * Next num_employees lines: employee data
 * Next num_vehicles lines: vehicle data
 * Last line: depot_lat depot_lon
 */

#ifndef VRP_PARSER_H
#define VRP_PARSER_H

#include "vrp_types.h"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <stdexcept>

namespace vrp {

// ============================================================================
// STRING UTILITIES
// ============================================================================

/**
 * Trim whitespace from string.
 */
inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/**
 * Split string by delimiter.
 */
inline std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

/**
 * Parse integer with default value.
 */
inline int parseInt(const std::string& s, int default_val = 0) {
    if (s.empty()) return default_val;
    try {
        return std::stoi(s);
    } catch (...) {
        return default_val;
    }
}

/**
 * Parse double with default value.
 */
inline double parseDouble(const std::string& s, double default_val = 0.0) {
    if (s.empty()) return default_val;
    try {
        return std::stod(s);
    } catch (...) {
        return default_val;
    }
}

// ============================================================================
// INPUT PARSING
// ============================================================================

/**
 * Parse problem instance from input file.
 * 
 * Actual format from cpp_input.txt:
 * Line 1: alpha beta time_multiplier distance_multiplier
 * Line 2: depot_lat depot_lon
 * Line 3: num_employees
 * Next num_employees lines: name lat lon earliest_minutes latest_minutes priority service_time cost
 * Line after employees: num_vehicles
 * Next num_vehicles lines: name capacity speed cost lat lon start_time
 */
inline ProblemInstance parseInputFile(const std::string& filename) {
    ProblemInstance problem;
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open input file: " + filename);
    }
    
    std::string line;
    
    // Line 1: alpha beta time_multiplier distance_multiplier
    if (!std::getline(file, line)) {
        throw std::runtime_error("Empty input file");
    }
    auto config = split(line, ' ');
    // Store config if needed (alpha, beta for objective weighting)
    double alpha = (config.size() >= 1) ? parseDouble(config[0], 0.6) : 0.6;
    double beta = (config.size() >= 2) ? parseDouble(config[1], 0.4) : 0.4;
    problem.config.alpha = alpha;
    problem.config.beta = beta;
    
    // Line 2: depot_lat depot_lon
    if (!std::getline(file, line)) {
        throw std::runtime_error("Missing depot location");
    }
    auto depot_parts = split(line, ' ');
    if (depot_parts.size() >= 2) {
        problem.depot_lat = parseDouble(depot_parts[0]);
        problem.depot_lon = parseDouble(depot_parts[1]);
    }
    
    // Line 3: num_employees
    if (!std::getline(file, line)) {
        throw std::runtime_error("Missing employee count");
    }
    int num_employees = parseInt(trim(line));
    
    // Parse employees
    // Format: name lat lon earliest_minutes latest_minutes priority service_time cost
    for (int i = 0; i < num_employees; i++) {
        if (!std::getline(file, line)) {
            throw std::runtime_error("Unexpected end of file while reading employees");
        }
        
        auto parts = split(line, ' ');
        
        Employee emp;
        emp.id = i;
        
        if (parts.size() >= 1) emp.name = parts[0];
        if (parts.size() >= 2) emp.pickup_lat = parseDouble(parts[1]);
        if (parts.size() >= 3) emp.pickup_lon = parseDouble(parts[2]);
        
        // Time windows are in minutes from midnight
        if (parts.size() >= 4) {
            int minutes = parseInt(parts[3]);
            emp.earliest_pickup = formatTime(minutes);
        } else {
            emp.earliest_pickup = "08:00";
        }
        
        if (parts.size() >= 5) {
            int minutes = parseInt(parts[4]);
            emp.latest_drop = formatTime(minutes);
        } else {
            emp.latest_drop = "10:00";
        }
        
        if (parts.size() >= 6) emp.priority = parseInt(parts[5], 1);
        
        // Column 7 might be baseline cost, column 8 is employee cost
        // Use a small fixed service time
        emp.service_time = 2.0;  // 2 minutes service time
        if (parts.size() >= 8) emp.cost = parseDouble(parts[7], 0.0);
        
        // Set defaults for other fields
        emp.vehicle_pref = 0;
        emp.sharing_pref = 3;
        emp.is_priority = (emp.priority >= 3);
        emp.gender = "O";
        
        problem.employees.push_back(emp);
    }
    
    // Line: num_vehicles
    if (!std::getline(file, line)) {
        throw std::runtime_error("Missing vehicle count");
    }
    int num_vehicles = parseInt(trim(line));
    
    // Parse vehicles
    // Format: name capacity speed cost lat lon start_time
    for (int i = 0; i < num_vehicles; i++) {
        if (!std::getline(file, line)) {
            throw std::runtime_error("Unexpected end of file while reading vehicles");
        }
        
        auto parts = split(line, ' ');
        
        Vehicle veh;
        veh.id = i;
        
        if (parts.size() >= 1) veh.name = parts[0];
        if (parts.size() >= 2) veh.capacity = parseInt(parts[1], 4);
        if (parts.size() >= 3) veh.speed_kmh = parseDouble(parts[2], 30.0);
        if (parts.size() >= 4) veh.cost_per_km = parseDouble(parts[3], 10.0);
        if (parts.size() >= 5) veh.start_lat = parseDouble(parts[4], problem.depot_lat);
        if (parts.size() >= 6) veh.start_lon = parseDouble(parts[5], problem.depot_lon);
        if (parts.size() >= 7) veh.start_time = parseInt(parts[6], 480);  // 8:00 AM default
        
        veh.category = 0;  // Default category
        
        problem.vehicles.push_back(veh);
    }
    
    file.close();
    
    // Compute distance matrix
    buildDistanceMatrix(problem);
    
    return problem;
}

/**
 * Alternative parser for JSON-like input.
 * (Reserved for future extension)
 */
inline ProblemInstance parseJsonInput(const std::string& json_str) {
    // TODO: Implement JSON parsing if needed
    throw std::runtime_error("JSON parsing not yet implemented");
}

// ============================================================================
// OUTPUT FORMATTING
// ============================================================================

/**
 * Format solution as JSON for Flask backend.
 */
inline std::string solutionToJson(
    const Solution& solution,
    const ProblemInstance& problem
) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"status\": \"success\",\n";
    json << "  \"total_cost\": " << solution.total_cost << ",\n";
    json << "  \"total_distance\": " << solution.total_distance << ",\n";
    json << "  \"vehicles_used\": " << solution.vehicles_used << ",\n";
    json << "  \"unassigned_count\": " << solution.unassigned.size() << ",\n";
    json << "  \"hard_violations\": " << solution.hard_violations << ",\n";
    json << "  \"soft_violations\": " << solution.soft_penalties << ",\n";
    
    // Routes
    json << "  \"routes\": [\n";
    bool first_route = true;
    
    for (int v = 0; v < (int)solution.routes.size(); v++) {
        const Route& route = solution.routes[v];
        if (route.empty()) continue;
        
        if (!first_route) json << ",\n";
        first_route = false;
        
        const Vehicle& veh = problem.vehicles[v];
        
        json << "    {\n";
        json << "      \"vehicle_id\": " << v << ",\n";
        json << "      \"vehicle_name\": \"" << veh.name << "\",\n";
        json << "      \"distance\": " << route.total_distance << ",\n";
        json << "      \"cost\": " << route.total_cost << ",\n";
        json << "      \"duration_minutes\": " << route.total_time << ",\n";
        json << "      \"employee_count\": " << route.employee_sequence.size() << ",\n";
        
        // Stops
        json << "      \"stops\": [\n";
        
        // Start at depot
        json << "        {\n";
        json << "          \"type\": \"depot_start\",\n";
        json << "          \"location\": [" << problem.depot_lat << ", " << problem.depot_lon << "],\n";
        json << "          \"time\": \"08:00\"\n";
        json << "        }";
        
        // Employee pickups
        for (int i = 0; i < (int)route.employee_sequence.size(); i++) {
            int emp_idx = route.employee_sequence[i];
            const Employee& emp = problem.employees[emp_idx];
            
            json << ",\n        {\n";
            json << "          \"type\": \"pickup\",\n";
            json << "          \"employee_id\": " << emp_idx << ",\n";
            json << "          \"employee_name\": \"" << emp.name << "\",\n";
            json << "          \"location\": [" << emp.pickup_lat << ", " << emp.pickup_lon << "],\n";
            if (i < (int)route.stops.size()) {
                json << "          \"arrival_time\": \"" << formatTime(route.stops[i].arrival_time) << "\",\n";
                json << "          \"earliest_time\": \"" << emp.earliest_pickup << "\",\n";
                json << "          \"latest_time\": \"" << emp.latest_drop << "\"\n";
            } else {
                json << "          \"earliest_time\": \"" << emp.earliest_pickup << "\",\n";
                json << "          \"latest_time\": \"" << emp.latest_drop << "\"\n";
            }
            json << "        }";
        }
        
        // End at depot
        json << ",\n        {\n";
        json << "          \"type\": \"depot_end\",\n";
        json << "          \"location\": [" << problem.depot_lat << ", " << problem.depot_lon << "],\n";
        json << "          \"arrival_time\": \"" << formatTime(route.total_time) << "\"\n";
        json << "        }\n";
        
        json << "      ]\n";
        json << "    }";
    }
    
    json << "\n  ],\n";
    
    // Unassigned employees
    json << "  \"unassigned\": [";
    for (int i = 0; i < (int)solution.unassigned.size(); i++) {
        if (i > 0) json << ", ";
        json << solution.unassigned[i];
    }
    json << "]\n";
    
    json << "}\n";
    
    return json.str();
}

/**
 * Print solution summary to console.
 */
inline void printSolution(
    const Solution& solution,
    const ProblemInstance& problem,
    bool detailed = false
) {
    std::cout << "\n=== VRP Solution ===" << std::endl;
    std::cout << "Total Cost: " << solution.total_cost << std::endl;
    std::cout << "Total Distance: " << solution.total_distance << " km" << std::endl;
    std::cout << "Vehicles Used: " << solution.vehicles_used << " / " << problem.vehicles.size() << std::endl;
    std::cout << "Unassigned: " << solution.unassigned.size() << std::endl;
    std::cout << "Hard Violations: " << solution.hard_violations << std::endl;
    std::cout << "Soft Penalties: " << solution.soft_penalties << std::endl;
    
    if (detailed) {
        std::cout << "\n--- Routes ---" << std::endl;
        
        for (int v = 0; v < (int)solution.routes.size(); v++) {
            const Route& route = solution.routes[v];
            if (route.empty()) continue;
            
            const Vehicle& veh = problem.vehicles[v];
            
            std::cout << "\nVehicle " << v << " (" << veh.name << "):" << std::endl;
            std::cout << "  Distance: " << route.total_distance << " km" << std::endl;
            std::cout << "  Cost: " << route.total_cost << std::endl;
            std::cout << "  Employees: " << route.employee_sequence.size() << std::endl;
            std::cout << "  Route: Depot";
            
            for (int emp_idx : route.employee_sequence) {
                std::cout << " -> " << problem.employees[emp_idx].name;
            }
            std::cout << " -> Depot" << std::endl;
        }
        
        if (!solution.unassigned.empty()) {
            std::cout << "\n--- Unassigned Employees ---" << std::endl;
            for (int emp_idx : solution.unassigned) {
                std::cout << "  " << problem.employees[emp_idx].name << std::endl;
            }
        }
    }
    
    std::cout << "==================\n" << std::endl;
}

} // namespace vrp

#endif // VRP_PARSER_H
