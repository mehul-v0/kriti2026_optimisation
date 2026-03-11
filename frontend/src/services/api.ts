/**
 * API Service Layer — connects frontend to Server2 (Flask backend on port 5000)
 */

// Use environment variable in production, fallback to /api for dev (proxied by Vite)
const API_BASE = import.meta.env.VITE_API_URL || '/api';

interface UploadResponse {
  success: boolean;
  message: string;
  filename: string;
  digest: {
    employees_count: number;
    vehicles_count: number;
    time_window_span: string;
    high_priority_percent: number;
    fleet_composition: { electric: number; petrol: number; diesel: number };
    vehicle_modes: { '2-wheeler': number; '4-wheeler': number; van: number };
  };
  employees: BackendEmployee[];
  vehicles: BackendVehicle[];
  baseline_cost: number;
  metadata?: {
    objective_cost_weight?: number;
    objective_time_weight?: number;
    priority_1_max_delay_min?: number;
    priority_2_max_delay_min?: number;
    priority_3_max_delay_min?: number;
    priority_4_max_delay_min?: number;
    priority_5_max_delay_min?: number;
    [key: string]: unknown;
  };
}

export interface ViolationDetails {
  capacity_violations: { vehicle: string; trip: number; passengers: number; capacity: number; employees: string }[];
  time_window_violations: { employee: string; vehicle: string; trip: number; office_arrival: string; deadline: string; delay_min: number }[];
  unassigned_employees: { employee: string }[];
  vehicle_pref_violations: { employee: string; vehicle: string; preferred: string; assigned: string }[];
  sharing_pref_violations: { employee: string; vehicle: string; trip: number; preferred: string; actual_riders: number }[];
  priority_delay_violations: { employee: string; vehicle: string; trip: number; priority: number; tolerance_min: number; actual_delay_min: number; within_tolerance: boolean }[];
}

interface OptimizeResponse {
  success: boolean;
  optimization_id?: string;  // ID for polling geometry updates
  result: {
    total_cost: number;
    baseline_cost: number;
    cost_savings: number;
    cost_savings_percent: number;
    total_distance: number;
    total_time: number;
    baseline_time: number;
    vehicles_used: number;
    vehicles_available: number;
    hard_violations: number;
    soft_violations: number;
  };
  routes: BackendRoute[];
  assignments: BackendAssignment[];
  violation_details?: ViolationDetails;
}

export interface BackendEmployee {
  employee_id: string;
  pickup_lat: number;
  pickup_lng: number;
  drop_lat: number;
  drop_lng: number;
  priority: number;
  earliest_pickup: string;
  latest_drop: string;
  vehicle_preference: string;
  sharing_preference: string;
}

export interface BackendVehicle {
  vehicle_id: string;
  current_lat: number;
  current_lng: number;
  capacity: number;
  cost_per_km: number;
  avg_speed_kmph: number;
  available_from: string;
  category: string;
}

export interface BackendRoute {
  vehicle_id: string;
  route_points: {
    lat: number;
    lng: number;
    type: 'pickup' | 'office';
    employee_id: string | null;
    trip_number: number;
    arrival_time?: string;
    departure_time?: string;
    distance_from_prev?: number;
    geometry?: [number, number][];  // Array of [lat, lng] coordinates for actual road route
  }[];
  total_distance: number;
  total_cost: number;
  passengers_count: number;
  capacity_utilization: number;
  trips_count: number;
  trip_costs?: Record<number, number>;  // Per-trip costs keyed by trip number
  trip_distances?: Record<number, number>;  // Per-trip distances keyed by trip number
}

export interface BackendAssignment {
  vehicle_id: string;
  employee_id: string;
  pickup_time: string;
  dropoff_time: string;
  sequence_order: number;
  is_pickup: boolean;
  trip_number: number;
}

/**
 * Health check
 */
export async function checkHealth(): Promise<boolean> {
  try {
    const res = await fetch(`${API_BASE}/health`);
    const data = await res.json();
    return data.status === 'ok';
  } catch {
    return false;
  }
}

/**
 * Upload Excel file to backend for conversion and preview
 */
export async function uploadFile(file: File): Promise<UploadResponse> {
  const formData = new FormData();
  formData.append('file', file);

  const res = await fetch(`${API_BASE}/upload`, {
    method: 'POST',
    body: formData,
  });

  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: 'Upload failed' }));
    throw new Error(err.error || 'Upload failed');
  }

  return res.json();
}

/**
 * Run optimization on previously uploaded data
 */
export async function runOptimization(): Promise<OptimizeResponse> {
  // Get optimization config from storage
  const configStr = sessionStorage.getItem('optimizationConfig');
  const solverDuration = sessionStorage.getItem('solverDuration');
  
  const payload: any = {};
  
  if (configStr) {
    try {
      const config = JSON.parse(configStr);
      payload.costWeight = config.costWeight;
      payload.timeWeight = config.timeWeight;
      payload.priorityDelays = config.priorityDelays;
      payload.distanceMethod = config.distanceMethod; // Add distance method setting
    } catch (e) {
      console.warn('Failed to parse optimization config:', e);
    }
  }
  
  if (solverDuration) {
    // Convert frontend duration labels to actual seconds for the solver
    const durationMap: Record<string, number> = { 
      Quick: 30,
      Standard: 60,
      Thorough: 120,
      Maximum: 300
    };
    payload.solverDurationSeconds = durationMap[solverDuration] || 30;
  }

  const res = await fetch(`${API_BASE}/optimize`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(payload),
  });

  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: 'Optimization failed' }));
    throw new Error(err.error || 'Optimization failed');
  }

  return res.json();
}

/**
 * Download solution JSON from backend
 */
export async function downloadSolution(): Promise<Blob> {
  const res = await fetch(`${API_BASE}/download-solution`);
  if (!res.ok) {
    throw new Error('No solution available for download');
  }
  return res.blob();
}

/**
 * Poll for geometry fetching status and get updated results
 */
export async function getGeometryStatus(optimizationId: string): Promise<{
  success: boolean;
  geometry_status: 'pending' | 'fetching' | 'complete' | 'not_needed';
  geometry_progress: { total: number; fetched: number };
  geometry_debug?: { routes_with_geometry: number; total_routes: number; points_with_geometry: number; total_points: number; ors_api_key_set: boolean };
  result: OptimizeResponse;
}> {
  const res = await fetch(`${API_BASE}/geometry-status/${optimizationId}`);
  if (!res.ok) {
    throw new Error('Failed to get geometry status');
  }
  return res.json();
}
