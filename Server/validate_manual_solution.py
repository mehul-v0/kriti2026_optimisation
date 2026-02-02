"""
Validate a manual VRP solution against all constraints
Usage: python validate_manual_solution.py tc01_manual_solution.json TestCase_TC03.xlsx
"""

import json
import sys
from solver import parse_excel_file
from solver_ortools_full import haversine_km, time_to_min, min_to_time

def validate_manual_solution(manual_solution_file, data_file):
    """Validate a manual solution against constraints."""
    
    # Load manual solution
    with open(manual_solution_file, 'r') as f:
        manual_sol = json.load(f)
    
    # Load data
    data = parse_excel_file(data_file)
    employees = data['employees']
    vehicles = data['vehicles']
    meta_list = data.get('metadataa', data.get('metadata', []))
    metadata = {m['key']: m['value'] for m in meta_list}
    
    # Build employee/vehicle lookup
    emp_map = {e['employee_id']: e for e in employees}
    veh_map = {v['vehicle_id']: v for v in vehicles}
    
    # Office location
    office_lat = float(employees[0]['drop_lat'])
    office_lng = float(employees[0]['drop_lng'])
    
    print("=" * 80)
    print("MANUAL SOLUTION VALIDATION")
    print("=" * 80)
    
    total_violations = 0
    total_cost = 0
    total_time = 0
    hard_violations = 0
    soft_violations = 0
    
    # Priority delays
    priority_delays = {
        1: int(metadata.get('priority_1_max_delay_min', 5)),
        2: int(metadata.get('priority_2_max_delay_min', 10)),
        3: int(metadata.get('priority_3_max_delay_min', 15)),
        4: int(metadata.get('priority_4_max_delay_min', 20)),
        5: int(metadata.get('priority_5_max_delay_min', 30)),
    }
    
    # Track employee assignments
    assigned_employees = set()
    
    # Validate each vehicle's trips
    for veh_data in manual_sol['vehicles']:
        veh_id = veh_data['vehicle_id']
        vehicle = veh_map[veh_id]
        
        print(f"\n{'='*80}")
        print(f"Vehicle: {veh_id}")
        print(f"  Capacity: {vehicle['capacity']}")
        print(f"  Speed: {vehicle['avg_speed_kmph']} km/h")
        print(f"  Cost: ${vehicle['cost_per_km']}/km")
        print(f"  Category: {vehicle.get('category', 'normal')}")
        print(f"  Available from: {vehicle.get('available_from', '08:00')}")
        
        vehicle_speed = float(vehicle['avg_speed_kmph'])
        vehicle_cost_per_km = float(vehicle['cost_per_km'])
        vehicle_avail = time_to_min(vehicle.get('available_from', '08:00'))
        vehicle_category = vehicle.get('category', 'normal').lower()
        
        current_time = vehicle_avail
        current_location = (float(vehicle['current_lat']), float(vehicle['current_lng']))
        
        for trip in veh_data['trips']:
            trip_num = trip['trip_number']
            print(f"\n  --- Trip {trip_num} ---")
            
            trip_employees = []
            trip_distance = 0
            trip_start_time = current_time
            
            stops = trip['stops']
            
            for i, stop in enumerate(stops):
                location = stop['location']
                
                if 'Pickup' in location:
                    # Extract employee ID
                    emp_id = location.split()[0]  # E.g., "E06"
                    
                    if emp_id in assigned_employees:
                        print(f"    ❌ ERROR: {emp_id} already assigned!")
                        hard_violations += 1
                    
                    assigned_employees.add(emp_id)
                    trip_employees.append(emp_id)
                    
                    employee = emp_map[emp_id]
                    emp_lat = float(employee['pickup_lat'])
                    emp_lng = float(employee['pickup_lng'])
                    
                    # Calculate distance to this pickup
                    dist = haversine_km(current_location[0], current_location[1], emp_lat, emp_lng)
                    trip_distance += dist
                    
                    # Calculate arrival time
                    travel_time = (dist / vehicle_speed) * 60
                    current_time += travel_time
                    
                    # Validate time windows
                    earliest_pickup = time_to_min(employee.get('earliest_pickup', '08:00'))
                    latest_drop = time_to_min(employee.get('latest_drop', '18:00'))
                    priority = int(employee.get('priority', 3))
                    max_delay = priority_delays.get(priority, 15)
                    
                    # If arrive before earliest, wait
                    if current_time < earliest_pickup:
                        current_time = earliest_pickup
                    
                    print(f"    {emp_id} Pickup at {min_to_time(int(current_time))}")
                    print(f"      Time window: [{min_to_time(earliest_pickup)} - {min_to_time(latest_drop)}]")
                    print(f"      Priority: {priority}, Max delay: {max_delay} min")
                    
                    # Check sharing preference
                    sharing_pref = employee.get('sharing_preference', 'triple').lower()
                    max_sharing = 1 if sharing_pref == 'single' else (2 if sharing_pref == 'double' else 3)
                    
                    # Check vehicle preference
                    vehicle_pref = employee.get('vehicle_preference', 'any').lower()
                    if vehicle_pref == 'premium' and vehicle_category != 'premium':
                        print(f"      ⚠️ SOFT VIOLATION: Wants premium vehicle, got {vehicle_category}")
                        soft_violations += 1
                    elif vehicle_pref == 'normal' and vehicle_category == 'premium':
                        print(f"      ⚠️ SOFT VIOLATION: Wants normal vehicle, got {vehicle_category}")
                        soft_violations += 1
                    
                    current_location = (emp_lat, emp_lng)
                
                elif 'Office' in location or 'Drop' in location:
                    # Travel to office
                    dist = haversine_km(current_location[0], current_location[1], office_lat, office_lng)
                    trip_distance += dist
                    travel_time = (dist / vehicle_speed) * 60
                    current_time += travel_time
                    
                    print(f"    Office Drop-off at {min_to_time(int(current_time))}")
                    
                    # Validate each employee's drop-off time
                    for emp_id in trip_employees:
                        employee = emp_map[emp_id]
                        latest_drop = time_to_min(employee.get('latest_drop', '18:00'))
                        priority = int(employee.get('priority', 3))
                        max_delay = priority_delays.get(priority, 15)
                        adjusted_latest = latest_drop + max_delay
                        
                        if current_time > adjusted_latest:
                            delay = current_time - latest_drop
                            print(f"      ❌ HARD VIOLATION: {emp_id} late by {int(delay - max_delay)} min (beyond allowed {max_delay} min delay)")
                            hard_violations += 1
                        elif current_time > latest_drop:
                            delay = current_time - latest_drop
                            print(f"      ⚠️ Within delay tolerance: {emp_id} late by {int(delay)} min (allowed {max_delay} min)")
                    
                    # Check sharing violations NOW that we know all trip employees
                    for emp_id in trip_employees:
                        employee = emp_map[emp_id]
                        sharing_pref = employee.get('sharing_preference', 'triple').lower()
                        
                        if sharing_pref == 'single' and len(trip_employees) > 1:
                            print(f"      ⚠️ SOFT VIOLATION: {emp_id} wants single ride, but sharing with {len(trip_employees)-1} others")
                            soft_violations += 1
                        elif sharing_pref == 'double' and len(trip_employees) > 2:
                            print(f"      ⚠️ SOFT VIOLATION: {emp_id} wants max 2, but {len(trip_employees)} in trip")
                            soft_violations += 1
                        elif sharing_pref == 'triple' and len(trip_employees) > 3:
                            print(f"      ⚠️ SOFT VIOLATION: {emp_id} wants max 3, but {len(trip_employees)} in trip")
                            soft_violations += 1
                    
                    # Check capacity
                    if len(trip_employees) > int(vehicle['capacity']):
                        print(f"      ❌ HARD VIOLATION: {len(trip_employees)} employees exceeds capacity {vehicle['capacity']}")
                        hard_violations += 1
                    
                    current_location = (office_lat, office_lng)
            
            trip_cost = trip_distance * vehicle_cost_per_km
            total_cost += trip_cost
            total_time += (current_time - trip_start_time)
            
            print(f"    Trip distance: {trip_distance:.2f} km")
            print(f"    Trip cost: ${trip_cost:.2f}")
            print(f"    Employees: {', '.join(trip_employees)}")
    
    # Check if all employees are assigned
    all_emp_ids = set(e['employee_id'] for e in employees)
    unassigned = all_emp_ids - assigned_employees
    
    if unassigned:
        print(f"\n❌ UNASSIGNED EMPLOYEES: {', '.join(unassigned)}")
        hard_violations += len(unassigned)
    
    print("\n" + "="*80)
    print("VALIDATION SUMMARY")
    print("="*80)
    print(f"Total Cost: ${total_cost:.2f}")
    print(f"Total Time: {total_time:.0f} minutes")
    print(f"Hard Violations: {hard_violations}")
    print(f"Soft Violations: {soft_violations}")
    
    if hard_violations == 0 and soft_violations == 0:
        print("\n✅ SOLUTION IS VALID - No violations!")
    elif hard_violations == 0:
        print(f"\n⚠️ SOLUTION IS FEASIBLE - {soft_violations} soft violations")
    else:
        print(f"\n❌ SOLUTION HAS HARD VIOLATIONS - Not feasible")
    
    return {
        'total_cost': total_cost,
        'total_time': total_time,
        'hard_violations': hard_violations,
        'soft_violations': soft_violations
    }

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python validate_manual_solution.py <manual_solution.json> <data_file.xlsx>")
        sys.exit(1)
    
    validate_manual_solution(sys.argv[1], sys.argv[2])
