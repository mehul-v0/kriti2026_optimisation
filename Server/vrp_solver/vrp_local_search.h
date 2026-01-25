/*
 * VRP Local Search Operators
 * 
 * Implements neighborhood operators used by OR-Tools:
 * 1. RELOCATE - Move one customer to another position
 * 2. EXCHANGE (Swap) - Swap two customers
 * 3. TWO_OPT - Reverse a segment within a route
 * 4. OR_OPT - Move a chain of 1-3 consecutive customers
 * 5. CROSS_EXCHANGE - Exchange segments between routes
 * 
 * Each operator:
 * - Generates candidate moves
 * - Evaluates move delta (change in objective)
 * - Applies accepted moves
 */

#ifndef VRP_LOCAL_SEARCH_H
#define VRP_LOCAL_SEARCH_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include <vector>
#include <algorithm>

namespace vrp {

// ============================================================================
// RELOCATE OPERATOR
// ============================================================================

/**
 * RELOCATE: Move one employee from one position to another.
 * 
 * Can be:
 * - Intra-route: Move within the same route
 * - Inter-route: Move to a different vehicle's route
 * 
 * Returns all improving moves found.
 */
inline std::vector<Move> generateRelocateMoves(
    const Solution& solution,
    const ProblemInstance& problem,
    bool allow_worsening = false
) {
    std::vector<Move> moves;
    
    for (int from_r = 0; from_r < (int)solution.routes.size(); from_r++) {
        const Route& from_route = solution.routes[from_r];
        if (from_route.empty()) continue;
        
        for (int from_pos = 0; from_pos < (int)from_route.employee_sequence.size(); from_pos++) {
            int emp_idx = from_route.employee_sequence[from_pos];
            const Employee& emp = problem.employees[emp_idx];
            
            // Try all target routes and positions
            for (int to_r = 0; to_r < (int)solution.routes.size(); to_r++) {
                const Route& to_route = solution.routes[to_r];
                const Vehicle& to_veh = problem.vehicles[to_r];
                
                // Check vehicle preference
                if (emp.vehicle_pref != 0 && to_veh.category != 0 &&
                    emp.vehicle_pref != to_veh.category) {
                    continue;  // Skip incompatible vehicles
                }
                
                // Check capacity (if moving to different route)
                if (to_r != from_r && (int)to_route.employee_sequence.size() >= to_veh.capacity) {
                    continue;  // No room
                }
                
                int max_pos = (int)to_route.employee_sequence.size();
                if (to_r == from_r) max_pos--;  // Adjust for removal
                
                for (int to_pos = 0; to_pos <= max_pos; to_pos++) {
                    // Skip if same position in same route
                    if (to_r == from_r && (to_pos == from_pos || to_pos == from_pos + 1)) {
                        continue;
                    }
                    
                    // Create temporary solution and evaluate
                    Solution temp = copySolution(solution);
                    
                    // Remove from source
                    removeEmployee(temp.routes[from_r], from_pos);
                    
                    // Adjust target position if same route and after removal point
                    int actual_to_pos = to_pos;
                    if (to_r == from_r && to_pos > from_pos) {
                        actual_to_pos--;
                    }
                    
                    // Insert at target
                    insertEmployee(temp.routes[to_r], actual_to_pos, emp_idx);
                    
                    // Evaluate
                    evaluateSolution(temp, problem);
                    
                    double delta = temp.objective_value - solution.objective_value;
                    
                    if (delta < -1e-6 || allow_worsening) {
                        Move move;
                        move.type = MoveType::RELOCATE;
                        move.from_route = from_r;
                        move.from_pos = from_pos;
                        move.to_route = to_r;
                        move.to_pos = to_pos;
                        move.chain_length = 1;
                        move.delta_objective = delta;
                        move.feasible = (temp.hard_violations == 0);
                        moves.push_back(move);
                    }
                }
            }
        }
    }
    
    return moves;
}

/**
 * Apply a relocate move to a solution.
 */
inline void applyRelocateMove(Solution& solution, const Move& move) {
    int emp_idx = removeEmployee(solution.routes[move.from_route], move.from_pos);
    
    int actual_to_pos = move.to_pos;
    if (move.to_route == move.from_route && move.to_pos > move.from_pos) {
        actual_to_pos--;
    }
    
    insertEmployee(solution.routes[move.to_route], actual_to_pos, emp_idx);
}

// ============================================================================
// EXCHANGE (SWAP) OPERATOR
// ============================================================================

/**
 * EXCHANGE: Swap two employees between positions.
 * 
 * Can be:
 * - Intra-route: Swap within same route
 * - Inter-route: Swap between different routes
 */
inline std::vector<Move> generateExchangeMoves(
    const Solution& solution,
    const ProblemInstance& problem,
    bool allow_worsening = false
) {
    std::vector<Move> moves;
    
    for (int r1 = 0; r1 < (int)solution.routes.size(); r1++) {
        const Route& route1 = solution.routes[r1];
        if (route1.empty()) continue;
        
        for (int pos1 = 0; pos1 < (int)route1.employee_sequence.size(); pos1++) {
            int emp1 = route1.employee_sequence[pos1];
            const Employee& e1 = problem.employees[emp1];
            
            // Try swapping with all other positions
            for (int r2 = r1; r2 < (int)solution.routes.size(); r2++) {
                const Route& route2 = solution.routes[r2];
                if (route2.empty()) continue;
                
                int start_pos2 = (r1 == r2) ? pos1 + 1 : 0;
                
                for (int pos2 = start_pos2; pos2 < (int)route2.employee_sequence.size(); pos2++) {
                    int emp2 = route2.employee_sequence[pos2];
                    const Employee& e2 = problem.employees[emp2];
                    
                    // Check vehicle preferences for both employees
                    const Vehicle& v1 = problem.vehicles[r1];
                    const Vehicle& v2 = problem.vehicles[r2];
                    
                    // After swap: emp1 goes to r2, emp2 goes to r1
                    bool e1_fits_r2 = (e1.vehicle_pref == 0 || v2.category == 0 || 
                                       e1.vehicle_pref == v2.category);
                    bool e2_fits_r1 = (e2.vehicle_pref == 0 || v1.category == 0 || 
                                       e2.vehicle_pref == v1.category);
                    
                    if (!e1_fits_r2 || !e2_fits_r1) continue;
                    
                    // Create temporary solution
                    Solution temp = copySolution(solution);
                    
                    // Perform swap
                    temp.routes[r1].employee_sequence[pos1] = emp2;
                    temp.routes[r2].employee_sequence[pos2] = emp1;
                    
                    evaluateSolution(temp, problem);
                    
                    double delta = temp.objective_value - solution.objective_value;
                    
                    if (delta < -1e-6 || allow_worsening) {
                        Move move;
                        move.type = MoveType::EXCHANGE;
                        move.from_route = r1;
                        move.from_pos = pos1;
                        move.to_route = r2;
                        move.to_pos = pos2;
                        move.delta_objective = delta;
                        move.feasible = (temp.hard_violations == 0);
                        moves.push_back(move);
                    }
                }
            }
        }
    }
    
    return moves;
}

/**
 * Apply an exchange move.
 */
inline void applyExchangeMove(Solution& solution, const Move& move) {
    int emp1 = solution.routes[move.from_route].employee_sequence[move.from_pos];
    int emp2 = solution.routes[move.to_route].employee_sequence[move.to_pos];
    
    solution.routes[move.from_route].employee_sequence[move.from_pos] = emp2;
    solution.routes[move.to_route].employee_sequence[move.to_pos] = emp1;
}

// ============================================================================
// 2-OPT OPERATOR
// ============================================================================

/**
 * 2-OPT: Reverse a segment within a route.
 * 
 * Classic TSP improvement operator.
 * Given route: ... A B C D E ...
 * 2-opt on segment [B,D] gives: ... A D C B E ...
 */
inline std::vector<Move> generateTwoOptMoves(
    const Solution& solution,
    const ProblemInstance& problem,
    bool allow_worsening = false
) {
    std::vector<Move> moves;
    
    for (int r = 0; r < (int)solution.routes.size(); r++) {
        const Route& route = solution.routes[r];
        if (route.employee_sequence.size() < 2) continue;
        
        int n = (int)route.employee_sequence.size();
        
        for (int i = 0; i < n - 1; i++) {
            for (int j = i + 1; j < n; j++) {
                // Create temporary solution
                Solution temp = copySolution(solution);
                
                // Reverse segment [i, j]
                std::reverse(
                    temp.routes[r].employee_sequence.begin() + i,
                    temp.routes[r].employee_sequence.begin() + j + 1
                );
                
                evaluateSolution(temp, problem);
                
                double delta = temp.objective_value - solution.objective_value;
                
                if (delta < -1e-6 || allow_worsening) {
                    Move move;
                    move.type = MoveType::TWO_OPT;
                    move.from_route = r;
                    move.from_pos = i;
                    move.to_route = r;
                    move.to_pos = j;
                    move.delta_objective = delta;
                    move.feasible = (temp.hard_violations == 0);
                    moves.push_back(move);
                }
            }
        }
    }
    
    return moves;
}

/**
 * Apply a 2-opt move.
 */
inline void applyTwoOptMove(Solution& solution, const Move& move) {
    std::reverse(
        solution.routes[move.from_route].employee_sequence.begin() + move.from_pos,
        solution.routes[move.from_route].employee_sequence.begin() + move.to_pos + 1
    );
}

// ============================================================================
// OR-OPT OPERATOR
// ============================================================================

/**
 * OR-OPT: Move a chain of consecutive employees to another position.
 * 
 * Chain lengths: 1, 2, or 3 consecutive employees.
 * Can move within same route or to different route.
 */
inline std::vector<Move> generateOrOptMoves(
    const Solution& solution,
    const ProblemInstance& problem,
    int max_chain_length = 3,
    bool allow_worsening = false
) {
    std::vector<Move> moves;
    
    for (int from_r = 0; from_r < (int)solution.routes.size(); from_r++) {
        const Route& from_route = solution.routes[from_r];
        if (from_route.empty()) continue;
        
        for (int chain_len = 1; chain_len <= max_chain_length; chain_len++) {
            for (int from_pos = 0; from_pos + chain_len <= (int)from_route.employee_sequence.size(); from_pos++) {
                
                // Extract the chain
                std::vector<int> chain(
                    from_route.employee_sequence.begin() + from_pos,
                    from_route.employee_sequence.begin() + from_pos + chain_len
                );
                
                // Check vehicle preferences for all employees in chain
                bool chain_feasible = true;
                
                // Try all target positions
                for (int to_r = 0; to_r < (int)solution.routes.size(); to_r++) {
                    const Route& to_route = solution.routes[to_r];
                    const Vehicle& to_veh = problem.vehicles[to_r];
                    
                    // Check if chain fits vehicle preference
                    chain_feasible = true;
                    for (int emp_idx : chain) {
                        const Employee& emp = problem.employees[emp_idx];
                        if (emp.vehicle_pref != 0 && to_veh.category != 0 &&
                            emp.vehicle_pref != to_veh.category) {
                            chain_feasible = false;
                            break;
                        }
                    }
                    if (!chain_feasible) continue;
                    
                    // Check capacity
                    int new_size = (int)to_route.employee_sequence.size();
                    if (to_r != from_r) {
                        new_size += chain_len;
                    }
                    if (new_size > to_veh.capacity) continue;
                    
                    int max_pos = (int)to_route.employee_sequence.size();
                    if (to_r == from_r) max_pos -= chain_len;
                    
                    for (int to_pos = 0; to_pos <= max_pos; to_pos++) {
                        // Skip if same position
                        if (to_r == from_r && 
                            (to_pos >= from_pos && to_pos <= from_pos + chain_len)) {
                            continue;
                        }
                        
                        // Create temporary solution
                        Solution temp = copySolution(solution);
                        
                        // Remove chain from source
                        temp.routes[from_r].employee_sequence.erase(
                            temp.routes[from_r].employee_sequence.begin() + from_pos,
                            temp.routes[from_r].employee_sequence.begin() + from_pos + chain_len
                        );
                        
                        // Adjust target position
                        int actual_to_pos = to_pos;
                        if (to_r == from_r && to_pos > from_pos) {
                            actual_to_pos -= chain_len;
                        }
                        
                        // Insert chain at target
                        temp.routes[to_r].employee_sequence.insert(
                            temp.routes[to_r].employee_sequence.begin() + actual_to_pos,
                            chain.begin(), chain.end()
                        );
                        
                        evaluateSolution(temp, problem);
                        
                        double delta = temp.objective_value - solution.objective_value;
                        
                        if (delta < -1e-6 || allow_worsening) {
                            Move move;
                            move.type = MoveType::OR_OPT;
                            move.from_route = from_r;
                            move.from_pos = from_pos;
                            move.to_route = to_r;
                            move.to_pos = to_pos;
                            move.chain_length = chain_len;
                            move.delta_objective = delta;
                            move.feasible = (temp.hard_violations == 0);
                            moves.push_back(move);
                        }
                    }
                }
            }
        }
    }
    
    return moves;
}

/**
 * Apply an Or-opt move.
 */
inline void applyOrOptMove(Solution& solution, const Move& move) {
    // Extract chain
    std::vector<int> chain(
        solution.routes[move.from_route].employee_sequence.begin() + move.from_pos,
        solution.routes[move.from_route].employee_sequence.begin() + move.from_pos + move.chain_length
    );
    
    // Remove from source
    solution.routes[move.from_route].employee_sequence.erase(
        solution.routes[move.from_route].employee_sequence.begin() + move.from_pos,
        solution.routes[move.from_route].employee_sequence.begin() + move.from_pos + move.chain_length
    );
    
    // Adjust target position
    int actual_to_pos = move.to_pos;
    if (move.to_route == move.from_route && move.to_pos > move.from_pos) {
        actual_to_pos -= move.chain_length;
    }
    
    // Insert at target
    solution.routes[move.to_route].employee_sequence.insert(
        solution.routes[move.to_route].employee_sequence.begin() + actual_to_pos,
        chain.begin(), chain.end()
    );
}

// ============================================================================
// CROSS-EXCHANGE OPERATOR
// ============================================================================

/**
 * CROSS-EXCHANGE: Exchange segments between two routes.
 * 
 * Given routes:
 *   Route 1: A B [C D E] F G
 *   Route 2: P Q [R S] T
 * 
 * Cross-exchange swaps [C D E] and [R S]:
 *   Route 1: A B [R S] F G
 *   Route 2: P Q [C D E] T
 */
inline std::vector<Move> generateCrossExchangeMoves(
    const Solution& solution,
    const ProblemInstance& problem,
    int max_segment_length = 3,
    bool allow_worsening = false
) {
    std::vector<Move> moves;
    
    for (int r1 = 0; r1 < (int)solution.routes.size() - 1; r1++) {
        const Route& route1 = solution.routes[r1];
        
        for (int r2 = r1 + 1; r2 < (int)solution.routes.size(); r2++) {
            const Route& route2 = solution.routes[r2];
            
            // Try all segment combinations
            for (int start1 = 0; start1 < (int)route1.employee_sequence.size(); start1++) {
                for (int len1 = 1; len1 <= max_segment_length && 
                     start1 + len1 <= (int)route1.employee_sequence.size(); len1++) {
                    
                    for (int start2 = 0; start2 < (int)route2.employee_sequence.size(); start2++) {
                        for (int len2 = 1; len2 <= max_segment_length && 
                             start2 + len2 <= (int)route2.employee_sequence.size(); len2++) {
                            
                            // Check capacity after exchange
                            const Vehicle& v1 = problem.vehicles[r1];
                            const Vehicle& v2 = problem.vehicles[r2];
                            
                            int new_size1 = (int)route1.employee_sequence.size() - len1 + len2;
                            int new_size2 = (int)route2.employee_sequence.size() - len2 + len1;
                            
                            if (new_size1 > v1.capacity || new_size2 > v2.capacity) {
                                continue;
                            }
                            
                            // Check vehicle preferences
                            bool feasible = true;
                            
                            // Segment from route1 going to route2
                            for (int i = start1; i < start1 + len1 && feasible; i++) {
                                const Employee& emp = problem.employees[route1.employee_sequence[i]];
                                if (emp.vehicle_pref != 0 && v2.category != 0 &&
                                    emp.vehicle_pref != v2.category) {
                                    feasible = false;
                                }
                            }
                            
                            // Segment from route2 going to route1
                            for (int i = start2; i < start2 + len2 && feasible; i++) {
                                const Employee& emp = problem.employees[route2.employee_sequence[i]];
                                if (emp.vehicle_pref != 0 && v1.category != 0 &&
                                    emp.vehicle_pref != v1.category) {
                                    feasible = false;
                                }
                            }
                            
                            if (!feasible) continue;
                            
                            // Create temporary solution
                            Solution temp = copySolution(solution);
                            
                            // Extract segments
                            std::vector<int> seg1(
                                route1.employee_sequence.begin() + start1,
                                route1.employee_sequence.begin() + start1 + len1
                            );
                            std::vector<int> seg2(
                                route2.employee_sequence.begin() + start2,
                                route2.employee_sequence.begin() + start2 + len2
                            );
                            
                            // Remove segments
                            temp.routes[r1].employee_sequence.erase(
                                temp.routes[r1].employee_sequence.begin() + start1,
                                temp.routes[r1].employee_sequence.begin() + start1 + len1
                            );
                            temp.routes[r2].employee_sequence.erase(
                                temp.routes[r2].employee_sequence.begin() + start2,
                                temp.routes[r2].employee_sequence.begin() + start2 + len2
                            );
                            
                            // Insert swapped segments
                            temp.routes[r1].employee_sequence.insert(
                                temp.routes[r1].employee_sequence.begin() + start1,
                                seg2.begin(), seg2.end()
                            );
                            temp.routes[r2].employee_sequence.insert(
                                temp.routes[r2].employee_sequence.begin() + start2,
                                seg1.begin(), seg1.end()
                            );
                            
                            evaluateSolution(temp, problem);
                            
                            double delta = temp.objective_value - solution.objective_value;
                            
                            if (delta < -1e-6 || allow_worsening) {
                                Move move;
                                move.type = MoveType::CROSS_EXCHANGE;
                                move.from_route = r1;
                                move.from_pos = start1;
                                move.chain_length = len1;
                                move.to_route = r2;
                                move.to_pos = start2;
                                move.swap_pos = len2;  // Using swap_pos for len2
                                move.delta_objective = delta;
                                move.feasible = (temp.hard_violations == 0);
                                moves.push_back(move);
                            }
                        }
                    }
                }
            }
        }
    }
    
    return moves;
}

/**
 * Apply a cross-exchange move.
 */
inline void applyCrossExchangeMove(Solution& solution, const Move& move) {
    int r1 = move.from_route;
    int r2 = move.to_route;
    int start1 = move.from_pos;
    int len1 = move.chain_length;
    int start2 = move.to_pos;
    int len2 = move.swap_pos;
    
    // Extract segments
    std::vector<int> seg1(
        solution.routes[r1].employee_sequence.begin() + start1,
        solution.routes[r1].employee_sequence.begin() + start1 + len1
    );
    std::vector<int> seg2(
        solution.routes[r2].employee_sequence.begin() + start2,
        solution.routes[r2].employee_sequence.begin() + start2 + len2
    );
    
    // Remove segments
    solution.routes[r1].employee_sequence.erase(
        solution.routes[r1].employee_sequence.begin() + start1,
        solution.routes[r1].employee_sequence.begin() + start1 + len1
    );
    solution.routes[r2].employee_sequence.erase(
        solution.routes[r2].employee_sequence.begin() + start2,
        solution.routes[r2].employee_sequence.begin() + start2 + len2
    );
    
    // Insert swapped segments
    solution.routes[r1].employee_sequence.insert(
        solution.routes[r1].employee_sequence.begin() + start1,
        seg2.begin(), seg2.end()
    );
    solution.routes[r2].employee_sequence.insert(
        solution.routes[r2].employee_sequence.begin() + start2,
        seg1.begin(), seg1.end()
    );
}

// ============================================================================
// UNIFIED MOVE APPLICATION
// ============================================================================

/**
 * Apply any move type to a solution.
 */
inline void applyMove(Solution& solution, const Move& move, const ProblemInstance& problem) {
    switch (move.type) {
        case MoveType::RELOCATE:
            applyRelocateMove(solution, move);
            break;
        case MoveType::EXCHANGE:
            applyExchangeMove(solution, move);
            break;
        case MoveType::TWO_OPT:
            applyTwoOptMove(solution, move);
            break;
        case MoveType::OR_OPT:
            applyOrOptMove(solution, move);
            break;
        case MoveType::CROSS_EXCHANGE:
            applyCrossExchangeMove(solution, move);
            break;
        default:
            break;
    }
    
    // Re-evaluate after move
    evaluateSolution(solution, problem);
}

// ============================================================================
// BEST IMPROVEMENT LOCAL SEARCH
// ============================================================================

/**
 * Find the best improving move across all operators.
 */
inline Move findBestMove(
    const Solution& solution,
    const ProblemInstance& problem,
    bool allow_worsening = false
) {
    Move best_move;
    best_move.delta_objective = 0;
    best_move.type = MoveType::RELOCATE;
    bool found = false;
    
    // Generate all moves
    std::vector<Move> all_moves;
    
    auto relocates = generateRelocateMoves(solution, problem, allow_worsening);
    all_moves.insert(all_moves.end(), relocates.begin(), relocates.end());
    
    auto exchanges = generateExchangeMoves(solution, problem, allow_worsening);
    all_moves.insert(all_moves.end(), exchanges.begin(), exchanges.end());
    
    auto two_opts = generateTwoOptMoves(solution, problem, allow_worsening);
    all_moves.insert(all_moves.end(), two_opts.begin(), two_opts.end());
    
    auto or_opts = generateOrOptMoves(solution, problem, 3, allow_worsening);
    all_moves.insert(all_moves.end(), or_opts.begin(), or_opts.end());
    
    auto cross_exchanges = generateCrossExchangeMoves(solution, problem, 2, allow_worsening);
    all_moves.insert(all_moves.end(), cross_exchanges.begin(), cross_exchanges.end());
    
    // Find best
    for (const Move& move : all_moves) {
        if (!found || move.delta_objective < best_move.delta_objective) {
            // Prefer feasible moves
            if (move.feasible || allow_worsening) {
                best_move = move;
                found = true;
            }
        }
    }
    
    best_move.feasible = found;
    return best_move;
}

/**
 * First improvement local search.
 * Returns true if an improvement was made.
 */
inline bool firstImprovement(
    Solution& solution,
    const ProblemInstance& problem
) {
    // Try operators in order until one improves
    
    // Relocate
    auto relocates = generateRelocateMoves(solution, problem, false);
    for (const Move& move : relocates) {
        if (move.delta_objective < -1e-6 && move.feasible) {
            applyMove(solution, move, problem);
            return true;
        }
    }
    
    // Exchange
    auto exchanges = generateExchangeMoves(solution, problem, false);
    for (const Move& move : exchanges) {
        if (move.delta_objective < -1e-6 && move.feasible) {
            applyMove(solution, move, problem);
            return true;
        }
    }
    
    // 2-opt
    auto two_opts = generateTwoOptMoves(solution, problem, false);
    for (const Move& move : two_opts) {
        if (move.delta_objective < -1e-6 && move.feasible) {
            applyMove(solution, move, problem);
            return true;
        }
    }
    
    // Or-opt
    auto or_opts = generateOrOptMoves(solution, problem, 3, false);
    for (const Move& move : or_opts) {
        if (move.delta_objective < -1e-6 && move.feasible) {
            applyMove(solution, move, problem);
            return true;
        }
    }
    
    return false;
}

/**
 * Best improvement local search.
 * Keeps applying best moves until no improvement found.
 */
inline void bestImprovementSearch(
    Solution& solution,
    const ProblemInstance& problem,
    int max_iterations = 1000
) {
    for (int iter = 0; iter < max_iterations; iter++) {
        Move best = findBestMove(solution, problem, false);
        
        if (!best.feasible || best.delta_objective >= -1e-6) {
            break;  // No improving move found
        }
        
        applyMove(solution, best, problem);
    }
}

} // namespace vrp

#endif // VRP_LOCAL_SEARCH_H
