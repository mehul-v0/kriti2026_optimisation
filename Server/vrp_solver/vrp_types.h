/*
 * VRP Types and Data Structures
 * 
 * This header defines all data structures used in the VRP solver.
 * Mirrors the data model from OR-Tools but in pure C++.
 */

#ifndef VRP_TYPES_H
#define VRP_TYPES_H

#include <vector>
#include <string>
#include <map>
#include <set>
#include <limits>
#include <cmath>

namespace vrp {

// ============================================================================
// CONSTANTS
// ============================================================================

constexpr double INF = std::numeric_limits<double>::infinity();
constexpr double EARTH_RADIUS_KM = 6371.0;
constexpr double PI = 3.14159265358979323846;

// Penalty weights for constraint violations
constexpr double PENALTY_HARD_VIOLATION = 1000000.0;  // Time window violation
constexpr double PENALTY_SINGLE_PREF = 5000.0;        // Single sharing preference violated
constexpr double PENALTY_DOUBLE_PREF = 2000.0;        // Double sharing preference violated
constexpr double PENALTY_TRIPLE_PREF = 1000.0;        // Triple sharing preference violated
constexpr double PENALTY_VEHICLE_PREF = 1000.0;       // Vehicle preference violated
constexpr double PENALTY_CAPACITY = 100000.0;         // Capacity exceeded

// ============================================================================
// EMPLOYEE DATA
// ============================================================================

struct Employee {
    int id;                       // Internal index (0-based)
    std::string name;             // Employee identifier
    
    // Location
    double pickup_lat;
    double pickup_lon;
    double drop_lat;              // Usually office location
    double drop_lon;
    
    // Time windows (as strings HH:MM for compatibility)
    std::string earliest_pickup;  // Cannot pick up before this time
    std::string latest_drop;      // Must arrive at office by this time
    
    // Preferences
    int priority;                 // 1 (lowest) to 3 (highest)
    int vehicle_pref;             // 0=any, 1=premium, 2=normal
    int sharing_pref;             // 1=single, 2=double, 3=any
    bool is_priority;             // High priority employee
    std::string gender;           // M/F/O
    
    // Additional from input format
    double service_time;          // Time to pick up (minutes)
    double cost;                  // Employee cost factor
    
    // Precomputed
    double direct_dist_to_office; // Direct distance from pickup to office
};

// ============================================================================
// VEHICLE DATA
// ============================================================================

struct Vehicle {
    int id;                       // Internal index (0-based)
    std::string name;             // Vehicle identifier
    
    // Starting location
    double start_lat;
    double start_lon;
    
    // Specifications
    int capacity;                 // Max passengers
    double cost_per_km;           // Cost in currency per km
    double speed_kmh;             // Average speed in km/h
    
    // Availability
    int start_time;               // Minutes from midnight when available
    
    // Category
    int category;                 // 0=any, 1=premium, 2=normal
};

// ============================================================================
// PROBLEM CONFIGURATION
// ============================================================================

struct ProblemConfig {
    // Objective weights (alpha for cost, beta for time)
    double alpha = 0.6;
    double beta = 0.4;
    
    // Time/distance multipliers
    double time_multiplier = 80;
    double distance_multiplier = 200;
    
    // Priority-based delay limits (in minutes)
    std::map<int, int> priority_max_delays = {
        {1, 30}, {2, 20}, {3, 10}
    };
    
    // Solver parameters
    int max_iterations = 10000;
    int time_limit_seconds = 30;
    double initial_temperature = 1000.0;
    double cooling_rate = 0.9995;
    
    // GLS parameters
    double gls_lambda = 0.1;      // Penalty weight for GLS
    int gls_penalty_period = 100; // Update penalties every N iterations
};

// ============================================================================
// SOLUTION REPRESENTATION
// ============================================================================

// A single stop in a route
struct RouteStop {
    int node_type;                // 0=depot, 1=employee_pickup, 2=office_drop
    int employee_index;           // -1 if not employee pickup
    
    double arrival_time;          // Minutes from midnight
    double departure_time;        // After any waiting
    double cumulative_distance;   // Distance from start of route
    
    // For validation
    bool time_window_satisfied;
    double slack;                 // Waiting time at this stop
};

// A single route (one trip of one vehicle)
struct Route {
    int vehicle_index;
    std::vector<int> employee_sequence;  // Indices of employees in pickup order
    
    // Computed metrics
    double total_distance;
    double total_time;            // Time from start to end
    double total_cost;
    
    // Detailed stops
    std::vector<RouteStop> stops;
    
    // Validation
    int capacity_violation;       // Number of passengers over capacity
    int time_window_violations;   // Number of late arrivals
    int sharing_violations;       // Sharing preference violations
    int vehicle_pref_violations;  // Vehicle preference violations
    
    // Helper methods
    bool empty() const { return employee_sequence.empty(); }
    int size() const { return (int)employee_sequence.size(); }
};

// Complete solution (all routes for all vehicles)
struct Solution {
    std::vector<Route> routes;    // One route per vehicle (can be empty)
    std::vector<int> unassigned;  // Employees not assigned to any route
    
    // Aggregate metrics
    double total_cost;
    double total_distance;
    double total_time;
    double soft_penalties;
    double objective_value;       // Combined weighted objective
    
    // Vehicle count
    int vehicles_used;
    
    // Violation counts
    int hard_violations;          // Time window violations (must be 0 for feasible)
    
    // For tracking
    int iteration_found;
    
    // Helper to get all assigned employees
    std::set<int> getAssignedEmployees() const {
        std::set<int> assigned;
        for (const auto& route : routes) {
            for (int e : route.employee_sequence) {
                assigned.insert(e);
            }
        }
        return assigned;
    }
    
    // Check if better than another solution
    bool isBetterThan(const Solution& other) const {
        // First priority: fewer hard violations
        if (hard_violations != other.hard_violations) {
            return hard_violations < other.hard_violations;
        }
        // Second priority: lower objective value
        return objective_value < other.objective_value;
    }
};

// ============================================================================
// PROBLEM INSTANCE
// ============================================================================

struct ProblemInstance {
    std::vector<Employee> employees;
    std::vector<Vehicle> vehicles;
    ProblemConfig config;
    
    // Depot/office location
    double depot_lat;
    double depot_lon;
    
    // Precomputed distance matrix
    // Index mapping: 0 = depot, 1..N = employees
    std::vector<std::vector<double>> distance_matrix;
    
    // Quick lookups
    int num_employees() const { return (int)employees.size(); }
    int num_vehicles() const { return (int)vehicles.size(); }
};

// ============================================================================
// MOVE TYPES FOR LOCAL SEARCH
// ============================================================================

enum class MoveType {
    RELOCATE,          // Move one employee to another position
    EXCHANGE,          // Swap two employees
    TWO_OPT,           // Reverse a segment within a route
    OR_OPT,            // Move a chain of 1-3 consecutive employees
    CROSS_EXCHANGE,    // Exchange segments between two routes
    RELOCATE_BLOCK,    // Move a block of consecutive employees
    INSERT_UNASSIGNED  // Insert an unassigned employee
};

struct Move {
    MoveType type;
    
    // Source
    int from_route;
    int from_pos;
    int chain_length;  // For Or-opt and block moves
    
    // Destination  
    int to_route;
    int to_pos;
    
    // For exchange moves
    int swap_pos;      // Position in to_route for exchange
    
    // Computed delta
    double delta_cost;
    double delta_penalty;
    double delta_objective;
    
    // Validity
    bool feasible;
};

// ============================================================================
// GLS PENALTY STRUCTURE
// ============================================================================

struct GLSPenalties {
    // Penalties for edges (arc usage)
    std::map<std::pair<int,int>, double> arc_penalties;
    
    // Penalties for assignments (employee -> vehicle)
    std::map<std::pair<int,int>, double> assignment_penalties;
    
    // Get penalty for an arc
    double getArcPenalty(int from, int to) const {
        auto key = std::make_pair(from, to);
        auto it = arc_penalties.find(key);
        return (it != arc_penalties.end()) ? it->second : 0.0;
    }
    
    // Increment arc penalty
    void incrementArcPenalty(int from, int to, double amount) {
        auto key = std::make_pair(from, to);
        arc_penalties[key] += amount;
    }
    
    // Get assignment penalty
    double getAssignmentPenalty(int emp, int veh) const {
        auto key = std::make_pair(emp, veh);
        auto it = assignment_penalties.find(key);
        return (it != assignment_penalties.end()) ? it->second : 0.0;
    }
    
    // Increment assignment penalty
    void incrementAssignmentPenalty(int emp, int veh, double amount) {
        auto key = std::make_pair(emp, veh);
        assignment_penalties[key] += amount;
    }
    
    // Decay all penalties (to forget old local optima)
    void decay(double factor) {
        for (auto& [key, val] : arc_penalties) {
            val *= factor;
        }
        for (auto& [key, val] : assignment_penalties) {
            val *= factor;
        }
    }
};

// ============================================================================
// SOLVER STATISTICS
// ============================================================================

struct SolverStats {
    int iterations = 0;
    int improvements = 0;
    int feasible_solutions_found = 0;
    
    double initial_objective = INF;
    double best_objective = INF;
    
    double time_elapsed_ms = 0;
    
    // Move statistics
    std::map<MoveType, int> moves_tried;
    std::map<MoveType, int> moves_accepted;
};

} // namespace vrp

#endif // VRP_TYPES_H
