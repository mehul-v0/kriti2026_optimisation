# Velora VRP Solver — Comprehensive Backend Report

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Architecture & Data Flow](#2-architecture--data-flow)
3. [Source File Map](#3-source-file-map)
4. [Input Pipeline](#4-input-pipeline)
5. [C++ Solver Core](#5-c-solver-core)
   - [Phase 1: Diverse Construction](#phase-1-diverse-construction)
   - [Phase 2: ALNS Optimization](#phase-2-alns-optimization)
   - [Destroy Operators](#destroy-operators-12)
   - [Repair Operators](#repair-operators-5)
   - [Local Search](#local-search)
   - [Post-ALNS Optimization](#post-alns-optimization)
6. [Scoring & Evaluation System](#6-scoring--evaluation-system)
7. [Validation & Simulation](#7-validation--simulation)
8. [Configuration System](#8-configuration-system)
9. [Flask API (app.py)](#9-flask-api-apppy)
10. [Build & Deployment](#10-build--deployment)
11. [Performance Results](#11-performance-results)

---

## 1. System Overview

The Velora backend is a **Vehicle Routing Problem (VRP) solver** built for employee pickup/drop-off optimization. The system minimizes a **competition metric**:

$$\text{Objective} = 0.7 \times \text{Total Distance Cost (\$)} + 0.3 \times \text{Total Time (minutes)}$$

Subject to **hard constraints** (time windows, vehicle capacity) and **soft constraints** (sharing preferences, vehicle preferences, lateness penalties).

**Core technology stack:**
- **Solver engine**: C++17 single-translation-unit compiled binary using ALNS (Adaptive Large Neighborhood Search) metaheuristic
- **API server**: Python Flask with Gunicorn, calling the C++ solver via subprocess
- **Distance computation**: Haversine (default) or OpenRouteService road distances

---

## 2. Architecture & Data Flow

```
┌──────────────┐    Excel Upload     ┌─────────────────┐
│   Frontend    │ ─────────────────→  │  Flask API       │
│   (React)     │                     │  (app.py)        │
└──────┬───────┘                     └────────┬────────┘
       │                                       │
       │  POST /api/optimize                   │ 1. Parse Excel → JSON
       │                                       │ 2. (Optional) Fetch ORS distances
       │                                       │ 3. Write input JSON
       │                                       │ 4. subprocess.run(vrp_solver.exe)
       │                                       ▼
       │                              ┌─────────────────┐
       │                              │  C++ Solver      │
       │                              │  (vrp_solver.exe)│
       │                              └────────┬────────┘
       │                                       │
       │                                       │ 5. Read input JSON
       │                                       │ 6. Phase 1: Construction
       │                                       │ 7. Phase 2: ALNS
       │                                       │ 8. Write output JSON
       │                                       ▼
       │                              ┌─────────────────┐
       │   JSON Response              │  Output JSON     │
       │ ◀────────────────────────── │  (transformed)   │
       │                              └─────────────────┘
```

**End-to-end flow:**

1. **User uploads Excel** → `POST /api/upload` → `convert_excel_to_json.py` parses 4 sheets (employees, vehicles, metadata, baseline) into structured Python dicts
2. **User triggers optimization** → `POST /api/optimize` with parameters (time limit, distance method)
3. **app.py** builds input JSON with employees, vehicles, metadata, and (optionally) a pre-computed distance matrix from OpenRouteService
4. **C++ solver** is called as a subprocess: `vrp_solver.exe <input.json> <output.json> <time_limit>`
5. **Solver reads** `solver_config.json` for all tunable parameters, then `input.json` for problem data
6. **Phase 1**: 10 diverse initial solutions constructed via parallel cheapest insertion
7. **Phase 2**: ALNS metaheuristic optimizes top 5 candidates using time budgets
8. **Solver writes** detailed output JSON (routes, per-employee stats, cost breakdown)
9. **app.py** transforms solver output into frontend-expected format via `_transform_solver_output()`
10. **Background thread** optionally fetches ORS route geometries for map display

---

## 3. Source File Map

### C++ Solver (`server/src/`)

| File | Lines | Role |
|------|-------|------|
| `vrp_solver_custom.cpp` | ~250 | **Main entry point** — argument parsing, config loading, Phase 1 construction, Phase 2 ALNS orchestration |
| `vrp_alns.h` | ~3700 | **Core ALNS engine** — all 12 destroy operators, 5 repair operators, local search, post-ALNS optimization, SA/LAHC/GLS acceptance |
| `vrp_simulation.h` | ~1050 | **Scoring & evaluation** — fast evaluation, full simulation, trip permutation optimization, output formatting |
| `vrp_validators.h` | ~500 | **Validation layer** — multi-trip simulation, violation counting, forward slack computation, insertion feasibility |
| `vrp_construction.h` | ~400 | **Initial solution building** — 11 ordering strategies, parallel cheapest insertion |
| `vrp_config.h` | ~500 | **Configuration system** — 100+ parameters, JSON loader, defaults |
| `vrp_types.h` | ~130 | **Data structures** — Employee, Vehicle, Metadata, Solution, NeighborList |
| `vrp_utils.h` | ~50 | **Utility functions** — haversine distance |
| `vrp_parser.h` | ~200 | **JSON parser** — reads input JSON into typed structures, builds distance matrix |
| `vrp_output.h` | ~200 | **Output formatter** — generates rich JSON from simulation results |
| `json.hpp` | - | nlohmann/json header-only library |

### Python Backend (`server/`)

| File | Lines | Role |
|------|-------|------|
| `app.py` | ~1100 | **Flask REST API** — file upload, solver invocation, output transformation, ORS integration |
| `convert_excel_to_json.py` | ~200 | **Excel parser** — reads .xlsx with 4 sheets → structured JSON |
| `solver_config.json` | ~80 | **Runtime configuration** — all tunable parameters (no recompilation needed) |

---

## 4. Input Pipeline

### Excel Input Format

The system accepts `.xlsx` files with **4 required sheets**:

| Sheet | Key Columns | Purpose |
|-------|-------------|---------|
| **employees** | ID, pickup lat/lng, drop lat/lng, priority (1–5), earliest_pickup, latest_drop, vehicle_pref, sharing_pref | Employee pickup/drop-off data |
| **vehicles** | ID, physical_id, lat/lng, capacity, cost_per_km, speed_kmph, available_from, category | Fleet definition |
| **metadata** | priority_max_delays, cost_weight, time_weight, distance_multiplier | Optimization parameters |
| **baseline** | Employee-level baseline costs (Ola/Uber/Rapido rates) | Cost comparison reference |

### Distance Matrix Options

| Method | Trigger | Accuracy | Behavior |
|--------|---------|----------|----------|
| **Haversine** | `distanceMethod='haversine'` (default) | Straight-line | C++ computes `haversine_km * distance_multiplier` for all location pairs |
| **ORS Road** | `distanceMethod='actual_maps'` | Actual roads | app.py calls ORS Matrix API (max 50 locations), embeds distance matrix in input JSON |
| **ORS Fallback** | ORS fails (no API key, >50 locs, 403) | Haversine × 1.3 | Falls back with 30% road factor correction |

### Data Structures (vrp_types.h)

```
Employee {
    id, node_idx, pickup_lat/lng, drop_lat/lng,
    priority (1-5), earliest_pickup, latest_drop, latest_arrival_deadline,
    vehicle_pref (0=any, 1=premium, 2=normal),
    sharing_pref (1=single, 2=double, 3=triple)
}

Vehicle {
    id, physical_id, node_idx, start_node,
    lat/lng, capacity, cost_per_km, speed_kmph,
    available_from, category, vehicle_mode
}

NeighborList {
    K-nearest neighbors per employee (default K=10),
    direct_to_office distances
}
```

---

## 5. C++ Solver Core

### Entry Point (`vrp_solver_custom.cpp`)

```
Usage: vrp_solver.exe <input_json> [output_json] [time_limit_seconds]
```

1. Parse command-line arguments
2. Load `solver_config.json` via `g_config.load("solver_config.json")`
3. Parse input JSON → employees, vehicles, metadata, distance matrix
4. Build K-nearest neighbor lists (K=10)
5. **Phase 1**: Construct 10 diverse initial solutions
6. Rank solutions: fewer hard violations → fewer soft violations → lower score
7. **Phase 2**: ALNS optimization on top N candidates
8. Output best solution as detailed JSON

---

### Phase 1: Diverse Construction

**10 initial solutions** are built using `ParallelCheapestInsertion` with different ordering strategies:

| # | Strategy | Description |
|---|----------|-------------|
| 1 | `SingleTrip` | One trip per vehicle |
| 2 | `EARLIEST_DEADLINE` | Tightest deadlines first |
| 3 | `LATEST_DEADLINE` | Relaxed deadlines first |
| 4 | `GEOGRAPHIC_CLUSTER` | Nearest to office first |
| 5 | `PRIORITY_BASED` | By employee priority (P1→P5) |
| 6 | `CHEAPEST_VEHICLE_FIRST` | Fill cheapest vehicles first |
| 7 | `DOLLAR_COST_AWARE` | Farthest from office, cheapest vehicle preference |
| 8 | `GEO_CLUSTER_CONSOL` | Angular sector grouping (~17° sectors from office) |
| 9 | `ANGULAR_SECTOR` | Formal angular sector partitioning (Vansteenwegen 5.2.1.3) |
| 10 | `Savings` | Clarke-Wright savings algorithm |

**How `ParallelCheapestInsertion` works:**
1. Order employees according to the chosen strategy
2. Order vehicles by `cost_per_km` ascending (for cost-aware strategies)
3. For each unrouted employee: find the cheapest feasible insertion across all vehicles × all positions
4. If no feasible insertion exists → forced insertion fallback (progressively relaxing constraints)

**Ranking**: Solutions are ranked lexicographically by (hard_violations, soft_violations, score). The top `num_candidates_to_optimize` (default 5) proceed to Phase 2.

---

### Phase 2: ALNS Optimization

The top 5 candidates each receive a time budget (default split: 35%, 25%, 20%, 12%, 8% of total time). The ALNS optimizer is a **Simulated Annealing + Late Acceptance Hill Climbing + Guided Local Search** hybrid.

#### Main Loop Structure

```
while (elapsed_time < time_limit):
    1. Select destroy operator (roulette wheel on adaptive weights)
    2. Determine destroy size (adaptive: large when stagnated, small when improving)
    3. Destroy → remove employees from solution
    4. Select repair operator (roulette wheel on adaptive weights)
    5. Repair → reinsert removed employees
    6. Repair integrity (remove duplicates, reinsert missing)
    7. Apply intra-route local search
    8. Apply inter-route moves
    9. Periodically apply cross-exchange
    10. Evaluate new solution
    11. Accept/reject (SA + LAHC hybrid)
    12. Update operator weights (periodic)
    13. Update GLS penalties (periodic)
    14. Check for reheating / random restart
```

#### Acceptance Criterion (SA + LAHC Hybrid)

The acceptance logic is a cascading decision:

| Condition | Action |
|-----------|--------|
| New global best | **Always accept** |
| More hard violations than current | **Always reject** |
| Fewer hard violations than current | **Always accept** |
| Less lateness at same violation count | **Always accept** |
| Higher cost, same violations | **LAHC check**: accept if cost ≤ `lahc_history[iter % LAHC_LENGTH]` |
| Higher cost, LAHC rejects | **SA check**: accept with probability $e^{-\Delta / T}$ |

**Temperature schedule:**
- Start: `T₀ = 50,000`
- Cooling: `T ← T × 0.99995` per iteration
- Reheating: When stagnated (`iters_since_best ≥ 800`), temperature resets to `T₀ × adaptive_scale × reheat_factor`

#### LAHC (Late Acceptance Hill Climbing)

- Maintains a history buffer of length 500
- Each iteration: `lahc_history[iter % 500] = current_cost`
- Accepts worse solutions if they're no worse than the cost from 500 iterations ago
- Provides medium-term memory to escape local optima

#### GLS (Guided Local Search)

- Every 200 iterations: identifies the edge with maximum utility = `cost / (1 + penalty)`, increments its penalty
- GLS penalty is **added** to cost for acceptance decisions but **NOT** for best-solution tracking (raw cost only)
- Lambda auto-calibrated: `λ = 0.1 × avg_cost / total_employees`
- Penalties decayed on reheat: `penalty *= 0.5`

#### Adaptive Destroy Size

| Phase | Min Destroy | Max Destroy | Trigger |
|-------|-------------|-------------|---------|
| Stagnated | 30% | 65% | `iters_since_best > 500` |
| Improving | 8% | 35% | `iters_since_best < 50` |
| Normal | 10% | 60% | Default |

#### Reheating & Random Restarts

- **Trigger**: `iters_since_best ≥ reheat_threshold (800)` and `reheat_count < max_reheats (20)`
- **Every 3rd reheat**: **RANDOM RESTART** — reconstruct from scratch or restore from solution pool, destroy 40%, rebuild with greedy
- **Other reheats**: Temperature reset, return to best solution, apply random perturbation swaps

#### Operator Weight Updates

Every 500 iterations, weights are updated via:

$$w_i \leftarrow \text{decay} \times w_i + (1 - \text{decay}) \times \frac{\text{success\_score}_i}{\text{use\_count}_i} \times \text{scale}$$

Where `success_score` accumulates:
- σ₁ = 33 points for new global best
- σ₂ = 9 points for improving current
- σ₃ = 3 points for accepted worse

Minimum weight enforced at 0.1 to prevent operator starvation.

---

### Destroy Operators (12)

| # | Operator | Initial Weight | Description |
|---|----------|----------------|-------------|
| 0 | **Random Removal** | 1.0 | Removes `num_remove` employees from random positions across all routes |
| 1 | **Worst Removal** | 1.0 | Removes employees with highest arc-removal savings (i.e., employees whose removal saves the most cost). Uses **neighbor-list boost** for cross-vehicle employees |
| 2 | **Shaw Removal** | 1.0 | Relatedness-based removal. Picks a seed employee, removes the `num_remove` most similar employees (distance + time + cost similarity). Uses neighbor list for candidate selection |
| 3 | **Route Removal** | 1.0 | Removes entire routes (1–2 random non-empty routes) |
| 4 | **Violation Removal** | 1.0 | Targets employees causing soft/hard violations (sharing preference, vehicle preference, time window). Falls back to random if no violations found |
| 5 | **Consolidation Removal** | 2.0 | Removes ALL employees from 1–2 random physical vehicles' routes, enabling complete re-batching |
| 6 | **Cross-Vehicle Removal** | 1.5 | Removes employees on expensive vehicles who could benefit from moving to cheaper vehicles (scored by cost delta) |
| 7 | **Vehicle Elimination** | 2.5 | Removes ALL employees from the most cost-ineffective physical vehicle (highest `cost_per_km / employees_served` ratio) |
| 8 | **Expensive Arc Removal** | 2.0 | Finds most expensive edges (in dollar cost), removes employees at their endpoints. **Neighbor-list boosted** for cross-vehicle targeting |
| 9 | **String Removal** | 1.5 | Removes geographically close employees from DIFFERENT vehicles (cross-vehicle clusters). Uses `cross_vehicle_discount = 0.5` for proximity |
| 10 | **Lateness Targeted** | 3.0 | Removes employees from routes with highest total lateness. Empties entire late routes |
| 11 | **Ejection Chain** | 2.0 | 3-phase priority-aware destroy: (1) Remove late P1-P2 employees, (2) Remove P4-P5 from nearby routes to create space, (3) Supplement with worst-cost removal |

**Neighbor-list-guided destroy**: Operators #1 and #8 use K-nearest neighbor lists (K=10) to detect when an employee has nearby neighbors in different vehicles, boosting their removal score. This encourages cross-vehicle reorganization.

---

### Repair Operators (5)

| # | Operator | Initial Weight | Description |
|---|----------|----------------|-------------|
| 0 | **Greedy Insertion** | 1.0 | Cheapest feasible insertion (dollar cost + time penalty + lateness penalty). Uses **forward-slack O(1) pruning** to skip infeasible positions |
| 1 | **Regret Insertion** | 4.0 | Regret-K insertion (K=3). Inserts employee with highest regret (difference between best and K-th best position). Same pruning |
| 2 | **Nearest Insertion** | 1.0 | Inserts each employee at position closest to predecessor by distance |
| 3 | **Batching Insertion** | 2.0 | Sorts employees by distance to office, fills cheapest vehicles first (consolidation-oriented) |
| 4 | **Cheapest Vehicle** | 4.0 | 3-pass: (1a) premium-pref → premium vehicles, (1b) normal-pref → normal vehicles, (2) remaining → cheapest `cost_per_km`. Falls back to greedy |

**All repair operators** end with `force_insert_all()` to guarantee **zero unassigned employees**.

#### `force_insert_all` — 4-Level Constraint Relaxation

| Level | Constraints Enforced | Behavior |
|-------|---------------------|----------|
| 1 | All (hard + soft) | Find cheapest fully-feasible insertion |
| 2 | Hard only | Relax soft constraints (sharing/vehicle pref) |
| 3 | Allow hard violations | Minimize penalty: `time_viol × P + pref × P + lateness × P + dist_cost` |
| 4 | None | Last resort — insert into smallest route |

Uses `compute_route_slack` and `fast_check_insertion_feasible` for O(1) pruning at every level.

#### `lateness_aware_insert`

For inherently infeasible employees who can't arrive on time regardless of placement:
- Tries every position across every route with `allow_hard_violations = true`
- Picks position minimizing: `hard_violations × time_violation_penalty + lateness × lateness_penalty + pref_violations × pref_penalty + distance_cost`
- Uses `simulate_route_multitrip` for accurate multi-trip evaluation

---

### Local Search

Three levels of local search are applied every ALNS iteration:

#### Intra-Route (within a single route)

| Move | Description |
|------|-------------|
| **Relocate** | Move one employee to a different position within the same route |
| **Exchange** | Swap positions of two employees within the same route |
| **2-opt** | Reverse a segment within the route |
| **Or-opt** | Move segments of 2–3 consecutive employees within the route |

- **Strategy**: First-improvement (accept first improving move)
- **Max passes**: 3 (configurable)

#### Inter-Route (between two routes)

| Move | Description |
|------|-------------|
| **Relocate** | Move employee from route A to route B (cost-aware with `cost_per_km`) |
| **Swap** | Swap one employee between route A and route B |
| **Or-opt-2** | Move 2 consecutive employees from route A to route B |

- Uses weighted violation score: `hard × w_hard + pref × w_pref + lateness × w_lat`
- **Max passes**: 3 (configurable)

#### Cross-Exchange

- Swaps a segment of 2 employees from route A with a segment of 1 from route B
- **Periodic**: runs every 5 ALNS iterations (configurable)
- **Max passes**: 2 (configurable)

---

### Post-ALNS Optimization

After the main ALNS loop completes for each candidate, four post-processing stages run:

| Stage | Method | Description |
|-------|--------|-------------|
| 1 | `reduce_lateness_pass` | For each late employee (sorted by priority P1→P5), try moving to any other route if it reduces total cost. Skipped if lateness penalties are 0 |
| 2 | `ejection_chain_optimize` | Depth-1 ejection chain: remove emp₁ from v₁, insert into v₂ (displacing emp₂), move emp₂ to cheapest v₃. Accepts if solution improves by lateness-aware comparison |
| 3 | `exact_tsp_small_routes` | Routes with ≤ 6 employees: try all permutations to find optimal ordering |
| 4 | `post_alns_optimize` | 4 sub-phases: (a) best-improvement relocations + swaps, (b) consolidation (empty expensive vehicles), (c) final re-optimization, (d) score-aware intra-route swaps using `0.7 × cost + 0.3 × time` weighting |

---

## 6. Scoring & Evaluation System

### Competition Metric

$$\text{Final Score} = 0.7 \times \text{Total Distance Cost (\$)} + 0.3 \times \text{Total Time (min)}$$

This is what the competition judges. The solver optimizes a richer internal score that includes penalty terms to guide the search, but the penalties are tuned so that the **clean** competition metric is minimized.

### Internal Score Function (`compute_score_from_components`)

$$\text{score} = \underbrace{U \times P_{unassigned}}_{\text{unassigned penalty}} + \underbrace{H \times P_{time}}_{\text{hard time violations}} + \underbrace{L \times P_{lateness}}_{\text{lateness penalty}} + \underbrace{PWL \times P_{priority\_lat}}_{\text{priority lateness}}$$

$$+ \underbrace{V_{pref} \times P_{pref}}_{\text{preference violations}} + \underbrace{W_{cost} \times C_{dist}}_{\text{distance cost}} + \underbrace{W_{time} \times T}_{\text{time}} + \underbrace{N_{veh} \times P_{activation}}_{\text{vehicle activation}}$$

$$+ \underbrace{W_{pax} \times P_{wait\_pax}}_{\text{wait with pax}} + \underbrace{R_{viol} \times P_{ride\_viol}}_{\text{ride time violation}} + \underbrace{E_{ride} \times P_{lateness} \times 0.5}_{\text{excess ride time}} + \underbrace{PWR \times P_{priority\_ride}}_{\text{priority ride time}}$$

**Current config** zeroes out non-competition terms (`priority_lateness_multiplier=0`, `vehicle_activation_cost=0`, `wait_with_pax_penalty=0`, `priority_ride_time_weight=0`), so the effective score closely matches the competition metric plus feasibility enforcement.

### Fast vs Full Evaluation

| Function | Purpose | Speed | Used By |
|----------|---------|-------|---------|
| `fast_evaluate_solution` | ALNS iteration evaluation | Fast (no stop details) | Every ALNS iteration |
| `simulate_full_solution` | Detailed simulation | Slower (full per-employee stats) | Final output, comparison |
| `evaluate_physical_vehicle` | Incremental delta evaluation | Fastest (single vehicle) | Not currently used for delta caching |

### Trip Permutation Optimization

For a physical vehicle with K trips:
- **K ≤ 7**: Exact — try all K! permutations, pick best
- **K > 7**: Heuristic — try 3 orderings: (1) as-is, (2) by earliest deadline, (3) by earliest pickup

Selection criterion: fewer violations → less lateness → lower cost.

---

## 7. Validation & Simulation

### Multi-Trip Model

Vehicles make **multiple trips**. When `route.size() > vehicle.capacity`, the vehicle makes `⌈route.size() / capacity⌉` trips:

```
Trip 1: Depot  → pickup emp[0..cap-1] → Office
Trip 2: Office → pickup emp[cap..2cap-1] → Office
Trip 3: Office → pickup emp[2cap..3cap-1] → Office
...
```

Each subsequent trip starts **after the previous trip returns to office**.

### Core Validation Functions

| Function | Purpose |
|----------|---------|
| `simulate_route_multitrip` | Simulates multi-trip route → returns distances, per-employee arrival times, last office arrival |
| `count_route_violations_multitrip` | Counts all violations from multi-trip simulation: hard time violations, lateness, sharing/vehicle preference violations |
| `compute_route_slack` | Computes forward time slack for O(1) insertion feasibility (Gschwind & Drexl 2019 method) |
| `fast_check_insertion_feasible` | O(1) check: can employee be inserted at position `pos` without time window violations? Uses pre-computed slack |
| `validate_full_route` | Complete route validation (capacity + sharing + vehicle pref + time windows) |
| `is_solution_better` | Lexicographic comparison: fewer hard → fewer soft → lower score |
| `is_solution_better_lateness` | Lexicographic: fewer hard → less total lateness → lower score |

### Forward Slack Pruning

The `compute_route_slack` function pre-computes:
- `arrival[]` — arrival time at each position
- `wait[]` — wait time at each position
- `cum_wait_from[i]` — cumulative wait from position i to end
- `route_slack` = min_deadline − office_arrival

`fast_check_insertion_feasible` then checks in O(1):
- **End of route**: new_office_arrival ≤ min(all deadlines)
- **Middle of route**: delay from insertion ≤ route_slack + cum_wait_from[pos]

This avoids O(n) re-simulation for every trial insertion, providing massive speedup during repair operations.

---

## 8. Configuration System

### Architecture

All 100+ parameters are stored in `vrp_config.h` as a global `SolverConfig g_config` with C++ defaults. At startup, `g_config.load("solver_config.json")` overrides any values present in the JSON file. **No recompilation needed** to change parameters.

### Key Configuration Groups

#### Penalty Weights
| Parameter | Value | Purpose |
|-----------|-------|---------|
| `unassigned_employee_penalty` | 100,000 | Massive penalty for unassigned employees |
| `time_violation_penalty` | 100,000 | Massive penalty for hard time window violations |
| `lateness_per_minute_penalty` | 5.0 | Soft penalty per minute of lateness |
| `priority_lateness_multiplier` | 0.0 | Disabled — competition doesn't score priority lateness |
| `preference_violation_penalty` | 10.0 | Soft penalty for vehicle/sharing preference violations |
| `vehicle_activation_cost` | 0.0 | Disabled — competition doesn't score fleet size |
| `wait_with_pax_penalty` | 0.0 | Disabled — competition doesn't score passenger wait |
| `priority_ride_time_weight` | 0.0 | Disabled — competition doesn't score ride time by priority |

#### Objective Weights
| Parameter | Value | Source |
|-----------|-------|--------|
| `cost_weight` | 0.7 | Competition metric coefficient |
| `time_weight` | 0.3 | Competition metric coefficient |

#### SA Parameters
| Parameter | Value |
|-----------|-------|
| `start_temperature` | 50,000 |
| `cooling_rate` | 0.99995 |
| `reheat_threshold_iters` | 800 |
| `max_reheats` | 20 |
| `reheat_factor` | 0.4 |

#### ALNS Operator Weights (Initial)

**Destroy:**
| Operator | Weight | Rationale |
|----------|--------|-----------|
| Lateness Targeted | 3.0 | Highest — directly targets constraint violations |
| Vehicle Elimination | 2.5 | High — reduces fleet size (cost savings) |
| Consolidation | 2.0 | High — enables re-batching |
| Expensive Arc | 2.0 | High — targets costly routing |
| Ejection Chain | 2.0 | High — priority-aware restructuring |
| Cross-Vehicle | 1.5 | Medium — enables vehicle swaps |
| String | 1.5 | Medium — cluster-based reorganization |
| Random/Worst/Shaw/Route/Violation | 1.0 | Standard baseline |

**Repair:**
| Operator | Weight | Rationale |
|----------|--------|-----------|
| Regret Insertion | 4.0 | Highest — best theoretical quality (anticipates future cost) |
| Cheapest Vehicle | 4.0 | Highest — directly minimizes cost |
| Batching | 2.0 | Medium — consolidation-oriented |
| Greedy/Nearest | 1.0 | Standard baseline |

#### Construction Parameters
| Parameter | Value |
|-----------|-------|
| `num_candidates_to_optimize` | 5 |
| `time_split_percent` | [35, 25, 20, 12, 8] |
| `trip_time_estimate_minutes` | 30 |
| `min_trips_per_vehicle` | 4 |

---

## 9. Flask API (app.py)

### API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/` | GET | Health check → `{"message": "VRP Solver Backend Server", "version": "2.0"}` |
| `/api/health` | GET | Status check → `{"status": "ok"}` |
| `/api/upload` | POST | Upload Excel file → parse → return digest + structured data |
| `/api/optimize` | POST | **Main endpoint** — run solver with parameters, return optimized routes |
| `/api/geometry-status/<id>` | GET | Poll for background geometry fetching progress |
| `/api/download-solution` | GET | Download raw solver JSON as file attachment |
| `/api/results/<filename>` | GET | Serve specific output file |
| `/api/list-results` | GET | List all output JSON files with metadata |

### `/api/optimize` — Detailed Flow

1. **Read parameters** from request: `timeLimit`, `distanceMethod`, `distanceMultiplier`
2. **Distance matrix**:
   - If `actual_maps`: call `_fetch_openrouteservice_distances()` (ORS Matrix API, max 50 locations)
   - On ORS failure: fall back to haversine with `distance_multiplier = 1.3`
   - If `haversine`: no pre-computation, C++ computes inline
3. **Build input JSON**: employees, vehicles, metadata, (optional) distance_matrix
4. **Write** to `output/input_<timestamp>.json`
5. **Invoke solver**:
   ```python
   subprocess.run(
       [solver_exe, input_json, output_json, str(duration)],
       capture_output=True, timeout=duration+30, encoding='utf-8'
   )
   ```
   - Auto-builds solver if binary not found (`build.bat` on Windows, `make` on Linux)
6. **Read** output JSON from solver
7. **Transform** via `_transform_solver_output()` into frontend format
8. **Start background thread** for geometry fetching (ORS Directions API)
9. **Return** response to frontend

### Output Transformation (`_transform_solver_output`)

Converts raw C++ solver output into frontend-expected structure:

| Field | Type | Content |
|-------|------|---------|
| `routes` | `BackendRoute[]` | Per-vehicle route points: lat/lng, type (pickup/office), employee_id, trip_number, times, geometry |
| `assignments` | `BackendAssignment[]` | Per-employee records: pickup/dropoff locations, times, vehicle, trip, sequence_order |
| `result` | Object | Summary: total_cost, baseline_cost, cost_savings, cost_savings_percent, vehicles_used, violations |
| `violation_details` | Object | Categorized violations: capacity, time_window, sharing_pref, vehicle_pref, unassigned |

---

## 10. Build & Deployment

### Compilation

```makefile
# Release build (default)
g++ -std=c++17 -O3 -march=native -flto -DNDEBUG -o vrp_solver.exe src/vrp_solver_custom.cpp -lpthread

# Debug build
g++ -std=c++17 -g -O0 -o vrp_solver.exe src/vrp_solver_custom.cpp -lpthread
```

**Single translation unit**: All logic resides in header files included by `vrp_solver_custom.cpp`. No separate compilation of modules.

### Docker Deployment

```dockerfile
FROM python:3.11-slim
RUN apt-get update && apt-get install -y g++ make
COPY src/ ./src/
RUN make                            # Compile C++ solver
COPY requirements.txt .
RUN pip install -r requirements.txt # Install Python deps
COPY *.py solver_config.json ./
EXPOSE 5000
CMD ["gunicorn", "--bind", "0.0.0.0:5000", "--workers", "1", "--timeout", "300", "app:app"]
```

**Key settings:**
- Single worker (solver is CPU-bound, one at a time)
- 300-second timeout (supports long solver runs)
- Dependencies: Flask, Flask-CORS, Pandas, openpyxl, requests, python-dotenv, gunicorn

### Environment Variables

| Variable | Purpose | Required |
|----------|---------|----------|
| `ORS_API_KEY` | OpenRouteService API key for road distances | Only for `actual_maps` mode |

---

## 11. Performance Results

### Test Case Scores

| Test Case | Previous Score | Current Score | Improvement |
|-----------|---------------|---------------|-------------|
| TC01 | 477.31 | **463.51** | -2.9% |
| TC02 | 629.98 | **614.30** | -2.5% |
| TC03 | 748.38 | **747.03** | -0.2% |

Score = $0.7 \times \text{cost} + 0.3 \times \text{time}$ (lower is better), with 0 hard violations.

### Key Optimizations Implemented

1. **Forward slack O(1) pruning** in all repair operators — avoids O(n) re-simulation per trial insertion
2. **Neighbor-list-guided destroy** — `destroy_worst` and `destroy_expensive_arc` boost cross-vehicle employees for better reorganization
3. **Priority-aware ejection chain** — 3-phase destroy targeting late high-priority employees
4. **Multi-trip simulation fix** — `force_insert_all` and `lateness_aware_insert` use multitrip-aware simulation
5. **Negative ALNS score fix** — `std::max(0, ...)` clamp prevents reward for worsening
6. **Competition metric alignment** — zeroed non-scoring penalties to avoid misleading the optimizer
7. **SA+LAHC+GLS hybrid acceptance** — combines three acceptance mechanisms for effective diversification
8. **Adaptive destroy sizing** — larger destroys when stagnated, smaller when improving
9. **Random restarts** — every 3rd reheat rebuilds from scratch to escape deep local optima
10. **Post-ALNS pipeline** — lateness reduction, ejection chain, exact TSP, consolidation
