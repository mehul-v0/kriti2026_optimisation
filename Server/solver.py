import sys
import random
import math
from collections import defaultdict


import pandas as pd

def parse_excel_file(path):
    # Read all sheets from the excel file
    xls = pd.read_excel(path, sheet_name=None)
    out = {}
    for sheet_name, df in xls.items():
        # Clean up column names (strip whitespace)
        df.columns = df.columns.str.strip()
        # Convert to list of dicts
        # Replace NaN with None or empty string equivalent for compatibility
        data = df.where(pd.notnull(df), None).to_dict(orient='records')
        
        # Normalize sheet names to match what we expect (remove extra spaces or 'sheet' prefix if needed)
        # However, based on the txt file, expected keys are 'employees', 'vehicles', 'baseline', 'metadataa'
        # Let's clean the keys. 
        # The txt file had '#sheet employees', implying the sheet name is 'employees'.
        # Let's trust the excel sheet names are correct.
        out[sheet_name.strip()] = data
    
    # Handle the 'metadataa' vs 'metadata' possible typo if present in excel
    if 'metadata' in out and 'metadataa' not in out:
        out['metadataa'] = out['metadata']
        
    return out

def parse_test_file(path):
    sheets = {}
    cur = None
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith('#sheet'):
                cur = line.split()[1]
                sheets[cur] = []
                header = None
                continue
            if cur is None:
                continue
            # split by tabs
            parts = line.split('\t')
            if header is None:
                header = parts
                sheets[cur].append(header)
            else:
                sheets[cur].append(parts)
    # convert to list of dicts per sheet
    out = {}
    for name, rows in sheets.items():
        hdr = rows[0]
        data = []
        for r in rows[1:]:
            row = {k: v for k, v in zip(hdr, r)}
            data.append(row)
        out[name] = data
    return out


import datetime

def time_to_min(t):
    # t like '8:10' -> minutes since midnight
    if t is None or t == '':
        return None
    if isinstance(t, datetime.time):
        return t.hour * 60 + t.minute
    if isinstance(t, str):
        h, m = t.split(':')
        return int(h) * 60 + int(m)
    return t # fallback if already int or float minutes? No, usually not expected.


def haversine_km(lat1, lon1, lat2, lon2):
    R = 6371.0
    phi1 = math.radians(float(lat1))
    phi2 = math.radians(float(lat2))
    dphi = math.radians(float(lat2) - float(lat1))
    dlambda = math.radians(float(lon2) - float(lon1))
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlambda/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))


class GAPlanner:
    def __init__(self, employees, vehicles, baseline, metadata, pop_size=80, generations=200):
        self.employees = {e['employee_id']: e for e in employees}
        self.vehicles = {v['vehicle_id']: v for v in vehicles}
        self.baseline = {b['employee_id']: b for b in baseline}
        self.metadata = {m['key']: m['value'] for m in metadata}
        self.pop_size = pop_size
        self.generations = generations
        self.office_lat = list(self.employees.values())[0]['drop_lat']
        self.office_lng = list(self.employees.values())[0]['drop_lng']
        self.employee_ids = list(self.employees.keys())

    def initial_population(self):
        pop = []
        vehicle_ids = list(self.vehicles.keys())
        for _ in range(self.pop_size):
            # assign each employee to a random vehicle (may violate capacity)
            assign = {vid: [] for vid in vehicle_ids}
            for eid in self.employee_ids:
                vid = random.choice(vehicle_ids)
                assign[vid].append(eid)
            # randomize order within each vehicle
            for vid in vehicle_ids:
                random.shuffle(assign[vid])
            pop.append(assign)
        return pop

    def evaluate(self, assign):
        # compute total cost and total employee ride time
        total_cost = 0.0
        total_emp_time = 0.0
        penalty = 0.0

        baseline_total_cost = sum(float(self.baseline[e]['baseline_cost']) for e in self.employee_ids)
        baseline_total_time = sum(float(self.baseline[e]['baseline_time_min']) for e in self.employee_ids)

        for vid, e_list in assign.items():
            v = self.vehicles[vid]
            cap = int(v['capacity'])
            if len(e_list) > cap:
                penalty += 1000 * (len(e_list) - cap)
            # check sharing pref violations
            for eid in e_list:
                pref = self.employees[eid]['sharing_preference']
                if pref == 'single' and len(e_list) > 1:
                    penalty += 500
                if pref == 'double' and len(e_list) > 2:
                    penalty += 200
            # simulate route
            cur_lat = float(v['current_lat'])
            cur_lng = float(v['current_lng'])
            cur_time = time_to_min(v.get('available_from', '8:00'))
            total_dist = 0.0
            # pick ups
            onboard = []
            for eid in e_list:
                emp = self.employees[eid]
                plat = float(emp['pickup_lat'])
                plng = float(emp['pickup_lng'])
                d = haversine_km(cur_lat, cur_lng, plat, plng)
                total_dist += d
                travel_min = d / float(v['avg_speed_kmph']) * 60.0
                cur_time += travel_min
                # wait if earlier than earliest pickup
                if emp.get('earliest_pickup'):
                    ep = time_to_min(emp['earliest_pickup'])
                    if cur_time < ep:
                        cur_time = ep
                onboard.append(eid)
                cur_lat, cur_lng = plat, plng
            # go to office drop
            d = haversine_km(cur_lat, cur_lng, float(self.office_lat), float(self.office_lng))
            total_dist += d
            travel_min = d / float(v['avg_speed_kmph']) * 60.0
            cur_time += travel_min
            drop_time = cur_time
            # cost for vehicle
            total_cost += total_dist * float(v['cost_per_km'])
            # employee ride times (pickup->drop): approximate by drop_time - pickup_arrival
            # For simplicity, compute average pickup arrival times by re-simulating
            cur_lat = float(v['current_lat'])
            cur_lng = float(v['current_lng'])
            cur_time = time_to_min(v.get('available_from', '8:00'))
            for eid in e_list:
                emp = self.employees[eid]
                plat = float(emp['pickup_lat'])
                plng = float(emp['pickup_lng'])
                d = haversine_km(cur_lat, cur_lng, plat, plng)
                travel_min = d / float(v['avg_speed_kmph']) * 60.0
                cur_time += travel_min
                if emp.get('earliest_pickup'):
                    ep = time_to_min(emp['earliest_pickup'])
                    if cur_time < ep:
                        cur_time = ep
                pickup_arrival = cur_time
                # compute ride time until drop
                # remaining distance from pickup to office
                d2 = haversine_km(plat, plng, float(self.office_lat), float(self.office_lng))
                ride_min = d2 / float(v['avg_speed_kmph']) * 60.0
                emp_ride = ride_min
                total_emp_time += emp_ride
                # check latest_drop constraint
                if emp.get('latest_drop'):
                    ld = time_to_min(emp['latest_drop'])
                    if drop_time > ld + int(self.metadata.get('priority_1_max_delay_min', 0)):
                        # naive: use small extra slack parameter; big penalty
                        penalty += 1000
                cur_lat, cur_lng = plat, plng

        # normalized objective relative to baseline sums
        norm_cost = total_cost / (baseline_total_cost + 1e-6)
        norm_time = total_emp_time / (baseline_total_time + 1e-6)
        w_cost = float(self.metadata.get('objective_cost_weight', 0.6))
        w_time = float(self.metadata.get('objective_time_weight', 0.4))
        score = w_cost * norm_cost + w_time * norm_time + penalty / 10000.0
        return score, total_cost, total_emp_time, penalty

    def mutate(self, assign):
        # move a random employee to another random vehicle or shuffle order
        new = {k: v.copy() for k, v in assign.items()}
        if random.random() < 0.7:
            # reassign
            from_vid = random.choice(list(new.keys()))
            if not new[from_vid]:
                return new
            eid = random.choice(new[from_vid])
            new[from_vid].remove(eid)
            to_vid = random.choice(list(new.keys()))
            new[to_vid].append(eid)
        else:
            vid = random.choice(list(new.keys()))
            random.shuffle(new[vid])
        return new

    def crossover(self, a, b):
        # child takes some vehicles assignments from a and rest from b
        child = {}
        vids = list(self.vehicles.keys())
        cut = random.randint(1, max(1, len(vids)-1))
        left = set(random.sample(vids, cut))
        for vid in vids:
            child[vid] = a[vid].copy() if vid in left else b[vid].copy()
        # ensure all employees present exactly once: repair
        present = [eid for lst in child.values() for eid in lst]
        missing = [eid for eid in self.employee_ids if eid not in present]
        duplicates = [eid for eid in present if present.count(eid) > 1]
        # remove duplicates extra occurrences
        for eid in set(duplicates):
            count = present.count(eid)
            removed = count - 1
            for vid in vids:
                if removed <= 0:
                    break
                if eid in child[vid]:
                    child[vid].remove(eid)
                    removed -= 1
        # add missing randomly
        for eid in missing:
            vid = random.choice(vids)
            child[vid].append(eid)
        for vid in vids:
            random.shuffle(child[vid])
        return child

    def solve(self, callback=None):
        pop = self.initial_population()
        scored = []
        for p in pop:
            s = self.evaluate(p)[0]
            scored.append((s, p))

        for gen in range(self.generations):
            scored.sort(key=lambda x: x[0])
            newpop = [p for (_, p) in scored[:max(2, int(0.2 * len(scored)))]]
            # elitism kept
            while len(newpop) < self.pop_size:
                a = random.choice(scored)[1]
                b = random.choice(scored)[1]
                child = self.crossover(a, b)
                if random.random() < 0.3:
                    child = self.mutate(child)
                newpop.append(child)
            pop = newpop
            scored = [(self.evaluate(p)[0], p) for p in pop]
            
            if callback and gen % 5 == 0:
                best_gen_score, best_gen_assign = min(scored, key=lambda x: x[0])
                callback({
                    'generation': gen,
                    'score': best_gen_score,
                    'assignment': best_gen_assign
                })

        scored.sort(key=lambda x: x[0])
        best_score, best_assign = scored[0]
        score, total_cost, total_emp_time, penalty = self.evaluate(best_assign)
        return best_assign, score, total_cost, total_emp_time, penalty


def main(path):
    if path.endswith('.xlsx'):
        data = parse_excel_file(path)
    else:
        data = parse_test_file(path)
    
    employees = data.get('employees', [])
    vehicles = data.get('vehicles', [])
    baseline = data.get('baseline', [])
    # Try 'metadataa' then 'metadata'
    metadata = data.get('metadataa', data.get('metadata', []))
    
    if not employees or not vehicles:
        print('missing sheets in input (checked employees, vehicles)')
        print('Available keys:', list(data.keys()))
        return
    planner = GAPlanner(employees, vehicles, baseline, metadata, pop_size=80, generations=200)
    best_assign, score, total_cost, total_time, penalty = planner.solve()
    print('GA result:')
    print(f'Score: {score:.4f}, Total cost: {total_cost:.2f}, Total_emp_time_min: {total_time:.1f}, penalty:{penalty}')
    for vid, elist in best_assign.items():
        if elist:
            print(f'{vid}: {",".join(elist)}')


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print('Usage: python solver.py test_case_excel.txt')
    else:
        main(sys.argv[1])
