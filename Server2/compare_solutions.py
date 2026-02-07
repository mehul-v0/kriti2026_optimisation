import math, json

def haversine(lat1, lon1, lat2, lon2):
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1))*math.cos(math.radians(lat2))*math.sin(dlon/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

with open('output/tc_04.json') as f:
    data = json.load(f)

office = (12.9716, 77.5946)
emps = {}
for e in data['employees']:
    eid = e['employee_id']
    h, m = e['earliest_pickup'].split(':')[:2]
    earliest = int(h)*60 + int(m)
    h, m = e['latest_drop'].split(':')[:2]
    latest_drop = int(h)*60 + int(m)
    delays = {1:5, 2:10, 3:15, 4:20, 5:30}
    emps[eid] = {
        'lat': e['pickup_lat'], 'lng': e['pickup_lng'],
        'earliest': earliest, 'latest_drop': latest_drop,
        'priority': e['priority'],
        'deadline': latest_drop + delays[e['priority']],
        'vehicle_pref': e.get('vehicle_preference','any'),
        'sharing_pref': e.get('sharing_preference','triple'),
    }

vehs = {}
for v in data['vehicles']:
    vehs[v['vehicle_id']] = {
        'lat': v['current_lat'], 'lng': v['current_lng'],
        'cap': v['capacity'], 'cost_km': v['cost_per_km'],
        'speed': v['avg_speed_kmph'], 'avail': 8*60, 'cat': v['category']
    }

def fmt(t):
    return f"{t//60:02d}:{t%60:02d}"

def simulate_vehicle(vid, trips):
    v = vehs[vid]
    total_cost = 0; total_time = 0; hard_v = 0
    curr_time = v['avail']
    is_first = True
    print(f"\n=== {vid} ({v['cat']}, cap={v['cap']}, cost={v['cost_km']}/km, speed={v['speed']}km/h) ===")
    for trip_num, trip_emps in enumerate(trips, 1):
        if is_first:
            curr_lat, curr_lng = v['lat'], v['lng']
            is_first = False
        else:
            curr_lat, curr_lng = office
        trip_start = curr_time
        trip_dist = 0
        sharing_count = len(trip_emps)
        print(f"  Trip {trip_num}: {trip_emps}")
        for eid in trip_emps:
            e = emps[eid]
            sp = {'single':1,'double':2,'triple':3}.get(e['sharing_pref'],3)
            if sharing_count > sp:
                print(f"    HARD VIOLATION: {eid} sharing_pref={e['sharing_pref']} but {sharing_count} in trip")
                hard_v += 1
            if e['vehicle_pref'] == 'premium' and v['cat'] != 'premium':
                print(f"    HARD VIOLATION: {eid} wants premium but {vid} is {v['cat']}")
                hard_v += 1
            if e['vehicle_pref'] == 'normal' and v['cat'] == 'premium':
                print(f"    HARD VIOLATION: {eid} wants normal but {vid} is premium")
                hard_v += 1
            d = haversine(curr_lat, curr_lng, e['lat'], e['lng'])
            travel = round((d / v['speed']) * 60)
            arrival = curr_time + travel
            depart = max(arrival, e['earliest'])
            wait = depart - arrival
            trip_dist += d
            curr_time = depart
            curr_lat, curr_lng = e['lat'], e['lng']
            print(f"    {eid}: dist={d:.2f}km, arrive={fmt(arrival)}, wait={wait}min, depart={fmt(depart)}")
        d = haversine(curr_lat, curr_lng, office[0], office[1])
        travel = round((d / v['speed']) * 60)
        off_arr = curr_time + travel
        trip_dist += d
        curr_time = off_arr
        trip_cost = trip_dist * v['cost_km']
        trip_time = off_arr - trip_start
        total_cost += trip_cost
        total_time += trip_time
        print(f"    -> Office: dist={d:.2f}km, arrive={fmt(off_arr)}")
        print(f"    Trip: cost=${trip_cost:.2f}, dist={trip_dist:.2f}km, time={trip_time}min")
        for eid in trip_emps:
            e = emps[eid]
            if off_arr > e['deadline']:
                print(f"    HARD VIOLATION: {eid} office arrival {fmt(off_arr)} > deadline {fmt(e['deadline'])}")
                hard_v += 1
    print(f"  VEHICLE TOTAL: cost=${total_cost:.2f}, time={total_time}min, hard_violations={hard_v}")
    return total_cost, total_time, hard_v

print("=" * 65)
print("MANUAL SOLUTION")
print("=" * 65)
m_cost = m_time = m_hard = 0
for vid, trips in [
    ('V01', [['E01'], ['E03'], ['E04'], ['E10']]),
    ('V02', [['E02'], ['E06','E05']]),
    ('V03', [['E09','E08','E07']]),
]:
    c, t, h = simulate_vehicle(vid, trips)
    m_cost += c; m_time += t; m_hard += h
score_m = 0.7 * m_cost + 0.3 * m_time
print(f"\n{'='*65}")
print(f"MANUAL TOTALS: cost=${m_cost:.2f}, time={m_time}min, hard={m_hard}, score={score_m:.2f}")
print(f"{'='*65}")

print(f"\n\n{'=' * 65}")
print("SOLVER SOLUTION (from sol_04_new.json)")
print("=" * 65)
s_cost = s_time = s_hard = 0
for vid, trips in [
    ('V01', [['E02'], ['E04'], ['E06','E05'], ['E10']]),
    ('V02', [['E01'], ['E03']]),
    ('V03', [['E09','E08','E07']]),
]:
    c, t, h = simulate_vehicle(vid, trips)
    s_cost += c; s_time += t; s_hard += h
score_s = 0.7 * s_cost + 0.3 * s_time
print(f"\n{'='*65}")
print(f"SOLVER TOTALS: cost=${s_cost:.2f}, time={s_time}min, hard={s_hard}, score={score_s:.2f}")
print(f"{'='*65}")

print(f"\n\n{'=' * 65}")
print("COMPARISON SUMMARY")
print("=" * 65)
print(f"                  Manual          Solver          Diff")
print(f"  Cost:          ${m_cost:>10.2f}     ${s_cost:>10.2f}     {s_cost-m_cost:+.2f}")
print(f"  Time:          {m_time:>10}min  {s_time:>10}min  {s_time-m_time:+d}min")
print(f"  Hard Viol:     {m_hard:>10}     {s_hard:>10}     {s_hard-m_hard:+d}")
print(f"  Score:         {score_m:>10.2f}     {score_s:>10.2f}     {score_s-score_m:+.2f}")
print(f"  Vehicles used: {3:>10}     {3:>10}")
if m_hard == s_hard:
    if m_cost < s_cost:
        print(f"\n  >> MANUAL WINS on cost (same violations)")
    elif s_cost < m_cost:
        print(f"\n  >> SOLVER WINS on cost (same violations)")
    else:
        print(f"\n  >> TIE")
elif m_hard < s_hard:
    print(f"\n  >> MANUAL WINS (fewer violations)")
else:
    print(f"\n  >> SOLVER WINS (fewer violations)")
