import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { Truck, Battery, Fuel, Car as CarIcon } from 'lucide-react';

export default function VehicleFleet() {
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

  const vehiclesUsed = [...new Set(currentResult.trips.map(t => t.vehicleId))].length;
  const vehiclesUnused = currentResult.vehicles.length - vehiclesUsed;

  const fuelDist = {
    Electric: currentResult.vehicles.filter(v => v.fuelType === 'Electric').length,
    Petrol: currentResult.vehicles.filter(v => v.fuelType === 'Petrol').length,
    Diesel: currentResult.vehicles.filter(v => v.fuelType === 'Diesel').length,
  };

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-7xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Vehicle Fleet Analysis</h1>
        <p className="text-gray mb-8">Deep dive into vehicle utilization and performance</p>

        {/* Fleet Summary */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="card mb-8">
          <div className="grid grid-cols-5 gap-6 text-center">
            <div>
              <Truck className="w-8 h-8 text-primary mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Total Vehicles</p>
              <p className="text-3xl font-bold">{currentResult.vehicles.length}</p>
            </div>
            <div>
              <CarIcon className="w-8 h-8 text-green-400 mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Vehicles Used</p>
              <p className="text-3xl font-bold text-green-400">{vehiclesUsed}</p>
            </div>
            <div>
              <CarIcon className="w-8 h-8 text-gray mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Vehicles Unused</p>
              <p className="text-3xl font-bold text-gray">{vehiclesUnused}</p>
            </div>
            <div>
              <Battery className="w-8 h-8 text-blue-400 mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Total Trips</p>
              <p className="text-3xl font-bold text-blue-400">{currentResult.trips.length}</p>
            </div>
            <div>
              <Fuel className="w-8 h-8 text-purple-400 mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Utilization Rate</p>
              <p className="text-3xl font-bold text-purple-400">
                {((vehiclesUsed / currentResult.vehicles.length) * 100).toFixed(0)}%
              </p>
            </div>
          </div>
        </motion.div>

        {/* Fleet Composition */}
        <div className="grid grid-cols-3 gap-6 mb-8">
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="card">
            <h3 className="font-bold mb-4">By Fuel Type</h3>
            <div className="space-y-3">
              <div>
                <div className="flex justify-between mb-1">
                  <span className="text-sm text-gray">⚡ Electric</span>
                  <span className="font-medium">{fuelDist.Electric}</span>
                </div>
                <div className="h-2 bg-dark-600 rounded-full overflow-hidden">
                  <div className="h-full bg-green-500" style={{ width: `${(fuelDist.Electric / currentResult.vehicles.length) * 100}%` }} />
                </div>
              </div>
              <div>
                <div className="flex justify-between mb-1">
                  <span className="text-sm text-gray">⛽ Petrol</span>
                  <span className="font-medium">{fuelDist.Petrol}</span>
                </div>
                <div className="h-2 bg-dark-600 rounded-full overflow-hidden">
                  <div className="h-full bg-blue-500" style={{ width: `${(fuelDist.Petrol / currentResult.vehicles.length) * 100}%` }} />
                </div>
              </div>
              <div>
                <div className="flex justify-between mb-1">
                  <span className="text-sm text-gray">🛢️ Diesel</span>
                  <span className="font-medium">{fuelDist.Diesel}</span>
                </div>
                <div className="h-2 bg-dark-600 rounded-full overflow-hidden">
                  <div className="h-full bg-orange-500" style={{ width: `${(fuelDist.Diesel / currentResult.vehicles.length) * 100}%` }} />
                </div>
              </div>
            </div>
          </motion.div>

          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }} className="card col-span-2">
            <h3 className="font-bold mb-4">Vehicle List</h3>
            <div className="grid grid-cols-2 gap-3 max-h-64 overflow-y-auto scrollbar-thin">
              {currentResult.vehicles.slice(0, 12).map((vehicle) => (
                <div key={vehicle.id} className="bg-dark-600 rounded-lg p-3">
                  <div className="flex items-center justify-between mb-1">
                    <span className="font-medium">{vehicle.id}</span>
                    <span className="text-xs text-gray">{vehicle.mode}</span>
                  </div>
                  <div className="flex gap-2">
                    <span className={`badge ${vehicle.fuelType === 'Electric' ? 'badge-success' : 'badge-info'} text-xs`}>
                      {vehicle.fuelType}
                    </span>
                    <span className="badge badge-primary text-xs">
                      Capacity: {vehicle.capacity}
                    </span>
                  </div>
                </div>
              ))}
            </div>
          </motion.div>
        </div>
      </div>
    </div>
  );
}
