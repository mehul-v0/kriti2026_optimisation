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
    'details': [],  # Route details for visualization
    'solution_type': '',  # Multi-stage solver result type
    'route_text': '',  # Formatted route text for display
    'logs': []
}

DATA_PATH = "TestCase_TC03.xlsx"
# Load data once to keep it simple, or reload on every run
cached_data = None

@app.route('/upload', methods=['POST'])
def upload_file():
    global DATA_PATH, cached_data
    if 'file' not in request.files:
        return jsonify({'status': 'error', 'message': 'No file part'})
    file = request.files['file']
    if file.filename == '':
        return jsonify({'status': 'error', 'message': 'No selected file'})
    if file:
        filename = "uploaded_data.xlsx"
        file.save(filename)
        DATA_PATH = filename
        cached_data = None # Invalidate cache to force reload
        return jsonify({'status': 'success', 'message': 'File uploaded successfully'})

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

@app.route('/get_metadata')
def get_metadata():
    """Return metadata including cost_weight and time_weight from the uploaded Excel."""
    data = get_data()
    if not data:
        return jsonify({'error': 'No data loaded'})
    
    # Extract metadata
    meta_list = data.get('metadataa', data.get('metadata', []))
    meta = {m['key']: m['value'] for m in meta_list}
    
    # Get weights with defaults
    cost_weight = float(meta.get('objective_cost_weight', 0.6))
    time_weight = float(meta.get('objective_time_weight', 0.4))
    
    return jsonify({
        'cost_weight': cost_weight,
        'time_weight': time_weight,
        'metadata': meta
    })



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


def run_cpp_solver_generic(executable_path, cost_weight, time_weight, pop_size, generations):
    global simulation_state
    simulation_state['running'] = True
    simulation_state['logs'] = [f"Starting {executable_path}..."]
    
    data = get_data()
    if not data:
        simulation_state['running'] = False; return

    # 1. Export Data
    inp_file = "cpp_input.txt"
    export_to_cpp_input(data, inp_file, cost_weight, time_weight, pop_size, generations)
    
    # 2. Check binary
    if not executable_path or not os.path.exists(executable_path):
        simulation_state['logs'].append(f"Error: Solver binary {executable_path} not found.")
        simulation_state['running'] = False
        return

    # 3. Run
    try:
        proc = subprocess.Popen([executable_path, inp_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
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

    # Ensure GA solver is compiled
    exe_path = compile_cpp_solver()

    thread = threading.Thread(target=run_cpp_solver_generic, args=(exe_path, cost_weight, time_weight, pop_size, generations))
    thread.daemon = True
    thread.start()
    
    return jsonify({'status': 'started'})

@app.route('/start_alns', methods=['POST'])
def start_alns():
    if simulation_state['running']:
        return jsonify({'status': 'error', 'message': 'Simulation already running'})
    
    params = request.json
    cost_weight = float(params.get('cost_weight', 0.6))
    time_weight = float(params.get('time_weight', 0.4))
    pop_size = int(params.get('pop_size', 50)) 
    generations = int(params.get('generations', 100)) # Used as iterations

    # Assume solver_alns.exe is compiled manually or exists
    exe_path = 'solver_alns.exe'
    if not os.path.exists(exe_path):
         # Try compile if missing?
         try:
            subprocess.check_call(['g++', '-O3', 'solver_alns.cpp', '-o', 'solver_alns.exe'])
         except:
            return jsonify({'status': 'error', 'message': 'ALNS solver binary not found and compilation failed'})

    thread = threading.Thread(target=run_cpp_solver_generic, args=(exe_path, cost_weight, time_weight, pop_size, generations))
    thread.daemon = True
    thread.start()
    
    return jsonify({'status': 'started'})


import solver_ortools
import solver_ortools_full

@app.route('/start_ortools', methods=['POST'])
def start_ortools():
    params = request.json
    # We can run this in a thread too, OR-Tools might take a few seconds
    
    thread = threading.Thread(target=run_ortools_solver, args=(params,))
    thread.daemon = True
    thread.start()
    
    return jsonify({'status': 'started'})

def run_ortools_solver(params):
    global simulation_state
    simulation_state['running'] = True
    simulation_state['logs'] = ["Running Google OR-Tools Solver..."]
    
    data = get_data()
    if not data:
        simulation_state['running'] = False
        return

    # Update weights in metadata just for consistent passing (wrapper logic)
    # The actual solver reads from the list of dicts or we pass explicit weights
    # In solver_ortools.py we wrote strict logic.
    
    # Let's map the params to metadata dict
    meta_list = data.get('metadataa', data.get('metadata', []))
    meta = {m['key']: m['value'] for m in meta_list}
    meta['objective_cost_weight'] = params.get('cost_weight', 0.6)
    meta['objective_time_weight'] = params.get('time_weight', 0.4)
    
    try:
        result = solver_ortools.solve_ortools(
            data['employees'],
            data['vehicles'],
            data['baseline'],
            meta
        )
        
        if result:
            simulation_state['generation'] = "Final"
            simulation_state['best_score'] = result['score']
            simulation_state['best_assignment'] = result['assignment']
            simulation_state['stats'] = result['stats']
            simulation_state['logs'].append("OR-Tools Finished Successfully.")
        else:
             simulation_state['logs'].append("OR-Tools could not find a solution.")
             
    except Exception as e:
        simulation_state['logs'].append(f"OR-Tools Error: {e}")
        print(e)
        
    simulation_state['running'] = False


@app.route('/start_ortools_full', methods=['POST'])
def start_ortools_full():
    """Start the comprehensive OR-Tools solver that uses ALL input fields."""
    params = request.json
    
    thread = threading.Thread(target=run_ortools_full_solver, args=(params,))
    thread.daemon = True
    thread.start()
    
    return jsonify({'status': 'started'})

def run_ortools_full_solver(params):
    """Run the full OR-Tools solver with all constraints."""
    global simulation_state
    simulation_state['running'] = True
    simulation_state['logs'] = ["Running OR-Tools Full Solver (All Constraints)..."]
    
    data = get_data()
    if not data:
        simulation_state['running'] = False
        return

    # Build metadata dict from the data
    meta_list = data.get('metadataa', data.get('metadata', []))
    meta = {m['key']: m['value'] for m in meta_list}
    meta['objective_cost_weight'] = params.get('cost_weight', 0.6)
    meta['objective_time_weight'] = params.get('time_weight', 0.4)
    
    try:
        result = solver_ortools_full.solve_ortools_full(
            data['employees'],
            data['vehicles'],
            data['baseline'],
            meta
        )
        
        if result:
            simulation_state['generation'] = "Final"
            simulation_state['best_score'] = result['score']
            simulation_state['best_assignment'] = result['assignment']
            simulation_state['stats'] = result['stats']
            simulation_state['details'] = result.get('details', [])  # Store route details
            simulation_state['solution_type'] = result.get('solution_type', '')  # Store solution type
            simulation_state['route_text'] = result.get('route_text', '')  # Store formatted route text
            simulation_state['logs'].append("OR-Tools Full Solver Finished Successfully.")
        else:
            simulation_state['logs'].append("OR-Tools Full could not find a solution.")
             
    except Exception as e:
        simulation_state['logs'].append(f"OR-Tools Full Error: {e}")
        import traceback
        traceback.print_exc()
        
    simulation_state['running'] = False

@app.route('/start_custom_ortools', methods=['POST'])
def start_custom_ortools():
    """Start the custom C++ OR-Tools solver (PARALLEL_CHEAPEST_INSERTION + GUIDED_LOCAL_SEARCH)."""
    params = request.json
    
    thread = threading.Thread(target=run_custom_ortools_solver, args=(params,))
    thread.daemon = True
    thread.start()
    
    return jsonify({'status': 'started'})

def export_full_constraints_to_cpp(data, filepath, metadata):
    """Export data with FULL constraints matching solver_ortools_full.py"""
    with open(filepath, 'w') as f:
        # Line 1: Metadata (objective weights, priority delays)
        cost_weight = float(metadata.get('objective_cost_weight', 0.6))
        time_weight = float(metadata.get('objective_time_weight', 0.4))
        priority_delays = [
            int(metadata.get('priority_1_max_delay_min', 5)),
            int(metadata.get('priority_2_max_delay_min', 10)),
            int(metadata.get('priority_3_max_delay_min', 15)),
            int(metadata.get('priority_4_max_delay_min', 20)),
            int(metadata.get('priority_5_max_delay_min', 30))
        ]
        f.write(f"{cost_weight} {time_weight} {' '.join(map(str, priority_delays))}\n")
        
        # Line 2: Office/depot location
        emp0 = data['employees'][0]
        f.write(f"{emp0['drop_lat']} {emp0['drop_lng']}\n")
        
        # Line 3: Number of employees
        emps = data['employees']
        f.write(f"{len(emps)}\n")
        
        # Employee lines: id pickup_lat pickup_lng drop_lat drop_lng earliest_pickup latest_drop priority vehicle_pref sharing_pref service_time
        for e in emps:
            eid = str(e['employee_id'])
            pickup_lat = e['pickup_lat']
            pickup_lng = e['pickup_lng']
            drop_lat = e['drop_lat']
            drop_lng = e['drop_lng']
            
            earliest = time_to_min(e.get('earliest_pickup', '08:00'))
            latest = time_to_min(e.get('latest_drop', '18:00'))
            
            priority = int(e.get('priority', 3))
            
            # Vehicle preference: any=0, premium=1, normal=2
            vp = e.get('vehicle_preference', 'any').lower()
            veh_pref = 1 if vp == 'premium' else (2 if vp == 'normal' else 0)
            
            # Sharing preference: single=1, double=2, triple/any=3
            sp = e.get('sharing_preference', 'triple').lower()
            share_pref = 1 if sp == 'single' else (2 if sp == 'double' else 3)
            
            service_time = int(e.get('service_time_min', 2))
            
            f.write(f"{eid} {pickup_lat} {pickup_lng} {drop_lat} {drop_lng} {earliest} {latest} {priority} {veh_pref} {share_pref} {service_time}\n")
        
        # Line: Number of vehicles
        vehs = data['vehicles']
        f.write(f"{len(vehs)}\n")
        
        # Vehicle lines: id capacity speed cost_per_km start_lat start_lng available_from category
        for v in vehs:
            vid = str(v['vehicle_id'])
            capacity = int(v['capacity'])
            speed = float(v['avg_speed_kmph'])
            cost = float(v['cost_per_km'])
            start_lat = v['current_lat']
            start_lng = v['current_lng']
            avail = time_to_min(v.get('available_from', '08:00'))
            
            # Category: any=0, premium=1, normal=2
            cat = v.get('category', 'normal').lower()
            category = 1 if cat == 'premium' else (2 if cat == 'normal' else 0)
            
            f.write(f"{vid} {capacity} {speed} {cost} {start_lat} {start_lng} {avail} {category}\n")

def run_custom_ortools_solver(params):
    """Run the custom C++ VRP solver that implements OR-Tools algorithms from scratch."""
    global simulation_state
    simulation_state['running'] = True
    simulation_state['logs'] = ["Running Custom OR-Tools C++ Solver..."]
    simulation_state['details'] = []
    simulation_state['solution_type'] = ''
    simulation_state['route_text'] = ''
    
    data = get_data()
    if not data:
        simulation_state['logs'].append("Error: No data loaded")
        simulation_state['running'] = False
        return

    # Build metadata dict
    meta_list = data.get('metadataa', data.get('metadata', []))
    metadata = {m['key']: m['value'] for m in meta_list}
    metadata['objective_cost_weight'] = params.get('cost_weight', 0.6)
    metadata['objective_time_weight'] = params.get('time_weight', 0.4)

    # Export data in the FULL format with all constraints
    inp_file = "cpp_input_full.txt"
    out_file = "vrp_output.json"
    
    export_full_constraints_to_cpp(data, inp_file, metadata)
    
    # Check if binary exists
    exe_path = 'vrp_solver/vrp_solver_full.exe'
    if not os.path.exists(exe_path):
        # Try to compile
        try:
            simulation_state['logs'].append("Compiling vrp_solver_full.cpp...")
            subprocess.check_call([
                'g++', '-std=c++17', '-O3', 
                'vrp_solver/vrp_solver_full.cpp', 
                '-o', exe_path
            ])
            simulation_state['logs'].append("Compilation successful.")
        except Exception as e:
            simulation_state['logs'].append(f"Error: Could not compile VRP solver: {e}")
            simulation_state['running'] = False
            return
    
    # Run the solver with 60 second time limit
    try:
        simulation_state['logs'].append("Running solver...")
        result = subprocess.run(
            [exe_path, inp_file, out_file, '60'],
            capture_output=True, text=True, timeout=120
        )
        
        if result.returncode != 0:
            simulation_state['logs'].append(f"Solver error: {result.stderr}")
            simulation_state['running'] = False
            return
        
        simulation_state['logs'].append("Solver completed. Parsing results...")
        
        # Read JSON output
        if not os.path.exists(out_file):
            simulation_state['logs'].append("Error: Output file not generated")
            simulation_state['running'] = False
            return
        
        with open(out_file, 'r') as f:
            solution = json.load(f)
        
        # Parse solution and update state
        parse_custom_ortools_solution(solution, data)
        
        simulation_state['logs'].append("Custom OR-Tools C++ Solver finished successfully.")
        
    except subprocess.TimeoutExpired:
        simulation_state['logs'].append("Error: Solver timed out after 120 seconds")
    except Exception as e:
        simulation_state['logs'].append(f"Error running solver: {e}")
        import traceback
        traceback.print_exc()
    
    simulation_state['running'] = False

def parse_custom_ortools_solution(solution, data):
    """Parse the JSON output from the custom C++ VRP solver - matches solver_ortools_full.py format."""
    global simulation_state
    
    # The new C++ output format matches Python exactly:
    # { "assignment": {...}, "score": ..., "stats": {...}, "details": [...] }
    
    # Extract assignment (now directly available in JSON)
    assignment = {}
    if 'assignment' in solution:
        # New format: assignment is a dict mapping vehicle_id -> [employee_ids]
        for veh_id, emp_ids in solution['assignment'].items():
            for emp_id in emp_ids:
                assignment[str(emp_id)] = str(veh_id)
    
    # Extract stats directly from JSON
    if 'stats' in solution:
        stats = solution['stats']
    else:
        # Fallback for old format
        stats = {
            'cost': solution.get('total_cost', 0),
            'time': solution.get('total_time', 0),
            'penalty': solution.get('penalty', 0),
            'hard_violations': solution.get('hard_violations', 0),
            'soft_violations': solution.get('soft_violations', 0)
        }
    
    # Use score directly if available
    score = solution.get('score', 0)
    
    # Build route text for display
    route_text = f"\n=== Custom OR-Tools C++ Solver Results ===\n"
    route_text += f"Total Cost: ${stats['cost']:.2f}\n"
    route_text += f"Hard Violations: {stats['hard_violations']}\n"
    route_text += f"Soft Violations: {stats['soft_violations']}\n\n"
    
    # The 'details' field from C++ solver matches solver_ortools_full.py format exactly
    details = solution.get('details', [])
    
    # If assignment wasn't in new format, build from details (backward compatibility)
    if not assignment:
        for vehicle_data in details:
            veh_id = vehicle_data.get('vehicle', vehicle_data.get('vehicle_id', 'V01'))
            for employee_id in vehicle_data.get('employees', []):
                assignment[str(employee_id)] = str(veh_id)
    
    # Build route text from details
    for vehicle_data in details:
        veh_id = vehicle_data.get('vehicle', vehicle_data.get('vehicle_id', 'V01'))
        num_trips = vehicle_data.get('num_trips', 0)
        
        if num_trips > 0:
            route_text += f"\n=== Vehicle {veh_id} ===\n"
            route_text += f"Trips: {num_trips}\n"
            route_text += f"Cost: ${vehicle_data.get('cost', 0):.2f}\n"
            route_text += f"Total Employees: {len(vehicle_data.get('employees', []))}\n"
            
            for trip_data in vehicle_data.get('trip_routes', []):
                trip_num = trip_data.get('trip_number', 1)
                route_text += f"\n  Trip {trip_num}:\n"
                route_text += f"    Distance: {trip_data.get('distance_km', 0):.2f} km\n"
                route_text += f"    Cost: ${trip_data.get('cost', 0):.2f}\n"
                route_text += f"    Employees: {len(trip_data.get('employees', []))}\n"
                
                # Build route sequence
                route_text += f"    Route: "
                stops = trip_data.get('detailed_stops', [])
                route_sequence = []
                for stop in stops:
                    if stop.get('type') == 'depot':
                        route_sequence.append('Depot')
                    elif stop.get('type') == 'employee':
                        route_sequence.append(stop.get('label', 'Employee'))
                    elif stop.get('type') == 'office' or stop.get('type') == 'office_start':
                        route_sequence.append('Office')
                route_text += " -> ".join(route_sequence) + "\n"
    
    # Determine solution type based on violations
    if stats['hard_violations'] == 0 and stats['soft_violations'] == 0:
        solution_type = 'OPTIMAL - No violations (Custom C++)'
    elif stats['hard_violations'] == 0:
        solution_type = f'FEASIBLE - {stats["soft_violations"]} soft violations only (Custom C++)'
    else:
        solution_type = f'BEST AVAILABLE - {stats["hard_violations"]} hard, {stats["soft_violations"]} soft violations (Custom C++)'
    
    simulation_state['generation'] = "Final"
    simulation_state['best_score'] = round(score, 4) if score else 0
    simulation_state['best_assignment'] = assignment
    simulation_state['stats'] = stats
    simulation_state['details'] = details  # Pass through details exactly as received
    simulation_state['solution_type'] = solution_type
    simulation_state['route_text'] = route_text

@app.route('/status')
def status():
    # Return current state with detailed route information
    
    data = get_data()
    # Helper to serialize for JSON
    
    return jsonify({
        'running': simulation_state['running'],
        'generation': simulation_state['generation'],
        'score': simulation_state['best_score'],
        'stats': simulation_state['stats'],
        'assignment': simulation_state['best_assignment'],
        'details': simulation_state.get('details', []),  # Include route details for pathway visualization
        'solution_type': simulation_state.get('solution_type', ''),  # Include solution type for multi-stage solver
        'route_text': simulation_state.get('route_text', ''),  # Include formatted route text
        # Send partial data needed for visualization
        'employees': {e['employee_id']: {'lat': e['drop_lat'], 'lng': e['drop_lng'], 'olat': e['pickup_lat'], 'olng': e['pickup_lng']} for e in data.get('employees', [])} if data else {},
        'vehicles': {v['vehicle_id']: {'lat': v['current_lat'], 'lng': v['current_lng']} for v in data.get('vehicles', [])} if data else {},
        'office': {'lat': data.get('employees', [])[0]['drop_lat'], 'lng': data.get('employees', [])[0]['drop_lng']} if data and data.get('employees') else {}
    })

if __name__ == '__main__':
    app.run(debug=True, port=5000)