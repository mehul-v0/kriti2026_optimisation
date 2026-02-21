#ifndef VRP_TYPES_H
#define VRP_TYPES_H

#include <string>
#include <vector>

// Global trips-per-vehicle (set dynamically by parser based on employee count)
inline int TRIPS_PER_VEHICLE = 4;

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct Employee {
    int id = 0;
    int node_idx = 0;
    double pickup_lat = 0, pickup_lng = 0;
    double drop_lat = 0, drop_lng = 0;
    int priority = 3;
    int earliest_pickup = 0;
    int latest_drop = 0;
    int latest_arrival_deadline = 0;
    int vehicle_pref = 0;        // 0=any, 1=premium, 2=normal
    int sharing_pref = 3;        // 1=single, 2=double, 3=triple
    std::string employee_id;
};

struct Vehicle {
    int id = 0;
    int physical_id = 0;
    int node_idx = 0;
    int start_node = 0;
    double current_lat = 0, current_lng = 0;
    int capacity = 4;
    double cost_per_km = 10.0;
    double speed_kmph = 40.0;
    int available_from = 0;
    int category = 0;            // 0=any, 1=premium, 2=normal
    std::string vehicle_id;
};

struct Metadata {
    int priority_max_delays[6] = {0, 5, 10, 15, 20, 30};  // index 0 = default 0
    double cost_weight = 0.7;
    double time_weight = 0.3;
    double distance_multiplier = 1.0;  // For actual map distances (default 1.0 for haversine)
};

// Constraint Programming Variable — supports arbitrary vehicle counts
struct IntVar {
    int employee_id;
    std::vector<bool> vehicle_domain;
    
    explicit IntVar(int num_vehicles) : employee_id(-1), vehicle_domain(num_vehicles, true) {}
    
    void remove_vehicle(int v) {
        if (v >= 0 && v < (int)vehicle_domain.size()) vehicle_domain[v] = false;
    }
    bool is_vehicle_valid(int v) const {
        return v >= 0 && v < (int)vehicle_domain.size() && vehicle_domain[v];
    }
};

struct InsertionInfo {
    int employee_idx, vehicle_idx, position;
    double delta_cost;
    InsertionInfo() : employee_idx(-1), vehicle_idx(-1), position(-1), delta_cost(1e18) {}
};

struct MoveDelta {
    double dist_cost, time_cost;
    double get_weighted_score(double cw, double tw) const { return cw * dist_cost + tw * time_cost; }
};

struct Stop {
    int node, employee_idx;
    std::string location_name;
    int arrival_time, departure_time, wait_time;
    double distance_from_prev;
};

struct Trip {
    int trip_number;
    std::vector<int> employee_indices;
    std::vector<Stop> stops;
    double total_distance, total_cost;
    int total_time;
};

struct VehicleSolution {
    std::string vehicle_id;
    int physical_id;
    std::vector<Trip> trips;
    double total_cost, total_distance;
    int total_time;
};

struct Solution {
    std::vector<VehicleSolution> vehicles;
    double total_cost, total_time, score;
    int hard_violations, soft_violations;
    std::string solution_type;
};

#endif
