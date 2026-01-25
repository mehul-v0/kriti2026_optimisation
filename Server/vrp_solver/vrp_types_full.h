/*
 * VRP Types - Full Constraints Version
 * 
 * Complete data structures matching solver_ortools_full.py
 * Includes ALL constraints:
 * - Multi-trip per vehicle
 * - Time windows with priority-based flexibility
 * - Vehicle-employee preference matching
 * - Sharing preferences
 * - Incompatibility constraints
 */

#ifndef VRP_TYPES_FULL_H
#define VRP_TYPES_FULL_H

#include <vector>
#include <string>
#include <map>
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
constexpr double PENALTY_HARD_TIME_VIOLATION = 100000.0;  // Time window hard violation
constexpr double PENALTY_SINGLE_PREF = 200.0;              // Single sharing preference violated
constexpr double PENALTY_DOUBLE_PREF = 150.0;              // Double sharing preference violated
constexpr double PENALTY_TRIPLE_PREF = 100.0;              // Triple sharing preference violated
constexpr double PENALTY_VEHICLE_PREF = 100.0;             // Vehicle preference violated
constexpr double PENALTY_CAPACITY = 100000.0;              // Capacity exceeded

// ============================================================================
// EMPLOYEE DATA
// ============================================================================

struct Employee {
    std::string id;               // Employee identifier (e.g., "E01")
    
    // Location
    double pickup_lat;
    double pickup_lon;
    double drop_lat;              // Office location
    double drop_lon;
    
    // Time windows (in minutes from midnight)
    int earliest_pickup;          // Cannot pick up before this time
    int latest_drop;              // Must arrive at office by this time
    
    // Preferences and priority
    int priority;                 // 1-5 (1 = highest priority, gets more delay flexibility)
    int vehicle_pref;             // 0=any, 1=premium, 2=normal
    int sharing_pref;             // 1=single, 2=double, 3=triple/any
    
    // Additional
    int service_time;             // Time to pick up (minutes)
    
    // Computed
    double dist_pickup_to_office; // Direct distance from pickup to office
};

// ============================================================================
// VEHICLE DATA
// ============================================================================

struct Vehicle {
    std::string id;               // Vehicle identifier (e.g., "V01")
    
    // Starting location
    double start_lat;
    double start_lon;
    
    // Specifications
    int capacity;                 // Max passengers
    double cost_per_km;           // Cost in currency per km
    double speed_kmh;             // Average speed in km/h
    
    // Availability
    int available_from;           // Minutes from midnight when available
    
    // Category
    int category;                 // 0=any, 1=premium, 2=normal
};

// ============================================================================
// PROBLEM CONFIGURATION
// ============================================================================

struct ProblemConfig {
    // Objective weights
    double cost_weight;           // Alpha
    double time_weight;           // Beta
    
    // Priority-based delay limits (indexed by priority-1)
    std::vector<int> priority_max_delays;  // [priority1_delay, priority2_delay, ...]
    
    // Office/depot location
    double office_lat;
    double office_lon;
    
    ProblemConfig() 
        : cost_weight(0.6)
        , time_weight(0.4)
        , priority_max_delays({5, 10, 15, 20, 30})  // Default delays for priorities 1-5
        , office_lat(0.0)
        , office_lon(0.0)
    {}
};

// ============================================================================
// ROUTE STOP
// ============================================================================

struct Stop {
    enum Type { DEPOT_START, PICKUP, OFFICE_DROP, DEPOT_END };
    
    Type type;
    int employee_idx;             // -1 for depot/office
    double lat;
    double lon;
    int arrival_time;             // Minutes from midnight
    int departure_time;           // After service time
    double distance_from_prev;    // km
};

// ============================================================================
// TRIP (single journey for a vehicle)
// ============================================================================

struct Trip {
    int vehicle_idx;
    std::vector<int> employee_indices;  // Employees picked up in this trip
    std::vector<Stop> stops;            // Detailed stop-by-stop route
    
    double total_distance;        // km
    double total_cost;            // currency
    int total_time;               // minutes
    int start_time;               // When trip starts (minutes from midnight)
    int end_time;                 // When trip ends at office
    bool starts_from_office;      // True if this is trip 2, 3, etc. (starts from office, not depot)
    
    // Constraint violations
    int hard_violations;          // Time window violations beyond priority flexibility
    int soft_violations;          // Sharing/vehicle preference violations
    
    Trip() 
        : vehicle_idx(-1)
        , total_distance(0.0)
        , total_cost(0.0)
        , total_time(0)
        , start_time(0)
        , end_time(0)
        , starts_from_office(false)
        , hard_violations(0)
        , soft_violations(0)
    {}
    
    bool empty() const {
        return employee_indices.empty();
    }
};

// ============================================================================
// SOLUTION
// ============================================================================

struct Solution {
    std::vector<std::vector<Trip>> vehicle_trips;  // vehicle_trips[v] = list of trips for vehicle v
    std::vector<int> unassigned;                   // Employee indices not assigned
    
    // Metrics
    double total_cost;
    double total_distance;
    double total_time;
    int vehicles_used;
    int trips_used;
    int hard_violations;
    int soft_violations;
    double penalty;               // Total penalty value
    double objective_value;       // Combined score
    
    Solution() 
        : total_cost(0.0)
        , total_distance(0.0)
        , total_time(0.0)
        , vehicles_used(0)
        , trips_used(0)
        , hard_violations(0)
        , soft_violations(0)
        , penalty(0.0)
        , objective_value(0.0)
    {}
};

// ============================================================================
// PROBLEM INSTANCE
// ============================================================================

struct ProblemInstance {
    std::vector<Employee> employees;
    std::vector<Vehicle> vehicles;
    ProblemConfig config;
    
    // Distance matrix cache (employees + office + vehicle starts)
    std::vector<std::vector<double>> distance_matrix;
    
    // Precomputed incompatibilities (pairs of employees that can't be in same trip)
    std::vector<std::pair<int, int>> incompatible_pairs;
    
    void computeDistanceMatrix();
    void computeIncompatibilities();
};

} // namespace vrp

#endif // VRP_TYPES_FULL_H
