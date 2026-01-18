"""
KRITI 2026 - Complete OR-Tools VRP Solver
Uses ALL input fields:
- Employee: priority, pickup/drop locations, time windows, vehicle_preference, sharing_preference
- Vehicle: capacity, cost, speed, location, available_from, category (premium/normal)
- Metadata: priority delays, objective weights

Features:
- Time windows with latest_drop enforcement
- Vehicle-employee preference matching (premium/normal)
- Sharing preferences (single/double/triple) via capacity constraints
- Priority-based soft penalties for delays
- Multi-vehicle routing with heterogeneous fleet
"""

import math
import sys
import json
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def haversine_km(lat1, lon1, lat2, lon2):
    """Calculate distance between two coordinates in kilometers."""
    R = 6371.0
    phi1 = math.radians(float(lat1))
    phi2 = math.radians(float(lat2))
    dphi = math.radians(float(lat2) - float(lat1))
    dlambda = math.radians(float(lon2) - float(lon1))
    a = math.sin(dphi/2)**2 + math.cos(phi1)*math.cos(phi2)*math.sin(dlambda/2)**2
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))

def time_to_min(t):
    """Convert time string 'HH:MM' to minutes from midnight."""
    if t is None or t == '': 
        return 0
    if isinstance(t, str):
        parts = t.split(':')
        return int(parts[0]) * 60 + int(parts[1])
    if hasattr(t, 'hour'):
        return t.hour * 60 + t.minute
    return int(t)

def min_to_time(minutes):
    """Convert minutes from midnight to 'HH:MM' string."""
    h = int(minutes) // 60
    m = int(minutes) % 60
    return f"{h:02d}:{m:02d}"

# ============================================================================
# DATA MODEL CREATION
# ============================================================================

def create_data_model(employees, vehicles, metadata):
    """
    Create the data model for OR-Tools.
    
    Node indices:
    - 0: Office (common drop-off point / depot end)
    - 1 to N: Employee pickup locations
    - N+1 to N+M: Vehicle start locations
    """
    data = {}
    
    # Extract office location from first employee's drop
    office_lat = float(employees[0]['drop_lat'])
    office_lng = float(employees[0]['drop_lng'])
    data['office'] = (office_lat, office_lng)
    
    # Build location list
    locations = [(office_lat, office_lng)]  # Index 0 = Office
    
    # Add employee pickup locations (indices 1 to N)
    data['employee_indices'] = []
    for i, emp in enumerate(employees):
        locations.append((float(emp['pickup_lat']), float(emp['pickup_lng'])))
        data['employee_indices'].append(i + 1)
    
    # Add vehicle start locations (indices N+1 to N+M)
    data['vehicle_start_indices'] = []
    start_offset = len(locations)
    for i, veh in enumerate(vehicles):
        locations.append((float(veh['current_lat']), float(veh['current_lng'])))
        data['vehicle_start_indices'].append(start_offset + i)
    
    data['locations'] = locations
    data['num_locations'] = len(locations)
    data['num_employees'] = len(employees)
    data['num_physical_vehicles'] = len(vehicles)
    
    # ========== ALLOW MULTIPLE TRIPS PER VEHICLE ==========
    # Create virtual vehicles (each physical vehicle can make up to 3 trips)
    max_trips_per_vehicle = 3
    data['num_vehicles'] = len(vehicles) * max_trips_per_vehicle
    data['vehicle_to_physical'] = []  # Maps virtual vehicle index to physical vehicle index
    
    # ========== VEHICLE DATA ==========
    data['vehicle_capacities'] = []
    data['vehicle_costs'] = []
    data['vehicle_speeds'] = []
    data['vehicle_available_from'] = []
    data['vehicle_categories'] = []  # 0=any, 1=premium, 2=normal
    data['vehicle_ids'] = []
    
    for trip in range(max_trips_per_vehicle):
        for v_idx, veh in enumerate(vehicles):
            data['vehicle_to_physical'].append(v_idx)
            data['vehicle_capacities'].append(int(veh['capacity']))
            data['vehicle_costs'].append(float(veh['cost_per_km']))
            data['vehicle_speeds'].append(float(veh['avg_speed_kmph']))
            data['vehicle_available_from'].append(time_to_min(veh.get('available_from', '08:00')))
            data['vehicle_ids'].append(f"{veh['vehicle_id']}_trip{trip+1}")
            
            cat = veh.get('category', 'normal').lower()
            if cat == 'any':
                data['vehicle_categories'].append(0)
            elif cat == 'premium':
                data['vehicle_categories'].append(1)
            else:  # normal
                data['vehicle_categories'].append(2)
    
    # ========== EMPLOYEE DATA ==========
    data['employee_priorities'] = []
    data['employee_earliest_pickup'] = []
    data['employee_latest_drop'] = []
    data['employee_vehicle_pref'] = []  # 0=any, 1=premium, 2=normal
    data['employee_sharing_pref'] = []  # 1=single, 2=double, 3=triple
    data['employee_ids'] = []  # Store employee IDs for debugging/logging
    
    for emp in employees:
        data['employee_ids'].append(emp['employee_id'])
        data['employee_priorities'].append(int(emp.get('priority', 3)))
        data['employee_earliest_pickup'].append(time_to_min(emp.get('earliest_pickup', '08:00')))
        data['employee_latest_drop'].append(time_to_min(emp.get('latest_drop', '18:00')))
        
        vp = emp.get('vehicle_preference', 'any').lower()
        if vp == 'premium':
            data['employee_vehicle_pref'].append(1)
        elif vp == 'normal':
            data['employee_vehicle_pref'].append(2)
        else:
            data['employee_vehicle_pref'].append(0)
        
        sp = emp.get('sharing_preference', 'triple').lower()
        if sp == 'single':
            data['employee_sharing_pref'].append(1)
        elif sp == 'double':
            data['employee_sharing_pref'].append(2)
        else:
            data['employee_sharing_pref'].append(3)
    
    # ========== PRIORITY DELAY LIMITS ==========
    data['priority_max_delays'] = {
        1: int(metadata.get('priority_1_max_delay_min', 5)),
        2: int(metadata.get('priority_2_max_delay_min', 10)),
        3: int(metadata.get('priority_3_max_delay_min', 15)),
        4: int(metadata.get('priority_4_max_delay_min', 20)),
        5: int(metadata.get('priority_5_max_delay_min', 30)),
    }
    
    # ========== OBJECTIVE WEIGHTS (from sheet) ==========
    # Fetch cost_weight and time_weight from metadata sheet
    if 'objective_cost_weight' not in metadata or 'objective_time_weight' not in metadata:
        print("⚠️ Warning: objective_cost_weight or objective_time_weight not found in metadata sheet!")
        print("   Using default values: cost_weight=0.7, time_weight=0.3")
        data['cost_weight'] = 0.7
        data['time_weight'] = 0.3
    else:
        data['cost_weight'] = float(metadata['objective_cost_weight'])
        data['time_weight'] = float(metadata['objective_time_weight'])
        print(f"✓ Loaded from sheet: cost_weight={data['cost_weight']}, time_weight={data['time_weight']}")
    
    # ========== DISTANCE MATRIX ==========
    size = len(locations)
    dist_matrix = [[0] * size for _ in range(size)]
    for i in range(size):
        for j in range(size):
            if i != j:
                dist_matrix[i][j] = haversine_km(
                    locations[i][0], locations[i][1],
                    locations[j][0], locations[j][1]
                )
    data['distance_matrix'] = dist_matrix
    
    # Routing model indices - vehicles START from depot, END at OFFICE
    # Each trip: Depot -> Pickup employees -> Office (drop)
    # Next trip starts from office
    data['starts'] = data['vehicle_start_indices'] * max_trips_per_vehicle
    # All trips end at office (node 0) for drop-off
    data['ends'] = [0] * (len(vehicles) * max_trips_per_vehicle)  # Office is node 0
    
    # Add office as a required visit point for drop-offs (handled in constraints)
    data['office_node'] = 0
    
    return data

# ============================================================================
# OR-TOOLS SOLVER
# ============================================================================

def solve_ortools_full(employees, vehicles, baseline, metadata):
    """
    Solve VRP using Google OR-Tools with multi-stage constraint handling:
    
    Stage 1: Try to find solution with NO violations (all constraints satisfied)
    Stage 2: If Stage 1 fails, allow SOFT constraint violations (sharing/vehicle preferences)
    Stage 3: Return best available solution (even with hard violations)
    
    In all cases, minimize cost.
    """
    
    print("\n" + "=" * 60)
    print("MULTI-STAGE VRP SOLVER")
    print("=" * 60)
    
    best_result = None
    
    # ========== STAGE 1: Try with ALL constraints (hard + soft) ==========
    print("\n📋 STAGE 1: Attempting solution with ALL constraints satisfied...")
    result_stage1 = solve_with_constraints(employees, vehicles, baseline, metadata, 
                                           enforce_soft_constraints=True)
    
    if result_stage1:
        stats = result_stage1.get('stats', {})
        hard_v = stats.get('hard_violations', 0)
        soft_v = stats.get('soft_violations', 0)
        best_result = result_stage1  # Keep as best so far
        
        if hard_v == 0 and soft_v == 0:
            print("✅ STAGE 1 SUCCESS: Found optimal solution with NO constraint violations!")
            result_stage1['solution_type'] = 'OPTIMAL - No violations'
            return result_stage1
        else:
            print(f"   Stage 1 result: {hard_v} hard violations, {soft_v} soft violations")
    else:
        print("   Stage 1: No solution found by solver")
    
    # ========== STAGE 2: Try with only HARD constraints ==========
    print("\n📋 STAGE 2: Attempting solution with only HARD constraints (time windows)...")
    print("   (Allowing SOFT constraint violations: sharing/vehicle preferences)")
    result_stage2 = solve_with_constraints(employees, vehicles, baseline, metadata,
                                           enforce_soft_constraints=False)
    
    if result_stage2:
        stats = result_stage2.get('stats', {})
        hard_v = stats.get('hard_violations', 0)
        soft_v = stats.get('soft_violations', 0)
        
        if hard_v == 0:
            print(f"✅ STAGE 2 SUCCESS: Found solution with {soft_v} soft violations only!")
            result_stage2['solution_type'] = 'FEASIBLE - Soft violations only'
            return result_stage2
        else:
            print(f"   Stage 2 result: {hard_v} hard violations")
            # Keep this as best if it's better than Stage 1
            if best_result is None or hard_v < best_result.get('stats', {}).get('hard_violations', 999):
                best_result = result_stage2
    else:
        print("   Stage 2: No solution found by solver")
    
    # ========== STAGE 3: Return best available solution ==========
    if best_result:
        stats = best_result.get('stats', {})
        hard_v = stats.get('hard_violations', 0)
        soft_v = stats.get('soft_violations', 0)
        print(f"\n⚠️ STAGE 3: Returning BEST AVAILABLE solution ({hard_v} hard, {soft_v} soft violations)")
        best_result['solution_type'] = f'BEST AVAILABLE - {hard_v} hard, {soft_v} soft violations'
        return best_result
    
    # Fallback: return empty if absolutely nothing worked
    print("\n❌ No solution could be found at all")
    return {
        'assignment': {v['vehicle_id']: [] for v in vehicles},
        'score': 999.0,
        'stats': {'cost': 0, 'time': 0, 'penalty': 999999, 'hard_violations': 999, 'soft_violations': 0},
        'solution_type': 'NO SOLUTION FOUND',
        'details': [],
        'route_text': 'No solution could be generated.'
    }


def solve_with_constraints(employees, vehicles, baseline, metadata, enforce_soft_constraints=True):
    """
    Core solver function with configurable constraint enforcement.
    
    Args:
        enforce_soft_constraints: If True, enforce sharing/vehicle preferences as hard constraints
                                  If False, only enforce time windows as hard constraints
    """
    data = create_data_model(employees, vehicles, metadata)
    
    # Create Routing Index Manager
    manager = pywrapcp.RoutingIndexManager(
        data['num_locations'],
        data['num_vehicles'],
        data['starts'],
        data['ends']
    )
    
    # Create Routing Model
    routing = pywrapcp.RoutingModel(manager)
    
    # ========== 1. DISTANCE/COST CALLBACKS WITH PENALTY-BASED OBJECTIVE ==========
    # Priority: 1) Zero hard violations, 2) Minimize soft violations, 3) Minimize cost
    # We achieve this by adding MASSIVE costs for potential violations
    
    def distance_callback(from_index, to_index):
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        return int(data['distance_matrix'][from_node][to_node] * 1000)  # Scale to meters
    
    dist_callback_index = routing.RegisterTransitCallback(distance_callback)
    
    # Per-vehicle cost callbacks with violation penalties
    for v in range(data['num_vehicles']):
        cost_per_km = data['vehicle_costs'][v]
        
        def make_cost_callback(vehicle_idx, cost):
            def cost_callback(from_index, to_index):
                from_node = manager.IndexToNode(from_index)
                to_node = manager.IndexToNode(to_index)
                dist_km = data['distance_matrix'][from_node][to_index]
                
                # Base cost (kept small)
                base_cost = int(dist_km * cost * 100)  # Range: 0-1000 typically
                
                # Add MASSIVE penalties for potential soft constraint violations
                # Penalty scale: 100,000,000 per violation >> base cost
                # This ensures even ONE violation is worse than any cost savings
                penalty = 0
                
                # Check if we're picking up an employee with preferences
                if enforce_soft_constraints and from_node in data['employee_indices']:
                    emp_idx = data['employee_indices'].index(from_node)
                    
                    # Vehicle preference violation penalty: 100 MILLION
                    # 0=any matches everything (no penalty)
                    # 1=premium must match 1 or 0, 2=normal must match 2 or 0
                    emp_veh_pref = data['employee_vehicle_pref'][emp_idx]  # 0=any, 1=premium, 2=normal
                    vehicle_cat = data['vehicle_categories'][vehicle_idx]  # 0=any, 1=premium, 2=normal
                    
                    # Only penalize if there's a mismatch AND neither is "any"
                    if emp_veh_pref != 0 and vehicle_cat != 0 and emp_veh_pref != vehicle_cat:
                        penalty += 100000000  # 100M >> any travel cost
                
                return base_cost + penalty
            return cost_callback
        
        cb = make_cost_callback(v, cost_per_km)
        cb_index = routing.RegisterTransitCallback(cb)
        routing.SetArcCostEvaluatorOfVehicle(cb_index, v)
    
    # Add fixed cost per vehicle usage
    # In Stage 1: ZERO fixed cost to maximize vehicle usage and minimize sharing violations
    # In Stage 2/3: High fixed cost to minimize vehicle count for cost efficiency
    if enforce_soft_constraints:
        # ZERO fixed cost - maximally encourages using all available vehicles
        # This spreads employees across more trips, reducing sharing violations
        for v in range(data['num_vehicles']):
            routing.SetFixedCostOfVehicle(0, v)  # No penalty for vehicle usage
    else:
        # High fixed cost - consolidates trips for cost efficiency
        for v in range(data['num_vehicles']):
            routing.SetFixedCostOfVehicle(100000, v)  # Discourage extra vehicles

    
    # ========== 2. TIME DIMENSION (with vehicle-specific speeds) ==========
    time_callbacks = []
    for v in range(data['num_vehicles']):
        speed = data['vehicle_speeds'][v]
        
        def make_time_callback(vehicle_idx, vehicle_speed):
            def time_callback(from_index, to_index):
                from_node = manager.IndexToNode(from_index)
                to_node = manager.IndexToNode(to_index)
                dist_km = data['distance_matrix'][from_node][to_node]
                minutes = (dist_km / vehicle_speed) * 60
                return int(minutes + 0.5)  # Round to nearest minute
            return time_callback
        
        cb = make_time_callback(v, speed)
        cb_index = routing.RegisterTransitCallback(cb)
        time_callbacks.append(cb_index)
    
    # Add time dimension with vehicle-specific transit times
    routing.AddDimensionWithVehicleTransits(
        time_callbacks,
        60,      # Allow waiting time (slack) up to 60 minutes
        24 * 60, # Maximum time per vehicle (full day)
        False,   # Don't force start cumul to zero (vehicles have availability times)
        "Time"
    )
    time_dimension = routing.GetDimensionOrDie("Time")
    
    # Set vehicle start time constraints (available_from)
    for v in range(data['num_vehicles']):
        start_index = routing.Start(v)
        avail_from = data['vehicle_available_from'][v]
        time_dimension.CumulVar(start_index).SetRange(avail_from, 24 * 60)
    
    # Set employee time window constraints
    for i, emp_node in enumerate(data['employee_indices']):
        index = manager.NodeToIndex(emp_node)
        earliest = data['employee_earliest_pickup'][i]
        # For pickup, allow from earliest_pickup to a reasonable upper bound
        # The latest_drop will be enforced as a soft constraint on total ride time
        time_dimension.CumulVar(index).SetRange(earliest, 24 * 60)
    
    # ========== 3. CAPACITY DIMENSION ==========
    def demand_callback(from_index):
        from_node = manager.IndexToNode(from_index)
        if from_node in data['employee_indices']:
            return 1
        return 0
    
    demand_callback_index = routing.RegisterUnaryTransitCallback(demand_callback)
    
    # Adjust vehicle capacities based on employee sharing preferences
    effective_capacities = []
    for v in range(data['num_vehicles']):
        base_capacity = data['vehicle_capacities'][v]
        if enforce_soft_constraints:
            # When enforcing soft constraints, respect sharing preferences more strictly
            effective_capacities.append(min(base_capacity, 3))
        else:
            # When relaxing soft constraints, allow full vehicle capacity
            effective_capacities.append(base_capacity)
    
    routing.AddDimensionWithVehicleCapacity(
        demand_callback_index,
        0,  # No slack
        effective_capacities,
        True,  # Start cumul at zero
        "Capacity"
    )
    
    # ========== 4. VEHICLE-EMPLOYEE PREFERENCE ==========
    if enforce_soft_constraints:
        # STAGE 1: Enforce vehicle preferences as HARD constraints
        # Premium employees can ONLY use premium or "any" vehicles
        # Normal employees can ONLY use normal or "any" vehicles
        # "Any" preference employees can use any vehicle
        for emp_idx, emp_node in enumerate(data['employee_indices']):
            emp_pref = data['employee_vehicle_pref'][emp_idx]  # 0=any, 1=premium, 2=normal
            if emp_pref != 0:  # If employee has a specific preference (not "any")
                for v in range(data['num_vehicles']):
                    vehicle_cat = data['vehicle_categories'][v]  # 0=any, 1=premium, 2=normal
                    # Only block if there's a mismatch AND vehicle is not "any"
                    if vehicle_cat != 0 and emp_pref != vehicle_cat:
                        index = manager.NodeToIndex(emp_node)
                        routing.VehicleVar(index).RemoveValue(v)
    # Else: STAGE 2 - Don't enforce, allow any vehicle assignment
    
    # Store for post-validation
    data['vehicle_preferences'] = data['employee_vehicle_pref'].copy()
    data['enforce_soft_constraints'] = enforce_soft_constraints
    
    # ========== 5. SHARING PREFERENCE CONSTRAINTS ==========
    # Sharing preferences are handled in post-validation
    # Single: Employee prefers to be alone
    # Double: Employee prefers max 2 people
    # Triple: Employee prefers max 3 people
    # Violations will incur penalties in post-validation, not prevent solutions
    
    # Create groups of employees by sharing preference
    single_pref_employees = []
    double_pref_employees = []
    triple_pref_employees = []
    
    for emp_idx, emp_node in enumerate(data['employee_indices']):
        sharing_pref = data['employee_sharing_pref'][emp_idx]
        emp_index = manager.NodeToIndex(emp_node)
        if sharing_pref == 1:  # Single preference
            single_pref_employees.append((emp_idx, emp_node, emp_index))
        elif sharing_pref == 2:  # Double preference
            double_pref_employees.append((emp_idx, emp_node, emp_index))
        else:  # Triple preference
            triple_pref_employees.append((emp_idx, emp_node, emp_index))
    
    # ========== ENFORCE SHARING PREFERENCES (Stage 1 only) ==========
    if enforce_soft_constraints:
        # For SINGLE preference employees: They cannot share with ANY other employee
        # Add constraint: VehicleVar(single_emp) != VehicleVar(any_other_emp)
        for emp_idx_i, emp_node_i, index_i in single_pref_employees:
            solver = routing.solver()
            vehicle_i = routing.VehicleVar(index_i)
            
            # This single-preference employee cannot be on the same vehicle as ANY other employee
            for emp_idx_j, emp_node_j in enumerate(data['employee_indices']):
                if emp_idx_j == emp_idx_i:
                    continue
                index_j = manager.NodeToIndex(emp_node_j)
                vehicle_j = routing.VehicleVar(index_j)
                # Add constraint: vehicle_i != vehicle_j
                solver.Add(vehicle_i != vehicle_j)
        
        # For DOUBLE preference employees: They can share with at most 1 other person
        # This is harder to enforce directly, but we can ensure they don't share with
        # more than 1 person by making them incompatible with groups
        # For now, we'll handle double in post-validation with penalties
    
    # Store sharing data for post-solve validation
    data['single_pref_employees'] = [emp_idx for emp_idx, _, _ in single_pref_employees]
    data['double_pref_employees'] = [emp_idx for emp_idx, _, _ in double_pref_employees]
    
    # ========== 6. TIME WINDOW CONSTRAINTS (HARD with priority-based flexibility) ==========
    # Time windows are HARD constraints with only priority-based flexibility
    # Employee must be picked up at or after earliest_pickup
    # Employee must be dropped at office by latest_drop + priority_max_delay
    # 
    # IMPORTANT: latest_drop is when employee must arrive at OFFICE, not pickup time!
    # We need to calculate: latest_pickup = latest_drop - travel_time_to_office
    # But since travel time depends on which vehicle (speed), we use a conservative estimate
    
    # Use average vehicle speed to estimate travel time (conservative approach)
    avg_speed = sum(data['vehicle_speeds']) / len(data['vehicle_speeds']) if data['vehicle_speeds'] else 35
    
    for i, emp_node in enumerate(data['employee_indices']):
        index = manager.NodeToIndex(emp_node)
        earliest = data['employee_earliest_pickup'][i]
        latest_drop = data['employee_latest_drop'][i]
        priority = data['employee_priorities'][i]
        max_delay = data['priority_max_delays'].get(priority, 15)  # Get allowed delay based on priority
        
        # Calculate travel time from employee to office (node 0)
        dist_to_office = data['distance_matrix'][emp_node][0]
        travel_time_to_office = (dist_to_office / avg_speed) * 60  # minutes
        
        # IMPORTANT: The time window at the pickup node must account for travel to office
        # If employee must arrive at office by latest_drop + max_delay,
        # then they must be picked up by (latest_drop + max_delay - travel_time_to_office)
        # But this is conservative - actual multi-pickup routes take longer
        # So we'll set a looser constraint and validate in post-processing
        adjusted_latest_drop = latest_drop + max_delay
        latest_pickup = adjusted_latest_drop - travel_time_to_office
        
        # HARD constraint: pickup time window
        # Ensure the range is valid (latest >= earliest), otherwise use a looser upper bound
        if latest_pickup >= earliest:
            time_dimension.CumulVar(index).SetRange(earliest, int(latest_pickup))
        else:
            # Time window is infeasible with direct route - use a very loose upper bound
            # and rely on post-validation to catch violations
            time_dimension.CumulVar(index).SetRange(earliest, 24 * 60)
        
        # Store adjusted latest for validation
        data['employee_adjusted_latest'] = data.get('employee_adjusted_latest', [])
        if len(data['employee_adjusted_latest']) <= i:
            data['employee_adjusted_latest'].append(adjusted_latest_drop)
    
    # ========== 7. INCOMPATIBILITY CONSTRAINTS ==========
    # Prevent employees with incompatible time windows from being on the same trip
    # If employee A must arrive at office by time X, and employee B's earliest_pickup >= X,
    # then A and B CANNOT be on the same vehicle trip (A would be late waiting for B)
    
    incompatible_pairs = []
    
    for i, emp_i in enumerate(data['employee_indices']):
        # Employee i must arrive at office by their adjusted_latest_drop
        deadline_i = data['employee_adjusted_latest'][i]
        
        for j, emp_j in enumerate(data['employee_indices']):
            if i >= j:
                continue  # Skip self and already-checked pairs
            
            # Employee j's earliest pickup time
            earliest_j = data['employee_earliest_pickup'][j]
            deadline_j = data['employee_adjusted_latest'][j]
            earliest_i = data['employee_earliest_pickup'][i]
            
            # If employee i's deadline is BEFORE employee j's earliest pickup,
            # they cannot be on the same trip (i would be late waiting for j)
            if deadline_i < earliest_j:
                incompatible_pairs.append((i, j, f"{data['employee_ids'][i]} deadline {min_to_time(int(deadline_i))} < {data['employee_ids'][j]} earliest {min_to_time(earliest_j)}"))
            
            # Also check the reverse
            if deadline_j < earliest_i:
                incompatible_pairs.append((j, i, f"{data['employee_ids'][j]} deadline {min_to_time(int(deadline_j))} < {data['employee_ids'][i]} earliest {min_to_time(earliest_i)}"))
    
    # Add incompatibility constraints to the solver
    if incompatible_pairs:
        for i, j, reason in incompatible_pairs:
            # Use disjunctions to prevent these employees from being on the same vehicle
            # We add a constraint: for all vehicles v, if emp_i is on v, emp_j cannot be on v
            emp_i_node = data['employee_indices'][i]
            emp_j_node = data['employee_indices'][j]
            emp_i_index = manager.NodeToIndex(emp_i_node)
            emp_j_index = manager.NodeToIndex(emp_j_node)
            
            # Create a constraint that emp_i and emp_j cannot have the same vehicle
            # This is done by saying: VehicleVar(i) != VehicleVar(j) OR one of them is unassigned
            solver = routing.solver()
            vehicle_i = routing.VehicleVar(emp_i_index)
            vehicle_j = routing.VehicleVar(emp_j_index)
            
            # Add constraint: vehicle_i != vehicle_j
            solver.Add(vehicle_i != vehicle_j)
    
    # ========== 8. SEARCH PARAMETERS ==========
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    # Use PARALLEL_CHEAPEST_INSERTION for better constraint satisfaction
    # This builds routes in parallel considering all constraints, not just cost
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PARALLEL_CHEAPEST_INSERTION
    )
    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH
    )
    search_parameters.time_limit.seconds = 30  # More time to find constraint-satisfying solutions
    search_parameters.log_search = False
    
    # ========== 8. SOLVE ==========
    solution = routing.SolveWithParameters(search_parameters)
    
    if solution:
        return format_solution(data, manager, routing, solution, employees, vehicles, baseline, metadata)
    else:
        # Return empty assignment
        return {
            'assignment': {v['vehicle_id']: [] for v in vehicles},
            'score': 999.0,
            'stats': {'cost': 0, 'time': 0, 'penalty': 999999}
        }

# ============================================================================
# SOLUTION FORMATTING
# ============================================================================

def format_solution(data, manager, routing, solution, employees, vehicles, baseline, metadata):
    """Extract solution and return clean JSON for frontend display."""
    
    assignment = {v['vehicle_id']: [] for v in vehicles}
    physical_vehicle_trips = {v['vehicle_id']: [] for v in vehicles}
    
    time_dimension = routing.GetDimensionOrDie("Time")
    
    total_cost = 0.0
    total_time = 0.0
    total_penalty = 0.0
    hard_violations = 0  # Time window violations (HARD constraints)
    soft_violations = 0  # Sharing/vehicle preference violations (SOFT constraints)
    
    # Baseline for scoring
    baseline_cost = sum(float(b.get('baseline_cost', 0)) for b in baseline)
    baseline_time = sum(float(b.get('baseline_time_min', b.get('baseline_time', 0))) for b in baseline)
    
    route_details = []
    
    # First pass: collect all trips grouped by physical vehicle
    trips_by_physical_vehicle = {v['vehicle_id']: [] for v in vehicles}
    
    # Process each virtual vehicle
    for v in range(data['num_vehicles']):
        physical_idx = data['vehicle_to_physical'][v]
        physical_vehicle_id = vehicles[physical_idx]['vehicle_id']
        virtual_vehicle_id = data['vehicle_ids'][v]
        route_employees = []
        route_nodes = []
        route_dist = 0.0
        segment_distances = []
        
        index = routing.Start(v)
        while not routing.IsEnd(index):
            node = manager.IndexToNode(index)
            route_nodes.append(node)
            
            if node in data['employee_indices']:
                emp_idx = data['employee_indices'].index(node)
                emp = employees[emp_idx]
                route_employees.append(emp['employee_id'])
            
            next_index = solution.Value(routing.NextVar(index))
            next_node = manager.IndexToNode(next_index)
            segment_dist = data['distance_matrix'][node][next_node]
            segment_distances.append({
                'from_node': node,
                'to_node': next_node,
                'distance_km': round(segment_dist, 3)
            })
            route_dist += segment_dist
            index = next_index
        
        # Add end node
        route_nodes.append(manager.IndexToNode(index))
        
        # Only process if this virtual vehicle was actually used
        if route_employees:
            # Add to physical vehicle's assignment
            assignment[physical_vehicle_id].extend(route_employees)
            
            # Calculate cost
            vehicle_cost = route_dist * data['vehicle_costs'][v]
            total_cost += vehicle_cost
            
            # Build detailed node-by-node route with RAW timing (will be corrected later)
            raw_stops = []
            temp_index = routing.Start(v)
            stop_num = 0
            cumulative_dist = 0.0
            
            while not routing.IsEnd(temp_index):
                temp_node = manager.IndexToNode(temp_index)
                node_time = solution.Min(time_dimension.CumulVar(temp_index))
                
                # Determine stop type and label
                if temp_node in data['vehicle_start_indices']:
                    stop_type = 'depot'
                    vehicle_idx = data['vehicle_start_indices'].index(temp_node)
                    stop_label = f"D{physical_idx + 1:02d}"
                elif temp_node in data['employee_indices']:
                    stop_type = 'employee'
                    emp_idx = data['employee_indices'].index(temp_node)
                    emp_id = str(employees[emp_idx]['employee_id'])
                    # Avoid double E prefix (e.g., EE01 -> E01)
                    if emp_id.startswith('E') or emp_id.startswith('e'):
                        stop_label = emp_id
                    else:
                        stop_label = f"E{emp_id}"
                elif temp_node == 0:
                    stop_type = 'office'
                    stop_label = "Office"
                else:
                    stop_type = 'other'
                    stop_label = f"Node{temp_node}"
                
                # Get next node for distance
                next_index = solution.Value(routing.NextVar(temp_index))
                next_node = manager.IndexToNode(next_index)
                dist_to_next = data['distance_matrix'][temp_node][next_node]
                
                raw_stops.append({
                    'stop_number': stop_num,
                    'label': stop_label,
                    'type': stop_type,
                    'raw_time_minutes': node_time,
                    'distance_to_next': round(dist_to_next, 2),
                    'cumulative_distance': round(cumulative_dist, 2),
                    'node': temp_node
                })
                
                cumulative_dist += dist_to_next
                stop_num += 1
                temp_index = next_index
            
            # Add final stop (office or depot)
            final_node = manager.IndexToNode(temp_index)
            final_time = solution.Min(time_dimension.CumulVar(temp_index))
            if final_node == 0:
                final_label = "Office"
                final_type = 'office'
            else:
                final_label = f"D{physical_idx + 1:02d}"
                final_type = 'depot'
            
            raw_stops.append({
                'stop_number': stop_num,
                'label': final_label,
                'type': final_type,
                'raw_time_minutes': final_time,
                'distance_to_next': 0,
                'cumulative_distance': round(cumulative_dist, 2),
                'node': final_node
            })
            
            # Store trip info with raw stops (will process timing later)
            trips_by_physical_vehicle[physical_vehicle_id].append({
                'trip_id': virtual_vehicle_id,
                'employees': route_employees,
                'distance_km': round(route_dist, 2),
                'cost': round(vehicle_cost, 2),
                'segment_distances': segment_distances,
                'raw_stops': raw_stops,
                'physical_idx': physical_idx,
                'vehicle_speed': data['vehicle_speeds'][v]
            })
        
            # Calculate time for each employee (pickup to drop)
            drop_time = solution.Min(time_dimension.CumulVar(index))
            
            # Get pickup times
            temp_index = routing.Start(v)
            while not routing.IsEnd(temp_index):
                temp_node = manager.IndexToNode(temp_index)
                if temp_node in data['employee_indices']:
                    emp_idx = data['employee_indices'].index(temp_node)
                    pickup_time = solution.Min(time_dimension.CumulVar(temp_index))
                    ride_time = drop_time - pickup_time
                    total_time += ride_time
                    
                    # Check for late arrival penalty
                    latest_drop = data['employee_latest_drop'][emp_idx]
                    if drop_time > latest_drop:
                        delay = drop_time - latest_drop
                        priority = data['employee_priorities'][emp_idx]
                        max_delay = data['priority_max_delays'].get(priority, 15)
                        if delay > max_delay:
                            # HARD VIOLATION: exceeded even priority-based flexibility
                            hard_violations += 1
                            total_penalty += (delay - max_delay) * 50 * (6 - priority)
                
                temp_index = solution.Value(routing.NextVar(temp_index))
            
            # Validate sharing and vehicle preferences (SOFT - add penalty but allow)
            for emp_id in route_employees:
                emp_idx = next(i for i, e in enumerate(employees) if e['employee_id'] == emp_id)
                sharing_pref = data['employee_sharing_pref'][emp_idx]
                vehicle_pref = data['employee_vehicle_pref'][emp_idx]  # 0=any, 1=premium, 2=normal
                vehicle_cat = data['vehicle_categories'][v]  # 0=any, 1=premium, 2=normal
                
                # SOFT CONSTRAINT: Vehicle preference (penalty but allowed)
                # Only penalize if there's a mismatch AND neither is "any"
                if vehicle_pref != 0 and vehicle_cat != 0 and vehicle_pref != vehicle_cat:
                    veh_type = 'PREMIUM' if vehicle_cat == 1 else 'NORMAL'
                    pref_type = 'PREMIUM' if vehicle_pref == 1 else 'NORMAL'
                    print(f"  ⚠️ SOFT VIOLATION: Employee {emp_id} prefers {pref_type} vehicle but got {veh_type} ({physical_vehicle_id})")
                    soft_violations += 1
                    total_penalty += 100  # Small penalty for soft constraint
                
                # SOFT CONSTRAINT: Sharing preference (penalty but allowed)
                if sharing_pref == 1 and len(route_employees) > 1:
                    print(f"  ⚠️ SOFT VIOLATION: Employee {emp_id} prefers SINGLE ride but is sharing with {len(route_employees)-1} others")
                    soft_violations += 1
                    total_penalty += 200  # Moderate penalty for single preference violation
                elif sharing_pref == 2 and len(route_employees) > 2:
                    print(f"  ⚠️ SOFT VIOLATION: Employee {emp_id} prefers max DOUBLE (2) but {len(route_employees)} people in trip")
                    soft_violations += 1
                    total_penalty += 150  # Smaller penalty for double preference violation
                elif sharing_pref == 3 and len(route_employees) > 3:
                    print(f"  ⚠️ SOFT VIOLATION: Employee {emp_id} prefers max TRIPLE (3) but {len(route_employees)} people in trip")
                    soft_violations += 1
                    total_penalty += 100  # Small penalty for triple preference violation
    
    # ========== POST-PROCESS: Chain trip timings correctly ==========
    # For each physical vehicle, trips must be sequential:
    # - Trip 1 starts from depot at vehicle's available_from time
    # - Trip 2 starts from Office (where trip 1 ended) after trip 1 finishes
    # - And so on...
    # - Employees are picked up respecting their earliest_pickup time
    # - Employees must be dropped at office before their latest_drop time
    
    for v_idx, v in enumerate(vehicles):
        v_id = v['vehicle_id']
        trips = trips_by_physical_vehicle[v_id]
        
        if not trips:
            continue
            
        # Sort trips by the earliest_pickup time of their first employee
        # This ensures trips are ordered by when employees need to be picked up
        def get_trip_earliest_pickup(trip):
            if trip['raw_stops']:
                for stop in trip['raw_stops']:
                    if stop['type'] == 'employee':
                        emp_node = stop['node']
                        if emp_node in data['employee_indices']:
                            emp_idx = data['employee_indices'].index(emp_node)
                            return data['employee_earliest_pickup'][emp_idx]
            return 9999
        
        trips.sort(key=get_trip_earliest_pickup)
        
        # Now chain the trips with correct timing
        current_time = data['vehicle_available_from'][v_idx]  # Start at vehicle's available time
        current_location = 'depot'  # Start from depot
        vehicle_speed = trips[0]['vehicle_speed'] if trips else 40  # km/h
        
        for trip_idx, trip in enumerate(trips):
            detailed_stops = []
            raw_stops = trip['raw_stops']
            
            if not raw_stops:
                continue
            
            # For FIRST TRIP: Start from depot, check first employee's earliest_pickup
            if trip_idx == 0:
                # Find first employee and their time window
                first_employee_stop = None
                first_emp_idx = None
                depot_node = None
                
                for stop in raw_stops:
                    if stop['type'] == 'depot':
                        depot_node = stop['node']
                    if stop['type'] == 'employee' and first_employee_stop is None:
                        first_employee_stop = stop
                        emp_node = stop['node']
                        if emp_node in data['employee_indices']:
                            first_emp_idx = data['employee_indices'].index(emp_node)
                
                if first_employee_stop and depot_node is not None:
                    # Calculate travel time from depot to first employee
                    first_emp_node = first_employee_stop['node']
                    dist_to_first = data['distance_matrix'][depot_node][first_emp_node]
                    travel_time = (dist_to_first / vehicle_speed) * 60
                    
                    # Check if we need to delay start to arrive at earliest_pickup
                    arrival_at_first_emp = current_time + travel_time
                    if first_emp_idx is not None:
                        earliest_pickup = data['employee_earliest_pickup'][first_emp_idx]
                        if arrival_at_first_emp < earliest_pickup:
                            # Delay start so we arrive exactly at earliest_pickup
                            current_time = earliest_pickup - travel_time
            
            # If this is not the first trip, we start from Office (where last trip ended)
            elif trip_idx > 0:
                current_location = 'office'
                # First stop of this trip - calculate time from office to first employee
                first_employee_stop = None
                first_emp_idx = None
                for stop in raw_stops:
                    if stop['type'] == 'employee':
                        first_employee_stop = stop
                        # Find employee index to get their time window
                        emp_node = stop['node']
                        if emp_node in data['employee_indices']:
                            first_emp_idx = data['employee_indices'].index(emp_node)
                        break
                
                if first_employee_stop:
                    # Calculate travel time from office to first employee
                    office_node = 0
                    first_emp_node = first_employee_stop['node']
                    dist_to_first = data['distance_matrix'][office_node][first_emp_node]
                    travel_time = (dist_to_first / vehicle_speed) * 60  # Convert to minutes
                    
                    # Check if we need to wait for employee's earliest_pickup
                    arrival_at_first_emp = current_time + travel_time
                    if first_emp_idx is not None:
                        earliest_pickup = data['employee_earliest_pickup'][first_emp_idx]
                        if arrival_at_first_emp < earliest_pickup:
                            # We need to wait - adjust start time so we arrive at earliest_pickup
                            current_time = earliest_pickup - travel_time
                    
                    # Add "Start from Office" stop
                    detailed_stops.append({
                        'stop_number': 0,
                        'label': 'Office (Start)',
                        'type': 'office_start',
                        'time': min_to_time(int(current_time)),
                        'time_minutes': int(current_time),
                        'distance_to_next': round(dist_to_first, 2),
                        'cumulative_distance': 0
                    })
                    
                    current_time += travel_time
            
            # Process each stop with corrected timing
            stop_num = len(detailed_stops)
            cumulative_dist = detailed_stops[-1]['cumulative_distance'] if detailed_stops else 0
            
            for i, raw_stop in enumerate(raw_stops):
                # Skip depot start if we're starting from office (trip 2+)
                if trip_idx > 0 and i == 0 and raw_stop['type'] == 'depot':
                    continue
                
                # Calculate time for this stop
                if i == 0 and trip_idx == 0:
                    # First stop of first trip - use vehicle available time
                    stop_time = current_time
                else:
                    # Calculate based on travel from previous stop
                    stop_time = current_time
                
                # For employee stops, STRICTLY respect time windows with priority flexibility
                if raw_stop['type'] == 'employee':
                    emp_node = raw_stop['node']
                    if emp_node in data['employee_indices']:
                        emp_idx = data['employee_indices'].index(emp_node)
                        earliest_pickup = data['employee_earliest_pickup'][emp_idx]
                        latest_drop = data['employee_latest_drop'][emp_idx]
                        priority = data['employee_priorities'][emp_idx]
                        max_delay = data['priority_max_delays'].get(priority, 15)
                        adjusted_latest_drop = latest_drop + max_delay
                        
                        # HARD: If we arrive before earliest_pickup, we MUST wait
                        if stop_time < earliest_pickup:
                            stop_time = earliest_pickup
                        
                        # Store time window info
                        raw_stop['earliest_pickup'] = earliest_pickup
                        raw_stop['latest_drop'] = latest_drop
                        raw_stop['priority'] = priority
                        raw_stop['max_delay'] = max_delay
                        raw_stop['adjusted_latest_drop'] = adjusted_latest_drop
                        raw_stop['pickup_time'] = stop_time
                        
                        # Check pickup time compliance
                        if stop_time < earliest_pickup:
                            hard_violations += 1
                            total_penalty += 100000
                
                # Build stop info with time window details
                stop_info = {
                    'stop_number': stop_num,
                    'label': raw_stop['label'],
                    'type': raw_stop['type'],
                    'time': min_to_time(int(stop_time)),
                    'time_minutes': int(stop_time),
                    'distance_to_next': raw_stop['distance_to_next'],
                    'cumulative_distance': round(cumulative_dist, 2)
                }
                
                # Add time window info for employees (store both formatted and minutes values)
                if raw_stop['type'] == 'employee' and 'earliest_pickup' in raw_stop:
                    stop_info['earliest_pickup'] = min_to_time(raw_stop['earliest_pickup'])
                    stop_info['latest_drop'] = min_to_time(raw_stop['latest_drop'])
                    stop_info['latest_drop_minutes'] = raw_stop['latest_drop']  # Store minutes for validation
                    stop_info['adjusted_latest_minutes'] = raw_stop.get('adjusted_latest_drop', raw_stop['latest_drop'])
                    stop_info['priority'] = raw_stop.get('priority', 3)
                    stop_info['max_delay'] = raw_stop.get('max_delay', 15)
                    stop_info['time_window'] = f"[{min_to_time(raw_stop['earliest_pickup'])} - {min_to_time(raw_stop['latest_drop'])}]"
                    stop_info['adjusted_window'] = f"[{min_to_time(raw_stop['earliest_pickup'])} - {min_to_time(raw_stop.get('adjusted_latest_drop', raw_stop['latest_drop']))}]"
                
                detailed_stops.append(stop_info)
                
                # Update time and distance for next stop
                if raw_stop['distance_to_next'] > 0:
                    travel_time = (raw_stop['distance_to_next'] / vehicle_speed) * 60
                    current_time = stop_time + travel_time
                    cumulative_dist += raw_stop['distance_to_next']
                else:
                    current_time = stop_time
                
                stop_num += 1
            
            # Store the corrected detailed stops
            trip['detailed_stops'] = detailed_stops
            
            # IMPORTANT: Find the actual office arrival time (last stop of trip)
            actual_office_arrival = None
            for stop in reversed(detailed_stops):
                if stop['type'] == 'office' or stop['label'] == 'Office':
                    actual_office_arrival = stop['time_minutes']
                    break
            
            # Update employee stops with actual office arrival and validate
            if actual_office_arrival is not None:
                for stop in detailed_stops:
                    if stop['type'] == 'employee':
                        stop['est_office_arrival'] = min_to_time(int(actual_office_arrival))
                        stop['est_office_arrival_minutes'] = actual_office_arrival
                        
                        # Validate time windows
                        if 'latest_drop_minutes' in stop:
                            adjusted_latest = stop['adjusted_latest_minutes']
                            if actual_office_arrival > adjusted_latest:
                                hard_violations += 1
                                total_penalty += 100000
            
            # Update physical_vehicle_trips
            physical_vehicle_trips[v_id].append({
                'trip_id': trip['trip_id'],
                'employees': trip['employees'],
                'distance_km': trip['distance_km'],
                'cost': trip['cost'],
                'segment_distances': trip['segment_distances'],
                'detailed_stops': detailed_stops
            })
    
    # Build route_details from physical vehicle trips with corrected timings
    route_details = []
    for v_id in [v['vehicle_id'] for v in vehicles]:
        trips = physical_vehicle_trips[v_id]
        if trips:
            total_vehicle_cost = sum(t['cost'] for t in trips)
            all_employees = []
            trip_routes = []
            
            for trip_idx, t in enumerate(trips):
                all_employees.extend(t['employees'])
                
                trip_routes.append({
                    'trip_number': trip_idx + 1,
                    'employees': t['employees'],
                    'distance_km': t['distance_km'],
                    'cost': t['cost'],
                    'detailed_stops': t.get('detailed_stops', [])
                })
            
            route_details.append({
                'vehicle': v_id,
                'employees': all_employees,
                'trips': trips,
                'trip_routes': trip_routes,
                'num_trips': len(trips),
                'cost': round(total_vehicle_cost, 2)
            })
        else:
            route_details.append({
                'vehicle': v_id,
                'employees': [],
                'trips': [],
                'trip_routes': [],
                'num_trips': 0,
                'cost': 0.0
            })
    
    # Calculate final score
    norm_cost = total_cost / (baseline_cost + 1e-6)
    norm_time = total_time / (baseline_time + 1e-6)
    score = (data['cost_weight'] * norm_cost + data['time_weight'] * norm_time + total_penalty / 10000.0)
    
    # Summary
    print(f"\n✓ Solution: Cost=${total_cost:.2f}, Hard violations={hard_violations}, Soft violations={soft_violations}")
    
    return {
        'assignment': assignment,
        'score': round(score, 4),
        'stats': {
            'cost': round(total_cost, 2),
            'time': round(total_time, 2),
            'penalty': round(total_penalty, 2),
            'hard_violations': hard_violations,
            'soft_violations': soft_violations
        },
        'details': route_details
    }


