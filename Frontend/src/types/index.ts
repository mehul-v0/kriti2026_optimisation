export interface EmployeeRequest {
  id: string;
  scenario_id: string;
  employee_id: string;
  priority: 'high' | 'medium' | 'low';
  pickup_lat: number;
  pickup_lng: number;
  pickup_address: string;
  destination_lat: number;
  destination_lng: number;
  destination_address: string;
  time_window_start: string;
  time_window_end: string;
  vehicle_preference: 'premium' | 'normal';
  sharing_preference: 'single' | 'double' | 'triple';
}

export interface Vehicle {
  id: string;
  scenario_id: string;
  vehicle_id: string;
  fuel_type: 'petrol' | 'diesel' | 'electric';
  vehicle_mode: '2-wheeler' | '4-wheeler' | 'van';
  capacity: number;
  cost_per_km: number;
  avg_mileage: number;
  avg_speed: number;
  vehicle_age: number;
  current_lat: number;
  current_lng: number;
  current_address: string;
  availability_time: string;
}

export interface OptimizationResult {
  id?: string;
  scenario_id: string;
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
  completed_at?: string;
}

export interface VehicleAssignment {
  id?: string;
  scenario_id?: string;
  vehicle_id: string;
  employee_id: string;
  pickup_time: string;
  dropoff_time: string;
  sequence_order: number;
  is_pickup: boolean;
  trip_number?: number;
}

export interface Route {
  id?: string;
  scenario_id?: string;
  vehicle_id: string;
  route_points: Array<{ lat: number; lng: number; type: 'start' | 'pickup' | 'dropoff' | 'end' | 'office'; employee_id?: string; trip_number?: number }>;
  total_distance: number;
  total_cost: number;
  passengers_count: number;
  capacity_utilization: number;
  trips_count?: number;
}

export interface Scenario {
  id: string;
  name: string;
  description: string;
  created_at: string;
  status: 'pending' | 'processing' | 'completed' | 'failed';
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
