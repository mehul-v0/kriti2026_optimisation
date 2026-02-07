import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { DollarSign, TrendingDown, TrendingUp } from 'lucide-react';
import { formatCurrency, formatPercentage } from '../utils/helpers';

export default function CostBreakdown() {
  const { currentResult } = useApp();

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center">
        <div className="text-center">
          <p className="text-gray mb-4">No optimization results available</p>
          <Link to="/upload" className="btn-primary">Start New Optimization</Link>
        </div>
      </div>
    );
  }

  const avgBaselineCost = currentResult.baselineCost / currentResult.employees.length;
  const avgOptimizedCost = currentResult.optimizedCost / currentResult.employees.length;
  const avgSavings = avgBaselineCost - avgOptimizedCost;

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-7xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Cost & Savings Breakdown</h1>
        <p className="text-gray mb-8">Complete financial analysis with transparent calculations</p>

        {/* Top-Level Summary */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
          <div className="card bg-gradient-to-br from-dark-700 to-dark-800">
            <DollarSign className="w-8 h-8 text-gray mb-3" />
            <p className="text-sm text-gray mb-2">Total Baseline Cost</p>
            <p className="text-3xl font-bold mb-2">{formatCurrency(currentResult.baselineCost)}</p>
            <p className="text-xs text-gray">Sum of baseline costs for all employees from input data</p>
          </div>

          <div className="card bg-gradient-to-br from-dark-700 to-dark-800">
            <TrendingDown className="w-8 h-8 text-blue-400 mb-3" />
            <p className="text-sm text-gray mb-2">Total Optimized Cost</p>
            <p className="text-3xl font-bold text-blue-400 mb-2">{formatCurrency(currentResult.optimizedCost)}</p>
            <p className="text-xs text-gray">Fleet operational cost after optimization</p>
          </div>

          <div className="card bg-gradient-to-br from-primary/10 to-primary/5 border-primary/30">
            <TrendingUp className="w-8 h-8 text-primary mb-3" />
            <p className="text-sm text-gray mb-2">Net Savings</p>
            <p className="text-3xl font-bold text-primary mb-2">{formatCurrency(currentResult.savings)}</p>
            <span className="badge badge-primary">
              {formatPercentage(currentResult.savingsPercentage)} reduction
            </span>
          </div>
        </motion.div>

        {/* Cost Methodology */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="card mb-8">
          <h2 className="text-2xl font-bold mb-4">How We Calculate Costs</h2>
          <div className="space-y-4 text-gray">
            <div>
              <h3 className="text-white font-semibold mb-2">Baseline Cost</h3>
              <p className="text-sm">
                For each employee, the baseline cost is provided directly in the input data. This represents the 
                estimated cost of an individual ride for that employee. The total baseline cost is the sum of all 
                individual baseline costs.
              </p>
            </div>
            <div>
              <h3 className="text-white font-semibold mb-2">Optimized Cost</h3>
              <p className="text-sm">
                The optimized cost is calculated as the sum of operational costs for all vehicles used:
              </p>
              <ul className="text-sm list-disc list-inside ml-4 mt-2 space-y-1">
                <li>For each vehicle: Distance traveled (km) × Cost per kilometer (from vehicle data)</li>
                <li>Total = Sum across all vehicles and all trips</li>
              </ul>
            </div>
            <div>
              <h3 className="text-white font-semibold mb-2">Savings</h3>
              <p className="text-sm">
                The difference between baseline and optimized costs represents your cost advantage.
              </p>
            </div>
          </div>
        </motion.div>

        {/* Per-Employee Cost Impact */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }} className="card mb-8">
          <h2 className="text-2xl font-bold mb-6">Per-Employee Cost Impact</h2>
          <div className="grid grid-cols-3 gap-6">
            <div className="text-center">
              <p className="text-sm text-gray mb-2">Average Baseline Cost</p>
              <p className="text-3xl font-bold">{formatCurrency(avgBaselineCost)}</p>
            </div>
            <div className="text-center">
              <p className="text-sm text-gray mb-2">Average Optimized Cost</p>
              <p className="text-3xl font-bold text-blue-400">{formatCurrency(avgOptimizedCost)}</p>
            </div>
            <div className="text-center">
              <p className="text-sm text-gray mb-2">Average Savings per Employee</p>
              <p className="text-3xl font-bold text-primary">{formatCurrency(avgSavings)}</p>
            </div>
          </div>
        </motion.div>

        {/* Projection Calculator */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.3 }} className="card">
          <h2 className="text-2xl font-bold mb-4">If this optimization were applied regularly:</h2>
          <div className="grid grid-cols-3 gap-6">
            <div className="bg-dark-600 rounded-lg p-6 text-center">
              <p className="text-sm text-gray mb-2">Daily Savings</p>
              <p className="text-3xl font-bold text-primary">{formatCurrency(currentResult.savings)}</p>
            </div>
            <div className="bg-dark-600 rounded-lg p-6 text-center">
              <p className="text-sm text-gray mb-2">Monthly Projection</p>
              <p className="text-3xl font-bold text-primary">{formatCurrency(currentResult.savings * 22)}</p>
              <p className="text-xs text-gray mt-1">22 working days</p>
            </div>
            <div className="bg-dark-600 rounded-lg p-6 text-center">
              <p className="text-sm text-gray mb-2">Annual Projection</p>
              <p className="text-3xl font-bold text-primary">{formatCurrency(currentResult.savings * 250)}</p>
              <p className="text-xs text-gray mt-1">250 working days</p>
            </div>
          </div>
        </motion.div>
      </div>
    </div>
  );
}
