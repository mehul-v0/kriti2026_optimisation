"""Flask Backend for VRP Solver"""

from flask import Flask, request, jsonify, send_file, send_from_directory
from flask_cors import CORS
import os
import json
import subprocess
import time
from datetime import datetime
from werkzeug.utils import secure_filename
from convert_excel_to_json import convert

app = Flask(__name__, static_folder='frontend/dist', static_url_path='')
CORS(app, resources={r"/api/*": {"origins": "*"}})

# Configuration
UPLOAD_FOLDER = 'uploads'
OUTPUT_FOLDER = 'output'
ALLOWED_EXTENSIONS = {'xlsx', 'xls'}

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max file size

# Create directories if they don't exist
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
os.makedirs(OUTPUT_FOLDER, exist_ok=True)

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

def calculate_data_digest(data):
    """Calculate data digest from input JSON"""
    employees = data.get('employees', [])
    vehicles = data.get('vehicles', [])
    
    # Count high priority employees
    high_priority = sum(1 for e in employees if e.get('priority', 3) == 1)
    high_priority_percent = (high_priority / len(employees) * 100) if employees else 0
    
    # Fleet composition
    fleet_composition = {
        'electric': sum(1 for v in vehicles if v.get('category', '').lower() == 'electric'),
        'petrol': sum(1 for v in vehicles if v.get('category', '').lower() == 'normal'),
        'diesel': sum(1 for v in vehicles if v.get('category', '').lower() == 'premium')
    }
    
    # Time window span
    if employees:
        earliest = min(e.get('earliest_pickup', '08:00') for e in employees)
        latest = max(e.get('latest_drop', '18:00') for e in employees)
        time_window_span = f"{earliest} - {latest}"
    else:
        time_window_span = "N/A"
    
    return {
        'employees_count': len(employees),
        'vehicles_count': len(vehicles),
        'time_window_span': time_window_span,
        'high_priority_percent': high_priority_percent,
        'fleet_composition': fleet_composition,
        'vehicle_modes': {
            '2-wheeler': 0,
            '4-wheeler': sum(1 for v in vehicles if v.get('capacity', 4) <= 4),
            'van': sum(1 for v in vehicles if v.get('capacity', 4) > 4)
        }
    }

@app.route('/api/health', methods=['GET'])
def health_check():
    return jsonify({'status': 'ok', 'message': 'Backend is running'})

@app.route('/api/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400
    
    file = request.files['file']
    
    if file.filename == '':
        return jsonify({'error': 'No file selected'}), 400
    
    if not allowed_file(file.filename):
        return jsonify({'error': 'Invalid file type. Please upload an Excel file (.xlsx or .xls)'}), 400
    
    try:
        # Save uploaded file
        filename = secure_filename(file.filename)
        filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        file.save(filepath)
        
        # Convert to JSON
        input_json = os.path.join(OUTPUT_FOLDER, 'input.json')
        success = convert(filepath, input_json)
        
        if not success:
            return jsonify({'error': 'Failed to convert Excel file to JSON'}), 500
        
        # Load and return the converted data for preview
        with open(input_json, 'r') as f:
            data = json.load(f)
        
        # Calculate digest
        digest = calculate_data_digest(data)
        
        # Calculate baseline cost
        baseline_data = data.get('baseline', [])
        baseline_cost = sum(b.get('baseline_cost', 0) for b in baseline_data)
        
        return jsonify({
            'success': True,
            'message': 'File uploaded and converted successfully',
            'filename': filename,
            'digest': digest,
            'employees': data.get('employees', []),
            'vehicles': data.get('vehicles', []),
            'baseline_cost': baseline_cost
        })
    
    except Exception as e:
        return jsonify({'error': f'Error processing file: {str(e)}'}), 500

@app.route('/api/optimize', methods=['POST'])
def run_optimization():
    try:
        input_json = os.path.join(OUTPUT_FOLDER, 'input.json')
        output_json = os.path.join(OUTPUT_FOLDER, 'solution.json')
        
        if not os.path.exists(input_json):
            return jsonify({'error': 'No input data found. Please upload a file first.'}), 400
        
        # Run the solver
        solver_exe = 'vrp_solver_custom.exe' if os.name == 'nt' else './vrp_solver_custom'
        
        if not os.path.exists(solver_exe):
            return jsonify({'error': 'Solver executable not found. Please build the solver first.'}), 500
        
        # Execute solver
        result = subprocess.run(
            [solver_exe, input_json, output_json],
            capture_output=True,
            text=True,
            timeout=60
        )
        
        if result.returncode != 0:
            return jsonify({'error': f'Solver failed: {result.stderr}'}), 500
        
        # Load solution
        if not os.path.exists(output_json):
            return jsonify({'error': 'Solver did not produce output file'}), 500
        
        with open(output_json, 'r') as f:
            solution = json.load(f)
        
        # Load input data for baseline
        with open(input_json, 'r') as f:
            input_data = json.load(f)
        
        baseline_data = input_data.get('baseline', [])
        baseline_cost = sum(b.get('baseline_cost', 0) for b in baseline_data)
        
        # Parse solution and create result
        stats = solution.get('stats', {})
        vehicles_data = solution.get('vehicles', [])
        
        # Calculate vehicles used
        vehicles_used = len([v for v in vehicles_data if v.get('trips', [])])
        vehicles_available = len(input_data.get('vehicles', []))
        
        # Create optimization result
        result_data = {
            'total_cost': stats.get('cost', 0),
            'baseline_cost': baseline_cost,
            'cost_savings': baseline_cost - stats.get('cost', 0),
            'cost_savings_percent': ((baseline_cost - stats.get('cost', 0)) / baseline_cost * 100) if baseline_cost > 0 else 0,
            'total_distance': sum(v.get('total_distance', 0) for v in vehicles_data),
            'total_time': stats.get('time', 0),
            'vehicles_used': vehicles_used,
            'vehicles_available': vehicles_available,
            'hard_violations': stats.get('hard_violations', 0),
            'soft_violations': stats.get('soft_violations', 0)
        }
        
        # Transform solution to frontend format
        transformed_routes = []
        transformed_assignments = []
        
        for vehicle in vehicles_data:
            vehicle_id = vehicle.get('vehicle_id', '')
            trips = vehicle.get('trips', [])
            
            if not trips:
                continue
            
            # Collect all route points and assignments from all trips
            route_points = []
            all_employees = []
            trip_counter = {}
            
            for trip_idx, trip in enumerate(trips):
                trip_number = trip.get('trip_number', trip_idx + 1)
                stops = trip.get('stops', [])
                
                for stop in stops:
                    location = stop.get('location', '')
                    
                    # Extract employee ID and type from location
                    if 'Pickup' in location and location not in ['Office (Drop-off)', 'Vehicle Depot']:
                        emp_id = location.split()[0]  # e.g., "E01 Pickup" -> "E01"
                        
                        # Find employee data
                        emp_data = next((e for e in input_data.get('employees', []) if e.get('employee_id') == emp_id), None)
                        
                        if emp_data:
                            route_points.append({
                                'lat': emp_data.get('pickup_lat', 0),
                                'lng': emp_data.get('pickup_lng', 0),
                                'type': 'pickup',
                                'employee_id': emp_id,
                                'trip_number': trip_number
                            })
                            
                            all_employees.append(emp_id)
                            
                            # Create assignment record
                            transformed_assignments.append({
                                'vehicle_id': vehicle_id,
                                'employee_id': emp_id,
                                'pickup_time': stop.get('arrival_time', ''),
                                'dropoff_time': '',  # Will be filled when we find the dropoff
                                'sequence_order': len(all_employees) - 1,
                                'is_pickup': True,
                                'trip_number': trip_number
                            })
                    
                    elif location == 'Office (Drop-off)':
                        # This is the office dropoff - use first employee's dropoff coordinates
                        # All employees go to the same office
                        if input_data.get('employees') and len(input_data.get('employees')) > 0:
                            office_lat = input_data['employees'][0].get('drop_lat', 12.9716)
                            office_lng = input_data['employees'][0].get('drop_lng', 77.5946)
                        else:
                            office_lat = 12.9716
                            office_lng = 77.5946
                        
                        route_points.append({
                            'lat': office_lat,
                            'lng': office_lng,
                            'type': 'office',
                            'employee_id': None,
                            'trip_number': trip_number
                        })
            
            if route_points:
                # Get vehicle capacity from input
                vehicle_info = next((v for v in input_data.get('vehicles', []) if v.get('vehicle_id') == vehicle_id), None)
                capacity = vehicle_info.get('capacity', 4) if vehicle_info else 4
                
                transformed_routes.append({
                    'vehicle_id': vehicle_id,
                    'route_points': route_points,
                    'total_distance': vehicle.get('total_distance', 0),
                    'total_cost': vehicle.get('total_cost', 0),
                    'passengers_count': len(all_employees),
                    'capacity_utilization': (len(all_employees) / capacity * 100) if capacity > 0 else 0,
                    'trips_count': len(trips)
                })
        
        return jsonify({
            'success': True,
            'result': result_data,
            'routes': transformed_routes,
            'assignments': transformed_assignments
        })
    
    except subprocess.TimeoutExpired:
        return jsonify({'error': 'Solver timed out'}), 500
    except Exception as e:
        return jsonify({'error': f'Error running optimization: {str(e)}'}), 500

@app.route('/api/download-solution', methods=['GET'])
def download_solution():
    try:
        output_json = os.path.join(OUTPUT_FOLDER, 'solution.json')
        if not os.path.exists(output_json):
            return jsonify({'error': 'No solution found'}), 404
        
        return send_file(output_json, as_attachment=True, download_name='solution.json')
    except Exception as e:
        return jsonify({'error': f'Error downloading solution: {str(e)}'}), 500

# Serve React App
@app.route('/', defaults={'path': ''})
@app.route('/<path:path>')
def serve_react_app(path):
    if path and os.path.exists(os.path.join(app.static_folder, path)):
        return send_from_directory(app.static_folder, path)
    else:
        return send_from_directory(app.static_folder, 'index.html')

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=5000)
