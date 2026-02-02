import { DollarSign, TrendingDown, Clock, Car } from 'lucide-react';
import type { OptimizationResult } from '../types';

interface KPIDashboardProps {
  result: OptimizationResult;
}

export function KPIDashboard({ result }: KPIDashboardProps) {
  return (
    <div className="bg-white rounded-xl shadow-lg p-6">
      <h2 className="text-xl font-bold text-slate-800 border-b pb-3 mb-6">Optimization Results</h2>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <div className="bg-gradient-to-br from-emerald-500 to-emerald-600 rounded-xl p-6 text-white shadow-lg">
          <div className="flex items-start justify-between mb-4">
            <DollarSign className="w-10 h-10 opacity-80" />
            <div className="bg-white/20 px-3 py-1 rounded-full text-xs font-medium">Optimized</div>
          </div>
          <p className="text-3xl font-bold mb-1">₹{result.total_cost.toFixed(0)}</p>
          <p className="text-emerald-100 text-sm">Total Cost</p>
        </div>

        <div className="bg-gradient-to-br from-blue-500 to-blue-600 rounded-xl p-6 text-white shadow-lg">
          <div className="flex items-start justify-between mb-4">
            <TrendingDown className="w-10 h-10 opacity-80" />
            <div className="bg-white/20 px-3 py-1 rounded-full text-xs font-medium">Savings</div>
          </div>
          <p className="text-3xl font-bold mb-1">{result.cost_savings_percent.toFixed(1)}%</p>
          <p className="text-blue-100 text-sm">₹{result.cost_savings.toFixed(0)} saved</p>
        </div>

        <div className="bg-gradient-to-br from-amber-500 to-amber-600 rounded-xl p-6 text-white shadow-lg">
          <div className="flex items-start justify-between mb-4">
            <Clock className="w-10 h-10 opacity-80" />
            <div className="bg-white/20 px-3 py-1 rounded-full text-xs font-medium">Time</div>
          </div>
          <p className="text-3xl font-bold mb-1">{(result.total_time / 60).toFixed(1)}h</p>
          <p className="text-amber-100 text-sm">{result.total_distance.toFixed(1)} km</p>
        </div>

        <div className="bg-gradient-to-br from-purple-500 to-purple-600 rounded-xl p-6 text-white shadow-lg">
          <div className="flex items-start justify-between mb-4">
            <Car className="w-10 h-10 opacity-80" />
            <div className="bg-white/20 px-3 py-1 rounded-full text-xs font-medium">Fleet</div>
          </div>
          <p className="text-3xl font-bold mb-1">
            {result.vehicles_used}/{result.vehicles_available}
          </p>
          <p className="text-purple-100 text-sm">Vehicles Used</p>
        </div>
      </div>

      <div className="mt-6 bg-gradient-to-r from-slate-50 to-slate-100 rounded-lg p-4 border border-slate-200">
        <div className="flex items-center justify-between">
          <div>
            <p className="text-sm text-slate-600">Baseline Cost (Market Rate)</p>
            <p className="text-2xl font-bold text-slate-800">₹{result.baseline_cost.toFixed(0)}</p>
          </div>
          <div className="text-right">
            <p className="text-sm text-slate-600">Cost Efficiency</p>
            <p className="text-2xl font-bold text-emerald-600">{(100 - result.cost_savings_percent).toFixed(1)}%</p>
          </div>
        </div>
      </div>
    </div>
  );
}
