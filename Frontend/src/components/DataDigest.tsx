import { Users, Car, Clock, TrendingUp, Fuel, Package } from 'lucide-react';
import type { DataDigest } from '../types';

interface DataDigestProps {
  digest: DataDigest;
}

export function DataDigestView({ digest }: DataDigestProps) {
  return (
    <div className="bg-white rounded-xl shadow-lg p-6 space-y-6">
      <h2 className="text-xl font-bold text-slate-800 border-b pb-3">Data Digest</h2>

      <div className="grid grid-cols-2 md:grid-cols-3 gap-4">
        <div className="bg-gradient-to-br from-blue-50 to-blue-100 rounded-lg p-4 border border-blue-200">
          <div className="flex items-center space-x-3">
            <Users className="w-8 h-8 text-blue-600" />
            <div>
              <p className="text-2xl font-bold text-blue-900">{digest.employees_count}</p>
              <p className="text-xs text-blue-700">Employees</p>
            </div>
          </div>
        </div>

        <div className="bg-gradient-to-br from-emerald-50 to-emerald-100 rounded-lg p-4 border border-emerald-200">
          <div className="flex items-center space-x-3">
            <Car className="w-8 h-8 text-emerald-600" />
            <div>
              <p className="text-2xl font-bold text-emerald-900">{digest.vehicles_count}</p>
              <p className="text-xs text-emerald-700">Vehicles</p>
            </div>
          </div>
        </div>

        <div className="bg-gradient-to-br from-amber-50 to-amber-100 rounded-lg p-4 border border-amber-200">
          <div className="flex items-center space-x-3">
            <Clock className="w-8 h-8 text-amber-600" />
            <div>
              <p className="text-sm font-bold text-amber-900">{digest.time_window_span}</p>
              <p className="text-xs text-amber-700">Time Window</p>
            </div>
          </div>
        </div>

        <div className="bg-gradient-to-br from-purple-50 to-purple-100 rounded-lg p-4 border border-purple-200">
          <div className="flex items-center space-x-3">
            <TrendingUp className="w-8 h-8 text-purple-600" />
            <div>
              <p className="text-2xl font-bold text-purple-900">{digest.high_priority_percent.toFixed(0)}%</p>
              <p className="text-xs text-purple-700">High Priority</p>
            </div>
          </div>
        </div>

        <div className="bg-gradient-to-br from-cyan-50 to-cyan-100 rounded-lg p-4 border border-cyan-200">
          <div className="flex items-center space-x-3">
            <Fuel className="w-8 h-8 text-cyan-600" />
            <div>
              <p className="text-xs font-medium text-cyan-900">
                EV: {digest.fleet_composition.electric} | P: {digest.fleet_composition.petrol} | D:{' '}
                {digest.fleet_composition.diesel}
              </p>
              <p className="text-xs text-cyan-700">Fleet Composition</p>
            </div>
          </div>
        </div>

        <div className="bg-gradient-to-br from-orange-50 to-orange-100 rounded-lg p-4 border border-orange-200">
          <div className="flex items-center space-x-3">
            <Package className="w-8 h-8 text-orange-600" />
            <div>
              <p className="text-xs font-medium text-orange-900">
                2W: {digest.vehicle_modes['2-wheeler']} | 4W: {digest.vehicle_modes['4-wheeler']} | Van:{' '}
                {digest.vehicle_modes.van}
              </p>
              <p className="text-xs text-orange-700">Vehicle Types</p>
            </div>
          </div>
        </div>
      </div>

      <div className="bg-emerald-50 border border-emerald-200 rounded-lg p-4">
        <p className="text-sm text-emerald-800 font-medium">
          Data validation successful. All constraints loaded. Ready for optimization.
        </p>
      </div>
    </div>
  );
}
