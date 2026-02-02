import type { EmployeeRequest, Vehicle, VehicleAssignment, Route, OptimizationResult } from '../types';

interface Point {
  lat: number;
  lng: number;
}

function calculateDistance(point1: Point, point2: Point): number {
  const R = 6371;
  const dLat = toRad(point2.lat - point1.lat);
  const dLng = toRad(point2.lng - point1.lng);
  const a =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos(toRad(point1.lat)) * Math.cos(toRad(point2.lat)) * Math.sin(dLng / 2) * Math.sin(dLng / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

function toRad(degrees: number): number {
  return (degrees * Math.PI) / 180;
}

function timeToMinutes(timeStr: string): number {
  const [hours, minutes] = timeStr.split(':').map(Number);
  return hours * 60 + minutes;
}

function minutesToTime(minutes: number): string {
  const hours = Math.floor(minutes / 60);
  const mins = Math.floor(minutes % 60);
  return `${String(hours).padStart(2, '0')}:${String(mins).padStart(2, '0')}:00`;
}

interface Assignment {
  vehicle: Vehicle;
  employees: EmployeeRequest[];
  route: Array<{
    lat: number;
    lng: number;
    type: 'start' | 'pickup' | 'dropoff' | 'end';
    employee_id?: string;
    time: number;
  }>;
  totalDistance: number;
  totalCost: number;
}

export class OptimizationEngine {
  private employees: EmployeeRequest[];
  private vehicles: Vehicle[];
  private scenarioId: string;

  constructor(employees: EmployeeRequest[], vehicles: Vehicle[], scenarioId: string) {
    this.employees = employees;
    this.vehicles = vehicles;
    this.scenarioId = scenarioId;
  }

  optimize(): {
    assignments: VehicleAssignment[];
    routes: Route[];
    result: OptimizationResult;
  } {
    const sortedEmployees = this.sortEmployeesByPriority();
    const assignments: Assignment[] = [];
    const assignedEmployees = new Set<string>();

    for (const employee of sortedEmployees) {
      let assigned = false;

      for (const assignment of assignments) {
        if (this.canAssignToVehicle(employee, assignment)) {
          assignment.employees.push(employee);
          this.updateRoute(assignment);
          assignedEmployees.add(employee.employee_id);
          assigned = true;
          break;
        }
      }

      if (!assigned) {
        const availableVehicle = this.findBestVehicle(employee, assignments);
        if (availableVehicle) {
          const newAssignment: Assignment = {
            vehicle: availableVehicle,
            employees: [employee],
            route: [],
            totalDistance: 0,
            totalCost: 0,
          };
          this.updateRoute(newAssignment);
          assignments.push(newAssignment);
          assignedEmployees.add(employee.employee_id);
        }
      }
    }

    const vehicleAssignments: VehicleAssignment[] = [];
    const routes: Route[] = [];
    let totalDistance = 0;
    let totalCost = 0;
    let totalTime = 0;

    assignments.forEach((assignment) => {
      assignment.route.forEach((point, index) => {
        if (point.employee_id) {
          vehicleAssignments.push({
            scenario_id: this.scenarioId,
            vehicle_id: assignment.vehicle.vehicle_id,
            employee_id: point.employee_id,
            pickup_time: minutesToTime(point.time),
            dropoff_time: minutesToTime(point.time + 30),
            sequence_order: index,
            is_pickup: point.type === 'pickup',
          });
        }
      });

      const routePoints = assignment.route.map((point) => ({
        lat: point.lat,
        lng: point.lng,
        type: point.type,
        employee_id: point.employee_id,
      }));

      routes.push({
        scenario_id: this.scenarioId,
        vehicle_id: assignment.vehicle.vehicle_id,
        route_points: routePoints,
        total_distance: assignment.totalDistance,
        total_cost: assignment.totalCost,
        passengers_count: assignment.employees.length,
        capacity_utilization: (assignment.employees.length / assignment.vehicle.capacity) * 100,
      });

      totalDistance += assignment.totalDistance;
      totalCost += assignment.totalCost;
      totalTime += assignment.totalDistance / assignment.vehicle.avg_speed;
    });

    const baselineCost = this.calculateBaselineCost();
    const costSavings = baselineCost - totalCost;
    const costSavingsPercent = (costSavings / baselineCost) * 100;

    const result: OptimizationResult = {
      scenario_id: this.scenarioId,
      total_cost: totalCost,
      baseline_cost: baselineCost,
      cost_savings: costSavings,
      cost_savings_percent: costSavingsPercent,
      total_distance: totalDistance,
      total_time: totalTime * 60,
      vehicles_used: assignments.length,
      vehicles_available: this.vehicles.length,
    };

    return { assignments: vehicleAssignments, routes, result };
  }

  private sortEmployeesByPriority(): EmployeeRequest[] {
    const priorityOrder = { high: 0, medium: 1, low: 2 };
    return [...this.employees].sort((a, b) => {
      return priorityOrder[a.priority] - priorityOrder[b.priority];
    });
  }

  private canAssignToVehicle(employee: EmployeeRequest, assignment: Assignment): boolean {
    if (assignment.employees.length >= assignment.vehicle.capacity) {
      return false;
    }

    const maxSharing = this.getMaxSharing(employee);
    if (assignment.employees.length >= maxSharing) {
      return false;
    }

    const vehicleMode = assignment.vehicle.vehicle_mode;
    if (employee.vehicle_preference === 'premium' && vehicleMode === '2-wheeler') {
      return false;
    }

    return true;
  }

  private getMaxSharing(employee: EmployeeRequest): number {
    switch (employee.sharing_preference) {
      case 'single':
        return 1;
      case 'double':
        return 2;
      case 'triple':
        return 3;
      default:
        return 2;
    }
  }

  private findBestVehicle(employee: EmployeeRequest, existingAssignments: Assignment[]): Vehicle | null {
    const usedVehicleIds = new Set(existingAssignments.map((a) => a.vehicle.vehicle_id));
    const availableVehicles = this.vehicles.filter((v) => !usedVehicleIds.has(v.vehicle_id));

    if (availableVehicles.length === 0) return null;

    let bestVehicle = availableVehicles[0];
    let minDistance = calculateDistance(
      { lat: bestVehicle.current_lat, lng: bestVehicle.current_lng },
      { lat: employee.pickup_lat, lng: employee.pickup_lng }
    );

    for (const vehicle of availableVehicles) {
      const distance = calculateDistance(
        { lat: vehicle.current_lat, lng: vehicle.current_lng },
        { lat: employee.pickup_lat, lng: employee.pickup_lng }
      );

      if (distance < minDistance) {
        minDistance = distance;
        bestVehicle = vehicle;
      }
    }

    return bestVehicle;
  }

  private updateRoute(assignment: Assignment): void {
    const vehicle = assignment.vehicle;
    const route: Assignment['route'] = [];

    route.push({
      lat: vehicle.current_lat,
      lng: vehicle.current_lng,
      type: 'start',
      time: timeToMinutes(vehicle.availability_time),
    });

    let currentTime = timeToMinutes(vehicle.availability_time);
    let currentLat = vehicle.current_lat;
    let currentLng = vehicle.current_lng;
    let totalDistance = 0;

    const pickupPoints = assignment.employees.map((emp) => ({
      lat: emp.pickup_lat,
      lng: emp.pickup_lng,
      employee_id: emp.employee_id,
      type: 'pickup' as const,
    }));

    pickupPoints.forEach((point) => {
      const distance = calculateDistance({ lat: currentLat, lng: currentLng }, { lat: point.lat, lng: point.lng });
      const travelTime = (distance / vehicle.avg_speed) * 60;
      currentTime += travelTime;
      totalDistance += distance;

      route.push({
        ...point,
        time: currentTime,
      });

      currentLat = point.lat;
      currentLng = point.lng;
    });

    const dropoffPoints = assignment.employees.map((emp) => ({
      lat: emp.destination_lat,
      lng: emp.destination_lng,
      employee_id: emp.employee_id,
      type: 'dropoff' as const,
    }));

    dropoffPoints.forEach((point) => {
      const distance = calculateDistance({ lat: currentLat, lng: currentLng }, { lat: point.lat, lng: point.lng });
      const travelTime = (distance / vehicle.avg_speed) * 60;
      currentTime += travelTime;
      totalDistance += distance;

      route.push({
        ...point,
        time: currentTime,
      });

      currentLat = point.lat;
      currentLng = point.lng;
    });

    assignment.route = route;
    assignment.totalDistance = totalDistance;
    assignment.totalCost = totalDistance * vehicle.cost_per_km;
  }

  private calculateBaselineCost(): number {
    const avgCostPerKm = 15;
    let totalBaselineCost = 0;

    this.employees.forEach((employee) => {
      const distance = calculateDistance(
        { lat: employee.pickup_lat, lng: employee.pickup_lng },
        { lat: employee.destination_lat, lng: employee.destination_lng }
      );
      totalBaselineCost += distance * avgCostPerKm;
    });

    return totalBaselineCost;
  }
}
