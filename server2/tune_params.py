#!/usr/bin/env python3
"""
Parameter Tuning Script for VRP Solver (Phase 5B)
Based on Şahin et al. 2022 (LBRanker): data-driven feature weighting.

Performs grid search / random search over key solver_config.json parameters,
evaluating each configuration against a set of test cases, and selecting
the parameter set that minimizes total score across all test cases.

Usage:
    python tune_params.py                     # Run with defaults
    python tune_params.py --cases output/input_tc01.json output/input_1771881446.json
    python tune_params.py --trials 50 --time-limit 15
    python tune_params.py --method grid       # Exhaustive grid search (slow)
"""

import argparse
import json
import os
import subprocess
import sys
import time
import random
import itertools
import copy
from pathlib import Path

# ===========================================================================
# DEFAULT CONFIG: base solver_config.json that gets modified per trial
# ===========================================================================
SOLVER_EXE = "vrp_solver.exe"
BASE_CONFIG = "solver_config.json"
TUNED_CONFIG = "solver_config_tuned.json"
OUTPUT_DIR = "output"

# The parameters to tune and their search ranges
# Format: (json_path, min_val, max_val, step)
# json_path is dot-separated: "penalty_weights.lateness_per_minute_penalty"
PARAM_SPACE = {
    "penalty_weights.lateness_per_minute_penalty": {
        "values": [5.0, 10.0, 20.0, 50.0, 100.0],
        "description": "Penalty per minute of lateness"
    },
    "penalty_weights.preference_violation_penalty": {
        "values": [50.0, 100.0, 200.0, 500.0, 1000.0],
        "description": "Penalty for preference violations"
    },
    "penalty_weights.vehicle_activation_cost": {
        "values": [0.0, 50.0, 100.0, 200.0],
        "description": "Cost per active vehicle (encourages consolidation)"
    },
    "sa_parameters.cooling_rate": {
        "values": [0.99990, 0.99995, 0.99998],
        "description": "SA cooling rate (higher = more exploration)"
    },
    "sa_parameters.start_temperature": {
        "values": [10000.0, 50000.0, 100000.0],
        "description": "SA starting temperature"
    },
    "alns_operator_weights.initial_repair_weights.regret_insertion": {
        "values": [1.0, 2.0, 4.0, 6.0],
        "description": "Regret insertion initial weight (Parragh recommends dominant)"
    },
    "priority_lateness_weights.P1": {
        "values": [5.0, 10.0, 20.0],
        "description": "Priority 1 lateness weight"
    },
    "gls_parameters.lambda_coefficient": {
        "values": [0.05, 0.1, 0.2],
        "description": "GLS penalty scaling factor"
    },
}


def set_nested(d, path, value):
    """Set a nested dictionary value using dot-separated path."""
    keys = path.split(".")
    for key in keys[:-1]:
        d = d.setdefault(key, {})
    d[keys[-1]] = value


def get_nested(d, path, default=None):
    """Get a nested dictionary value using dot-separated path."""
    keys = path.split(".")
    for key in keys:
        if isinstance(d, dict) and key in d:
            d = d[key]
        else:
            return default
    return d


def load_base_config():
    """Load the base solver_config.json."""
    if os.path.exists(BASE_CONFIG):
        with open(BASE_CONFIG, "r") as f:
            return json.load(f)
    return {}


def run_solver(input_file, config, time_limit=15):
    """
    Run the VRP solver with a given config and return the output score.
    Returns (score, hard_violations, lateness) or None on failure.
    """
    # Write temp config
    temp_config = "_tune_temp_config.json"
    with open(temp_config, "w") as f:
        json.dump(config, f, indent=2)

    try:
        result = subprocess.run(
            [f"./{SOLVER_EXE}", input_file, str(time_limit)],
            capture_output=True,
            text=True,
            timeout=time_limit + 30,  # extra grace period
            cwd=os.getcwd(),
            env={**os.environ, "SOLVER_CONFIG": temp_config}
        )

        # Parse output JSON from stdout
        # The solver prints the result JSON to a file in output/
        # Find the most recent output file
        output_files = sorted(
            Path(OUTPUT_DIR).glob("output_*.json"),
            key=lambda p: p.stat().st_mtime,
            reverse=True
        )

        if output_files:
            with open(output_files[0], "r") as f:
                output = json.load(f)

            score = output.get("score", float("inf"))
            hard_v = output.get("hard_violations", 999)
            lateness = output.get("total_lateness_minutes", 9999)

            # Composite metric: prioritize feasibility, then lateness, then cost
            composite = hard_v * 1000000 + lateness * 1000 + score
            return {
                "score": score,
                "hard_violations": hard_v,
                "lateness": lateness,
                "composite": composite
            }
    except subprocess.TimeoutExpired:
        print(f"  [TIMEOUT] {input_file}")
    except Exception as e:
        print(f"  [ERROR] {input_file}: {e}")
    finally:
        if os.path.exists(temp_config):
            os.remove(temp_config)

    return None


def evaluate_config(config, test_cases, time_limit):
    """Evaluate a config across all test cases. Returns total composite score."""
    total_composite = 0
    total_score = 0
    total_hard = 0
    total_lateness = 0
    n_success = 0

    for tc in test_cases:
        result = run_solver(tc, config, time_limit)
        if result:
            total_composite += result["composite"]
            total_score += result["score"]
            total_hard += result["hard_violations"]
            total_lateness += result["lateness"]
            n_success += 1
        else:
            total_composite += 1e12  # penalty for failure

    return {
        "composite": total_composite,
        "score": total_score,
        "hard_violations": total_hard,
        "lateness": total_lateness,
        "n_success": n_success
    }


def random_search(base_config, test_cases, num_trials, time_limit):
    """Random search: sample random parameter combinations."""
    print(f"\n{'='*70}")
    print(f"RANDOM SEARCH: {num_trials} trials, {len(test_cases)} test cases")
    print(f"{'='*70}\n")

    best_result = None
    best_config = None
    best_params = None

    for trial in range(num_trials):
        # Sample random values for each parameter
        config = copy.deepcopy(base_config)
        params = {}
        for param_path, param_info in PARAM_SPACE.items():
            value = random.choice(param_info["values"])
            set_nested(config, param_path, value)
            params[param_path] = value

        print(f"Trial {trial+1}/{num_trials}:")
        for k, v in params.items():
            print(f"  {k.split('.')[-1]} = {v}")

        result = evaluate_config(config, test_cases, time_limit)
        print(f"  -> composite={result['composite']:.1f}, score={result['score']:.1f}, "
              f"hard_v={result['hard_violations']}, lateness={result['lateness']}")

        if best_result is None or result["composite"] < best_result["composite"]:
            best_result = result
            best_config = config
            best_params = params
            print(f"  ** NEW BEST **")

        print()

    return best_config, best_params, best_result


def grid_search(base_config, test_cases, time_limit):
    """Grid search: try all combinations (can be very slow)."""
    param_names = list(PARAM_SPACE.keys())
    param_values = [PARAM_SPACE[k]["values"] for k in param_names]

    total_combos = 1
    for v in param_values:
        total_combos *= len(v)

    print(f"\n{'='*70}")
    print(f"GRID SEARCH: {total_combos} combinations, {len(test_cases)} test cases")
    print(f"{'='*70}\n")

    if total_combos > 500:
        print(f"WARNING: {total_combos} combinations is very large. Consider --method random.")
        print("Proceeding anyway...\n")

    best_result = None
    best_config = None
    best_params = None
    trial = 0

    for combo in itertools.product(*param_values):
        trial += 1
        config = copy.deepcopy(base_config)
        params = {}
        for name, value in zip(param_names, combo):
            set_nested(config, name, value)
            params[name] = value

        print(f"Combo {trial}/{total_combos}:")
        result = evaluate_config(config, test_cases, time_limit)
        print(f"  composite={result['composite']:.1f}, score={result['score']:.1f}")

        if best_result is None or result["composite"] < best_result["composite"]:
            best_result = result
            best_config = config
            best_params = params
            print(f"  ** NEW BEST **")

    return best_config, best_params, best_result


def main():
    parser = argparse.ArgumentParser(description="VRP Solver Parameter Tuning")
    parser.add_argument("--cases", nargs="+", help="Input JSON test case files")
    parser.add_argument("--trials", type=int, default=30, help="Number of random trials (default: 30)")
    parser.add_argument("--time-limit", type=int, default=15, help="Solver time limit per run in seconds (default: 15)")
    parser.add_argument("--method", choices=["random", "grid"], default="random", help="Search method")
    parser.add_argument("--output", default=TUNED_CONFIG, help="Output config file")
    args = parser.parse_args()

    # Find test cases
    if args.cases:
        test_cases = args.cases
    else:
        # Auto-discover input files
        test_cases = sorted(str(p) for p in Path(OUTPUT_DIR).glob("input_*.json"))
        if not test_cases:
            print("ERROR: No test cases found. Provide --cases or put input_*.json in output/")
            sys.exit(1)

    print(f"Test cases ({len(test_cases)}):")
    for tc in test_cases:
        print(f"  {tc}")

    # Check solver exists
    if not os.path.exists(SOLVER_EXE):
        print(f"ERROR: {SOLVER_EXE} not found. Compile first: g++ -std=c++17 -O2 -o vrp_solver.exe src/vrp_solver_custom.cpp")
        sys.exit(1)

    # Load base config
    base_config = load_base_config()
    print(f"\nBase config: {BASE_CONFIG}")
    print(f"Tuning {len(PARAM_SPACE)} parameters:")
    for name, info in PARAM_SPACE.items():
        current = get_nested(base_config, name, "?")
        print(f"  {name}: current={current}, search={info['values']}")

    # Run search
    start_time = time.time()
    if args.method == "random":
        best_config, best_params, best_result = random_search(
            base_config, test_cases, args.trials, args.time_limit)
    else:
        best_config, best_params, best_result = grid_search(
            base_config, test_cases, args.time_limit)
    elapsed = time.time() - start_time

    # Report results
    print(f"\n{'='*70}")
    print(f"TUNING COMPLETE ({elapsed:.0f}s)")
    print(f"{'='*70}")
    print(f"\nBest configuration:")
    for name, value in best_params.items():
        desc = PARAM_SPACE[name]["description"]
        print(f"  {name.split('.')[-1]} = {value}  ({desc})")
    print(f"\nBest result:")
    print(f"  Composite score: {best_result['composite']:.1f}")
    print(f"  Total cost:      {best_result['score']:.1f}")
    print(f"  Hard violations: {best_result['hard_violations']}")
    print(f"  Total lateness:  {best_result['lateness']} min")

    # Save tuned config
    with open(args.output, "w") as f:
        json.dump(best_config, f, indent=2)
    print(f"\nTuned config saved to: {args.output}")
    print(f"To use: copy {args.output} to {BASE_CONFIG}")


if __name__ == "__main__":
    main()
