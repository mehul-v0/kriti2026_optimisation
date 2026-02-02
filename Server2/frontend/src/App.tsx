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

type AppState = 'upload' | 'data-loaded' | 'optimizing' | 'completed';
type ViewMode = 'fleet' | 'employee';

function App() {
  const [state, setState] = useState<AppState>('upload');
  const [viewMode, setViewMode] = useState<ViewMode>('fleet');
  const [showOptimized, setShowOptimized] = useState(false);
  const [digest, setDigest] = useState<DataDigest | null>(null);
  const [employees, setEmployees] = useState<EmployeeRequest[]>([]);
  const [vehicles, setVehicles] = useState<Vehicle[]>([]);
  const [result, setResult] = useState<OptimizationResult | null>(null);
  const [assignments, setAssignments] = useState<VehicleAssignment[]>([]);
  const [routes, setRoutes] = useState<Route[]>([]);
  const [optimizationLog, setOptimizationLog] = useState<string[]>([]);
  const [isUploading, setIsUploading] = useState(false);
  const [fileName, setFileName] = useState<string>('');

  const handleFileUpload = async (file: File) => {
    setIsUploading(true);
    setOptimizationLog(['Uploading file...']);
    setState('upload');
    
    const formData = new FormData();
    formData.append('file', file);

    try {
      const response = await fetch('/api/upload', {
        method: 'POST',
        body: formData,
      });

      const data = await response.json();

      if (!response.ok) {
        throw new Error(data.error || 'Upload failed');
      }

      setOptimizationLog((prev) => [...prev, 'Processing Excel file...', 'Parsing sheets...']);
      
      await new Promise(resolve => setTimeout(resolve, 500));

      setFileName(data.filename);
      setDigest(data.digest);
      setEmployees(data.employees);
      setVehicles(data.vehicles);
      setOptimizationLog((prev) => [
        ...prev,
        'File parsed successfully!',
        `✓ ${data.digest.employees_count} employees loaded`,
        `✓ ${data.digest.vehicles_count} vehicles loaded`,
        `✓ Baseline cost: ₹${data.baseline_cost.toFixed(0)}`,
      ]);
    } catch (error) {
      console.error('Upload error:', error);
      setOptimizationLog([
        `❌ Error: ${error instanceof Error ? error.message : 'Upload failed'}`,
      ]);
      setTimeout(() => {
        setOptimizationLog([]);
        setFileName('');
      }, 3000);
    } finally {
      setIsUploading(false);
    }
  };

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      handleFileUpload(file);
    }
  };

  const handleDrop = (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault();
    const file = event.dataTransfer.files[0];
    if (file) {
      handleFileUpload(file);
    }
  };

  const runOptimization = async () => {
    try {
      setState('optimizing');
      setOptimizationLog(['Starting optimization...', 'Loading constraints...', 'Running solver...']);

      const response = await fetch('/api/optimize', {
        method: 'POST',
      });

      const data = await response.json();

      if (!response.ok) {
        throw new Error(data.error || 'Optimization failed');
      }

      setOptimizationLog((prev) => [
        ...prev,
        'Solver completed successfully',
        'Processing results...',
      ]);

      setResult(data.result);
      setRoutes(data.routes);
      setAssignments(data.assignments);
      setState('completed');
      setShowOptimized(true);
      setOptimizationLog((prev) => [
        ...prev,
        'Optimization complete!',
        `${data.result.cost_savings_percent.toFixed(1)}% cost savings achieved`,
        `Cost reduced from ₹${data.result.baseline_cost.toFixed(0)} to ₹${data.result.total_cost.toFixed(0)}`,
      ]);
    } catch (error) {
      console.error('Optimization error:', error);
      setOptimizationLog((prev) => [
        ...prev,
        `Error: ${error instanceof Error ? error.message : 'Optimization failed'}`,
      ]);
      setState('data-loaded');
    }
  };

  const resetApp = () => {
    setState('upload');
    setViewMode('fleet');
    setShowOptimized(false);
    setDigest(null);
    setEmployees([]);
    setVehicles([]);
    setResult(null);
    setAssignments([]);
    setRoutes([]);
    setOptimizationLog([]);
    setFileName('');
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100">
      <Header />

      <div className="container mx-auto px-6 py-8">
        {state === 'upload' && (
          <div className="max-w-4xl mx-auto">
            <div className="bg-white rounded-xl shadow-lg p-8">
              <h2 className="text-2xl font-bold text-slate-800 mb-2">Upload Excel File</h2>
              <p className="text-slate-600 mb-6">Upload your test case Excel file to begin optimization</p>

              <div
                className="border-3 border-dashed border-slate-300 rounded-xl p-12 text-center hover:border-emerald-400 transition-all cursor-pointer bg-slate-50 hover:bg-slate-100"
                onDrop={handleDrop}
                onDragOver={(e) => e.preventDefault()}
                onClick={() => document.getElementById('fileInput')?.click()}
              >
                <Upload className="w-16 h-16 mx-auto mb-4 text-slate-400" />
                <p className="text-lg font-medium text-slate-700 mb-2">
                  Drag & drop your Excel file here
                </p>
                <p className="text-sm text-slate-500">or click to browse</p>
                <p className="text-xs text-slate-400 mt-4">Supports .xlsx and .xls files</p>
              </div>

              <input
                id="fileInput"
                type="file"
                accept=".xlsx,.xls"
                onChange={handleFileSelect}
                className="hidden"
              />

              {isUploading && (
                <div className="mt-6 bg-blue-50 border border-blue-200 rounded-lg p-4">
                  <div className="flex items-center space-x-3">
                    <div className="animate-spin rounded-full h-5 w-5 border-b-2 border-blue-600"></div>
                    <div className="flex-1">
                      {optimizationLog.map((log, index) => (
                        <p key={index} className="text-sm text-blue-700">{log}</p>
                      ))}
                    </div>
                  </div>
                </div>
              )}

              {fileName && digest && !isUploading && state === 'upload' && (
                <div className="mt-6 space-y-4 animate-fadeIn">
                  <div className="bg-emerald-50 border-l-4 border-emerald-500 rounded-lg p-4">
                    <div className="flex items-center space-x-3 mb-3">
                      <div className="bg-emerald-500 rounded-full p-2">
                        <svg className="w-5 h-5 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                        </svg>
                      </div>
                      <div>
                        <p className="font-bold text-emerald-900">File Uploaded Successfully!</p>
                        <p className="text-sm text-emerald-700">{fileName}</p>
                      </div>
                    </div>
                  </div>

                  <div className="bg-gradient-to-br from-slate-50 to-slate-100 rounded-xl p-6 border border-slate-200">
                    <h3 className="text-lg font-bold text-slate-800 mb-4 flex items-center">
                      <svg className="w-5 h-5 mr-2 text-blue-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
                      </svg>
                      Parsed Data Summary
                    </h3>

                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
                      <div className="bg-white rounded-lg p-4 shadow-sm border border-blue-100">
                        <div className="flex items-center space-x-2 mb-2">
                          <UsersIcon className="w-5 h-5 text-blue-600" />
                          <p className="text-xs font-medium text-slate-600">Employees</p>
                        </div>
                        <p className="text-3xl font-bold text-blue-900">{digest.employees_count}</p>
                      </div>

                      <div className="bg-white rounded-lg p-4 shadow-sm border border-emerald-100">
                        <div className="flex items-center space-x-2 mb-2">
                          <svg className="w-5 h-5 text-emerald-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
                          </svg>
                          <p className="text-xs font-medium text-slate-600">Vehicles</p>
                        </div>
                        <p className="text-3xl font-bold text-emerald-900">{digest.vehicles_count}</p>
                      </div>

                      <div className="bg-white rounded-lg p-4 shadow-sm border border-purple-100">
                        <div className="flex items-center space-x-2 mb-2">
                          <svg className="w-5 h-5 text-purple-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 7h8m0 0v8m0-8l-8 8-4-4-6 6" />
                          </svg>
                          <p className="text-xs font-medium text-slate-600">High Priority</p>
                        </div>
                        <p className="text-3xl font-bold text-purple-900">{digest.high_priority_percent.toFixed(0)}%</p>
                      </div>

                      <div className="bg-white rounded-lg p-4 shadow-sm border border-amber-100">
                        <div className="flex items-center space-x-2 mb-2">
                          <svg className="w-5 h-5 text-amber-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" />
                          </svg>
                          <p className="text-xs font-medium text-slate-600">Time Window</p>
                        </div>
                        <p className="text-sm font-bold text-amber-900">{digest.time_window_span}</p>
                      </div>
                    </div>

                    <div className="grid grid-cols-2 gap-4">
                      <div className="bg-white rounded-lg p-3 shadow-sm border border-slate-200">
                        <p className="text-xs font-medium text-slate-600 mb-2">Fleet Composition</p>
                        <div className="flex items-center justify-between text-xs">
                          <span className="text-emerald-700">⚡ Electric: {digest.fleet_composition.electric}</span>
                          <span className="text-blue-700">⛽ Petrol: {digest.fleet_composition.petrol}</span>
                          <span className="text-amber-700">🛢️ Diesel: {digest.fleet_composition.diesel}</span>
                        </div>
                      </div>

                      <div className="bg-white rounded-lg p-3 shadow-sm border border-slate-200">
                        <p className="text-xs font-medium text-slate-600 mb-2">Vehicle Types</p>
                        <div className="flex items-center justify-between text-xs">
                          <span className="text-slate-700">🏍️ 2W: {digest.vehicle_modes['2-wheeler']}</span>
                          <span className="text-slate-700">🚗 4W: {digest.vehicle_modes['4-wheeler']}</span>
                          <span className="text-slate-700">🚐 Van: {digest.vehicle_modes.van}</span>
                        </div>
                      </div>
                    </div>
                  </div>

                  <div className="flex items-center space-x-3">
                    <button
                      onClick={() => {
                        setState('data-loaded');
                      }}
                      className="flex-1 bg-gradient-to-r from-emerald-500 to-emerald-600 text-white px-6 py-3 rounded-lg font-semibold hover:from-emerald-600 hover:to-emerald-700 transition-all shadow-lg hover:shadow-xl flex items-center justify-center space-x-2"
                    >
                      <Play className="w-5 h-5" />
                      <span>Continue to Optimization</span>
                    </button>
                    <button
                      onClick={resetApp}
                      className="px-6 py-3 rounded-lg font-semibold bg-slate-200 text-slate-700 hover:bg-slate-300 transition-colors"
                    >
                      Upload Different File
                    </button>
                  </div>
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
                  onClick={resetApp}
                  className="px-6 py-3 rounded-lg font-medium bg-slate-200 text-slate-700 hover:bg-slate-300 transition-colors"
                >
                  Upload New File
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
          </div>
        )}

        {state === 'completed' && result && (
          <div className="space-y-6">
            <KPIDashboard result={result} />

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
                onClick={resetApp}
                className="px-4 py-2 rounded-lg font-medium bg-blue-500 text-white hover:bg-blue-600 transition-colors"
              >
                <Download className="w-4 h-4 inline mr-2" />
                Load New Scenario
              </button>
            </div>

            <MapVisualization routes={routes} employees={employees} showOptimized={showOptimized} />

            {viewMode === 'fleet' && (
              <VehiclePanel routes={routes} vehicles={vehicles} assignments={assignments} />
            )}

            {viewMode === 'employee' && <EmployeeView employees={employees} assignments={assignments} />}

            <div className="bg-white rounded-xl shadow-lg p-6">
              <h2 className="text-xl font-bold text-slate-800 border-b pb-3 mb-4">Constraint Validation</h2>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div className="bg-emerald-50 border border-emerald-200 rounded-lg p-4">
                  <p className="text-2xl font-bold text-emerald-700">{result.hard_violations || 0}</p>
                  <p className="text-sm text-emerald-600">Hard Constraint Violations</p>
                </div>
                <div className="bg-emerald-50 border border-emerald-200 rounded-lg p-4">
                  <p className="text-2xl font-bold text-emerald-700">{result.soft_violations || 0}</p>
                  <p className="text-sm text-emerald-600">Soft Constraint Violations</p>
                </div>
                <div className="bg-emerald-50 border border-emerald-200 rounded-lg p-4">
                  <p className="text-2xl font-bold text-emerald-700">
                    {result.hard_violations === 0 && result.soft_violations === 0 ? '100%' : 
                     ((employees.length - (result.hard_violations || 0)) / employees.length * 100).toFixed(0) + '%'}
                  </p>
                  <p className="text-sm text-emerald-600">Solution Quality</p>
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
