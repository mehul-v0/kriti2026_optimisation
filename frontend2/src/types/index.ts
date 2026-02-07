export interface Employee {
  id: string;
  priority: 'High' | 'Medium' | 'Low';
  pickupLocation: string;
  pickupLat: number;
  pickupLng: number;
  destination: string;
  destinationLat: number;
  destinationLng: number;
  timeWindowStart: string;
  timeWindowEnd: string;
  vehiclePreference: 'Premium' | 'Normal';
  sharingPreference: 'Single' | 'Double' | 'Triple';
  baselineCost: number;
}

export interface Vehicle {
  id: string;
  fuelType: 'Electric' | 'Petrol' | 'Diesel';
  mode: '2-Wheeler' | '4-Wheeler' | 'Van';
  capacity: number;
  costPerKm: number;
  currentLocation: string;
  currentLat: number;
  currentLng: number;
  availabilityTime: string;
}

export interface Trip {
  tripNumber: number;
  vehicleId: string;
  employees: string[];
  route: RoutePoint[];
  distance: number;
  duration: number;
  cost: number;
  startTime: string;
  endTime: string;
}

export interface RoutePoint {
  type: 'pickup' | 'dropoff' | 'office';
  lat: number;
  lng: number;
  address: string;
  employeeId?: string;
  time: string;
}

export interface Assignment {
  employeeId: string;
  vehicleId: string;
  tripNumber: number;
  pickupTime: string;
  dropoffTime: string;
  actualSharing: string;
  vehiclePreferenceMet: boolean;
  sharingPreferenceMet: boolean;
  timeWindowMet: boolean;
}

export interface OptimizationResult {
  sessionId: string;
  timestamp: string;
  inputFile: string;
  employees: Employee[];
  vehicles: Vehicle[];
  trips: Trip[];
  assignments: Assignment[];
  baselineCost: number;
  optimizedCost: number;
  savings: number;
  savingsPercentage: number;
  constraints: ConstraintReport;
  solverDuration: number;
  solverMode: 'Quick' | 'Standard' | 'Thorough' | 'Maximum';
}

export interface ConstraintReport {
  hard: ConstraintCategory;
  soft: ConstraintCategory;
}

export interface ConstraintCategory {
  total: number;
  satisfied: number;
  violated: number;
  complianceRate: number;
  details: ConstraintDetail[];
}

export interface ConstraintDetail {
  name: string;
  description: string;
  status: 'satisfied' | 'violated' | 'relaxed';
  violations: any[];
}

export interface SessionHistory {
  id: string;
  timestamp: string;
  employeeCount: number;
  vehiclesUsed: number;
  savings: number;
  status: 'completed' | 'failed';
  result?: OptimizationResult;
}

export interface LifetimeMetrics {
  totalOptimizations: number;
  cumulativeSavings: number;
  totalEmployees: number;
  totalKilometers: number;
}
