"""Validate manual solution for Test Case 4"""

import json
import pandas as pd
from math import radians, cos, sin, asin, sqrt

def haversine(lat1, lon1, lat2, lon2):
    """Calculate distance between two points in km"""
    R = 6371  # Earth radius in km
    lat1, lon1, lat2, lon2 = map(radians, [lat1, lon1, lat2, lon2])
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    a = sin(dlat/2)**2 + cos(lat1) * cos(lat2) * sin(dlon/2)**2
    c = 2 * asin(sqrt(a))
    return R * c

def time_to_min(t):
    """Convert time string to minutes from midnight"""
    if isinstance(t, str) and ':' in t:
        parts = t.split(':')
        h, m = int(parts[0]), int(parts[1])
        return h * 60 + m
    return t

def min_to_time(m):
    """Convert minutes to HH:MM format"""
    h = int(m // 60)
    mn = int(m % 60)
    return f"{h:02d}:{mn:02d}"

# Load test case data
with open('tc04_input.json', 'r') as f:
    data = json.load(f)

employees = {e['employee_id']: e for e in data['employees']}
vehicles = {v['vehicle_id']: v for v in data['vehicles']}

# Office location (from first employee)
office_lat = data['employees'][0]['drop_lat']
office_lng = data['employees'][0]['drop_lng']

# Manual solution
manual_routes = {
    'V01': [
        ('depot', None),
        ('E01', 'pickup'),
        ('office', 'dropoff'),
        ('E03', 'pickup'),
        ('office', 'dropoff'),
        ('E04', 'pickup'),
        ('office', 'dropoff'),
        ('E10', 'pickup'),
        ('office', 'dropoff')
    ],
    'V02': [
        ('depot', None),
        ('E02', 'pickup'),
        ('office', 'dropoff'),
        ('E06', 'pickup'),
        ('E05', 'pickup'),
        ('office', 'dropoff')
    ],
    'V03': [
        ('depot', None),
        ('E09', 'pickup'),
        ('E08', 'pickup'),
        ('E07', 'pickup'),
        ('office', 'dropoff')
    ]
}

print("=" * 70)
print("VALIDATING MANUAL SOLUTION FOR TEST CASE 4")
print("=" * 70)

total_cost = 0
total_time = 0
hard_violations = 0
soft_violations = 0
violation_details = []

for vehicle_id, route in manual_routes.items():
    print(f"\n{vehicle_id}:")
    vehicle = vehicles[vehicle_id]
    
    current_lat = vehicle['current_lat']
    current_lng = vehicle['current_lng']
    current_time = time_to_min(vehicle['available_from'])
    vehicle_cost = 0
    vehicle_distance = 0
    passengers = []
    
    print(f"  Starting at depot ({current_lat:.4f}, {current_lng:.4f}) at {min_to_time(current_time)}")
    print(f"  Capacity: {vehicle['capacity']}, Speed: {vehicle['avg_speed_kmph']} km/h")
    
    for stop_id, action in route[1:]:  # Skip depot start
        if stop_id == 'office':
            # Drop off all passengers at office
            distance = haversine(current_lat, current_lng, office_lat, office_lng)
            travel_time = (distance / vehicle['avg_speed_kmph']) * 60
            cost = distance * vehicle['cost_per_km']
            
            current_time += travel_time
            vehicle_cost += cost
            vehicle_distance += distance
            
            print(f"  -> Office at {min_to_time(current_time)} [{distance:.2f} km, ${cost:.2f}]")
            
            # Check each passenger's deadline
            for emp_id in passengers:
                emp = employees[emp_id]
                deadline = time_to_min(emp['latest_drop'])
                if current_time > deadline:
                    violation = f"{emp_id} arrives at {min_to_time(current_time)} > deadline {emp['latest_drop']}"
                    hard_violations += 1
                    violation_details.append(f"  HARD: {violation}")
                    print(f"     ❌ HARD VIOLATION: {violation}")
                else:
                    print(f"     ✓ {emp_id} on time (deadline: {emp['latest_drop']})")
            
            passengers = []
            current_lat = office_lat
            current_lng = office_lng
            
        else:
            # Pick up employee
            emp = employees[stop_id]
            
            # Check capacity
            if len(passengers) >= vehicle['capacity']:
                violation = f"{vehicle_id} exceeds capacity {vehicle['capacity']}"
                hard_violations += 1
                violation_details.append(f"  HARD: {violation}")
                print(f"     ❌ HARD VIOLATION: {violation}")
            
            distance = haversine(current_lat, current_lng, emp['pickup_lat'], emp['pickup_lng'])
            travel_time = (distance / vehicle['avg_speed_kmph']) * 60
            cost = distance * vehicle['cost_per_km']
            
            current_time += travel_time
            vehicle_cost += cost
            vehicle_distance += distance
            
            # Check earliest pickup
            earliest = time_to_min(emp['earliest_pickup'])
            wait_time = 0
            if current_time < earliest:
                wait_time = earliest - current_time
                current_time = earliest
            
            passengers.append(stop_id)
            
            print(f"  -> Pickup {stop_id} at {min_to_time(current_time)} [{distance:.2f} km, ${cost:.2f}]", end="")
            if wait_time > 0:
                print(f" (waited {wait_time:.0f} min)")
            else:
                print()
            
            if current_time < earliest:
                print(f"     ⚠️ Arrived before earliest pickup {emp['earliest_pickup']}")
            
            # Check vehicle preference
            veh_pref = emp.get('vehicle_preference', 'any')
            if veh_pref != 'any' and veh_pref != vehicle_id.lower():
                soft_violations += 1
                violation_details.append(f"  SOFT: {stop_id} prefers {veh_pref}, assigned to {vehicle_id}")
                print(f"     ⚠️ SOFT: Vehicle preference violation (prefers {veh_pref})")
            
            # Check sharing preference
            sharing_pref = emp.get('sharing_preference', 'triple')
            if sharing_pref == 'single' and len(passengers) > 1:
                soft_violations += 1
                violation_details.append(f"  SOFT: {stop_id} prefers single but sharing with others")
                print(f"     ⚠️ SOFT: Sharing preference violation (prefers single)")
            
            current_lat = emp['pickup_lat']
            current_lng = emp['pickup_lng']
    
    print(f"  Vehicle total: ${vehicle_cost:.2f}, {vehicle_distance:.2f} km")
    total_cost += vehicle_cost
    total_time += current_time - time_to_min(vehicle['available_from'])

print("\n" + "=" * 70)
print("MANUAL SOLUTION SUMMARY")
print("=" * 70)
print(f"Total Cost: ${total_cost:.2f}")
print(f"Total Time: {total_time:.0f} min")
print(f"Hard Violations: {hard_violations}")
print(f"Soft Violations: {soft_violations}")
print(f"Score: {total_cost + hard_violations * 1000 + soft_violations * 100:.2f}")

if violation_details:
    print("\nViolation Details:")
    for v in violation_details:
        print(v)

print("\n" + "=" * 70)
print("COMPARISON WITH SOLVER OUTPUT")
print("=" * 70)

# Load solver output
with open('tc04_output.json', 'r') as f:
    solver_output = json.load(f)

print(f"Solver Cost: ${solver_output['stats']['cost']:.2f}")
print(f"Solver Hard Violations: {solver_output['stats']['hard_violations']}")
print(f"Solver Soft Violations: {solver_output['stats']['soft_violations']}")
print(f"Solver Score: {solver_output['score']:.2f}")

print("\n" + "-" * 70)
print("VERDICT:")
if hard_violations < solver_output['stats']['hard_violations']:
    print("✓ MANUAL SOLUTION IS BETTER (fewer hard violations)")
elif hard_violations == solver_output['stats']['hard_violations']:
    if soft_violations < solver_output['stats']['soft_violations']:
        print("✓ MANUAL SOLUTION IS BETTER (same hard, fewer soft violations)")
    elif soft_violations == solver_output['stats']['soft_violations']:
        if total_cost < solver_output['stats']['cost']:
            print("✓ MANUAL SOLUTION IS BETTER (same violations, lower cost)")
        else:
            print("≈ SOLUTIONS ARE EQUIVALENT")
    else:
        print("✗ SOLVER SOLUTION IS BETTER (fewer soft violations)")
else:
    print("✗ SOLVER SOLUTION IS BETTER (fewer hard violations)")
print("=" * 70)
