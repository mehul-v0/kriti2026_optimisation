import math, json

def haversine(lat1, lon1, lat2, lon2):
    R = 6371
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon/2)**2
    return R * 2 * math.asin(math.sqrt(a))

with open('output/tc01_input.json') as f:
    data = json.load(f)

emps = {e['employee_id']: e for e in data['employees']}
vehs = {v['vehicle_id']: v for v in data['vehicles']}
office = (12.9716, 77.5946)
cost_w = data['metadata']['objective_cost_weight']
time_w = data['metadata']['objective_time_weight']

# Manual solution
manual = {
    'V01': [['E06'], ['E01']],
    'V02': [['E02', 'E05'], ['E07', 'E08']],
    'V03': [['E04', 'E03']]
}

def analyze_solution(name, sol_routes, vehs_dict):
    print(f"\n{'='*60}")
    print(f"  {name}")
    print(f"{'='*60}")
    total_cost = 0
    total_time = 0
    total_dist = 0
    for vid, trips in sol_routes.items():
        v = vehs_dict[vid]
        veh_cost = 0
        veh_time = 0
        for ti, trip_emps in enumerate(trips):
            if ti == 0:
                start = (v['current_lat'], v['current_lng'])
            else:
                start = office
            dist = 0
            prev = start
            for eid in trip_emps:
                e = emps[eid]
                pos = (e['pickup_lat'], e['pickup_lng'])
                d = haversine(prev[0], prev[1], pos[0], pos[1])
                dist += d
                prev = pos
            d_off = haversine(prev[0], prev[1], office[0], office[1])
            dist += d_off
            trip_cost = dist * v['cost_per_km']
            trip_time = round((dist / v['avg_speed_kmph']) * 60)
            veh_cost += trip_cost
            veh_time += trip_time
            emp_str = " -> ".join(trip_emps)
            print(f"  {vid} Trip {ti+1}: {emp_str} -> Office | {dist:.2f}km ${trip_cost:.2f} {trip_time}min")
        total_cost += veh_cost
        total_time += veh_time
    score = cost_w * total_cost + time_w * total_time
    print(f"\n  Total Cost:  ${total_cost:.2f}")
    print(f"  Total Time:  {total_time} min")
    print(f"  Score:       {score:.2f} ({cost_w}*cost + {time_w}*time)")
    return total_cost, total_time, score

# Analyze manual solution
m_cost, m_time, m_score = analyze_solution("MANUAL SOLUTION (Your Optimal)", manual, vehs)

# Load and analyze solver solution  
with open('output/tc01_new_run.json') as f:
    solver = json.load(f)

solver_routes = {}
for v in solver['vehicles']:
    vid = v['vehicle_id']
    trips = []
    for t in v['trips']:
        trip_emps = [s['location'].replace(' Pickup', '') for s in t['stops'] 
                     if 'Pickup' in s['location']]
        if trip_emps:
            trips.append(trip_emps)
    if trips:
        solver_routes[vid] = trips

s_cost, s_time, s_score = analyze_solution("SOLVER SOLUTION", solver_routes, vehs)

# Print comparison
print(f"\n{'='*60}")
print(f"  COMPARISON")
print(f"{'='*60}")
print(f"  {'Metric':<20} {'Manual':>12} {'Solver':>12} {'Diff':>12}")
print(f"  {'-'*56}")
print(f"  {'Cost ($)':<20} {m_cost:>12.2f} {s_cost:>12.2f} {s_cost-m_cost:>+12.2f}")
print(f"  {'Time (min)':<20} {m_time:>12} {s_time:>12} {s_time-m_time:>+12}")
print(f"  {'Score':<20} {m_score:>12.2f} {s_score:>12.2f} {s_score-m_score:>+12.2f}")
print(f"  {'Hard Violations':<20} {'0':>12} {solver.get('hard_violations',0) or 0:>12}")
print(f"  {'Soft Violations':<20} {'0':>12} {solver.get('soft_violations',0) or 0:>12}")
print(f"  {'Solution Type':<20} {'MANUAL':>12} {solver['solution_type']}")

if s_score < m_score:
    pct = ((m_score - s_score) / m_score) * 100
    print(f"\n  ★ SOLVER WINS by {pct:.1f}% ({m_score - s_score:.2f} points better)")
elif s_score > m_score:
    pct = ((s_score - m_score) / m_score) * 100
    print(f"\n  ★ MANUAL WINS by {pct:.1f}% ({s_score - m_score:.2f} points worse)")
else:
    print(f"\n  ★ TIE!")
