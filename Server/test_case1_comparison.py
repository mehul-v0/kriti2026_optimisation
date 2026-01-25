"""
Compare outputs from Python OR-Tools and Custom C++ solver using test_case1.txt
"""

import json
import subprocess
import os
import pandas as pd
from solver import parse_excel_file
import solver_ortools_full
from app import export_full_constraints_to_cpp

def parse_test_case_txt(filepath):
    """Parse the test_case1.txt format"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    data = {
        'employees': [],
        'vehicles': [],
        'baseline': [],
        'metadata': []
    }
    
    # Split by double newlines to get sections
    sections = content.split('\n\n')
    
    for section_text in sections:
        lines = section_text.strip().split('\n')
        if not lines or not lines[0].strip():
            continue
        
        header = lines[0].strip().split('\t')
        
        # Employee section
        if header[0] == 'employee_id' and len(header) > 5 and header[1] == 'priority':
            for line in lines[1:]:
                line = line.strip()
                if not line:
                    continue
                parts = line.split('\t')
                if len(parts) >= 10:
                    emp = {
                        'employee_id': parts[0],
                        'priority': int(parts[1]),
                        'pickup_lat': float(parts[2]),
                        'pickup_lng': float(parts[3]),
                        'drop_lat': float(parts[4]),
                        'drop_lng': float(parts[5]),
                        'earliest_pickup': parts[6],
                        'latest_drop': parts[7],
                        'vehicle_preference': parts[8],
                        'sharing_preference': parts[9],
                        'service_time_min': 2
                    }
                    data['employees'].append(emp)
        
        # Vehicle section
        elif header[0] == 'vehicle_id' and len(header) > 5:
            for line in lines[1:]:
                line = line.strip()
                if not line:
                    continue
                parts = line.split('\t')
                if len(parts) >= 10:
                    veh = {
                        'vehicle_id': parts[0],
                        'fuel_type': parts[1],
                        'vehicle_type': parts[2],
                        'capacity': int(parts[3]),
                        'cost_per_km': float(parts[4]),
                        'avg_speed_kmph': float(parts[5]),
                        'current_lat': float(parts[6]),
                        'current_lng': float(parts[7]),
                        'available_from': parts[8],
                        'category': parts[9]
                    }
                    data['vehicles'].append(veh)
        
        # Baseline section  
        elif header[0] == 'employee_id' and len(header) >= 3 and 'baseline_cost' in header[1]:
            for line in lines[1:]:
                line = line.strip()
                if not line:
                    continue
                parts = line.split('\t')
                if len(parts) >= 3:
                    base = {
                        'employee_id': parts[0],
                        'baseline_cost': float(parts[1]),
                        'baseline_time_min': float(parts[2])
                    }
                    data['baseline'].append(base)
        
        # Metadata section
        elif header[0] == 'key' and len(header) >= 2:
            for line in lines[1:]:
                line = line.strip()
                if not line:
                    continue
                parts = line.split('\t')
                if len(parts) >= 2:
                    meta = {
                        'key': parts[0],
                        'value': parts[1]
                    }
                    data['metadata'].append(meta)
    
    return data

def run_comparison():
    print("="*80)
    print("COMPARING PYTHON OR-TOOLS vs CUSTOM C++ SOLVER")
    print("="*80)
    
    # Load test case
    print("\n1. Loading test_case1.txt...")
    test_file = "../test_case1.txt"
    if not os.path.exists(test_file):
        print(f"ERROR: {test_file} not found!")
        return
    
    data = parse_test_case_txt(test_file)
    print(f"   Employees: {len(data['employees'])}")
    print(f"   Vehicles: {len(data['vehicles'])}")
    
    # Print employee details
    print("\n   Employee Details:")
    for emp in data['employees']:
        print(f"   {emp['employee_id']}: Priority {emp['priority']}, "
              f"{emp['earliest_pickup']}-{emp['latest_drop']}, "
              f"{emp['vehicle_preference']} vehicle, {emp['sharing_preference']} sharing")
    
    # Build metadata dict
    meta_list = data['metadata']
    metadata = {m['key']: m['value'] for m in meta_list}
    
    # Run Python OR-Tools solver
    print("\n2. Running Python OR-Tools Full solver...")
    print("-" * 80)
    python_result = solver_ortools_full.solve_ortools_full(
        data['employees'],
        data['vehicles'],
        data['baseline'],
        metadata
    )
    print("-" * 80)
    
    print(f"\n   Python Results:")
    print(f"   Score: {python_result['score']}")
    print(f"   Cost: ${python_result['stats']['cost']}")
    print(f"   Time: {python_result['stats']['time']} min")
    print(f"   Hard violations: {python_result['stats']['hard_violations']}")
    print(f"   Soft violations: {python_result['stats']['soft_violations']}")
    print(f"   Solution type: {python_result.get('solution_type', 'N/A')}")
    
    # Print assignments
    print(f"\n   Python Assignments:")
    for emp_id, veh_id in sorted(python_result['assignment'].items()):
        print(f"   {emp_id} -> {veh_id}")
    
    # Run C++ solver
    print("\n3. Running Custom C++ VRP solver...")
    print("-" * 80)
    
    inp_file = "test_case1_cpp_input.txt"
    out_file = "test_case1_cpp_output.json"
    
    export_full_constraints_to_cpp(data, inp_file, metadata)
    
    exe_path = 'vrp_solver/vrp_solver_full.exe'
    if not os.path.exists(exe_path):
        print(f"   ERROR: {exe_path} not found!")
        return
    
    result = subprocess.run(
        [exe_path, inp_file, out_file, '30'],
        capture_output=True, text=True, timeout=60
    )
    
    print(result.stdout)
    print("-" * 80)
    
    if result.returncode != 0:
        print(f"   ERROR: Solver failed: {result.stderr}")
        return
    
    with open(out_file, 'r') as f:
        cpp_result = json.load(f)
    
    print(f"\n   C++ Results:")
    print(f"   Total cost: ${cpp_result['total_cost']}")
    print(f"   Total time: {cpp_result['total_time']} min")
    print(f"   Hard violations: {cpp_result['hard_violations']}")
    print(f"   Soft violations: {cpp_result['soft_violations']}")
    print(f"   Vehicles used: {cpp_result['vehicles_used']}")
    print(f"   Trips used: {cpp_result['trips_used']}")
    
    # Build C++ assignments
    cpp_assignments = {}
    for vehicle_data in cpp_result.get('details', []):
        veh_id = vehicle_data['vehicle']
        for emp_id in vehicle_data.get('employees', []):
            cpp_assignments[emp_id] = veh_id
    
    print(f"\n   C++ Assignments:")
    for emp_id, veh_id in sorted(cpp_assignments.items()):
        print(f"   {emp_id} -> {veh_id}")
    
    # Compare assignments
    print("\n4. Comparing Assignments...")
    print("-" * 80)
    
    all_employees = set(python_result['assignment'].keys()) | set(cpp_assignments.keys())
    differences = []
    
    for emp_id in sorted(all_employees):
        py_veh = python_result['assignment'].get(emp_id, 'MISSING')
        cpp_veh = cpp_assignments.get(emp_id, 'MISSING')
        
        if py_veh != cpp_veh:
            differences.append((emp_id, py_veh, cpp_veh))
            print(f"   ❌ {emp_id}: Python={py_veh}, C++={cpp_veh}")
        else:
            print(f"   ✅ {emp_id}: Both={py_veh}")
    
    # Compare violations
    print("\n5. Comparing Violations...")
    print("-" * 80)
    
    py_hard = python_result['stats']['hard_violations']
    py_soft = python_result['stats']['soft_violations']
    cpp_hard = cpp_result['hard_violations']
    cpp_soft = cpp_result['soft_violations']
    
    print(f"   Hard violations: Python={py_hard}, C++={cpp_hard} {'✅' if py_hard == cpp_hard else '❌'}")
    print(f"   Soft violations: Python={py_soft}, C++={cpp_soft} {'✅' if py_soft == cpp_soft else '❌'}")
    
    # Detailed route comparison
    print("\n6. Detailed Route Analysis...")
    print("-" * 80)
    
    print("\n   Python Routes:")
    for detail in python_result.get('details', []):
        veh_id = detail.get('vehicle', detail.get('vehicle_id'))
        num_trips = detail.get('num_trips', 0)
        if num_trips > 0:
            print(f"\n   Vehicle {veh_id}: {num_trips} trip(s)")
            for trip in detail.get('trip_routes', []):
                print(f"     Trip {trip['trip_number']}: {len(trip['employees'])} employees")
                print(f"       Employees: {', '.join(trip['employees'])}")
                print(f"       Distance: {trip['distance_km']:.2f} km, Cost: ${trip['cost']:.2f}")
                
                # Show time violations
                for stop in trip.get('detailed_stops', []):
                    if stop.get('type') == 'employee':
                        arrival = stop.get('est_office_arrival_minutes', 0)
                        latest = stop.get('adjusted_latest_minutes', 9999)
                        if arrival > latest:
                            print(f"       ⚠️ {stop['label']}: Arrives {arrival} > Latest {latest}")
    
    print("\n   C++ Routes:")
    for detail in cpp_result.get('details', []):
        veh_id = detail['vehicle']
        num_trips = detail['num_trips']
        if num_trips > 0:
            print(f"\n   Vehicle {veh_id}: {num_trips} trip(s)")
            for trip in detail.get('trip_routes', []):
                print(f"     Trip {trip['trip_number']}: {len(trip['employees'])} employees")
                print(f"       Employees: {', '.join(trip['employees'])}")
                print(f"       Distance: {trip['distance_km']:.2f} km, Cost: ${trip['cost']:.2f}")
                
                # Show time violations
                for stop in trip.get('detailed_stops', []):
                    if stop.get('type') == 'employee':
                        arrival = stop.get('est_office_arrival_minutes', 0)
                        latest = stop.get('adjusted_latest_minutes', 9999)
                        if arrival > latest:
                            print(f"       ⚠️ {stop['label']}: Arrives {arrival} > Latest {latest}")
    
    # Summary
    print("\n" + "="*80)
    if differences:
        print(f"❌ FOUND {len(differences)} ASSIGNMENT DIFFERENCES")
        print("="*80)
        return False
    else:
        print("✅ ALL ASSIGNMENTS MATCH!")
        if py_hard == cpp_hard and py_soft == cpp_soft:
            print("✅ VIOLATIONS MATCH!")
        else:
            print("⚠️ VIOLATION COUNTS DIFFER")
        print("="*80)
        return True

if __name__ == '__main__':
    import sys
    try:
        success = run_comparison()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n❌ ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
