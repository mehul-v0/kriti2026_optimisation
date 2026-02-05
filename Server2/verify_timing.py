import math, json

def haversine(lat1, lon1, lat2, lon2):
    R = 6371
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon/2)**2
    return R * 2 * math.asin(math.sqrt(a))

def min_to_time(m):
    return "{:02d}:{:02d}:{:02.0f}".format(int(m) // 60, int(m) % 60, (m % 1) * 60)

with open('output/tc04_input.json') as f:
    data = json.load(f)

emps = {e['employee_id']: e for e in data['employees']}
office = (12.9716, 77.5946)

# V01: premium, 30 kmph, starts at (12.935, 77.62)
v01_start = (12.935, 77.62)
speed = 30.0

print("=== V01 Detailed Timing (back-to-back trips) ===")
print()

# Trip 1: E01 (solo)
e01 = emps['E01']
e01_pos = (e01['pickup_lat'], e01['pickup_lng'])

d1 = haversine(v01_start[0], v01_start[1], e01_pos[0], e01_pos[1])
t1 = (d1 / speed) * 60
print(f"Trip 1: V01 depot -> E01")
print(f"  V01 depot: ({v01_start[0]}, {v01_start[1]})")
print(f"  E01 pickup: ({e01_pos[0]}, {e01_pos[1]})")
print(f"  Distance: {d1:.4f} km")
print(f"  Travel time: {t1:.2f} min")
print(f"  Depart depot: 08:00 (480 min)")
print(f"  Arrive E01: {min_to_time(480 + t1)} ({480+t1:.2f} min)")

# Wait for earliest pickup?
e01_earliest = 8*60 + 10  # 08:10
arrival_e01 = 480 + t1
wait_e01 = max(0, e01_earliest - arrival_e01)
depart_e01 = max(arrival_e01, e01_earliest)
print(f"  E01 earliest_pickup: 08:10 (490 min)")
print(f"  Wait at E01: {wait_e01:.2f} min")
print(f"  Depart E01: {min_to_time(depart_e01)} ({depart_e01:.2f} min)")

# E01 -> Office
d1_off = haversine(e01_pos[0], e01_pos[1], office[0], office[1])
t1_off = (d1_off / speed) * 60
arrive_office_1 = depart_e01 + t1_off
print(f"  E01 -> Office distance: {d1_off:.4f} km")
print(f"  E01 -> Office travel: {t1_off:.2f} min")
print(f"  Arrive Office: {min_to_time(arrive_office_1)} ({arrive_office_1:.2f} min)")
print(f"  E01 latest_drop: 08:45 (525 min) | deadline: 08:50 (530 min)")
print(f"  {'OK' if arrive_office_1 <= 530 else 'VIOLATION'}")
print()

# Trip 2: E03 (solo) — starts from OFFICE when Trip 1 ends
e03 = emps['E03']
e03_pos = (e03['pickup_lat'], e03['pickup_lng'])

trip2_depart = arrive_office_1  # BACK-TO-BACK: next trip starts when previous ends
d2 = haversine(office[0], office[1], e03_pos[0], e03_pos[1])
t2 = (d2 / speed) * 60
arrive_e03 = trip2_depart + t2
print(f"Trip 2: Office -> E03 (departs when Trip 1 ends)")
print(f"  Office: ({office[0]}, {office[1]})")
print(f"  E03 pickup: ({e03_pos[0]}, {e03_pos[1]})")
print(f"  Distance: {d2:.4f} km")
print(f"  Travel time: {t2:.2f} min")
print(f"  Depart Office: {min_to_time(trip2_depart)} ({trip2_depart:.2f} min)")
print(f"  Arrive E03: {min_to_time(arrive_e03)} ({arrive_e03:.2f} min)")

e03_earliest = 8*60 + 20  # 08:20
wait_e03 = max(0, e03_earliest - arrive_e03)
depart_e03 = max(arrive_e03, e03_earliest)
print(f"  E03 earliest_pickup: 08:20 (500 min)")
print(f"  Wait at E03: {wait_e03:.2f} min")
print(f"  Depart E03: {min_to_time(depart_e03)} ({depart_e03:.2f} min)")

d2_off = haversine(e03_pos[0], e03_pos[1], office[0], office[1])
t2_off = (d2_off / speed) * 60
arrive_office_2 = depart_e03 + t2_off
print(f"  E03 -> Office distance: {d2_off:.4f} km")
print(f"  E03 -> Office travel: {t2_off:.2f} min")
print(f"  Arrive Office: {min_to_time(arrive_office_2)} ({arrive_office_2:.2f} min)")
print(f"  E03 latest_drop: 08:55 (535 min) | deadline: 09:00 (540 min)")
print(f"  {'OK' if arrive_office_2 <= 540 else 'VIOLATION'}: arrive {arrive_office_2:.2f} vs deadline 540")
print()

# Trip 3: E04 (solo) — starts from OFFICE when Trip 2 ends
e04 = emps['E04']
e04_pos = (e04['pickup_lat'], e04['pickup_lng'])

trip3_depart = arrive_office_2
d3 = haversine(office[0], office[1], e04_pos[0], e04_pos[1])
t3 = (d3 / speed) * 60
arrive_e04 = trip3_depart + t3
print(f"Trip 3: Office -> E04 (departs when Trip 2 ends)")
print(f"  E04 pickup: ({e04_pos[0]}, {e04_pos[1]})")
print(f"  Distance: {d3:.4f} km")
print(f"  Travel time: {t3:.2f} min")
print(f"  Depart Office: {min_to_time(trip3_depart)} ({trip3_depart:.2f} min)")
print(f"  Arrive E04: {min_to_time(arrive_e04)} ({arrive_e04:.2f} min)")

e04_earliest = 8*60 + 25  # 08:25
wait_e04 = max(0, e04_earliest - arrive_e04)
depart_e04 = max(arrive_e04, e04_earliest)
print(f"  E04 earliest_pickup: 08:25 (505 min)")
print(f"  Wait at E04: {wait_e04:.2f} min")
print(f"  Depart E04: {min_to_time(depart_e04)} ({depart_e04:.2f} min)")

d3_off = haversine(e04_pos[0], e04_pos[1], office[0], office[1])
t3_off = (d3_off / speed) * 60
arrive_office_3 = depart_e04 + t3_off
print(f"  E04 -> Office distance: {d3_off:.4f} km")
print(f"  E04 -> Office travel: {t3_off:.2f} min")
print(f"  Arrive Office: {min_to_time(arrive_office_3)} ({arrive_office_3:.2f} min)")
print(f"  E04 latest_drop: 09:00 (540 min) | deadline: 09:05 (545 min)")
if arrive_office_3 > 545:
    print(f"  HARD VIOLATION: {arrive_office_3:.2f} > 545 ({arrive_office_3 - 545:.2f} min late)")
elif arrive_office_3 > 540:
    print(f"  SOFT VIOLATION: {arrive_office_3:.2f} > 540 (but within 5min grace)")
else:
    print(f"  OK")
print()

# Trip 4: E10 (solo) — starts from OFFICE when Trip 3 ends
e10 = emps['E10']
e10_pos = (e10['pickup_lat'], e10['pickup_lng'])

trip4_depart = arrive_office_3
d4 = haversine(office[0], office[1], e10_pos[0], e10_pos[1])
t4 = (d4 / speed) * 60
arrive_e10 = trip4_depart + t4
print(f"Trip 4: Office -> E10 (departs when Trip 3 ends)")
print(f"  E10 pickup: ({e10_pos[0]}, {e10_pos[1]})")
print(f"  Distance: {d4:.4f} km")
print(f"  Travel time: {t4:.2f} min")
print(f"  Depart Office: {min_to_time(trip4_depart)} ({trip4_depart:.2f} min)")
print(f"  Arrive E10: {min_to_time(arrive_e10)} ({arrive_e10:.2f} min)")

e10_earliest = 8*60  # 08:00
wait_e10 = max(0, e10_earliest - arrive_e10)
depart_e10 = max(arrive_e10, e10_earliest)
print(f"  E10 earliest_pickup: 08:00 (480 min)")
print(f"  Wait at E10: {wait_e10:.2f} min")
print(f"  Depart E10: {min_to_time(depart_e10)} ({depart_e10:.2f} min)")

d4_off = haversine(e10_pos[0], e10_pos[1], office[0], office[1])
t4_off = (d4_off / speed) * 60
arrive_office_4 = depart_e10 + t4_off
print(f"  E10 -> Office distance: {d4_off:.4f} km")
print(f"  E10 -> Office travel: {t4_off:.2f} min")
print(f"  Arrive Office: {min_to_time(arrive_office_4)} ({arrive_office_4:.2f} min)")
print(f"  E10 latest_drop: 08:40 (520 min) | deadline: 08:45 (525 min)")
if arrive_office_4 > 525:
    print(f"  HARD VIOLATION: {arrive_office_4:.2f} > 525 ({arrive_office_4 - 525:.2f} min late)")
elif arrive_office_4 > 520:
    print(f"  SOFT VIOLATION: {arrive_office_4:.2f} > 520 (but within 5min grace)")
else:
    print(f"  OK")
