"""
Simple comparison script for test_case1.txt
"""

import json
import subprocess
import os
import sys
from test_case1_comparison import parse_test_case_txt
import solver_ortools_full
from app import export_full_constraints_to_cpp

# Load test case
test_file = '../test_case1.txt'
print("Loading test_case1.txt...")
data = parse_test_case_txt(test_file)
print(f"Employees: {len(data['employees'])}")
print(f"Vehicles: {len(data['vehicles'])}")

# Build metadata dict
meta_list = data['metadata']
metadata = {m['key']: m['value'] for m in meta_list}

# Export data for C++ solver first (doesn't block)
inp_file = "test_case1_cpp_input.txt"
out_file = "test_case1_cpp_output.json"
export_full_constraints_to_cpp(data, inp_file, metadata)
print(f"Exported C++ input to {inp_file}")

# Check if C++ solver exists
exe_path = 'vrp_solver/vrp_solver_full.exe'
if not os.path.exists(exe_path):
    print(f"\nERROR: {exe_path} not found! Need to compile first.")
    sys.exit(1)

# Run C++ solver first (fast)
print("\n" + "="*60)
print("CUSTOM C++ SOLVER")
print("="*60)

result = subprocess.run(
    [exe_path, inp_file, out_file, '10'],
    capture_output=True, text=True, timeout=60
)

print(result.stdout)

if result.returncode != 0:
    print(f"ERROR: Solver failed: {result.stderr}")
    sys.exit(1)

with open(out_file, 'r') as f:
    cpp_result = json.load(f)

print(f"\nC++ Results:")
print(f"  Total cost: ${cpp_result['total_cost']:.2f}")
print(f"  Hard violations: {cpp_result['hard_violations']}")
print(f"  Soft violations: {cpp_result['soft_violations']}")

# Build C++ assignments
cpp_assignments = {}
print(f"\nC++ Assignments:")
for vehicle_data in cpp_result.get('details', []):
    veh_id = vehicle_data['vehicle']
    emps = vehicle_data.get('employees', [])
    if emps:
        print(f"  {veh_id}: {emps}")
        for emp_id in emps:
            cpp_assignments[emp_id] = veh_id

print(f"\nTotal employees assigned by C++: {len(cpp_assignments)}")

# Now run Python OR-Tools solver (may take longer)
print("\n" + "="*60)
print("PYTHON OR-TOOLS SOLVER")
print("="*60)
python_result = solver_ortools_full.solve_ortools_full(
    data['employees'],
    data['vehicles'],
    data['baseline'],
    metadata
)

print(f"\nPython Results:")
print(f"  Cost: ${python_result['stats']['cost']}")
print(f"  Hard violations: {python_result['stats']['hard_violations']}")
print(f"  Soft violations: {python_result['stats']['soft_violations']}")
print(f"  Solution type: {python_result.get('solution_type', 'N/A')}")

# Build Python assignments
py_assignments = {}
print(f"\nPython Assignments:")
for veh, emps in python_result['assignment'].items():
    if emps:
        print(f"  {veh}: {emps}")
        for emp in emps:
            py_assignments[emp] = veh

print(f"\nTotal employees assigned by Python: {len(py_assignments)}")

# Compare
print("\n" + "="*60)
print("COMPARISON")
print("="*60)

py_hard = python_result['stats']['hard_violations']
py_soft = python_result['stats']['soft_violations']
cpp_hard = cpp_result['hard_violations']
cpp_soft = cpp_result['soft_violations']
py_cost = python_result['stats']['cost']
cpp_cost = cpp_result['total_cost']

print(f"\n{'Metric':<25} {'C++':<15} {'Python':<15} {'Winner':<10}")
print("-" * 65)
print(f"{'Cost':<25} ${cpp_cost:<14.2f} ${py_cost:<14.2f} {'C++' if cpp_cost < py_cost else 'Python' if py_cost < cpp_cost else 'Tie'}")
print(f"{'Hard violations':<25} {cpp_hard:<15} {py_hard:<15} {'C++' if cpp_hard < py_hard else 'Python' if py_hard < cpp_hard else 'Tie'}")
print(f"{'Soft violations':<25} {cpp_soft:<15} {py_soft:<15} {'C++' if cpp_soft < py_soft else 'Python' if py_soft < cpp_soft else 'Tie'}")

# Check assignments match
differences = []
all_emps = set(py_assignments.keys()) | set(cpp_assignments.keys())
for emp in sorted(all_emps):
    py_veh = py_assignments.get(emp, 'MISSING')
    cpp_veh = cpp_assignments.get(emp, 'MISSING')
    if py_veh != cpp_veh:
        differences.append((emp, py_veh, cpp_veh))

if differences:
    print(f"\nAssignment differences ({len(differences)}):")
    for emp, py_veh, cpp_veh in differences:
        print(f"  {emp}: Python={py_veh}, C++={cpp_veh}")
else:
    print("\nAll assignments match!")

# Summary
print("\n" + "="*60)
if cpp_hard <= py_hard and cpp_soft <= py_soft:
    if cpp_hard < py_hard or cpp_soft < py_soft:
        print("✅ C++ SOLVER FOUND A BETTER SOLUTION!")
    else:
        print("✅ BOTH SOLVERS FOUND EQUIVALENT SOLUTIONS")
elif py_hard <= cpp_hard and py_soft <= cpp_soft:
    print("⚠️ Python OR-Tools found a better solution")
else:
    print("⚠️ Mixed results - neither strictly dominates")
print("="*60)
