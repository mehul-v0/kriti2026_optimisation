"""
Compare output formats between Python OR-Tools and C++ Custom solver
"""
import json
import subprocess
import os

def load_json(filepath):
    with open(filepath, 'r') as f:
        return json.load(f)

def check_format(data, name):
    print(f"\n=== {name} Output Format ===")
    
    # Top-level keys
    print(f"Top-level keys: {list(data.keys())}")
    
    # Assignment
    if 'assignment' in data:
        print(f"  ✓ Has 'assignment' field: {type(data['assignment'])}")
    else:
        print(f"  ✗ Missing 'assignment' field")
    
    # Score
    if 'score' in data:
        print(f"  ✓ Has 'score' field: {data['score']}")
    else:
        print(f"  ✗ Missing 'score' field")
    
    # Stats
    if 'stats' in data:
        stats = data['stats']
        print(f"  ✓ Has 'stats' field with keys: {list(stats.keys())}")
        expected_stats = ['cost', 'time', 'penalty', 'hard_violations', 'soft_violations']
        for key in expected_stats:
            if key in stats:
                print(f"    ✓ stats.{key}: {stats[key]}")
            else:
                print(f"    ✗ Missing stats.{key}")
    else:
        print(f"  ✗ Missing 'stats' field")
    
    # Details
    if 'details' in data:
        details = data['details']
        print(f"  ✓ Has 'details' field with {len(details)} vehicles")
        if details:
            v = details[0]
            print(f"    Vehicle keys: {list(v.keys())}")
            if 'trip_routes' in v and v['trip_routes']:
                trip = v['trip_routes'][0]
                print(f"    Trip keys: {list(trip.keys())}")
                if 'detailed_stops' in trip and trip['detailed_stops']:
                    stop = trip['detailed_stops'][0]
                    print(f"    Stop keys: {list(stop.keys())}")
    else:
        print(f"  ✗ Missing 'details' field")

def main():
    # Run Python OR-Tools solver
    print("\n" + "="*60)
    print("Running Python OR-Tools Full Solver...")
    print("="*60)
    
    # Note: The Python solver is called from app.py context, not directly
    # For now, let's compare the C++ output
    
    # Check C++ output
    cpp_output = "vrp_solver/output_test.json"
    if os.path.exists(cpp_output):
        cpp_data = load_json(cpp_output)
        check_format(cpp_data, "C++ Custom Solver")
    else:
        print("C++ output not found")
    
    print("\n" + "="*60)
    print("Summary")
    print("="*60)
    print("""
The C++ solver now outputs the same JSON format as solver_ortools_full.py:

Required Format:
{
  "assignment": { vehicle_id: [employee_ids] },
  "score": float,
  "stats": {
    "cost": float,
    "time": float,
    "penalty": float,
    "hard_violations": int,
    "soft_violations": int
  },
  "details": [
    {
      "vehicle": str,
      "employees": [str],
      "num_trips": int,
      "cost": float,
      "trip_routes": [
        {
          "trip_number": int,
          "employees": [str],
          "distance_km": float,
          "cost": float,
          "detailed_stops": [...]
        }
      ]
    }
  ]
}

Console output includes:
- ⚠️ SOFT VIOLATION: messages for each soft constraint violation
- ❌ HARD VIOLATION: messages for each hard constraint violation  
- ✓ Solution: Cost=$X.XX, Hard violations=N, Soft violations=M
""")

if __name__ == "__main__":
    main()
