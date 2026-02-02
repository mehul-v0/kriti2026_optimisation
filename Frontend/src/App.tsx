import { useState } from 'react';
import { Play, Download, Eye, Users as UsersIcon, Upload } from 'lucide-react';
import { Header } from './components/Header';
import { DataDigestView } from './components/DataDigest';
import { KPIDashboard } from './components/KPIDashboard';
import { MapVisualization } from './components/MapVisualization';
import { VehiclePanel } from './components/VehiclePanel';
import { EmployeeView } from './components/EmployeeView';
import type {
  DataDigest,
  EmployeeRequest,
  Vehicle,
  OptimizationResult,
  VehicleAssignment,
  Route,
} from './types';

type AppState = 'initial' | 'data-loaded' | 'optimizing' | 'completed';
type ViewMode = 'fleet' | 'employee';

const API_BASE_URL = 'http://localhost:5000/api';

function App() {
  const [state, setState] = useState<AppState>('initial');
  const [viewMode, setViewMode] = useState<ViewMode>('fleet');
  const [showOptimized, setShowOptimized] = useState(false);
  const [digest, setDigest] = useState<DataDigest | null>(null);
  const [employees, setEmployees] = useState<EmployeeRequest[]>([]);
  const [vehicles, setVehicles] = useState<Vehicle[]>([]);
  const [result, setResult] = useState<OptimizationResult | null>(null);
  const [assignments, setAssignments] = useState<VehicleAssignment[]>([]);
  const [routes, setRoutes] = useState<Route[]>([]);
  const [optimizationLog, setOptimizationLog] = useState<string[]>([]);
  const [baselineCost, setBaselineCost] = useState<number>(0);
  const [uploadedFile, setUploadedFile] = useState<File | null>(null);

  const handleFileUpload = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    try {
      setState('initial');
      setOptimizationLog(['Uploading file...']);
      setUploadedFile(file);

      const formData = new FormData();
      formData.append('file', file);

      const response = await fetch(`${API_BASE_URL}/upload`, {
        method: 'POST',
        body: formData,
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error || 'Failed to upload file');
      }

      const data = await response.json();
      
      setOptimizationLog(['File uploaded successfully', 'Processing data...']);
      
      // Transform backend data to frontend format
      const transformedEmployees: EmployeeRequest[] = data.employees.map((emp: any) => ({
        id: emp.employee_id,
        scenario_id: 'backend',
        employee_id: emp.employee_id,
        priority: emp.priority === 1 ? 'high' : emp.priority === 2 ? 'medium' : 'low',
        pickup_lat: emp.pickup_lat,
        pickup_lng: emp.pickup_lng,
        pickup_address: emp.pickup_address || '',
        destination_lat: emp.drop_lat,
        destination_lng: emp.drop_lng,
        destination_address: emp.drop_address || '',
        time_window_start: emp.earliest_pickup,
        time_window_end: emp.latest_drop,
        vehicle_preference: emp.vehicle_preference || 'normal',
        sharing_preference: emp.sharing_preference || 'double',
      }));

      const transformedVehicles: Vehicle[] = data.vehicles.map((veh: any) => ({
        id: veh.vehicle_id,
        scenario_id: 'backend',
        vehicle_id: veh.vehicle_id,
        fuel_type: veh.category.toLowerCase() === 'electric' ? 'electric' : veh.category.toLowerCase() === 'premium' ? 'diesel' : 'petrol',
        vehicle_mode: veh.capacity <= 2 ? '2-wheeler' : veh.capacity <= 4 ? '4-wheeler' : 'van',
        capacity: veh.capacity,
        cost_per_km: veh.cost_per_km,
        avg_mileage: veh.mileage || 15,
        avg_speed: veh.speed || 30,
        vehicle_age: 0,
        current_lat: veh.start_lat,
        current_lng: veh.start_lng,
        current_address: veh.start_address || '',
        availability_time: veh.availability_time || '08:00',
      }));

      setEmployees(transformedEmployees);
      setVehicles(transformedVehicles);
      setDigest(data.digest);
      setBaselineCost(data.baseline_cost);
      setState('data-loaded');
      setOptimizationLog([
        'Data loaded successfully',
        `${transformedEmployees.length} employees loaded`,
        `${transformedVehicles.length} vehicles loaded`,
        `Baseline cost: ₹${data.baseline_cost.toFixed(0)}`,
      ]);
    } catch (error) {
      console.error('Error uploading file:', error);
      const errorMessage = error instanceof Error ? error.message : 'Failed to upload file';
      setOptimizationLog([
        'Error uploading file',
        errorMessage,
        'Please check that the backend server is running on http://localhost:5000'
      ]);
      setState('initial');
    }
  };

  const runOptimization = async () => {
    try {
      setState('optimizing');
      setOptimizationLog(['Starting optimization...', 'Running VRP solver...', 'This may take a few moments...']);

      const response = await fetch(`${API_BASE_URL}/optimize`, {
        method: 'POST',
      });

      if (!response.ok) {
        const error = await response.json();
        throw new Error(error.error || 'Optimization failed');
      }

      const data = await response.json();

      setOptimizationLog(['Optimization complete!', 'Processing results...']);

      // Set the optimization result
      setResult({
        scenario_id: 'backend',
        total_cost: data.result.total_cost,
        baseline_cost: data.result.baseline_cost,
        cost_savings: data.result.cost_savings,
        cost_savings_percent: data.result.cost_savings_percent,
        total_distance: data.result.total_distance,
        total_time: data.result.total_time,
        vehicles_used: data.result.vehicles_used,
        vehicles_available: data.result.vehicles_available,
        hard_violations: data.result.hard_violations || 0,
        soft_violations: data.result.soft_violations || 0,
      });

      setRoutes(data.routes);
      setAssignments(data.assignments);
      setState('completed');
      setShowOptimized(true);
      
      setOptimizationLog([
        'Optimization complete!',
        `Cost savings: ₹${data.result.cost_savings.toFixed(0)} (${data.result.cost_savings_percent.toFixed(1)}%)`,
        `Total distance: ${data.result.total_distance.toFixed(1)} km`,
        `Vehicles used: ${data.result.vehicles_used}/${data.result.vehicles_available}`,
        `Hard violations: ${data.result.hard_violations}`,
        `Soft violations: ${data.result.soft_violations}`,
      ]);
    } catch (error) {
      console.error('Error running optimization:', error);
      setOptimizationLog(['Error: ' + (error instanceof Error ? error.message : 'Optimization failed')]);
      setState('data-loaded');
    }
  };

  const resetAndUploadNew = () => {
    setState('initial');
    setDigest(null);
    setEmployees([]);
    setVehicles([]);
    setResult(null);
    setAssignments([]);
    setRoutes([]);
    setOptimizationLog([]);
    setBaselineCost(0);
    setUploadedFile(null);
    setShowOptimized(false);
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100">
      <Header />

      <div className="container mx-auto px-6 py-8">
        {state === 'initial' && (
          <div className="flex items-center justify-center h-64">
            <div className="text-center bg-white rounded-xl shadow-lg p-8 max-w-md mx-auto">
              <h2 className="text-2xl font-bold text-slate-800 mb-4">Upload Excel File</h2>
              <p className="text-slate-600 mb-6">
                Upload your Excel file containing employee and vehicle data to get started
              </p>
              <label className="cursor-pointer">
                <input
                  type="file"
                  accept=".xlsx,.xls"
                  onChange={handleFileUpload}
                  className="hidden"
                />
                <div className="bg-gradient-to-r from-emerald-500 to-emerald-600 text-white px-8 py-3 rounded-lg font-medium hover:from-emerald-600 hover:to-emerald-700 transition-all shadow-lg hover:shadow-xl inline-flex items-center space-x-2">
                  <Upload className="w-5 h-5" />
                  <span>Choose Excel File</span>
                </div>
              </label>
              {optimizationLog.length > 0 && (
                <div className="mt-4 space-y-2">
                  {optimizationLog.map((log, index) => (
                    <p key={index} className="text-sm text-slate-600">
                      {log}
                    </p>
                  ))}
                </div>
              )}
            </div>
          </div>
        )}

        {state === 'data-loaded' && digest && (
          <div className="space-y-6">
            <DataDigestView digest={digest} />

            <div className="bg-white rounded-xl shadow-lg p-6">
              <h2 className="text-xl font-bold text-slate-800 mb-4">Optimization Control</h2>
              <div className="flex items-center space-x-4">
                <button
                  onClick={runOptimization}
                  className="bg-gradient-to-r from-emerald-500 to-emerald-600 text-white px-8 py-3 rounded-lg font-medium hover:from-emerald-600 hover:to-emerald-700 transition-all shadow-lg hover:shadow-xl flex items-center space-x-2"
                >
                  <Play className="w-5 h-5" />
                  <span>Run Optimization</span>
                </button>
                <button
                  onClick={resetAndUploadNew}
                  className="bg-slate-200 text-slate-700 px-6 py-3 rounded-lg font-medium hover:bg-slate-300 transition-all flex items-center space-x-2"
                >
                  <Upload className="w-5 h-5" />
                  <span>Upload New File</span>
                </button>
              </div>
            </div>

            <MapVisualization routes={[]} employees={employees} showOptimized={false} />
          </div>
        )}

        {state === 'optimizing' && (
          <div className="space-y-6">
            {digest && <DataDigestView digest={digest} />}

            <div className="bg-white rounded-xl shadow-lg p-6">
              <h2 className="text-xl font-bold text-slate-800 mb-4">Optimization in Progress</h2>
              <div className="space-y-2">
                {optimizationLog.map((log, index) => (
                  <div key={index} className="flex items-center space-x-2">
                    <div className="w-2 h-2 bg-emerald-500 rounded-full animate-pulse"></div>
                    <p className="text-sm text-slate-600">{log}</p>
                  </div>
                ))}
              </div>
            </div>

            <MapVisualization routes={[]} employees={employees} showOptimized={false} />
          </div>
        )}

        {state === 'completed' && result && (
          <div className="space-y-6">
            <KPIDashboard result={result} />

            <MapVisualization routes={routes} employees={employees} showOptimized={showOptimized} />

            <div className="flex items-center justify-between bg-white rounded-xl shadow-lg p-4">
              <div className="flex items-center space-x-4">
                <button
                  onClick={() => setShowOptimized(!showOptimized)}
                  className={`px-4 py-2 rounded-lg font-medium transition-colors ${
                    showOptimized
                      ? 'bg-emerald-500 text-white'
                      : 'bg-slate-200 text-slate-700 hover:bg-slate-300'
                  }`}
                >
                  <Eye className="w-4 h-4 inline mr-2" />
                  {showOptimized ? 'Showing Optimized Routes' : 'Show Optimized Routes'}
                </button>
                <button
                  onClick={() => setViewMode(viewMode === 'fleet' ? 'employee' : 'fleet')}
                  className="px-4 py-2 rounded-lg font-medium bg-slate-200 text-slate-700 hover:bg-slate-300 transition-colors"
                >
                  <UsersIcon className="w-4 h-4 inline mr-2" />
                  {viewMode === 'fleet' ? 'Switch to Employee View' : 'Switch to Fleet View'}
                </button>
              </div>
              <button
                onClick={resetAndUploadNew}
                className="px-4 py-2 rounded-lg font-medium bg-blue-500 text-white hover:bg-blue-600 transition-colors"
              >
                <Upload className="w-4 h-4 inline mr-2" />
                Upload New File
              </button>
            </div>

            {viewMode === 'fleet' && (
              <VehiclePanel routes={routes} vehicles={vehicles} assignments={assignments} />
            )}

            {viewMode === 'employee' && <EmployeeView employees={employees} assignments={assignments} />}

            <div className="bg-white rounded-xl shadow-lg p-6">
              <h2 className="text-xl font-bold text-slate-800 border-b pb-3 mb-4">Constraint Validation</h2>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div className={`${result?.hard_violations === 0 ? 'bg-emerald-50 border-emerald-200' : 'bg-red-50 border-red-200'} border rounded-lg p-4`}>
                  <p className={`text-2xl font-bold ${result?.hard_violations === 0 ? 'text-emerald-700' : 'text-red-700'}`}>
                    {result?.hard_violations || 0}
                  </p>
                  <p className={`text-sm ${result?.hard_violations === 0 ? 'text-emerald-600' : 'text-red-600'}`}>
                    Hard Constraint Violations
                  </p>
                </div>
                <div className={`${result?.soft_violations === 0 ? 'bg-emerald-50 border-emerald-200' : 'bg-amber-50 border-amber-200'} border rounded-lg p-4`}>
                  <p className={`text-2xl font-bold ${result?.soft_violations === 0 ? 'text-emerald-700' : 'text-amber-700'}`}>
                    {result?.soft_violations || 0}
                  </p>
                  <p className={`text-sm ${result?.soft_violations === 0 ? 'text-emerald-600' : 'text-amber-600'}`}>
                    Soft Constraint Violations
                  </p>
                </div>
                <div className="bg-emerald-50 border border-emerald-200 rounded-lg p-4">
                  <p className="text-2xl font-bold text-emerald-700">
                    {((result?.vehicles_used || 0) / (result?.vehicles_available || 1) * 100).toFixed(0)}%
                  </p>
                  <p className="text-sm text-emerald-600">Fleet Utilization</p>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default App;
