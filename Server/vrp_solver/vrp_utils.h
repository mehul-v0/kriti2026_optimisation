/*
 * VRP Distance and Utility Functions
 * 
 * Provides:
 * - Haversine distance calculation
 * - Time conversion utilities
 * - Distance matrix construction
 * - Route evaluation functions
 */

#ifndef VRP_UTILS_H
#define VRP_UTILS_H

#include "vrp_types.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>

namespace vrp {

// ============================================================================
// DISTANCE CALCULATIONS
// ============================================================================

/**
 * Calculate Haversine distance between two coordinates.
 * Returns distance in kilometers.
 */
inline double haversine(double lat1, double lon1, double lat2, double lon2) {
    double phi1 = lat1 * PI / 180.0;
    double phi2 = lat2 * PI / 180.0;
    double dphi = (lat2 - lat1) * PI / 180.0;
    double dlambda = (lon2 - lon1) * PI / 180.0;
    
    double a = std::sin(dphi / 2) * std::sin(dphi / 2) +
               std::cos(phi1) * std::cos(phi2) *
               std::sin(dlambda / 2) * std::sin(dlambda / 2);
    
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    
    return EARTH_RADIUS_KM * c;
}

/**
 * Build the complete distance matrix for the problem.
 * Uses a simple structure: distance from employee i to employee j
 * Also computes distances to/from depot
 */
inline void buildDistanceMatrix(ProblemInstance& problem) {
    int n = problem.num_employees();
    
    // Distance matrix is (n+1) x (n+1):
    // Index 0 = depot
    // Index 1..n = employees
    problem.distance_matrix.assign(n + 1, std::vector<double>(n + 1, 0.0));
    
    // Distances from depot to employees and back
    for (int i = 0; i < n; i++) {
        double d = haversine(
            problem.depot_lat, problem.depot_lon,
            problem.employees[i].pickup_lat, problem.employees[i].pickup_lon
        );
        problem.distance_matrix[0][i + 1] = d;
        problem.distance_matrix[i + 1][0] = d;
    }
    
    // Distances between employees
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j) {
                problem.distance_matrix[i + 1][j + 1] = haversine(
                    problem.employees[i].pickup_lat, problem.employees[i].pickup_lon,
                    problem.employees[j].pickup_lat, problem.employees[j].pickup_lon
                );
            }
        }
    }
    
    // Precompute direct distances to depot for employees
    for (int i = 0; i < n; i++) {
        problem.employees[i].direct_dist_to_office = problem.distance_matrix[i + 1][0];
    }
}

// ============================================================================
// TIME UTILITIES
// ============================================================================

/**
 * Parse time string "HH:MM" to minutes from midnight.
 */
inline int parseTime(const std::string& timeStr) {
    if (timeStr.empty()) return 0;
    
    int hours = 0, minutes = 0;
    char colon;
    std::istringstream iss(timeStr);
    iss >> hours >> colon >> minutes;
    
    return hours * 60 + minutes;
}

/**
 * Format minutes from midnight as "HH:MM" string.
 */
inline std::string formatTime(int minutes) {
    int h = minutes / 60;
    int m = minutes % 60;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":"
        << std::setfill('0') << std::setw(2) << m;
    
    return oss.str();
}

/**
 * Format minutes as string (also handles double).
 */
inline std::string formatTime(double minutes) {
    return formatTime(static_cast<int>(minutes));
}

// ============================================================================
// ROUTE EVALUATION
// ============================================================================

/**
 * Get distance between two nodes (0 = depot, 1..n = employees)
 */
inline double getDistance(const ProblemInstance& problem, int from, int to) {
    return problem.distance_matrix[from][to];
}

/**
 * Evaluate a single route - compute all metrics.
 * Updates:
 * - route.total_distance
 * - route.total_time  
 * - route.total_cost
 * - route.time_window_violations
 * - route.capacity_violation
 * - route.sharing_violations
 * - route.vehicle_pref_violations
 * - route.stops
 */
inline void evaluateRoute(Route& route, const ProblemInstance& problem) {
    if (route.employee_sequence.empty()) {
        route.total_distance = 0;
        route.total_time = 0;
        route.total_cost = 0;
        route.time_window_violations = 0;
        route.capacity_violation = 0;
        route.sharing_violations = 0;
        route.vehicle_pref_violations = 0;
        route.stops.clear();
        return;
    }
    
    const Vehicle& vehicle = problem.vehicles[route.vehicle_index];
    double speed = vehicle.speed_kmh;
    if (speed <= 0) speed = 30.0;  // Default speed
    
    // Reset metrics
    route.total_distance = 0;
    route.time_window_violations = 0;
    route.sharing_violations = 0;
    route.vehicle_pref_violations = 0;
    route.stops.clear();
    
    // Check capacity
    int num_passengers = (int)route.employee_sequence.size();
    route.capacity_violation = std::max(0, num_passengers - vehicle.capacity);
    
    // Start from vehicle's start time (or 8:00 AM = 480 min)
    double current_time = vehicle.start_time;
    if (current_time <= 0) current_time = 480;
    
    double cumulative_distance = 0;
    int current_node = 0;  // Start at depot
    
    // Visit each employee
    for (int i = 0; i < (int)route.employee_sequence.size(); i++) {
        int emp_idx = route.employee_sequence[i];
        int emp_node = emp_idx + 1;  // Offset for depot at index 0
        
        const Employee& emp = problem.employees[emp_idx];
        
        // Distance from current to employee
        double dist = getDistance(problem, current_node, emp_node);
        double travel_time = (dist / speed) * 60.0;  // Convert to minutes
        
        cumulative_distance += dist;
        current_time += travel_time;
        
        // Check time window (earliest pickup)
        int earliest = parseTime(emp.earliest_pickup);
        if (current_time < earliest) {
            // Wait until earliest pickup time
            current_time = earliest;
        }
        
        // Create stop record
        RouteStop stop;
        stop.node_type = 1;  // Pickup
        stop.employee_index = emp_idx;
        stop.arrival_time = current_time;
        stop.cumulative_distance = cumulative_distance;
        stop.time_window_satisfied = true;
        stop.slack = 0;
        route.stops.push_back(stop);
        
        // Add service time
        current_time += emp.service_time;
        if (current_time < earliest) current_time = earliest;
        
        // Check vehicle preference
        if (emp.vehicle_pref != 0 && vehicle.category != 0) {
            if (emp.vehicle_pref != vehicle.category) {
                route.vehicle_pref_violations++;
            }
        }
        
        current_node = emp_node;
    }
    
    // Return to depot
    double dist_to_depot = getDistance(problem, current_node, 0);
    double travel_time = (dist_to_depot / speed) * 60.0;
    cumulative_distance += dist_to_depot;
    current_time += travel_time;
    
    route.total_distance = cumulative_distance;
    route.total_time = current_time - vehicle.start_time;
    if (vehicle.start_time <= 0) route.total_time = current_time - 480;
    
    route.total_cost = cumulative_distance * vehicle.cost_per_km;
    
    // Check sharing preferences
    for (int emp_idx : route.employee_sequence) {
        const Employee& emp = problem.employees[emp_idx];
        int sharing = emp.sharing_pref;
        
        // sharing_pref: 1=single, 2=double, 3=any
        if (sharing == 1 && num_passengers > 1) {
            route.sharing_violations++;  // Wants single but sharing
        } else if (sharing == 2 && num_passengers > 2) {
            route.sharing_violations++;  // Wants max double but triple+
        }
    }
    
    // Check time window violations (latest drop)
    for (int emp_idx : route.employee_sequence) {
        const Employee& emp = problem.employees[emp_idx];
        int latest = parseTime(emp.latest_drop);
        
        // Allow some buffer based on priority
        int max_delay = 15;  // Default 15 min buffer
        auto it = problem.config.priority_max_delays.find(emp.priority);
        if (it != problem.config.priority_max_delays.end()) {
            max_delay = it->second;
        }
        
        if (current_time > latest + max_delay) {
            route.time_window_violations++;
        }
    }
}

/**
 * Evaluate complete solution.
 */
inline void evaluateSolution(Solution& solution, const ProblemInstance& problem) {
    solution.total_cost = 0;
    solution.total_distance = 0;
    solution.total_time = 0;
    solution.soft_penalties = 0;
    solution.hard_violations = 0;
    solution.vehicles_used = 0;
    
    // Evaluate each route
    for (int r = 0; r < (int)solution.routes.size(); r++) {
        solution.routes[r].vehicle_index = r;
        evaluateRoute(solution.routes[r], problem);
        
        Route& route = solution.routes[r];
        
        if (!route.empty()) {
            solution.vehicles_used++;
            solution.total_cost += route.total_cost;
            solution.total_distance += route.total_distance;
            solution.total_time = std::max(solution.total_time, route.total_time);
            
            // Accumulate violations
            solution.hard_violations += route.time_window_violations;
            solution.hard_violations += route.capacity_violation;
            
            // Soft penalties
            solution.soft_penalties += route.sharing_violations * PENALTY_SINGLE_PREF;
            solution.soft_penalties += route.vehicle_pref_violations * PENALTY_VEHICLE_PREF;
        }
    }
    
    // Add penalty for unassigned
    int total_assigned = 0;
    for (const Route& route : solution.routes) {
        total_assigned += (int)route.employee_sequence.size();
    }
    
    int unassigned = problem.num_employees() - total_assigned;
    solution.unassigned.clear();
    
    // Find which employees are unassigned
    std::set<int> assigned = solution.getAssignedEmployees();
    for (int i = 0; i < problem.num_employees(); i++) {
        if (assigned.find(i) == assigned.end()) {
            solution.unassigned.push_back(i);
        }
    }
    
    solution.hard_violations += unassigned;  // Unassigned is a hard violation
    
    // Calculate objective value
    // Use weighted combination + penalties
    double alpha = problem.config.alpha;
    double beta = problem.config.beta;
    
    solution.objective_value = 
        alpha * solution.total_cost +
        beta * solution.total_time +
        solution.soft_penalties +
        solution.hard_violations * PENALTY_HARD_VIOLATION;
}

// ============================================================================
// SOLUTION MANIPULATION
// ============================================================================

/**
 * Create an empty solution with routes for all vehicles.
 */
inline Solution createEmptySolution(const ProblemInstance& problem) {
    Solution solution;
    solution.routes.resize(problem.num_vehicles());
    
    for (int v = 0; v < problem.num_vehicles(); v++) {
        solution.routes[v].vehicle_index = v;
        solution.routes[v].employee_sequence.clear();
        solution.routes[v].total_distance = 0;
        solution.routes[v].total_time = 0;
        solution.routes[v].total_cost = 0;
        solution.routes[v].time_window_violations = 0;
        solution.routes[v].capacity_violation = 0;
        solution.routes[v].sharing_violations = 0;
        solution.routes[v].vehicle_pref_violations = 0;
    }
    
    solution.total_cost = 0;
    solution.total_distance = 0;
    solution.total_time = 0;
    solution.soft_penalties = 0;
    solution.objective_value = 0;
    solution.hard_violations = problem.num_employees();  // All unassigned
    solution.vehicles_used = 0;
    
    // All employees unassigned
    for (int i = 0; i < problem.num_employees(); i++) {
        solution.unassigned.push_back(i);
    }
    
    return solution;
}

/**
 * Insert employee at position in route.
 */
inline void insertEmployee(Route& route, int position, int employee_idx) {
    if (position < 0) position = 0;
    if (position > (int)route.employee_sequence.size()) {
        position = (int)route.employee_sequence.size();
    }
    route.employee_sequence.insert(
        route.employee_sequence.begin() + position,
        employee_idx
    );
}

/**
 * Remove employee at position from route.
 * Returns the removed employee index.
 */
inline int removeEmployee(Route& route, int position) {
    if (position < 0 || position >= (int)route.employee_sequence.size()) {
        return -1;
    }
    int emp_idx = route.employee_sequence[position];
    route.employee_sequence.erase(route.employee_sequence.begin() + position);
    return emp_idx;
}

/**
 * Create a deep copy of a solution.
 */
inline Solution copySolution(const Solution& src) {
    Solution dst;
    
    dst.routes.resize(src.routes.size());
    for (size_t r = 0; r < src.routes.size(); r++) {
        dst.routes[r].vehicle_index = src.routes[r].vehicle_index;
        dst.routes[r].employee_sequence = src.routes[r].employee_sequence;
        dst.routes[r].total_distance = src.routes[r].total_distance;
        dst.routes[r].total_time = src.routes[r].total_time;
        dst.routes[r].total_cost = src.routes[r].total_cost;
        dst.routes[r].time_window_violations = src.routes[r].time_window_violations;
        dst.routes[r].capacity_violation = src.routes[r].capacity_violation;
        dst.routes[r].sharing_violations = src.routes[r].sharing_violations;
        dst.routes[r].vehicle_pref_violations = src.routes[r].vehicle_pref_violations;
        dst.routes[r].stops = src.routes[r].stops;
    }
    
    dst.unassigned = src.unassigned;
    dst.total_cost = src.total_cost;
    dst.total_distance = src.total_distance;
    dst.total_time = src.total_time;
    dst.soft_penalties = src.soft_penalties;
    dst.objective_value = src.objective_value;
    dst.vehicles_used = src.vehicles_used;
    dst.hard_violations = src.hard_violations;
    dst.iteration_found = src.iteration_found;
    
    return dst;
}

} // namespace vrp

#endif // VRP_UTILS_H
