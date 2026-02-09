from flask import Flask, request, jsonify, send_file
from flask_cors import CORS
import os
import subprocess
import json
import math
import re
from werkzeug.utils import secure_filename
import time

app = Flask(__name__)
CORS(app)

# Configuration
UPLOAD_FOLDER = 'uploads'
OUTPUT_FOLDER = 'output'
ALLOWED_EXTENSIONS = {'xlsx', 'xls'}

os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(OUTPUT_FOLDER, exist_ok=True)

# Server state: track current upload for the optimize call
_state = {
    'input_json': None,       # path to input JSON after conversion
    'output_json': None,      # path to solver output JSON
    'excel_filename': None,   # original uploaded filename
    'input_data': None,       # parsed input data (employees, vehicles, baseline, metadata)
}

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


# ---------------------------------------------------------------------------
#  Helpers
# ---------------------------------------------------------------------------

def _time_str_to_minutes(t):
    """Convert '08:15' or '08:15:00' to minutes since midnight."""
    parts = str(t).strip().split(':')
    h = int(parts[0])
    m = int(parts[1]) if len(parts) > 1 else 0
    return h * 60 + m


def _compute_digest(employees, vehicles):
    """Build a summary digest from employee/vehicle lists."""
    emp_count = len(employees)
    veh_count = len(vehicles)

    # Time window span
    if emp_count > 0:
        earliest = min(_time_str_to_minutes(e['earliest_pickup']) for e in employees)
        latest = max(_time_str_to_minutes(e['latest_drop']) for e in employees)
        eh, em = divmod(earliest, 60)
        lh, lm = divmod(latest, 60)
        time_window_span = f"{eh:02d}:{em:02d} - {lh:02d}:{lm:02d}"
    else:
        time_window_span = "N/A"

    # High priority %
    high_priority = sum(1 for e in employees if int(e.get('priority', 3)) <= 2)
    high_priority_percent = round(high_priority / emp_count * 100) if emp_count > 0 else 0

    # Fleet composition by category
    fleet = {'electric': 0, 'petrol': 0, 'diesel': 0}
    modes = {'2-wheeler': 0, '4-wheeler': 0, 'van': 0}
    for v in vehicles:
        cat = str(v.get('category', 'normal')).lower()
        if cat == 'electric':
            fleet['electric'] += 1
        elif cat == 'premium':
            fleet['diesel'] += 1
        else:
            fleet['petrol'] += 1
        cap = int(v.get('capacity', 4))
        if cap <= 2:
            modes['2-wheeler'] += 1
        elif cap <= 4:
            modes['4-wheeler'] += 1
        else:
            modes['van'] += 1

    return {
        'employees_count': emp_count,
        'vehicles_count': veh_count,
        'time_window_span': time_window_span,
        'high_priority_percent': high_priority_percent,
        'fleet_composition': fleet,
        'vehicle_modes': modes,
    }


def _compute_baseline_cost(baseline_list):
    """Sum baseline costs for all employees."""
    return sum(float(b.get('baseline_cost', 0)) for b in baseline_list)


def _haversine(lat1, lng1, lat2, lng2):
    """Haversine distance in km."""
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlng = math.radians(lng2 - lng1)
    a = math.sin(dlat / 2) ** 2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlng / 2) ** 2
    return R * 2 * math.asin(math.sqrt(a))


def _extract_employee_id(location_str):
    """Extract employee ID from location string like 'E01 Pickup' or 'E11 Pickup'."""
    m = re.match(r'^(E\d+)\s', location_str)
    return m.group(1) if m else None


def _transform_solver_output(solver_output, input_data):
    """
    Transform the C++ solver output into the shape the frontend expects:
      - routes:  BackendRoute[]
      - assignments:  BackendAssignment[]
      - result:  summary object
      - violation_details:  constraint violation details
    """
    employees = input_data.get('employees', [])
    vehicles_input = input_data.get('vehicles', [])
    baseline_list = input_data.get('baseline', [])
    metadata = input_data.get('metadata', {})

    baseline_cost = _compute_baseline_cost(baseline_list)
    total_cost = float(solver_output.get('cost', 0))
    cost_savings = baseline_cost - total_cost
    cost_savings_percent = round(cost_savings / baseline_cost * 100, 2) if baseline_cost > 0 else 0

    solver_vehicles = solver_output.get('vehicles', [])

    # Build employee lookup (for lat/lng of pickups)
    emp_lookup = {e['employee_id']: e for e in employees}
    veh_lookup = {v['vehicle_id']: v for v in vehicles_input}

    # Office location: use drop_lat/drop_lng from first employee (all employees share the same office)
    office_lat = employees[0]['drop_lat'] if employees else 0
    office_lng = employees[0]['drop_lng'] if employees else 0

    routes = []
    assignments = []
    assigned_employee_ids = set()
    total_distance = 0
    total_time = 0
    seq_counter = 0

    # For violation checking
    capacity_violations = []
    time_window_violations = []
    vehicle_pref_violations = []
    sharing_pref_violations = []

    for sv in solver_vehicles:
        vid = sv['vehicle_id']
        veh_info = veh_lookup.get(vid, {})
        veh_capacity = int(veh_info.get('capacity', 4))
        veh_category = str(veh_info.get('category', 'normal')).lower()

        route_points = []
        trips_count = len(sv.get('trips', []))
        veh_total_distance = float(sv.get('total_distance', 0))
        veh_total_cost = float(sv.get('total_cost', 0))
        passengers_in_vehicle = set()

        for trip in sv.get('trips', []):
            trip_num = trip['trip_number']
            stops = trip.get('stops', [])
            passengers_in_trip = []

            for stop in stops:
                loc = stop.get('location', '')
                emp_id = _extract_employee_id(loc)

                if 'Pickup' in loc and emp_id:
                    emp = emp_lookup.get(emp_id, {})
                    route_points.append({
                        'lat': emp.get('pickup_lat', 0),
                        'lng': emp.get('pickup_lng', 0),
                        'type': 'pickup',
                        'employee_id': emp_id,
                        'trip_number': trip_num,
                        'arrival_time': stop.get('arrival_time', ''),
                        'departure_time': stop.get('departure_time', ''),
                        'distance_from_prev': float(stop.get('distance_from_prev', 0)),
                    })
                    passengers_in_trip.append(emp_id)
                    passengers_in_vehicle.add(emp_id)
                    assigned_employee_ids.add(emp_id)

                    seq_counter += 1
                    assignments.append({
                        'vehicle_id': vid,
                        'employee_id': emp_id,
                        'pickup_time': stop.get('departure_time', stop.get('arrival_time', '')),
                        'dropoff_time': '',
                        'sequence_order': seq_counter,
                        'is_pickup': True,
                        'trip_number': trip_num,
                    })

                elif 'Drop' in loc or 'Office' in loc:
                    route_points.append({
                        'lat': office_lat,
                        'lng': office_lng,
                        'type': 'office',
                        'employee_id': None,
                        'trip_number': trip_num,
                        'arrival_time': stop.get('arrival_time', ''),
                        'departure_time': stop.get('departure_time', ''),
                        'distance_from_prev': float(stop.get('distance_from_prev', 0)),
                    })

                    # Create dropoff assignments for all passengers in this trip
                    for pid in passengers_in_trip:
                        # Find the matching pickup assignment and fill dropoff_time
                        for a in reversed(assignments):
                            if a['employee_id'] == pid and a['vehicle_id'] == vid and a['trip_number'] == trip_num and a['is_pickup']:
                                a['dropoff_time'] = stop.get('arrival_time', '')
                                break
                        # Also add a dropoff assignment entry
                        seq_counter += 1
                        assignments.append({
                            'vehicle_id': vid,
                            'employee_id': pid,
                            'pickup_time': '',
                            'dropoff_time': stop.get('arrival_time', ''),
                            'sequence_order': seq_counter,
                            'is_pickup': False,
                            'trip_number': trip_num,
                        })

            # --- Constraint checking per trip ---
            # Capacity check
            if len(passengers_in_trip) > veh_capacity:
                capacity_violations.append({
                    'vehicle': vid,
                    'trip': trip_num,
                    'passengers': len(passengers_in_trip),
                    'capacity': veh_capacity,
                    'employees': ', '.join(passengers_in_trip),
                })

            # Time window check (office arrival vs employee latest_drop)
            office_arrival = None
            for stop in reversed(stops):
                if 'Drop' in stop.get('location', '') or 'Office' in stop.get('location', ''):
                    office_arrival = stop.get('arrival_time', '')
                    break
            if office_arrival:
                for pid in passengers_in_trip:
                    emp = emp_lookup.get(pid, {})
                    deadline = emp.get('latest_drop', '23:59')
                    oa_min = _time_str_to_minutes(office_arrival)
                    dl_min = _time_str_to_minutes(deadline)
                    if oa_min > dl_min:
                        time_window_violations.append({
                            'employee': pid,
                            'vehicle': vid,
                            'trip': trip_num,
                            'office_arrival': office_arrival,
                            'deadline': deadline,
                            'delay_min': oa_min - dl_min,
                        })

            # Sharing preference check
            sharing_map = {'single': 1, 'double': 2, 'triple': 3}
            for pid in passengers_in_trip:
                emp = emp_lookup.get(pid, {})
                spref = str(emp.get('sharing_preference', 'triple')).lower()
                max_riders = sharing_map.get(spref, 3)
                if len(passengers_in_trip) > max_riders:
                    sharing_pref_violations.append({
                        'employee': pid,
                        'vehicle': vid,
                        'trip': trip_num,
                        'preferred': spref.capitalize(),
                        'actual_riders': len(passengers_in_trip),
                    })

            # Vehicle preference check
            for pid in passengers_in_trip:
                emp = emp_lookup.get(pid, {})
                vpref = str(emp.get('vehicle_preference', 'any')).lower()
                if vpref != 'any':
                    if vpref != veh_category:
                        assigned_label = 'Diesel' if veh_category == 'premium' else ('Electric' if veh_category == 'electric' else 'Petrol')
                        preferred_label = 'Diesel' if vpref == 'premium' else ('Electric' if vpref == 'electric' else 'Petrol')
                        vehicle_pref_violations.append({
                            'employee': pid,
                            'vehicle': vid,
                            'preferred': preferred_label,
                            'assigned': assigned_label,
                        })

        total_distance += veh_total_distance
        total_time += float(sv.get('total_time', 0))

        cap_util = round(len(passengers_in_vehicle) / veh_capacity * 100, 1) if veh_capacity > 0 else 0

        routes.append({
            'vehicle_id': vid,
            'route_points': route_points,
            'total_distance': veh_total_distance,
            'total_cost': veh_total_cost,
            'passengers_count': len(passengers_in_vehicle),
            'capacity_utilization': cap_util,
            'trips_count': trips_count,
        })

    # Unassigned employees
    unassigned = []
    for emp in employees:
        if emp['employee_id'] not in assigned_employee_ids:
            unassigned.append({'employee': emp['employee_id']})

    hard_violations = len(capacity_violations) + len(time_window_violations) + len(unassigned)
    soft_violations = len(vehicle_pref_violations) + len(sharing_pref_violations)

    violation_details = {
        'capacity_violations': capacity_violations,
        'time_window_violations': time_window_violations,
        'unassigned_employees': unassigned,
        'vehicle_pref_violations': vehicle_pref_violations,
        'sharing_pref_violations': sharing_pref_violations,
    }

    result_summary = {
        'total_cost': total_cost,
        'baseline_cost': baseline_cost,
        'cost_savings': round(cost_savings, 2),
        'cost_savings_percent': cost_savings_percent,
        'total_distance': round(total_distance, 2),
        'total_time': round(total_time, 2),
        'vehicles_used': len(solver_vehicles),
        'vehicles_available': len(vehicles_input),
        'hard_violations': hard_violations,
        'soft_violations': soft_violations,
    }

    return {
        'success': True,
        'result': result_summary,
        'routes': routes,
        'assignments': assignments,
        'violation_details': violation_details,
    }


# ---------------------------------------------------------------------------
#  Routes
# ---------------------------------------------------------------------------

@app.route('/')
def index():
    return jsonify({'message': 'VRP Solver Backend Server', 'version': '2.0'})


@app.route('/api/health')
def health():
    return jsonify({'status': 'ok'})


@app.route('/api/upload', methods=['POST'])
def upload_file():
    try:
        if 'file' not in request.files:
            return jsonify({'success': False, 'error': 'No file uploaded'}), 400

        file = request.files['file']
        if file.filename == '':
            return jsonify({'success': False, 'error': 'No file selected'}), 400

        if not file or not allowed_file(file.filename):
            return jsonify({'success': False, 'error': 'Invalid file type. Only .xlsx/.xls allowed'}), 400

        filename = secure_filename(file.filename)
        timestamp = str(int(time.time()))
        saved_name = f"{timestamp}_{filename}"
        filepath = os.path.join(UPLOAD_FOLDER, saved_name)
        file.save(filepath)

        # Convert Excel → JSON
        input_json = os.path.join(OUTPUT_FOLDER, f'input_{timestamp}.json')
        try:
            result = subprocess.run(
                ['python', 'convert_excel_to_json.py', filepath, input_json],
                capture_output=True, text=True, timeout=30,
            )
            if result.returncode != 0:
                return jsonify({'success': False, 'error': f'Excel conversion failed: {result.stderr}'}), 500
        except subprocess.TimeoutExpired:
            return jsonify({'success': False, 'error': 'Excel conversion timed out'}), 500

        # Parse the converted JSON
        with open(input_json, 'r') as f:
            input_data = json.load(f)

        employees = input_data.get('employees', [])
        vehicles = input_data.get('vehicles', [])
        baseline_list = input_data.get('baseline', [])
        baseline_cost = _compute_baseline_cost(baseline_list)
        digest = _compute_digest(employees, vehicles)

        # Store state for optimize call
        _state['input_json'] = input_json
        _state['excel_filename'] = saved_name
        _state['input_data'] = input_data

        return jsonify({
            'success': True,
            'message': f'Parsed {len(employees)} employees and {len(vehicles)} vehicles',
            'filename': saved_name,
            'digest': digest,
            'employees': employees,
            'vehicles': vehicles,
            'baseline_cost': baseline_cost,
        })

    except Exception as e:
        return jsonify({'success': False, 'error': f'Upload failed: {str(e)}'}), 500


@app.route('/api/optimize', methods=['POST'])
def optimize():
    try:
        if not _state.get('input_json') or not os.path.exists(_state['input_json']):
            return jsonify({'success': False, 'error': 'No data uploaded yet. Please upload an Excel file first.'}), 400

        input_json = _state['input_json']
        input_data = _state['input_data']

        # Read optional solver parameters from request
        body = request.get_json(silent=True) or {}
        solver_duration = int(body.get('solverDurationSeconds', 23))

        timestamp = str(int(time.time()))
        output_json = os.path.join(OUTPUT_FOLDER, f'output_{timestamp}.json')

        # Ensure solver executable exists
        solver_executable = 'vrp_solver_custom.exe' if os.name == 'nt' else './vrp_solver_custom'
        if not os.path.exists(solver_executable):
            if os.name == 'nt':
                build_result = subprocess.run(['build.bat'], capture_output=True, text=True, timeout=120, shell=True)
            else:
                build_result = subprocess.run(['make'], capture_output=True, text=True, timeout=120)
            if build_result.returncode != 0:
                return jsonify({'success': False, 'error': f'Solver build failed: {build_result.stderr}'}), 500

        # Run solver
        timeout_sec = max(solver_duration + 30, 60)  # add buffer
        try:
            solver_result = subprocess.run(
                [solver_executable, input_json, output_json, str(solver_duration)],
                capture_output=True, text=True, timeout=timeout_sec,
            )
            if solver_result.returncode != 0:
                return jsonify({
                    'success': False,
                    'error': f'Solver failed: {solver_result.stderr}',
                }), 500
        except subprocess.TimeoutExpired:
            return jsonify({'success': False, 'error': f'Solver timed out after {timeout_sec}s'}), 500

        # Read solver output
        if not os.path.exists(output_json):
            return jsonify({'success': False, 'error': 'Solver did not produce output'}), 500

        with open(output_json, 'r') as f:
            solver_output = json.load(f)

        # Store for download
        _state['output_json'] = output_json

        # Transform to frontend shape
        response = _transform_solver_output(solver_output, input_data)
        return jsonify(response)

    except Exception as e:
        return jsonify({'success': False, 'error': f'Optimization failed: {str(e)}'}), 500


@app.route('/api/download-solution')
def download_solution():
    output = _state.get('output_json')
    if output and os.path.exists(output):
        return send_file(output, as_attachment=True, download_name='solution.json')
    return jsonify({'error': 'No solution available'}), 404


@app.route('/api/results/<filename>')
def get_results(filename):
    filepath = os.path.join(OUTPUT_FOLDER, filename)
    if os.path.exists(filepath):
        return send_file(filepath)
    return jsonify({'error': 'File not found'}), 404


@app.route('/api/list-results')
def list_results():
    try:
        files = []
        for filename in os.listdir(OUTPUT_FOLDER):
            if filename.endswith('.json'):
                filepath = os.path.join(OUTPUT_FOLDER, filename)
                stat = os.stat(filepath)
                files.append({
                    'filename': filename,
                    'size': stat.st_size,
                    'created': stat.st_ctime,
                    'modified': stat.st_mtime,
                })
        files.sort(key=lambda x: x['created'], reverse=True)
        return jsonify({'files': files})
    except Exception as e:
        return jsonify({'error': str(e)}), 500


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 5000))
    print(f"\n  VRP Solver Backend — http://localhost:{port}")
    print(f"  Upload folder : {UPLOAD_FOLDER}")
    print(f"  Output folder : {OUTPUT_FOLDER}\n")
    app.run(host='0.0.0.0', port=port, debug=True)
