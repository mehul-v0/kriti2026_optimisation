import json
import sys
from math import radians, cos, sin, asin, sqrt

def haversine(lat1, lon1, lat2, lon2):
    """Calculate distance between two points in km"""
    lat1, lon1, lat2, lon2 = map(radians, [lat1, lon1, lat2, lon2])
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    a = sin(dlat/2)**2 + cos(lat1) * cos(lat2) * sin(dlon/2)**2
    c = 2 * asin(sqrt(a))
    return c * 6371

def time_to_minutes(time_str):
    """Convert HH:MM or HH:MM:SS to minutes from midnight"""
    parts = time_str.split(':')
    if len(parts) == 2:
        h, m = map(int, parts)
    elif len(parts) == 3:
        h, m, s = map(int, parts)
    else:
        raise ValueError(f"Invalid time format: {time_str}")
    return h * 60 + m

def minutes_to_time(minutes):
    """Convert minutes from midnight to HH:MM"""
    h = int(minutes // 60)
    m = int(minutes % 60)
    return f"{h:02d}:{m:02d}"

def validate_solution(input_file, solution_file):
    """Validate a VRP solution against input constraints"""
    
    # Load data
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    with open(solution_file, 'r') as f:
        solution = json.load(f)
    
    # Build lookup maps
    employees = {e['employee_id']: e for e in data['employees']}
    vehicles = {v['vehicle_id']: v for v in data['vehicles']}
    
    # Get priority max delays
    metadata = data.get('metadata', {})
    priority_max_delays = {
        1: metadata.get('priority_1_max_delay_min', 5),
        2: metadata.get('priority_2_max_delay_min', 10),
        3: metadata.get('priority_3_max_delay_min', 15),
        4: metadata.get('priority_4_max_delay_min', 20),
        5: metadata.get('priority_5_max_delay_min', 30)
    }
    
    print("=" * 70)
    print(f"VALIDATING: {solution_file}")
    print("=" * 70)
    
    # Track violations
    hard_violations = []
    soft_violations = []
    assigned_employees = set()
    
    # Validate each vehicle's trips
    for vehicle_sol in solution['vehicles']:
        vehicle_id = vehicle_sol['vehicle_id']
        vehicle = vehicles[vehicle_id]
        vehicle_type = vehicle.get('category', vehicle.get('type', 'unknown'))
        
        print(f"\n{vehicle_id} ({vehicle_type}, capacity: {vehicle['capacity']})")
        
        for trip in vehicle_sol.get('trips', []):
            trip_num = trip['trip_number']
            stops = trip['stops']
            
            # Extract employees from pickup stops
            emp_ids = [s['location'].split()[0] for s in stops if 'Pickup' in s['location']]
            
            if not emp_ids:
                continue
                
            print(f"  Trip {trip_num}: {', '.join(emp_ids)}")
            
            # Check capacity
            if len(emp_ids) > vehicle['capacity']:
                msg = f"    ❌ CAPACITY: {len(emp_ids)} > {vehicle['capacity']}"
                hard_violations.append(f"{vehicle_id} Trip{trip_num}: {msg}")
                print(msg)
            
            # Track assigned employees
            for emp_id in emp_ids:
                if emp_id in assigned_employees:
                    msg = f"    ❌ DUPLICATE: {emp_id} assigned multiple times"
                    hard_violations.append(msg)
                    print(msg)
                assigned_employees.add(emp_id)
            
            # Validate sharing preferences
            for emp_id in emp_ids:
                emp = employees[emp_id]
                sharing = emp.get('sharing_preference', 0)
                
                # Map string to value
                if isinstance(sharing, str):
                    if sharing == 'single':
                        sharing = 1
                    elif sharing == 'double':
                        sharing = 2
                    elif sharing == 'triple':
                        sharing = 3
                    else:
                        sharing = 0
                
                if sharing == 1 and len(emp_ids) > 1:
                    msg = f"    ❌ SHARING: {emp_id} wants single but with {len(emp_ids)} total"
                    soft_violations.append(msg)
                    print(msg)
                elif sharing == 2 and len(emp_ids) > 2:
                    msg = f"    ❌ SHARING: {emp_id} wants max 2 but with {len(emp_ids)} total"
                    soft_violations.append(msg)
                    print(msg)
                elif sharing == 3 and len(emp_ids) > 3:
                    msg = f"    ❌ SHARING: {emp_id} wants max 3 but with {len(emp_ids)} total"
                    soft_violations.append(msg)
                    print(msg)
            
            # Validate vehicle preferences
            for emp_id in emp_ids:
                emp = employees[emp_id]
                veh_pref = emp.get('vehicle_preference', 0)
                
                # Map preference to value
                if isinstance(veh_pref, str):
                    if veh_pref == 'premium':
                        veh_pref = 1
                    elif veh_pref == 'normal':
                        veh_pref = 2
                    else:
                        veh_pref = 0
                
                if veh_pref == 1 and vehicle_type != 'premium':
                    msg = f"    ❌ VEHICLE: {emp_id} wants premium but in {vehicle_type}"
                    soft_violations.append(msg)
                    print(msg)
                elif veh_pref == 2 and vehicle_type != 'normal':
                    msg = f"    ❌ VEHICLE: {emp_id} wants normal but in {vehicle_type}"
                    soft_violations.append(msg)
                    print(msg)
            
            # Validate time windows
            for i, stop in enumerate(stops):
                if 'Pickup' in stop['location']:
                    emp_id = stop['location'].split()[0]
                    emp = employees[emp_id]
                    
                    # Use departure time (after waiting) as actual pickup time
                    pickup_min = time_to_minutes(stop['departure_time'])
                    earliest = time_to_minutes(emp['earliest_pickup'])
                    latest = time_to_minutes(emp.get('latest_pickup', emp.get('latest_drop', '23:59')))
                    
                    if pickup_min < earliest:
                        msg = f"    ❌ EARLY: {emp_id} picked at {stop['departure_time']}, earliest {emp['earliest_pickup']}"
                        hard_violations.append(msg)
                        print(msg)
                    elif pickup_min > latest:
                        latest_str = emp.get('latest_pickup', emp.get('latest_drop', '23:59'))
                        msg = f"    ❌ LATE: {emp_id} picked at {stop['departure_time']}, latest {latest_str}"
                        hard_violations.append(msg)
                        print(msg)
                
                # Check office arrival deadline (with priority max delay buffer)
                if 'Drop-off' in stop['location'] or stop['location'] == 'Office':
                    arrival_min = time_to_minutes(stop['arrival_time'])
                    for emp_id in emp_ids:
                        emp = employees[emp_id]
                        deadline = time_to_minutes(emp.get('arrival_deadline', emp.get('latest_drop', '23:59')))
                        priority = emp.get('priority', 5)
                        max_delay = priority_max_delays.get(priority, 30)
                        
                        # Hard violation only if beyond deadline + max_delay
                        if arrival_min > deadline + max_delay:
                            deadline_str = emp.get('arrival_deadline', emp.get('latest_drop', '23:59'))
                            delay_min = arrival_min - deadline
                            msg = f"    ❌ DEADLINE: {emp_id} (P{priority}) arrives at {stop['arrival_time']}, deadline {deadline_str} (+{max_delay}min grace = {minutes_to_time(deadline + max_delay)}), actual delay: {delay_min}min"
                            hard_violations.append(msg)
                            print(msg)
                            break
    
    # Check all employees assigned
    missing = set(employees.keys()) - assigned_employees
    if missing:
        msg = f"❌ UNASSIGNED: {', '.join(sorted(missing))}"
        hard_violations.append(msg)
        print(f"\n{msg}")
    
    # Summary
    print("\n" + "=" * 70)
    print("VALIDATION SUMMARY")
    print("=" * 70)
    print(f"Total Cost: ${solution['total_cost']:.2f}")
    print(f"Total Time: {solution['total_time']:.0f} minutes")
    print(f"Hard Violations: {len(hard_violations)}")
    print(f"Soft Violations: {len(soft_violations)}")
    
    if len(hard_violations) == 0 and len(soft_violations) == 0:
        print("\n✅ VALID SOLUTION - No violations!")
    elif len(hard_violations) == 0:
        print(f"\n⚠️  FEASIBLE - {len(soft_violations)} soft violations")
    else:
        print(f"\n❌ INFEASIBLE - {len(hard_violations)} hard violations")
    
    return len(hard_violations), len(soft_violations)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python validate_solution.py <input.json> <solution.json>")
        print("Example: python validate_solution.py tc01_input.json tc01_solution_final.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    solution_file = sys.argv[2]
    
    validate_solution(input_file, solution_file)
