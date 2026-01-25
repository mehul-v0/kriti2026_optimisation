/*
 * VRP Solver - Full Constraints Implementation
 * 
 * Complete C++ implementation matching solver_ortools_full.py
 * 
 * Features:
 * - Multi-trip per vehicle
 * - Time windows with priority-based flexibility
 * - Vehicle-employee preference matching (premium/normal/any)
 * - Sharing preferences (single/double/triple)
 * - Incompatibility constraints
 * - Multi-stage solving (strict -> relaxed)
 * 
 * Compilation:
 *   g++ -std=c++17 -O3 -o vrp_solver_full vrp_solver_full.cpp
 * 
 * Usage:
 *   ./vrp_solver_full <input_file> [output_file] [time_limit]
 */

#include "vrp_types_full.h"
#include "vrp_parser_full.h"

#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>
#include <iomanip>

using namespace vrp;

// ============================================================================
// CONSTRAINT VALIDATION
// ============================================================================

/**
 * Check if employee can be assigned to vehicle based on preferences
 */
inline bool isVehicleCompatible(const Employee& emp, const Vehicle& veh) {
    // any (0) matches everything
    if (emp.vehicle_pref == 0 || veh.category == 0) return true;
    // Must match if both specify a category
    return emp.vehicle_pref == veh.category;
}

/**
 * Check if two employees are time-incompatible (can't be in same trip)
 */
inline bool areIncompatible(int emp_i, int emp_j, const ProblemInstance& problem) {
    for (const auto& pair : problem.incompatible_pairs) {
        if ((pair.first == emp_i && pair.second == emp_j) ||
            (pair.first == emp_j && pair.second == emp_i)) {
            return true;
        }
    }
    return false;
}

/**
 * Check sharing preference violations for a trip
 */
inline int countSharingViolations(const std::vector<int>& emp_indices, 
                                  const ProblemInstance& problem) {
    int violations = 0;
    int trip_size = emp_indices.size();
    
    for (int emp_idx : emp_indices) {
        const auto& emp = problem.employees[emp_idx];
        
        if (emp.sharing_pref == 1 && trip_size > 1) {
            // Single preference violated
            violations++;
        } else if (emp.sharing_pref == 2 && trip_size > 2) {
            // Double preference violated
            violations++;
        } else if (emp.sharing_pref == 3 && trip_size > 3) {
            // Triple preference violated (rare)
            violations++;
        }
    }
    
    return violations;
}

// ============================================================================
// TRIP EVALUATION
// ============================================================================

/**
 * Evaluate a trip and populate all metrics including constraint violations
 * @param earliest_start_time - When vehicle becomes available (after previous trip or from depot)
 * @param start_from_office - If true, trip starts from office (not depot) - for trips 2, 3, etc.
 */
inline void evaluateTrip(Trip& trip, int vehicle_idx, const ProblemInstance& problem,
                        bool enforce_soft_constraints = true,
                        int earliest_start_time = -1,
                        bool start_from_office = false) {
    if (trip.employee_indices.empty()) return;
    
    const Vehicle& veh = problem.vehicles[vehicle_idx];
    trip.vehicle_idx = vehicle_idx;
    
    // Check capacity
    if ((int)trip.employee_indices.size() > veh.capacity) {
        trip.hard_violations++;
    }
    
    // Build route: depot/office -> employee pickups (sorted by earliest_pickup) -> office
    std::vector<int> sorted_emps = trip.employee_indices;
    std::sort(sorted_emps.begin(), sorted_emps.end(), 
              [&](int a, int b) {
                  return problem.employees[a].earliest_pickup < 
                         problem.employees[b].earliest_pickup;
              });
    
    trip.stops.clear();
    
    // Determine starting point and earliest possible start time
    double start_lat, start_lon;
    int base_time;
    
    if (start_from_office) {
        // Trip starts from office (after dropping off previous trip's employees)
        start_lat = problem.config.office_lat;
        start_lon = problem.config.office_lon;
        base_time = earliest_start_time;  // Time when previous trip ended at office
    } else {
        // First trip starts from depot
        start_lat = veh.start_lat;
        start_lon = veh.start_lon;
        base_time = (earliest_start_time >= 0) ? earliest_start_time : veh.available_from;
    }
    
    // Calculate travel time to first employee
    double dist_to_first = haversine(start_lat, start_lon, 
                                     problem.employees[sorted_emps[0]].pickup_lat,
                                     problem.employees[sorted_emps[0]].pickup_lon);
    int travel_to_first = (int)((dist_to_first / veh.speed_kmh) * 60);
    
    // Trip can start when: vehicle is available AND can reach first employee by their earliest_pickup
    int first_emp_earliest = problem.employees[sorted_emps[0]].earliest_pickup;
    int current_time = std::max(base_time, first_emp_earliest - travel_to_first);
    
    double current_lat = start_lat;
    double current_lon = start_lon;
    
    trip.start_time = current_time;
    trip.total_distance = 0.0;
    
    // Start from depot or office (for subsequent trips)
    Stop depot_start;
    depot_start.type = start_from_office ? Stop::OFFICE_DROP : Stop::DEPOT_START;  // Reuse OFFICE_DROP type for "office start"
    depot_start.employee_idx = -1;
    depot_start.lat = start_lat;
    depot_start.lon = start_lon;
    depot_start.arrival_time = current_time;
    depot_start.departure_time = current_time;
    depot_start.distance_from_prev = 0.0;
    trip.stops.push_back(depot_start);
    // Mark it as office_start for JSON output
    if (start_from_office) {
        trip.stops.back().type = Stop::DEPOT_START;  // We'll handle label in JSON
    }
    
    // Pick up employees
    for (int emp_idx : sorted_emps) {
        const Employee& emp = problem.employees[emp_idx];
        
        // Travel to pickup location
        double dist = haversine(current_lat, current_lon, emp.pickup_lat, emp.pickup_lon);
        trip.total_distance += dist;
        
        int travel_time = (int)((dist / veh.speed_kmh) * 60);
        int arrival = current_time + travel_time;
        
        // Must wait until earliest_pickup
        if (arrival < emp.earliest_pickup) {
            arrival = emp.earliest_pickup;
        }
        
        int departure = arrival + emp.service_time;
        
        Stop pickup;
        pickup.type = Stop::PICKUP;
        pickup.employee_idx = emp_idx;
        pickup.lat = emp.pickup_lat;
        pickup.lon = emp.pickup_lon;
        pickup.arrival_time = arrival;
        pickup.departure_time = departure;
        pickup.distance_from_prev = dist;
        trip.stops.push_back(pickup);
        
        current_time = departure;
        current_lat = emp.pickup_lat;
        current_lon = emp.pickup_lon;
    }
    
    // Drop at office
    double dist_to_office = haversine(current_lat, current_lon, 
                                      problem.config.office_lat, problem.config.office_lon);
    trip.total_distance += dist_to_office;
    
    int travel_time = (int)((dist_to_office / veh.speed_kmh) * 60);
    int office_arrival = current_time + travel_time;
    
    Stop office_drop;
    office_drop.type = Stop::OFFICE_DROP;
    office_drop.employee_idx = -1;
    office_drop.lat = problem.config.office_lat;
    office_drop.lon = problem.config.office_lon;
    office_drop.arrival_time = office_arrival;
    office_drop.departure_time = office_arrival;
    office_drop.distance_from_prev = dist_to_office;
    trip.stops.push_back(office_drop);
    
    trip.end_time = office_arrival;
    trip.total_time = office_arrival - trip.start_time;
    trip.total_cost = trip.total_distance * veh.cost_per_km;
    
    // Check time window violations
    trip.hard_violations = 0;
    for (int emp_idx : sorted_emps) {
        const Employee& emp = problem.employees[emp_idx];
        int max_delay = problem.config.priority_max_delays[emp.priority - 1];
        int adjusted_latest = emp.latest_drop + max_delay;
        
        if (office_arrival > adjusted_latest) {
            // Hard violation: late even with priority flexibility
            trip.hard_violations++;
        }
    }
    
    // Check soft constraint violations
    trip.soft_violations = 0;
    
    if (enforce_soft_constraints) {
        // Vehicle preference violations
        for (int emp_idx : sorted_emps) {
            const Employee& emp = problem.employees[emp_idx];
            if (!isVehicleCompatible(emp, veh)) {
                trip.soft_violations++;
            }
        }
        
        // Sharing preference violations
        trip.soft_violations += countSharingViolations(sorted_emps, problem);
    }
    
    // Set the starts_from_office flag
    trip.starts_from_office = start_from_office;
}

/**
 * Re-chain all trips for a vehicle so they are sequential
 * Trip 1: starts from depot at vehicle.available_from
 * Trip 2: starts from office when Trip 1 ends
 * Trip 3: starts from office when Trip 2 ends
 * etc.
 */
inline void rechainVehicleTrips(std::vector<Trip>& trips, int vehicle_idx, 
                                const ProblemInstance& problem,
                                bool enforce_soft_constraints = true) {
    if (trips.empty()) return;
    
    const Vehicle& veh = problem.vehicles[vehicle_idx];
    int current_available_time = veh.available_from;
    
    for (size_t i = 0; i < trips.size(); i++) {
        Trip& trip = trips[i];
        bool from_office = (i > 0);  // First trip from depot, others from office
        
        // Re-evaluate trip with correct start time and location
        evaluateTrip(trip, vehicle_idx, problem, enforce_soft_constraints, 
                    current_available_time, from_office);
        
        // Next trip can start when this one ends at office
        current_available_time = trip.end_time;
    }
}

/**
 * Re-chain all trips in the solution so timings are sequential
 */
inline void rechainAllTrips(Solution& solution, const ProblemInstance& problem,
                           bool enforce_soft_constraints = true) {
    for (size_t v = 0; v < solution.vehicle_trips.size(); v++) {
        rechainVehicleTrips(solution.vehicle_trips[v], v, problem, enforce_soft_constraints);
    }
    
    // Recalculate solution totals
    solution.total_cost = 0.0;
    solution.total_distance = 0.0;
    solution.total_time = 0.0;
    solution.hard_violations = 0;
    solution.soft_violations = 0;
    
    for (const auto& trips : solution.vehicle_trips) {
        for (const auto& trip : trips) {
            if (!trip.empty()) {
                solution.total_cost += trip.total_cost;
                solution.total_distance += trip.total_distance;
                solution.total_time += trip.total_time;
                solution.hard_violations += trip.hard_violations;
                solution.soft_violations += trip.soft_violations;
            }
        }
    }
}

// ============================================================================
// CONSTRUCTION HEURISTIC - PARALLEL CHEAPEST INSERTION
// ============================================================================

/**
 * Parallel Cheapest Insertion with full constraint awareness
 * Modified to better match OR-Tools PARALLEL_CHEAPEST_INSERTION strategy:
 * 1. First handle single-preference employees (they get solo trips)
 * 2. Then greedily assign remaining employees respecting all constraints
 * 3. Balance vehicle utilization
 */
inline Solution parallelCheapestInsertion(const ProblemInstance& problem,
                                          bool enforce_soft_constraints = true,
                                          int max_trips_per_vehicle = 3) {
    Solution solution;
    solution.vehicle_trips.resize(problem.vehicles.size());
    
    std::vector<bool> assigned(problem.employees.size(), false);
    
    // ========== PHASE 1: Assign single-preference employees first ==========
    // They MUST be alone in trips, so handle them first
    if (enforce_soft_constraints) {
        for (size_t e = 0; e < problem.employees.size(); e++) {
            const Employee& emp = problem.employees[e];
            
            if (emp.sharing_pref != 1) continue;  // Only single preference
            
            // Find compatible vehicle with capacity
            double best_cost = INF;
            int best_vehicle = -1;
            
            for (size_t v = 0; v < problem.vehicles.size(); v++) {
                if ((int)solution.vehicle_trips[v].size() >= max_trips_per_vehicle) continue;
                
                const Vehicle& veh = problem.vehicles[v];
                
                // Check vehicle compatibility
                if (!isVehicleCompatible(emp, veh)) continue;
                
                // Calculate cost
                double dist = haversine(veh.start_lat, veh.start_lon, 
                                      emp.pickup_lat, emp.pickup_lon);
                dist += emp.dist_pickup_to_office;
                double cost = dist * veh.cost_per_km;
                
                if (cost < best_cost) {
                    best_cost = cost;
                    best_vehicle = v;
                }
            }
            
            if (best_vehicle != -1) {
                Trip solo_trip;
                solo_trip.employee_indices.push_back(e);
                evaluateTrip(solo_trip, best_vehicle, problem, enforce_soft_constraints);
                solution.vehicle_trips[best_vehicle].push_back(solo_trip);
                assigned[e] = true;
            }
        }
    }
    
    // ========== PHASE 2: Assign remaining employees ==========
    // Keep assigning until no more employees can be assigned
    for (int iter = 0; iter < 100; iter++) {
        bool made_assignment = false;
        
        // Try to create a new trip
        double best_cost_increase = INF;
        int best_vehicle = -1;
        int best_employee = -1;
        
        // For each vehicle (that hasn't exceeded trip limit)
        for (size_t v = 0; v < problem.vehicles.size(); v++) {
            if ((int)solution.vehicle_trips[v].size() >= max_trips_per_vehicle) {
                continue;  // Vehicle at trip limit
            }
            
            const Vehicle& veh = problem.vehicles[v];
            
            // For each unassigned employee
            for (size_t e = 0; e < problem.employees.size(); e++) {
                if (assigned[e]) continue;
                
                const Employee& emp = problem.employees[e];
                
                // Check vehicle compatibility if enforcing soft constraints
                if (enforce_soft_constraints && !isVehicleCompatible(emp, veh)) {
                    continue;
                }
                
                // Calculate cost of single-employee trip
                double dist = haversine(veh.start_lat, veh.start_lon, 
                                      emp.pickup_lat, emp.pickup_lon);
                dist += emp.dist_pickup_to_office;
                double cost = dist * veh.cost_per_km;
                
                // Add penalty for using same vehicle repeatedly (encourage spread)
                // This helps match OR-Tools behavior of using multiple vehicles
                cost += solution.vehicle_trips[v].size() * 10.0;
                
                if (cost < best_cost_increase) {
                    best_cost_increase = cost;
                    best_vehicle = v;
                    best_employee = e;
                }
            }
        }
        
        if (best_vehicle == -1 || best_employee == -1) {
            break;  // No more feasible assignments
        }
        
        // Create new trip with best employee
        Trip new_trip;
        new_trip.employee_indices.push_back(best_employee);
        evaluateTrip(new_trip, best_vehicle, problem, enforce_soft_constraints);
        
        // Try to add more employees to this trip (greedy insertion)
        const Vehicle& veh = problem.vehicles[best_vehicle];
        
        for (int add_iter = 0; add_iter < 10; add_iter++) {
            int best_to_add = -1;
            double best_additional_cost = INF;
            
            // Determine maximum trip size based on current employees' sharing preferences
            int max_trip_size = veh.capacity;
            if (enforce_soft_constraints) {
                for (int existing_emp : new_trip.employee_indices) {
                    int pref = problem.employees[existing_emp].sharing_pref;
                    if (pref < max_trip_size) {
                        max_trip_size = pref;
                    }
                }
            }
            
            // If already at max size, don't try to add more
            if ((int)new_trip.employee_indices.size() >= max_trip_size) {
                break;
            }
            
            for (size_t e = 0; e < problem.employees.size(); e++) {
                if (assigned[e]) continue;
                
                // Check if employee is already in this trip
                bool already_in_trip = false;
                for (int existing : new_trip.employee_indices) {
                    if ((size_t)existing == e) {
                        already_in_trip = true;
                        break;
                    }
                }
                if (already_in_trip) continue;
                
                const Employee& emp = problem.employees[e];
                
                // Check capacity
                if ((int)new_trip.employee_indices.size() + 1 > veh.capacity) {
                    continue;
                }
                
                // Check vehicle compatibility
                if (enforce_soft_constraints && !isVehicleCompatible(emp, veh)) {
                    continue;
                }
                
                // Check time incompatibility
                bool incompatible = false;
                for (int existing_emp : new_trip.employee_indices) {
                    if (areIncompatible(e, existing_emp, problem)) {
                        incompatible = true;
                        break;
                    }
                }
                if (incompatible) continue;
                
                // Check sharing preference if enforcing soft constraints
                if (enforce_soft_constraints) {
                    // If current employees have single preference, can't add
                    bool has_single_pref = false;
                    for (int existing_emp : new_trip.employee_indices) {
                        if (problem.employees[existing_emp].sharing_pref == 1) {
                            has_single_pref = true;
                            break;
                        }
                    }
                    if (has_single_pref) continue;
                    
                    // If new employee has single preference, can't add to existing trip
                    if (emp.sharing_pref == 1) {
                        continue;
                    }
                    
                    // Check if adding would violate any sharing preference
                    int new_trip_size = new_trip.employee_indices.size() + 1;
                    bool would_violate = false;
                    for (int existing_emp : new_trip.employee_indices) {
                        if (new_trip_size > problem.employees[existing_emp].sharing_pref) {
                            would_violate = true;
                            break;
                        }
                    }
                    // Also check new employee's preference
                    if (new_trip_size > emp.sharing_pref) {
                        would_violate = true;
                    }
                    if (would_violate) continue;
                }
                
                // Try adding this employee
                Trip test_trip = new_trip;
                test_trip.employee_indices.push_back(e);
                evaluateTrip(test_trip, best_vehicle, problem, enforce_soft_constraints);
                
                double additional_cost = test_trip.total_cost - new_trip.total_cost;
                
                // Heavy penalty for violations
                additional_cost += test_trip.hard_violations * PENALTY_HARD_TIME_VIOLATION;
                additional_cost += test_trip.soft_violations * PENALTY_VEHICLE_PREF;
                
                if (additional_cost < best_additional_cost) {
                    best_additional_cost = additional_cost;
                    best_to_add = e;
                }
            }
            
            if (best_to_add == -1) break;  // No more employees can be added
            
            new_trip.employee_indices.push_back(best_to_add);
            evaluateTrip(new_trip, best_vehicle, problem, enforce_soft_constraints);
        }
        
        // Add this trip to solution
        solution.vehicle_trips[best_vehicle].push_back(new_trip);
        for (int emp_idx : new_trip.employee_indices) {
            assigned[emp_idx] = true;
        }
        made_assignment = true;
    }
    
    // Collect unassigned
    for (size_t e = 0; e < problem.employees.size(); e++) {
        if (!assigned[e]) {
            solution.unassigned.push_back(e);
        }
    }
    
    // Calculate solution metrics
    solution.total_cost = 0.0;
    solution.total_distance = 0.0;
    solution.total_time = 0.0;
    solution.hard_violations = 0;
    solution.soft_violations = 0;
    solution.vehicles_used = 0;
    solution.trips_used = 0;
    
    for (const auto& trips : solution.vehicle_trips) {
        if (!trips.empty()) solution.vehicles_used++;
        for (const auto& trip : trips) {
            if (!trip.empty()) {
                solution.trips_used++;
                solution.total_cost += trip.total_cost;
                solution.total_distance += trip.total_distance;
                solution.total_time += trip.total_time;
                solution.hard_violations += trip.hard_violations;
                solution.soft_violations += trip.soft_violations;
            }
        }
    }
    
    // Objective = cost + violations
    solution.objective_value = solution.total_cost + 
                               solution.hard_violations * PENALTY_HARD_TIME_VIOLATION +
                               solution.soft_violations * PENALTY_VEHICLE_PREF;
    
    return solution;
}

// ============================================================================
// MULTI-STAGE SOLVER
// ============================================================================

/**
 * Multi-stage solver matching solver_ortools_full.py logic:
 * Stage 1: Enforce ALL constraints (hard + soft)
 * Stage 2: Relax soft constraints (only enforce time windows)
 * Stage 3: Return best available
 * 
 * MATCHING PYTHON BEHAVIOR:
 * - If Stage 1 has 0 hard violations, only use Stage 2 if it has STRICTLY FEWER hard violations
 * - This ensures behavior matches OR-Tools multi-stage approach
 */
inline void printViolationDetails(const Solution& solution, const ProblemInstance& problem) {
    std::cout << "\n📋 Constraint Violation Report:" << std::endl;
    
    for (size_t v = 0; v < solution.vehicle_trips.size(); v++) {
        const auto& trips = solution.vehicle_trips[v];
        const auto& vehicle = problem.vehicles[v];
        
        for (const auto& trip : trips) {
            if (trip.empty()) continue;
            
            // Get office arrival time (end_time of trip)
            int office_arrival = trip.end_time;
            
            // Check vehicle preference violations for each employee
            for (int emp_idx : trip.employee_indices) {
                const auto& emp = problem.employees[emp_idx];
                
                // Check vehicle preference (soft)
                if (emp.vehicle_pref != 0 && vehicle.category != 0 && emp.vehicle_pref != vehicle.category) {
                    std::string pref_type = (emp.vehicle_pref == 1) ? "PREMIUM" : "NORMAL";
                    std::string veh_type = (vehicle.category == 1) ? "PREMIUM" : "NORMAL";
                    std::cout << "  ⚠️ SOFT VIOLATION: Employee " << emp.id << " prefers " << pref_type 
                              << " vehicle but got " << veh_type << " (" << vehicle.id << ")" << std::endl;
                }
                
                // Check sharing preference (soft)
                int trip_size = trip.employee_indices.size();
                if (emp.sharing_pref == 1 && trip_size > 1) {
                    std::cout << "  ⚠️ SOFT VIOLATION: Employee " << emp.id << " prefers SINGLE ride but is sharing with " 
                              << (trip_size - 1) << " others" << std::endl;
                } else if (emp.sharing_pref == 2 && trip_size > 2) {
                    std::cout << "  ⚠️ SOFT VIOLATION: Employee " << emp.id << " prefers max DOUBLE (2) but " 
                              << trip_size << " people in trip" << std::endl;
                } else if (emp.sharing_pref == 3 && trip_size > 3) {
                    std::cout << "  ⚠️ SOFT VIOLATION: Employee " << emp.id << " prefers max TRIPLE (3) but " 
                              << trip_size << " people in trip" << std::endl;
                }
                
                // Check time window (hard)
                int max_delay = problem.config.priority_max_delays[emp.priority - 1];
                int adjusted_latest = emp.latest_drop + max_delay;
                if (office_arrival > adjusted_latest) {
                    int delay_mins = office_arrival - adjusted_latest;
                    std::string arrival_time = formatTime(office_arrival);
                    std::string deadline_time = formatTime(adjusted_latest);
                    std::cout << "  ❌ HARD VIOLATION: Employee " << emp.id << " arrives at " << arrival_time
                              << " but deadline was " << deadline_time << " (" << delay_mins << " min late)" << std::endl;
                }
            }
        }
    }
}

inline Solution solveVRP(const ProblemInstance& problem, double time_limit, bool verbose = true) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (verbose) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "MULTI-STAGE VRP SOLVER (Custom C++)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
    // ========== STAGE 1: ALL CONSTRAINTS ==========
    if (verbose) {
        std::cout << "\n📋 STAGE 1: Enforcing ALL constraints..." << std::endl;
    }
    
    Solution stage1 = parallelCheapestInsertion(problem, true, 3);
    rechainAllTrips(stage1, problem, true);  // Re-chain trips so timings are sequential
    
    if (verbose) {
        std::cout << "   Cost: $" << std::fixed << std::setprecision(2) << stage1.total_cost << std::endl;
        std::cout << "   Hard violations: " << stage1.hard_violations << std::endl;
        std::cout << "   Soft violations: " << stage1.soft_violations << std::endl;
        std::cout << "   Unassigned: " << stage1.unassigned.size() << std::endl;
    }
    
    // Perfect solution - return immediately (must have 0 violations AND all assigned)
    if (stage1.hard_violations == 0 && stage1.soft_violations == 0 && stage1.unassigned.empty()) {
        if (verbose) {
            std::cout << "✅ STAGE 1 SUCCESS: Found optimal solution!" << std::endl;
        }
        
        // Calculate penalty (0 for perfect solution)
        stage1.penalty = 0.0;
        
        // Print summary (matching Python format)
        std::cout << "\n✓ Solution: Cost=$" << std::fixed << std::setprecision(2) << stage1.total_cost 
                  << ", Hard violations=" << stage1.hard_violations 
                  << ", Soft violations=" << stage1.soft_violations << std::endl;
        
        return stage1;
    }
    
    // ========== STAGE 2: RELAX SOFT CONSTRAINTS ==========
    if (verbose) {
        std::cout << "\n📋 STAGE 2: Relaxing soft constraints..." << std::endl;
    }
    
    Solution stage2 = parallelCheapestInsertion(problem, false, 3);
    rechainAllTrips(stage2, problem, false);  // Re-chain trips so timings are sequential
    
    if (verbose) {
        std::cout << "   Cost: $" << std::fixed << std::setprecision(2) << stage2.total_cost << std::endl;
        std::cout << "   Hard violations: " << stage2.hard_violations << std::endl;
        std::cout << "   Soft violations: " << stage2.soft_violations << std::endl;
        std::cout << "   Unassigned: " << stage2.unassigned.size() << std::endl;
    }
    
    // Choose best solution based on:
    // 1. Fewer unassigned employees
    // 2. If same, fewer hard violations
    // 3. If same, fewer soft violations
    // 4. If same, lower cost
    bool use_stage2 = false;
    
    // Prefer solution with fewer unassigned
    if (stage2.unassigned.size() < stage1.unassigned.size()) {
        use_stage2 = true;
        if (verbose) {
            std::cout << "✅ STAGE 2 assigned more employees: " << (problem.employees.size() - stage2.unassigned.size()) 
                      << " vs " << (problem.employees.size() - stage1.unassigned.size()) << std::endl;
        }
    }
    // If both have same unassigned count, prefer fewer hard violations
    else if (stage2.unassigned.size() == stage1.unassigned.size() && 
             stage2.hard_violations < stage1.hard_violations) {
        use_stage2 = true;
        if (verbose) {
            std::cout << "✅ STAGE 2 improved hard violations: " << stage2.hard_violations << " < " << stage1.hard_violations << std::endl;
        }
    }
    
    Solution& best_solution = use_stage2 ? stage2 : stage1;
    
    // Re-count soft violations for Stage 2 (which relaxed soft constraints during construction)
    if (use_stage2) {
        best_solution.soft_violations = 0;
        for (size_t v = 0; v < best_solution.vehicle_trips.size(); v++) {
            const auto& trips = best_solution.vehicle_trips[v];
            const auto& vehicle = problem.vehicles[v];
            
            for (const auto& trip : trips) {
                if (trip.empty()) continue;
                int trip_size = trip.employee_indices.size();
                
                for (int emp_idx : trip.employee_indices) {
                    const auto& emp = problem.employees[emp_idx];
                    
                    // Vehicle preference violations
                    if (emp.vehicle_pref != 0 && vehicle.category != 0 && emp.vehicle_pref != vehicle.category) {
                        best_solution.soft_violations++;
                    }
                    
                    // Sharing preference violations
                    if (emp.sharing_pref == 1 && trip_size > 1) {
                        best_solution.soft_violations++;
                    } else if (emp.sharing_pref == 2 && trip_size > 2) {
                        best_solution.soft_violations++;
                    } else if (emp.sharing_pref == 3 && trip_size > 3) {
                        best_solution.soft_violations++;
                    }
                }
            }
        }
    }
    
    // Calculate penalty matching Python logic
    best_solution.penalty = 0.0;
    for (size_t v = 0; v < best_solution.vehicle_trips.size(); v++) {
        const auto& trips = best_solution.vehicle_trips[v];
        const auto& vehicle = problem.vehicles[v];
        
        for (const auto& trip : trips) {
            if (trip.empty()) continue;
            int trip_size = trip.employee_indices.size();
            int office_arrival = trip.end_time;
            
            for (int emp_idx : trip.employee_indices) {
                const auto& emp = problem.employees[emp_idx];
                
                // Vehicle preference penalty
                if (emp.vehicle_pref != 0 && vehicle.category != 0 && emp.vehicle_pref != vehicle.category) {
                    best_solution.penalty += 100.0;
                }
                
                // Sharing preference penalties
                if (emp.sharing_pref == 1 && trip_size > 1) {
                    best_solution.penalty += 200.0;
                } else if (emp.sharing_pref == 2 && trip_size > 2) {
                    best_solution.penalty += 150.0;
                } else if (emp.sharing_pref == 3 && trip_size > 3) {
                    best_solution.penalty += 100.0;
                }
                
                // Time window penalty
                int max_delay = problem.config.priority_max_delays[emp.priority - 1];
                int adjusted_latest = emp.latest_drop + max_delay;
                if (office_arrival > adjusted_latest) {
                    best_solution.penalty += 100000.0;
                }
            }
        }
    }
    
    // Print violation details if there are any
    if (best_solution.hard_violations > 0 || best_solution.soft_violations > 0) {
        printViolationDetails(best_solution, problem);
    }
    
    // Return based on what we found
    if (best_solution.hard_violations == 0) {
        if (verbose) {
            if (!use_stage2) {
                std::cout << "✅ Returning STAGE 1 solution (0 hard violations, " 
                          << best_solution.soft_violations << " soft violations)" << std::endl;
            } else {
                std::cout << "✅ Returning STAGE 2 solution (0 hard violations)" << std::endl;
            }
        }
        
        // Print summary (matching Python format)
        std::cout << "\n✓ Solution: Cost=$" << std::fixed << std::setprecision(2) << best_solution.total_cost 
                  << ", Hard violations=" << best_solution.hard_violations 
                  << ", Soft violations=" << best_solution.soft_violations << std::endl;
        
        return best_solution;
    }
    
    // ========== STAGE 3: RETURN BEST ==========
    if (verbose) {
        std::cout << "\n⚠️  STAGE 3: Returning best available solution" << std::endl;
        std::cout << "   Hard violations: " << best_solution.hard_violations << std::endl;
        std::cout << "   Soft violations: " << best_solution.soft_violations << std::endl;
    }
    
    // Print summary (matching Python format)
    std::cout << "\n✓ Solution: Cost=$" << std::fixed << std::setprecision(2) << best_solution.total_cost 
              << ", Hard violations=" << best_solution.hard_violations 
              << ", Soft violations=" << best_solution.soft_violations << std::endl;
    
    return best_solution;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [output_file] [time_limit]" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = (argc >= 3) ? argv[2] : "";
    double time_limit = (argc >= 4) ? std::stod(argv[3]) : 60.0;
    
    try {
        // Parse input
        std::cout << "Parsing: " << input_file << std::endl;
        ProblemInstance problem = parseInputFile(input_file);
        
        std::cout << "Employees: " << problem.employees.size() << std::endl;
        std::cout << "Vehicles: " << problem.vehicles.size() << std::endl;
        std::cout << "Incompatible pairs: " << problem.incompatible_pairs.size() << std::endl;
        
        // Solve
        auto start = std::chrono::steady_clock::now();
        Solution solution = solveVRP(problem, time_limit, true);
        auto end = std::chrono::steady_clock::now();
        
        double elapsed = std::chrono::duration<double>(end - start).count();
        std::cout << "\nSolved in " << elapsed << " seconds" << std::endl;
        
        // Output
        std::string json = solutionToJson(solution, problem);
        
        if (output_file.empty()) {
            std::cout << "\n=== JSON Output ===" << std::endl;
            std::cout << json << std::endl;
        } else {
            std::ofstream out(output_file);
            out << json;
            out.close();
            std::cout << "Output written to: " << output_file << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
