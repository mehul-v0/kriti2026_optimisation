import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { TrendingDown, Car, Route, Users, Clock, MapPin, ArrowRight, FileCheck, Map, DollarSign } from 'lucide-react';
import { formatCurrency, formatNumber, formatDate, formatPercentage } from '../utils/helpers';

export default function ResultsOverview() {
  const { currentResult } = useApp();

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center">
        <div className="text-center">
          <p className="text-gray mb-4">No optimization results available</p>
          <Link to="/upload" className="btn-primary">
            Start New Optimization
          </Link>
        </div>
      </div>
    );
  }

  const vehiclesUsed = [...new Set(currentResult.trips.map(t => t.vehicleId))].length;
  const totalTrips = currentResult.trips.length;
  const totalDistance = currentResult.trips.reduce((sum, trip) => sum + trip.distance, 0);
  const totalPassengers = currentResult.assignments.length;
  const avgOccupancy = totalPassengers / totalTrips;

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="flex items-center justify-between mb-8">
          <div>
            <h1 className="text-4xl font-bold mb-2">Optimization Results</h1>
            <p className="text-gray">{formatDate(currentResult.timestamp)}</p>
          </div>
          <div className="flex gap-3">
            <Link to="/upload" className="btn-secondary">
              New Optimization
            </Link>
          </div>
        </div>

        {/* Primary Savings Summary */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="card mb-8"
        >
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            <div className="text-center">
              <p className="text-sm text-gray mb-2">Baseline Cost (Individual Rides)</p>
              <p className="text-4xl font-bold mb-2">{formatCurrency(currentResult.baselineCost)}</p>
              <p className="text-xs text-gray">Sum of baseline costs from input data</p>
            </div>

            <div className="text-center">
              <p className="text-sm text-gray mb-2">Optimized Fleet Cost</p>
              <p className="text-4xl font-bold text-blue-400 mb-2">{formatCurrency(currentResult.optimizedCost)}</p>
              <p className="text-xs text-gray">Total operational cost with optimization</p>
            </div>

            <div className="text-center">
              <div className="inline-flex items-center gap-2 mb-2">
                <TrendingDown className="w-8 h-8 text-primary" />
              </div>
              <p className="text-sm text-gray mb-2">Total Savings</p>
              <p className="text-4xl font-bold text-primary mb-2">{formatCurrency(currentResult.savings)}</p>
              <span className="badge badge-primary text-base px-4 py-2">
                {formatPercentage(currentResult.savingsPercentage)} reduction
              </span>
              <p className="text-xs text-gray mt-2">Your cost advantage</p>
            </div>
          </div>
        </motion.div>

        {/* Key Performance Metrics */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.1 }}
          className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4 mb-8"
        >
          {[
            { label: 'Vehicles Utilized', value: `${vehiclesUsed} of ${currentResult.vehicles.length}`, icon: Car, color: 'text-blue-400' },
            { label: 'Total Trips', value: totalTrips.toString(), icon: Route, color: 'text-purple-400' },
            { label: 'Total Distance', value: `${formatNumber(Math.round(totalDistance))} km`, icon: MapPin, color: 'text-green-400' },
            { label: 'Avg Occupancy', value: avgOccupancy.toFixed(1), icon: Users, color: 'text-yellow-400' },
            { label: 'Employees Served', value: `${currentResult.employees.length}/${currentResult.employees.length}`, icon: Users, color: 'text-pink-400' },
            { label: 'Solver Duration', value: `${Math.floor(currentResult.solverDuration / 60)}m`, icon: Clock, color: 'text-orange-400' },
          ].map((metric) => (
            <div key={metric.label} className="card">
              <metric.icon className={`w-6 h-6 ${metric.color} mb-2`} />
              <p className="text-sm text-gray mb-1">{metric.label}</p>
              <p className="text-2xl font-bold">{metric.value}</p>
            </div>
          ))}
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
                color: 'text-blue-400',
              },
              {
                title: 'View Constraint Compliance',
                description: 'See which constraints were honored and any violations',
                icon: FileCheck,
                to: '/constraints',
                color: 'text-green-400',
              },
              {
                title: 'Analyze Cost Breakdown',
                description: 'Detailed cost analysis and per-vehicle economics',
                icon: DollarSign,
                to: '/costs',
                color: 'text-purple-400',
              },
            ].map((card, index) => (
              <Link key={card.title} to={card.to}>
                <motion.div
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: 0.3 + index * 0.1 }}
                  whileHover={{ scale: 1.02 }}
                  className="card-hover h-full group"
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
          className="card"
        >
          <h3 className="text-xl font-bold mb-4">Session Summary</h3>
          <div className="grid grid-cols-2 md:grid-cols-4 gap-6 text-sm">
            <div>
              <p className="text-gray mb-1">Solver Mode</p>
              <p className="font-medium">{currentResult.solverMode}</p>
            </div>
            <div>
              <p className="text-gray mb-1">Duration</p>
              <p className="font-medium">{Math.floor(currentResult.solverDuration / 60)} minutes</p>
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
