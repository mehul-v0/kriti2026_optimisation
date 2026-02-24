import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { TrendingDown, Car, Route, Users, Clock, MapPin, ArrowRight, FileCheck, Map, Info, DollarSign, BarChart3, Truck } from 'lucide-react';
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
          <p className="text-gray mb-4">No optimization results available. Run optimization first.</p>
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

  const cardClass = "bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-float hover:shadow-float-lg transition-all duration-300 ";

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="flex items-center justify-between mb-8">
          <div>
            <h1 className="text-4xl font-bold mb-2">Optimization Results</h1>
            <p className="text-gray">{formatDate(currentResult.timestamp)}</p>
          </div>
        </div>

        {/* Fixed New Optimization Button */}
        <Link to="/upload" className="fixed bottom-6 right-6 z-50 btn-primary flex items-center gap-2 px-5 py-3 rounded-xl">
          New Optimization
        </Link>

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
              <DollarSign className="absolute -right-4 top-6 w-32 h-32 text-primary/10 pointer-events-none z-0" />
              
              <h3 className="text-sm font-bold uppercase tracking-widest text-white/90 mb-1 relative z-10">Cost Breakdown</h3>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-sm text-gray">Baseline Cost</p>
                  <div className="relative group/info">
                    <Info className="w-3.5 h-3.5 text-gray/50 hover:text-gray cursor-help transition-colors" />
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/70 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Individual rides total
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold">{formatCurrency(currentResult.baselineCost)}</p>
              </div>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-sm text-gray">Optimized Fleet Cost</p>
                  <div className="relative group/info">
                    <Info className="w-3.5 h-3.5 text-gray/50 hover:text-gray cursor-help transition-colors" />
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/70 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Shared fleet total
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold text-blue-400">{formatCurrency(currentResult.optimizedCost)}</p>
              </div>

              <div className="relative z-10">
                <p className="text-sm text-gray mb-1">Total Savings</p>
                <div className="flex items-center gap-3">
                  <p className="text-3xl font-bold text-primary-bright">{formatCurrency(currentResult.savings)}</p>
                  <span className="badge badge-primary text-sm px-3 py-1">
                    <TrendingDown className="w-3.5 h-3.5 mr-1 inline" />
                    {formatPercentage(currentResult.savingsPercentage)}
                  </span>
                </div>
              </div>
            </div>

            {/* Time Metrics — Own Card */}
            <div className={`${cardClass} p-6 flex flex-col gap-5 relative overflow-hidden`}>
              {/* Background icon */}
              <Clock className="absolute -right-4 top-6 w-32 h-32 text-primary/10 pointer-events-none z-0" />
              
              <h3 className="text-sm font-bold uppercase tracking-widest text-white/90 mb-1 relative z-10">Time Breakdown</h3>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-sm text-gray">Baseline Time</p>
                  <div className="relative group/info">
                    <Info className="w-3.5 h-3.5 text-gray/50 hover:text-gray cursor-help transition-colors" />
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/70 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Estimated individual rides
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold">{formatTripTime(baselineTime)}</p>
              </div>

              <div className="relative z-10">
                <div className="flex items-center gap-2 mb-1">
                  <p className="text-sm text-gray">Optimized Time</p>
                  <div className="relative group/info">
                    <Info className="w-3.5 h-3.5 text-gray/50 hover:text-gray cursor-help transition-colors" />
                    <div className="absolute left-1/2 -translate-x-1/2 bottom-full mb-2 px-3 py-2 bg-dark-800 border border-gray/20 rounded-lg text-xs text-gray/70 whitespace-nowrap opacity-0 invisible group-hover/info:opacity-100 group-hover/info:visible transition-all duration-200 z-50">
                      Shared fleet total
                      <div className="absolute left-1/2 -translate-x-1/2 top-full w-0 h-0 border-l-4 border-r-4 border-t-4 border-transparent border-t-gray/20" />
                    </div>
                  </div>
                </div>
                <p className="text-3xl font-bold text-blue-400">{formatTripTime(optimizedTime)}</p>
              </div>

              <div className="relative z-10">
                <p className="text-sm text-gray mb-1">Total Reduction</p>
                <div className="flex items-center gap-3">
                  <p className="text-3xl font-bold text-primary-bright">{formatTripTime(timeSavings)}</p>
                  <span className="badge badge-primary text-sm px-3 py-1">
                    <TrendingDown className="w-3.5 h-3.5 mr-1 inline" />
                    {timeSavingsPercent.toFixed(1)}%
                  </span>
                </div>
              </div>
            </div>

            {/* Key Stats — Own Card with 2×3 Grid */}
            <div className={`${cardClass} p-6`}>
              <h3 className="text-sm font-bold uppercase tracking-widest text-white/90 mb-4">Key Metrics</h3>
              <div className="grid grid-cols-2 gap-4">
                {[
                  { label: 'Vehicles Utilized', value: `${vehiclesUsed} / ${currentResult.vehicles.length}`, icon: Car, color: 'text-primary-muted' },
                  { label: 'Total Trips', value: totalTrips.toString(), icon: Route, color: 'text-primary' },
                  { label: 'Total Distance', value: `${formatNumber(Math.round(totalDistance))} km`, icon: MapPin, color: 'text-primary-muted' },
                  { label: 'Avg Occupancy', value: avgOccupancy.toFixed(1), icon: Users, color: 'text-primary' },
                  { label: 'Employees Served', value: `${currentResult.employees.length}`, icon: Users, color: 'text-primary-muted' },
                  { label: 'Solver Duration', value: formatDuration(currentResult.solverDuration), icon: Clock, color: 'text-primary-muted' },
                ].map((metric) => (
                  <div key={metric.label} className={`${cardClass} p-4 relative overflow-hidden`}>
                    {/* Background icon - bottom-right corner */}
                    <metric.icon className="absolute right-0 bottom-0 w-20 h-20 text-primary/10 pointer-events-none" />
                    
                    {/* Content */}
                    <div className="relative z-10 flex flex-col h-full">
                      <p className="text-sm text-gray mb-2">{metric.label}</p>
                      <p className="text-2xl font-bold">{metric.value}</p>
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
          <h2 className="text-2xl font-bold mb-6">Explore Details</h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
            {[
              {
                title: 'Explore Routes on Map',
                description: 'Interactive visualization of all vehicle routes',
                icon: Map,
                to: '/routes',
              },
              {
                title: 'View Constraint Compliance',
                description: 'See which constraints were honored and any violations',
                icon: FileCheck,
                to: '/constraints',
              },
              {
                title: 'Vehicle Analysis',
                description: 'Detailed breakdown of vehicle utilization and performance',
                icon: Truck,
                to: '/fleet',
              },
              {
                title: 'Fleet Analysis',
                description: 'Comprehensive fleet efficiency and optimization metrics',
                icon: BarChart3,
                to: '/fleet',
              },
            ].map((card, index) => (
              <Link key={card.title} to={card.to}>
                <motion.div
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: 0.3 + index * 0.1 }}
                  whileHover={{ scale: 1.02 }}
                  className={`${cardClass} p-6 hover:border-gray/20 hover:scale-[1.02] cursor-pointer h-full group transition-all duration-300`}
                >
                  <card.icon className="w-12 h-12 text-primary mb-4" />
                  <h3 className="text-xl font-bold mb-2 flex items-center justify-between">
                    {card.title}
                    <ArrowRight className="w-5 h-5 text-gray group-hover:text-primary transition-colors" />
                  </h3>
                  <p className="text-gray text-sm">{card.description}</p>
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
          <h3 className="text-xl font-bold mb-4">Session Summary</h3>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-6 text-sm">
            <div>
              <p className="text-gray mb-1">Solver Mode</p>
              <p className="font-medium">{currentResult.solverMode}</p>
            </div>
            <div>
              <p className="text-gray mb-1">Duration</p>
              <p className="font-medium">{formatDuration(currentResult.solverDuration)}</p>
            </div>
            <div>
              <p className="text-gray mb-1">Input File</p>
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

