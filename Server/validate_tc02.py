"""
Validate and compare manual solution vs solver output for TC02.
Checks ALL constraints: time windows, capacity, sharing, vehicle preference.
"""

import json
import math

# ============================================================================
# UTILITIES
# ============================================================================

def haversine_km(lat1, lon1, lat2, lon2):
    R = 6371.0
    phi1 = math.radians(float(lat1))
    phi2 = math.radians(float(lat2))
    dphi = math.radians(float(lat2) - float(lat1))
    dlambda = math.radians(float(lon2) - float(lon1))
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlambda/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def time_to_min(t):
    if isinstance(t, (int, float)):
        return int(t)
    parts = str(t).strip().split(':')
    return int(parts[0]) * 60 + int(parts[1])

def min_to_time(m):
    h = int(m) // 60
    mi = int(m) % 60
    return f"{h:02d}:{mi:02d}"

# ============================================================================
# LOAD TEST CASE
# ============================================================================

with open('output/tc_02.json', 'r') as f:
    tc = json.load(f)

employees = tc['employees']
vehicles = tc['vehicles']
metadata = tc['metadata']
baseline = tc['baseline']

emp_map = {e['employee_id']: e for e in employees}
veh_map = {v['vehicle_id']: v for v in vehicles}

office_lat = float(employees[0]['drop_lat'])
office_lng = float(employees[0]['drop_lng'])

priority_delays = {
    1: metadata['priority_1_max_delay_min'],
    2: metadata['priority_2_max_delay_min'],
    3: metadata['priority_3_max_delay_min'],
    4: metadata['priority_4_max_delay_min'],
    5: metadata['priority_5_max_delay_min'],
}

cost_weight = metadata['objective_cost_weight']
time_weight = metadata['objective_time_weight']

baseline_cost = sum(b['baseline_cost'] for b in baseline)

# ============================================================================
# VALIDATE A SOLUTION
# ============================================================================

def validate_solution(solution_name, vehicle_trips):
    """
    vehicle_trips: list of dicts, each with:
      - vehicle_id: str
      - trips: list of trips, each trip is list of employee_ids (in pickup order)
    
    Returns dict with total_cost, total_time, score, violations, etc.
    """
    print(f"\n{'='*80}")
    print(f"  VALIDATING: {solution_name}")
    print(f"{'='*80}")
    
    total_cost = 0.0
    total_time = 0.0
    hard_violations = []
    soft_violations = []
    assigned = set()
    
    for vt in vehicle_trips:
        vid = vt['vehicle_id']
        veh = veh_map[vid]
        speed = float(veh['avg_speed_kmph'])
        cost_per_km = float(veh['cost_per_km'])
        capacity = int(veh['capacity'])
        category = veh['category'].lower()
        avail_from = time_to_min(veh['available_from'])
        
        veh_start_lat = float(veh['current_lat'])
        veh_start_lng = float(veh['current_lng'])
        
        current_time = avail_from
        current_lat = veh_start_lat
        current_lng = veh_start_lng
        
        veh_total_cost = 0.0
        veh_total_time = 0.0
        veh_start_time = avail_from
        
        print(f"\n--- {vid} (category={category}, capacity={capacity}, speed={speed}, cost/km={cost_per_km}) ---")
        
        for trip_idx, trip_emps in enumerate(vt['trips'], 1):
            trip_distance = 0.0
            trip_start_time = current_time
            
            print(f"\n  Trip {trip_idx}: {trip_emps}")
            
            if trip_idx == 1:
                print(f"    {min_to_time(int(current_time))} - Vehicle Start ({current_lat:.4f}, {current_lng:.4f})")
            else:
                print(f"    {min_to_time(int(current_time))} - Office Start ({current_lat:.4f}, {current_lng:.4f})")
            
            # Capacity check
            if len(trip_emps) > capacity:
                msg = f"HARD: {vid} T{trip_idx} has {len(trip_emps)} passengers, capacity={capacity}"
                hard_violations.append(msg)
                print(f"    ❌ {msg}")
            
            # Sharing preference check
            for eid in trip_emps:
                emp = emp_map[eid]
                sharing = emp['sharing_preference'].lower()
                max_share = {'single': 1, 'double': 2, 'triple': 3}.get(sharing, 3)
                if len(trip_emps) > max_share:
                    msg = f"SOFT: {eid} wants {sharing} (max {max_share}), but {len(trip_emps)} in trip"
                    soft_violations.append(msg)
                    print(f"    ⚠️  {msg}")
            
            # Vehicle preference check
            for eid in trip_emps:
                emp = emp_map[eid]
                pref = emp['vehicle_preference'].lower()
                if pref != 'any' and pref != category:
                    msg = f"SOFT: {eid} prefers {pref} vehicle, got {category}"
                    soft_violations.append(msg)
                    print(f"    ⚠️  {msg}")
            
            # Duplicate check
            for eid in trip_emps:
                if eid in assigned:
                    msg = f"HARD: {eid} already assigned in another trip!"
                    hard_violations.append(msg)
                    print(f"    ❌ {msg}")
                assigned.add(eid)
            
            # Simulate route: pickup each employee, then drive to office
            pickup_times = {}
            for eid in trip_emps:
                emp = emp_map[eid]
                elat = float(emp['pickup_lat'])
                elng = float(emp['pickup_lng'])
                
                dist = haversine_km(current_lat, current_lng, elat, elng)
                travel_min = (dist / speed) * 60
                current_time += travel_min
                trip_distance += dist
                
                earliest = time_to_min(emp['earliest_pickup'])
                if current_time < earliest:
                    wait = earliest - current_time
                    print(f"    {min_to_time(int(current_time))} - Arrive {eid} (wait {wait:.0f} min for earliest {min_to_time(earliest)})")
                    current_time = earliest
                
                pickup_times[eid] = current_time
                print(f"    {min_to_time(int(current_time))} - Pickup [{eid}] (dist={dist:.2f}km)")
                
                current_lat = elat
                current_lng = elng
            
            # Drive to office
            dist_to_office = haversine_km(current_lat, current_lng, office_lat, office_lng)
            travel_min = (dist_to_office / speed) * 60
            current_time += travel_min
            trip_distance += dist_to_office
            
            print(f"    {min_to_time(int(current_time))} - Office Drop-off (dist={dist_to_office:.2f}km)")
            
            # Check time window violations for each employee
            for eid in trip_emps:
                emp = emp_map[eid]
                latest_drop = time_to_min(emp['latest_drop'])
                priority = int(emp['priority'])
                max_delay = priority_delays[priority]
                
                if current_time > latest_drop + max_delay:
                    actual_delay = current_time - latest_drop
                    msg = f"HARD: {eid} arrives office at {min_to_time(int(current_time))}, latest={min_to_time(latest_drop)}, delay={actual_delay:.0f}min, max_allowed={max_delay}min"
                    hard_violations.append(msg)
                    print(f"    ❌ {msg}")
                elif current_time > latest_drop:
                    actual_delay = current_time - latest_drop
                    print(f"    ⏰ {eid}: within delay tolerance ({actual_delay:.0f}/{max_delay} min)")
            
            trip_cost = trip_distance * cost_per_km
            trip_time = current_time - trip_start_time
            veh_total_cost += trip_cost
            
            print(f"    Trip distance: {trip_distance:.2f} km, cost: ${trip_cost:.2f}, time: {trip_time:.0f} min")
            
            # After drop-off at office, vehicle is at office
            current_lat = office_lat
            current_lng = office_lng
        
        veh_total_time = current_time - veh_start_time
        total_cost += veh_total_cost
        total_time += veh_total_time
        
        print(f"  {vid} Total: cost=${veh_total_cost:.2f}, time={veh_total_time:.0f} min")
    
    # Check unassigned
    all_emps = set(e['employee_id'] for e in employees)
    unassigned = all_emps - assigned
    if unassigned:
        for eid in sorted(unassigned):
            msg = f"HARD: {eid} is unassigned!"
            hard_violations.append(msg)
            print(f"\n❌ {msg}")
    
    score = cost_weight * total_cost + time_weight * total_time
    savings = baseline_cost - total_cost
    savings_pct = (savings / baseline_cost * 100) if baseline_cost > 0 else 0
    
    print(f"\n{'─'*60}")
    print(f"  SUMMARY: {solution_name}")
    print(f"{'─'*60}")
    print(f"  Total Cost:      ${total_cost:.2f}")
    print(f"  Total Time:      {total_time:.0f} min")
    print(f"  Score:           {score:.2f}  (0.65*cost + 0.35*time)")
    print(f"  Baseline Cost:   ${baseline_cost:.2f}")
    print(f"  Cost Savings:    ${savings:.2f} ({savings_pct:.1f}%)")
    print(f"  Hard Violations: {len(hard_violations)}")
    print(f"  Soft Violations: {len(soft_violations)}")
    
    if hard_violations:
        print(f"\n  ❌ HARD VIOLATIONS:")
        for v in hard_violations:
            print(f"     - {v}")
    if soft_violations:
        print(f"\n  ⚠️  SOFT VIOLATIONS:")
        for v in soft_violations:
            print(f"     - {v}")
    
    if len(hard_violations) == 0:
        print(f"\n  ✅ SOLUTION IS FEASIBLE")
    else:
        print(f"\n  ❌ SOLUTION IS INFEASIBLE ({len(hard_violations)} hard violations)")
    
    return {
        'total_cost': total_cost,
        'total_time': total_time,
        'score': score,
        'hard_violations': len(hard_violations),
        'soft_violations': len(soft_violations),
        'hard_violation_details': hard_violations,
        'soft_violation_details': soft_violations,
    }


# ============================================================================
# MANUAL SOLUTION (as provided by user)
# ============================================================================

manual_solution = [
    {'vehicle_id': 'V01', 'trips': [
        ['E02', 'E11'],   # T1: from depot
        ['E12'],           # T2: from office
        ['E06'],           # T3: from office
    ]},
    {'vehicle_id': 'V02', 'trips': [
        ['E05', 'E09'],   # T1: from depot
    ]},
    {'vehicle_id': 'V03', 'trips': [
        ['E10', 'E08', 'E03'],  # T1: from depot
    ]},
    {'vehicle_id': 'V04', 'trips': [
        ['E01'],           # T1: from depot
    ]},
    {'vehicle_id': 'V05', 'trips': [
        ['E07'],           # T1: from depot
        ['E04'],           # T2: from office
    ]},
]

# ============================================================================
# SOLVER SOLUTION (from final_tc02.json)
# ============================================================================

# Parse the solver output into the same format
with open('output/final_tc02.json', 'r') as f:
    solver_out = json.load(f)

solver_solution = []
for sv in solver_out['vehicles']:
    vid = sv['vehicle_id']
    trips = []
    for trip in sv['trips']:
        trip_emps = []
        for stop in trip['stops']:
            loc = stop['location']
            if 'Pickup' in loc:
                # Extract employee ID
                import re
                m = re.match(r'(E\d+)', loc)
                if m:
                    trip_emps.append(m.group(1))
        if trip_emps:
            trips.append(trip_emps)
    if trips:
        solver_solution.append({'vehicle_id': vid, 'trips': trips})


# ============================================================================
# RUN VALIDATION
# ============================================================================

print("\n" + "=" * 80)
print("  TC02 SOLUTION COMPARISON")
print("  Employees:", len(employees))
print("  Vehicles:", len(vehicles))
print("  Baseline Cost: $" + f"{baseline_cost:.2f}")
print("  Scoring: {:.0f}% cost + {:.0f}% time".format(cost_weight*100, time_weight*100))
print("=" * 80)

# Print employee constraints summary
print("\n  Employee Constraints:")
print(f"  {'ID':<5} {'Priority':<9} {'Window':<20} {'VehPref':<10} {'Sharing':<8}")
for e in employees:
    tw = f"{e['earliest_pickup']}-{e['latest_drop']}"
    print(f"  {e['employee_id']:<5} P{e['priority']:<8} {tw:<20} {e['vehicle_preference']:<10} {e['sharing_preference']:<8}")

print(f"\n  Vehicle Fleet:")
print(f"  {'ID':<5} {'Cap':<5} {'Category':<10} {'Speed':<8} {'$/km':<8} {'Available'}")
for v in vehicles:
    print(f"  {v['vehicle_id']:<5} {v['capacity']:<5} {v['category']:<10} {v['avg_speed_kmph']:<8} {v['cost_per_km']:<8} {v['available_from']}")

# Validate manual solution
manual_result = validate_solution("MANUAL SOLUTION (as provided)", manual_solution)

# Validate solver solution
solver_result = validate_solution("C++ SOLVER SOLUTION (final_tc02.json)", solver_solution)


# ============================================================================
# ALSO TRY REORDERED MANUAL (E06 first, E12 second, E02+E11 third)
# ============================================================================

manual_reordered = [
    {'vehicle_id': 'V01', 'trips': [
        ['E06'],           # T1: from depot (serve E06 first - tight deadline 09:00)
        ['E12'],           # T2: from office (serve E12 second - deadline 09:15)
        ['E02', 'E11'],   # T3: from office (E02+E11 have flexible windows)
    ]},
    {'vehicle_id': 'V02', 'trips': [
        ['E05', 'E09'],
    ]},
    {'vehicle_id': 'V03', 'trips': [
        ['E10', 'E08', 'E03'],
    ]},
    {'vehicle_id': 'V04', 'trips': [
        ['E01'],
    ]},
    {'vehicle_id': 'V05', 'trips': [
        ['E07'],
        ['E04'],
    ]},
]

manual_reordered_result = validate_solution("MANUAL REORDERED (E06→E12→E02,E11)", manual_reordered)


# ============================================================================
# FINAL COMPARISON
# ============================================================================

print("\n\n" + "=" * 80)
print("  FINAL COMPARISON")
print("=" * 80)

results = [
    ("Manual (as written)", manual_result),
    ("Manual (reordered)", manual_reordered_result),
    ("C++ Solver", solver_result),
]

print(f"\n  {'Solution':<25} {'Cost':>10} {'Time':>10} {'Score':>10} {'Hard':>6} {'Soft':>6} {'Status':<15}")
print("  " + "─" * 85)
for name, r in results:
    status = "✅ Feasible" if r['hard_violations'] == 0 else f"❌ {r['hard_violations']} violations"
    print(f"  {name:<25} ${r['total_cost']:>8.2f} {r['total_time']:>8.0f}m {r['score']:>9.2f} {r['hard_violations']:>5} {r['soft_violations']:>5}  {status}")

# Find best feasible
feasible = [(name, r) for name, r in results if r['hard_violations'] == 0]
if feasible:
    best = min(feasible, key=lambda x: x[1]['score'])
    print(f"\n  🏆 BEST FEASIBLE SOLUTION: {best[0]} (score={best[1]['score']:.2f})")
else:
    print(f"\n  ⚠️  No feasible solution found!")
