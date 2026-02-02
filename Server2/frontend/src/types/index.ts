export interface EmployeeRequest {
  employee_id: string;
  priority: number;
  pickup_lat: number;
  pickup_lng: number;
  drop_lat: number;
  drop_lng: number;
  earliest_pickup: string;
  latest_drop: string;
  vehicle_preference: string;
  sharing_preference: string;
}

export interface Vehicle {
  vehicle_id: string;
  current_lat: number;
  current_lng: number;
  capacity: number;
  cost_per_km: number;
  avg_speed_kmph: number;
  available_from: string;
  category: string;
}

export interface OptimizationResult {
  total_cost: number;
  baseline_cost: number;
  cost_savings: number;
  cost_savings_percent: number;
  total_distance: number;
  total_time: number;
  vehicles_used: number;
  vehicles_available: number;
  hard_violations?: number;
  soft_violations?: number;
}

export interface VehicleAssignment {
  vehicle_id: string;
  employee_id: string;
  pickup_time: string;
  dropoff_time: string;
  sequence_order: number;
  is_pickup: boolean;
}

export interface Route {
  vehicle_id: string;
  route_points: Array<{ lat: number; lng: number; type: 'start' | 'pickup' | 'dropoff' | 'end'; employee_id?: string }>;
  total_distance: number;
  total_cost: number;
  passengers_count: number;
  capacity_utilization: number;
}

export interface DataDigest {
  employees_count: number;
  vehicles_count: number;
  time_window_span: string;
  high_priority_percent: number;
  fleet_composition: {
    electric: number;
    petrol: number;
    diesel: number;
  };
  vehicle_modes: {
    '2-wheeler': number;
    '4-wheeler': number;
    'van': number;
  };
}
