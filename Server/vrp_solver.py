import pandas as pd
import json
import sys
import math
from datetime import datetime

# --- HELPER FUNCTIONS ---
def haversine(lat1, lon1, lat2, lon2):
    R = 6371  # Earth radius in km
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat/2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon/2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c

def solve_vrp(data):
    # 1. Parse Data
    employees = sorted(data['employees'], key=lambda x: (x['priority'], x['earliest_pickup']))
    vehicles = data['vehicles']
    metadata = data['metadata']
    
    # 2. Initialize Routes
    routes = []
    for v in vehicles:
        routes.append({
            "vehicle_id": v['vehicle_id'],
            "capacity": v['capacity'],
            "cost_per_km": v['cost_per_km'],
            "stops": [],
            "current_load": 0,
            "total_dist": 0.0,
            "total_cost": 0.0,
            "category": v['category']
        })

    # 3. Greedy Assignment Algorithm (Simplified for Demo)
    unassigned = []
    
    for emp in employees:
        assigned = False
        best_vehicle = None
        min_added_cost = float('inf')

        # Try to find best vehicle
        for route in routes:
            # Check Capacity
            if route['current_load'] + 1 > route['capacity']:
                continue
            
            # Check Vehicle Preference
            if emp['vehicle_preference'] != 'any' and emp['vehicle_preference'] != route['category']:
                continue

            # Simple Cost Calculation (Distance from last stop to this pickup)
            if not route['stops']:
                # From vehicle start loc
                # Note: Using 0,0 for ease if lat is missing, but in real app use v['current_lat']
                # Here we assume vehicle starts at first pickup for simplicity of this heuristic
                dist = 0 
            else:
                last_stop = route['stops'][-1]
                dist = haversine(last_stop['lat'], last_stop['lng'], emp['pickup_lat'], emp['pickup_lng'])
            
            if dist < min_added_cost:
                min_added_cost = dist
                best_vehicle = route

        # Assign
        if best_vehicle:
            # Add Pickup
            best_vehicle['stops'].append({
                "type": "pickup",
                "id": emp['employee_id'],
                "lat": emp['pickup_lat'],
                "lng": emp['pickup_lng'],
                "time": emp['earliest_pickup']
            })
            # Add Drop (Simplified: Immediate drop logic or end of route)
            # For VRP, we usually collect all then drop, but here we just log the job
            best_vehicle['stops'].append({
                "type": "drop",
                "id": emp['employee_id'],
                "lat": emp['drop_lat'],
                "lng": emp['drop_lng'],
                "time": emp['latest_drop']
            })
            
            best_vehicle['current_load'] += 1
            best_vehicle['total_dist'] += min_added_cost
            best_vehicle['total_cost'] += min_added_cost * best_vehicle['cost_per_km']
            assigned = True
        else:
            unassigned.append(emp['employee_id'])

    # 4. Final Output Formatting
    return {
        "metadata": metadata,
        "routes": [r for r in routes if len(r['stops']) > 0],
        "unassigned": unassigned,
        "total_fleet_cost": sum(r['total_cost'] for r in routes),
        "total_employees_served": len(employees) - len(unassigned)
    }

# --- MAIN EXECUTION ---
if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python vrp_solver.py <input_excel> <output_json>")
        sys.exit(1)

    excel_file = sys.argv[1]
    output_file = sys.argv[2]

    try:
        # Load Excel using your friend's logic structure
        try:
            emp_df = pd.read_excel(excel_file, sheet_name='employees')
            veh_df = pd.read_excel(excel_file, sheet_name='vehicles')
            meta_df = pd.read_excel(excel_file, sheet_name='metadata')
            base_df = pd.read_excel(excel_file, sheet_name='baseline')
        except:
            # Fallback for capitalization
            emp_df = pd.read_excel(excel_file, sheet_name='Employees')
            veh_df = pd.read_excel(excel_file, sheet_name='Vehicles')
            meta_df = pd.read_excel(excel_file, sheet_name='Metadata')
            base_df = pd.read_excel(excel_file, sheet_name='Baseline')

        # Convert to Dictionary
        data = {
            'employees': emp_df.to_dict(orient='records'),
            'vehicles': veh_df.to_dict(orient='records'),
            'metadata': meta_df.set_index('key')['value'].to_dict(),
            'baseline': base_df.to_dict(orient='records')
        }
        
        # CLEAN DATA TYPES (Handle NaN, float conversion)
        for e in data['employees']:
            e['priority'] = int(e.get('priority', 3))
            e['pickup_lat'] = float(e.get('pickup_lat', 0))
            e['pickup_lng'] = float(e.get('pickup_lng', 0))
            e['drop_lat'] = float(e.get('drop_lat', 0))
            e['drop_lng'] = float(e.get('drop_lng', 0))
            e['vehicle_preference'] = str(e.get('vehicle_preference', 'any')).lower()
            
        for v in data['vehicles']:
            v['capacity'] = int(v.get('capacity', 4))
            v['cost_per_km'] = float(v.get('cost_per_km', 10))
            v['category'] = str(v.get('category', 'normal')).lower()

        # RUN OPTIMIZATION
        result = solve_vrp(data)

        # Save Output
        with open(output_file, 'w') as f:
            json.dump(result, f, indent=2)

    except Exception as e:
        print(f"Error processing file: {str(e)}")
        sys.exit(1)