/*
 * VRP Construction Heuristics
 * 
 * Implements initial solution construction methods:
 * 1. PARALLEL_CHEAPEST_INSERTION (OR-Tools primary method)
 * 2. Sequential Cheapest Insertion
 * 3. Time-Oriented Nearest Neighbor
 * 4. Simple Nearest Neighbor
 */

#ifndef VRP_CONSTRUCTION_H
#define VRP_CONSTRUCTION_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include <vector>
#include <algorithm>
#include <limits>
#include <set>
#include <random>
#include <iostream>

namespace vrp {

// ============================================================================
// INSERTION COST STRUCTURE
// ============================================================================

struct InsertionCost {
    int route_index;
    int position;
    double cost_increase;
    double total_cost_after;
    bool feasible;
    int time_violations;
    int capacity_violations;
};

// ============================================================================
// INSERTION COST CALCULATION
// ============================================================================

/**
 * Calculate the cost of inserting an employee at a specific position in a route.
 * 
 * Cost = additional_distance * cost_per_km + time_penalty
 */
inline InsertionCost calculateInsertionCost(
    const Route& route,
    int position,
    int employee_idx,
    const ProblemInstance& problem,
    bool check_feasibility = true
) {
    InsertionCost result;
    result.route_index = route.vehicle_index;
    result.position = position;
    result.feasible = true;
    result.time_violations = 0;
    result.capacity_violations = 0;
    
    const Vehicle& vehicle = problem.vehicles[route.vehicle_index];
    const Employee& emp = problem.employees[employee_idx];
    
    // Node indices: 0 = depot, employee_idx+1 = employee node
    int emp_node = employee_idx + 1;
    
    // Check capacity
    if ((int)route.employee_sequence.size() >= vehicle.capacity) {
        result.feasible = false;
        result.capacity_violations = 1;
        result.cost_increase = INF;
        return result;
    }
    
    // Check vehicle preference
    if (emp.vehicle_pref != 0 && vehicle.category != 0) {
        if (emp.vehicle_pref != vehicle.category) {
            result.cost_increase = PENALTY_VEHICLE_PREF;  // Still try but penalize
        }
    }
    
    // Calculate distance increase
    double dist_increase = 0;
    
    if (route.empty()) {
        // Empty route: depot -> employee -> depot
        dist_increase = problem.distance_matrix[0][emp_node] + 
                       problem.distance_matrix[emp_node][0];
    } else {
        // Get predecessor and successor nodes
        int pred_node = 0;  // Default to depot
        int succ_node = 0;  // Default to depot
        
        if (position > 0) {
            pred_node = route.employee_sequence[position - 1] + 1;
        }
        
        if (position < (int)route.employee_sequence.size()) {
            succ_node = route.employee_sequence[position] + 1;
        }
        
        // Distance increase = d(pred, emp) + d(emp, succ) - d(pred, succ)
        double old_dist = problem.distance_matrix[pred_node][succ_node];
        double new_dist = problem.distance_matrix[pred_node][emp_node] + 
                         problem.distance_matrix[emp_node][succ_node];
        dist_increase = new_dist - old_dist;
    }
    
    result.cost_increase = dist_increase * vehicle.cost_per_km;
    
    // Check time feasibility if requested
    if (check_feasibility) {
        // Quick feasibility check - create temporary route
        Route temp_route = route;
        temp_route.employee_sequence.insert(
            temp_route.employee_sequence.begin() + position,
            employee_idx
        );
        evaluateRoute(temp_route, problem);
        
        result.time_violations = temp_route.time_window_violations;
        result.total_cost_after = temp_route.total_cost;
        
        if (temp_route.time_window_violations > 0) {
            result.feasible = false;
            result.cost_increase += PENALTY_HARD_VIOLATION * temp_route.time_window_violations;
        }
        
        if (temp_route.capacity_violation > 0) {
            result.feasible = false;
            result.capacity_violations = temp_route.capacity_violation;
        }
    }
    
    return result;
}

/**
 * Find the best insertion position for an employee in a route.
 */
inline InsertionCost findBestInsertion(
    const Route& route,
    int employee_idx,
    const ProblemInstance& problem,
    bool require_feasible = true
) {
    InsertionCost best;
    best.cost_increase = INF;
    best.feasible = false;
    best.route_index = route.vehicle_index;
    best.position = 0;
    
    int max_pos = (int)route.employee_sequence.size();
    
    for (int pos = 0; pos <= max_pos; pos++) {
        InsertionCost cost = calculateInsertionCost(route, pos, employee_idx, problem, true);
        
        bool is_better = false;
        
        if (require_feasible) {
            // Prefer feasible insertions
            if (cost.feasible && !best.feasible) {
                is_better = true;
            } else if (cost.feasible == best.feasible) {
                is_better = (cost.cost_increase < best.cost_increase);
            }
        } else {
            is_better = (cost.cost_increase < best.cost_increase);
        }
        
        if (is_better) {
            best = cost;
        }
    }
    
    return best;
}

// ============================================================================
// PARALLEL CHEAPEST INSERTION
// ============================================================================

/**
 * Parallel Cheapest Insertion - OR-Tools primary construction heuristic.
 * 
 * Algorithm:
 * 1. Start with all routes empty
 * 2. For each unassigned employee, find best (route, position) pair
 * 3. Insert the employee with minimum cost increase
 * 4. Repeat until all employees assigned or no feasible insertion
 */
inline Solution parallelCheapestInsertion(
    const ProblemInstance& problem,
    bool respect_hard_constraints = false  // Start lenient
) {
    Solution solution = createEmptySolution(problem);
    
    // Set of unassigned employees
    std::set<int> unassigned;
    for (int i = 0; i < problem.num_employees(); i++) {
        unassigned.insert(i);
    }
    
    // Main loop - insert one employee at a time
    while (!unassigned.empty()) {
        int best_emp = -1;
        int best_route = -1;
        int best_pos = -1;
        double best_cost = INF;
        bool found_feasible = false;
        
        // Find best insertion across all unassigned employees and all routes
        for (int emp_idx : unassigned) {
            for (int r = 0; r < (int)solution.routes.size(); r++) {
                InsertionCost cost = findBestInsertion(
                    solution.routes[r], emp_idx, problem, respect_hard_constraints
                );
                
                bool is_better = false;
                
                if (respect_hard_constraints) {
                    if (cost.feasible && !found_feasible) {
                        is_better = true;
                        found_feasible = true;
                    } else if (cost.feasible == found_feasible) {
                        is_better = (cost.cost_increase < best_cost);
                    }
                } else {
                    is_better = (cost.cost_increase < best_cost);
                }
                
                if (is_better) {
                    best_emp = emp_idx;
                    best_route = r;
                    best_pos = cost.position;
                    best_cost = cost.cost_increase;
                }
            }
        }
        
        // Insert best employee
        if (best_emp >= 0 && best_cost < INF / 2) {
            insertEmployee(solution.routes[best_route], best_pos, best_emp);
            unassigned.erase(best_emp);
        } else {
            // No feasible insertion found
            if (respect_hard_constraints) {
                // Try without hard constraints
                respect_hard_constraints = false;
                continue;
            }
            // Truly stuck - mark remaining as unassigned
            break;
        }
    }
    
    // Update solution's unassigned list
    solution.unassigned.clear();
    for (int emp_idx : unassigned) {
        solution.unassigned.push_back(emp_idx);
    }
    
    evaluateSolution(solution, problem);
    return solution;
}

// ============================================================================
// SEQUENTIAL CHEAPEST INSERTION
// ============================================================================

/**
 * Sequential Cheapest Insertion - Build one route at a time.
 * 
 * Algorithm:
 * 1. For each vehicle:
 *    a. Start with empty route
 *    b. Repeatedly insert the cheapest unassigned employee
 *    c. Stop when route is full or no feasible insertion
 * 2. Continue until all employees assigned
 */
inline Solution sequentialCheapestInsertion(const ProblemInstance& problem) {
    Solution solution = createEmptySolution(problem);
    
    std::set<int> unassigned;
    for (int i = 0; i < problem.num_employees(); i++) {
        unassigned.insert(i);
    }
    
    // Build each route sequentially
    for (int r = 0; r < (int)solution.routes.size() && !unassigned.empty(); r++) {
        const Vehicle& vehicle = problem.vehicles[r];
        
        // Keep adding to this route until full or no improvement
        while ((int)solution.routes[r].employee_sequence.size() < vehicle.capacity && 
               !unassigned.empty()) {
            
            int best_emp = -1;
            int best_pos = 0;
            double best_cost = INF;
            
            for (int emp_idx : unassigned) {
                InsertionCost cost = findBestInsertion(
                    solution.routes[r], emp_idx, problem, true
                );
                
                if (cost.feasible && cost.cost_increase < best_cost) {
                    best_emp = emp_idx;
                    best_pos = cost.position;
                    best_cost = cost.cost_increase;
                }
            }
            
            if (best_emp >= 0) {
                insertEmployee(solution.routes[r], best_pos, best_emp);
                unassigned.erase(best_emp);
            } else {
                break;  // No more feasible insertions for this route
            }
        }
    }
    
    solution.unassigned.clear();
    for (int emp_idx : unassigned) {
        solution.unassigned.push_back(emp_idx);
    }
    
    evaluateSolution(solution, problem);
    return solution;
}

// ============================================================================
// TIME-ORIENTED INSERTION
// ============================================================================

/**
 * Time-Oriented Insertion - Prioritize employees by deadline.
 * 
 * Insert employees in order of their latest drop time (earliest deadline first).
 */
inline Solution timeOrientedInsertion(const ProblemInstance& problem) {
    Solution solution = createEmptySolution(problem);
    
    // Sort employees by latest drop time (deadline)
    std::vector<int> employees;
    for (int i = 0; i < problem.num_employees(); i++) {
        employees.push_back(i);
    }
    
    std::sort(employees.begin(), employees.end(), [&problem](int a, int b) {
        int time_a = parseTime(problem.employees[a].latest_drop);
        int time_b = parseTime(problem.employees[b].latest_drop);
        return time_a < time_b;  // Earlier deadline first
    });
    
    std::set<int> unassigned(employees.begin(), employees.end());
    
    // Insert in deadline order
    for (int emp_idx : employees) {
        if (unassigned.find(emp_idx) == unassigned.end()) continue;
        
        int best_route = -1;
        int best_pos = 0;
        double best_cost = INF;
        
        for (int r = 0; r < (int)solution.routes.size(); r++) {
            InsertionCost cost = findBestInsertion(
                solution.routes[r], emp_idx, problem, true
            );
            
            if (cost.feasible && cost.cost_increase < best_cost) {
                best_route = r;
                best_pos = cost.position;
                best_cost = cost.cost_increase;
            }
        }
        
        if (best_route >= 0) {
            insertEmployee(solution.routes[best_route], best_pos, emp_idx);
            unassigned.erase(emp_idx);
        }
    }
    
    solution.unassigned.clear();
    for (int emp_idx : unassigned) {
        solution.unassigned.push_back(emp_idx);
    }
    
    evaluateSolution(solution, problem);
    return solution;
}

// ============================================================================
// NEAREST NEIGHBOR
// ============================================================================

/**
 * Nearest Neighbor Heuristic.
 * 
 * For each route, start from depot and repeatedly add the nearest unassigned employee.
 */
inline Solution nearestNeighbor(const ProblemInstance& problem) {
    Solution solution = createEmptySolution(problem);
    
    std::set<int> unassigned;
    for (int i = 0; i < problem.num_employees(); i++) {
        unassigned.insert(i);
    }
    
    for (int r = 0; r < (int)solution.routes.size() && !unassigned.empty(); r++) {
        const Vehicle& vehicle = problem.vehicles[r];
        
        int current_node = 0;  // Start at depot
        
        while ((int)solution.routes[r].employee_sequence.size() < vehicle.capacity &&
               !unassigned.empty()) {
            
            // Find nearest unassigned employee
            int nearest_emp = -1;
            double nearest_dist = INF;
            
            for (int emp_idx : unassigned) {
                int emp_node = emp_idx + 1;
                double dist = problem.distance_matrix[current_node][emp_node];
                
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_emp = emp_idx;
                }
            }
            
            if (nearest_emp >= 0) {
                solution.routes[r].employee_sequence.push_back(nearest_emp);
                unassigned.erase(nearest_emp);
                current_node = nearest_emp + 1;
            } else {
                break;
            }
        }
    }
    
    solution.unassigned.clear();
    for (int emp_idx : unassigned) {
        solution.unassigned.push_back(emp_idx);
    }
    
    evaluateSolution(solution, problem);
    return solution;
}

// ============================================================================
// MULTI-START CONSTRUCTION
// ============================================================================

/**
 * Try multiple construction heuristics and return the best solution.
 */
inline Solution multiStartConstruction(const ProblemInstance& problem) {
    std::vector<Solution> solutions;
    
    // Try all heuristics
    solutions.push_back(parallelCheapestInsertion(problem, true));
    solutions.push_back(sequentialCheapestInsertion(problem));
    solutions.push_back(timeOrientedInsertion(problem));
    solutions.push_back(nearestNeighbor(problem));
    
    // Find best
    Solution best = solutions[0];
    for (const Solution& sol : solutions) {
        if (sol.isBetterThan(best)) {
            best = sol;
        }
    }
    
    return best;
}

} // namespace vrp

#endif // VRP_CONSTRUCTION_H
