/*
 * Guided Local Search (GLS) Metaheuristic
 * 
 * This is the primary metaheuristic used by Google OR-Tools for VRP.
 * 
 * GLS escapes local optima by augmenting the objective function with penalties
 * for frequently-occurring solution features. When stuck in a local optimum,
 * GLS identifies the most "costly" features and penalizes them, forcing the
 * search to explore different regions.
 * 
 * Key Components:
 * 1. Feature Definition: Arcs (edges) used in routes
 * 2. Utility Calculation: Cost / (1 + penalty_count)
 * 3. Penalty Updates: Penalize features with maximum utility
 * 4. Augmented Objective: Original cost + lambda * sum(penalties)
 * 
 * Reference: Voudouris, C., & Tsang, E. (1999). Guided local search.
 */

#ifndef VRP_GLS_H
#define VRP_GLS_H

#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_local_search.h"
#include "vrp_construction.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

namespace vrp {

// ============================================================================
// GLS CONFIGURATION
// ============================================================================

struct GLSConfig {
    double lambda = 0.2;              // Penalty weight coefficient
    double penalty_factor = 0.1;      // How much to penalize per iteration
    int max_iterations = 1000;        // Maximum GLS iterations
    int max_iterations_without_improvement = 100;  // Early termination
    int local_search_depth = 100;     // Max local search iterations per GLS iter
    double time_limit_seconds = 60.0; // Time limit
    bool verbose = false;             // Print progress
};

// ============================================================================
// GLS FEATURE REPRESENTATION
// ============================================================================

/**
 * Feature in GLS = an arc (edge) used in the solution.
 * 
 * We represent arcs as pairs of (from_id, to_id) where:
 * - from_id can be depot (special index) or employee
 * - to_id can be employee or depot
 */
struct GLSFeature {
    int from_node;  // -1 for depot
    int to_node;    // -1 for depot
    int vehicle;    // Which vehicle uses this arc
    
    bool operator==(const GLSFeature& other) const {
        return from_node == other.from_node && 
               to_node == other.to_node && 
               vehicle == other.vehicle;
    }
};

struct GLSFeatureHash {
    size_t operator()(const GLSFeature& f) const {
        return std::hash<int>()(f.from_node) ^ 
               (std::hash<int>()(f.to_node) << 16) ^
               (std::hash<int>()(f.vehicle) << 24);
    }
};

// ============================================================================
// GLS STATE
// ============================================================================

struct GLSState {
    // Penalty counts for each feature
    std::unordered_map<GLSFeature, double, GLSFeatureHash> penalties;
    
    // Configuration
    GLSConfig config;
    
    // Statistics
    int total_iterations = 0;
    int iterations_since_improvement = 0;
    double best_objective = INF;
    
    // Lambda (auto-calibrated)
    double lambda = 0.2;
};

// ============================================================================
// GLS CORE FUNCTIONS
// ============================================================================

/**
 * Extract features (arcs) from a solution.
 */
inline std::vector<GLSFeature> extractFeatures(
    const Solution& solution,
    const ProblemInstance& problem
) {
    std::vector<GLSFeature> features;
    
    for (int v = 0; v < (int)solution.routes.size(); v++) {
        const Route& route = solution.routes[v];
        if (route.empty()) continue;
        
        // Depot -> first employee
        GLSFeature f1;
        f1.from_node = -1;  // Depot
        f1.to_node = route.employee_sequence[0];
        f1.vehicle = v;
        features.push_back(f1);
        
        // Employee -> Employee arcs
        for (int i = 0; i < (int)route.employee_sequence.size() - 1; i++) {
            GLSFeature f;
            f.from_node = route.employee_sequence[i];
            f.to_node = route.employee_sequence[i + 1];
            f.vehicle = v;
            features.push_back(f);
        }
        
        // Last employee -> Depot
        GLSFeature f2;
        f2.from_node = route.employee_sequence.back();
        f2.to_node = -1;  // Depot
        f2.vehicle = v;
        features.push_back(f2);
    }
    
    return features;
}

/**
 * Calculate the cost of a feature (arc).
 */
inline double featureCost(
    const GLSFeature& feature,
    const ProblemInstance& problem
) {
    double lat1, lon1, lat2, lon2;
    
    if (feature.from_node == -1) {
        // From depot
        lat1 = problem.depot_lat;
        lon1 = problem.depot_lon;
    } else {
        lat1 = problem.employees[feature.from_node].pickup_lat;
        lon1 = problem.employees[feature.from_node].pickup_lon;
    }
    
    if (feature.to_node == -1) {
        // To depot
        lat2 = problem.depot_lat;
        lon2 = problem.depot_lon;
    } else {
        lat2 = problem.employees[feature.to_node].pickup_lat;
        lon2 = problem.employees[feature.to_node].pickup_lon;
    }
    
    double distance = haversine(lat1, lon1, lat2, lon2);
    double cost_per_km = problem.vehicles[feature.vehicle].cost_per_km;
    
    return distance * cost_per_km;
}

/**
 * Calculate utility of a feature.
 * Utility = cost / (1 + penalty_count)
 * Higher utility features are penalized first.
 */
inline double featureUtility(
    const GLSFeature& feature,
    const GLSState& state,
    const ProblemInstance& problem
) {
    double cost = featureCost(feature, problem);
    double penalty = 0.0;
    
    auto it = state.penalties.find(feature);
    if (it != state.penalties.end()) {
        penalty = it->second;
    }
    
    return cost / (1.0 + penalty);
}

/**
 * Calculate augmented objective (original objective + penalties).
 */
inline double augmentedObjective(
    const Solution& solution,
    const GLSState& state,
    const ProblemInstance& problem
) {
    double base = solution.objective_value;
    double penalty_sum = 0.0;
    
    auto features = extractFeatures(solution, problem);
    for (const GLSFeature& f : features) {
        auto it = state.penalties.find(f);
        if (it != state.penalties.end()) {
            penalty_sum += it->second * featureCost(f, problem);
        }
    }
    
    return base + state.lambda * penalty_sum;
}

/**
 * Update penalties based on current local optimum.
 * Penalize features with maximum utility.
 */
inline void updatePenalties(
    const Solution& solution,
    GLSState& state,
    const ProblemInstance& problem
) {
    auto features = extractFeatures(solution, problem);
    
    if (features.empty()) return;
    
    // Find maximum utility
    double max_utility = -INF;
    for (const GLSFeature& f : features) {
        double util = featureUtility(f, state, problem);
        max_utility = std::max(max_utility, util);
    }
    
    // Penalize all features with maximum utility (or close to it)
    double threshold = max_utility * 0.99;  // Within 1% of max
    
    for (const GLSFeature& f : features) {
        double util = featureUtility(f, state, problem);
        if (util >= threshold) {
            state.penalties[f] += state.config.penalty_factor;
        }
    }
}

/**
 * Auto-calibrate lambda based on solution characteristics.
 * Lambda should be proportional to solution cost / number of features.
 */
inline void calibrateLambda(
    const Solution& solution,
    GLSState& state,
    const ProblemInstance& problem
) {
    auto features = extractFeatures(solution, problem);
    
    if (features.empty()) {
        state.lambda = state.config.lambda;
        return;
    }
    
    double total_cost = 0.0;
    for (const GLSFeature& f : features) {
        total_cost += featureCost(f, problem);
    }
    
    // Lambda = alpha * average_feature_cost
    double avg_cost = total_cost / features.size();
    state.lambda = state.config.lambda * avg_cost;
}

// ============================================================================
// GLS-AWARE LOCAL SEARCH
// ============================================================================

/**
 * Find best move using augmented objective.
 */
inline Move findBestMoveGLS(
    const Solution& solution,
    const GLSState& state,
    const ProblemInstance& problem
) {
    Move best_move;
    best_move.delta_objective = 0;
    best_move.feasible = false;
    
    double current_aug = augmentedObjective(solution, state, problem);
    double best_aug = current_aug;
    
    // Generate all moves
    auto relocates = generateRelocateMoves(solution, problem, true);
    auto exchanges = generateExchangeMoves(solution, problem, true);
    auto two_opts = generateTwoOptMoves(solution, problem, true);
    auto or_opts = generateOrOptMoves(solution, problem, 3, true);
    
    std::vector<Move> all_moves;
    all_moves.insert(all_moves.end(), relocates.begin(), relocates.end());
    all_moves.insert(all_moves.end(), exchanges.begin(), exchanges.end());
    all_moves.insert(all_moves.end(), two_opts.begin(), two_opts.end());
    all_moves.insert(all_moves.end(), or_opts.begin(), or_opts.end());
    
    for (const Move& move : all_moves) {
        // Apply move temporarily
        Solution temp = copySolution(solution);
        applyMove(temp, move, problem);
        
        // Calculate augmented objective
        double aug = augmentedObjective(temp, state, problem);
        
        if (aug < best_aug - 1e-6) {
            best_aug = aug;
            best_move = move;
            best_move.delta_objective = aug - current_aug;
            best_move.feasible = true;
        }
    }
    
    return best_move;
}

/**
 * Local search using augmented objective.
 * Continues until local optimum with respect to augmented objective.
 */
inline void localSearchGLS(
    Solution& solution,
    const GLSState& state,
    const ProblemInstance& problem,
    int max_iterations
) {
    for (int iter = 0; iter < max_iterations; iter++) {
        Move best = findBestMoveGLS(solution, state, problem);
        
        if (!best.feasible || best.delta_objective >= -1e-6) {
            break;  // Local optimum reached
        }
        
        applyMove(solution, best, problem);
    }
}

// ============================================================================
// MAIN GLS ALGORITHM
// ============================================================================

/**
 * Guided Local Search main algorithm.
 * 
 * Algorithm:
 * 1. Start with initial solution
 * 2. Apply local search to reach local optimum
 * 3. If best solution found, record it
 * 4. Update penalties for current local optimum
 * 5. Apply local search with augmented objective
 * 6. Repeat until termination condition
 */
inline Solution guidedLocalSearch(
    const ProblemInstance& problem,
    const GLSConfig& config = GLSConfig()
) {
    GLSState state;
    state.config = config;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Build initial solution using multi-start construction
    Solution current = multiStartConstruction(problem);
    evaluateSolution(current, problem);
    
    // Apply initial local search
    bestImprovementSearch(current, problem, config.local_search_depth);
    
    // Initialize best solution
    Solution best = copySolution(current);
    state.best_objective = best.objective_value;
    
    // Calibrate lambda
    calibrateLambda(current, state, problem);
    
    if (config.verbose) {
        std::cout << "GLS Initial solution: " << best.objective_value << std::endl;
        std::cout << "Lambda calibrated to: " << state.lambda << std::endl;
    }
    
    // Main GLS loop
    for (int iter = 0; iter < config.max_iterations; iter++) {
        // Check time limit
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        if (elapsed > config.time_limit_seconds) {
            if (config.verbose) {
                std::cout << "GLS: Time limit reached at iteration " << iter << std::endl;
            }
            break;
        }
        
        // Check iterations without improvement
        if (state.iterations_since_improvement > config.max_iterations_without_improvement) {
            if (config.verbose) {
                std::cout << "GLS: No improvement for " << state.iterations_since_improvement 
                          << " iterations" << std::endl;
            }
            break;
        }
        
        // Update penalties for current local optimum
        updatePenalties(current, state, problem);
        
        // Local search with augmented objective
        localSearchGLS(current, state, problem, config.local_search_depth);
        
        // Check if new best found (using original objective)
        if (current.objective_value < state.best_objective - 1e-6) {
            state.best_objective = current.objective_value;
            best = copySolution(current);
            state.iterations_since_improvement = 0;
            
            if (config.verbose) {
                std::cout << "GLS Iteration " << iter 
                          << ": New best = " << state.best_objective << std::endl;
            }
        } else {
            state.iterations_since_improvement++;
        }
        
        state.total_iterations = iter + 1;
    }
    
    // Final local search on best solution (without penalties)
    bestImprovementSearch(best, problem, config.local_search_depth);
    
    if (config.verbose) {
        std::cout << "GLS Final solution: " << best.objective_value << std::endl;
        std::cout << "Total iterations: " << state.total_iterations << std::endl;
    }
    
    return best;
}

// ============================================================================
// ITERATED LOCAL SEARCH (ILS) - Alternative metaheuristic
// ============================================================================

/**
 * Perturbation for ILS: Make random moves to escape local optimum.
 */
inline void perturbSolution(
    Solution& solution,
    const ProblemInstance& problem,
    int strength = 3
) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (int i = 0; i < strength; i++) {
        // Random relocate move
        std::vector<int> non_empty_routes;
        for (int r = 0; r < (int)solution.routes.size(); r++) {
            if (!solution.routes[r].empty()) {
                non_empty_routes.push_back(r);
            }
        }
        
        if (non_empty_routes.empty()) return;
        
        // Pick random source
        int src_r = non_empty_routes[gen() % non_empty_routes.size()];
        int src_pos = gen() % solution.routes[src_r].employee_sequence.size();
        
        // Pick random destination
        int dst_r = gen() % solution.routes.size();
        int dst_pos = gen() % (solution.routes[dst_r].employee_sequence.size() + 1);
        
        // Skip if same position
        if (src_r == dst_r && (dst_pos == src_pos || dst_pos == src_pos + 1)) {
            continue;
        }
        
        // Check capacity
        if (src_r != dst_r && 
            (int)solution.routes[dst_r].employee_sequence.size() >= 
            problem.vehicles[dst_r].capacity) {
            continue;
        }
        
        // Apply move
        int emp = removeEmployee(solution.routes[src_r], src_pos);
        if (src_r == dst_r && dst_pos > src_pos) dst_pos--;
        insertEmployee(solution.routes[dst_r], dst_pos, emp);
    }
    
    evaluateSolution(solution, problem);
}

/**
 * Iterated Local Search.
 */
inline Solution iteratedLocalSearch(
    const ProblemInstance& problem,
    int max_iterations = 1000,
    int perturbation_strength = 3,
    double time_limit_seconds = 60.0
) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Build initial solution
    Solution current = multiStartConstruction(problem);
    evaluateSolution(current, problem);
    bestImprovementSearch(current, problem, 100);
    
    Solution best = copySolution(current);
    
    int iterations_without_improvement = 0;
    
    for (int iter = 0; iter < max_iterations; iter++) {
        // Check time limit
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        if (elapsed > time_limit_seconds) break;
        
        // Check stagnation
        if (iterations_without_improvement > 100) break;
        
        // Perturb
        Solution candidate = copySolution(current);
        perturbSolution(candidate, problem, perturbation_strength);
        
        // Local search
        bestImprovementSearch(candidate, problem, 100);
        
        // Acceptance criterion
        if (candidate.objective_value < current.objective_value - 1e-6) {
            current = copySolution(candidate);
            iterations_without_improvement = 0;
            
            if (current.objective_value < best.objective_value - 1e-6) {
                best = copySolution(current);
            }
        } else {
            iterations_without_improvement++;
            
            // Accept with small probability (diversification)
            std::random_device rd;
            std::mt19937 gen(rd());
            if (gen() % 10 == 0) {
                current = copySolution(candidate);
            }
        }
    }
    
    return best;
}

// ============================================================================
// COMBINED SOLVER
// ============================================================================

/**
 * Solve VRP using combination of GLS and ILS.
 * Runs both metaheuristics and returns best solution.
 */
inline Solution solveVRP(
    const ProblemInstance& problem,
    double time_limit_seconds = 60.0,
    bool verbose = false
) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Split time between methods
    double gls_time = time_limit_seconds * 0.7;
    double ils_time = time_limit_seconds * 0.3;
    
    // Run GLS
    GLSConfig gls_config;
    gls_config.time_limit_seconds = gls_time;
    gls_config.verbose = verbose;
    
    Solution gls_solution = guidedLocalSearch(problem, gls_config);
    
    if (verbose) {
        std::cout << "GLS solution cost: " << gls_solution.total_cost << std::endl;
    }
    
    // Check remaining time
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time).count();
    double remaining = time_limit_seconds - elapsed;
    
    if (remaining > 5.0) {
        // Run ILS with remaining time
        Solution ils_solution = iteratedLocalSearch(problem, 1000, 3, remaining);
        
        if (verbose) {
            std::cout << "ILS solution cost: " << ils_solution.total_cost << std::endl;
        }
        
        // Return best
        if (ils_solution.objective_value < gls_solution.objective_value) {
            return ils_solution;
        }
    }
    
    return gls_solution;
}

} // namespace vrp

#endif // VRP_GLS_H
