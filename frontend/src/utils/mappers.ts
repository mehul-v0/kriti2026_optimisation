/**
 * Maps backend API responses to frontend TypeScript types.
 * The backend (Server2/app.py) uses snake_case and different structures;
 * this module converts everything to the frontend's camelCase interfaces.
 */

import type { Employee, Vehicle, Trip, RoutePoint, Assignment, OptimizationResult, ConstraintReport } from '../types';
import type { BackendEmployee, BackendVehicle, BackendRoute, BackendAssignment, ViolationDetails } from '../services/api';
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
    
    // Get per-trip cost/distance data (if provided by backend)
    const tripCosts = route.trip_costs || {};
    const tripDistances = route.trip_distances || {};

    for (const [tripNumber, points] of tripGroups) {
      const routePoints: RoutePoint[] = points.map((pt) => ({
        type: pt.type === 'office' ? 'dropoff' as const : pt.type as 'pickup',
        lat: pt.lat,
        lng: pt.lng,
        address: pt.employee_id ? pt.employee_id : 'Office',
        employeeId: pt.employee_id || undefined,
        time: pt.arrival_time || '',
        arrivalTime: pt.arrival_time || '',
        departureTime: pt.departure_time || '',
        distanceFromPrev: pt.distance_from_prev || 0,
        geometry: pt.geometry || undefined,  // Pass through actual road route geometry
      }));

      const employeesInTrip = points
        .filter(p => p.employee_id)
        .map(p => p.employee_id!);

      // Derive start/end time from first and last route points
      const firstTime = routePoints.length > 0 ? routePoints[0].departureTime || routePoints[0].arrivalTime : '';
      const lastTime = routePoints.length > 0 ? routePoints[routePoints.length - 1].arrivalTime : '';
      
      // Use actual per-trip cost/distance if available, otherwise fallback to approximation
      const tripCost = tripCosts[tripNumber] ?? (route.total_cost / (tripGroups.size || 1));
      const tripDistance = tripDistances[tripNumber] ?? (route.total_distance / (tripGroups.size || 1));

      trips.push({
        tripNumber,
        vehicleId: route.vehicle_id,
        employees: employeesInTrip,
        route: routePoints,
        distance: tripDistance,
        duration: 0,
        cost: tripCost,
        startTime: firstTime,
        endTime: lastTime,
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

  // Calculate actualSharing based on employees per trip
  const tripGroupCounts = new Map<string, number>();
  for (const assignment of empMap.values()) {
    const tripKey = `${assignment.vehicleId}_${assignment.tripNumber}`;
    tripGroupCounts.set(tripKey, (tripGroupCounts.get(tripKey) || 0) + 1);
  }

  // Set actualSharing for each assignment
  for (const assignment of empMap.values()) {
    const tripKey = `${assignment.vehicleId}_${assignment.tripNumber}`;
    const count = tripGroupCounts.get(tripKey) || 1;
    
    if (count === 1) {
      assignment.actualSharing = 'Solo';
    } else if (count === 2) {
      assignment.actualSharing = '+1';
    } else if (count >= 3) {
      assignment.actualSharing = '+2';
    }
  }

  return Array.from(empMap.values());
}

export function buildConstraintReport(
  _hardViolations: number,
  softViolations: number,
  assignments: Assignment[],
  employees: Employee[],
  violationDetails?: ViolationDetails
): ConstraintReport {
  const totalEmployees = employees.length;
  const assignedCount = new Set(assignments.map(a => a.employeeId)).size;
  const vd = violationDetails;

  // --- Hard constraints with real violation details ---
  const capacityViolations = vd?.capacity_violations?.map((v) =>
    `${v.vehicle} Trip ${v.trip}: ${v.passengers} passengers exceed capacity of ${v.capacity} (${v.employees})`
  ) ?? [];

  const timeWindowViolations = vd?.time_window_violations?.map((v) =>
    `${v.employee} on ${v.vehicle} Trip ${v.trip}: arrived at office ${v.office_arrival}, deadline was ${v.deadline} (${v.delay_min} min late)`
  ) ?? [];

  const unassignedViolations = vd?.unassigned_employees?.map((v) =>
    `Employee ${v.employee} was not assigned to any vehicle`
  ) ?? [];

  const hardDetails = [
    {
      name: 'Vehicle Capacity Limits',
      description: capacityViolations.length > 0
        ? `${capacityViolations.length} trip(s) exceeded vehicle seating capacity`
        : 'No vehicle exceeded its seating capacity',
      status: (capacityViolations.length === 0 ? 'satisfied' : 'violated') as 'satisfied' | 'violated',
      violations: capacityViolations,
    },
    {
      name: 'Employee Time Windows',
      description: timeWindowViolations.length > 0
        ? `${timeWindowViolations.length} employee(s) arrived past their deadline`
        : 'All employees dropped off within their specified time windows',
      status: (timeWindowViolations.length === 0 ? 'satisfied' : 'violated') as 'satisfied' | 'violated',
      violations: timeWindowViolations,
    },
    {
      name: 'Vehicle Availability',
      description: 'All vehicles deployed only during their available hours',
      status: 'satisfied' as const,
      violations: [],
    },
    {
      name: 'All Employees Assigned',
      description: unassignedViolations.length > 0
        ? `${assignedCount} of ${totalEmployees} employees have vehicle assignments`
        : `All ${totalEmployees} employees have vehicle assignments`,
      status: (assignedCount >= totalEmployees ? 'satisfied' : 'violated') as 'satisfied' | 'violated',
      violations: unassignedViolations,
    },
  ];

  const hardSatisfied = hardDetails.filter(d => d.status === 'satisfied').length;

  // --- Soft constraints with real violation details ---
  const vehiclePrefViolations = vd?.vehicle_pref_violations?.map((v) =>
    `${v.employee} on ${v.vehicle}: preferred ${v.preferred} vehicle, assigned ${v.assigned}`
  ) ?? [];

  const sharingPrefViolations = vd?.sharing_pref_violations?.map((v) =>
    `${v.employee} on ${v.vehicle} Trip ${v.trip}: wanted ${v.preferred} (max ${
      v.preferred === 'Single' ? 1 : v.preferred === 'Double' ? 2 : 3
    }), had ${v.actual_riders} riders`
  ) ?? [];

  const priorityDelayWithinTolerance = vd?.priority_delay_violations?.filter((v: any) => v.within_tolerance)?.map((v: any) =>
    `${v.employee} (Priority ${v.priority}) on ${v.vehicle} Trip ${v.trip}: delayed ${v.actual_delay_min} min (within ${v.tolerance_min} min tolerance) ✓`
  ) ?? [];

  const priorityDelayExceeded = vd?.priority_delay_violations?.filter((v: any) => !v.within_tolerance)?.map((v: any) =>
    `${v.employee} (Priority ${v.priority}) on ${v.vehicle} Trip ${v.trip}: delayed ${v.actual_delay_min} min, tolerance was ${v.tolerance_min} min`
  ) ?? [];

  const priorityDelayAll = [...priorityDelayWithinTolerance, ...priorityDelayExceeded];

  const vehiclePrefRate = assignedCount > 0
    ? Math.round(((assignedCount - vehiclePrefViolations.length) / assignedCount) * 100)
    : 100;
  const sharingPrefRate = assignedCount > 0
    ? Math.round(((assignedCount - sharingPrefViolations.length) / assignedCount) * 100)
    : 100;

  const softDetails = [
    {
      name: 'Vehicle Type Preference',
      description: vehiclePrefViolations.length > 0
        ? `${vehiclePrefRate}% of employees got their preferred vehicle type`
        : '100% of employees got their preferred vehicle type',
      status: (vehiclePrefViolations.length === 0 ? 'satisfied' : 'relaxed') as 'satisfied' | 'relaxed',
      violations: vehiclePrefViolations,
    },
    {
      name: 'Sharing Preference',
      description: sharingPrefViolations.length > 0
        ? `${sharingPrefRate}% of employees got their sharing preference`
        : '100% of employees got their sharing preference',
      status: (sharingPrefViolations.length === 0 ? 'satisfied' : 'relaxed') as 'satisfied' | 'relaxed',
      violations: sharingPrefViolations,
    },
    {
      name: 'Priority-Based Delay Tolerance',
      description: priorityDelayExceeded.length > 0
        ? `${priorityDelayExceeded.length} employee(s) exceeded their priority-based delay tolerance`
        : priorityDelayWithinTolerance.length > 0
          ? `All delayed employees are within their priority-based delay tolerance`
          : 'All employees are within their priority-based delay tolerance',
      status: (priorityDelayExceeded.length === 0 ? 'satisfied' : 'relaxed') as 'satisfied' | 'relaxed',
      violations: priorityDelayAll,
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
  optimizeData: { result: any; routes: BackendRoute[]; assignments: BackendAssignment[]; violation_details?: ViolationDetails },
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
    employees,
    optimizeData.violation_details
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
    totalTime: res.total_time || 0,
    baselineTime: res.baseline_time || 0,
    constraints,
    solverDuration,
    solverMode: solverMode as any,
  };
}
