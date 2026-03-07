#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_parser.h"
#include "vrp_constraints.h"
#include "vrp_construction.h"
#include "vrp_single_trip_construction.h"
#include "vrp_savings_construction.h"
#include "vrp_validators.h"
#include "vrp_local_search.h"
#include "vrp_alns.h"
#include "vrp_output.h"
#include "vrp_simulation.h"
#include "vrp_config.h"
#include <iostream>
#include <fstream>
#include <chrono>

std::vector<std::vector<double>> dist_matrix;
int OFFICE_NODE = 0;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json> [output_json] [time_limit_seconds]" << std::endl;
        return 1;
    }
    
    std::string output_file = (argc >= 3) ? argv[2] : "output/vrp_solution_custom.json";
    int time_limit = (argc >= 4) ? std::atoi(argv[3]) : 30;
    
    // Load configuration
    g_config.load("solver_config.json");
    if (g_config.verbose) g_config.print_summary();
    
    auto global_start = std::chrono::high_resolution_clock::now();
    auto elapsed_sec = [&]() -> int {
        auto now = std::chrono::high_resolution_clock::now();
        return (int)std::chrono::duration_cast<std::chrono::seconds>(now - global_start).count();
    };
    auto time_left = [&]() -> int {
        return std::max(0, time_limit - elapsed_sec());
    };
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "CUSTOM VRP SOLVER - UNIFIED OPTIMIZATION\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Input: " << argv[1] << std::endl;
    std::cout << "Output: " << output_file << std::endl;
    std::cout << "Time Limit: " << time_limit << " seconds" << std::endl;
    
    // ============================================================
    // LOAD DATA
    // ============================================================
    std::vector<Employee> emps;
    std::vector<Vehicle> phys, virt;
    Metadata meta;
    
    if (!VRPParser::load(argv[1], emps, phys, virt, meta, dist_matrix, OFFICE_NODE)) {
        std::cerr << "Failed to load input" << std::endl;
        return 1;
    }
    
    // Use objective weights from the input JSON (test case specifies the competition weights)
    // Only fall back to config weights if input JSON didn't provide them
    if (meta.cost_weight == 0.7 && meta.time_weight == 0.3) {
        // Input provided default 0.7/0.3 — use as-is (competition standard)
    } else {
        // Input specified custom weights — use those
    }
    std::cout << "Objective weights: cost=" << meta.cost_weight << " time=" << meta.time_weight << std::endl;
    
    std::cout << "Loaded in " << elapsed_sec() << "s\n";
    
    // ============================================================
    // BUILD NEIGHBOR LIST (shared across all strategies)
    // ============================================================
    NeighborList nlist;
    nlist.K = g_config.neighbor_K;
    nlist.build(emps, dist_matrix, OFFICE_NODE);
    nlist.build_direct_distances(emps, dist_matrix, OFFICE_NODE);
    
    // ============================================================
    // CONSTRAINT ENGINES
    // cp_soft: for construction with preference enforcement
    // cp_hard: for relaxed construction (more capacity flexibility)
    // ALNS internally uses penalty-based (all violations allowed but penalized)
    // ============================================================
    ConstraintEngine cp_soft, cp_hard;
    cp_soft.setup(true, emps, virt, meta);
    cp_hard.setup(false, emps, virt, meta);
    
    // ============================================================
    // PHASE 1: Quick construction of diverse initial solutions
    // ============================================================
    
    std::vector<OrderingStrategy> strategies = {
        EARLIEST_DEADLINE,
        GEOGRAPHIC_CLUSTER,
        DOLLAR_COST_AWARE,
        PREFERENCE_PRIORITY,
        PRIORITY_BASED,
        GEO_CLUSTER_CONSOL,
        DEPOT_PROXIMITY,  // Uses more vehicles by assigning to nearest depot
        ANGULAR_SECTOR    // Radial sweep partitioning for geographic diversity
    };
    
    struct Candidate {
        std::vector<std::vector<int>> routes;
        double initial_score;
        int hard_v;
        int soft_v;
        std::string name;
    };
    std::vector<Candidate> candidates;
    
    std::cout << "\n" << std::string(70, '-') << "\n";
    std::cout << "PHASE 1: Building initial solutions\n";
    std::cout << std::string(70, '-') << "\n";
    
    // A: Single-trip construction (preference-aware)
    {
        std::vector<std::vector<int>> routes;
        SingleTripConstruction::build(routes, emps, virt, cp_soft, true, meta);
        Solution sol = OutputFormatter::format(routes, virt, phys, emps, meta, true, "init");
        candidates.push_back({routes, sol.score, sol.hard_violations, sol.soft_violations, "SingleTrip"});
        std::cout << "  SingleTrip:       score=" << sol.score << " hard=" << sol.hard_violations << " soft=" << sol.soft_violations << "\n";
    }
    
    // B-G: Parallel cheapest insertion with various orderings
    // Alternate soft/hard enforcement for diversity
    for (size_t i = 0; i < strategies.size(); i++) {
        bool use_soft = (i % 2 == 0);
        ConstraintEngine& cp_use = use_soft ? cp_soft : cp_hard;
        
        std::vector<std::vector<int>> routes;
        ParallelCheapestInsertion::build(routes, emps, virt, cp_use, use_soft, meta, strategies[i]);
        Solution sol = OutputFormatter::format(routes, virt, phys, emps, meta, true, "init");
        
        std::string name;
        switch(strategies[i]) {
            case EARLIEST_DEADLINE:   name = "EarliestDeadline"; break;
            case GEOGRAPHIC_CLUSTER:  name = "GeoCluster"; break;
            case DOLLAR_COST_AWARE:   name = "DollarCost"; break;
            case PREFERENCE_PRIORITY: name = "PrefPriority"; break;
            case PRIORITY_BASED:      name = "PriorityBased"; break;
            case GEO_CLUSTER_CONSOL:  name = "GeoConsol"; break;
            case DEPOT_PROXIMITY:     name = "DepotProx"; break;
            case ANGULAR_SECTOR:     name = "AngularSector"; break;
            default:                  name = "Strategy_" + std::to_string(i); break;
        }
        if (!use_soft) name += "(relaxed)";
        
        candidates.push_back({routes, sol.score, sol.hard_violations, sol.soft_violations, name});
        std::cout << "  " << name << ": score=" << sol.score << " hard=" << sol.hard_violations << " soft=" << sol.soft_violations << "\n";
    }
    
    // H: Clarke-Wright Savings (relaxed)
    {
        std::vector<std::vector<int>> routes;
        SavingsConstruction::build(routes, emps, virt, cp_hard, false, meta, nlist);
        Solution sol = OutputFormatter::format(routes, virt, phys, emps, meta, true, "init");
        candidates.push_back({routes, sol.score, sol.hard_violations, sol.soft_violations, "Savings"});
        std::cout << "  Savings:          score=" << sol.score << " hard=" << sol.hard_violations << " soft=" << sol.soft_violations << "\n";
    }
    
    std::cout << "  Built " << candidates.size() << " candidates in " << elapsed_sec() << "s\n";
    
    // ============================================================
    // RANK: fewer hard > fewer soft > lower score
    // ============================================================
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.hard_v != b.hard_v) return a.hard_v < b.hard_v;
        if (a.soft_v != b.soft_v) return a.soft_v < b.soft_v;
        return a.initial_score < b.initial_score;
    });
    
    std::cout << "\n  Ranked:\n";
    for (size_t i = 0; i < candidates.size(); i++) {
        std::cout << "    #" << (i+1) << " " << candidates[i].name 
                  << " (hard=" << candidates[i].hard_v << " soft=" << candidates[i].soft_v 
                  << " score=" << candidates[i].initial_score << ")\n";
    }
    
    // ============================================================
    // PHASE 2: ALNS Optimization on top candidates
    // Best candidate: 50% time, 2nd: 30%, 3rd: 20%
    // ============================================================
    
    int remaining = time_left();
    std::cout << "\n" << std::string(70, '-') << "\n";
    std::cout << "PHASE 2: ALNS Optimization (" << remaining << "s remaining)\n";
    std::cout << std::string(70, '-') << "\n";
    
    Solution best_sol;
    best_sol.hard_violations = 999999;
    best_sol.soft_violations = 999999;
    best_sol.total_cost = 1e9;
    best_sol.score = 1e18;
    std::vector<std::vector<int>> final_best_routes;  // Track best routes for rich output
    
    int num_opt = std::min(g_config.num_candidates_to_optimize, (int)candidates.size());
    if (remaining < 15 && num_opt > 2) num_opt = 2;
    if (remaining < 6 && num_opt > 1) num_opt = 1;
    
    for (int ci = 0; ci < num_opt; ci++) {
        int alns_time;
        if (ci == num_opt - 1) {
            alns_time = std::max(2, time_left());  // Last one gets all remaining
        } else {
            alns_time = std::max(2, time_left() * g_config.time_split_pct[ci] / 100);
        }
        
        if (time_left() <= 1) {
            std::cout << "  Time exhausted, skipping remaining candidates\n";
            break;
        }
        
        std::cout << "\n--- #" << (ci+1) << ": " << candidates[ci].name 
                  << " (" << alns_time << "s) ---\n";
        
        AdaptiveLargeNeighborhoodSearch alns;
        alns.set_constraint_engine(&cp_soft);
        alns.set_neighbor_list(&nlist);
        
        std::vector<std::vector<int>> best_routes;
        double best_cost = 0;
        // enforce_soft=true passed to ALNS, but ALNS internally uses penalty-based
        alns.optimize(candidates[ci].routes, best_routes, best_cost, virt, emps, meta, true, alns_time);
        
        Solution sol = OutputFormatter::format(best_routes, virt, phys, emps, meta, true, "OPTIMIZED");
        
        std::cout << "  Result: cost=$" << sol.total_cost 
                  << " time=" << sol.total_time << "min"
                  << " hard=" << sol.hard_violations 
                  << " soft=" << sol.soft_violations 
                  << " score=" << sol.score << "\n";
        
        if (is_solution_better(sol.hard_violations, sol.soft_violations, sol.score,
                                best_sol.hard_violations, best_sol.soft_violations, best_sol.score)) {
            best_sol = sol;
            final_best_routes = best_routes;
            std::cout << "  >> New best!\n";
        }
    }
    
    // ============================================================
    // SOLUTION TYPE
    // ============================================================
    if (best_sol.hard_violations == 0 && best_sol.soft_violations == 0) {
        best_sol.solution_type = "OPTIMAL - No violations";
    } else if (best_sol.hard_violations == 0) {
        best_sol.solution_type = "FEASIBLE - " + std::to_string(best_sol.soft_violations) + " soft violations";
    } else {
        best_sol.solution_type = std::to_string(best_sol.hard_violations) + " hard, " 
                                + std::to_string(best_sol.soft_violations) + " soft violations";
    }
    
    // ============================================================
    // OUTPUT — Rich JSON with per-employee details & baseline costs
    // ============================================================
    if (best_sol.hard_violations < 999999) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "FINAL: " << best_sol.solution_type << "\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "Cost: $" << best_sol.total_cost << "\n";
        std::cout << "Time: " << best_sol.total_time << " min\n";
        std::cout << "Hard: " << best_sol.hard_violations << ", Soft: " << best_sol.soft_violations << "\n";
        std::cout << "Score: " << best_sol.score << "\n";
        std::cout << "Solver time: " << elapsed_sec() << "s / " << time_limit << "s\n";
        
        // Generate full simulation result for rich output
        SimulationResult final_sim = simulate_full_solution(final_best_routes, virt, phys, emps, meta);
        
        // Compute and display baseline comparison
        BaselineCost bc = compute_baseline_cost(emps, best_sol.total_cost);
        std::cout << "\nBaseline Comparison:\n";
        std::cout << "  Ola estimate:    $" << bc.ola_total << "\n";
        std::cout << "  Uber estimate:   $" << bc.uber_total << "\n";
        std::cout << "  Rapido estimate: $" << bc.rapido_total << "\n";
        std::cout << "  Market average:  $" << bc.average_total << "\n";
        std::cout << "  Optimized cost:  $" << bc.optimized_cost << "\n";
        std::cout << "  Savings:         " << bc.savings_percent << "% ($" << bc.savings_absolute << ")\n";
        
        json out = OutputFormatter::to_json_rich(best_sol, emps, &final_sim);
        std::ofstream f(output_file);
        if (!f.is_open()) {
            std::cerr << "Error: Cannot write to " << output_file << std::endl;
            return 1;
        }
        f << out.dump(2);
        f.close();
        if (f.fail()) {
            std::cerr << "Error: Write failed for " << output_file << std::endl;
            return 1;
        }
        
        std::cout << "\nSaved: " << output_file << std::endl;
        return 0;
    } else {
        std::cerr << "\nNo solution found" << std::endl;
        return 1;
    }
}
