"""Excel to JSON Converter for Custom VRP Solver"""

import pandas as pd
import json
import sys

def convert(excel_file, output='input.json'):
    print(f"Reading: {excel_file}")
    
    try:
        # Try lowercase first, then capitalized
        try:
            emp_df = pd.read_excel(excel_file, sheet_name='employees')
            veh_df = pd.read_excel(excel_file, sheet_name='vehicles')
            meta_df = pd.read_excel(excel_file, sheet_name='metadata')
            base_df = pd.read_excel(excel_file, sheet_name='baseline')
        except:
            emp_df = pd.read_excel(excel_file, sheet_name='Employees')
            veh_df = pd.read_excel(excel_file, sheet_name='Vehicles')
            meta_df = pd.read_excel(excel_file, sheet_name='Metadata')
            base_df = pd.read_excel(excel_file, sheet_name='Baseline')
    except Exception as e:
        print(f"Error: {e}")
        return False
    
    data = {}
    
    # Employees
    employees = []
    for _, row in emp_df.iterrows():
        employees.append({
            'employee_id': str(row.get('employee_id', '')),
            'pickup_lat': float(row.get('pickup_lat', 0)),
            'pickup_lng': float(row.get('pickup_lng', 0)),
            'drop_lat': float(row.get('drop_lat', 0)),
            'drop_lng': float(row.get('drop_lng', 0)),
            'priority': int(row.get('priority', 3)),
            'earliest_pickup': str(row.get('earliest_pickup', '08:00')),
            'latest_drop': str(row.get('latest_drop', '18:00')),
            'vehicle_preference': str(row.get('vehicle_preference', 'any')).lower(),
            'sharing_preference': str(row.get('sharing_preference', 'triple')).lower()
        })
    data['employees'] = employees
    print(f"{len(employees)} employees")
    
    # Vehicles
    vehicles = []
    for _, row in veh_df.iterrows():
        vehicles.append({
            'vehicle_id': str(row.get('vehicle_id', '')),
            'current_lat': float(row.get('current_lat', 0)),
            'current_lng': float(row.get('current_lng', 0)),
            'capacity': int(row.get('capacity', 4)),
            'cost_per_km': float(row.get('cost_per_km', 10.0)),
            'avg_speed_kmph': float(row.get('avg_speed_kmph', 40.0)),
            'available_from': str(row.get('available_from', '08:00')),
            'category': str(row.get('category', 'normal')).lower()
        })
    data['vehicles'] = vehicles
    print(f"{len(vehicles)} vehicles")
    
    # Metadata
    metadata = {}
    for _, row in meta_df.iterrows():
        key = str(row.get('key', ''))
        value = row.get('value', 0)
        # Handle different value types
        if 'weight' in key:
            metadata[key] = float(value)
        elif 'delay' in key:
            try:
                metadata[key] = int(value)
            except:
                metadata[key] = int(float(value))
        else:
            metadata[key] = value
    data['metadata'] = metadata
    print(f"{len(metadata)} metadata entries")
    
    # Baseline
    baseline = []
    for _, row in base_df.iterrows():
        baseline.append({
            'employee_id': str(row.get('employee_id', '')),
            'baseline_cost': float(row.get('baseline_cost', 0)),
            'baseline_time': float(row.get('baseline_time', 0))
        })
    data['baseline'] = baseline
    print(f"{len(baseline)} baseline entries")
    
    # Save
    with open(output, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"\nSaved: {output}")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python convert_excel_to_json.py <excel_file> [output.json]")
        sys.exit(1)
    
    excel = sys.argv[1]
    output = sys.argv[2] if len(sys.argv) > 2 else 'input.json'
    
    if convert(excel, output):
        print("\nSuccess")
    else:
        print("\nFailed")
        sys.exit(1)
