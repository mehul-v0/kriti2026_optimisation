import { useState } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../context/AppContext';
import { motion } from 'framer-motion';
import { Map as MapIcon, MapPin, Route as RouteIcon, ChevronRight } from 'lucide-react';
import { getRouteColors, formatNumber } from '../utils/helpers';

export default function RouteExplorer() {
  const { currentResult } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);

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

  // Group trips by vehicle
  const vehicleTrips = new Map<string, typeof currentResult.trips>();
  for (const trip of currentResult.trips) {
    if (!vehicleTrips.has(trip.vehicleId)) vehicleTrips.set(trip.vehicleId, []);
    vehicleTrips.get(trip.vehicleId)!.push(trip);
  }

  const vehicleIds = Array.from(vehicleTrips.keys());
  const colors = getRouteColors(vehicleIds.length);
  const selectedTrips = selectedVehicle ? vehicleTrips.get(selectedVehicle) || [] : currentResult.trips;

  return (
    <div className="h-screen bg-dark flex flex-col">
      <div className="p-6 border-b border-gray/20">
        <h1 className="text-3xl font-bold flex items-center gap-3">
          <MapIcon className="w-8 h-8 text-primary" />
          Route Explorer
        </h1>
        <p className="text-gray mt-1">
          {currentResult.trips.length} trips across {vehicleIds.length} vehicles
        </p>
      </div>

      <div className="flex-1 flex overflow-hidden">
        {/* Sidebar — Vehicle Selector */}
        <div className="w-80 border-r border-gray/20 overflow-y-auto p-4 space-y-3">
          <button
            onClick={() => setSelectedVehicle(null)}
            className={`w-full text-left p-3 rounded-lg transition-all ${
              !selectedVehicle ? 'bg-primary/20 border border-primary/40' : 'bg-dark-700 hover:bg-dark-600'
            }`}
          >
            <div className="flex items-center gap-2">
              <RouteIcon className="w-5 h-5 text-primary" />
              <span className="font-medium">All Routes</span>
            </div>
            <p className="text-xs text-gray mt-1">
              {currentResult.trips.length} total trips
            </p>
          </button>

          {vehicleIds.map((vehicleId, idx) => {
            const trips = vehicleTrips.get(vehicleId) || [];
            const totalDist = trips.reduce((s, t) => s + t.distance, 0);
            const totalEmp = new Set(trips.flatMap(t => t.employees)).size;
            return (
              <button
                key={vehicleId}
                onClick={() => setSelectedVehicle(vehicleId)}
                className={`w-full text-left p-3 rounded-lg transition-all ${
                  selectedVehicle === vehicleId ? 'bg-primary/20 border border-primary/40' : 'bg-dark-700 hover:bg-dark-600'
                }`}
              >
                <div className="flex items-center gap-2">
                  <div className="w-3 h-3 rounded-full" style={{ backgroundColor: colors[idx % colors.length] }} />
                  <span className="font-medium">{vehicleId}</span>
                  <ChevronRight className="w-4 h-4 text-gray ml-auto" />
                </div>
                <div className="flex gap-4 text-xs text-gray mt-1">
                  <span>{trips.length} trip{trips.length > 1 ? 's' : ''}</span>
                  <span>{totalEmp} employee{totalEmp > 1 ? 's' : ''}</span>
                  <span>{formatNumber(Math.round(totalDist))} km</span>
                </div>
              </button>
            );
          })}
        </div>

        {/* Main content — Trip details  */}
        <div className="flex-1 overflow-y-auto p-6">
          <div className="space-y-4">
            {selectedTrips.length === 0 ? (
              <div className="text-center text-gray py-12">
                <MapPin className="w-16 h-16 text-gray/30 mx-auto mb-4" />
                <p>No routes available for this selection</p>
              </div>
            ) : (
              selectedTrips.map((trip, tIdx) => {
                const vIdx = vehicleIds.indexOf(trip.vehicleId);
                const color = colors[vIdx % colors.length];
                return (
                  <motion.div
                    key={`${trip.vehicleId}-${trip.tripNumber}`}
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ delay: tIdx * 0.05 }}
                    className="card"
                  >
                    <div className="flex items-center gap-3 mb-4">
                      <div className="w-4 h-4 rounded-full" style={{ backgroundColor: color }} />
                      <h3 className="font-bold text-lg">{trip.vehicleId} — Trip {trip.tripNumber}</h3>
                      <div className="ml-auto flex gap-3 text-sm">
                        <span className="badge badge-info">{trip.employees.length} passengers</span>
                        <span className="badge badge-primary">{formatNumber(Math.round(trip.distance))} km</span>
                        <span className="badge badge-success">₹{formatNumber(Math.round(trip.cost))}</span>
                      </div>
                    </div>

                    {/* Route sequence */}
                    <div className="relative pl-8">
                      {trip.route.map((point, pIdx) => (
                        <div key={pIdx} className="relative pb-4 last:pb-0">
                          {/* Vertical line */}
                          {pIdx < trip.route.length - 1 && (
                            <div className="absolute left-[-20px] top-6 bottom-0 w-0.5 bg-gray/20" />
                          )}
                          {/* Dot */}
                          <div
                            className={`absolute left-[-24px] top-1.5 w-3 h-3 rounded-full border-2 ${
                              point.type === 'pickup' ? 'bg-green-400 border-green-400' : 'bg-red-400 border-red-400'
                            }`}
                          />
                          <div className="flex items-center gap-2">
                            <span className={`text-xs font-medium uppercase ${
                              point.type === 'pickup' ? 'text-green-400' : 'text-red-400'
                            }`}>
                              {point.type}
                            </span>
                            <span className="text-sm text-gray">
                              {point.employeeId ? `Employee ${point.employeeId}` : 'Office'} — ({point.lat.toFixed(4)}, {point.lng.toFixed(4)})
                            </span>
                          </div>
                        </div>
                      ))}
                    </div>
                  </motion.div>
                );
              })
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
