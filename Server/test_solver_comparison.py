"""
Test script to compare output formats between solver_ortools_full.py and custom C++ solver
"""

import json
import sys
from solver import parse_excel_file
import solver_ortools_full
from app import export_full_constraints_to_cpp
import subprocess
import os

def test_output_format():
    print("="*70)
    print("TESTING OUTPUT FORMAT COMPATIBILITY")
    print("="*70)
    
    # Load data
    print("\n1. Loading test data...")
    data = parse_excel_file("TestCase_TC03.xlsx")
    
    # Get metadata
    meta_list = data.get('metadataa', data.get('metadata', []))
    metadata = {m['key']: m['value'] for m in meta_list}
    metadata['objective_cost_weight'] = 0.6
    metadata['objective_time_weight'] = 0.4
    
    print(f"   Employees: {len(data['employees'])}")
    print(f"   Vehicles: {len(data['vehicles'])}")
    
    # Test Python OR-Tools solver
    print("\n2. Running Python OR-Tools Full solver...")
    python_result = solver_ortools_full.solve_ortools_full(
        data['employees'],
        data['vehicles'],
        data['baseline'],
        metadata
    )
    
    print(f"   Score: {python_result['score']}")
    print(f"   Hard violations: {python_result['stats']['hard_violations']}")
    print(f"   Soft violations: {python_result['stats']['soft_violations']}")
    
    # Test C++ solver
    print("\n3. Running Custom C++ VRP solver...")
    
    # Export data
    inp_file = "test_cpp_input.txt"
    out_file = "test_cpp_output.json"
    export_full_constraints_to_cpp(data, inp_file, metadata)
    
    # Run solver
    exe_path = 'vrp_solver/vrp_solver_full.exe'
    if not os.path.exists(exe_path):
        print(f"   ERROR: {exe_path} not found!")
        return False
    
    result = subprocess.run(
        [exe_path, inp_file, out_file, '30'],
        capture_output=True, text=True, timeout=60
    )
    
    if result.returncode != 0:
        print(f"   ERROR: Solver failed: {result.stderr}")
        return False
    
    # Load output
    with open(out_file, 'r') as f:
        cpp_result = json.load(f)
    
    print(f"   Total cost: {cpp_result['total_cost']}")
    print(f"   Hard violations: {cpp_result['hard_violations']}")
    print(f"   Soft violations: {cpp_result['soft_violations']}")
    
    # Compare output structures
    print("\n4. Comparing output structures...")
    
    def check_structure(obj, path=""):
        """Recursively check structure"""
        if isinstance(obj, dict):
            return {k: check_structure(v, f"{path}.{k}") for k, v in obj.items()}
        elif isinstance(obj, list):
            if obj:
                return [check_structure(obj[0], f"{path}[0]")]
            return []
        else:
            return type(obj).__name__
    
    python_structure = check_structure(python_result.get('details', []))
    cpp_structure = check_structure(cpp_result.get('details', []))
    
    print("\n   Python 'details' structure:")
    print(json.dumps(python_structure, indent=2)[:500])
    
    print("\n   C++ 'details' structure:")
    print(json.dumps(cpp_structure, indent=2)[:500])
    
    # Check key fields match
    print("\n5. Checking key field compatibility...")
    
    errors = []
    
    # Check Python details structure
    if python_result.get('details'):
        py_detail = python_result['details'][0]
        required_fields = ['vehicle', 'employees', 'trip_routes', 'num_trips', 'cost']
        
        for field in required_fields:
            if field not in py_detail:
                errors.append(f"Python output missing field: {field}")
        
        if 'trip_routes' in py_detail and py_detail['trip_routes']:
            trip = py_detail['trip_routes'][0]
            trip_fields = ['trip_number', 'employees', 'distance_km', 'cost', 'detailed_stops']
            for field in trip_fields:
                if field not in trip:
                    errors.append(f"Python trip missing field: {field}")
            
            if 'detailed_stops' in trip and trip['detailed_stops']:
                stop = trip['detailed_stops'][0]
                stop_fields = ['stop_number', 'label', 'type', 'time', 'time_minutes', 'distance_to_next', 'cumulative_distance']
                for field in stop_fields:
                    if field not in stop:
                        errors.append(f"Python stop missing field: {field}")
    
    # Check C++ details structure
    if cpp_result.get('details'):
        cpp_detail = cpp_result['details'][0]
        required_fields = ['vehicle', 'employees', 'trip_routes', 'num_trips', 'cost']
        
        for field in required_fields:
            if field not in cpp_detail:
                errors.append(f"C++ output missing field: {field}")
        
        if 'trip_routes' in cpp_detail and cpp_detail['trip_routes']:
            trip = cpp_detail['trip_routes'][0]
            trip_fields = ['trip_number', 'employees', 'distance_km', 'cost', 'detailed_stops']
            for field in trip_fields:
                if field not in trip:
                    errors.append(f"C++ trip missing field: {field}")
            
            if 'detailed_stops' in trip and trip['detailed_stops']:
                stop = trip['detailed_stops'][0]
                stop_fields = ['stop_number', 'label', 'type', 'time', 'time_minutes', 'distance_to_next', 'cumulative_distance']
                for field in stop_fields:
                    if field not in stop:
                        errors.append(f"C++ stop missing field: {field}")
    
    if errors:
        print("\n   ❌ ERRORS FOUND:")
        for error in errors:
            print(f"      - {error}")
        return False
    else:
        print("\n   ✅ All required fields present in both outputs!")
    
    # Display sample output comparison
    print("\n6. Sample detailed_stops comparison:")
    
    if python_result.get('details') and python_result['details'][0].get('trip_routes'):
        py_stops = python_result['details'][0]['trip_routes'][0].get('detailed_stops', [])
        if py_stops:
            print("\n   Python first stop:")
            print(json.dumps(py_stops[0], indent=4))
    
    if cpp_result.get('details') and cpp_result['details'][0].get('trip_routes'):
        cpp_stops = cpp_result['details'][0]['trip_routes'][0].get('detailed_stops', [])
        if cpp_stops:
            print("\n   C++ first stop:")
            print(json.dumps(cpp_stops[0], indent=4))
    
    print("\n" + "="*70)
    print("✅ TEST COMPLETE - Output formats are compatible!")
    print("="*70)
    
    return True

if __name__ == '__main__':
    try:
        success = test_output_format()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n❌ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
