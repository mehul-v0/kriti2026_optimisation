import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { TrendingDown, Car, Route, Users, Clock, MapPin, ArrowRight, FileCheck, Map, DollarSign } from 'lucide-react';
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

  const cardClass = "bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40";

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
            <div className={`${cardClass} p-6 flex flex-col gap-5`}>
              <h3 className="text-sm font-bold uppercase tracking-widest text-gray/40 mb-1">Cost Breakdown</h3>

              <div>
                <p className="text-sm text-gray mb-1">Baseline Cost</p>
                <p className="text-3xl font-bold">{formatCurrency(currentResult.baselineCost)}</p>
                <p className="text-xs text-gray/50 mt-1">Individual rides total</p>
              </div>

              <div>
                <p className="text-sm text-gray mb-1">Optimized Fleet Cost</p>
                <p className="text-3xl font-bold text-blue-400">{formatCurrency(currentResult.optimizedCost)}</p>
                <p className="text-xs text-gray/50 mt-1">Shared fleet total</p>
              </div>

              <div>
                <p className="text-sm text-gray mb-1">Total Savings</p>
                <div className="flex items-center gap-3">
                  <p className="text-3xl font-bold text-primary">{formatCurrency(currentResult.savings)}</p>
                  <span className="badge badge-primary text-sm px-3 py-1">
                    <TrendingDown className="w-3.5 h-3.5 mr-1 inline" />
                    {formatPercentage(currentResult.savingsPercentage)}
                  </span>
                </div>
              </div>
            </div>

            {/* Time Metrics — Own Card */}
            <div className={`${cardClass} p-6 flex flex-col gap-5`}>
              <h3 className="text-sm font-bold uppercase tracking-widest text-gray/40 mb-1">Time Breakdown</h3>

              <div>
                <p className="text-sm text-gray mb-1">Baseline Time</p>
                <p className="text-3xl font-bold">{formatTripTime(baselineTime)}</p>
                <p className="text-xs text-gray/50 mt-1">Estimated individual rides</p>
              </div>

              <div>
                <p className="text-sm text-gray mb-1">Optimized Time</p>
                <p className="text-3xl font-bold text-blue-400">{formatTripTime(optimizedTime)}</p>
                <p className="text-xs text-gray/50 mt-1">Shared fleet total</p>
              </div>

              <div>
                <p className="text-sm text-gray mb-1">Total Reduction</p>
                <div className="flex items-center gap-3">
                  <p className="text-3xl font-bold text-primary">{formatTripTime(timeSavings)}</p>
                  <span className="badge badge-primary text-sm px-3 py-1">
                    <TrendingDown className="w-3.5 h-3.5 mr-1 inline" />
                    {timeSavingsPercent.toFixed(1)}%
                  </span>
                </div>
              </div>
            </div>

            {/* Key Stats — Own Card with 2×3 Grid */}
            <div className={`${cardClass} p-6`}>
              <h3 className="text-sm font-bold uppercase tracking-widest text-gray/40 mb-4">Key Metrics</h3>
              <div className="grid grid-cols-2 gap-4">
                {[
                  { label: 'Vehicles Utilized', value: `${vehiclesUsed} / ${currentResult.vehicles.length}`, icon: Car, color: 'text-primary' },
                  { label: 'Total Trips', value: totalTrips.toString(), icon: Route, color: 'text-primary' },
                  { label: 'Total Distance', value: `${formatNumber(Math.round(totalDistance))} km`, icon: MapPin, color: 'text-primary' },
                  { label: 'Avg Occupancy', value: avgOccupancy.toFixed(1), icon: Users, color: 'text-primary' },
                  { label: 'Employees Served', value: `${currentResult.employees.length}`, icon: Users, color: 'text-primary' },
                  { label: 'Solver Duration', value: formatDuration(currentResult.solverDuration), icon: Clock, color: 'text-primary' },
                ].map((metric) => (
                  <div key={metric.label} className={`${cardClass} p-4`}>
                    <metric.icon className={`w-5 h-5 ${metric.color} mb-1.5`} />
                    <p className="text-xs text-gray mb-0.5">{metric.label}</p>
                    <p className="text-xl font-bold">{metric.value}</p>
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
          <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
            {[
              {
                title: 'Explore Routes on Map',
                description: 'Interactive visualization of all vehicle routes',
                icon: Map,
                to: '/routes',
                color: 'text-primary',
              },
              {
                title: 'View Constraint Compliance',
                description: 'See which constraints were honored and any violations',
                icon: FileCheck,
                to: '/constraints',
                color: 'text-primary',
              },
              {
                title: 'Analyze Cost Breakdown',
                description: 'Detailed cost analysis and per-vehicle economics',
                icon: DollarSign,
                to: '/costs',
                color: 'text-primary',
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
                  <card.icon className={`w-12 h-12 ${card.color} mb-4`} />
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
