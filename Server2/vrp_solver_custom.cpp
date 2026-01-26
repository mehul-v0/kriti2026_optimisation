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
                     const Metadata& meta, int stage) {
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "STAGE " << stage << ": " 
              << (enforce_soft ? "ALL CONSTRAINTS" : "HARD ONLY") << "\n";
    std::cout << std::string(70, '=') << std::endl;
    
    ConstraintEngine cp;
    cp.setup(enforce_soft, emps, virt, meta);
    
    std::vector<std::vector<int>> routes;
    ParallelCheapestInsertion::build(routes, emps, virt, cp, enforce_soft);
    
    GuidedLocalSearch gls(dist_matrix.size());
    std::vector<std::vector<int>> best;
    double best_cost = 0;
    gls.optimize(routes, best, best_cost, virt, emps, meta, enforce_soft, 30);
    
    std::string sol_type = enforce_soft ? "STAGE_1_ALL_CONSTRAINTS" : "STAGE_2_HARD_ONLY";
    Solution sol = OutputFormatter::format(best, virt, phys, emps, meta, enforce_soft, sol_type);
    
    std::cout << "\nStage " << stage << " Results:\n";
    std::cout << "   Cost: $" << sol.total_cost << "\n";
    std::cout << "   Time: " << sol.total_time << " min\n";
    std::cout << "   Hard violations: " << sol.hard_violations << "\n";
    std::cout << "   Soft violations: " << sol.soft_violations << "\n";
    std::cout << "   Score: " << sol.score << std::endl;
    
    return sol;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json>" << std::endl;
        return 1;
    }
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "CUSTOM VRP SOLVER - CP + GLS\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Input: " << argv[1] << std::endl;
    
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
        Solution s1 = solve_stage(true, emps, phys, virt, meta, 1);
        if (s1.hard_violations == 0 && s1.soft_violations == 0) {
            std::cout << "\nSTAGE 1 SUCCESS: OPTIMAL solution\n";
            s1.solution_type = "OPTIMAL - No violations";
            best = s1;
            
            json out = OutputFormatter::to_json(best);
            std::ofstream f("vrp_solution_custom.json");
            f << out.dump(2);
            f.close();
            std::cout << "\nSaved: vrp_solution_custom.json\n\n" << out.dump(2) << std::endl;
            return 0;
        }
        best = s1;
    } catch (const std::exception& e) {
        std::cerr << "⚠️ Stage 1 error: " << e.what() << std::endl;
    }
    
    // Stage 2: HARD constraints only
    try {
        Solution s2 = solve_stage(false, emps, phys, virt, meta, 2);
        if (s2.hard_violations == 0) {
            std::cout << "\nSTAGE 2 SUCCESS: FEASIBLE solution\n";
            s2.solution_type = "FEASIBLE - Soft violations only";
            best = s2;
        } else if (s2.hard_violations < best.hard_violations) {
            best = s2;
        }
    } catch (const std::exception& e) {
        std::cerr << "⚠️ Stage 2 error: " << e.what() << std::endl;
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
        std::ofstream f("vrp_solution_custom.json");
        f << out.dump(2);
        f.close();
        
        std::cout << "\n✓ Saved: vrp_solution_custom.json\n\n" << out.dump(2) << std::endl;
        return 0;
    } else {
        std::cerr << "\nNo solution found" << std::endl;
        return 1;
    }
}
