import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Clock, Users, Car, ChevronDown, ChevronUp } from 'lucide-react';
import type { Employee, Vehicle } from '../types';

export default function DataInsights() {
  const navigate = useNavigate();
  const [data, setData] = useState<{ employees: Employee[]; vehicles: Vehicle[] } | null>(null);
  const [solverDuration, setSolverDuration] = useState<'Quick' | 'Standard' | 'Thorough' | 'Maximum'>('Standard');
  const [showEmployees, setShowEmployees] = useState(false);

  useEffect(() => {
    const savedData = sessionStorage.getItem('uploadedData');
    if (savedData) {
      const parsed = JSON.parse(savedData);
      setData({ employees: parsed.employees, vehicles: parsed.vehicles });
    } else {
      navigate('/upload');
    }
  }, [navigate]);

  if (!data) return null;

  const timeWindow = {
    start: data.vehicles.reduce((min, v) => (v.availabilityTime < min ? v.availabilityTime : min), data.vehicles[0].availabilityTime),
    end: data.employees.reduce((max, e) => (e.timeWindowEnd > max ? e.timeWindowEnd : max), data.employees[0].timeWindowEnd),
  };

  const priorityDist = {
    High: data.employees.filter((e) => e.priority === 'High').length,
    Medium: data.employees.filter((e) => e.priority === 'Medium').length,
    Low: data.employees.filter((e) => e.priority === 'Low').length,
  };

  const fuelDist = {
    Electric: data.vehicles.filter((v) => v.fuelType === 'Electric').length,
    Petrol: data.vehicles.filter((v) => v.fuelType === 'Petrol').length,
    Diesel: data.vehicles.filter((v) => v.fuelType === 'Diesel').length,
  };

  const modeDist = {
    '2-Wheeler': data.vehicles.filter((v) => v.mode === '2-Wheeler').length,
    '4-Wheeler': data.vehicles.filter((v) => v.mode === '4-Wheeler').length,
    Van: data.vehicles.filter((v) => v.mode === 'Van').length,
  };

  const handleRunOptimization = () => {
    sessionStorage.setItem('solverDuration', solverDuration);
    navigate('/processing');
  };

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-7xl mx-auto">
        {/* Progress Indicator */}
        <div className="mb-8">
          <div className="flex items-center justify-between">
            {['Upload Data', 'Review Insights', 'Configure', 'Optimize'].map((step, index) => (
              <div key={step} className="flex items-center">
                <div
                  className={`w-10 h-10 rounded-full flex items-center justify-center font-bold ${
                    index <= 1 ? 'bg-primary text-dark' : 'bg-dark-700 text-gray'
                  }`}
                >
                  {index + 1}
                </div>
                <span className={`ml-2 ${index <= 1 ? 'text-white font-medium' : 'text-gray'}`}>
                  {step}
                </span>
                {index < 3 && <div className="w-12 h-px bg-gray/30 mx-4" />}
              </div>
            ))}
          </div>
        </div>

        <h1 className="text-4xl font-bold mb-2">Data Insights Preview</h1>
        <p className="text-gray mb-8">Review your uploaded data before optimization</p>

        {/* Time Window Overview */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="card mb-6">
          <h2 className="text-xl font-bold mb-4 flex items-center gap-2">
            <Clock className="w-6 h-6 text-primary" />
            Planning Window
          </h2>
          <div className="flex items-center justify-between">
            <div className="text-center">
              <p className="text-sm text-gray mb-1">Start Time</p>
              <p className="text-2xl font-bold text-primary">{timeWindow.start}</p>
            </div>
            <div className="flex-1 mx-8">
              <div className="h-2 bg-gradient-to-r from-primary via-primary-dark to-primary rounded-full" />
            </div>
            <div className="text-center">
              <p className="text-sm text-gray mb-1">End Time</p>
              <p className="text-2xl font-bold text-primary">{timeWindow.end}</p>
            </div>
          </div>
        </motion.div>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-6">
          {/* Employee Requests */}
          <motion.div initial={{ opacity: 0, x: -20 }} animate={{ opacity: 1, x: 0 }} className="card">
            <h2 className="text-xl font-bold mb-4 flex items-center gap-2">
              <Users className="w-6 h-6 text-blue-400" />
              Employee Requests Overview
            </h2>
            <div className="space-y-4">
              <div>
                <p className="text-sm text-gray mb-2">Total Employees</p>
                <p className="text-3xl font-bold">{data.employees.length}</p>
              </div>
              <div>
                <p className="text-sm text-gray mb-2">Priority Distribution</p>
                <div className="flex gap-2 mb-2">
                  <div className="flex-1 bg-red-500 h-8 rounded flex items-center justify-center text-sm font-medium" style={{ width: `${(priorityDist.High / data.employees.length) * 100}%` }}>
                    {priorityDist.High > 0 && `High: ${priorityDist.High}`}
                  </div>
                  <div className="flex-1 bg-yellow-500 h-8 rounded flex items-center justify-center text-sm font-medium" style={{ width: `${(priorityDist.Medium / data.employees.length) * 100}%` }}>
                    {priorityDist.Medium > 0 && `Med: ${priorityDist.Medium}`}
                  </div>
                  <div className="flex-1 bg-green-500 h-8 rounded flex items-center justify-center text-sm font-medium" style={{ width: `${(priorityDist.Low / data.employees.length) * 100}%` }}>
                    {priorityDist.Low > 0 && `Low: ${priorityDist.Low}`}
                  </div>
                </div>
              </div>
            </div>
          </motion.div>

          {/* Vehicle Fleet */}
          <motion.div initial={{ opacity: 0, x: 20 }} animate={{ opacity: 1, x: 0 }} className="card">
            <h2 className="text-xl font-bold mb-4 flex items-center gap-2">
              <Car className="w-6 h-6 text-purple-400" />
              Available Fleet
            </h2>
            <div className="space-y-4">
              <div>
                <p className="text-sm text-gray mb-2">Total Vehicles</p>
                <p className="text-3xl font-bold">{data.vehicles.length}</p>
              </div>
              <div>
                <p className="text-sm text-gray mb-2">By Fuel Type</p>
                <div className="flex gap-4 text-sm">
                  <div><span className="text-green-400">⚡ Electric:</span> {fuelDist.Electric}</div>
                  <div><span className="text-blue-400">⛽ Petrol:</span> {fuelDist.Petrol}</div>
                  <div><span className="text-orange-400">🛢️ Diesel:</span> {fuelDist.Diesel}</div>
                </div>
              </div>
              <div>
                <p className="text-sm text-gray mb-2">By Vehicle Mode</p>
                <div className="flex gap-4 text-sm">
                  <div>🏍️ 2-Wheeler: {modeDist['2-Wheeler']}</div>
                  <div>🚗 4-Wheeler: {modeDist['4-Wheeler']}</div>
                  <div>🚐 Van: {modeDist.Van}</div>
                </div>
              </div>
            </div>
          </motion.div>
        </div>

        {/* Data Tables Preview */}
        <div className="card mb-6">
          <button
            onClick={() => setShowEmployees(!showEmployees)}
            className="w-full flex items-center justify-between py-3"
          >
            <span className="font-bold">Preview Employee Requests (First 10)</span>
            {showEmployees ? <ChevronUp /> : <ChevronDown />}
          </button>
          {showEmployees && (
            <div className="mt-4 overflow-x-auto">
              <table className="w-full text-sm">
                <thead className="bg-dark-600">
                  <tr>
                    <th className="px-4 py-2 text-left">Employee ID</th>
                    <th className="px-4 py-2 text-left">Priority</th>
                    <th className="px-4 py-2 text-left">Pickup Location</th>
                    <th className="px-4 py-2 text-left">Time Window</th>
                    <th className="px-4 py-2 text-right">Baseline Cost</th>
                  </tr>
                </thead>
                <tbody>
                  {data.employees.slice(0, 10).map((emp, index) => (
                    <tr key={emp.id} className={index % 2 === 0 ? 'bg-dark-700/50' : ''}>
                      <td className="px-4 py-2">{emp.id}</td>
                      <td className="px-4 py-2">
                        <span className={`badge ${emp.priority === 'High' ? 'badge-error' : emp.priority === 'Medium' ? 'badge-warning' : 'badge-success'}`}>
                          {emp.priority}
                        </span>
                      </td>
                      <td className="px-4 py-2">{emp.pickupLocation}</td>
                      <td className="px-4 py-2">{emp.timeWindowStart} - {emp.timeWindowEnd}</td>
                      <td className="px-4 py-2 text-right">₹{emp.baselineCost}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>

        {/* Solver Configuration */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="card mb-6">
          <h2 className="text-xl font-bold mb-4">Optimization Settings</h2>
          <div className="space-y-4">
            <div>
              <p className="text-sm text-gray mb-3">Solver Duration</p>
              <div className="grid grid-cols-4 gap-4">
                {(['Quick', 'Standard', 'Thorough', 'Maximum'] as const).map((mode) => (
                  <button
                    key={mode}
                    onClick={() => setSolverDuration(mode)}
                    className={`p-4 rounded-lg border-2 transition-all ${
                      solverDuration === mode
                        ? 'border-primary bg-primary/10 text-white'
                        : 'border-gray/30 text-gray hover:border-gray/50'
                    }`}
                  >
                    <div className="font-bold mb-1">{mode}</div>
                    <div className="text-xs">
                      {mode === 'Quick' && '30 seconds'}
                      {mode === 'Standard' && '2 minutes'}
                      {mode === 'Thorough' && '5 minutes'}
                      {mode === 'Maximum' && '10 minutes'}
                    </div>
                  </button>
                ))}
              </div>
              <p className="text-xs text-gray mt-2">
                Longer runtime allows the solver to explore more solutions and typically produces better cost savings.
              </p>
            </div>
          </div>
        </motion.div>

        {/* Action Footer */}
        <div className="flex justify-between">
          <button onClick={() => navigate('/upload')} className="btn-secondary">
            ← Back to Upload
          </button>
          <button onClick={handleRunOptimization} className="btn-primary">
            Run Optimization ({solverDuration === 'Quick' ? '30s' : solverDuration === 'Standard' ? '2m' : solverDuration === 'Thorough' ? '5m' : '10m'}) →
          </button>
        </div>
      </div>
    </div>
  );
}
