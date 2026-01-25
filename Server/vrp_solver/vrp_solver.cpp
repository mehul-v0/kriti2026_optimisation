/*
 * VRP Solver - Main Entry Point
 * 
 * Complete C++ implementation of the algorithms used by Google OR-Tools:
 * 1. PARALLEL_CHEAPEST_INSERTION - Construction heuristic
 * 2. GUIDED_LOCAL_SEARCH - Metaheuristic for improvement
 * 
 * This solver replicates OR-Tools functionality from scratch in pure C++.
 * 
 * Usage:
 *   ./vrp_solver input.txt [output.json] [time_limit_seconds]
 * 
 * Compilation:
 *   g++ -std=c++17 -O3 -o vrp_solver vrp_solver.cpp
 */

#include "vrp_types.h"
#include "vrp_utils.h"
#include "vrp_construction.h"
#include "vrp_local_search.h"
#include "vrp_gls.h"
#include "vrp_parser.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <string>

using namespace vrp;

// ============================================================================
// COMMAND LINE INTERFACE
// ============================================================================

void printUsage(const char* program) {
    std::cout << "VRP Solver - Pure C++ Implementation of OR-Tools Algorithms\n";
    std::cout << "\nUsage: " << program << " <input_file> [output_file] [time_limit]\n";
    std::cout << "\nArguments:\n";
    std::cout << "  input_file    : Path to input file (tab-separated)\n";
    std::cout << "  output_file   : Path to output JSON file (optional, default: stdout)\n";
    std::cout << "  time_limit    : Time limit in seconds (optional, default: 60)\n";
    std::cout << "\nInput file format:\n";
    std::cout << "  Line 1: num_employees num_vehicles\n";
    std::cout << "  Next num_employees lines: employee data\n";
    std::cout << "  Next num_vehicles lines: vehicle data\n";
    std::cout << "  Last line: depot_lat depot_lon\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << program << " input.txt output.json 30\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = "";
    double time_limit = 60.0;
    
    if (argc >= 3) {
        output_file = argv[2];
    }
    
    if (argc >= 4) {
        try {
            time_limit = std::stod(argv[3]);
        } catch (...) {
            std::cerr << "Invalid time limit: " << argv[3] << std::endl;
            return 1;
        }
    }
    
    // Start timer
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Parse input
        std::cout << "Parsing input file: " << input_file << std::endl;
        ProblemInstance problem = parseInputFile(input_file);
        
        std::cout << "Problem loaded:" << std::endl;
        std::cout << "  Employees: " << problem.employees.size() << std::endl;
        std::cout << "  Vehicles: " << problem.vehicles.size() << std::endl;
        std::cout << "  Depot: (" << problem.depot_lat << ", " << problem.depot_lon << ")" << std::endl;
        
        // Solve using GLS (main OR-Tools algorithm)
        std::cout << "\nSolving with Guided Local Search (time limit: " 
                  << time_limit << "s)..." << std::endl;
        
        Solution solution = solveVRP(problem, time_limit, true);
        
        // Calculate elapsed time
        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        // Print solution
        printSolution(solution, problem, true);
        
        std::cout << "Solved in " << elapsed << " seconds" << std::endl;
        
        // Output JSON
        std::string json_output = solutionToJson(solution, problem);
        
        if (output_file.empty()) {
            // Print to stdout
            std::cout << "\n=== JSON Output ===" << std::endl;
            std::cout << json_output << std::endl;
        } else {
            // Write to file
            std::ofstream out(output_file);
            if (!out.is_open()) {
                std::cerr << "Cannot open output file: " << output_file << std::endl;
                return 1;
            }
            out << json_output;
            out.close();
            std::cout << "Output written to: " << output_file << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
