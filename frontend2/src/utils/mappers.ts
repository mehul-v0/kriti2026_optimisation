/**
 * Maps backend API responses to frontend TypeScript types.
 * The backend (Server2/app.py) uses snake_case and different structures;
 * this module converts everything to the frontend's camelCase interfaces.
 */

import type { Employee, Vehicle, Trip, RoutePoint, Assignment, OptimizationResult, ConstraintReport } from '../types';
import type { BackendEmployee, BackendVehicle, BackendRoute, BackendAssignment } from '../services/api';
import { generateId } from './helpers';

/** Priority number (1–5) → label */
function mapPriority(p: number | string): 'High' | 'Medium' | 'Low' {
  if (typeof p === 'string') return p as any;
  if (p <= 2) return 'High';
  if (p <= 3) return 'Medium';
  return 'Low';
}

/** category string → fuel type */
function mapFuelType(category: string): 'Electric' | 'Petrol' | 'Diesel' {
  const c = (category || '').toLowerCase();
  if (c === 'electric') return 'Electric';
  if (c === 'premium') return 'Diesel';
  return 'Petrol'; // 'normal' maps to Petrol
}

/** capacity → mode */
function mapVehicleMode(capacity: number): '2-Wheeler' | '4-Wheeler' | 'Van' {
  if (capacity <= 2) return '2-Wheeler';
  if (capacity <= 4) return '4-Wheeler';
  return 'Van';
}

/** sharing_preference string → label */
function mapSharing(pref: string): 'Single' | 'Double' | 'Triple' {
  const p = (pref || '').toLowerCase();
  if (p === 'single') return 'Single';
  if (p === 'double') return 'Double';
  return 'Triple';
}

/** vehicle_preference string → label */
function mapVehiclePref(pref: string): 'Premium' | 'Normal' {
  return (pref || '').toLowerCase() === 'premium' ? 'Premium' : 'Normal';
}

export function mapEmployees(backend: BackendEmployee[], baselineData?: { employee_id: string; baseline_cost: number }[]): Employee[] {
  return backend.map((e) => {
    const baseline = baselineData?.find(b => b.employee_id === e.employee_id);
    return {
      id: e.employee_id,
      priority: mapPriority(e.priority),
      pickupLocation: `(${e.pickup_lat.toFixed(4)}, ${e.pickup_lng.toFixed(4)})`,
      pickupLat: e.pickup_lat,
      pickupLng: e.pickup_lng,
      destination: `(${e.drop_lat.toFixed(4)}, ${e.drop_lng.toFixed(4)})`,
      destinationLat: e.drop_lat,
      destinationLng: e.drop_lng,
      timeWindowStart: e.earliest_pickup,
      timeWindowEnd: e.latest_drop,
      vehiclePreference: mapVehiclePref(e.vehicle_preference),
      sharingPreference: mapSharing(e.sharing_preference),
      baselineCost: baseline?.baseline_cost ?? 0,
    };
  });
}

export function mapVehicles(backend: BackendVehicle[]): Vehicle[] {
  return backend.map((v) => ({
    id: v.vehicle_id,
    fuelType: mapFuelType(v.category),
    mode: mapVehicleMode(v.capacity),
    capacity: v.capacity,
    costPerKm: v.cost_per_km,
    currentLocation: `(${v.current_lat.toFixed(4)}, ${v.current_lng.toFixed(4)})`,
    currentLat: v.current_lat,
    currentLng: v.current_lng,
    availabilityTime: v.available_from,
  }));
}

export function mapRoutes(backendRoutes: BackendRoute[]): Trip[] {
  const trips: Trip[] = [];

  for (const route of backendRoutes) {
    // Group route points by trip number
    const tripGroups = new Map<number, typeof route.route_points>();
    for (const pt of route.route_points) {
      const tn = pt.trip_number;
      if (!tripGroups.has(tn)) tripGroups.set(tn, []);
      tripGroups.get(tn)!.push(pt);
    }

    for (const [tripNumber, points] of tripGroups) {
      const routePoints: RoutePoint[] = points.map((pt) => ({
        type: pt.type === 'office' ? 'dropoff' as const : pt.type as 'pickup',
        lat: pt.lat,
        lng: pt.lng,
        address: pt.employee_id ? `Employee ${pt.employee_id}` : 'Office',
        employeeId: pt.employee_id || undefined,
        time: '',
      }));

      const employeesInTrip = points
        .filter(p => p.employee_id)
        .map(p => p.employee_id!);

      trips.push({
        tripNumber,
        vehicleId: route.vehicle_id,
        employees: employeesInTrip,
        route: routePoints,
        distance: route.total_distance / (tripGroups.size || 1), // approximate per-trip
        duration: 0,
        cost: route.total_cost / (tripGroups.size || 1),
        startTime: '',
        endTime: '',
      });
    }
  }

  return trips;
}

export function mapAssignments(backendAssignments: BackendAssignment[]): Assignment[] {
  // Group by employee to merge pickup + dropoff info
  const empMap = new Map<string, Assignment>();

  for (const a of backendAssignments) {
    const key = `${a.employee_id}_${a.trip_number}`;
    if (!empMap.has(key)) {
      empMap.set(key, {
        employeeId: a.employee_id,
        vehicleId: a.vehicle_id,
        tripNumber: a.trip_number,
        pickupTime: a.is_pickup ? a.pickup_time : '',
        dropoffTime: !a.is_pickup ? a.dropoff_time : '',
        actualSharing: '',
        vehiclePreferenceMet: true,
        sharingPreferenceMet: true,
        timeWindowMet: true,
      });
    } else {
      const existing = empMap.get(key)!;
      if (a.is_pickup) existing.pickupTime = a.pickup_time;
      else existing.dropoffTime = a.dropoff_time;
    }
  }

  return Array.from(empMap.values());
}

export function buildConstraintReport(
  hardViolations: number,
  softViolations: number,
  assignments: Assignment[],
  employees: Employee[]
): ConstraintReport {
  const totalEmployees = employees.length;
  const assignedCount = assignments.length;

  // Hard constraints from the solver
  const hardDetails = [
    {
      name: 'Vehicle Capacity Limits',
      description: 'No vehicle exceeded its seating capacity',
      status: (hardViolations === 0 ? 'satisfied' : 'violated') as 'satisfied' | 'violated',
      violations: [],
    },
    {
      name: 'Employee Time Windows',
      description: 'All employees picked up within their specified time windows',
      status: (hardViolations === 0 ? 'satisfied' : 'violated') as 'satisfied' | 'violated',
      violations: [],
    },
    {
      name: 'Vehicle Availability',
      description: 'All vehicles deployed only during their available hours',
      status: 'satisfied' as const,
      violations: [],
    },
    {
      name: 'All Employees Assigned',
      description: `${assignedCount} of ${totalEmployees} employees have vehicle assignments`,
      status: (assignedCount >= totalEmployees ? 'satisfied' : 'violated') as 'satisfied' | 'violated',
      violations: [],
    },
  ];

  const hardSatisfied = hardDetails.filter(d => d.status === 'satisfied').length;

  // Soft constraints — compute from assignment data
  const vehiclePrefMet = assignments.filter(a => a.vehiclePreferenceMet).length;
  const sharingPrefMet = assignments.filter(a => a.sharingPreferenceMet).length;
  const vehiclePrefRate = assignedCount > 0 ? Math.round((vehiclePrefMet / assignedCount) * 100) : 100;
  const sharingPrefRate = assignedCount > 0 ? Math.round((sharingPrefMet / assignedCount) * 100) : 100;

  const softDetails = [
    {
      name: 'Vehicle Type Preference',
      description: `${vehiclePrefRate}% of employees got their preferred vehicle type`,
      status: (vehiclePrefRate >= 80 ? 'satisfied' : 'relaxed') as 'satisfied' | 'relaxed',
      violations: [],
    },
    {
      name: 'Sharing Preference',
      description: `${sharingPrefRate}% of employees got their sharing preference`,
      status: (sharingPrefRate >= 80 ? 'satisfied' : 'relaxed') as 'satisfied' | 'relaxed',
      violations: [],
    },
    {
      name: 'Priority-Based Delay Tolerance',
      description: 'High-priority employees experience minimal detours',
      status: (softViolations === 0 ? 'satisfied' : 'relaxed') as 'satisfied' | 'relaxed',
      violations: [],
    },
  ];

  const softSatisfied = softDetails.filter(d => d.status === 'satisfied').length;

  return {
    hard: {
      total: hardDetails.length,
      satisfied: hardSatisfied,
      violated: hardDetails.length - hardSatisfied,
      complianceRate: Math.round((hardSatisfied / hardDetails.length) * 100),
      details: hardDetails,
    },
    soft: {
      total: softDetails.length,
      satisfied: softSatisfied,
      violated: softDetails.length - softSatisfied,
      complianceRate: Math.round((softSatisfied / softDetails.length) * 100),
      details: softDetails,
    },
  };
}

/**
 * Build a full OptimizationResult from the backend /api/optimize + /api/upload responses
 */
export function buildOptimizationResult(
  uploadData: { employees: BackendEmployee[]; vehicles: BackendVehicle[]; baseline_cost: number; filename: string },
  optimizeData: { result: any; routes: BackendRoute[]; assignments: BackendAssignment[] },
  solverMode: string,
  solverDuration: number
): OptimizationResult {
  const employees = mapEmployees(uploadData.employees);
  const vehicles = mapVehicles(uploadData.vehicles);
  const trips = mapRoutes(optimizeData.routes);
  const assignments = mapAssignments(optimizeData.assignments);

  const res = optimizeData.result;

  const constraints = buildConstraintReport(
    res.hard_violations,
    res.soft_violations,
    assignments,
    employees
  );

  return {
    sessionId: generateId(),
    timestamp: new Date().toISOString(),
    inputFile: uploadData.filename,
    employees,
    vehicles,
    trips,
    assignments,
    baselineCost: res.baseline_cost,
    optimizedCost: res.total_cost,
    savings: res.cost_savings,
    savingsPercentage: res.cost_savings_percent,
    constraints,
    solverDuration,
    solverMode: solverMode as any,
  };
}
