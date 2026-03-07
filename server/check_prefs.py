import json, re

with open('output/input_tc01.json') as f:
    inp = json.load(f)
with open('output/output_tc01_nopref.json') as f:
    out = json.load(f)

# Vehicle category map
veh_categories = {}
for v in inp['vehicles']:
    veh_categories[v['vehicle_id']] = v.get('category', '?')

# Employee preferences
emp_prefs = {}
for emp in inp['employees']:
    emp_prefs[emp['employee_id']] = emp.get('vehicle_preference', 'any')

print("=== VEHICLES ===")
for vid, cat in veh_categories.items():
    print(f"  {vid}: category={cat}")

print("\n=== EMPLOYEE PREFERENCES ===")
for eid, pref in emp_prefs.items():
    print(f"  {eid} (P{next(e['priority'] for e in inp['employees'] if e['employee_id']==eid)}): wants={pref}")

# Parse output: extract employee IDs from stop locations
print("\n=== ASSIGNMENTS (from output) ===")
emp_to_veh = {}
for veh in out.get('vehicles', []):
    vid = veh['vehicle_id']
    for trip in veh.get('trips', []):
        for stop in trip.get('stops', []):
            loc = stop.get('location', '')
            # Match patterns like "E01 Pickup"
            m = re.match(r'(E\d+)\s+Pickup', loc)
            if m:
                eid = m.group(1)
                emp_to_veh[eid] = vid

for eid, vid in sorted(emp_to_veh.items()):
    print(f"  {eid} -> {vid} ({veh_categories.get(vid, '?')})")

# Check violations
print("\n=== PREFERENCE VIOLATION CHECK ===")
violations = 0
for eid, pref in sorted(emp_prefs.items()):
    assigned_vid = emp_to_veh.get(eid, 'UNASSIGNED')
    assigned_cat = veh_categories.get(assigned_vid, '?')
    
    if pref.lower() == 'any' or pref.lower() == 'none':
        print(f"  {eid}: pref=any, assigned={assigned_vid}({assigned_cat}) -> OK (no preference)")
    elif pref.lower() == assigned_cat.lower():
        print(f"  {eid}: pref={pref}, assigned={assigned_vid}({assigned_cat}) -> OK (matched)")
    else:
        violations += 1
        print(f"  {eid}: pref={pref}, assigned={assigned_vid}({assigned_cat}) -> VIOLATED!")

print(f"\nTotal preference violations: {violations}/{len(emp_prefs)}")
