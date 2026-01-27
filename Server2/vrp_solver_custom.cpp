#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_parser.h"
#include "vrp_constraints.h"
#include "vrp_construction.h"
#include "vrp_validators.h"
#include "vrp_local_search.h"
#include "vrp_gls.h"
#include "vrp_output.h"
#include <iostream>
#include <fstream>

std::vector<std::vector<double>> dist_matrix;
int OFFICE_NODE = 0;

Solution solve_stage(bool enforce_soft, const std::vector<Employee>& emps,
                     const std::vector<Vehicle>& phys, const std::vector<Vehicle>& virt,
                     const Metadata& meta, int stage, int time_limit) {
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "STAGE " << stage << ": " 
              << (enforce_soft ? "ALL CONSTRAINTS (MULTI-TRIP)" : "HARD ONLY (MULTI-TRIP)") << "\n";
    std::cout << std::string(70, '=') << std::endl;
    
    // Use all virtual vehicles (multi-trip) for both stages
    std::vector<Vehicle> active_virt = virt;
    std::cout << "Using " << active_virt.size() << " virtual vehicles (multi-trip mode)\n";
    
    ConstraintEngine cp;
    cp.setup(enforce_soft, emps, active_virt, meta);
    
    std::vector<std::vector<int>> routes;
    ParallelCheapestInsertion::build(routes, emps, active_virt, cp, enforce_soft, meta);
    
    GuidedLocalSearch gls(dist_matrix.size());
    gls.set_constraint_engine(&cp);
    std::vector<std::vector<int>> best;
    double best_cost = 0;
    gls.optimize(routes, best, best_cost, active_virt, emps, meta, enforce_soft, time_limit);
    
    // Debug: Print routes
    std::cout << "\nRoutes from GLS:\n";
    for (size_t v = 0; v < best.size(); v++) {
        if (!best[v].empty()) {
            std::cout << "  V" << (v/3)+1 << " Trip" << (v%3)+1 << " (" << active_virt[v].vehicle_id << "): ";
            for (int e : best[v]) std::cout << emps[e].employee_id << " ";
            std::cout << "(start: " << active_virt[v].available_from << " min)\n";
        }
    }
    
    std::string sol_type = enforce_soft ? "STAGE_1_ALL_CONSTRAINTS" : "STAGE_2_HARD_ONLY";
    Solution sol = OutputFormatter::format(best, active_virt, phys, emps, meta, enforce_soft, sol_type);
    
    std::cout << "\nStage " << stage << " Results:\n";
    std::cout << "   GLS Optimization Cost: $" << best_cost << "\n";
    std::cout << "   Output Formatter Cost: $" << sol.total_cost << "\n";
    std::cout << "   Time: " << sol.total_time << " min\n";
    std::cout << "   Hard violations: " << sol.hard_violations << "\n";
    std::cout << "   Soft violations: " << sol.soft_violations << "\n";
    std::cout << "   Score: " << sol.score << std::endl;
    
    return sol;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json> [output_json] [time_limit_seconds]" << std::endl;
        return 1;
    }
    
    std::string output_file = (argc >= 3) ? argv[2] : "output/vrp_solution_custom.json";
    int time_limit = (argc >= 4) ? std::atoi(argv[3]) : 10;
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "CUSTOM VRP SOLVER - CP + GLS\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Input: " << argv[1] << std::endl;
    std::cout << "Output: " << output_file << std::endl;
    std::cout << "Time Limit: " << time_limit << " seconds" << std::endl;
    
    std::vector<Employee> emps;
    std::vector<Vehicle> phys, virt;
    Metadata meta;
    
    if (!VRPParser::load(argv[1], emps, phys, virt, meta, dist_matrix, OFFICE_NODE)) {
        std::cerr << "Failed to load input" << std::endl;
        return 1;
    }
    
    Solution best;
    best.hard_violations = best.soft_violations = 999999;
    
    // Stage 1: ALL constraints
    try {
        Solution s1 = solve_stage(true, emps, phys, virt, meta, 1, time_limit);
        if (s1.hard_violations == 0 && s1.soft_violations == 0) {
            std::cout << "\nSTAGE 1 SUCCESS: OPTIMAL solution\n";
            s1.solution_type = "OPTIMAL - No violations";
            best = s1;
            
            json out = OutputFormatter::to_json(best);
            std::ofstream f(output_file);
            f << out.dump(2);
            f.close();
            std::cout << "\nSaved: " << output_file << "\n\n" << out.dump(2) << std::endl;
            return 0;
        }
        best = s1;
    } catch (const std::exception& e) {
        std::cerr << "⚠️ Stage 1 error: " << e.what() << std::endl;
    }
    
    // Stage 2: HARD constraints only (allows soft violations)
    try {
        Solution s2 = solve_stage(false, emps, phys, virt, meta, 2, time_limit);
        
        // Compare solutions - PRIORITY ORDER:
        // 1. Fewer hard violations
        // 2. Fewer soft violations
        // 3. Lower cost
        bool s2_better = false;
        
        if (s2.hard_violations < best.hard_violations) {
            s2_better = true;
        } else if (s2.hard_violations == best.hard_violations) {
            if (s2.soft_violations < best.soft_violations) {
                s2_better = true;
            } else if (s2.soft_violations == best.soft_violations && s2.total_cost < best.total_cost) {
                s2_better = true;
            }
        }
        
        if (s2_better) {
            if (s2.hard_violations == 0 && s2.soft_violations == 0) {
                s2.solution_type = "OPTIMAL - No violations";
            } else if (s2.hard_violations == 0) {
                s2.solution_type = "FEASIBLE - " + std::to_string(s2.soft_violations) + " soft violations";
            } else {
                s2.solution_type = "INFEASIBLE - " + std::to_string(s2.hard_violations) + " hard, " + std::to_string(s2.soft_violations) + " soft";
            }
            best = s2;
            std::cout << "\nStage 2 solution is BETTER\n";
        } else {
            std::cout << "\nStage 1 solution is BETTER (fewer violations)\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Stage 2 error: " << e.what() << std::endl;
    }
    
    // Output best
    if (best.hard_violations < 999999) {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "FINAL: " << best.solution_type << "\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "Cost: $" << best.total_cost << "\n";
        std::cout << "Time: " << best.total_time << " min\n";
        std::cout << "Hard: " << best.hard_violations << ", Soft: " << best.soft_violations << "\n";
        std::cout << "Score: " << best.score << std::endl;
        
        json out = OutputFormatter::to_json(best);
        std::ofstream f(output_file);
        f << out.dump(2);
        f.close();
        
        std::cout << "\nSaved: " << output_file << "\n\n" << out.dump(2) << std::endl;
        return 0;
    } else {
        std::cerr << "\nNo solution found" << std::endl;
        return 1;
    }
}
