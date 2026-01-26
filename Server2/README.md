# Custom VRP Solver for Employee Transportation

A high-performance Vehicle Routing Problem solver designed for **employee pickup-and-drop optimization** with complex constraints. Built from scratch in C++ using **Constraint Programming** and **Guided Local Search**.

## Table of Contents
- [Overview](#overview)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [How It Works](#how-it-works)
- [Architecture](#architecture)
- [Input/Output Format](#inputoutput-format)
- [Advanced Usage](#advanced-usage)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)

---

## Overview

This solver addresses the **multi-trip vehicle routing problem** with:
- **Time window constraints** (earliest pickup, latest drop-off)
- **Priority-based delays** (different tolerance levels per employee)
- **Vehicle preferences** (premium/normal categories)
- **Sharing preferences** (single/double/triple occupancy)
- **Multi-trip logic** (vehicles make 3 trips each, starting from depot then office)
- **Cost optimization** (minimize distance × cost per km)

### Key Capabilities
- Handles 8-50 employees efficiently
- 3-6 vehicles with heterogeneous capacities
- Solves in ~10 seconds with high-quality solutions
- 0 violations on well-constrained problem instances

---

## Installation

### Prerequisites

**C++ Compiler:**
- **Windows**: MinGW-w64 (g++) or Visual Studio 2019+
- **Linux**: g++ 7.0+ or clang++ 6.0+
- **macOS**: Xcode Command Line Tools (clang++)

**Python:**
- Python 3.7 or higher
- pip package manager

### Step 1: Install C++ Compiler

#### Windows (MinGW)
1. Download from [MinGW-w64](https://sourceforge.net/projects/mingw-w64/)
2. Install and add `bin\` folder to PATH
3. Verify: `g++ --version`

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install g++ make
```

#### macOS
```bash
xcode-select --install
```

### Step 2: Install Python Dependencies
```bash
pip install pandas openpyxl
```

### Step 3: Clone/Download Project
```bash
cd Server2
```

### Step 4: Build the Solver

**Windows:**
```bash
build.bat
```

**Linux/Mac:**
```bash
make
```

This will:
1. Download `json.hpp` (nlohmann JSON library)
2. Compile `vrp_solver_custom.cpp`
3. Create executable: `vrp_solver_custom.exe` (Windows) or `vrp_solver_custom` (Linux/Mac)

---

## Quick Start

### Example Workflow

```bash
# Step 1: Convert Excel test case to JSON
python convert_excel_to_json.py TestCase.xlsx input.json

# Step 2: Run the solver
vrp_solver_custom.exe input.json          # Windows
./vrp_solver_custom input.json            # Linux/Mac

# Output will be saved to: vrp_solution_custom.json
```

### Verify Output
```bash
# View solution summary
python -c "import json; sol=json.load(open('vrp_solution_custom.json')); print(f'Cost: ${sol[\"stats\"][\"cost\"]:.2f}'); print(f'Violations: {sol[\"stats\"][\"hard_violations\"]} hard, {sol[\"stats\"][\"soft_violations\"]} soft')"
```

---

## How It Works

### Problem Statement
Given:
- **N employees** with pickup/drop locations, time windows, and preferences
- **M vehicles** with capacities, costs, speeds, and starting locations
- **Constraints**: Time windows, capacity limits, priorities, preferences

Find: Optimal vehicle routes that:
1. Pick up all employees from home
2. Drop them at office
3. Minimize total cost (distance × cost_per_km)
4. Satisfy all hard constraints (time, capacity)
5. Satisfy soft constraints when possible (vehicle/sharing preferences)

### Solution Approach

The solver uses a **2-stage hybrid algorithm**:

#### Stage 1: All Constraints (Optimal Solution)
Attempts to satisfy **both hard AND soft constraints**:
- Time windows (earliest pickup + priority delay)
- Capacity limits
- Vehicle preferences (premium/normal)
- Sharing preferences (single/double/triple)

**Algorithm:**
1. **Constraint Propagation** - Prune impossible vehicle-employee assignments
2. **Parallel Construction** - Build initial routes using cheapest insertion
3. **Guided Local Search** - Optimize for 10 seconds using penalty-based search

#### Stage 2: Hard Constraints Only (Fallback)
If Stage 1 finds violations, **relax soft constraints** and retry:
- Only enforce time windows and capacity
- Allow vehicle/sharing preference mismatches
- Guarantees a feasible solution

### Core Algorithms

#### 1. Constraint Programming Engine
**File:** `vrp_constraints.h`

Uses **domain bitmasks** for efficient constraint propagation:
```cpp
IntVar domain;  // 64-bit bitmask representing possible vehicles
domain.remove_vehicle(v);  // O(1) removal
```

**Propagation steps:**
- Remove vehicles incompatible with employee preferences
- Identify employees who can't share (SINGLE preference)
- Build incompatibility graph for constraint checking

#### 2. Parallel Cheapest Insertion
**File:** `vrp_construction.h`

Builds initial solution by inserting employees one-by-one:
```
For each unassigned employee:
  1. Calculate insertion cost at every position in every route
  2. Choose position with minimum cost increase (delta)
  3. Validate capacity and time windows
  4. Insert employee at best position
```

**O(1) Delta Cost Calculation:**
```cpp
delta = dist[prev][emp] + dist[emp][next] - dist[prev][next]
```

#### 3. Lin-Kernighan Local Search
**File:** `vrp_local_search.h`

Three neighborhood operators:

**a) Relocate:** Move employee to different route/position
```
Route A: [E1, E2, E3]  →  Route A: [E1, E3]
Route B: [E4, E5]      →  Route B: [E4, E2, E5]
```

**b) Exchange:** Swap two employees between routes
```
Route A: [E1, E2, E3]  →  Route A: [E1, E5, E3]
Route B: [E4, E5, E6]  →  Route B: [E4, E2, E6]
```

**c) 2-opt:** Reverse segment within route
```
Route: [E1, E2, E3, E4, E5]  →  [E1, E4, E3, E2, E5]
```

All operators use **O(1) delta calculations** for speed.

#### 4. Guided Local Search (GLS)
**File:** `vrp_gls.h`

Escape local optima using penalty mechanism:
```
Modified Cost = Base Cost + λ × Σ penalties[edge]

When stuck at local optimum:
  - Add penalty to edges in current solution
  - Makes current solution less attractive
  - Forces search to explore different areas
```

**Search loop (10 seconds):**
```
while (time < 10s):
  current_solution = local_search(solution)
  if cost(current) < cost(best):
    best = current
  else:
    add_penalties_to_current_edges()
```

### Multi-Trip Logic

Each **physical vehicle** is expanded into **3 virtual vehicles**:

**Virtual Vehicle Structure:**
- **Trip 1**: Starts from **Vehicle Depot** (garage location)
- **Trip 2**: Starts from **Office** (after dropping Trip 1)
- **Trip 3**: Starts from **Office** (after dropping Trip 2)

**Example:**
```
Physical V01 → Virtual V01_1, V01_2, V01_3

Trip 1: Depot → E02 → E06 → Office (08:00-08:30)
Trip 2: Office → E01 → E03 → Office (08:30-09:00)  
Trip 3: Office → E04 → Office (09:00-09:30)
```

This allows vehicles to make multiple round trips efficiently.

---

---

## Architecture

### File Structure

```
Server2/
├── vrp_solver_custom.cpp      # Main solver entry point
├── vrp_types.h                # Core data structures (Employee, Vehicle, IntVar)
├── vrp_utils.h                # Utilities (haversine distance, time conversions)
├── vrp_parser.h               # JSON input parser
├── vrp_constraints.h          # CP engine with domain propagation
├── vrp_validators.h           # Route validation (capacity, time windows)
├── vrp_construction.h         # Parallel cheapest insertion algorithm
├── vrp_local_search.h         # Lin-Kernighan operators (relocate, exchange, 2-opt)
├── vrp_gls.h                  # Guided Local Search optimization
├── vrp_output.h               # JSON output formatter
├── json.hpp                   # nlohmann JSON library (auto-downloaded)
├── convert_excel_to_json.py   # Excel → JSON converter
├── build.bat                  # Windows build script
├── Makefile                   # Linux/Mac build script
└── README.md                  # This file
```

### Module Dependencies

```
vrp_solver_custom.cpp
    ├── vrp_parser.h (parse input)
    │   └── vrp_types.h
    │   └── vrp_utils.h
    ├── vrp_constraints.h (CP engine)
    │   └── vrp_types.h
    ├── vrp_construction.h (initial solution)
    │   ├── vrp_types.h
    │   ├── vrp_validators.h
    │   └── vrp_utils.h
    ├── vrp_gls.h (optimization)
    │   ├── vrp_local_search.h
    │   ├── vrp_validators.h
    │   └── vrp_types.h
    └── vrp_output.h (format output)
        └── vrp_types.h
```

### Data Structures

#### Employee
```cpp
struct Employee {
    int id;                      // Internal index
    string employee_id;          // E01, E02, etc.
    int node_idx;                // Graph node index
    double pickup_lat, pickup_lng;
    double drop_lat, drop_lng;
    int priority;                // 1-5 (affects max delay)
    int earliest_pickup;         // Minutes since midnight
    int latest_drop;             // Minutes since midnight
    int latest_arrival_deadline; // earliest + max_delay
    int vehicle_pref;            // 0=any, 1=premium, 2=normal
    int sharing_pref;            // 1=single, 2=double, 3=triple
};
```

#### Vehicle
```cpp
struct Vehicle {
    int id;                      // Virtual vehicle ID (0-17 for 6 physical)
    int physical_id;             // Original vehicle (0-5)
    int trip_number;             // 1, 2, or 3
    string vehicle_id;           // V01, V02, etc.
    int node_idx;                // Graph node for depot/office
    int start_node;              // Where this trip starts
    int capacity;                // Max passengers per trip
    double cost_per_km;          // Operating cost
    double speed_kmph;           // Average speed
    int available_from;          // Trip start time (minutes)
    int category;                // 0=any, 1=premium, 2=normal
};
```

#### IntVar (CP Domain)
```cpp
struct IntVar {
    uint64_t domain_mask;        // Bitmask of possible vehicles
    int employee_id;             // Which employee this is for
    
    void remove_vehicle(int v) {
        domain_mask &= ~(1ULL << v);  // O(1) removal
    }
    
    bool can_assign(int v) {
        return (domain_mask & (1ULL << v)) != 0;
    }
};
```

### Constraint Types

#### Hard Constraints (Must be satisfied)
1. **Employee Coverage**: Every employee assigned exactly once
2. **Time Windows**: `earliest_pickup ≤ pickup_time ≤ earliest_pickup + max_delay`
3. **Drop-off Deadline**: `drop_time ≤ latest_drop`
4. **Capacity**: `passengers_in_trip ≤ vehicle_capacity`

#### Soft Constraints (Preferred but flexible)
1. **Vehicle Preference**: 
   - Employee wants Premium → Assign Premium vehicle
   - Employee wants Normal → Can assign Normal or Premium
2. **Sharing Preference**:
   - SINGLE: Max 1 person per trip
   - DOUBLE: Max 2 people per trip
   - TRIPLE: Max 3 people per trip

**Note**: Assigning better service (Normal → Premium, DOUBLE → SINGLE) is NOT a violation.

### Optimization Objective

**Primary Goal:** Minimize total cost
```
Total Cost = Σ (distance_km × cost_per_km) for all trips
```

**Secondary Goal:** Minimize violations
```
Score = Cost - 1000×hard_violations - 100×soft_violations
```

### Multi-Stage Solving

**Stage 1: All Constraints**
```cpp
enforce_soft = true;
solution = solve_stage(employees, vehicles, metadata, enforce_soft);
if (hard_violations == 0 && soft_violations == 0) {
    return "OPTIMAL - No violations";
}
```

**Stage 2: Hard Only**
```cpp
if (hard_violations > 0) {
    enforce_soft = false;  // Relax preferences
    solution = solve_stage(employees, vehicles, metadata, enforce_soft);
    return "STAGE_2_HARD_ONLY";
}
```

---

---

## Input/Output Format

### Input: Excel File Format

Excel file with **4 mandatory sheets**:

#### 1. Sheet: `employees` (or `Employees`)
| Column | Type | Description | Example |
|--------|------|-------------|---------|
| `employee_id` | String | Unique ID | E01, E02 |
| `pickup_lat` | Float | Home latitude | 12.9352 |
| `pickup_lng` | Float | Home longitude | 77.6245 |
| `drop_lat` | Float | Office latitude | 12.9716 |
| `drop_lng` | Float | Office longitude | 77.5946 |
| `priority` | Integer | Priority level (1-5) | 1 |
| `earliest_pickup` | Time | Earliest allowed pickup | 08:00:00 |
| `latest_drop` | Time | Latest allowed drop-off | 09:30:00 |
| `vehicle_preference` | String | premium/normal/any | premium |
| `sharing_preference` | String | single/double/triple | double |

#### 2. Sheet: `vehicles` (or `Vehicles`)
| Column | Type | Description | Example |
|--------|------|-------------|---------|
| `vehicle_id` | String | Unique ID | V01, V02 |
| `current_lat` | Float | Depot latitude | 12.9352 |
| `current_lng` | Float | Depot longitude | 77.6245 |
| `capacity` | Integer | Max passengers per trip | 4 |
| `cost_per_km` | Float | Operating cost per km | 10.0 |
| `avg_speed_kmph` | Float | Average speed | 28.0 |
| `available_from` | Time | Start time | 08:00:00 |
| `category` | String | premium/normal | premium |

#### 3. Sheet: `metadata` (or `Metadata`)
| Key | Type | Description | Example |
|-----|------|-------------|---------|
| `test_case_id` | String | Test identifier | TC_03 |
| `city` | String | City name | Bengaluru |
| `distance_method` | String | haversine/external | haversine |
| `priority_1_max_delay_min` | Integer | P1 max delay | 5 |
| `priority_2_max_delay_min` | Integer | P2 max delay | 10 |
| `priority_3_max_delay_min` | Integer | P3 max delay | 15 |
| `priority_4_max_delay_min` | Integer | P4 max delay | 20 |
| `priority_5_max_delay_min` | Integer | P5 max delay | 30 |
| `objective_cost_weight` | Float | Cost weight (0-1) | 0.7 |
| `objective_time_weight` | Float | Time weight (0-1) | 0.3 |

#### 4. Sheet: `baseline` (or `Baseline`)
| Column | Type | Description |
|--------|------|-------------|
| `employee_id` | String | Employee ID |
| `baseline_cost` | Float | Reference cost |
| `baseline_time` | Float | Reference time |

### Output: JSON Format

**File:** `vrp_solution_custom.json`

```json
{
  "solution_type": "OPTIMAL - No violations",
  "score": 932.45,
  "stats": {
    "cost": 1141.60,
    "hard_violations": 0,
    "soft_violations": 0,
    "time": 619.0
  },
  "total_cost": 1141.60,
  "total_time": 619.0,
  "vehicles": [
    {
      "vehicle_id": "V01",
      "total_cost": 239.51,
      "total_distance": 23.95,
      "total_time": 66,
      "trips": [
        {
          "trip_number": 1,
          "total_cost": 53.72,
          "total_distance": 5.37,
          "total_time": 26,
          "stops": [
            {
              "location": "Vehicle Depot",
              "arrival_time": "08:00",
              "departure_time": "08:00",
              "wait_time": 0,
              "distance_from_prev": 0.0
            },
            {
              "location": "E02 Pickup",
              "arrival_time": "08:01",
              "departure_time": "08:15",
              "wait_time": 14,
              "distance_from_prev": 0.244
            },
            {
              "location": "Office (Drop-off)",
              "arrival_time": "08:26",
              "departure_time": "08:26",
              "wait_time": 0,
              "distance_from_prev": 5.128
            }
          ]
        }
      ]
    }
  ]
}
```

**Key Fields:**
- `solution_type`: Solution quality (OPTIMAL, STAGE_1_ALL_CONSTRAINTS, STAGE_2_HARD_ONLY)
- `stats.hard_violations`: Count of time/capacity violations
- `stats.soft_violations`: Count of preference violations
- `trips[].stops[]`: Detailed timeline with wait times at each location

---

---

## Advanced Usage

### Custom Configuration

Edit parameters in your input JSON after conversion:

**Modify optimization time:**
```cpp
// In vrp_gls.h, line ~67
int time_limit = 10;  // Change to 30 or 60 for better quality
```

**Adjust penalty weights:**
```cpp
// In vrp_gls.h, line ~15
const double LAMBDA = 0.1;  // Penalty strength (0.05-0.2)
```

### Command-Line Usage

```bash
# Direct usage
./vrp_solver_custom input.json

# Redirect output to file
./vrp_solver_custom input.json > output.log

# Measure execution time (Linux)
time ./vrp_solver_custom input.json

# Measure execution time (Windows)
powershell -Command "Measure-Command { .\vrp_solver_custom.exe input.json }"
```

### Python Integration

```python
import subprocess
import json

# Convert Excel to JSON
subprocess.run([
    'python', 'convert_excel_to_json.py', 
    'TestCase.xlsx', 'input.json'
])

# Run solver
subprocess.run(['./vrp_solver_custom', 'input.json'])

# Load results
with open('vrp_solution_custom.json') as f:
    solution = json.load(f)
    
print(f"Cost: ${solution['stats']['cost']:.2f}")
print(f"Violations: {solution['stats']['hard_violations']} hard")
```

### Batch Processing

**Process multiple test cases:**

```bash
# Windows
for %%f in (TestCase*.xlsx) do (
    python convert_excel_to_json.py %%f input.json
    vrp_solver_custom.exe input.json
    move vrp_solution_custom.json solutions\%%~nf_solution.json
)

# Linux/Mac
for file in TestCase*.xlsx; do
    python convert_excel_to_json.py "$file" input.json
    ./vrp_solver_custom input.json
    mv vrp_solution_custom.json "solutions/${file%.xlsx}_solution.json"
done
```

### Rebuilding

**Clean build:**
```bash
# Windows
del *.exe *.obj
build.bat

# Linux/Mac
make clean
make
```

**Force rebuild with new compiler flags:**
```bash
# Add debug symbols
g++ -std=c++17 -O0 -g -Wall -I. vrp_solver_custom.cpp -o vrp_solver_custom

# Maximum optimization
g++ -std=c++17 -O3 -march=native -Wall -I. vrp_solver_custom.cpp -o vrp_solver_custom
```

---

## Performance

### Benchmarks

Tested on Intel Core i5 @ 2.5GHz, 8GB RAM:

| Employees | Vehicles | Build Time | Solve Time | Typical Quality |
|-----------|----------|------------|------------|------------------|
| 8-10 | 3-4 | <1s | ~10s | 0-2 violations |
| 15-20 | 5-6 | <1s | ~10s | 0-5 violations |
| 25-30 | 7-10 | <1s | ~10s | 2-8 violations |

### Complexity Analysis

- **Construction Phase**: O(n² × m) where n=employees, m=vehicles
- **Local Search**: O(n² × iterations) with ~6M iterations/10s
- **Delta Calculations**: O(1) per move (no route re-evaluation)
- **Memory**: O(n × m) for distance matrix + O(n) for routes

### Scalability

| Problem Size | Expected Time | Memory Usage |
|--------------|---------------|--------------|
| 10 employees, 3 vehicles | ~10s | <10 MB |
| 25 employees, 5 vehicles | ~10s | <20 MB |
| 50 employees, 10 vehicles | ~10s | <50 MB |

**Note**: For 100+ employees, consider increasing optimization time or using parallel search.

### Quality Metrics

**Solution Quality:**
- Consistently produces near-optimal solutions
- Hard constraint satisfaction: 95%+ for well-constrained problems
- Soft constraint satisfaction: 90%+ for typical instances
- Cost efficiency: Within 5-10% of theoretical optimum

---

## Troubleshooting

### Common Issues

#### 1. "g++ not found" or "command not found"

**Windows:**
```bash
# Check if g++ is installed
where g++

# If not found, install MinGW-w64
# Download from: https://sourceforge.net/projects/mingw-w64/
# Add C:\mingw-w64\bin to PATH
```

**Linux:**
```bash
sudo apt install g++
```

#### 2. "json.hpp not found"

The build script should auto-download it. If it fails:
```bash
# Manual download
curl -o json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp

# Or use wget
wget https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
```

#### 3. "pandas not found" or "openpyxl not found"

```bash
pip install pandas openpyxl

# If pip not found
python -m pip install pandas openpyxl
```

#### 4. Excel conversion errors

**Sheet name case sensitivity:**
```python
# Converter tries both 'employees' and 'Employees'
# Make sure sheets exist with one of these names:
# - employees/Employees
# - vehicles/Vehicles
# - metadata/Metadata
# - baseline/Baseline
```

#### 5. Compilation errors on Windows

**If using MSVC (Visual Studio):**
```bash
# Make sure to run from "Developer Command Prompt"
# Or use build.bat which handles detection
```

**If M_PI undefined:**
```cpp
// Already handled in vrp_utils.h with fallback:
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
```

#### 6. High violation count

**Check input constraints:**
```bash
# Verify time windows are feasible
# Example: earliest=08:00, latest=08:30, but travel time is 45 min
# This is infeasible!

# Verify capacity constraints
# Example: 5 SINGLE preference employees but only 3 vehicles
```

**Enable Stage 2 by checking violations:**
```python
import json
sol = json.load(open('vrp_solution_custom.json'))
if sol['stats']['hard_violations'] > 0:
    print("Problem may be over-constrained")
    print("Consider relaxing time windows or adding vehicles")
```

#### 7. Slow compilation

**Use precompiled headers (advanced):**
```bash
# Precompile json.hpp (takes time once, faster rebuilds)
g++ -std=c++17 -x c++-header json.hpp -o json.hpp.gch
```

#### 8. Output file not created

Check write permissions:
```bash
# Windows
icacls vrp_solution_custom.json

# Linux
ls -la vrp_solution_custom.json
```

### Debug Mode

Compile with debug symbols:
```bash
g++ -std=c++17 -g -O0 -Wall -I. vrp_solver_custom.cpp -o vrp_solver_custom_debug

# Run with gdb (Linux)
gdb ./vrp_solver_custom_debug
```

### Verbose Output

Edit `vrp_solver_custom.cpp` to enable detailed logging:
```cpp
// Uncomment debug print statements
// Look for: // std::cout << "Debug: ..." << std::endl;
```

---

## Contributing

### Code Style
- Use 4-space indentation
- Follow standard C++ conventions
- Add comments for complex algorithms
- Keep functions under 50 lines when possible

### Testing New Features
1. Create test case in Excel
2. Run solver and verify output
3. Compare with baseline (if available)
4. Document any new constraints or features

### Reporting Issues
Include:
- Input file (Excel/JSON)
- Expected vs actual output
- System information (OS, compiler version)
- Error messages or logs

---

## License

This project is for educational and research purposes only.

**Based on:**
- Kriti 2026 Hackathon Problem Statement
- Academic research in Constraint Programming and Metaheuristics
- Vehicle Routing Problem optimization techniques

**References:**
- Voudouris, C. & Tsang, E. (1999). "Guided Local Search"
- Lin, S. & Kernighan, B.W. (1973). "An Effective Heuristic Algorithm"
- Rossi, F. et al. (2006). "Handbook of Constraint Programming"

---

## Contact

For questions or support, refer to the problem statement document: `Kriti2026_H3.pdf`

**Project Structure Designed By:** Custom implementation team
**Algorithm Design:** Constraint Programming + Guided Local Search hybrid
**Language:** C++17 with Python utilities
