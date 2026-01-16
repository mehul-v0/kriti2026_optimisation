import math
import sys
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp
import pandas as pd
from datetime import datetime, time

# Re-use helper logic for parsing or standard logic
# We need to read the same inputs.

def haversine_km(lat1, lon1, lat2, lon2):
    R = 6371.0
    phi1 = math.radians(float(lat1))
    phi2 = math.radians(float(lat2))
    dphi = math.radians(float(lat2) - float(lat1))
    dlambda = math.radians(float(lon2) - float(lon1))
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlambda/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def create_data_model(employees, vehicles, office_loc, config):
    data = {}
    
    # Locations: [Office] + [Employees...] + [Vehicle Starts...]
    # OR-Tools VRP expects an index for every node.
    # We have N employees.
    # Vehicles start at unique locations.
    # All vehicles end at Office.
    
    # Let's organize indices:
    # 0: Office (Depot match for End)
    # 1..n: Employees
    # n+1..n+m: Vehicle Start Locations (Dummy depots)
    
    data['office_idx'] = 0
    locs = [(office_loc['lat'], office_loc['lng'])] # Index 0
    
    data['employee_indices'] = []
    for i, e in enumerate(employees):
        locs.append((e['pickup_lat'], e['pickup_lng']))
        data['employee_indices'].append(i + 1)
        
    data['vehicle_start_indices'] = []
    # For vehicles, we need their start locations in the matrix
    # But in RoutingModel with different starts, we pass the index of the start node
    start_loc_offset = len(locs)
    for i, v in enumerate(vehicles):
        locs.append((v['current_lat'], v['current_lng']))
        data['vehicle_start_indices'].append(start_loc_offset + i)
        
    data['locations'] = locs
    data['num_vehicles'] = len(vehicles)
    data['vehicle_capacities'] = [int(v['capacity']) for v in vehicles]
    data['vehicle_costs'] = [float(v['cost_per_km']) for v in vehicles]
    data['vehicle_speeds'] = [float(v['avg_speed_kmph']) for v in vehicles]
    
    # Distance Matrix (km)
    size = len(locs)
    dist_matrix = [[0.0] * size for _ in range(size)]
    for i in range(size):
        for j in range(size):
            if i == j: continue
            dist_matrix[i][j] = haversine_km(locs[i][0], locs[i][1], locs[j][0], locs[j][1])
    data['distance_matrix'] = dist_matrix
    
    # Time Matrix (minutes)
    # Depends on vehicle speed. But matrix is uniform in model usually.
    # Multi-vehicle types with different speeds is supported by setting transit callback per vehicle.
    # We will build base distances and convert dynamically in callback.
    
    # Time Windows
    # Service time at pickup? Let's say 0.
    # Earliest/Latest for employees.
    # Office open forever?
    # Index 0 is office drop.
    data['time_windows'] = {}
    data['time_windows'][0] = (0, 24*60) # Office
    
    for i, idx in enumerate(data['employee_indices']):
        e = employees[i]
        ep = e.get('earliest_pickup')
        ld = e.get('latest_drop')
        
        start = 0
        end = 24*60
        
        if ep:
            start = time_to_min(ep)
        
        # Latest drop is at DESTINATION. 
        # But this node is the PICKUP location.
        # VRPTW usually imposes windows at the node visit.
        # "Earliest pickup" is a window on the pickup node [start, end].
        # "Latest drop" is a constraint on the arrival at the DROP node (Office).
        # Since everyone drops at Office (Index 0), we can't easily set individual max times for arrival at Index 0 per vehicle... 
        # Actually OR-Tools accumulates time.
        # We can enforce max ride time constraint on the dimension?
        # Or simplest: max END time for vehicle = min(latest_drop of all passengers)?
        
        data['time_windows'][idx] = (start, 24*60) # Relaxed upper bound for pickup node
        
        # We need to track latest drop separate?
        # Let's just store it for penalty calculation or `AddDimensionWithVehicleCapacity` type logic?
        # OR Tools 'Time' dimension accumulates.
        
    data['starts'] = data['vehicle_start_indices']
    data['ends'] = [0] * data['num_vehicles'] # All end at Office
    
    return data

def time_to_min(t):
    if t is None or t == '': return 0
    if isinstance(t, str):
        h, m = t.split(':')
        return int(h) * 60 + int(m)
    if hasattr(t, 'hour'):
        return t.hour * 60 + t.minute
    return t

def solve_ortools(employees, vehicles, baseline, metadata):
    # Prepare
    office = employees[0]
    office_loc = {'lat': office['drop_lat'], 'lng': office['drop_lng']}
    
    data = create_data_model(employees, vehicles, office_loc, metadata)
    
    manager = pywrapcp.RoutingIndexManager(len(data['locations']),
                                           data['num_vehicles'],
                                           data['starts'],
                                           data['ends'])

    routing = pywrapcp.RoutingModel(manager)

    # 1. Distance Callback & Dimension
    def distance_callback(from_index, to_index):
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        # return km distance
        return data['distance_matrix'][from_node][to_node]

    dist_callback_index = routing.RegisterTransitCallback(distance_callback)
    
    # We want to minimize Cost. Cost depends on Vehicle.
    # routing.SetArcCostEvaluatorOfAllVehicles(dist_callback_index) 
    # But costs differ per vehicle.
    
    for i in range(data['num_vehicles']):
        cost_per_km = data['vehicle_costs'][i]
        # internal cost must be integer? OR tools usually works with integers.
        # standard practice: scale up by 100 or 1000.
        def vehicle_cost_callback(from_index, to_index, v_idx=i):
            dist = distance_callback(from_index, to_index)
            return int(dist * data['vehicle_costs'][v_idx] * 100)
            
        idx = routing.RegisterTransitCallback(vehicle_cost_callback)
        routing.SetArcCostEvaluatorOfVehicle(idx, i)

    # 2. Time Dimension
    # time = dist / speed * 60
    # Different speeds.
    
    time_callback_indices = []
    for i in range(data['num_vehicles']):
        speed = data['vehicle_speeds'][i]
        def time_callback(from_index, to_index, v_idx=i):
            from_node = manager.IndexToNode(from_index)
            to_node = manager.IndexToNode(to_index)
            dist = data['distance_matrix'][from_node][to_node]
            minutes = (dist / data['vehicle_speeds'][v_idx]) * 60
            return int(minutes + 0.5) # round to int minutes
        
        t_idx = routing.RegisterTransitCallback(time_callback)
        time_callback_indices.append(t_idx)

    # Add Time Dimension
    # We need to pass a callback that works for a specific vehicle? 
    # AddDimensionWithVehicleTransits logic
    
    routing.AddDimensionWithVehicleTransits(
        time_callback_indices,
        3000,  # slack (wait time allowed at nodes)
        3000,  # capacity (max time per vehicle route)
        False, # fix_start_cumul_to_zero
        "Time"
    )
    time_dimension = routing.GetDimensionOrDie("Time")
    
    # Add Time Windows
    # vehicles start available_from
    for i, v in enumerate(vehicles):
        avail = time_to_min(v.get('available_from', '08:00'))
        index = routing.Start(i)
        time_dimension.CumulVar(index).SetRange(avail, 24*60)

    for i, idx in enumerate(data['employee_indices']):
        # Employees are nodes 1..n
        window = data['time_windows'][idx]
        index = manager.NodeToIndex(idx)
        time_dimension.CumulVar(index).SetRange(window[0], window[1])

    # 3. Capacity Dimension
    def demand_callback(from_index):
        from_node = manager.IndexToNode(from_index)
        # If it is an employee node, demand is 1. Else 0.
        if from_node in data['employee_indices']:
            return 1
        return 0

    demand_callback_index = routing.RegisterUnaryTransitCallback(demand_callback)
    routing.AddDimensionWithVehicleCapacity(
        demand_callback_index,
        0,  # null capacity slack
        data['vehicle_capacities'],  # vehicle maximum capacities
        True,  # start cumul to zero
        "Capacity")

    # 4. Search parameters
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)
    search_parameters.time_limit.seconds = 5

    # Solve
    solution = routing.SolveWithParameters(search_parameters)

    # Formatter
    if solution:
        return format_solution(data, manager, routing, solution, employees, vehicles, baseline, metadata)
    else:
        return None

def format_solution(data, manager, routing, solution, employees, vehicles, b, m):
    # Reconstruct assignment dict similar to python/C++ solver
    assignment = {}
    
    total_cost = 0
    total_time = 0
    penalty = 0 # Not tracked directly by OR tools unless we added soft constraints
    
    time_dimension = routing.GetDimensionOrDie("Time")
    
    # Baseline totals for score calculation
    baseline_total_cost = sum(float(x.get('baseline_cost', 0)) for x in b)
    baseline_total_time = sum(float(x.get('baseline_time_min', x.get('baseline_time', 0))) for x in b)

    for vehicle_id in range(data['num_vehicles']):
        index = routing.Start(vehicle_id)
        vid_str = vehicles[vehicle_id]['vehicle_id']
        route_employees = []
        
        # Track route to calc costs
        dist_km = 0
        nodes = []

        while not routing.IsEnd(index):
            node_index = manager.IndexToNode(index)
            nodes.append(node_index)
            
            # If it's an employee
            if node_index in data['employee_indices']:
                # map back to employee ID
                # data['employee_indices'] is a list where i-th element is node index for employee i
                e_idx = data['employee_indices'].index(node_index)
                emp = employees[e_idx]
                route_employees.append(emp['employee_id'])
                
                # Capture arrival/finish times
                # time_var = time_dimension.CumulVar(index)
                # arrival = solution.Min(time_var)
                
            previous_index = index
            index = solution.Value(routing.NextVar(index))
            
            # distance from prev to next
            dist_km += data['distance_matrix'][manager.IndexToNode(previous_index)][manager.IndexToNode(index)]

        assignment[vid_str] = route_employees
        
        # Vehicle Cost
        v_cost = dist_km * data['vehicle_costs'][vehicle_id]
        total_cost += v_cost

        # Calculate Employee Times (approx)
        # In OR Tools, we can get exact times.
        # Get Drop Time (Time at End Node)
        end_index = index # This is the End Node (Office)
        drop_time = solution.Min(time_dimension.CumulVar(end_index))
        
        # Get Pickup Times
        # Iterate again? or store during first pass.
        # Let's iterate nodes again to find pick times.
        t_index = routing.Start(vehicle_id)
        while not routing.IsEnd(t_index):
            node_index = manager.IndexToNode(t_index)
            if node_index in data['employee_indices']:
                pickup_time = solution.Min(time_dimension.CumulVar(t_index))
                ride = drop_time - pickup_time
                total_time += ride
            t_index = solution.Value(routing.NextVar(t_index))

    # Score
    # Same formula
    cost_w = float(m.get('objective_cost_weight', 0.6))
    time_w = float(m.get('objective_time_weight', 0.4))
    
    norm_cost = total_cost / (baseline_total_cost + 1e-6)
    norm_time = total_time / (baseline_total_time + 1e-6)
    
    score = cost_w * norm_cost + time_w * norm_time
    
    return {
        'assignment': assignment,
        'score': score,
        'stats': {
            'cost': total_cost,
            'time': total_time,
            'penalty': 0 # OR tools hard constraints satisfied = 0 penalty usually
        }
    }

if __name__ == "__main__":
    # If run standalone, parse args
    # ...
    pass
