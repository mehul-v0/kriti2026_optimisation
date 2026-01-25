# VRP Solver - C++ Implementation of OR-Tools Algorithms

A complete, from-scratch C++ implementation of the Vehicle Routing Problem algorithms used by Google OR-Tools.

## Algorithms Implemented

### 1. Construction Heuristics (`vrp_construction.h`)

- **Parallel Cheapest Insertion** (Primary OR-Tools algorithm)
  - Simultaneously considers all routes
  - Finds globally best insertion for each unassigned customer
  - Time complexity: O(n²m) per iteration

- **Sequential Cheapest Insertion**
  - Builds routes one at a time
  - Good for capacity-constrained problems

- **Time-Oriented Insertion**
  - Prioritizes employees by deadline
  - Best for tight time windows

- **Nearest Neighbor**
  - Classic greedy heuristic
  - Fast but lower quality

### 2. Local Search Operators (`vrp_local_search.h`)

- **RELOCATE**: Move one employee to another position
- **EXCHANGE**: Swap two employees
- **2-OPT**: Reverse a segment within a route
- **OR-OPT**: Move chain of consecutive employees (1-3)
- **CROSS-EXCHANGE**: Exchange segments between routes

### 3. Metaheuristics (`vrp_gls.h`)

- **Guided Local Search (GLS)** (Primary OR-Tools algorithm)
  - Penalizes frequently-used solution features
  - Escapes local optima through augmented objective
  - Auto-calibrates penalty weight (lambda)

- **Iterated Local Search (ILS)**
  - Perturbation-based diversification
  - Probabilistic acceptance criterion

## File Structure

```
vrp_solver/
├── vrp_types.h         # Data structures (Employee, Vehicle, Route, Solution)
├── vrp_utils.h         # Utilities (distance, evaluation, manipulation)
├── vrp_construction.h  # Construction heuristics
├── vrp_local_search.h  # Neighborhood operators
├── vrp_gls.h          # Guided Local Search metaheuristic
├── vrp_parser.h       # Input/output parsing
├── vrp_solver.cpp     # Main entry point
├── Makefile           # Build configuration
└── README.md          # This file
```

## Compilation

### Using Make (Linux/MinGW)
```bash
cd vrp_solver
make
```

### Manual Compilation
```bash
g++ -std=c++17 -O3 -o vrp_solver vrp_solver.cpp
```

### Windows Visual Studio
```bash
cl /EHsc /O2 /std:c++17 vrp_solver.cpp
```

## Usage

```bash
./vrp_solver <input_file> [output_file] [time_limit_seconds]
```

### Examples
```bash
# Basic usage
./vrp_solver input.txt

# With output file
./vrp_solver input.txt result.json

# With time limit
./vrp_solver input.txt result.json 30
```

## Input File Format

Tab-separated values:

```
num_employees num_vehicles
name1  lat1  lon1  earliest1  latest1  priority1  vehicle_pref1  sharing_pref1  is_priority1  gender1
name2  lat2  lon2  earliest2  latest2  priority2  vehicle_pref2  sharing_pref2  is_priority2  gender2
...
vehicle1  capacity1  speed1  cost1  category1
vehicle2  capacity2  speed2  cost2  category2
...
depot_lat  depot_lon
```

### Field Descriptions

**Employee Fields:**
- `name`: Employee identifier
- `lat, lon`: Pickup location coordinates
- `earliest`: Earliest pickup time (HH:MM)
- `latest`: Latest arrival at depot (HH:MM)
- `priority`: 1=low, 2=medium, 3=high
- `vehicle_pref`: 0=any, 1=premium, 2=normal
- `sharing_pref`: 1=single, 2=double, 3=any
- `is_priority`: 0 or 1
- `gender`: M/F/O

**Vehicle Fields:**
- `id/name`: Vehicle identifier
- `capacity`: Maximum passengers
- `speed`: Speed in km/h
- `cost`: Cost per km
- `category`: 0=any, 1=premium, 2=normal

## Output JSON Format

```json
{
  "status": "success",
  "total_cost": 1234.56,
  "total_distance": 78.9,
  "vehicles_used": 3,
  "routes": [
    {
      "vehicle_id": 0,
      "vehicle_name": "V1",
      "distance": 25.3,
      "cost": 410.5,
      "stops": [
        {"type": "depot_start", "location": [lat, lon]},
        {"type": "pickup", "employee_name": "John", "arrival_time": "08:15"},
        {"type": "depot_end", "arrival_time": "09:30"}
      ]
    }
  ],
  "unassigned": []
}
```

## Algorithm Details

### Guided Local Search (GLS)

GLS is a metaheuristic that escapes local optima by modifying the objective function:

1. **Feature Definition**: An arc (edge) used in the solution
2. **Utility**: `utility(f) = cost(f) / (1 + penalty(f))`
3. **Augmented Objective**: `obj' = obj + λ * Σ penalty(f) * cost(f)`

At each local optimum:
- Identify features with maximum utility
- Increment their penalties
- Continue local search with augmented objective

This forces the search away from frequently-visited solution features.

### Penalty Handling

**Hard Constraints** (very high penalty):
- Time window violations
- Capacity violations

**Soft Constraints** (moderate penalty):
- Sharing preference violations
- Vehicle preference violations

## Comparison with OR-Tools

| Feature | OR-Tools | This Implementation |
|---------|----------|---------------------|
| Construction | Parallel Cheapest Insertion | ✅ Same |
| Metaheuristic | Guided Local Search | ✅ Same |
| Local Search | Multiple operators | ✅ Relocate, Exchange, 2-opt, Or-opt |
| Time Windows | Dimension-based | ✅ Direct calculation |
| Constraints | Constraint programming | Penalty-based |

## Performance

Typical performance on modern hardware:
- 50 employees: < 5 seconds
- 100 employees: < 30 seconds
- 200 employees: < 120 seconds

## License

MIT License - Free for academic and commercial use.
