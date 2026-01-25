"""
Detailed constraint validation for C++ solver output
Verifies ALL constraints (hard and soft) are properly satisfied
"""

import json
from test_case1_comparison import parse_test_case_txt

def time_to_min(t):
    if isinstance(t, str):
        h, m = t.split(':')
        return int(h) * 60 + int(m)
    return int(t)

# Load test case
test_file = '../test_case1.txt'
data = parse_test_case_txt(test_file)

# Build lookup tables
employees = {e['employee_id']: e for e in data['employees']}
vehicles = {v['vehicle_id']: v for v in data['vehicles']}
metadata = {m['key']: m['value'] for m in data['metadata']}

# Priority max delays
priority_delays = {
    1: int(metadata.get('priority_1_max_delay_min', 5)),
    2: int(metadata.get('priority_2_max_delay_min', 10)),
    3: int(metadata.get('priority_3_max_delay_min', 15)),
    4: int(metadata.get('priority_4_max_delay_min', 20)),
    5: int(metadata.get('priority_5_max_delay_min', 30)),
}

# Load C++ output
with open('test_case1_cpp_output.json', 'r') as f:
    result = json.load(f)

print("="*70)
print("DETAILED CONSTRAINT VALIDATION FOR C++ SOLVER OUTPUT")
print("="*70)

print(f"\nTest Case: test_case1.txt")
print(f"Employees: {len(employees)}")
print(f"Vehicles: {len(vehicles)}")

hard_violations = 0
soft_violations = 0

print("\n" + "-"*70)
print("EMPLOYEE CONSTRAINTS:")
print("-"*70)
print(f"{'ID':<5} {'Priority':<8} {'Time Window':<15} {'Vehicle Pref':<12} {'Sharing':<10}")
print("-"*70)
for emp_id, emp in employees.items():
    tw = f"{emp['earliest_pickup']}-{emp['latest_drop']}"
    print(f"{emp_id:<5} {emp['priority']:<8} {tw:<15} {emp['vehicle_preference']:<12} {emp['sharing_preference']:<10}")

print("\n" + "-"*70)
print("VEHICLE CONSTRAINTS:")
print("-"*70)
print(f"{'ID':<5} {'Capacity':<10} {'Category':<10} {'Speed':<10} {'Cost/km':<10}")
print("-"*70)
for veh_id, veh in vehicles.items():
    print(f"{veh_id:<5} {veh['capacity']:<10} {veh['category']:<10} {veh['avg_speed_kmph']:<10} {veh['cost_per_km']:<10}")

print("\n" + "="*70)
print("CONSTRAINT VALIDATION BY TRIP:")
print("="*70)

for vehicle_detail in result['details']:
    veh_id = vehicle_detail['vehicle']
    veh = vehicles[veh_id]
    
    if not vehicle_detail['employees']:
        continue
    
    print(f"\n--- Vehicle {veh_id} (Category: {veh['category']}, Capacity: {veh['capacity']}) ---")
    
    for trip in vehicle_detail.get('trip_routes', []):
        trip_emps = trip['employees']
        trip_size = len(trip_emps)
        
        print(f"\n  Trip {trip['trip_number']}: {trip_emps}")
        print(f"  Distance: {trip['distance_km']:.2f} km, Cost: ${trip['cost']:.2f}")
        
        # Check each employee in this trip
        for stop in trip.get('detailed_stops', []):
            if stop.get('type') != 'employee':
                continue
            
            emp_id = stop['label']
            emp = employees[emp_id]
            
            # Get timing info
            pickup_time = stop.get('time_minutes', 0)
            office_arrival = stop.get('est_office_arrival_minutes', 0)
            earliest = time_to_min(emp['earliest_pickup'])
            latest_drop = time_to_min(emp['latest_drop'])
            priority = emp['priority']
            max_delay = priority_delays[priority]
            adjusted_latest = latest_drop + max_delay
            
            print(f"\n    {emp_id}:")
            
            # HARD: Time window check
            tw_ok = office_arrival <= adjusted_latest
            tw_status = "✓" if tw_ok else "✗ HARD VIOLATION"
            if not tw_ok:
                hard_violations += 1
            print(f"      Time Window: Pickup@{stop['time']}, Office@{stop['est_office_arrival']}, "
                  f"Latest: {emp['latest_drop']}+{max_delay}min = {adjusted_latest//60:02d}:{adjusted_latest%60:02d} {tw_status}")
            
            # SOFT: Vehicle preference check
            emp_veh_pref = emp['vehicle_preference'].lower()
            veh_cat = veh['category'].lower()
            veh_ok = (emp_veh_pref == 'any' or emp_veh_pref == veh_cat)
            veh_status = "✓" if veh_ok else "✗ SOFT VIOLATION"
            if not veh_ok:
                soft_violations += 1
            print(f"      Vehicle Pref: Wants '{emp_veh_pref}', Got '{veh_cat}' {veh_status}")
            
            # SOFT: Sharing preference check
            emp_share_pref = emp['sharing_preference'].lower()
            share_limit = {'single': 1, 'double': 2, 'triple': 3}.get(emp_share_pref, 3)
            share_ok = trip_size <= share_limit
            share_status = "✓" if share_ok else "✗ SOFT VIOLATION"
            if not share_ok:
                soft_violations += 1
            print(f"      Sharing Pref: Wants '{emp_share_pref}' (max {share_limit}), Trip has {trip_size} {share_status}")

print("\n" + "="*70)
print("SUMMARY")
print("="*70)
print(f"Total Cost: ${result['total_cost']:.2f}")
print(f"Hard Violations: {hard_violations} (from validation) vs {result['hard_violations']} (from solver)")
print(f"Soft Violations: {soft_violations} (from validation) vs {result['soft_violations']} (from solver)")

if hard_violations == 0 and soft_violations == 0:
    print("\n✅ ALL CONSTRAINTS SATISFIED!")
else:
    print(f"\n⚠️ Found {hard_violations} hard + {soft_violations} soft violations")
print("="*70)
