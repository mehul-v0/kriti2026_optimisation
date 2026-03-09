from flask import Flask, request, jsonify, send_file
from flask_cors import CORS
import os
import subprocess
import json
import math
import re
from werkzeug.utils import secure_filename
import time
import requests
from dotenv import load_dotenv
import copy  # For deep copying input data
import threading  # For background geometry fetching
import convert_excel_to_json  # Direct import instead of subprocess

# Load environment variables from .env file
load_dotenv()

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
    'solver_executable': None, # cached solver executable path
}

# Store optimization results with geometry fetching status
_optimization_results = {}
_geometry_fetch_lock = threading.Lock()

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


def _compute_baseline_time(baseline_list):
    """Sum baseline times for all employees from the baseline data (in minutes)."""
    return sum(float(b.get('baseline_time', 0)) for b in baseline_list)


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


def _fetch_openrouteservice_distances(locations):
    """
    Fetch actual road distances using OpenRouteService Matrix API.
    
    Args:
        locations: List of (lat, lng) tuples
        
    Returns:
        2D distance matrix in km, or None if API call fails
    """
    n = len(locations)
    if n == 0:
        return None
    
    # OpenRouteService Matrix API endpoint
    # Using the public API - you can get a free API key at https://openrouteservice.org/dev/#/signup
    # For now, using the demo endpoint (limited requests)
    ORS_API_URL = "https://api.openrouteservice.org/v2/matrix/driving-car"
    
    # Note: You should set your own API key here for production use
    # Get free key at: https://openrouteservice.org/dev/#/signup
    # For demo purposes, we'll try without a key first, then fall back to haversine
    
    try:
        # ORS expects [lng, lat] format (opposite of our storage)
        coords = [[lng, lat] for lat, lng in locations]
        
        # ORS free tier limits: max 50 locations at once
        # For larger problems, we'd need to batch
        if n > 50:
            print(f"Warning: {n} locations exceeds ORS limit (50). Using haversine approximation.")
            return None
        
        # Prepare request payload
        payload = {
            "locations": coords,
            "metrics": ["distance"],  # We want distances in meters
            "resolve_locations": "false"
        }
        
        # Get API key from environment variable
        api_key = os.environ.get('ORS_API_KEY', '')
        
        if not api_key or api_key == 'your_api_key_here':
            print("No valid ORS_API_KEY found. Skipping OpenRouteService...")
            return None
        
        # OpenRouteService accepts API key as Authorization header
        headers = {
            'Authorization': api_key,
            'Content-Type': 'application/json; charset=utf-8',
            'Accept': 'application/json, application/geo+json, application/gpx+xml, img/png; charset=utf-8'
        }
        
        print(f"Using OpenRouteService Matrix API")
        print(f"  API Key (first 30 chars): {api_key[:30]}...")
        print(f"  Number of locations: {n}")
        print(f"  Endpoint: {ORS_API_URL}")
        
        # Make request with timeout
        response = requests.post(ORS_API_URL, json=payload, headers=headers, timeout=30)
        
        print(f"  Response Status: {response.status_code}")
        
        if response.status_code == 200:
            data = response.json()
            distances_meters = data.get('distances', [])
            
            # Convert from meters to kilometers
            distance_matrix = []
            for row in distances_meters:
                distance_matrix.append([d / 1000.0 if d is not None else 0.0 for d in row])
            
            print(f"✓ Successfully fetched actual road distances from OpenRouteService")
            return distance_matrix
        elif response.status_code == 403:
            error_data = response.json() if response.headers.get('content-type', '').startswith('application/json') else {}
            error_msg = error_data.get('error', {}).get('message', response.text[:500]) if isinstance(error_data.get('error'), dict) else str(error_data.get('error', response.text[:500]))
            
            print("=" * 70)
            print("❌ OpenRouteService 403 FORBIDDEN")
            print("=" * 70)
            print(f"Error: {error_msg}")
            print("\n🔍 DIAGNOSTIC INFO:")
            print(f"   - API Key Length: {len(api_key)} characters")
            print(f"   - API Key starts with: {api_key[:10]}...")
            print(f"   - Request URL: {ORS_API_URL}")
            print(f"   - Request Headers: Authorization header present = {bool('Authorization' in headers)}")
            print("\n⚠️  COMMON CAUSES:")
            print("   1. EMAIL NOT VERIFIED - Check your OpenRouteService signup email")
            print("   2. API KEY EXPIRED - Keys may expire after inactivity")
            print("   3. WRONG ENDPOINT ACCESS - Your key may not have Matrix API enabled")
            print("   4. INVALID KEY FORMAT - Ensure no extra spaces/quotes in .env file")
            print("\n✅ HOW TO FIX:")
            print("   Step 1: Go to https://openrouteservice.org/dev/#/home")
            print("   Step 2: Log in and verify your email if needed")
            print("   Step 3: Go to Dashboard → API Keys (or Tokens)")
            print("   Step 4: DELETE your old key")
            print("   Step 5: CREATE NEW TOKEN with these settings:")
            print("           - Name: VRP Solver")
            print("           - Rate Limit: Standard (free)")
            print("           - Permissions: CHECK 'Matrix' endpoint")
            print("   Step 6: Copy the NEW key")
            print("   Step 7: Update server/.env file:")
            print("           ORS_API_KEY=<paste_new_key_here>")
            print("   Step 8: Restart Flask server (Ctrl+C and run again)")
            print("=" * 70)
            return None
        elif response.status_code == 401:
            print("❌ 401 UNAUTHORIZED - Invalid API key format or missing")
            print("   Check that .env file has: ORS_API_KEY=your_actual_key")
            return None
        elif response.status_code == 429:
            print("❌ 429 RATE LIMIT EXCEEDED - Too many requests")
            print("   Wait a few minutes or upgrade your ORS plan")
            return None
        else:
            print(f"OpenRouteService API error: {response.status_code}")
            print(f"Response: {response.text[:300]}")
            return None
            
    except requests.exceptions.Timeout:
        print("OpenRouteService request timed out. Using fallback distance calculation.")
        return None
    except Exception as e:
        print(f"Error fetching OpenRouteService distances: {str(e)}")
        return None


def _fetch_route_geometry(start_lat, start_lng, end_lat, end_lng, max_retries=3):
    """
    Fetch actual road route geometry from OpenRouteService Directions API with retry logic.
    
    Args:
        start_lat, start_lng: Starting coordinates
        end_lat, end_lng: Ending coordinates
        max_retries: Maximum number of retry attempts for failed requests (default: 3)
        
    Returns:
        List of [lat, lng] coordinates along the route, or None if API call fails
    """
    # Skip if start and end are the same point (e.g., office to office)
    # Use small tolerance for floating point comparison (about 11 meters)
    distance = ((end_lat - start_lat) ** 2 + (end_lng - start_lng) ** 2) ** 0.5
    if distance < 0.0001:  # ~11 meters tolerance
        return None
    
    api_key = os.environ.get('ORS_API_KEY', '')
    
    if not api_key or api_key == 'your_api_key_here':
        return None
    
    ORS_DIRECTIONS_URL = "https://api.openrouteservice.org/v2/directions/driving-car"
    
    # Retry loop with exponential backoff
    for attempt in range(max_retries):
        try:
            # POST request with GeoJSON format (as per ORS documentation)
            payload = {
                "coordinates": [
                    [start_lng, start_lat],
                    [end_lng, end_lat]
                ],
                "format": "geojson"  # Request GeoJSON format for decoded coordinates
            }
            
            headers = {
                'Authorization': api_key,
                'Content-Type': 'application/json',
                'Accept': 'application/json, application/geo+json'
            }
            
            # Increased timeout from 3s to 10s for better reliability
            response = requests.post(ORS_DIRECTIONS_URL, json=payload, headers=headers, timeout=10)
            
            if response.status_code == 200:
                try:
                    data = response.json()
                    
                    # With format=geojson, response structure is GeoJSON FeatureCollection
                    if 'features' in data:
                        features = data.get('features', [])
                        if features and len(features) > 0:
                            geometry = features[0].get('geometry', {})
                            if geometry.get('type') == 'LineString':
                                coordinates = geometry.get('coordinates', [])
                                if coordinates:
                                    # Convert from [lng, lat] to [lat, lng]
                                    result = [[coord[1], coord[0]] for coord in coordinates]
                                    if attempt > 0:
                                        print(f"    ✓ Fetched geometry with {len(result)} points (retry {attempt})")
                                    else:
                                        print(f"    ✓ Fetched geometry with {len(result)} points")
                                    return result
                    
                    # Fallback to routes format (ORS returns this even with format=geojson)
                    routes = data.get('routes', [])
                    if routes and len(routes) > 0:
                        geometry = routes[0].get('geometry')
                        if geometry:
                            # Check if it's an encoded polyline string
                            if isinstance(geometry, str):
                                # Decode the polyline string
                                try:
                                    import polyline
                                    decoded = polyline.decode(geometry)
                                    if attempt > 0:
                                        print(f"    ✓ Decoded polyline with {len(decoded)} points (retry {attempt})")
                                    else:
                                        print(f"    ✓ Decoded polyline with {len(decoded)} points")
                                    return decoded  # Already in [lat, lng] format
                                except Exception as e:
                                    print(f"    ⚠ Polyline decode error: {str(e)}")
                                    return None
                            # Check if it's GeoJSON object
                            elif isinstance(geometry, dict) and geometry.get('type') == 'LineString':
                                coordinates = geometry.get('coordinates', [])
                                if coordinates:
                                    result = [[coord[1], coord[0]] for coord in coordinates]
                                    if attempt > 0:
                                        print(f"    ✓ Fetched geometry with {len(result)} points (retry {attempt})")
                                    else:
                                        print(f"    ✓ Fetched geometry with {len(result)} points")
                                    return result
                    
                    print(f"    ⚠ No geometry in response")
                    return None
                    
                except Exception as e:
                    print(f"    ⚠ Parse error: {str(e)}")
                    return None
            
            # Handle HTTP error codes
            elif response.status_code == 404:
                # 404 Not Found - don't retry, route doesn't exist
                print(f"    ⚠ HTTP 404 - Route not found")
                return None
            
            elif response.status_code == 429:
                # Rate limit exceeded - don't retry immediately
                print(f"    ⚠ HTTP 429 - Rate limit exceeded")
                return None
            
            elif response.status_code >= 500:
                # Server errors - retry with backoff
                if attempt < max_retries - 1:
                    backoff_time = 2 ** attempt  # Exponential backoff: 1s, 2s, 4s
                    print(f"    ⚠ HTTP {response.status_code} - Server error, retrying in {backoff_time}s (attempt {attempt + 1}/{max_retries})")
                    time.sleep(backoff_time)
                    continue  # Retry
                else:
                    print(f"    ⚠ HTTP {response.status_code} - Max retries reached")
                    return None
            
            else:
                # Other HTTP errors - don't retry
                print(f"    ⚠ HTTP {response.status_code}")
                return None
            
        except requests.exceptions.Timeout:
            # Timeout - retry with backoff
            if attempt < max_retries - 1:
                backoff_time = 2 ** attempt  # Exponential backoff: 1s, 2s, 4s
                print(f"    ⚠ Timeout (10s), retrying in {backoff_time}s (attempt {attempt + 1}/{max_retries})")
                time.sleep(backoff_time)
                continue  # Retry
            else:
                print(f"    ⚠ Timeout - Max retries reached")
                return None
        
        except requests.exceptions.ConnectionError as e:
            # Connection errors - retry with backoff
            if attempt < max_retries - 1:
                backoff_time = 2 ** attempt  # Exponential backoff: 1s, 2s, 4s
                print(f"    ⚠ Connection error, retrying in {backoff_time}s (attempt {attempt + 1}/{max_retries})")
                time.sleep(backoff_time)
                continue  # Retry
            else:
                print(f"    ⚠ Connection error - Max retries reached: {str(e)}")
                return None
        
        except Exception as e:
            # Other errors - don't retry
            print(f"    ⚠ Request failed: {str(e)}")
            return None
    
    # If we get here, all retries failed
    return None


def _fetch_geometries_background(optimization_id, solver_output, input_data):
    """
    Background thread function to fetch route geometries after optimization completes.
    Updates _optimization_results[optimization_id] with geometry data as it's fetched.
    """
    print(f"\n🔄 [Background] Starting geometry fetch for optimization {optimization_id}")
    
    try:
        employees = input_data.get('employees', [])
        vehicles_input = input_data.get('vehicles', [])
        
        emp_lookup = {e['employee_id']: e for e in employees}
        veh_lookup = {v['vehicle_id']: v for v in vehicles_input}
        
        office_lat = employees[0]['drop_lat'] if employees else 0
        office_lng = employees[0]['drop_lng'] if employees else 0
        
        solver_vehicles = solver_output.get('vehicles', [])
        
        # Count total geometry segments needed
        total_segments = 0
        for sv in solver_vehicles:
            for trip in sv.get('trips', []):
                total_segments += len(trip.get('stops', []))
        
        # Update progress
        with _geometry_fetch_lock:
            if optimization_id in _optimization_results:
                _optimization_results[optimization_id]['geometry_status'] = 'fetching'
                _optimization_results[optimization_id]['geometry_progress'] = {
                    'total': total_segments,
                    'fetched': 0
                }
        
        fetched_count = 0
        
        # Iterate through all vehicles/trips/stops and fetch geometry
        for sv in solver_vehicles:
            vid = sv['vehicle_id']
            veh_info = veh_lookup.get(vid, {})
            
            # Find the corresponding route in the stored response
            with _geometry_fetch_lock:
                if optimization_id not in _optimization_results:
                    print(f"⚠️ [Background] Optimization {optimization_id} not found, stopping")
                    return
                
                routes = _optimization_results[optimization_id]['response']['routes']
                route_idx = next((i for i, r in enumerate(routes) if r['vehicle_id'] == vid), None)
                
                if route_idx is None:
                    continue
            
            for trip in sv.get('trips', []):
                trip_num = trip['trip_number']
                stops = trip.get('stops', [])
                
                # Track previous location
                if trip_num == 1:
                    prev_lat, prev_lng = veh_info.get('current_lat', 0), veh_info.get('current_lng', 0)
                else:
                    prev_lat, prev_lng = office_lat, office_lng
                
                for stop_idx, stop in enumerate(stops):
                    loc = stop.get('location', '')
                    emp_id = _extract_employee_id(loc)
                    
                    if 'Pickup' in loc and emp_id:
                        emp = emp_lookup.get(emp_id, {})
                        pickup_lat = emp.get('pickup_lat', 0)
                        pickup_lng = emp.get('pickup_lng', 0)
                        
                        # Fetch geometry
                        geometry = _fetch_route_geometry(prev_lat, prev_lng, pickup_lat, pickup_lng)
                        
                        # Update the stored result
                        with _geometry_fetch_lock:
                            if optimization_id in _optimization_results:
                                route_points = routes[route_idx]['route_points']
                                # Find matching route point and update
                                updated = False
                                for rp in route_points:
                                    if (rp['type'] == 'pickup' and 
                                        rp['employee_id'] == emp_id and 
                                        rp['trip_number'] == trip_num):
                                        rp['geometry'] = geometry  # Update even if None
                                        updated = True
                                        if geometry:
                                            fetched_count += 1
                                        break
                                
                                if not updated:
                                    print(f"    ⚠️ [Background] Could not find matching pickup point for {emp_id} trip {trip_num}")
                                
                                _optimization_results[optimization_id]['geometry_progress']['fetched'] = fetched_count
                        
                        prev_lat, prev_lng = pickup_lat, pickup_lng
                        
                    elif 'Drop' in loc or 'Office' in loc:
                        # Fetch geometry to office
                        geometry = _fetch_route_geometry(prev_lat, prev_lng, office_lat, office_lng)
                        
                        # Update the stored result
                        with _geometry_fetch_lock:
                            if optimization_id in _optimization_results:
                                route_points = routes[route_idx]['route_points']
                                # Find matching office route point for this trip and update
                                updated = False
                                for rp in route_points:
                                    if (rp['type'] == 'office' and 
                                        rp['trip_number'] == trip_num):
                                        rp['geometry'] = geometry  # Update even if None
                                        updated = True
                                        if geometry:
                                            fetched_count += 1
                                        break
                                
                                if not updated:
                                    print(f"    ⚠️ [Background] Could not find matching office point for trip {trip_num}")
                                
                                _optimization_results[optimization_id]['geometry_progress']['fetched'] = fetched_count
                        
                        prev_lat, prev_lng = office_lat, office_lng
    
    except Exception as e:
        print(f"❌ [Background] Error in geometry fetch: {str(e)}")
        import traceback
        traceback.print_exc()
    
    finally:
        # Always mark as complete, even if there were errors
        with _geometry_fetch_lock:
            if optimization_id in _optimization_results:
                _optimization_results[optimization_id]['geometry_status'] = 'complete'
                fetched = _optimization_results[optimization_id]['geometry_progress'].get('fetched', 0)
                total = _optimization_results[optimization_id]['geometry_progress'].get('total', 0)
                print(f"✅ [Background] Geometry fetch complete for optimization {optimization_id} ({fetched}/{total} segments)")


def _transform_solver_output(solver_output, input_data, fetch_geometry=False):
    """
    Transform the C++ solver output into the shape the frontend expects:
      - routes:  BackendRoute[]
      - assignments:  BackendAssignment[]
      - result:  summary object
      - violation_details:  constraint violation details
      
    Args:
        solver_output: Raw solver output dict
        input_data: Input data dict
        fetch_geometry: If True, fetch route geometries (blocking). If False, skip geometry.
    """
    employees = input_data.get('employees', [])
    vehicles_input = input_data.get('vehicles', [])
    baseline_list = input_data.get('baseline', [])
    metadata = input_data.get('metadata', {})
    
    # Check if we used actual map distances (for route geometry)
    uses_actual_maps = metadata.get('uses_custom_distance_matrix', False) and fetch_geometry
    import time

    baseline_cost = _compute_baseline_cost(baseline_list)
    baseline_time = _compute_baseline_time(baseline_list)
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
    geometry_fetches = 0  # Track number of geometry API calls

    # For violation checking
    capacity_violations = []
    time_window_violations = []
    vehicle_pref_violations = []
    sharing_pref_violations = []
    priority_delay_violations = []

    # Priority delay tolerances from metadata
    priority_delay_map = {}
    for p in range(1, 6):
        priority_delay_map[p] = int(metadata.get(f'priority_{p}_max_delay_min', 15))

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
        
        # Store per-trip cost/distance data
        trip_costs = {}  # trip_num -> cost
        trip_distances = {}  # trip_num -> distance

        for trip in sv.get('trips', []):
            trip_num = trip['trip_number']
            stops = trip.get('stops', [])
            passengers_in_trip = []
            
            # Store per-trip totals from solver output
            trip_costs[trip_num] = float(trip.get('total_cost', 0))
            trip_distances[trip_num] = float(trip.get('total_distance', 0))
            
            # Track previous location for geometry fetching
            # For first trip, start from vehicle's current location;  for subsequent trips, start from office
            if trip_num == 1:
                prev_lat, prev_lng = veh_info.get('current_lat', 0), veh_info.get('current_lng', 0)
            else:
                prev_lat, prev_lng = office_lat, office_lng

            for stop in stops:
                loc = stop.get('location', '')
                emp_id = _extract_employee_id(loc)

                if 'Pickup' in loc and emp_id:
                    emp = emp_lookup.get(emp_id, {})
                    pickup_lat = emp.get('pickup_lat', 0)
                    pickup_lng = emp.get('pickup_lng', 0)
                    
                    # Fetch route geometry if using actual maps
                    geometry = None
                    if uses_actual_maps and prev_lat is not None and prev_lng is not None:
                        geometry = _fetch_route_geometry(prev_lat, prev_lng, pickup_lat, pickup_lng)
                        if geometry:
                            geometry_fetches += 1
                    
                    route_points.append({
                        'lat': pickup_lat,
                        'lng': pickup_lng,
                        'type': 'pickup',
                        'employee_id': emp_id,
                        'trip_number': trip_num,
                        'arrival_time': stop.get('arrival_time', ''),
                        'departure_time': stop.get('departure_time', ''),
                        'distance_from_prev': float(stop.get('distance_from_prev', 0)),
                        'geometry': geometry,  # Route geometry from previous point to this point
                    })
                    
                    prev_lat, prev_lng = pickup_lat, pickup_lng
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
                    # Fetch route geometry if using actual maps
                    geometry = None
                    if uses_actual_maps and prev_lat is not None and prev_lng is not None:
                        geometry = _fetch_route_geometry(prev_lat, prev_lng, office_lat, office_lng)
                        if geometry:
                            geometry_fetches += 1
                    
                    route_points.append({
                        'lat': office_lat,
                        'lng': office_lng,
                        'type': 'office',
                        'employee_id': None,
                        'trip_number': trip_num,
                        'arrival_time': stop.get('arrival_time', ''),
                        'departure_time': stop.get('departure_time', ''),
                        'distance_from_prev': float(stop.get('distance_from_prev', 0)),
                        'geometry': geometry,  # Route geometry from previous point to office
                    })
                    
                    prev_lat, prev_lng = office_lat, office_lng

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
                    delay = oa_min - dl_min

                    # Priority-based delay tolerance
                    emp_priority = int(emp.get('priority', 5))
                    tolerance = priority_delay_map.get(emp_priority, 15)

                    if delay > 0 and delay <= tolerance:
                        # Within priority tolerance — not a hard violation,
                        # just mention under priority-based delay tolerance
                        priority_delay_violations.append({
                            'employee': pid,
                            'vehicle': vid,
                            'trip': trip_num,
                            'priority': emp_priority,
                            'tolerance_min': tolerance,
                            'actual_delay_min': delay,
                            'within_tolerance': True,
                        })
                    elif delay > tolerance:
                        # Exceeds priority tolerance — hard time-window violation
                        time_window_violations.append({
                            'employee': pid,
                            'vehicle': vid,
                            'trip': trip_num,
                            'office_arrival': office_arrival,
                            'deadline': deadline,
                            'delay_min': delay,
                        })
                        # Also note under priority delay
                        priority_delay_violations.append({
                            'employee': pid,
                            'vehicle': vid,
                            'trip': trip_num,
                            'priority': emp_priority,
                            'tolerance_min': tolerance,
                            'actual_delay_min': delay,
                            'within_tolerance': False,
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
                        vehicle_pref_violations.append({
                            'employee': pid,
                            'vehicle': vid,
                            'preferred': vpref.capitalize(),
                            'assigned': veh_category.capitalize(),
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
            'trip_costs': trip_costs,
            'trip_distances': trip_distances,
        })

    # Unassigned employees
    unassigned = []
    for emp in employees:
        if emp['employee_id'] not in assigned_employee_ids:
            unassigned.append({'employee': emp['employee_id']})

    hard_violations = len(capacity_violations) + len(time_window_violations) + len(unassigned)
    # Only count priority delay entries that exceeded tolerance as soft violations
    priority_exceeded = [v for v in priority_delay_violations if not v.get('within_tolerance', False)]
    soft_violations = len(vehicle_pref_violations) + len(sharing_pref_violations) + len(priority_exceeded)

    violation_details = {
        'capacity_violations': capacity_violations,
        'time_window_violations': time_window_violations,
        'unassigned_employees': unassigned,
        'vehicle_pref_violations': vehicle_pref_violations,
        'sharing_pref_violations': sharing_pref_violations,
        'priority_delay_violations': priority_delay_violations,
    }

    result_summary = {
        'total_cost': total_cost,
        'baseline_cost': baseline_cost,
        'cost_savings': round(cost_savings, 2),
        'cost_savings_percent': cost_savings_percent,
        'total_distance': round(total_distance, 2),
        'total_time': round(total_time, 2),
        'baseline_time': round(baseline_time, 2),
        'vehicles_used': len(solver_vehicles),
        'vehicles_available': len(vehicles_input),
        'hard_violations': hard_violations,
        'soft_violations': soft_violations,
    }
    
    # Log geometry fetch stats
    if uses_actual_maps and geometry_fetches > 0:
        print(f"  ✓ Fetched {geometry_fetches} route geometries for map visualization")

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

        # Convert Excel → JSON (using direct function call instead of subprocess)
        input_json = os.path.join(OUTPUT_FOLDER, f'input_{timestamp}.json')
        
        print(f"Converting Excel file: {filepath}...")
        conversion_start = time.time()
        
        try:
            # Direct function call - much faster than subprocess!
            success = convert_excel_to_json.convert(filepath, input_json)
            conversion_time = time.time() - conversion_start
            
            if not success:
                return jsonify({'success': False, 'error': 'Excel conversion failed. Check file format and required sheets.'}), 500
            
            print(f"✓ Conversion complete in {conversion_time:.2f}s")
            
        except Exception as e:
            return jsonify({'success': False, 'error': f'Excel conversion error: {str(e)}'}), 500

        # Parse the converted JSON
        with open(input_json, 'r') as f:
            input_data = json.load(f)

        employees = input_data.get('employees', [])
        vehicles = input_data.get('vehicles', [])
        baseline_list = input_data.get('baseline', [])
        
        # Validate data
        if len(employees) == 0:
            return jsonify({'success': False, 'error': 'No employees found in Excel file. Please check the Employees sheet.'}), 400
        if len(vehicles) == 0:
            return jsonify({'success': False, 'error': 'No vehicles found in Excel file. Please check the Vehicles sheet.'}), 400
        
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
            'metadata': input_data.get('metadata', {}),
        })

    except Exception as e:
        return jsonify({'success': False, 'error': f'Upload failed: {str(e)}'}), 500


@app.route('/api/optimize', methods=['POST'])
def optimize():
    try:
        # Read request body
        body = request.get_json(silent=True) or {}

        # Check if the request body contains inline input data (sent by Flutter app).
        # Inline data is detected by the presence of an 'employees' list in the body.
        if 'employees' in body and isinstance(body.get('employees'), list):
            # Flutter app sends full input data + config in the body.
            # Write it to a temporary JSON file for the solver.
            timestamp = str(int(time.time()))
            input_json = os.path.join(OUTPUT_FOLDER, f'input_{timestamp}.json')
            # Separate solver config keys from the input data
            config_keys = {'solverDurationSeconds', 'costWeight', 'timeWeight',
                           'priorityDelays', 'distanceMethod', 'mode'}
            input_data = {k: v for k, v in body.items() if k not in config_keys}
        elif _state.get('input_json') and os.path.exists(_state['input_json']):
            # Website flow: data was previously uploaded via /api/upload
            input_json = _state['input_json']
            input_data = copy.deepcopy(_state['input_data'])
        else:
            return jsonify({'success': False, 'error': 'No data uploaded yet. Please upload an Excel file first.'}), 400

        solver_duration = int(body.get('solverDurationSeconds', 30))  # Default to 30s (Standard)
        
        print(f"\n{'='*60}")
        print(f"Optimization Request: {solver_duration}s solver duration")
        print(f"{'='*60}")
        
        # Get optimization config from frontend, falling back to Excel metadata values
        excel_meta = input_data.get('metadata', {})
        cost_weight = float(body.get('costWeight', excel_meta.get('objective_cost_weight', 0.7)))
        time_weight = float(body.get('timeWeight', excel_meta.get('objective_time_weight', 0.3)))
        default_delays = {
            1: excel_meta.get('priority_1_max_delay_min', 5),
            2: excel_meta.get('priority_2_max_delay_min', 5),
            3: excel_meta.get('priority_3_max_delay_min', 10),
            4: excel_meta.get('priority_4_max_delay_min', 15),
            5: excel_meta.get('priority_5_max_delay_min', 15),
        }
        priority_delays = body.get('priorityDelays', default_delays)
        distance_method = body.get('distanceMethod', 'haversine')
        
        # Update metadata in input data with frontend settings
        if 'metadata' not in input_data:
            input_data['metadata'] = {}
        
        input_data['metadata']['objective_cost_weight'] = cost_weight
        input_data['metadata']['objective_time_weight'] = time_weight
        input_data['metadata']['priority_1_max_delay_min'] = int(priority_delays.get('1', priority_delays.get(1, default_delays[1])))
        input_data['metadata']['priority_2_max_delay_min'] = int(priority_delays.get('2', priority_delays.get(2, default_delays[2])))
        input_data['metadata']['priority_3_max_delay_min'] = int(priority_delays.get('3', priority_delays.get(3, default_delays[3])))
        input_data['metadata']['priority_4_max_delay_min'] = int(priority_delays.get('4', priority_delays.get(4, default_delays[4])))
        input_data['metadata']['priority_5_max_delay_min'] = int(priority_delays.get('5', priority_delays.get(5, default_delays[5])))
        
        # Handle distance calculation method
        if distance_method == 'actual_maps':
            print("\n=== Fetching Actual Road Distances from OpenRouteService ===")
            
            # Build list of all locations: [office, emp1_pickup, emp2_pickup, ..., veh1_start, veh2_start, ...]
            employees = input_data.get('employees', [])
            vehicles = input_data.get('vehicles', [])
            
            locations = []
            location_labels = []
            
            # Office location (from first employee's drop location)
            if employees:
                office_lat = employees[0].get('drop_lat', 0)
                office_lng = employees[0].get('drop_lng', 0)
                locations.append((office_lat, office_lng))
                location_labels.append("Office")
            
            # Employee pickup locations
            for emp in employees:
                locations.append((emp.get('pickup_lat', 0), emp.get('pickup_lng', 0)))
                location_labels.append(f"Emp {emp.get('employee_id', 'UNKNOWN')}")
            
            # Vehicle start locations
            for veh in vehicles:
                locations.append((veh.get('current_lat', 0), veh.get('current_lng', 0)))
                location_labels.append(f"Veh {veh.get('vehicle_id', 'UNKNOWN')}")
            
            print(f"Fetching distances for {len(locations)} locations...")
            
            # Fetch actual distances from OpenRouteService
            distance_matrix = _fetch_openrouteservice_distances(locations)
            
            if distance_matrix is not None:
                # Success! Store the custom distance matrix
                input_data['distance_matrix'] = distance_matrix
                input_data['metadata']['distance_multiplier'] = 1.0  # Already using actual distances
                input_data['metadata']['uses_custom_distance_matrix'] = True
                print(f"✓ Using actual road distances from OpenRouteService")
                print(f"  Distance matrix size: {len(distance_matrix)}x{len(distance_matrix[0])}")
                print(f"  Sample distances (km): [0][1]={distance_matrix[0][1]:.2f}, [1][2]={distance_matrix[1][2]:.2f}")
            else:
                # Fallback to haversine with 1.3x multiplier
                print("⚠ Falling back to haversine approximation (1.3x multiplier)")
                input_data['metadata']['distance_multiplier'] = 1.3
                input_data['metadata']['uses_custom_distance_matrix'] = False
                if 'distance_matrix' in input_data:
                    del input_data['distance_matrix']
        else:
            # Use haversine distance calculation
            input_data['metadata']['distance_multiplier'] = 1.0
            input_data['metadata']['uses_custom_distance_matrix'] = False
            if 'distance_matrix' in input_data:
                del input_data['distance_matrix']
            print("Using haversine distance calculation")
        
        # Write updated input JSON (no indent for faster write)
        print("\n=== Starting Optimization ===")
        json_write_start = time.time()
        with open(input_json, 'w') as f:
            json.dump(input_data, f)  # Removed indent=2 for 3-5x faster write
        json_write_time = time.time() - json_write_start
        print(f"✓ Updated input JSON in {json_write_time:.3f}s")
        
        # Verify what we wrote
        has_distance_matrix = 'distance_matrix' in input_data
        print(f"  Contains distance_matrix: {has_distance_matrix}")
        if has_distance_matrix:
            print(f"  Distance matrix size: {len(input_data['distance_matrix'])}x{len(input_data['distance_matrix'][0])}")

        timestamp = str(int(time.time()))
        output_json = os.path.join(OUTPUT_FOLDER, f'output_{timestamp}.json')

        # Use cached solver executable or check
        solver_executable = _state.get('solver_executable')
        if not solver_executable or not os.path.exists(solver_executable):
            solver_executable = 'vrp_solver_custom.exe' if os.name == 'nt' else './vrp_solver_custom'
            _state['solver_executable'] = solver_executable
        
        if not os.path.exists(solver_executable):
            print("⚠ Solver not found, building...")
            build_start = time.time()
            if os.name == 'nt':
                build_result = subprocess.run(['build.bat'], capture_output=True, text=True, timeout=120, shell=True)
            else:
                build_result = subprocess.run(['make'], capture_output=True, text=True, timeout=120)
            build_time = time.time() - build_start
            if build_result.returncode != 0:
                return jsonify({'success': False, 'error': f'Solver build failed: {build_result.stderr}'}), 500
            print(f"✓ Solver built in {build_time:.2f}s")

        # Print key cost-function parameters from solver_config.json
        try:
            config_path = os.path.join(os.path.dirname(__file__), 'solver_config.json')
            with open(config_path, 'r') as cf:
                solver_cfg = json.load(cf)
            pw = solver_cfg.get('penalty_weights', {})
            ow = solver_cfg.get('objective_weights', {})
            plw = solver_cfg.get('priority_lateness_weights', {})
            lsw = solver_cfg.get('local_search_weights', {})
            print("\n📋 Active Solver Config (cost-related):")
            print(f"  Penalty Weights:")
            print(f"    unassigned_employee_penalty : {pw.get('unassigned_employee_penalty', '?')}")
            print(f"    time_violation_penalty      : {pw.get('time_violation_penalty', '?')}")
            print(f"    lateness_per_minute_penalty  : {pw.get('lateness_per_minute_penalty', '?')}")
            print(f"    priority_lateness_multiplier : {pw.get('priority_lateness_multiplier', '?')}")
            print(f"    preference_violation_penalty : {pw.get('preference_violation_penalty', '?')}")
            print(f"    vehicle_activation_cost      : {pw.get('vehicle_activation_cost', '?')}")
            print(f"  Objective Weights:")
            print(f"    cost_weight : {ow.get('cost_weight', '?')}")
            print(f"    time_weight : {ow.get('time_weight', '?')}")
            print(f"  Priority Lateness Weights:")
            print(f"    P1={plw.get('P1','?')} P2={plw.get('P2','?')} P3={plw.get('P3','?')} P4={plw.get('P4','?')} P5={plw.get('P5','?')}")
            print(f"  Local Search Weights:")
            print(f"    hard_violation : {lsw.get('hard_violation_weight', '?')}")
            print(f"    pref_violation : {lsw.get('pref_violation_weight', '?')}")
            print(f"    lateness       : {lsw.get('lateness_weight', '?')}")
        except Exception as e:
            print(f"⚠ Could not read solver_config.json: {e}")

        # Run solver
        print(f"\n🚀 Starting C++ solver (duration: {solver_duration}s)...")
        solver_start = time.time()
        timeout_sec = max(solver_duration + 30, 60)  # add buffer
        try:
            solver_result = subprocess.run(
                [solver_executable, input_json, output_json, str(solver_duration)],
                capture_output=True, timeout=timeout_sec,
                encoding='utf-8', errors='replace',
            )
            solver_time = time.time() - solver_start
            
            if solver_result.returncode != 0:
                return jsonify({
                    'success': False,
                    'error': f'Solver failed: {solver_result.stderr}',
                }), 500
            
            print(f"✓ Solver completed in {solver_time:.2f}s")
            
        except subprocess.TimeoutExpired:
            return jsonify({'success': False, 'error': f'Solver timed out after {timeout_sec}s'}), 500

        # Read solver output
        print("📥 Reading and transforming results...")
        read_start = time.time()
        if not os.path.exists(output_json):
            return jsonify({'success': False, 'error': 'Solver did not produce output'}), 500

        with open(output_json, 'r') as f:
            solver_output = json.load(f)
        
        # Log solver output stats
        solver_cost = solver_output.get('cost', 0)
        solver_vehicles = solver_output.get('vehicles', [])
        solver_total_dist = sum(v.get('total_distance', 0) for v in solver_vehicles)
        print(f"  Solver output: cost={solver_cost:.2f}, total_distance={solver_total_dist:.2f}km, vehicles={len(solver_vehicles)}")
        
        # Check if we should fetch route geometries in background
        # Fetch geometry if user selected 'actual_maps' regardless of distance matrix success
        uses_actual_maps = distance_method == 'actual_maps'
        print(f"  Distance method: {distance_method}, Will fetch geometry: {uses_actual_maps}")

        # Store for download
        _state['output_json'] = output_json

        # Transform to frontend shape WITHOUT geometry first (fast return)
        response = _transform_solver_output(solver_output, input_data, fetch_geometry=False)
        
        # Generate optimization ID
        optimization_id = str(int(time.time() * 1000))  # millisecond timestamp
        response['optimization_id'] = optimization_id
        
        # Store initial response (without geometry)
        with _geometry_fetch_lock:
            _optimization_results[optimization_id] = {
                'response': response,
                'geometry_status': 'pending' if uses_actual_maps else 'not_needed',
                'geometry_progress': {'total': 0, 'fetched': 0},
            }
        
        # Start background geometry fetching if needed
        if uses_actual_maps:
            print(f"  📍 Starting background geometry fetch (optimization_id: {optimization_id})")
            thread = threading.Thread(
                target=_fetch_geometries_background,
                args=(optimization_id, solver_output, input_data),
                daemon=True
            )
            thread.start()
        
        transform_time = time.time() - read_start
        print(f"✓ Results transformed in {transform_time:.3f}s (geometry fetching in background)")
        print(f"{'=' * 60}\n")
        
        return jsonify(response)

    except Exception as e:
        return jsonify({'success': False, 'error': f'Optimization failed: {str(e)}'}), 500


@app.route('/api/geometry-status/<optimization_id>')
def get_geometry_status(optimization_id):
    """
    Poll endpoint to check geometry fetching progress.
    Returns the current optimization result with updated geometry data.
    """
    with _geometry_fetch_lock:
        if optimization_id not in _optimization_results:
            print(f"⚠️ Geometry status check: optimization {optimization_id} not found")
            return jsonify({'success': False, 'error': 'Optimization not found'}), 404
        
        opt_data = _optimization_results[optimization_id]
        status = opt_data['geometry_status']
        progress = opt_data['geometry_progress']
        
        # Debug logging - count actual geometry in response
        routes = opt_data['response'].get('routes', [])
        routes_with_geom = 0
        total_points = 0
        points_with_geom = 0
        
        for route in routes:
            has_geom = False
            for pt in route.get('route_points', []):
                total_points += 1
                if pt.get('geometry') and len(pt.get('geometry', [])) > 0:
                    points_with_geom += 1
                    has_geom = True
            if has_geom:
                routes_with_geom += 1
        
        if status == 'complete':
            print(f"📊 Returning COMPLETE status for {optimization_id} ({progress['fetched']}/{progress['total']})")
            print(f"   Response has {routes_with_geom}/{len(routes)} routes with geometry, {points_with_geom}/{total_points} points with geometry")
        
        return jsonify({
            'success': True,
            'geometry_status': status,
            'geometry_progress': progress,
            'result': opt_data['response'],  # Return full result with updated geometry
        })


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
    
    # Pre-check solver executable at startup
    solver_exe = 'vrp_solver_custom.exe' if os.name == 'nt' else './vrp_solver_custom'
    solver_ready = os.path.exists(solver_exe)
    
    print(f"\n{'='*60}")
    print(f"  VRP Solver Backend — http://localhost:{port}")
    print(f"{'='*60}")
    print(f"  Upload folder : {UPLOAD_FOLDER}")
    print(f"  Output folder : {OUTPUT_FOLDER}")
    print(f"  Solver status : {'✓ Ready' if solver_ready else '⚠ Not built (will compile on first use)'}")
    if solver_ready:
        _state['solver_executable'] = solver_exe
    print(f"{'='*60}\n")
    
    app.run(host='0.0.0.0', port=port, debug=True)
