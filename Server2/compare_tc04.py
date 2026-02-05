import math, json

def haversine(lat1, lon1, lat2, lon2):
    R = 6371
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon/2)**2
    return R * 2 * math.asin(math.sqrt(a))

def time_to_min(t):
    parts = t.split(':')
    return int(parts[0]) * 60 + int(parts[1])

def min_to_time(m):
    return "{:02d}:{:02d}".format(int(m) // 60, int(m) % 60)

# Load input
with open('output/tc04_input.json') as f:
    data = json.load(f)

emps = {e['employee_id']: e for e in data['employees']}
vehs = {v['vehicle_id']: v for v in data['vehicles']}
office = (12.9716, 77.5946)
cost_w = data['metadata']['objective_cost_weight']
time_w = data['metadata']['objective_time_weight']

# Priority max delays
priority_delays = {
    1: data['metadata']['priority_1_max_delay_min'],
    2: data['metadata']['priority_2_max_delay_min'],
    3: data['metadata']['priority_3_max_delay_min'],
    4: data['metadata']['priority_4_max_delay_min'],
    5: data['metadata']['priority_5_max_delay_min'],
}

# Manual solution from user:
# V01: E1 -> office -> E3 -> office -> E4 -> office -> E10 -> office (4 solo trips)
# V02: E2 -> office -> E06, E05 -> office (1 solo + 1 double trip)
# V03: E9, E8, E7 -> office (1 triple trip)
manual = {
    'V01': [['E01'], ['E03'], ['E04'], ['E10']],
    'V02': [['E02'], ['E06', 'E05']],
    'V03': [['E09', 'E08', 'E07']]
}

def check_violations(vid, trip_idx, trip_emps, arrival_at_office, v):
    hard_v = []
    soft_v = []
    for eid in trip_emps:
        e = emps[eid]
        latest_drop = time_to_min(e['latest_drop'])
        priority = e['priority']
        max_delay = priority_delays[priority]
        hard_deadline = latest_drop + max_delay

        # Sharing preference check
        pref = e['sharing_preference']
        count = len(trip_emps)
        if pref == 'single' and count > 1:
            hard_v.append(eid + " sharing=single but " + str(count) + " in trip")
        elif pref == 'double' and count > 2:
            hard_v.append(eid + " sharing=double but " + str(count) + " in trip")
        elif pref == 'triple' and count > 3:
            hard_v.append(eid + " sharing=triple but " + str(count) + " in trip")

        # Vehicle preference check
        veh_pref = e['vehicle_preference']
        veh_cat = v['category']
        if veh_pref == 'premium' and veh_cat != 'premium':
            hard_v.append(eid + " needs premium but got " + veh_cat)

        # Time check
        if arrival_at_office > hard_deadline:
            hard_v.append(eid + " arrives at " + min_to_time(int(arrival_at_office)) +
                         " > hard deadline " + min_to_time(hard_deadline) +
                         " (latest_drop=" + e['latest_drop'][:5] + " + " + str(max_delay) + "min)")
        elif arrival_at_office > latest_drop:
            soft_v.append(eid + " arrives at " + min_to_time(int(arrival_at_office)) +
                         " > latest_drop " + e['latest_drop'][:5] +
                         " (but within " + str(max_delay) + "min grace)")

    return hard_v, soft_v

def analyze_solution(name, sol_routes, vehs_dict):
    print("")
    print("=" * 70)
    print("  " + name)
    print("=" * 70)
    total_cost = 0.0
    total_time = 0
    all_hard = []
    all_soft = []
    all_assigned = []

    for vid, trips in sorted(sol_routes.items()):
        v = vehs_dict[vid]
        cpk = v['cost_per_km']
        spd = v['avg_speed_kmph']
        avail = time_to_min(v['available_from'])
        veh_cost = 0.0
        veh_time = 0
        next_available = avail  # Track when vehicle is free for next trip
        print("")
        print("  " + vid + " (" + v['category'] + ", $" + str(cpk) + "/km, " + str(spd) + "kmph, cap=" + str(v['capacity']) + ")")
        print("  " + "-" * 60)

        for ti, trip_emps in enumerate(trips):
            all_assigned.extend(trip_emps)
            # Determine start position
            if ti == 0:
                start = (v['current_lat'], v['current_lng'])
                depart_time = avail
            else:
                start = office
                depart_time = next_available  # Back-to-back: depart when previous trip ends

            # Calculate route: depot/office -> pickup1 -> pickup2 -> ... -> office
            dist = 0.0
            current_time = depart_time
            prev = start
            pickup_details = []

            for eid in trip_emps:
                e = emps[eid]
                pos = (e['pickup_lat'], e['pickup_lng'])
                d = haversine(prev[0], prev[1], pos[0], pos[1])
                travel_min = (d / spd) * 60
                current_time += travel_min
                # Wait if arrived before earliest pickup
                earliest = time_to_min(e['earliest_pickup'])
                wait = 0
                if current_time < earliest:
                    wait = earliest - current_time
                    current_time = earliest
                dist += d
                pickup_details.append((eid, min_to_time(int(current_time)), wait))
                prev = pos

            # Final leg to office
            d_off = haversine(prev[0], prev[1], office[0], office[1])
            travel_min = (d_off / spd) * 60
            current_time += travel_min
            dist += d_off

            trip_cost = dist * cpk
            trip_time = int(current_time - depart_time)
            veh_cost += trip_cost
            veh_time += trip_time

            emp_str = " -> ".join(trip_emps)
            pickups_str = ", ".join([p[0] + "@" + p[1] + ("(w" + str(int(p[2])) + ")" if p[2] > 0 else "") for p in pickup_details])
            print("    Trip " + str(ti+1) + ": [" + emp_str + "] -> Office")
            print("      Depart: " + min_to_time(int(depart_time)) + " | Arrive office: " + min_to_time(int(current_time)))
            print("      Dist: " + "{:.2f}".format(dist) + "km | Cost: $" + "{:.2f}".format(trip_cost) + " | Time: " + str(trip_time) + "min")
            print("      Pickups: " + pickups_str)

            # Update next available time (vehicle returns to office at current_time)
            next_available = current_time

            # Check violations
            hv, sv = check_violations(vid, ti, trip_emps, current_time, v)
            for h in hv:
                print("      !! HARD VIOLATION: " + h)
                all_hard.append(h)
            for s in sv:
                print("      ~ Soft violation: " + s)
                all_soft.append(s)

        total_cost += veh_cost
        total_time += veh_time
        print("    " + vid + " total: $" + "{:.2f}".format(veh_cost) + " | " + str(veh_time) + "min")

    # Check for unassigned employees
    unassigned = [eid for eid in emps if eid not in all_assigned]
    if unassigned:
        print("")
        print("  !! UNASSIGNED: " + ", ".join(unassigned))

    score = cost_w * total_cost + time_w * total_time
    print("")
    print("  SUMMARY:")
    print("    Total Cost:       $" + "{:.2f}".format(total_cost))
    print("    Total Time:       " + str(total_time) + " min")
    print("    Score:            " + "{:.2f}".format(score))
    print("    Hard Violations:  " + str(len(all_hard)))
    print("    Soft Violations:  " + str(len(all_soft)))
    print("    Assigned:         " + str(len(all_assigned)) + "/" + str(len(emps)))
    return total_cost, total_time, score, len(all_hard), len(all_soft)

# Analyze manual solution
m_cost, m_time, m_score, m_hard, m_soft = analyze_solution("YOUR MANUAL SOLUTION", manual, vehs)

# Load solver solution
with open('output/tc04_new_run.json') as f:
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

s_cost, s_time, s_score, s_hard, s_soft = analyze_solution("SOLVER SOLUTION (ALNS)", solver_routes, vehs)

# Print comparison
print("")
print("=" * 70)
print("  SIDE-BY-SIDE COMPARISON")
print("=" * 70)
header = "  {:<22} {:>14} {:>14} {:>14}".format("Metric", "Manual", "Solver", "Diff")
print(header)
print("  " + "-" * 64)
print("  {:<22} {:>14.2f} {:>14.2f} {:>+14.2f}".format("Cost ($)", m_cost, s_cost, s_cost - m_cost))
print("  {:<22} {:>14} {:>14} {:>+14}".format("Time (min)", m_time, s_time, s_time - m_time))
print("  {:<22} {:>14.2f} {:>14.2f} {:>+14.2f}".format("Score", m_score, s_score, s_score - m_score))
print("  {:<22} {:>14} {:>14}".format("Hard Violations", m_hard, s_hard))
print("  {:<22} {:>14} {:>14}".format("Soft Violations", m_soft, s_soft))
print("  {:<22} {:>14} {:>14}".format("Assigned", "10/10", str(len([e for trips in solver_routes.values() for t in trips for e in t])) + "/10"))

print("")
# Determine winner based on: fewer hard -> fewer soft -> lower score
if m_hard < s_hard:
    winner = "MANUAL"
elif m_hard > s_hard:
    winner = "SOLVER"
elif m_soft < s_soft:
    winner = "MANUAL"
elif m_soft > s_soft:
    winner = "SOLVER"
elif m_score < s_score:
    winner = "MANUAL"
elif m_score > s_score:
    winner = "SOLVER"
else:
    winner = "TIE"

if winner == "MANUAL":
    pct = ((s_score - m_score) / s_score * 100) if s_score > m_score else 0
    reason = ""
    if m_hard < s_hard:
        reason = " (fewer hard violations: " + str(m_hard) + " vs " + str(s_hard) + ")"
    elif m_soft < s_soft:
        reason = " (fewer soft violations: " + str(m_soft) + " vs " + str(s_soft) + ")"
    else:
        reason = " (" + "{:.1f}".format(pct) + "% better score)"
    print("  ** YOUR MANUAL SOLUTION WINS" + reason)
elif winner == "SOLVER":
    pct = ((m_score - s_score) / m_score * 100) if m_score > s_score else 0
    reason = ""
    if s_hard < m_hard:
        reason = " (fewer hard violations: " + str(s_hard) + " vs " + str(m_hard) + ")"
    elif s_soft < m_soft:
        reason = " (fewer soft violations: " + str(s_soft) + " vs " + str(m_soft) + ")"
    else:
        reason = " (" + "{:.1f}".format(pct) + "% better score)"
    print("  ** SOLVER WINS" + reason)
else:
    print("  ** TIE!")
