#ifndef VRP_TYPES_H
#define VRP_TYPES_H

#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// CORE DATA STRUCTURES
// ============================================================================

struct Employee {
    int id;
    int node_idx;
    double pickup_lat, pickup_lng;
    double drop_lat, drop_lng;
    int priority;
    int earliest_pickup;
    int latest_drop;
    int latest_arrival_deadline;
    int vehicle_pref;        // 0=any, 1=premium, 2=normal
    int sharing_pref;        // 1=single, 2=double, 3=triple
    std::string employee_id;
};

struct Vehicle {
    int id;
    int physical_id;
    int node_idx;
    int start_node;
    double current_lat, current_lng;
    int capacity;
    double cost_per_km;
    double speed_kmph;
    int available_from;
    int category;            // 0=any, 1=premium, 2=normal
    std::string vehicle_id;
};

struct Metadata {
    int priority_max_delays[6];
    double cost_weight;
    double time_weight;
};

// Constraint Programming Variable
struct IntVar {
    int employee_id;
    uint64_t vehicle_domain;
    
    explicit IntVar(int num_vehicles) : employee_id(-1) {
        vehicle_domain = (num_vehicles <= 64) ? ((1ULL << num_vehicles) - 1) : 0xFFFFFFFFFFFFFFFFULL;
    }
    
    void remove_vehicle(int v) { vehicle_domain &= ~(1ULL << v); }
    bool is_vehicle_valid(int v) const { return (vehicle_domain & (1ULL << v)) != 0; }
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
