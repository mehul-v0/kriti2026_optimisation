/**
 * API Service Layer — connects frontend to Server2 (Flask backend on port 5000)
 */

const API_BASE = '/api';

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
}

interface OptimizeResponse {
  success: boolean;
  result: {
    total_cost: number;
    baseline_cost: number;
    cost_savings: number;
    cost_savings_percent: number;
    total_distance: number;
    total_time: number;
    vehicles_used: number;
    vehicles_available: number;
    hard_violations: number;
    soft_violations: number;
  };
  routes: BackendRoute[];
  assignments: BackendAssignment[];
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
  }[];
  total_distance: number;
  total_cost: number;
  passengers_count: number;
  capacity_utilization: number;
  trips_count: number;
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
  const res = await fetch(`${API_BASE}/optimize`, {
    method: 'POST',
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
