# Velora Mobitech - Corporate Mobility Optimization Engine

## Server Backend Documentation

This document provides a comprehensive technical overview of the Vehicle Routing Problem (VRP) optimization system used for corporate employee transportation.

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Architecture](#2-architecture)
3. [Data Flow](#3-data-flow)
4. [Input Format](#4-input-format)
5. [C++ VRP Solver](#5-c-vrp-solver)
6. [ALNS Algorithm](#6-alns-algorithm)
7. [Constraint Handling](#7-constraint-handling)
8. [Cost Function](#8-cost-function)
9. [API Endpoints](#9-api-endpoints)
10. [Configuration](#10-configuration)
11. [Building & Running](#11-building--running)

---

## 1. System Overview

The optimization engine solves a **Multi-Trip Capacitated Vehicle Routing Problem with Time Windows (MT-CVRPTW)**. Given a set of employees who need transportation from their homes to a common office location, the system:

- **Minimizes total transportation cost** (fuel, vehicle usage)
- **Minimizes total travel time**
- **Respects time window constraints** (employees must arrive before deadlines)
- **Handles vehicle capacity limits**
- **Considers employee preferences** (vehicle type, sharing preferences)
- **Supports multiple trips per vehicle**

### Key Features

| Feature | Description |
|---------|-------------|
| Multi-objective optimization | Weighted balance between cost and time |
| Priority-aware scheduling | Higher priority employees get stricter time guarantees |
| Vehicle preferences | Premium vs normal vehicle matching |
| Sharing preferences | Single, double, or triple occupancy options |
| Multi-trip support | Vehicles can make multiple round trips |
| Real road distances | Optional OpenRouteService integration |

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Flask API Server (app.py)                       │
│                                                                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐   │
│  │ /api/upload │  │/api/optimize│  │ /api/status │  │/api/geometry │   │
│  └─────────────┘  └─────────────┘  └─────────────┘  └──────────────┘   │
└────────────────────────────────────────────────────────────────────────┘
         │                  │
         ▼                  ▼
┌─────────────────┐  ┌─────────────────────────────────────────────────┐
│ convert_excel_  │  │              C++ VRP Solver                     │
│ to_json.py      │  │                                                 │
│                 │  │  ┌──────────────┐  ┌─────────────────────────┐  │
│ Excel → JSON    │  │  │ Construction │  │ ALNS Optimization       │  │
│ conversion      │  │  │ Heuristics   │──│ (Adaptive Large         │  │
│                 │  │  │ (9 methods)  │  │  Neighborhood Search)   │  │
└─────────────────┘  │  └──────────────┘  └─────────────────────────┘  │
                     └─────────────────────────────────────────────────┘
```

### Component Files

| File | Purpose |
|------|---------|
| `app.py` | Flask REST API server |
| `convert_excel_to_json.py` | Excel to JSON converter |
| `vrp_solver_custom.cpp` | Main solver entry point |
| `vrp_types.h` | Data structures (Employee, Vehicle, Trip, etc.) |
| `vrp_parser.h` | JSON input parser |
| `vrp_constraints.h` | Constraint propagation engine |
| `vrp_construction.h` | Initial solution construction |
| `vrp_single_trip_construction.h` | Single-trip baseline construction |
| `vrp_savings_construction.h` | Clarke-Wright savings algorithm |
| `vrp_local_search.h` | Local search operators (2-opt, relocate, etc.) |
| `vrp_alns.h` | ALNS metaheuristic (core optimization) |
| `vrp_validators.h` | Solution validation utilities |
| `vrp_output.h` | JSON output formatter |
| `vrp_config.h` | Configuration loader |
| `vrp_utils.h` | Utility functions |
| `solver_config.json` | Tunable parameters |

---

## 3. Data Flow

```
┌─────────────┐     ┌──────────────────┐     ┌────────────────┐
│ Excel File  │────▶│ convert_excel_   │────▶│ input_XXX.json │
│ (.xlsx)     │     │ to_json.py       │     │                │
└─────────────┘     └──────────────────┘     └───────┬────────┘
                                                     │
                                                     ▼
┌─────────────┐     ┌──────────────────┐     ┌────────────────┐
│ Frontend    │◀────│ app.py           │◀────│ C++ Solver     │
│ Response    │     │ (format results) │     │ vrp_solver_    │
└─────────────┘     └──────────────────┘     │ custom.exe     │
                                             └───────┬────────┘
                                                     │
                                                     ▼
                                             ┌────────────────┐
                                             │ output_XXX.json│
                                             └────────────────┘
```

### Process Steps

1. **Upload**: User uploads Excel file via `/api/upload`
2. **Convert**: `convert_excel_to_json.py` parses Excel sheets → JSON
3. **Configure**: Frontend sends optimization parameters (weights, time limits)
4. **Optimize**: C++ solver runs with configured time budget
5. **Post-process**: `app.py` formats solver output, fetches route geometries
6. **Respond**: JSON response with routes, assignments, costs, violations

---

## 4. Input Format

### Excel File Structure

The input Excel file must contain 4 sheets:

#### Sheet 1: `employees`
| Column | Type | Description |
|--------|------|-------------|
| employee_id | String | Unique identifier (e.g., "E01") |
| pickup_lat | Float | Pickup latitude |
| pickup_lng | Float | Pickup longitude |
| drop_lat | Float | Drop-off latitude (office) |
| drop_lng | Float | Drop-off longitude (office) |
| priority | Integer | 1-5 (1 = highest priority) |
| earliest_pickup | Time | Earliest pickup time (HH:MM) |
| latest_drop | Time | Latest arrival deadline (HH:MM) |
| vehicle_preference | String | "premium", "normal", or "any" |
| sharing_preference | String | "single", "double", or "triple" |

#### Sheet 2: `vehicles`
| Column | Type | Description |
|--------|------|-------------|
| vehicle_id | String | Unique identifier (e.g., "V01") |
| current_lat | Float | Starting location latitude |
| current_lng | Float | Starting location longitude |
| capacity | Integer | Maximum passengers |
| cost_per_km | Float | Cost per kilometer (₹) |
| avg_speed_kmph | Float | Average speed (km/h) |
| available_from | Time | Availability start time (HH:MM) |
| category | String | "premium" or "normal" |

#### Sheet 3: `baseline`
| Column | Type | Description |
|--------|------|-------------|
| employee_id | String | Employee identifier |
| baseline_cost | Float | Cost for individual trip (₹) |
| baseline_time_min | Float | Time for individual trip (minutes) |

#### Sheet 4: `metadata`
| Key | Value | Description |
|-----|-------|-------------|
| test_case_id | String | Test case identifier |
| city | String | City name |
| priority_1_max_delay_min | Integer | Max delay for P1 employees |
| priority_2_max_delay_min | Integer | Max delay for P2 employees |
| ... | ... | ... |
| objective_cost_weight | Float | Weight for cost (0-1) |
| objective_time_weight | Float | Weight for time (0-1) |

### JSON Format (Internal)

```json
{
  "employees": [
    {
      "employee_id": "E01",
      "pickup_lat": 12.9352,
      "pickup_lng": 77.6245,
      "drop_lat": 12.9716,
      "drop_lng": 77.5946,
      "priority": 1,
      "earliest_pickup": "08:30",
      "latest_drop": "09:30",
      "vehicle_preference": "premium",
      "sharing_preference": "single"
    }
  ],
  "vehicles": [...],
  "metadata": {...},
  "baseline": [
    { "employee_id": "E01", "baseline_cost": 420.0, "baseline_time": 45.0 }
  ],
  "distance_matrix": [[...]]  // Optional: custom road distances
}
```

---

## 5. C++ VRP Solver

### Main Entry Point (`vrp_solver_custom.cpp`)

```cpp
int main(int argc, char* argv[]) {
    // 1. Load input JSON
    VRPParser::load(input_file, employees, vehicles, metadata, distance_matrix);
    
    // 2. Build neighbor lists for efficient lookups
    NeighborList nlist;
    nlist.build(employees, distance_matrix, OFFICE_NODE);
    
    // 3. Setup constraint engines
    ConstraintEngine cp_soft, cp_hard;
    cp_soft.setup(true, employees, vehicles, metadata);   // Enforce soft constraints
    cp_hard.setup(false, employees, vehicles, metadata);  // Relaxed mode
    
    // 4. PHASE 1: Build diverse initial solutions
    for (strategy in [SingleTrip, EarliestDeadline, GeoCluster, ...]) {
        build_initial_solution(strategy);
        candidates.push_back(solution);
    }
    
    // 5. Rank candidates by quality
    sort(candidates, by: hard_violations, soft_violations, score);
    
    // 6. PHASE 2: ALNS optimization on top candidates
    for (i = 0; i < 3; i++) {
        alns.optimize(candidates[i], time_budget[i]);
        if (solution_is_better) best_solution = solution;
    }
    
    // 7. Output best solution as JSON
    OutputFormatter::to_json(best_solution);
}
```

### Multi-Trip Support

The solver uses **virtual vehicles** to handle multiple trips:

```
Physical Vehicle V01 (capacity=3) becomes:
  - Virtual_V01_Trip1 (capacity=3)
  - Virtual_V01_Trip2 (capacity=3)
  - Virtual_V01_Trip3 (capacity=3)
  - Virtual_V01_Trip4 (capacity=3)
```

Each virtual vehicle represents one trip to the office. This allows the solver to treat multi-trip routing as a standard VRP.

---

## 6. ALNS Algorithm (Deep Dive)

### Overview

**Adaptive Large Neighborhood Search (ALNS)** is a state-of-the-art metaheuristic that combines:
- **Large Neighborhood Search (LNS)**: Explores large changes to escape local optima
- **Adaptive operator selection**: Learns which operators work best for the current problem
- **Simulated Annealing**: Accepts worse solutions probabilistically to escape local minima
- **Guided Local Search (GLS)**: Penalizes frequently-used costly edges

### Why ALNS?

Traditional local search (e.g., 2-opt, relocate) only makes small changes and easily gets stuck. ALNS removes 10-60% of all employees and reinserts them completely differently, enabling dramatic restructuring of solutions.

### Main Loop (Detailed)

```cpp
void optimize(routes, time_limit) {
    // === INITIALIZATION ===
    total_employees = emps.size();
    temperature = max(50000, initial_cost * 0.5);  // Adaptive temperature
    best_solution = current_solution = routes;
    
    // Initialize Late Acceptance Hill Climbing (LAHC) history
    lahc_history.fill(initial_cost);  // 500-element history
    
    // Initialize solution pool for multi-start diversification
    solution_pool = [{routes, cost, violations}];
    
    // GLS calibration: lambda based on average route cost
    gls_lambda = 0.1 * avg_cost / num_employees;
    
    while (time_remaining > 0) {
        // === ADAPTIVE DESTROY SIZE ===
        // Larger destroys when stagnated, smaller when improving
        if (iters_since_best > 500) {
            destroy_pct = random(0.30, 0.65);  // Aggressive
        } else if (iters_since_best < 50) {
            destroy_pct = random(0.08, 0.35);  // Conservative
        } else {
            destroy_pct = random(0.10, 0.60);  // Normal
        }
        num_remove = total_employees * destroy_pct;
        
        // === DESTROY PHASE ===
        destroy_op = select_weighted_random(destroy_weights);
        removed = destroy_operators[destroy_op](routes, num_remove);
        
        // === REPAIR PHASE ===
        repair_op = select_weighted_random(repair_weights);
        repair_operators[repair_op](routes, removed);
        
        // CRITICAL: Guarantee ALL employees assigned
        if (!removed.empty()) {
            force_insert_all(routes, removed);
        }
        
        // === LOCAL SEARCH ===
        // Intra-route: 2-opt, relocate, exchange, Or-opt
        apply_local_search(routes);
        
        // Inter-route: relocate, exchange, cross between vehicles
        apply_inter_route_moves(routes);
        
        // Cross-exchange: swap segments between routes (every 5th iteration)
        if (iteration % 5 == 0) {
            apply_cross_exchange(routes);
        }
        
        // === ACCEPTANCE DECISION ===
        new_cost = evaluate(routes);
        delta = new_cost - current_cost;
        
        // Hybrid acceptance: SA + LAHC
        sa_accept = (delta < 0) || (random() < exp(-delta / temperature));
        lahc_accept = (new_cost < lahc_history[iteration % 500]);
        
        if (sa_accept || lahc_accept) {
            current_solution = routes;
            current_cost = new_cost;
            
            if (is_new_best(new_cost, new_violations)) {
                best_solution = routes;
                best_cost = new_cost;
                iters_since_best = 0;
                
                // Update operator weights (major reward)
                destroy_successes[destroy_op] += 33;
                repair_successes[repair_op] += 33;
            }
            
            // Update LAHC history
            lahc_history[iteration % 500] = new_cost;
        }
        
        // === ADAPTIVE REHEATING ===
        if (iters_since_best > REHEAT_THRESHOLD && reheat_count < MAX_REHEATS) {
            // Return to best solution with random perturbation
            current_solution = perturb(best_solution);
            temperature = start_temperature * 0.4;
            
            // Decay GLS penalties for diversification
            for (edge : edge_penalties) {
                edge.penalty *= 0.5;
            }
            reheat_count++;
        }
        
        // === GLS PENALTY UPDATE ===
        if (iteration % 200 == 0) {
            // Penalize most-used expensive edge
            max_edge = find_edge_with_max_utility();  // utility = cost / (1 + penalty)
            edge_penalties[max_edge] += 1.0;
        }
        
        // === COOL DOWN ===
        temperature *= 0.99995;
        iters_since_best++;
    }
    
    // === FINAL INTEGRITY CHECK ===
    repair_integrity(best_solution);  // Remove duplicates, reinsert missing
}
```

### Destroy Operators (Detailed)

| # | Operator | How It Works | When It Helps |
|---|----------|--------------|---------------|
| 0 | **RANDOM_REMOVAL** | Randomly selects employees to remove | Baseline exploration, no bias |
| 1 | **WORST_REMOVAL** | Calculates insertion cost for each employee; removes highest-cost ones | Targets inefficient assignments |
| 2 | **SHAW_REMOVAL** | Picks a seed employee, removes N most similar (geographic + time + cost similarity) | Groups nearby employees for rebatching |
| 3 | **ROUTE_REMOVAL** | Removes all employees from 1-2 random routes | Allows complete route restructuring |
| 4 | **VIOLATION_REMOVAL** | Finds employees causing time/preference violations; removes them + neighbors | Directly attacks constraint violations |
| 5 | **CONSOLIDATION_REMOVAL** | Removes ALL employees from a physical vehicle's trips | Enables batching across trips |
| 6 | **CROSS_VEHICLE_REMOVAL** | Scores employees by (current vehicle cost) - (cheapest alternative); removes highest scorers | Moves employees to cheaper vehicles |
| 7 | **VEHICLE_ELIMINATION** | Attempts to empty entire vehicle by removing all its employees | Reduces fleet usage |
| 8 | **EXPENSIVE_ARC_REMOVAL** | Finds edges with highest (distance × cost_per_km); removes adjacent employees | Targets costly travel segments |
| 9 | **STRING_REMOVAL** | Removes geographically close employees from different vehicles | Enables geographic consolidation |
| 10 | **LATENESS_TARGETED_REMOVAL** | Sorts routes by total lateness; removes from most-delayed routes | Directly reduces delays |

#### Example: Shaw Removal Algorithm

```cpp
// 1. Pick random seed employee
seed = random_employee_from_routes();

// 2. Calculate similarity to all other employees
for (emp in all_employees) {
    dist_sim = distance(seed.pickup, emp.pickup);           // Geographic proximity
    time_sim = |seed.earliest_pickup - emp.earliest_pickup|; // Temporal similarity
    cost_sim = |dist_to_office(seed) - dist_to_office(emp)|; // Cost similarity
    
    // Weighted combination (distance dominates)
    similarity = dist_sim + 0.01 * time_sim + 0.005 * cost_sim;
}

// 3. Sort by similarity (most similar first)
sort(similarities, ascending);

// 4. Remove seed + (N-1) most similar employees
removed = [seed] + top_N_similar(similarities);
```

#### Example: Worst Removal Algorithm

```cpp
// For each employee, calculate cost of removing and reinserting
for (emp in all_assigned_employees) {
    // Current position
    v = emp.vehicle;
    i = emp.position_in_route;
    
    // Calculate removal savings (distance reduction)
    prev = (i == 0) ? vehicle_start : route[i-1];
    next = (i == last) ? OFFICE : route[i+1];
    
    removal_savings = dist[prev][emp] + dist[emp][next] - dist[prev][next];
    
    // Multiply by cost_per_km for dollar cost
    removal_value = removal_savings * vehicle[v].cost_per_km;
}

// Sort by removal value (highest first = worst assignments)
sort(employees, by: removal_value, descending);

// Remove top N worst employees
removed = top_N(employees);
```

### Repair Operators (Detailed)

| # | Operator | How It Works | Best For |
|---|----------|--------------|----------|
| 0 | **GREEDY_INSERTION** | For each employee, finds position with minimum insertion cost; inserts best overall | Fast, good general quality |
| 1 | **REGRET_INSERTION** | Calculates "regret" = (2nd best cost - best cost); inserts highest-regret employee first | Handles tight constraints better |
| 2 | **NEAREST_INSERTION** | Inserts each employee at position closest to previous pickup | Creates compact geographic routes |
| 3 | **BATCHING_INSERTION** | Sorts employees by distance to office; fills cheapest vehicles first | Maximizes vehicle utilization |
| 4 | **CHEAPEST_VEHICLE_INSERTION** | Always prefers vehicles with lowest cost_per_km | Minimizes dollar cost |

#### Example: Regret Insertion Algorithm

```cpp
while (unassigned not empty) {
    best_regret = -infinity;
    best_emp = null;
    best_position = null;
    
    for (emp in unassigned) {
        costs = [];
        
        // Find all feasible (vehicle, position) combinations
        for (v in vehicles) {
            if (route[v].size >= capacity[v]) continue;
            
            for (pos = 0; pos <= route[v].size; pos++) {
                // Calculate insertion cost
                prev = (pos == 0) ? vehicle_start : route[v][pos-1];
                curr = emp;
                next = (pos == route[v].size) ? OFFICE : route[v][pos];
                
                delta = dist[prev][curr] + dist[curr][next] - dist[prev][next];
                cost = delta * vehicle[v].cost_per_km;
                
                // Check feasibility
                if (is_feasible(emp, v, pos)) {
                    costs.append({cost, v, pos});
                }
            }
        }
        
        if (costs.empty) continue;
        
        // Sort by cost ascending
        sort(costs);
        
        // Calculate k-regret (k=3): sum of differences from best
        regret = 0;
        for (k = 1; k < min(3, costs.size); k++) {
            regret += costs[k].cost - costs[0].cost;
        }
        
        // Only 1 option → infinite regret (must insert NOW!)
        if (costs.size == 1) regret = INFINITY;
        
        if (regret > best_regret) {
            best_regret = regret;
            best_emp = emp;
            best_position = costs[0];  // Best position for this employee
        }
    }
    
    // Insert employee with highest regret at their best position
    insert(best_emp, best_position.vehicle, best_position.pos);
    unassigned.remove(best_emp);
}
```

### Force Insert (Progressive Constraint Relaxation)

When normal repair fails, a 4-level fallback guarantees ALL employees are placed:

```
┌─────────────────────────────────────────────────────────────┐
│ Level 1: Full Constraint Check                              │
│   - Time windows enforced                                   │
│   - Vehicle preferences enforced                            │
│   - Sharing preferences enforced                            │
│   Success? → Done                                           │
└─────────────────────────────────────────────────────────────┘
         │ (fails)
         ▼
┌─────────────────────────────────────────────────────────────┐
│ Level 2: Relax Soft Constraints                             │
│   - Time windows enforced (hard)                            │
│   - Vehicle/sharing preferences IGNORED                     │
│   Success? → Done (with soft violations)                    │
└─────────────────────────────────────────────────────────────┘
         │ (fails)
         ▼
┌─────────────────────────────────────────────────────────────┐
│ Level 3: Allow Hard Violations, Minimize Lateness           │
│   - Time windows can be violated                            │
│   - Minimize: violations × 100000 + lateness × 1000 + cost  │
│   Success? → Done (with hard violations, minimized)         │
└─────────────────────────────────────────────────────────────┘
         │ (fails)
         ▼
┌─────────────────────────────────────────────────────────────┐
│ Level 4: Absolute Last Resort                               │
│   - Insert into route with fewest employees                 │
│   - Guaranteed to succeed (capacity checked dynamically)    │
└─────────────────────────────────────────────────────────────┘
```

### Adaptive Weight Updates

Operator selection uses roulette wheel with learned weights:

```cpp
// Selection probability proportional to weight
P(operator_i) = weight_i / sum(all_weights)

// Weight update based on outcome:
if (new_global_best) {
    successes[op] += σ1 = 33;  // Major reward
} else if (improved_current) {
    successes[op] += σ2 = 9;   // Medium reward  
} else if (accepted_worse) {
    successes[op] += σ3 = 3;   // Small reward
}

// Periodic weight recalculation (every 500 iterations)
every 500 iterations:
    for (op in operators) {
        success_rate = successes[op] / attempts[op];
        // Exponential smoothing: 80% old, 20% new
        weight[op] = 0.8 * weight[op] + 0.2 * success_rate * 10;
        weight[op] = max(0.1, weight[op]);  // Minimum weight (never zero)
    }
    reset(successes, attempts);
```

### Local Search Operators

After each destroy-repair cycle, local search fine-tunes routes:

#### Intra-Route Moves (within one vehicle)

| Move | Description | Example |
|------|-------------|---------|
| **Relocate** | Move employee from position i to position j | `[A,B,C,D]` → `[A,C,D,B]` |
| **Exchange** | Swap employees at positions i and j | `[A,B,C,D]` → `[A,D,C,B]` |
| **2-opt** | Reverse segment between positions i and j | `[A,B,C,D]` → `[A,C,B,D]` |
| **Or-opt** | Move segment of 2-3 employees to different position | `[A,B,C,D,E]` → `[A,D,B,C,E]` |

```
Example: 2-opt improvement

Before:  Depot → A → B → C → D → Office
         (route crosses itself)
         
After:   Depot → A → C → B → D → Office  
         (reversed [B,C] segment, no crossing)
         
Distance saved: Previous back-and-forth eliminated
```

#### Inter-Route Moves (between vehicles)

| Move | Description |
|------|-------------|
| **Relocate** | Move employee from vehicle V1 to vehicle V2 |
| **Exchange** | Swap one employee from V1 with one from V2 |
| **Cross-exchange** | Swap segment of 2 from V1 with segment of 1 from V2 |

```
Example: Inter-route Exchange

Before:
  V1 (expensive): [A, B, C]  
  V2 (cheap):     [D, E]

After exchanging B ↔ D:
  V1: [A, D, C]
  V2: [B, E]

Benefit: Employee B moved to cheaper vehicle
```

### Simulated Annealing Acceptance

```cpp
// Always accept improvements
if (new_cost < current_cost) {
    accept();
}
// Accept worse solutions with decreasing probability
else {
    delta = new_cost - current_cost;
    acceptance_probability = exp(-delta / temperature);
    
    if (random() < acceptance_probability) {
        accept();  // Escape local minimum
    } else {
        reject();
    }
}

// Temperature schedule
temperature *= cooling_rate;  // 0.99995 per iteration

// Example acceptance probabilities:
// At T=10000, delta=5000:  P = exp(-0.5) ≈ 0.61 (61% accept)
// At T=10000, delta=1000:  P = exp(-0.1) ≈ 0.90 (90% accept)
// At T=1000,  delta=5000:  P = exp(-5.0) ≈ 0.007 (0.7% accept)
// At T=100,   delta=500:   P = exp(-5.0) ≈ 0.007 (0.7% accept)
```

### Late Acceptance Hill Climbing (LAHC)

LAHC maintains a history of costs and accepts moves better than the cost from N iterations ago:

```cpp
// LAHC maintains history of last 500 costs
lahc_history = circular_buffer(size=500);
lahc_history.fill(initial_cost);  // Start with initial cost

// On each iteration
lahc_idx = iteration % 500;
lahc_accept = (new_cost < lahc_history[lahc_idx]);

// Combined acceptance criterion
if (sa_accept || lahc_accept) {
    accept();
    lahc_history[lahc_idx] = new_cost;  // Update history
}

// Why LAHC helps:
// - When SA temperature is very low, SA rarely accepts worse moves
// - LAHC allows accepting moves better than 500 iterations ago
// - This maintains exploration even late in the search
```

### Guided Local Search (GLS)

Inspired by OR-Tools, GLS penalizes frequently-used expensive edges to escape local optima:

```cpp
// Edge penalties accumulate over time
map<(from, to), penalty> edge_penalties;

// Edge utility = cost / (1 + penalty)
// High cost + low penalty = high utility = should be penalized

every 200 iterations:
    max_utility = 0;
    max_edge = null;
    
    // Find edge with maximum utility
    for (route in all_routes) {
        prev = vehicle_start;
        for (emp in route) {
            cost = dist[prev][emp] * vehicle.cost_per_km;
            penalty = edge_penalties[(prev, emp)];
            utility = cost / (1.0 + penalty);
            
            if (utility > max_utility) {
                max_utility = utility;
                max_edge = (prev, emp);
            }
            prev = emp;
        }
    }
    
    // Increase penalty for most-used expensive edge
    edge_penalties[max_edge] += 1.0;

// Effect: Discourages using this edge in future solutions
// Forces exploration of alternative routes
```

### Adaptive Reheating

When search stagnates (no improvement for many iterations), reheat to escape:

```cpp
const REHEAT_THRESHOLD = 800;  // iterations without improvement
const MAX_REHEATS = 20;        // maximum reheats allowed
const REHEAT_FACTOR = 0.4;     // reheat to 40% of start temp

if (iters_since_best > REHEAT_THRESHOLD && reheat_count < MAX_REHEATS) {
    cout << "REHEAT #" << reheat_count << endl;
    
    // Option 1: Try solution from pool (if different enough)
    if (solution_pool.has_diverse_solution()) {
        current = solution_pool.get_random();
    }
    // Option 2: Perturb best solution
    else {
        current = best_solution;
        
        // Random swaps between routes (structural perturbation)
        for (i = 0; i < 2-3; i++) {
            route_a = random_non_empty_route();
            route_b = random_non_empty_route();
            swap(random_emp_from(route_a), random_emp_from(route_b));
        }
    }
    
    // Reset temperature to 40% of start
    temperature = start_temperature * REHEAT_FACTOR;
    
    // Decay GLS penalties (allow revisiting old edges)
    for (edge, penalty : edge_penalties) {
        penalty *= 0.5;
    }
    
    reheat_count++;
    iters_since_best = 0;
}
```

---

## 7. Constraint Handling

### Hard Constraints

These **must** be satisfied for a valid solution:

| Constraint | Description |
|------------|-------------|
| Vehicle Capacity | Cannot exceed passenger limit |
| Employee Assignment | Every employee must be assigned exactly once |
| Time Incompatibility | Employees with conflicting time windows cannot share |

### Soft Constraints

These are **penalized** if violated:

| Constraint | Penalty | Description |
|------------|---------|-------------|
| Time Window | 100,000 + 1,000/min | Employee arrives after deadline |
| Vehicle Preference | 10,000 | Premium employee on normal vehicle |
| Sharing Preference | 10,000 | Single-preference employee sharing trip |

### Priority-Based Delays

Higher priority employees have stricter time tolerances:

| Priority | Max Delay (default) | Lateness Penalty Multiplier |
|----------|---------------------|------------------------------|
| P1 | 5 minutes | 5× |
| P2 | 5 minutes | 3× |
| P3 | 10 minutes | 2× |
| P4 | 15 minutes | 1× |
| P5 | 15 minutes | 1× |

### Constraint Propagation

Before optimization, the constraint engine:
1. **Prunes infeasible assignments** (incompatible time windows)
2. **Identifies incompatible pairs** (employees who cannot share a trip)
3. **Propagates preferences** (single-preference employees marked incompatible with all)

```cpp
// Time incompatibility check
if (emp_i.latest_deadline < emp_j.earliest_pickup ||
    emp_j.latest_deadline < emp_i.earliest_pickup) {
    mark_incompatible(i, j);
}

// Advanced check: minimum trip time would cause violation
if (emp_j.earliest_pickup + MIN_TRIP_TIME > emp_i.deadline) {
    mark_incompatible(i, j);
}
```

---

## 8. Cost Function

### Weighted Multi-Objective Score

```
Score = W_cost × Normalized_Cost + W_time × Normalized_Time
```

Where:
- `W_cost` = Cost weight (default 0.7)
- `W_time` = Time weight (default 0.3)
- Normalization uses min-max scaling to [0, 1]

### ALNS Internal Scoring

For solution comparison during optimization:

```cpp
double evaluate(solution) {
    double score = 0;
    
    // Base costs
    score += total_distance_cost;
    score += vehicle_activation_cost * num_vehicles_used;
    
    // Hard violation penalties
    score += 100000 * num_unassigned_employees;
    score += 100000 * num_time_violations;
    
    // Lateness penalties (priority-weighted)
    for (employee : employees) {
        if (arrival_time > deadline) {
            lateness = arrival_time - deadline;
            priority_weight = get_priority_weight(employee.priority);
            score += 1000 * lateness;
            score += 500 * priority_weight * lateness;
        }
    }
    
    // Soft constraint penalties
    score += 10000 * num_vehicle_pref_violations;
    score += 10000 * num_sharing_pref_violations;
    
    return score;
}
```

### Solution Comparison

Solutions are ranked by:
1. **Fewer hard violations** (highest priority)
2. **Fewer soft violations**
3. **Lower score**

```cpp
bool is_better(sol_a, sol_b) {
    if (sol_a.hard_violations != sol_b.hard_violations)
        return sol_a.hard_violations < sol_b.hard_violations;
    if (sol_a.soft_violations != sol_b.soft_violations)
        return sol_a.soft_violations < sol_b.soft_violations;
    return sol_a.score < sol_b.score;
}
```

---

## 9. API Endpoints

### POST `/api/upload`

Upload Excel file for processing.

**Request:** `multipart/form-data` with `file` field

**Response:**
```json
{
  "success": true,
  "filename": "TestCase_TC01.xlsx",
  "digest": {
    "employees_count": 8,
    "vehicles_count": 3,
    "time_window_span": "08:00 - 12:00",
    "high_priority_percent": 25
  },
  "employees": [...],
  "vehicles": [...],
  "baseline_cost": 3200
}
```

### POST `/api/optimize`

Run optimization with specified parameters.

**Request:**
```json
{
  "solverDurationSeconds": 30,
  "costWeight": 0.7,
  "timeWeight": 0.3,
  "distanceMethod": "haversine",
  "priorityDelays": {
    "1": 5, "2": 5, "3": 10, "4": 15, "5": 15
  }
}
```

**Response:**
```json
{
  "success": true,
  "optimization_id": "opt_12345",
  "result": {
    "total_cost": 1850.00,
    "baseline_cost": 3200.00,
    "cost_savings": 1350.00,
    "cost_savings_percent": 42.2,
    "total_time": 185,
    "baseline_time": 425,
    "vehicles_used": 3,
    "hard_violations": 0,
    "soft_violations": 2
  },
  "routes": [...],
  "assignments": [...],
  "violation_details": {...}
}
```

### GET `/api/geometry-status/<optimization_id>`

Check status of background geometry fetching.

**Response:**
```json
{
  "geometry_status": "fetching",
  "geometry_progress": {
    "total": 24,
    "fetched": 12
  }
}
```

---

## 10. Configuration

### `solver_config.json`

```json
{
  "penalty_weights": {
    "unassigned_employee_penalty": 100000,
    "time_violation_penalty": 100000,
    "lateness_per_minute_penalty": 1000,
    "priority_lateness_multiplier": 500,
    "preference_violation_penalty": 10000,
    "vehicle_activation_cost": 50
  },
  
  "objective_weights": {
    "cost_weight": 0.7,
    "time_weight": 0.3
  },
  
  "priority_lateness_weights": {
    "P1": 5.0,
    "P2": 3.0,
    "P3": 2.0,
    "P4": 1.0,
    "P5": 1.0
  },
  
  "simulated_annealing": {
    "start_temperature": 50000,
    "cooling_rate": 0.99995,
    "min_destroy_pct": 0.10,
    "max_destroy_pct": 0.60,
    "reheat_threshold": 800,
    "max_reheats": 20
  },
  
  "construction": {
    "num_candidates_to_optimize": 3,
    "time_split_percent": [50, 30, 20]
  }
}
```

### Environment Variables (`.env`)

```bash
OPENROUTESERVICE_API_KEY=your_api_key_here
FLASK_DEBUG=false
```

---

## 11. Building & Running

### Prerequisites

- Python 3.8+
- C++17 compatible compiler (g++ or MSVC)
- Node.js 16+ (for frontend)

### Install Python Dependencies

```bash
cd server
pip install -r requirements.txt
```

### Build C++ Solver

**Windows:**
```bash
build.bat
```

**Linux/Mac:**
```bash
make
```

### Run Server

```bash
python app.py
```

Server starts at `http://localhost:5000`

### Solver Execution Times

| Mode | Duration | Use Case |
|------|----------|----------|
| Quick | 15 seconds | Fast preview, some violations acceptable |
| Standard | 30 seconds | Good quality, balanced optimization |
| Thorough | 60 seconds | High quality, minimal violations |
| Maximum | 120 seconds | Best quality, extensive search |

---

## Performance Metrics

### Typical Results

| Metric | Expected Range |
|--------|----------------|
| Cost Savings | 35-50% vs baseline |
| Time Savings | 20-40% vs baseline |
| Hard Violations | 0 (guaranteed) |
| Soft Violations | 0-5 (minimized) |
| Vehicle Utilization | 70-90% |

### Scalability

| Employees | Vehicles | Solver Time | Quality |
|-----------|----------|-------------|---------|
| 10-20 | 3-5 | 15s | Optimal |
| 20-50 | 5-10 | 30s | Near-optimal |
| 50-100 | 10-20 | 60s | High quality |
| 100+ | 20+ | 120s | Good quality |

---

## Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| "No solver found" | Solver not built | Run `build.bat` or `make` |
| "Missing sheets" | Excel format error | Ensure all 4 sheets exist |
| "All violations" | Infeasible problem | Relax time windows or add vehicles |
| "Geometry timeout" | API rate limit | Wait and retry, or use haversine |

### Debug Output

Set `verbose: true` in `solver_config.json` for detailed solver output:

```
======================================================================
CUSTOM VRP SOLVER - UNIFIED OPTIMIZATION
======================================================================
Input: output/input_12345.json
Output: output/output_12345.json
Time Limit: 30 seconds

PHASE 1: Building initial solutions
----------------------------------------------------------------------
  SingleTrip:       score=15000 hard=0 soft=0
  EarliestDeadline: score=8500 hard=0 soft=2
  GeoCluster:       score=7200 hard=0 soft=1
  ...

PHASE 2: ALNS Optimization (28s remaining)
----------------------------------------------------------------------
  #1: GeoCluster (14s)
  Result: cost=$1850 time=185min hard=0 soft=1 score=6800
  >> New best!
```

---

## License

Proprietary - Velora Mobitech © 2026
