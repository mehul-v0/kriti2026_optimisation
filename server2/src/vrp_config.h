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
    double lateness_per_min_penalty = 5.0;
    double priority_lateness_multiplier = 500.0;
    double pref_violation_penalty = 20.0;
    double vehicle_activation_cost = 0.0;  // Don't penalize using available vehicles

    // Objective weights (output score)
    double cost_weight = 0.7;
    double time_weight = 0.3;

    // Priority lateness weights
    double priority_weight_P1 = 5.0;
    double priority_weight_P2 = 3.0;
    double priority_weight_P3 = 2.0;
    double priority_weight_P4 = 1.0;
    double priority_weight_P5 = 1.0;

    // Local search weighted_violation_score weights
    long long ls_hard_violation_weight = 10000000LL;
    long long ls_pref_violation_weight = 20LL;
    long long ls_lateness_weight = 1000LL;

    // SA parameters
    double start_temperature = 50000.0;
    double cooling_rate = 0.99995;
    double min_destroy_pct = 0.10;
    double max_destroy_pct = 0.60;
    int reheat_threshold = 800;
    int max_reheats = 20;
    double reheat_factor = 0.4;
    double adaptive_temp_scale = 0.5;

    // ALNS operator weight management
    double sigma1 = 33.0;
    double sigma2 = 9.0;
    double sigma3 = 3.0;
    double decay_factor = 0.8;
    double weight_update_scale = 10.0;
    double min_operator_weight = 0.1;
    int weight_update_interval = 500;

    // Initial destroy operator weights (indexed by DestroyOperator enum order)
    double init_destroy_weights[11] = {1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.5, 2.5, 2.0, 1.5, 3.0};
    // Initial repair operator weights (indexed by RepairOperator enum order)
    // Regret insertion boosted to 4x (Parragh 2011: dominates across DR-PBS categories)
    double init_repair_weights[5] = {1.0, 4.0, 1.0, 1.0, 2.0};

    // Per-priority max ride time in minutes (DARP constraint, Parragh constraint 18, Portell constraint 11)
    int max_ride_time_P1 = 30;
    int max_ride_time_P2 = 45;
    int max_ride_time_P3 = 60;
    int max_ride_time_P4 = 75;
    int max_ride_time_P5 = 90;
    double ride_time_violation_penalty = 5000.0;

    // Waiting-with-passengers penalty weight theta (Parragh 2011, Objective Function 1)
    double wait_with_pax_penalty = 0.0;  // Quality metric only, not part of cost+time objective

    // Priority-weighted ride time in objective (Portell Section 4.3)
    double priority_ride_time_weight = 0.0;  // Not part of cost+time objective

    // Shaw removal relatedness coefficients
    double shaw_time_coeff = 0.01;
    double shaw_cost_coeff = 0.005;

    // GLS parameters
    double gls_lambda_coeff = 0.1;
    int gls_update_interval = 200;
    double gls_penalty_decay_on_reheat = 0.5;
    double gls_edge_penalty_increment = 1.0;

    // Adaptive destroy sizing
    int stagnation_threshold_iters = 500;
    double stagnation_min_destroy = 0.30;
    double stagnation_max_destroy = 0.65;
    int improving_threshold_iters = 50;
    double improving_min_destroy = 0.08;
    double improving_max_destroy = 0.35;
    double restart_destroy_fraction = 0.333;
    int perturbation_swaps_min = 2;
    int perturbation_swaps_range = 2;
    int route_removal_max_routes = 2;
    int consolidation_removal_max_vehicles = 2;

    // Vehicle elimination
    double vehicle_elim_random_pick_prob = 0.3;

    // String removal
    double string_cross_vehicle_discount = 0.5;

    // Local search pass limits
    int intra_route_max_passes = 3;
    int inter_route_max_passes = 3;
    int cross_exchange_max_passes = 2;
    int cross_exchange_frequency = 5;
    int post_alns_max_passes = 150;
    int final_reopt_max_passes = 50;
    int or_opt_max_segment_length = 3;
    int exact_tsp_max_route_size = 6;
    double violation_to_cost_factor = 1000.0;
    int consolidation_max_route_size = 4;
    int consolidation_max_orderings = 10;

    // Repair parameters
    double new_trip_penalty = 25.0;
    double time_penalty_multiplier = 0.1;
    int regret_k = 3;

    // Construction
    int num_candidates_to_optimize = 5;
    int time_split_pct[5] = {35, 25, 20, 12, 8};
    int trip_time_estimate_minutes = 30;
    int min_trips_per_vehicle = 4;
    int trips_buffer = 2;
    double infeasible_pref_penalty = 50000.0;
    double infeasible_trip_idx_weight = 10000.0;
    double infeasible_earlier_trip_bonus = 5000.0;
    double infeasible_lateness_coeff = 10.0;

    // Constraints
    int min_trip_time_minutes = 30;

    // Consolidation
    double detour_ratio_threshold = 1.5;
    double detour_penalty_multiplier = 10.0;

    // Neighbor list
    int neighbor_K = 10;

    // Solution pool
    int max_pool_size = 5;
    int lahc_history_length = 500;

    // Solver
    int default_time_limit = 30;
    bool verbose = true;

    // Helper: get priority weight by priority level (1-5)
    double get_priority_weight(int priority) const {
        switch (priority) {
            case 1: return priority_weight_P1;
            case 2: return priority_weight_P2;
            case 3: return priority_weight_P3;
            case 4: return priority_weight_P4;
            case 5: return priority_weight_P5;
            default: return priority_weight_P5;
        }
    }

    // Helper: get max ride time for a priority level (DARP constraint)
    int get_max_ride_time(int priority) const {
        switch (priority) {
            case 1: return max_ride_time_P1;
            case 2: return max_ride_time_P2;
            case 3: return max_ride_time_P3;
            case 4: return max_ride_time_P4;
            case 5: return max_ride_time_P5;
            default: return max_ride_time_P5;
        }
    }

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
                if (sa.contains("reheat_factor"))
                    reheat_factor = sa["reheat_factor"].get<double>();
                if (sa.contains("adaptive_temp_scale"))
                    adaptive_temp_scale = sa["adaptive_temp_scale"].get<double>();
            }

            // Local search weights
            if (j.contains("local_search_weights")) {
                auto& lw = j["local_search_weights"];
                if (lw.contains("hard_violation_weight"))
                    ls_hard_violation_weight = lw["hard_violation_weight"].get<long long>();
                if (lw.contains("pref_violation_weight"))
                    ls_pref_violation_weight = lw["pref_violation_weight"].get<long long>();
                if (lw.contains("lateness_weight"))
                    ls_lateness_weight = lw["lateness_weight"].get<long long>();
            }

            // ALNS operator weights
            if (j.contains("alns_operator_weights")) {
                auto& aw = j["alns_operator_weights"];
                if (aw.contains("sigma1_new_best"))
                    sigma1 = aw["sigma1_new_best"].get<double>();
                if (aw.contains("sigma2_improve_current"))
                    sigma2 = aw["sigma2_improve_current"].get<double>();
                if (aw.contains("sigma3_accepted_worse"))
                    sigma3 = aw["sigma3_accepted_worse"].get<double>();
                if (aw.contains("decay_factor"))
                    decay_factor = aw["decay_factor"].get<double>();
                if (aw.contains("weight_update_scale"))
                    weight_update_scale = aw["weight_update_scale"].get<double>();
                if (aw.contains("min_operator_weight"))
                    min_operator_weight = aw["min_operator_weight"].get<double>();
                if (aw.contains("weight_update_interval"))
                    weight_update_interval = aw["weight_update_interval"].get<int>();
                if (aw.contains("initial_destroy_weights")) {
                    auto& dw = aw["initial_destroy_weights"];
                    if (dw.contains("random_removal")) init_destroy_weights[0] = dw["random_removal"].get<double>();
                    if (dw.contains("worst_removal")) init_destroy_weights[1] = dw["worst_removal"].get<double>();
                    if (dw.contains("shaw_removal")) init_destroy_weights[2] = dw["shaw_removal"].get<double>();
                    if (dw.contains("route_removal")) init_destroy_weights[3] = dw["route_removal"].get<double>();
                    if (dw.contains("violation_removal")) init_destroy_weights[4] = dw["violation_removal"].get<double>();
                    if (dw.contains("consolidation_removal")) init_destroy_weights[5] = dw["consolidation_removal"].get<double>();
                    if (dw.contains("cross_vehicle_removal")) init_destroy_weights[6] = dw["cross_vehicle_removal"].get<double>();
                    if (dw.contains("vehicle_elimination")) init_destroy_weights[7] = dw["vehicle_elimination"].get<double>();
                    if (dw.contains("expensive_arc_removal")) init_destroy_weights[8] = dw["expensive_arc_removal"].get<double>();
                    if (dw.contains("string_removal")) init_destroy_weights[9] = dw["string_removal"].get<double>();
                    if (dw.contains("lateness_targeted_removal")) init_destroy_weights[10] = dw["lateness_targeted_removal"].get<double>();
                }
                if (aw.contains("initial_repair_weights")) {
                    auto& rw = aw["initial_repair_weights"];
                    if (rw.contains("greedy_insertion")) init_repair_weights[0] = rw["greedy_insertion"].get<double>();
                    if (rw.contains("regret_insertion")) init_repair_weights[1] = rw["regret_insertion"].get<double>();
                    if (rw.contains("nearest_insertion")) init_repair_weights[2] = rw["nearest_insertion"].get<double>();
                    if (rw.contains("batching_insertion")) init_repair_weights[3] = rw["batching_insertion"].get<double>();
                    if (rw.contains("cheapest_vehicle_insertion")) init_repair_weights[4] = rw["cheapest_vehicle_insertion"].get<double>();
                }
            }

            // Shaw removal
            if (j.contains("shaw_removal")) {
                auto& sr = j["shaw_removal"];
                if (sr.contains("time_similarity_coeff"))
                    shaw_time_coeff = sr["time_similarity_coeff"].get<double>();
                if (sr.contains("cost_similarity_coeff"))
                    shaw_cost_coeff = sr["cost_similarity_coeff"].get<double>();
            }

            // GLS parameters
            if (j.contains("gls_parameters")) {
                auto& gls = j["gls_parameters"];
                if (gls.contains("lambda_coefficient"))
                    gls_lambda_coeff = gls["lambda_coefficient"].get<double>();
                if (gls.contains("update_interval"))
                    gls_update_interval = gls["update_interval"].get<int>();
                if (gls.contains("penalty_decay_on_reheat"))
                    gls_penalty_decay_on_reheat = gls["penalty_decay_on_reheat"].get<double>();
                if (gls.contains("edge_penalty_increment"))
                    gls_edge_penalty_increment = gls["edge_penalty_increment"].get<double>();
            }

            // Adaptive destroy
            if (j.contains("adaptive_destroy")) {
                auto& ad = j["adaptive_destroy"];
                if (ad.contains("stagnation_threshold_iters"))
                    stagnation_threshold_iters = ad["stagnation_threshold_iters"].get<int>();
                if (ad.contains("stagnation_min_destroy"))
                    stagnation_min_destroy = ad["stagnation_min_destroy"].get<double>();
                if (ad.contains("stagnation_max_destroy"))
                    stagnation_max_destroy = ad["stagnation_max_destroy"].get<double>();
                if (ad.contains("improving_threshold_iters"))
                    improving_threshold_iters = ad["improving_threshold_iters"].get<int>();
                if (ad.contains("improving_min_destroy"))
                    improving_min_destroy = ad["improving_min_destroy"].get<double>();
                if (ad.contains("improving_max_destroy"))
                    improving_max_destroy = ad["improving_max_destroy"].get<double>();
                if (ad.contains("restart_destroy_fraction"))
                    restart_destroy_fraction = ad["restart_destroy_fraction"].get<double>();
                if (ad.contains("perturbation_swaps_min"))
                    perturbation_swaps_min = ad["perturbation_swaps_min"].get<int>();
                if (ad.contains("perturbation_swaps_range"))
                    perturbation_swaps_range = ad["perturbation_swaps_range"].get<int>();
                if (ad.contains("route_removal_max_routes"))
                    route_removal_max_routes = ad["route_removal_max_routes"].get<int>();
                if (ad.contains("consolidation_removal_max_vehicles"))
                    consolidation_removal_max_vehicles = ad["consolidation_removal_max_vehicles"].get<int>();
            }

            // Vehicle elimination
            if (j.contains("vehicle_elimination")) {
                auto& ve = j["vehicle_elimination"];
                if (ve.contains("random_pick_probability"))
                    vehicle_elim_random_pick_prob = ve["random_pick_probability"].get<double>();
            }

            // String removal
            if (j.contains("string_removal")) {
                auto& sr2 = j["string_removal"];
                if (sr2.contains("cross_vehicle_discount"))
                    string_cross_vehicle_discount = sr2["cross_vehicle_discount"].get<double>();
            }

            // Local search passes
            if (j.contains("local_search_passes")) {
                auto& lsp = j["local_search_passes"];
                if (lsp.contains("intra_route_max_passes"))
                    intra_route_max_passes = lsp["intra_route_max_passes"].get<int>();
                if (lsp.contains("inter_route_max_passes"))
                    inter_route_max_passes = lsp["inter_route_max_passes"].get<int>();
                if (lsp.contains("cross_exchange_max_passes"))
                    cross_exchange_max_passes = lsp["cross_exchange_max_passes"].get<int>();
                if (lsp.contains("post_alns_max_passes"))
                    post_alns_max_passes = lsp["post_alns_max_passes"].get<int>();
                if (lsp.contains("final_reopt_max_passes"))
                    final_reopt_max_passes = lsp["final_reopt_max_passes"].get<int>();
                if (lsp.contains("cross_exchange_frequency"))
                    cross_exchange_frequency = lsp["cross_exchange_frequency"].get<int>();
                if (lsp.contains("or_opt_max_segment_length"))
                    or_opt_max_segment_length = lsp["or_opt_max_segment_length"].get<int>();
                if (lsp.contains("exact_tsp_max_route_size"))
                    exact_tsp_max_route_size = lsp["exact_tsp_max_route_size"].get<int>();
                if (lsp.contains("violation_to_cost_factor"))
                    violation_to_cost_factor = lsp["violation_to_cost_factor"].get<double>();
                if (lsp.contains("consolidation_max_route_size"))
                    consolidation_max_route_size = lsp["consolidation_max_route_size"].get<int>();
                if (lsp.contains("consolidation_max_orderings"))
                    consolidation_max_orderings = lsp["consolidation_max_orderings"].get<int>();
            }

            // Repair parameters
            if (j.contains("repair_parameters")) {
                auto& rp = j["repair_parameters"];
                if (rp.contains("new_trip_penalty"))
                    new_trip_penalty = rp["new_trip_penalty"].get<double>();
                if (rp.contains("time_penalty_multiplier"))
                    time_penalty_multiplier = rp["time_penalty_multiplier"].get<double>();
                if (rp.contains("regret_k"))
                    regret_k = rp["regret_k"].get<int>();
            }

            // Construction
            if (j.contains("construction")) {
                auto& ct = j["construction"];
                if (ct.contains("num_candidates_to_optimize"))
                    num_candidates_to_optimize = ct["num_candidates_to_optimize"].get<int>();
                if (ct.contains("time_split_percent") && ct["time_split_percent"].is_array()) {
                    auto& ts = ct["time_split_percent"];
                    for (int i = 0; i < 5 && i < (int)ts.size(); i++)
                        time_split_pct[i] = ts[i].get<int>();
                }
                if (ct.contains("trip_time_estimate_minutes"))
                    trip_time_estimate_minutes = ct["trip_time_estimate_minutes"].get<int>();
                if (ct.contains("min_trips_per_vehicle"))
                    min_trips_per_vehicle = ct["min_trips_per_vehicle"].get<int>();
                if (ct.contains("trips_buffer"))
                    trips_buffer = ct["trips_buffer"].get<int>();
                if (ct.contains("infeasible_pref_penalty"))
                    infeasible_pref_penalty = ct["infeasible_pref_penalty"].get<double>();
                if (ct.contains("infeasible_trip_idx_weight"))
                    infeasible_trip_idx_weight = ct["infeasible_trip_idx_weight"].get<double>();
                if (ct.contains("infeasible_earlier_trip_bonus"))
                    infeasible_earlier_trip_bonus = ct["infeasible_earlier_trip_bonus"].get<double>();
                if (ct.contains("infeasible_lateness_coeff"))
                    infeasible_lateness_coeff = ct["infeasible_lateness_coeff"].get<double>();
            }

            // Constraints
            if (j.contains("constraints")) {
                auto& cn = j["constraints"];
                if (cn.contains("min_trip_time_minutes"))
                    min_trip_time_minutes = cn["min_trip_time_minutes"].get<int>();
            }

            // Consolidation
            if (j.contains("consolidation")) {
                auto& co = j["consolidation"];
                if (co.contains("detour_ratio_threshold"))
                    detour_ratio_threshold = co["detour_ratio_threshold"].get<double>();
                if (co.contains("detour_penalty_multiplier"))
                    detour_penalty_multiplier = co["detour_penalty_multiplier"].get<double>();
            }

            // Neighbor list
            if (j.contains("neighbor_list")) {
                auto& nl = j["neighbor_list"];
                if (nl.contains("K"))
                    neighbor_K = nl["K"].get<int>();
            }

            // Solution pool
            if (j.contains("solution_pool")) {
                auto& sp = j["solution_pool"];
                if (sp.contains("max_pool_size"))
                    max_pool_size = sp["max_pool_size"].get<int>();
                if (sp.contains("lahc_history_length"))
                    lahc_history_length = sp["lahc_history_length"].get<int>();
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
        std::cout << "[CONFIG] LS weights: hard=" << ls_hard_violation_weight
                  << " pref=" << ls_pref_violation_weight
                  << " lateness=" << ls_lateness_weight << "\n";
        std::cout << "[CONFIG] Objective: cost_w=" << cost_weight
                  << " time_w=" << time_weight << "\n";
        std::cout << "[CONFIG] SA: temp=" << start_temperature
                  << " cool=" << cooling_rate
                  << " destroy=" << min_destroy_pct << "-" << max_destroy_pct
                  << " reheats=" << max_reheats << "\n";
        std::cout << "[CONFIG] ALNS: sigma1=" << sigma1
                  << " sigma2=" << sigma2
                  << " sigma3=" << sigma3
                  << " decay=" << decay_factor << "\n";
    }
};

// Global config instance
inline SolverConfig g_config;

#endif
