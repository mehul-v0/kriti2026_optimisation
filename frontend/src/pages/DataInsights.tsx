import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Users, Car, ChevronDown, ChevronUp, RotateCcw, ChevronLeft, ChevronRight, Search, Filter, Info } from 'lucide-react';
import type { Employee, Vehicle } from '../types';
import { useApp } from '../context/AppContext';

export default function DataInsights() {
  const navigate = useNavigate();
  const { startOptimization, lastOptimizationKey, currentResult } = useApp();
  const [data, setData] = useState<{ employees: Employee[]; vehicles: Vehicle[] } | null>(null);
  const [solverDuration, setSolverDuration] = useState<'Quick' | 'Standard' | 'Thorough' | 'Maximum'>('Standard');
  const [showEmployees, setShowEmployees] = useState(false);
  
  // Table pagination and filter state
  const [currentPage, setCurrentPage] = useState(0);
  const [searchQuery, setSearchQuery] = useState('');
  const [priorityFilter, setPriorityFilter] = useState<'All' | 'High' | 'Medium' | 'Low'>('All');
  const ROWS_PER_PAGE = 10;
  
  // Configuration state with default values
  const defaultConfig = {
    costWeight: 0.7,
    timeWeight: 0.3,
    distanceMethod: 'haversine' as 'haversine' | 'actual_maps',
    priorityDelays: {
      1: 5,
      2: 5,
      3: 10,
      4: 15,
      5: 15
    } as Record<number, number>
  };
  
  const [config, setConfig] = useState(defaultConfig);
  
  const resetConfig = () => {
    setConfig(defaultConfig);
  };
  
  const updateCostWeight = (value: number) => {
    setConfig({ ...config, costWeight: value, timeWeight: 1 - value });
  };
  
  const updatePriorityDelay = (priority: number, delay: number) => {
    setConfig({
      ...config,
      priorityDelays: {
        ...config.priorityDelays,
        [priority]: delay
      }
    });
  };

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

  // Filter and paginate employees
  const filteredEmployees = data.employees.filter(emp => {
    const matchesSearch = emp.id.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         emp.pickupLocation.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         emp.priority.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         emp.timeWindowStart.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         emp.timeWindowEnd.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         emp.sharingPreference.toLowerCase().includes(searchQuery.toLowerCase()) ||
                         (emp.baselineCost || 150).toString().includes(searchQuery.toLowerCase());
    const matchesPriority = priorityFilter === 'All' || emp.priority === priorityFilter;
    return matchesSearch && matchesPriority;
  });
  
  const totalPages = Math.ceil(filteredEmployees.length / ROWS_PER_PAGE);
  const paginatedEmployees = filteredEmployees.slice(
    currentPage * ROWS_PER_PAGE,
    (currentPage + 1) * ROWS_PER_PAGE
  );
  
  const nextPage = () => {
    if (currentPage < totalPages - 1) setCurrentPage(currentPage + 1);
  };
  
  const prevPage = () => {
    if (currentPage > 0) setCurrentPage(currentPage - 1);
  };

  // Calculate fleet capacity metrics

  const priorityDist = {
    High: data.employees.filter((e) => e.priority === 'High').length,
    Medium: data.employees.filter((e) => e.priority === 'Medium').length,
    Low: data.employees.filter((e) => e.priority === 'Low').length,
  };
  
  const sharingDist = {
    Single: data.employees.filter((e) => e.sharingPreference === 'Single').length,
    Double: data.employees.filter((e) => e.sharingPreference === 'Double').length,
    Triple: data.employees.filter((e) => e.sharingPreference === 'Triple').length,
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
    sessionStorage.setItem('optimizationConfig', JSON.stringify(config));

    // Build fingerprint of current data + config
    const dataStr = sessionStorage.getItem('uploadedData') || '';
    const configStr = JSON.stringify(config);
    const currentKey = `${dataStr.length}|${configStr}|${solverDuration}`;

    // If nothing changed since last successful run and we already have results, skip
    if (currentKey === lastOptimizationKey && currentResult) {
      sessionStorage.setItem('optimizationComplete', 'true');
      navigate('/results');
      return;
    }

    sessionStorage.removeItem('optimizationComplete');
    // Start the optimization in AppContext (runs in background)
    startOptimization();
    navigate('/processing');
  };

  return (
    <div className="min-h-screen p-3" style={{ background: 'linear-gradient(180deg, #000000 0%, #0a0a0a 50%, #121212 100%)' }}>
      <div className="max-w-7xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Data Insights Preview</h1>
        <p className="text-gray mb-8">Review your uploaded data before optimization</p>

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-3 mb-3">
          {/* Employee Requests */}
          <motion.div initial={{ opacity: 0, x: -20 }} animate={{ opacity: 1, x: 0 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 relative overflow-hidden shadow-float hover:shadow-float-lg transition-all duration-300">
            {/* Background icon - zoomed, top-right-aligned */}
            <Users className="absolute -right-4 top-6 w-36 h-36 text-primary/10 pointer-events-none z-0" />
            
            <h2 className="text-xl font-bold mb-4 relative z-10">
              Employee Requests Overview
            </h2>
            <div className="space-y-4 relative z-10">
              <div>
                <p className="text-sm text-gray mb-2">Total Employees</p>
                <p className="text-3xl font-bold">{data.employees.length}</p>
              </div>
              <div>
                <p className="text-lg font-semibold text-white mb-4">Priority Distribution</p>
                <div className="grid grid-cols-3 gap-4 mb-4 relative z-10">
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">High</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{priorityDist.High}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (priorityDist.High / data.employees.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Medium</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{priorityDist.Medium}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (priorityDist.Medium / data.employees.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Low</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{priorityDist.Low}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (priorityDist.Low / data.employees.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                </div>
              </div>
              <div>
                <p className="text-lg font-semibold text-white mb-4">Sharing Preferences</p>
                <div className="grid grid-cols-3 gap-4 relative z-10">
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Single</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{sharingDist.Single}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (sharingDist.Single / data.employees.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Double</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{sharingDist.Double}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (sharingDist.Double / data.employees.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Triple</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{sharingDist.Triple}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (sharingDist.Triple / data.employees.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </motion.div>

          {/* Vehicle Fleet */}
          <motion.div initial={{ opacity: 0, x: 20 }} animate={{ opacity: 1, x: 0 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 relative overflow-hidden shadow-float hover:shadow-float-lg transition-all duration-300">
            {/* Background icon - zoomed, top-right-aligned */}
            <Car className="absolute right-4 top-6 w-36 h-36 text-primary/10 pointer-events-none z-0" />
            
            <h2 className="text-xl font-bold mb-4 relative z-10">
              Fleet Capacity Overview
            </h2>
            <div className="space-y-4 relative z-10">
              <div>
                <p className="text-sm text-gray mb-2">Total Vehicles</p>
                <p className="text-3xl font-bold">{data.vehicles.length}</p>
              </div>
              <div>
                <p className="text-lg font-semibold text-white mb-4">Fuel Type Distribution</p>
                <div className="grid grid-cols-3 gap-4 mb-4 relative z-10">
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Electric</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{fuelDist.Electric}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (fuelDist.Electric / data.vehicles.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Petrol</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{fuelDist.Petrol}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (fuelDist.Petrol / data.vehicles.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Diesel</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{fuelDist.Diesel}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (fuelDist.Diesel / data.vehicles.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                </div>
              </div>
              <div>
                <p className="text-lg font-semibold text-white mb-4">Vehicle Mode Distribution</p>
                <div className="grid grid-cols-3 gap-4 relative z-10">
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">2-Wheeler</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{modeDist['2-Wheeler']}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (modeDist['2-Wheeler'] / data.vehicles.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">4-Wheeler</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{modeDist['4-Wheeler']}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (modeDist['4-Wheeler'] / data.vehicles.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                  <div className="bg-dark-600/80 p-4 rounded-lg shadow-float">
                    <div className="text-sm text-gray mb-1">Van</div>
                    <div className="flex items-center gap-3">
                      <div className="text-2xl font-bold">{modeDist.Van}</div>
                      <div className="flex gap-1">
                        {Array.from({ length: 10 }, (_, i) => (
                          <div 
                            key={i}
                            className={`w-1 h-4 rounded ${(i + 1) <= (modeDist.Van / data.vehicles.length) * 10 ? 'bg-primary' : 'bg-dark-500'}`}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </motion.div>
        </div>

        {/* Employee Data Table */}
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 mb-3 shadow-float">
          <button
            onClick={() => setShowEmployees(!showEmployees)}
            className="w-full flex items-center justify-between py-3 hover:bg-dark-700/30 rounded-lg transition-colors"
          >
            <h3 className="text-xl font-bold text-white">Employee Requests Preview</h3>
            {showEmployees ? <ChevronUp className="w-5 h-5" /> : <ChevronDown className="w-5 h-5" />}
          </button>
          
          {showEmployees && (
            <div className="mt-4">
              {/* Search and Filter Controls */}
              <div className="flex flex-col sm:flex-row gap-4 mb-4">
                <div className="flex-1 relative">
                  <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 w-4 h-4 text-gray/60" />
                  <input
                    type="text"
                    placeholder="Search across all columns..."
                    value={searchQuery}
                    onChange={(e) => {
                      setSearchQuery(e.target.value);
                      setCurrentPage(0);
                    }}
                    className="w-full pl-10 pr-4 py-2 bg-dark-700 border border-gray/30 rounded-lg text-white placeholder-gray/60 focus:border-primary focus:outline-none"
                  />
                </div>
                <div className="flex items-center gap-3">
                  <Filter className="w-4 h-4 text-gray/60" />
                  <div className="relative">
                    <select
                      value={priorityFilter}
                      onChange={(e) => {
                        setPriorityFilter(e.target.value as 'All' | 'High' | 'Medium' | 'Low');
                        setCurrentPage(0);
                      }}
                      className="pl-3 pr-8 py-2 bg-dark-700 border border-gray/30 rounded-lg text-white focus:border-primary focus:outline-none appearance-none cursor-pointer"
                    >
                      <option value="All">All Priorities</option>
                      <option value="High">High</option>
                      <option value="Medium">Medium</option>
                      <option value="Low">Low</option>
                    </select>
                    <ChevronDown className="absolute right-2 top-1/2 transform -translate-y-1/2 w-4 h-4 text-gray/60 pointer-events-none" />
                  </div>
                </div>
              </div>
              
              {/* Table */}
              <div className="overflow-x-auto rounded-lg border border-gray/20">
                <table className="w-full">
                  <thead>
                    <tr className="bg-primary-muted/20">
                      <th className="px-4 py-3 text-left text-sm font-semibold text-gray/80">Employee ID</th>
                      <th className="px-4 py-3 text-left text-sm font-semibold text-gray/80">Priority</th>
                      <th className="px-4 py-3 text-left text-sm font-semibold text-gray/80">Pickup Location</th>
                      <th className="px-4 py-3 text-left text-sm font-semibold text-gray/80">Time Window</th>
                      <th className="px-4 py-3 text-left text-sm font-semibold text-gray/80">Sharing</th>
                      <th className="px-4 py-3 text-right text-sm font-semibold text-gray/80">Baseline Cost</th>
                    </tr>
                  </thead>
                  <tbody className="bg-dark-700/30">
                    {paginatedEmployees.map((emp) => (
                      <tr key={emp.id} className="border-b border-gray/10 hover:bg-dark-600/30 transition-colors">
                        <td className="px-4 py-3 text-sm font-medium text-white">{emp.id}</td>
                        <td className="px-4 py-3">
                          <span className={`px-2 py-1 rounded text-xs font-medium ${
                            emp.priority === 'High' 
                              ? 'bg-red-500/20 text-red-300 border border-red-500/30' 
                              : emp.priority === 'Medium' 
                              ? 'bg-yellow-500/20 text-yellow-300 border border-yellow-500/30' 
                              : 'bg-blue-500/20 text-blue-300 border border-blue-500/30'
                          }`}>
                            {emp.priority}
                          </span>
                        </td>
                        <td className="px-4 py-3 text-sm text-gray/90">{emp.pickupLocation}</td>
                        <td className="px-4 py-3 text-sm text-gray/90">{emp.timeWindowStart} - {emp.timeWindowEnd}</td>
                        <td className="px-4 py-3 text-sm text-gray/90">{emp.sharingPreference}</td>
                        <td className="px-4 py-3 text-right text-sm font-medium text-primary-bright">₹{emp.baselineCost || 150}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              
              {/* Pagination Controls */}
              <div className="flex items-center justify-between mt-4">
                <div className="text-sm text-gray/70">
                  Showing {currentPage * ROWS_PER_PAGE + 1}-{Math.min((currentPage + 1) * ROWS_PER_PAGE, filteredEmployees.length)} of {filteredEmployees.length} employees
                </div>
                <div className="flex items-center gap-2">
                  <button
                    onClick={prevPage}
                    disabled={currentPage === 0}
                    className="p-2 rounded-lg bg-dark-700 hover:bg-dark-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
                  >
                    <ChevronLeft className="w-4 h-4" />
                  </button>
                  <span className="px-3 py-1 text-sm bg-dark-700 rounded-lg">
                    {currentPage + 1} of {totalPages || 1}
                  </span>
                  <button
                    onClick={nextPage}
                    disabled={currentPage >= totalPages - 1}
                    className="p-2 rounded-lg bg-dark-700 hover:bg-dark-600 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
                  >
                    <ChevronRight className="w-4 h-4" />
                  </button>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Solver Configuration */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 mb-3 shadow-float">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-2xl font-bold text-white">Optimization Settings</h2>
            <button 
              onClick={resetConfig}
              className="flex items-center gap-2 px-4 py-2 text-sm bg-primary hover:bg-primary-light text-dark font-semibold rounded-lg transition-all duration-300"
            >
              <RotateCcw className="w-4 h-4" />
              Reset
            </button>
          </div>
          <div className="space-y-6">
            {/* Distance Calculation Method */}
            <div>
              <div className="flex items-center gap-2 mb-3">
                <p className="text-base font-semibold text-white">Distance Calculation Method</p>
                <div className="relative group/info">
                  <Info className="w-4 h-4 text-gray/60 hover:text-gray cursor-help transition-colors" />
                  <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/90 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50 max-w-xs w-max">
                    Haversine is faster but less accurate. Actual maps provide real road distances but may be slower.
                    <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                  </div>
                </div>
              </div>
              <div className="grid grid-cols-2 gap-4">
                <button
                  onClick={() => setConfig({ ...config, distanceMethod: 'haversine' })}
                  className={`p-4 rounded-lg border-2 transition-all ${
                    config.distanceMethod === 'haversine'
                      ? 'border-primary bg-primary/10 text-white'
                      : 'border-gray/30 text-gray hover:border-gray/50'
                  }`}
                >
                  <div className="font-bold mb-1">Haversine Formula</div>
                  <div className="text-xs">Fast calculation using geographic coordinates</div>
                </button>
                <button
                  onClick={() => setConfig({ ...config, distanceMethod: 'actual_maps' })}
                  className={`p-4 rounded-lg border-2 transition-all ${
                    config.distanceMethod === 'actual_maps'
                      ? 'border-primary bg-primary/10 text-white'
                      : 'border-gray/30 text-gray hover:border-gray/50'
                  }`}
                >
                  <div className="font-bold mb-1">Actual Map Routes</div>
                  <div className="text-xs">Real road distances via mapping service</div>
                </button>
              </div>
            </div>

            {/* Combined Cost/Time Weight Slider */}
            <div>
              <p className="text-base font-semibold text-white mb-3">Cost vs Time Priority</p>
              <div className="relative">
                {/* Weight Labels */}
                <div className="flex justify-between items-center mb-2">
                  <div className="flex items-center gap-2">
                    <div className="w-3 h-3 bg-primary rounded-full"></div>
                    <span className="text-sm text-gray">Cost: {config.costWeight.toFixed(2)}</span>
                  </div>
                  <div className="flex items-center gap-2">
                    <span className="text-sm text-gray">Time: {config.timeWeight.toFixed(2)}</span>
                    <div className="w-3 h-3 bg-white rounded-full"></div>
                  </div>
                </div>
                
                {/* Custom Slider */}
                <div className="relative h-6 rounded-full overflow-hidden">
                  {/* Two-tone background */}
                  <div className="absolute inset-0 flex">
                    <div className="bg-primary/40" style={{ width: `${config.costWeight * 100}%` }}></div>
                    <div className="bg-white/20" style={{ width: `${config.timeWeight * 100}%` }}></div>
                  </div>
                  
                  {/* Slider track */}
                  <input 
                    type="range" 
                    min="0" 
                    max="1" 
                    step="0.01" 
                    value={config.costWeight}
                    onChange={(e) => updateCostWeight(parseFloat(e.target.value))}
                    className="absolute inset-0 w-full h-full opacity-0 cursor-pointer z-10"
                  />
                  
                  {/* Slider thumb indicator - Fixed positioning */}
                  <div 
                    className="absolute top-1 w-4 h-4 bg-white rounded-full shadow-lg border-2 border-primary pointer-events-none z-20"
                    style={{ left: `calc(${config.costWeight * 100}% - 8px)` }}
                  ></div>
                </div>
                
                <div className="flex justify-between text-xs text-gray mt-1">
                  <span>Cost Focus</span>
                  <span>Balanced</span>
                  <span>Time Focus</span>
                </div>
              </div>
            </div>
            
            {/* Priority Delay Configuration */}
            <div>
              <div className="flex items-center gap-2 mb-3">
                <p className="text-base font-semibold text-white">Priority Delay Limits</p>
                <div className="relative group/info">
                  <Info className="w-4 h-4 text-gray/60 hover:text-gray cursor-help transition-colors" />
                  <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/90 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50 max-w-xs w-max">
                    Maximum delay allowed for each priority level before the employee is considered late.
                    <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                  </div>
                </div>
              </div>
              <div className="grid grid-cols-2 lg:grid-cols-5 gap-4">
                {[1, 2, 3, 4, 5].map((p) => (
                  <div key={p} className="bg-dark-700/50 p-4 rounded-lg">
                    <label className="block text-sm text-gray mb-2">Priority {p}</label>
                    <div className="flex items-center gap-2">
                      <input 
                        type="number"
                        min="1"
                        max="60"
                        value={config.priorityDelays[p]}
                        onChange={(e) => updatePriorityDelay(p, parseInt(e.target.value) || 0)}
                        className="w-full px-3 py-2 bg-dark-700 border border-dark-600 rounded-lg focus:border-primary focus:outline-none"
                      />
                      <span className="text-xs text-gray whitespace-nowrap">min</span>
                    </div>
                  </div>
                ))}
              </div>
            </div>
            
            {/* Solver Duration */}
            <div>
              <div className="flex items-center gap-2 mb-3">
                <p className="text-base font-semibold text-white">Solver Duration</p>
                <div className="relative group/info">
                  <Info className="w-4 h-4 text-gray/60 hover:text-gray cursor-help transition-colors" />
                  <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/90 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50 max-w-xs w-max">
                    Longer runtime allows the solver to explore more solutions and typically produces better cost savings.
                    <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                  </div>
                </div>
              </div>
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
                      {mode === 'Quick' && '15 seconds'}
                      {mode === 'Standard' && '30 seconds'}
                      {mode === 'Thorough' && '1 minute'}
                      {mode === 'Maximum' && '2 minutes'}
                    </div>
                  </button>
                ))}
              </div>
            </div>
          </div>
        </motion.div>

        {/* Action Footer */}
        <div className="flex justify-between">
          <button 
            onClick={() => navigate('/upload')} 
            className="bg-dark-700 hover:bg-dark-600 text-white text-base font-semibold px-6 py-3 rounded-lg transition-all duration-300 shadow-float hover:shadow-float-lg"
          >
            ← Back to Upload
          </button>
          <button 
            onClick={handleRunOptimization} 
            className="bg-primary hover:bg-primary-light text-dark text-base font-bold px-6 py-3 rounded-lg transition-all duration-300 shadow-float"
          >
            Run Optimization ({solverDuration === 'Quick' ? '15s' : solverDuration === 'Standard' ? '30s' : solverDuration === 'Thorough' ? '1m' : '2m'}) →
          </button>
        </div>
      </div>
    </div>
  );
}

