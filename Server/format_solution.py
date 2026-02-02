"""
Format OR-Tools solution into clean JSON output format
Converts solver_ortools_full output to the desired structure
"""

def format_solution_output(ortools_result):
    """
    Convert solver_ortools_full result to clean JSON format.
    
    Args:
        ortools_result: Output from solver_ortools_full.solve_ortools_full() or other solvers
    
    Returns:
        Dict with formatted solution in desired structure
    """
    
    # Handle empty or failed solutions
    if not ortools_result:
        return create_empty_solution()
    
    # Detect if this is a C++ solver output (has 'assignment' dict but no 'details')
    if 'assignment' in ortools_result and 'details' not in ortools_result:
        return format_cpp_solver_output(ortools_result)
    
    # Handle OR-Tools format
    if 'details' not in ortools_result:
        return create_empty_solution()
    
    return format_ortools_output(ortools_result)


def create_empty_solution():
    """Return empty solution structure."""
    return {
        "score": 999999,
        "solution_type": "NO_SOLUTION",
        "stats": {
            "cost": 0,
            "time": 0,
            "hard_violations": 999,
            "soft_violations": 0
        },
        "total_cost": 0,
        "total_time": 0,
        "vehicles": []
    }


def format_cpp_solver_output(cpp_result):
    """
    Format C++ solver output (GA, ALNS) to standard format.
    C++ solvers output: {generation, score, assignment: {emp_id: veh_id}, stats: {cost, time, penalty}}
    """
    # For C++ solvers, we don't have detailed trip information
    # So we'll create a simplified structure
    
    stats = cpp_result.get('stats', {})
    assignment = cpp_result.get('assignment', {})
    
    # Group employees by vehicle
    vehicle_assignments = {}
    for emp_id, veh_id in assignment.items():
        if veh_id not in vehicle_assignments:
            vehicle_assignments[veh_id] = []
        vehicle_assignments[veh_id].append(emp_id)
    
    formatted = {
        "score": cpp_result.get('score', 999999),
        "solution_type": "GA/ALNS_SOLUTION",
        "stats": {
            "cost": stats.get('cost', 0),
            "time": stats.get('time', 0),
            "hard_violations": stats.get('hard_violations', 0),
            "soft_violations": stats.get('soft_violations', 0)
        },
        "total_cost": stats.get('cost', 0),
        "total_time": stats.get('time', 0),
        "vehicles": []
    }
    
    # Create simplified vehicle entries
    for veh_id, employees in vehicle_assignments.items():
        vehicle_output = {
            "vehicle_id": veh_id,
            "trips": [{
                "trip_number": 1,
                "stops": [],
                "total_cost": 0,
                "total_distance": 0,
                "total_time": 0,
                "employees": employees
            }],
            "total_cost": 0,
            "total_distance": 0,
            "total_time": 0
        }
        formatted['vehicles'].append(vehicle_output)
    
    return formatted


def format_ortools_output(ortools_result):
    """Format OR-Tools detailed output to standard format."""
    formatted_output = {
        "score": ortools_result.get('score', 0),
        "solution_type": ortools_result.get('solution_type', 'UNKNOWN'),
        "stats": {
            "cost": ortools_result['stats'].get('cost', 0),
            "time": ortools_result['stats'].get('time', 0),
            "hard_violations": ortools_result['stats'].get('hard_violations', 0),
            "soft_violations": ortools_result['stats'].get('soft_violations', 0)
        },
        "total_cost": ortools_result['stats'].get('cost', 0),
        "total_time": ortools_result['stats'].get('time', 0),
        "vehicles": []
    }
    
    # Process each vehicle's details
    for vehicle_data in ortools_result['details']:
        vehicle_id = vehicle_data['vehicle']
        vehicle_trips = vehicle_data.get('trip_routes', [])
        
        if not vehicle_trips:
            continue
        
        vehicle_output = {
            "vehicle_id": vehicle_id,
            "trips": [],
            "total_cost": vehicle_data.get('cost', 0),
            "total_distance": 0,
            "total_time": 0
        }
        
        # Process each trip
        for trip in vehicle_trips:
            trip_number = trip.get('trip_number', 1)
            detailed_stops = trip.get('detailed_stops', [])
            
            if not detailed_stops:
                continue
            
            trip_output = {
                "trip_number": trip_number,
                "stops": [],
                "total_cost": trip.get('cost', 0),
                "total_distance": trip.get('distance_km', 0),
                "total_time": 0
            }
            
            # Calculate trip time (last stop time - first stop time)
            if detailed_stops:
                first_time = detailed_stops[0].get('time_minutes', 0)
                last_time = detailed_stops[-1].get('time_minutes', 0)
                trip_output['total_time'] = last_time - first_time
            
            prev_time = None
            
            # Process each stop
            for i, stop in enumerate(detailed_stops):
                stop_time = stop.get('time', '00:00')
                stop_time_minutes = stop.get('time_minutes', 0)
                distance_to_next = stop.get('distance_to_next', 0)
                
                # Determine location label
                location = stop.get('label', 'Unknown')
                stop_type = stop.get('type', 'other')
                
                if stop_type == 'depot':
                    location = "Vehicle Depot"
                elif stop_type == 'office' or stop_type == 'office_start':
                    if i == 0:
                        location = "Office"
                    else:
                        location = "Office (Drop-off)"
                elif stop_type == 'employee':
                    location = f"{location} Pickup"
                
                # Calculate wait time (time between arrival at this stop and departure)
                # For simplicity, assume no wait time unless it's the first employee pickup
                wait_time = 0
                if i > 0 and prev_time is not None:
                    # Check if we arrived early (before earliest pickup time)
                    if 'earliest_pickup' in stop and stop_type == 'employee':
                        earliest = stop.get('time_minutes', 0)
                        # Wait time would be handled in the solver, reflected in stop time
                        pass
                
                # Distance from previous stop
                distance_from_prev = 0.0
                if i > 0:
                    distance_from_prev = detailed_stops[i-1].get('distance_to_next', 0)
                
                stop_output = {
                    "location": location,
                    "arrival_time": stop_time,
                    "departure_time": stop_time,  # Same as arrival unless waiting
                    "distance_from_prev": distance_from_prev,
                    "wait_time": wait_time
                }
                
                trip_output['stops'].append(stop_output)
                prev_time = stop_time_minutes
            
            vehicle_output['trips'].append(trip_output)
            vehicle_output['total_distance'] += trip_output['total_distance']
            vehicle_output['total_time'] += trip_output['total_time']
        
        formatted_output['vehicles'].append(vehicle_output)
    
    return formatted_output


def save_formatted_solution(ortools_result, output_file='formatted_solution.json'):
    """
    Format solution and save to JSON file.
    
    Args:
        ortools_result: Output from solver_ortools_full
        output_file: Path to save formatted JSON
    """
    import json
    
    formatted = format_solution_output(ortools_result)
    
    with open(output_file, 'w') as f:
        json.dump(formatted, f, indent=2)
    
    print(f"✓ Formatted solution saved to {output_file}")
    return formatted


if __name__ == '__main__':
    # Test with sample data
    import sys
    import json
    from solver import parse_excel_file
    import solver_ortools_full
    
    if len(sys.argv) < 2:
        print("Usage: python format_solution.py <excel_file>")
        print("Example: python format_solution.py TestCase_TC03.xlsx")
        sys.exit(1)
    
    data_file = sys.argv[1]
    
    print(f"Loading data from {data_file}...")
    data = parse_excel_file(data_file)
    
    # Build metadata
    meta_list = data.get('metadataa', data.get('metadata', []))
    metadata = {m['key']: m['value'] for m in meta_list}
    
    print("Running OR-Tools solver...")
    result = solver_ortools_full.solve_ortools_full(
        data['employees'],
        data['vehicles'],
        data['baseline'],
        metadata
    )
    
    print("\nFormatting solution...")
    formatted = save_formatted_solution(result, 'formatted_solution.json')
    
    print("\n" + "="*60)
    print("FORMATTED SOLUTION SUMMARY")
    print("="*60)
    print(f"Score: {formatted['score']:.2f}")
    print(f"Solution Type: {formatted['solution_type']}")
    print(f"Total Cost: ${formatted['total_cost']:.2f}")
    print(f"Total Time: {formatted['total_time']:.0f} minutes")
    print(f"Hard Violations: {formatted['stats']['hard_violations']}")
    print(f"Soft Violations: {formatted['stats']['soft_violations']}")
    print(f"\nVehicles Used: {len(formatted['vehicles'])}")
    
    for vehicle in formatted['vehicles']:
        print(f"\n  {vehicle['vehicle_id']}: {len(vehicle['trips'])} trip(s)")
        print(f"    Cost: ${vehicle['total_cost']:.2f}")
        print(f"    Distance: {vehicle['total_distance']:.2f} km")
        print(f"    Time: {vehicle['total_time']:.0f} min")
