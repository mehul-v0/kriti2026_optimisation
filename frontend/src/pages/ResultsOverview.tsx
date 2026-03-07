import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { formatCurrency, formatNumber, formatDate, formatPercentage } from '../utils/helpers';

function formatDuration(seconds: number): string {
  if (seconds <= 0) return '0s';
  const m = Math.floor(seconds / 60);
  const s = seconds % 60;
  if (m === 0) return `${s}s`;
  if (s === 0) return `${m}m`;
  return `${m}m ${s}s`;
}

function formatTripTime(minutes: number): string {
  if (minutes <= 0) return '0 min';
  const h = Math.floor(minutes / 60);
  const m = Math.round(minutes % 60);
  if (h === 0) return `${m} min`;
  if (m === 0) return `${h}h`;
  return `${h}h ${m}m`;
}

export default function ResultsOverview() {
  const { currentResult } = useApp();

  // Block access if processing hasn't completed
  const processingDone = sessionStorage.getItem('optimizationComplete') === 'true';

  if (!processingDone || !currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center">
        <div className="text-center">
          <p className="text-white/30 mb-4">No optimization results available. Run optimization first.</p>
          <Link to="/insights" className="btn-primary">
            Go to Data Insights
          </Link>
        </div>
      </div>
    );
  }

  const vehiclesUsed = [...new Set(currentResult.trips.map(t => t.vehicleId))].length;
  const totalTrips = currentResult.trips.length;
  const totalDistance = currentResult.trips.reduce((sum, trip) => sum + trip.distance, 0);
  const totalPassengers = currentResult.assignments.length;
  const avgOccupancy = totalTrips > 0 ? totalPassengers / totalTrips : 0;

  // Time metrics — from backend
  const optimizedTime = currentResult.totalTime;
  const baselineTime = currentResult.baselineTime;
  const timeSavings = baselineTime - optimizedTime;
  const timeSavingsPercent = baselineTime > 0 ? ((timeSavings / baselineTime) * 100) : 0;

  const cardClass = "bg-panel-dark border border-white/10";

  return (
    <div className="min-h-screen p-8">
      <div className="max-w-[1400px] mx-auto">
        {/* Header */}
        <div className="flex items-center justify-between mb-8">
          <div>
            <h1 className="text-3xl font-black text-white tracking-tight uppercase mb-2">Optimization Results</h1>
            <p className="text-xs font-mono text-white/30">{formatDate(currentResult.timestamp)}</p>
          </div>
        </div>



        {/* Primary Summary — Three Column Layout */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="mb-8"
        >
          <div className="grid grid-cols-1 lg:grid-cols-[1fr_1fr_1.5fr] gap-6 lg:gap-6">

            {/* Cost Metrics — Own Card */}
            <div className={`${cardClass} p-6 flex flex-col gap-5 relative overflow-hidden`}>
              {/* Background icon */}
              <span className="material-symbols-outlined absolute right-5 top-6 text-[128px] text-primary/10 pointer-events-none z-0">payments</span>
              
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-1 relative z-10">Cost Breakdown</h3>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-xs font-mono text-white/30">Optimized Fleet Cost</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/20 hover:text-white/50 cursor-help transition-colors text-sm">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-xs text-white/30 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Shared fleet total
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold font-mono text-primary/70">{formatCurrency(currentResult.optimizedCost)}</p>
              </div>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-xs font-mono text-white/30">Baseline Cost</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/20 hover:text-white/50 cursor-help transition-colors text-sm">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-xs text-white/30 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Individual rides total
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold font-mono line-through text-white/30">{formatCurrency(currentResult.baselineCost)}</p>
              </div>

              <div className="relative z-10">
                <p className="text-xs font-mono text-white/30 mb-1">Total Savings</p>
                <div className="flex items-center gap-3">
                  <p className="text-3xl font-bold font-mono text-primary">{formatCurrency(currentResult.savings)}</p>
                  <span className="inline-flex items-center gap-1 bg-primary/15 text-primary text-sm px-3 py-1 font-medium">
                    <span className="material-symbols-outlined text-sm">trending_down</span>
                    {formatPercentage(currentResult.savingsPercentage)}
                  </span>
                </div>
              </div>
            </div>

            {/* Time Metrics — Own Card */}
            <div className={`${cardClass} p-6 flex flex-col gap-5 relative overflow-hidden`}>
              {/* Background icon */}
              <span className="material-symbols-outlined absolute right-5 top-6 text-[128px] text-primary/10 pointer-events-none z-0">schedule</span>
              
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-1 relative z-10">Time Breakdown</h3>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-xs font-mono text-white/30">Optimized Time</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/20 hover:text-white/50 cursor-help transition-colors text-sm">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-xs text-white/30 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Shared fleet total
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold font-mono text-primary/70">{formatTripTime(optimizedTime)}</p>
              </div>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-xs font-mono text-white/30">Baseline Time</p>
                  <div className="relative group/info">
                    <span className="material-symbols-outlined text-white/20 hover:text-white/50 cursor-help transition-colors text-sm">info</span>
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-panel-dark border border-white/10 text-xs text-white/30 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Estimated individual rides
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-white/10" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold font-mono line-through text-white/30">{formatTripTime(baselineTime)}</p>
              </div>

              <div className="relative z-10">
                <p className="text-xs font-mono text-white/30 mb-1">Total Reduction</p>
                <div className="flex items-center gap-3">
                  <p className="text-3xl font-bold font-mono text-primary">{formatTripTime(timeSavings)}</p>
                  <span className="inline-flex items-center gap-1 bg-primary/15 text-primary text-sm px-3 py-1 font-medium">
                    <span className="material-symbols-outlined text-sm">trending_down</span>
                    {timeSavingsPercent.toFixed(1)}%
                  </span>
                </div>
              </div>
            </div>

            {/* Key Stats — Own Card with 2×3 Grid */}
            <div className={`${cardClass} p-6`}>
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Key Metrics</h3>
              <div className="grid grid-cols-2 gap-4">
                {[
                  { label: 'Vehicles Utilized', value: `${vehiclesUsed} / ${currentResult.vehicles.length}`, icon: 'directions_car', color: 'text-primary/70' },
                  { label: 'Total Trips', value: totalTrips.toString(), icon: 'route', color: 'text-primary' },
                  { label: 'Total Distance', value: `${formatNumber(Math.round(totalDistance))} km`, icon: 'location_on', color: 'text-primary/70' },
                  { label: 'Avg Occupancy', value: avgOccupancy.toFixed(1), icon: 'groups', color: 'text-primary' },
                  { label: 'Employees Served', value: `${currentResult.employees.length}`, icon: 'person', color: 'text-primary/70' },
                  { label: 'Solver Duration', value: formatDuration(currentResult.solverDuration), icon: 'timer', color: 'text-primary/70' },
                ].map((metric) => (
                  <div key={metric.label} className={`${cardClass} p-4 relative overflow-hidden`}>
                    {/* Background icon - bottom-right corner */}
                    <span className={`material-symbols-outlined absolute right-0 bottom-0 text-[80px] text-primary/10 pointer-events-none`}>{metric.icon}</span>
                    
                    {/* Content */}
                    <div className="relative z-10 flex flex-col h-full">
                      <p className="text-xs font-mono text-white/30 mb-2">{metric.label}</p>
                      <p className="text-2xl font-bold font-mono">{metric.value}</p>
                    </div>
                  </div>
                ))}
              </div>
            </div>

          </div>
        </motion.div>

        {/* Quick Navigation Cards */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.2 }}
        >
          <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-6">Explore Details</h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
            {[
              {
                title: 'Explore Routes on Map',
                description: 'Interactive visualization of all vehicle routes',
                icon: 'map',
                to: '/routes',
              },
              {
                title: 'View Constraint Compliance',
                description: 'See which constraints were honored and any violations',
                icon: 'verified_user',
                to: '/constraints',
              },
              {
                title: 'Vehicle Analysis',
                description: 'Detailed breakdown of vehicle utilization and performance',
                icon: 'local_shipping',
                to: '/fleet',
              },
              {
                title: 'Fleet Analysis',
                description: 'Comprehensive fleet efficiency and optimization metrics',
                icon: 'bar_chart',
                to: '/fleet',
              },
            ].map((card, index) => (
              <Link key={card.title} to={card.to}>
                <motion.div
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: 0.3 + index * 0.1 }}
                  whileHover={{ scale: 1.02 }}
                  className={`${cardClass} p-6 hover:border-primary/30 cursor-pointer h-full group transition-all duration-300`}
                >
                  <span className="material-symbols-outlined text-primary text-3xl mb-4 block">{card.icon}</span>
                  <h3 className="text-sm font-label font-bold uppercase tracking-wider text-white mb-2 flex items-center justify-between">
                    {card.title}
                    <span className="material-symbols-outlined text-white/20 group-hover:text-primary transition-colors text-xl">arrow_forward</span>
                  </h3>
                  <p className="text-xs font-mono text-white/30">{card.description}</p>
                </motion.div>
              </Link>
            ))}
          </div>
        </motion.div>

        {/* Session Summary */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.4 }}
          className={`${cardClass} p-6`}
        >
          <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Session Summary</h3>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-6 text-sm">
            <div>
              <p className="text-white/30 mb-1">Solver Mode</p>
              <p className="font-medium">{currentResult.solverMode}</p>
            </div>
            <div>
              <p className="text-white/30 mb-1">Duration</p>
              <p className="font-medium">{formatDuration(currentResult.solverDuration)}</p>
            </div>
            <div>
              <p className="text-white/30 mb-1">Input File</p>
              <p className="font-medium">{currentResult.inputFile}</p>
            </div>
            <div>
              <Link to="/export" className="btn-primary w-full">
                Download Full Report
              </Link>
            </div>
          </div>
        </motion.div>
      </div>
    </div>
  );
}

