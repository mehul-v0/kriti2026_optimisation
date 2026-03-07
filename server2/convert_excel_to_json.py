"""Excel to JSON Converter for Custom VRP Solver"""

import pandas as pd
import json
import sys
import math

def _safe_float(val, default=0.0):
    """Convert value to float, returning default if NaN or invalid."""
    try:
        result = float(val)
        if math.isnan(result) or math.isinf(result):
            return default
        return result
    except (ValueError, TypeError):
        return default

def _safe_int(val, default=0):
    """Convert value to int, returning default if NaN or invalid."""
    try:
        result = float(val)
        if math.isnan(result) or math.isinf(result):
            return default
        return int(result)
    except (ValueError, TypeError):
        return default

def _safe_str(val, default=''):
    """Convert value to string, returning default if NaN or empty."""
    if val is None or (isinstance(val, float) and math.isnan(val)):
        return default
    result = str(val).strip()
    return result if result else default

def convert(excel_file, output='input.json'):
    print(f"Reading: {excel_file}")
    
    try:
        # Read all sheet names from the Excel file
        xls = pd.ExcelFile(excel_file)
        available_sheets = [s.lower() for s in xls.sheet_names]
        sheet_map = {s.lower(): s for s in xls.sheet_names}
        
        # Check for required sheets (case-insensitive)
        required = ['employees', 'vehicles', 'metadata', 'baseline']
        missing = [s for s in required if s not in available_sheets]
        
        if missing:
            print(f"Error: Missing required sheets: {', '.join(missing)}")
            print(f"Available sheets: {', '.join(xls.sheet_names)}")
            print(f"Required sheets: Employees, Vehicles, Metadata, Baseline (case-insensitive)")
            return False
        
        # Read sheets using actual names from file
        emp_df = pd.read_excel(excel_file, sheet_name=sheet_map['employees'])
        veh_df = pd.read_excel(excel_file, sheet_name=sheet_map['vehicles'])
        meta_df = pd.read_excel(excel_file, sheet_name=sheet_map['metadata'])
        base_df = pd.read_excel(excel_file, sheet_name=sheet_map['baseline'])
        
        # Validate required columns
        emp_required = ['employee_id', 'pickup_lat', 'pickup_lng', 'drop_lat', 'drop_lng']
        emp_missing = [col for col in emp_required if col not in emp_df.columns]
        if emp_missing:
            print(f"Error: Employees sheet is missing required columns: {', '.join(emp_missing)}")
            print(f"Available columns: {', '.join(emp_df.columns)}")
            return False
        
        veh_required = ['vehicle_id', 'current_lat', 'current_lng', 'capacity']
        veh_missing = [col for col in veh_required if col not in veh_df.columns]
        if veh_missing:
            print(f"Error: Vehicles sheet is missing required columns: {', '.join(veh_missing)}")
            print(f"Available columns: {', '.join(veh_df.columns)}")
            return False
        
    except Exception as e:
        print(f"Error reading Excel file: {str(e)}")
        import traceback
        traceback.print_exc()
        return False
    
    data = {}
    
    # Employees
    employees = []
    for idx, row in emp_df.iterrows():
        emp_id = _safe_str(row.get('employee_id', ''), '')
        
        # Skip rows with empty employee_id
        if not emp_id or emp_id == 'UNKNOWN':
            continue
            
        emp = {
            'employee_id': emp_id,
            'pickup_lat': _safe_float(row.get('pickup_lat', 0)),
            'pickup_lng': _safe_float(row.get('pickup_lng', 0)),
            'drop_lat': _safe_float(row.get('drop_lat', 0)),
            'drop_lng': _safe_float(row.get('drop_lng', 0)),
            'priority': _safe_int(row.get('priority', 3), 3),
            'earliest_pickup': _safe_str(row.get('earliest_pickup', '08:00'), '08:00'),
            'latest_drop': _safe_str(row.get('latest_drop', '18:00'), '18:00'),
            'vehicle_preference': _safe_str(row.get('vehicle_preference', 'any'), 'any').lower(),
            'sharing_preference': _safe_str(row.get('sharing_preference', 'triple'), 'triple').lower()
        }
        # Validate priority range
        if emp['priority'] < 1 or emp['priority'] > 5:
            print(f"Warning: Employee {emp['employee_id']} has invalid priority {emp['priority']}, clamping to 1-5")
            emp['priority'] = max(1, min(5, emp['priority']))
        employees.append(emp)
    data['employees'] = employees
    print(f"{len(employees)} employees")
    
    # Vehicles
    vehicles = []
    for idx, row in veh_df.iterrows():
        veh_id = _safe_str(row.get('vehicle_id', ''), '')
        
        # Skip rows with empty vehicle_id
        if not veh_id or veh_id == 'UNKNOWN':
            continue
            
        vehicles.append({
            'vehicle_id': veh_id,
            'current_lat': _safe_float(row.get('current_lat', 0)),
            'current_lng': _safe_float(row.get('current_lng', 0)),
            'capacity': _safe_int(row.get('capacity', 4), 4),
            'cost_per_km': _safe_float(row.get('cost_per_km', 10.0), 10.0),
            'avg_speed_kmph': _safe_float(row.get('avg_speed_kmph', 40.0), 40.0),
            'available_from': _safe_str(row.get('available_from', '08:00'), '08:00'),
            'category': _safe_str(row.get('category', 'normal'), 'normal').lower()
        })
    data['vehicles'] = vehicles
    print(f"{len(vehicles)} vehicles")
    
    # Metadata
    metadata = {}
    for _, row in meta_df.iterrows():
        key = _safe_str(row.get('key', ''))
        value = row.get('value', 0)
        # Handle different value types
        if 'weight' in key:
            metadata[key] = _safe_float(value, 0.5)
        elif 'delay' in key:
            metadata[key] = _safe_int(value, 10)
        else:
            if isinstance(value, float) and math.isnan(value):
                metadata[key] = 0
            else:
                metadata[key] = value
    data['metadata'] = metadata
    print(f"{len(metadata)} metadata entries")
    
    # Baseline
    baseline = []
    for _, row in base_df.iterrows():
        # baseline_time column may be named 'baseline_time' or 'baseline_time_min'
        baseline_time = _safe_float(row.get('baseline_time_min', 0))
        if baseline_time == 0:
            baseline_time = _safe_float(row.get('baseline_time', 0))
        baseline.append({
            'employee_id': _safe_str(row.get('employee_id', '')),
            'baseline_cost': _safe_float(row.get('baseline_cost', 0)),
            'baseline_time': baseline_time
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
