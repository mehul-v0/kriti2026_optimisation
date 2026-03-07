import { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
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

  /* ---------- tiny helper for the bar‑graph dots ---------- */
  const BarDots = ({ filled, total }: { filled: number; total: number }) => (
    <div className="flex gap-0.5">
      {Array.from({ length: 10 }, (_, i) => (
        <div
          key={i}
          className={`w-1 h-4 rounded ${
            i + 1 <= (filled / total) * 10 ? 'bg-primary' : 'bg-white/[0.04]'
          }`}
        />
      ))}
    </div>
  );

  return (
    <div className="max-w-[1400px] mx-auto p-6 md:p-8">
      {/* Page header */}
      <div className="mb-6">
        <h1 className="text-2xl font-black text-white tracking-tight uppercase">Data Insights Preview</h1>
        <p className="text-xs font-mono text-white/30 mt-1">Review your uploaded data before optimization</p>
      </div>

      {/* ===== Two‑column layout: scrollable left | sticky right ===== */}
      <div className="flex flex-col lg:flex-row gap-6 items-start">

        {/* ── LEFT COLUMN ── scrollable insights ── */}
        <div className="flex-1 min-w-0 space-y-6">

          {/* KPI row ─ Employee & Vehicle counts */}
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {/* Employee KPI */}
            <motion.div
              initial={{ opacity: 0, y: 16 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ duration: 0.4 }}
              className="bg-panel-dark border border-white/10 p-5 flex flex-col gap-2 relative overflow-hidden"
            >
              <div className="absolute top-0 right-0 w-24 h-24 bg-primary/10 rounded-full blur-xl -mr-10 -mt-10 pointer-events-none" />
              <div className="flex justify-between items-start">
                <p className="text-[11px] font-label uppercase tracking-widest text-white/40">Total Employees</p>
                <span className="material-symbols-outlined text-primary text-sm">groups</span>
              </div>
              <p className="text-3xl font-bold font-mono text-white mt-1">{data.employees.length}</p>
            </motion.div>

            {/* Vehicle KPI */}
            <motion.div
              initial={{ opacity: 0, y: 16 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ duration: 0.4, delay: 0.05 }}
              className="bg-panel-dark border border-white/10 p-5 flex flex-col gap-2 relative overflow-hidden"
            >
              <div className="absolute top-0 right-0 w-24 h-24 bg-primary/10 rounded-full blur-xl -mr-10 -mt-10 pointer-events-none" />
              <div className="flex justify-between items-start">
                <p className="text-[11px] font-label uppercase tracking-widest text-white/40">Total Vehicles</p>
                <span className="material-symbols-outlined text-primary text-sm">local_shipping</span>
              </div>
              <p className="text-3xl font-bold font-mono text-white mt-1">{data.vehicles.length}</p>
            </motion.div>
          </div>

          {/* ── Employee Requests Overview ── */}
          <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.4, delay: 0.1 }}
            className="bg-panel-dark border border-white/10 overflow-hidden"
          >
            <div className="p-5 border-b border-white/10 flex items-center gap-2 bg-white/[0.02]">
              <span className="material-symbols-outlined text-white/40">groups</span>
              <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Employee Requests Overview</h2>
            </div>
            <div className="p-5 space-y-6">
              {/* Priority Distribution */}
              <div>
                <p className="text-[11px] font-label uppercase tracking-widest text-white/30 mb-3">Priority Distribution</p>
                <div className="grid grid-cols-3 gap-3">
                  {(['High', 'Medium', 'Low'] as const).map((level) => (
                    <div key={level} className="bg-white/[0.02] p-4 border border-white/10">
                      <div className="text-[10px] font-mono text-white/30 mb-1">{level}</div>
                      <div className="flex items-center gap-3">
                        <div className="text-2xl font-bold font-mono text-white">{priorityDist[level]}</div>
                        <BarDots filled={priorityDist[level]} total={data.employees.length} />
                      </div>
                    </div>
                  ))}
                </div>
              </div>
              {/* Sharing Preferences */}
              <div>
                <p className="text-[11px] font-label uppercase tracking-widest text-white/30 mb-3">Sharing Preferences</p>
                <div className="grid grid-cols-3 gap-3">
                  {(['Single', 'Double', 'Triple'] as const).map((pref) => (
                    <div key={pref} className="bg-white/[0.02] p-4 border border-white/10">
                      <div className="text-[10px] font-mono text-white/30 mb-1">{pref}</div>
                      <div className="flex items-center gap-3">
                        <div className="text-2xl font-bold font-mono text-white">{sharingDist[pref]}</div>
                        <BarDots filled={sharingDist[pref]} total={data.employees.length} />
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </motion.div>

          {/* ── Fleet Capacity Overview ── */}
          <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.4, delay: 0.15 }}
            className="bg-panel-dark border border-white/10 overflow-hidden"
          >
            <div className="p-5 border-b border-white/10 flex items-center gap-2 bg-white/[0.02]">
              <span className="material-symbols-outlined text-white/40">local_shipping</span>
              <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Fleet Capacity Overview</h2>
            </div>
            <div className="p-5 space-y-6">
              {/* Fuel Type */}
              <div>
                <p className="text-[11px] font-label uppercase tracking-widest text-white/30 mb-3">Fuel Type Distribution</p>
                <div className="grid grid-cols-3 gap-3">
                  {(['Electric', 'Petrol', 'Diesel'] as const).map((fuel) => (
                    <div key={fuel} className="bg-white/[0.02] p-4 border border-white/10">
                      <div className="text-[10px] font-mono text-white/30 mb-1">{fuel}</div>
                      <div className="flex items-center gap-3">
                        <div className="text-2xl font-bold font-mono text-white">{fuelDist[fuel]}</div>
                        <BarDots filled={fuelDist[fuel]} total={data.vehicles.length} />
                      </div>
                    </div>
                  ))}
                </div>
              </div>
              {/* Vehicle Mode */}
              <div>
                <p className="text-[11px] font-label uppercase tracking-widest text-white/30 mb-3">Vehicle Mode Distribution</p>
                <div className="grid grid-cols-3 gap-3">
                  {(['2-Wheeler', '4-Wheeler', 'Van'] as const).map((mode) => (
                    <div key={mode} className="bg-white/[0.02] p-4 border border-white/10">
                      <div className="text-[10px] font-mono text-white/30 mb-1">{mode}</div>
                      <div className="flex items-center gap-3">
                        <div className="text-2xl font-bold font-mono text-white">{modeDist[mode]}</div>
                        <BarDots filled={modeDist[mode]} total={data.vehicles.length} />
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </motion.div>

          {/* ── Employee Data Table (collapsible) ── */}
          <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.4, delay: 0.2 }}
            className="bg-panel-dark border border-white/10 overflow-hidden"
          >
            <button
              onClick={() => setShowEmployees(!showEmployees)}
              className="w-full p-5 flex items-center justify-between hover:bg-white/5 transition-colors"
            >
              <div className="flex items-center gap-2">
                <span className="material-symbols-outlined text-white/40">table_chart</span>
                <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Employee Requests Preview</h3>
              </div>
              <span className="material-symbols-outlined text-white/40 text-xl">
                {showEmployees ? 'expand_less' : 'expand_more'}
              </span>
            </button>

            {showEmployees && (
              <div className="px-5 pb-5">
                {/* Search and Filter Controls */}
                <div className="flex flex-col sm:flex-row gap-3 mb-4">
                  <div className="flex-1 relative">
                    <span className="material-symbols-outlined absolute left-3 top-1/2 -translate-y-1/2 text-white/40 text-lg">search</span>
                    <input
                      type="text"
                      placeholder="Search across all columns..."
                      value={searchQuery}
                      onChange={(e) => {
                        setSearchQuery(e.target.value);
                        setCurrentPage(0);
                      }}
                      className="w-full pl-10 pr-4 py-2.5 bg-white/[0.02] border border-white/10 font-mono text-sm text-white/70 placeholder-white/30 focus:border-primary focus:outline-none"
                    />
                  </div>
                  <div className="flex items-center gap-2">
                    <span className="material-symbols-outlined text-white/40 text-lg">filter_list</span>
                    <div className="relative">
                      <select
                        value={priorityFilter}
                        onChange={(e) => {
                          setPriorityFilter(e.target.value as 'All' | 'High' | 'Medium' | 'Low');
                          setCurrentPage(0);
                        }}
                        className="pl-3 pr-8 py-2 bg-[#0D1117] border border-white/10 font-mono text-[11px] text-white/70 focus:border-primary focus:outline-none appearance-none cursor-pointer"
                      >
                        <option value="All" className="bg-[#0D1117] text-white/70">All Priorities</option>
                        <option value="High" className="bg-[#0D1117] text-white/70">High</option>
                        <option value="Medium" className="bg-[#0D1117] text-white/70">Medium</option>
                        <option value="Low" className="bg-[#0D1117] text-white/70">Low</option>
                      </select>
                      <span className="material-symbols-outlined absolute right-2 top-1/2 -translate-y-1/2 text-white/40 text-sm pointer-events-none">expand_more</span>
                    </div>
                  </div>
                </div>

                {/* Table */}
                <div className="overflow-x-auto border border-white/10">
                  <table className="w-full text-left border-collapse">
                    <thead>
                      <tr className="border-b border-white/10 bg-white/[0.015]">
                        <th className="px-4 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Employee ID</th>
                        <th className="px-4 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Priority</th>
                        <th className="px-4 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Pickup Location</th>
                        <th className="px-4 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Time Window</th>
                        <th className="px-4 py-3 text-[10px] font-label uppercase tracking-widest text-white/30">Sharing</th>
                        <th className="px-4 py-3 text-right text-[10px] font-label uppercase tracking-widest text-white/30">Baseline Cost</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-white/10">
                      {paginatedEmployees.map((emp) => (
                        <tr key={emp.id} className="hover:bg-white/5 transition-colors">
                          <td className="px-4 py-3 text-sm font-mono text-white/60">{emp.id}</td>
                          <td className="px-4 py-3">
                            <span className={`inline-flex items-center gap-1.5 px-2 py-0.5 text-[9px] font-mono border ${
                              emp.priority === 'High'
                                ? 'bg-red-500/10 text-red-400 border-red-500/20'
                                : emp.priority === 'Medium'
                                ? 'bg-yellow-500/10 text-yellow-400 border-yellow-500/20'
                                : 'bg-blue-500/10 text-blue-400 border-blue-500/20'
                            }`}>
                              <span className={`w-1.5 h-1.5 rounded-full ${
                                emp.priority === 'High' ? 'bg-red-400' : emp.priority === 'Medium' ? 'bg-yellow-400' : 'bg-blue-400'
                              }`} />
                              {emp.priority}
                            </span>
                          </td>
                          <td className="px-4 py-3 text-sm font-mono text-white/40">{emp.pickupLocation}</td>
                          <td className="px-4 py-3 text-sm font-mono text-white/40">{emp.timeWindowStart} – {emp.timeWindowEnd}</td>
                          <td className="px-4 py-3 text-sm font-mono text-white/40">{emp.sharingPreference}</td>
                          <td className="px-4 py-3 text-right text-sm font-medium text-primary">₹{emp.baselineCost || 150}</td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>

                {/* Pagination Controls */}
                <div className="flex items-center justify-between mt-4">
                  <div className="text-xs font-mono text-white/30">
                    Showing {currentPage * ROWS_PER_PAGE + 1}–{Math.min((currentPage + 1) * ROWS_PER_PAGE, filteredEmployees.length)} of {filteredEmployees.length}
                  </div>
                  <div className="flex items-center gap-2">
                    <button
                      onClick={prevPage}
                      disabled={currentPage === 0}
                      className="p-1.5 bg-white/[0.02] border border-white/10 hover:bg-white/5 disabled:opacity-30 disabled:cursor-not-allowed transition-colors"
                    >
                      <span className="material-symbols-outlined text-lg">chevron_left</span>
                    </button>
                    <span className="px-3 py-1 text-xs font-mono bg-white/[0.02] border border-white/10 text-white/40">
                      {currentPage + 1} / {totalPages || 1}
                    </span>
                    <button
                      onClick={nextPage}
                      disabled={currentPage >= totalPages - 1}
                      className="p-1.5 bg-white/[0.02] border border-white/10 hover:bg-white/5 disabled:opacity-30 disabled:cursor-not-allowed transition-colors"
                    >
                      <span className="material-symbols-outlined text-lg">chevron_right</span>
                    </button>
                  </div>
                </div>
              </div>
            )}
          </motion.div>

        </div>{/* end left column */}

        {/* ── RIGHT COLUMN ── sticky optimization config ── */}
        <div className="w-full lg:w-[460px] flex-shrink-0 lg:sticky lg:bottom-6 lg:self-end">
          <motion.div
            initial={{ opacity: 0, x: 20 }}
            animate={{ opacity: 1, x: 0 }}
            transition={{ duration: 0.4, delay: 0.1 }}
            className="bg-panel-dark border border-white/10 overflow-hidden"
          >
            {/* Header */}
            <div className="p-5 border-b border-white/10 flex justify-between items-center bg-white/[0.02]">
              <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 flex items-center gap-2">
                <span className="material-symbols-outlined text-primary">tune</span>
                Optimization Settings
              </h2>
              <button
                onClick={resetConfig}
                className="flex items-center gap-1.5 px-3 py-1.5 text-xs font-mono bg-white/[0.02] hover:bg-white/5 border border-white/10 transition-colors text-white/40 hover:text-white"
              >
                <span className="material-symbols-outlined text-sm">restart_alt</span>
                Reset
              </button>
            </div>

            <div className="p-5 space-y-6">

              {/* Distance Calculation Method */}
              <div>
                <div className="flex items-center gap-2 mb-3">
                  <p className="text-[11px] font-label uppercase tracking-widest text-white/40">Distance Method</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/40 text-base cursor-help hover:text-white/60 transition-colors">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-[10px] font-mono text-white/40 opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50 w-56 whitespace-normal">
                      Haversine is faster but less accurate. Actual maps provide real road distances.
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <div className="grid grid-cols-2 gap-3">
                  <button
                    onClick={() => setConfig({ ...config, distanceMethod: 'haversine' })}
                    className={`p-3 border transition-all text-left ${
                      config.distanceMethod === 'haversine'
                        ? 'border-primary bg-primary/10 text-white'
                        : 'border-white/10 text-white/40 hover:border-white/20'
                    }`}
                  >
                    <div className="text-sm font-bold font-mono mb-0.5">Haversine</div>
                    <div className="text-[11px] font-mono text-white/30">Geographic coords</div>
                  </button>
                  <button
                    onClick={() => setConfig({ ...config, distanceMethod: 'actual_maps' })}
                    className={`p-3 border transition-all text-left ${
                      config.distanceMethod === 'actual_maps'
                        ? 'border-primary bg-primary/10 text-white'
                        : 'border-white/10 text-white/40 hover:border-white/20'
                    }`}
                  >
                    <div className="text-sm font-bold font-mono mb-0.5">Map Routes</div>
                    <div className="text-[11px] font-mono text-white/30">Real road distances</div>
                  </button>
                </div>
              </div>

              {/* Cost vs Time Slider */}
              <div>
                <p className="text-[11px] font-label uppercase tracking-widest text-white/40 mb-3">Cost vs Time Priority</p>
                <div className="relative">
                  <div className="flex justify-between items-center mb-2">
                    <div className="flex items-center gap-1.5">
                      <div className="w-2.5 h-2.5 bg-primary" />
                      <span className="text-xs font-mono text-white/40">Cost: {config.costWeight.toFixed(2)}</span>
                    </div>
                    <div className="flex items-center gap-1.5">
                      <span className="text-xs font-mono text-white/40">Time: {config.timeWeight.toFixed(2)}</span>
                      <div className="w-2.5 h-2.5 bg-white" />
                    </div>
                  </div>
                  <div className="relative h-6 overflow-hidden">
                    <div className="absolute inset-0 flex">
                      <div className="bg-primary/40" style={{ width: `${config.costWeight * 100}%` }} />
                      <div className="bg-white/20" style={{ width: `${config.timeWeight * 100}%` }} />
                    </div>
                    <input
                      type="range"
                      min="0"
                      max="1"
                      step="0.01"
                      value={config.costWeight}
                      onChange={(e) => updateCostWeight(parseFloat(e.target.value))}
                      className="absolute inset-0 w-full h-full opacity-0 cursor-pointer z-10"
                    />
                    <div
                      className="absolute top-1 w-4 h-4 bg-white rounded-full shadow-lg border-2 border-primary pointer-events-none z-20"
                      style={{ left: `calc(${config.costWeight * 100}% - 8px)` }}
                    />
                  </div>
                  <div className="flex justify-between text-[10px] font-mono text-white/30 mt-1">
                    <span>Cost Focus</span>
                    <span>Balanced</span>
                    <span>Time Focus</span>
                  </div>
                </div>
              </div>

              {/* Priority Delay Limits */}
              <div>
                <div className="flex items-center gap-2 mb-3">
                  <p className="text-[11px] font-label uppercase tracking-widest text-white/40">Priority Delay Limits</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/40 text-base cursor-help hover:text-white/60 transition-colors">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-[10px] font-mono text-white/40 opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50 w-56 whitespace-normal">
                      Maximum delay allowed for each priority level before an employee is considered late.
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <div className="grid grid-cols-5 gap-2">
                  {[1, 2, 3, 4, 5].map((p) => (
                    <div key={p} className="bg-white/[0.02] p-2.5 border border-white/10">
                      <label className="block text-[10px] font-mono text-white/30 mb-1 text-center">P{p}</label>
                      <div className="flex items-center gap-1">
                        <input
                          type="number"
                          min="1"
                          max="60"
                          value={config.priorityDelays[p]}
                          onChange={(e) => updatePriorityDelay(p, parseInt(e.target.value) || 0)}
                          className="w-full px-1.5 py-1.5 bg-transparent border border-white/10 rounded-sm text-center text-sm font-mono focus:border-primary focus:outline-none text-white"
                        />
                      </div>
                      <div className="text-[9px] font-mono text-white/20 text-center mt-0.5">min</div>
                    </div>
                  ))}
                </div>
              </div>

              {/* Solver Duration */}
              <div>
                <div className="flex items-center gap-2 mb-3">
                  <p className="text-[11px] font-label uppercase tracking-widest text-white/40">Solver Duration</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/40 text-base cursor-help hover:text-white/60 transition-colors">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-[10px] font-mono text-white/40 opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50 w-56 whitespace-normal">
                      Longer runtime lets the solver explore more solutions for better savings.
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <div className="grid grid-cols-2 gap-2">
                  {([
                    { key: 'Quick' as const, time: '15s' },
                    { key: 'Standard' as const, time: '30s' },
                    { key: 'Thorough' as const, time: '1m' },
                    { key: 'Maximum' as const, time: '2m' },
                  ]).map(({ key, time }) => (
                    <button
                      key={key}
                      onClick={() => setSolverDuration(key)}
                      className={`p-3 border transition-all text-left ${
                        solverDuration === key
                          ? 'border-primary bg-primary/10 text-white'
                          : 'border-white/10 text-white/40 hover:border-white/20'
                      }`}
                    >
                      <div className="text-sm font-bold font-mono">{key}</div>
                      <div className="text-[11px] font-mono text-white/30">{time}</div>
                    </button>
                  ))}
                </div>
              </div>

            </div>

            {/* Run & Back actions */}
            <div className="p-5 border-t border-white/10 space-y-3">
              <button
                onClick={handleRunOptimization}
                className="w-full px-8 py-3 bg-primary text-background-dark font-label font-bold tracking-widest uppercase glow-amber hover:bg-primary/90 transition-all flex items-center justify-center gap-2 text-sm"
              >
                <span className="material-symbols-outlined text-lg">rocket_launch</span>
                Run Optimization ({solverDuration === 'Quick' ? '15s' : solverDuration === 'Standard' ? '30s' : solverDuration === 'Thorough' ? '1m' : '2m'})
              </button>
              <button
                onClick={() => navigate('/upload')}
                className="w-full px-6 py-3 border border-white/10 bg-white/[0.02] hover:bg-white/5 text-white/50 font-label font-bold uppercase tracking-wider transition-colors flex items-center justify-center gap-2 text-sm"
              >
                <span className="material-symbols-outlined text-lg">arrow_back</span>
                Back to Upload
              </button>
            </div>
          </motion.div>
        </div>{/* end right column */}

      </div>
    </div>
  );
}

