from flask import Flask, render_template, request, jsonify, Response
import json
import threading
import time
import pandas as pd
from solver import GAPlanner, parse_excel_file

app = Flask(__name__)

# Global state to store current simulation status
simulation_state = {
    'running': False,
    'generation': 0,
    'best_score': 0,
    'best_assignment': {},
    'stats': {},
    'logs': []
}

DATA_PATH = "TestCase_TC03.xlsx"
# Load data once to keep it simple, or reload on every run
cached_data = None

def get_data():
    global cached_data
    if cached_data is None:
        try:
            cached_data = parse_excel_file(DATA_PATH)
        except Exception as e:
            print(f"Error loading data: {e}")
            return None
    return cached_data

@app.route('/')
def index():
    return render_template('index.html')

import subprocess
import os

def time_to_min(t):
    # Same helper to format times for C++ input
    if t is None or t == '':
        return -1
    if hasattr(t, 'hour'):
        return t.hour * 60 + t.minute
    if isinstance(t, str) and ':' in t:
        h, m = t.split(':')
        return int(h) * 60 + int(m)
    return -1

def export_to_cpp_input(data, filepath, cost_weight, time_weight, pop_size, generations):
    with open(filepath, 'w') as f:
        # Header: weights, pop, gens
        f.write(f"{cost_weight} {time_weight} {pop_size} {generations}\n")
        
        # Office location (from first employee drop)
        emp0 = data['employees'][0]
        f.write(f"{emp0['drop_lat']} {emp0['drop_lng']}\n")

        # Map baseline data for lookup
        baseline_map = {str(b['employee_id']): b for b in data.get('baseline', [])}

        # Employees
        emps = data['employees']
        f.write(f"{len(emps)}\n")
        for e in emps:
            # Format: id pick_lat pick_lng earliest latest sharing base_cost base_time
            # sharing map: single->1, double->2, triple->3, any->0
            smap = {'single': 1, 'double': 2, 'triple': 3}
            spref = smap.get(e['sharing_preference'], 0)
            
            e_pick = time_to_min(e.get('earliest_pickup')) if e.get('earliest_pickup') else -1
            l_drop = time_to_min(e.get('latest_drop')) if e.get('latest_drop') else -1
            
            eid = str(e['employee_id'])
            b_info = baseline_map.get(eid, {})
            base_cost = b_info.get('baseline_cost', 0)
            base_time = b_info.get('baseline_time_min', b_info.get('baseline_time', 0))

            f.write(f"{eid} {e['pickup_lat']} {e['pickup_lng']} {e_pick} {l_drop} {spref} {base_cost} {base_time}\n")
        
        # Vehicles
        vehs = data['vehicles']
        f.write(f"{len(vehs)}\n")
        for v in vehs:
             # id cap speed cost lat lng avail
             avail = time_to_min(v.get('available_from')) if v.get('available_from') else 480 # 8:00 default
             f.write(f"{v['vehicle_id']} {v['capacity']} {v['avg_speed_kmph']} {v['cost_per_km']} {v['current_lat']} {v['current_lng']} {avail}\n")


def compile_cpp_solver():
    # Try multiple compilers
    compilers = ['g++', 'clang++', 'cl']
    src = 'solver.cpp'
    out = 'solver.exe' if os.name == 'nt' else './solver'
    
    # Check if already compiled?
    if os.path.exists(out):
        # Optional: check modified time?
        pass

    # Try simple g++
    try:
        subprocess.check_call(['g++', '-O3', src, '-o', out])
        print("Compiled with g++")
        return out
    except:
        pass

    # Try cl (MSVC)
    try:
        # cl needs special environment, usually run from dev prompt. 
        # But we can try just 'cl' if it's in path.
        subprocess.check_call(['cl', '/EHsc', '/O2', src])
        print("Compiled with cl")
        return 'solver.exe'
    except:
        pass
        
    print("Could not compile C++ solver automatically. Please compile 'solver.cpp' to 'solver.exe' manually.")
    return None

def run_solver(cost_weight, time_weight, pop_size, generations):
    global simulation_state
    simulation_state['running'] = True
    simulation_state['logs'] = []
    
    data = get_data()
    if not data:
        simulation_state['running'] = False; return

    # 1. Export Data
    inp_file = "cpp_input.txt"
    export_to_cpp_input(data, inp_file, cost_weight, time_weight, pop_size, generations)
    
    # 2. Compile/Check binary
    exe_path = compile_cpp_solver()
    if not exe_path or not os.path.exists(exe_path):
        simulation_state['logs'].append("Error: C++ Solver binary not found. Compilation failed or compiler missing.")
        # Fallback to python? Or just stop. User asked for C++.
        simulation_state['running'] = False
        return

    # 3. Run
    try:
        proc = subprocess.Popen([exe_path, inp_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        for line in proc.stdout:
            line = line.strip()
            if not line: continue
            try:
                # Parse JSON updates
                import json
                info = json.loads(line)
                simulation_state['generation'] = info['generation']
                simulation_state['best_score'] = info['score']
                simulation_state['best_assignment'] = info['assignment']
                simulation_state['stats'] = info['stats']
            except Exception as e:
                print("Parse error from C++:", line, e)
                
        proc.wait()
        simulation_state['logs'].append("C++ Simulation finished.")
    except Exception as e:
        simulation_state['logs'].append(f"Error running solver: {e}")
    
    simulation_state['running'] = False


@app.route('/start', methods=['POST'])
def start_simulation():
    if simulation_state['running']:
        return jsonify({'status': 'error', 'message': 'Simulation already running'})
    
    params = request.json
    cost_weight = float(params.get('cost_weight', 0.6))
    time_weight = float(params.get('time_weight', 0.4))
    pop_size = int(params.get('pop_size', 50))
    generations = int(params.get('generations', 100))

    thread = threading.Thread(target=run_solver, args=(cost_weight, time_weight, pop_size, generations))
    thread.daemon = True
    thread.start()
    
    return jsonify({'status': 'started'})

@app.route('/status')
def status():
    # Return current state
    # We also need to return the vehicle paths for the map if possible
    # For now, let's just return assignments and stats
    
    # To facilitate map: calculate routes (lat/lng sequence) for the current assignment
    # This might be heavy to do every poll. Let's do it on the client side or lightweight here.
    # We have lat/lngs in employees and vehicles.
    # Let's send the raw assignment + data (vehicles/employees) so client can draw lines.
    
    data = get_data()
    # Helper to serialize for JSON
    
    return jsonify({
        'running': simulation_state['running'],
        'generation': simulation_state['generation'],
        'score': simulation_state['best_score'],
        'stats': simulation_state['stats'],
        'assignment': simulation_state['best_assignment'],
        # Send partial data needed for visualization
        'employees': {e['employee_id']: {'lat': e['drop_lat'], 'lng': e['drop_lng'], 'olat': e['pickup_lat'], 'olng': e['pickup_lng']} for e in data.get('employees', [])} if data else {},
        'vehicles': {v['vehicle_id']: {'lat': v['current_lat'], 'lng': v['current_lng']} for v in data.get('vehicles', [])} if data else {},
        'office': {'lat': data.get('employees', [])[0]['drop_lat'], 'lng': data.get('employees', [])[0]['drop_lng']} if data and data.get('employees') else {}
    })

if __name__ == '__main__':
    app.run(debug=True, port=5000)
