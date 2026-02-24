#ifndef VRP_CONFIG_H
#define VRP_CONFIG_H

#include "json.hpp"
#include <fstream>
#include <iostream>
#include <string>

using json = nlohmann::json;

// All tunable solver parameters in one place.
// Loaded from solver_config.json if present, otherwise uses defaults.
struct SolverConfig {
    // Penalty weights (ALNS internal scoring)
    double unassigned_penalty = 100000.0;
    double time_violation_penalty = 100000.0;
    double lateness_per_min_penalty = 1000.0;
    double priority_lateness_multiplier = 500.0;
    double pref_violation_penalty = 10000.0;
    double vehicle_activation_cost = 50.0;

    // Objective weights (output score)
    double cost_weight = 0.7;
    double time_weight = 0.3;

    // Priority lateness weights
    double priority_weight_P1 = 5.0;
    double priority_weight_P2 = 3.0;
    double priority_weight_P3 = 2.0;
    double priority_weight_P4 = 1.0;
    double priority_weight_P5 = 1.0;

    // SA parameters
    double start_temperature = 50000.0;
    double cooling_rate = 0.99995;
    double min_destroy_pct = 0.10;
    double max_destroy_pct = 0.60;
    int reheat_threshold = 800;
    int max_reheats = 20;

    // Construction
    int num_candidates_to_optimize = 3;
    int time_split_pct[3] = {50, 30, 20};

    // Consolidation
    double detour_ratio_threshold = 1.5;
    double detour_penalty_multiplier = 10.0;

    // Solver
    int default_time_limit = 30;
    bool verbose = true;

    // Load from JSON file. Returns true if file was found and parsed.
    bool load(const std::string& path = "solver_config.json") {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cout << "[CONFIG] No " << path << " found, using defaults\n";
            return false;
        }

        try {
            json j;
            f >> j;

            // Penalty weights
            if (j.contains("penalty_weights")) {
                auto& pw = j["penalty_weights"];
                if (pw.contains("unassigned_employee_penalty"))
                    unassigned_penalty = pw["unassigned_employee_penalty"].get<double>();
                if (pw.contains("time_violation_penalty"))
                    time_violation_penalty = pw["time_violation_penalty"].get<double>();
                if (pw.contains("lateness_per_minute_penalty"))
                    lateness_per_min_penalty = pw["lateness_per_minute_penalty"].get<double>();
                if (pw.contains("priority_lateness_multiplier"))
                    priority_lateness_multiplier = pw["priority_lateness_multiplier"].get<double>();
                if (pw.contains("preference_violation_penalty"))
                    pref_violation_penalty = pw["preference_violation_penalty"].get<double>();
                if (pw.contains("vehicle_activation_cost"))
                    vehicle_activation_cost = pw["vehicle_activation_cost"].get<double>();
            }

            // Objective weights
            if (j.contains("objective_weights")) {
                auto& ow = j["objective_weights"];
                if (ow.contains("cost_weight"))
                    cost_weight = ow["cost_weight"].get<double>();
                if (ow.contains("time_weight"))
                    time_weight = ow["time_weight"].get<double>();
            }

            // Priority weights
            if (j.contains("priority_lateness_weights")) {
                auto& plw = j["priority_lateness_weights"];
                if (plw.contains("P1")) priority_weight_P1 = plw["P1"].get<double>();
                if (plw.contains("P2")) priority_weight_P2 = plw["P2"].get<double>();
                if (plw.contains("P3")) priority_weight_P3 = plw["P3"].get<double>();
                if (plw.contains("P4")) priority_weight_P4 = plw["P4"].get<double>();
                if (plw.contains("P5")) priority_weight_P5 = plw["P5"].get<double>();
            }

            // SA parameters
            if (j.contains("sa_parameters")) {
                auto& sa = j["sa_parameters"];
                if (sa.contains("start_temperature"))
                    start_temperature = sa["start_temperature"].get<double>();
                if (sa.contains("cooling_rate"))
                    cooling_rate = sa["cooling_rate"].get<double>();
                if (sa.contains("min_destroy_percent"))
                    min_destroy_pct = sa["min_destroy_percent"].get<double>();
                if (sa.contains("max_destroy_percent"))
                    max_destroy_pct = sa["max_destroy_percent"].get<double>();
                if (sa.contains("reheat_threshold_iters"))
                    reheat_threshold = sa["reheat_threshold_iters"].get<int>();
                if (sa.contains("max_reheats"))
                    max_reheats = sa["max_reheats"].get<int>();
            }

            // Construction
            if (j.contains("construction")) {
                auto& ct = j["construction"];
                if (ct.contains("num_candidates_to_optimize"))
                    num_candidates_to_optimize = ct["num_candidates_to_optimize"].get<int>();
                if (ct.contains("time_split_percent") && ct["time_split_percent"].is_array()) {
                    auto& ts = ct["time_split_percent"];
                    for (int i = 0; i < 3 && i < (int)ts.size(); i++)
                        time_split_pct[i] = ts[i].get<int>();
                }
            }

            // Consolidation
            if (j.contains("consolidation")) {
                auto& co = j["consolidation"];
                if (co.contains("detour_ratio_threshold"))
                    detour_ratio_threshold = co["detour_ratio_threshold"].get<double>();
                if (co.contains("detour_penalty_multiplier"))
                    detour_penalty_multiplier = co["detour_penalty_multiplier"].get<double>();
            }

            // Solver
            if (j.contains("solver")) {
                auto& sv = j["solver"];
                if (sv.contains("default_time_limit_seconds"))
                    default_time_limit = sv["default_time_limit_seconds"].get<int>();
                if (sv.contains("verbose"))
                    verbose = sv["verbose"].get<bool>();
            }

            std::cout << "[CONFIG] Loaded from " << path << "\n";
            return true;

        } catch (const std::exception& e) {
            std::cerr << "[CONFIG] Error parsing " << path << ": " << e.what() << "\n";
            std::cerr << "[CONFIG] Using defaults\n";
            return false;
        }
    }

    void print_summary() const {
        std::cout << "[CONFIG] Penalties: unassigned=" << unassigned_penalty
                  << " time_viol=" << time_violation_penalty
                  << " late/min=" << lateness_per_min_penalty
                  << " pref_viol=" << pref_violation_penalty << "\n";
        std::cout << "[CONFIG] Objective: cost_w=" << cost_weight
                  << " time_w=" << time_weight << "\n";
        std::cout << "[CONFIG] SA: temp=" << start_temperature
                  << " cool=" << cooling_rate
                  << " destroy=" << min_destroy_pct << "-" << max_destroy_pct
                  << " reheats=" << max_reheats << "\n";
    }
};

// Global config instance
inline SolverConfig g_config;

#endif
