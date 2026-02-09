import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useState } from 'react';
import { useApp } from '../context/AppContext';
import { Truck, Car as CarIcon, Route as RouteIcon, MapPin, Users, Clock, Gauge } from 'lucide-react';
import { getRouteColors, formatNumber } from '../utils/helpers';

export default function VehicleFleet() {
  const { currentResult } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);

  if (!currentResult) {
    return (
      <div className="min-h-screen flex items-center justify-center p-8">
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-12 text-center">
          <Truck className="w-16 h-16 text-gray/30 mx-auto mb-4" />
          <p className="text-gray mb-6">No optimization results available</p>
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

  // Group trips by vehicle for route details
  const vehicleTrips = new Map<string, typeof currentResult.trips>();
  for (const trip of currentResult.trips) {
    if (!vehicleTrips.has(trip.vehicleId)) vehicleTrips.set(trip.vehicleId, []);
    vehicleTrips.get(trip.vehicleId)!.push(trip);
  }

  const vehicleIds = Array.from(vehicleTrips.keys());
  const colors = getRouteColors(vehicleIds.length);
  const selectedTrips = selectedVehicle ? vehicleTrips.get(selectedVehicle) || [] : currentResult.trips;

  // Haversine distance between two lat/lng points in km
  const haversineDistance = (lat1: number, lng1: number, lat2: number, lng2: number): number => {
    const R = 6371;
    const dLat = ((lat2 - lat1) * Math.PI) / 180;
    const dLng = ((lng2 - lng1) * Math.PI) / 180;
    const a =
      Math.sin(dLat / 2) * Math.sin(dLat / 2) +
      Math.cos((lat1 * Math.PI) / 180) * Math.cos((lat2 * Math.PI) / 180) *
      Math.sin(dLng / 2) * Math.sin(dLng / 2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  };

  // Calculate average capacity utilization per vehicle
  const getVehicleCapacityInfo = (vehicleId: string) => {
    const vehicle = currentResult.vehicles.find(v => v.id === vehicleId);
    const trips = vehicleTrips.get(vehicleId) || [];
    const totalCapacity = vehicle?.capacity || 0;
    if (trips.length === 0) return { avgUsed: 0, totalCapacity };
    const avgUsed = trips.reduce((s, t) => s + t.employees.length, 0) / trips.length;
    return { avgUsed, totalCapacity };
  };

  return (
    <div className="min-h-screen p-3">
      <div className="max-w-7xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Vehicle Fleet Analysis</h1>
        <p className="text-gray mb-8">Deep dive into vehicle utilization and performance</p>

        {/* Fleet Summary */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6 mb-8">
          <div className="grid grid-cols-5 gap-6 text-center">
            <div>
              <Truck className="w-8 h-8 text-primary mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Total Vehicles</p>
              <p className="text-3xl font-bold">{currentResult.vehicles.length}</p>
            </div>
            <div>
              <CarIcon className="w-8 h-8 text-primary mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Vehicles Used</p>
              <p className="text-3xl font-bold text-primary">{vehiclesUsed}</p>
            </div>
            <div>
              <CarIcon className="w-8 h-8 text-gray mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Vehicles Unused</p>
              <p className="text-3xl font-bold text-gray">{vehiclesUnused}</p>
            </div>
            <div>
              <RouteIcon className="w-8 h-8 text-primary mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Total Trips</p>
              <p className="text-3xl font-bold text-primary">{currentResult.trips.length}</p>
            </div>
            <div>
              <Gauge className="w-8 h-8 text-primary mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Utilization Rate</p>
              <p className="text-3xl font-bold text-primary">
                {((vehiclesUsed / currentResult.vehicles.length) * 100).toFixed(0)}%
              </p>
            </div>
          </div>
        </motion.div>

        {/* Fleet Composition */}
        <div className="grid grid-cols-3 gap-6 mb-8">
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6">
            <h3 className="font-bold mb-4">By Fuel Type</h3>
            <div className="space-y-3">
              {Object.entries(fuelDist).map(([type, count]) => (
                <div key={type}>
                  <div className="flex justify-between mb-1">
                    <span className="text-sm text-gray">{type}</span>
                    <span className="font-medium">{count}</span>
                  </div>
                  <div className="h-2 bg-dark-600 rounded-full overflow-hidden">
                    <div className="h-full bg-primary rounded-full transition-all duration-500" style={{ width: `${(count / currentResult.vehicles.length) * 100}%` }} />
                  </div>
                </div>
              ))}
            </div>
          </motion.div>

          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6 col-span-2">
            <h3 className="font-bold mb-4">Vehicle List</h3>
            <div className="grid grid-cols-2 gap-3 max-h-64 overflow-y-auto scrollbar-thin">
              {currentResult.vehicles.map((vehicle) => {
                const { avgUsed, totalCapacity } = getVehicleCapacityInfo(vehicle.id);
                const isUsed = vehicleTrips.has(vehicle.id);
                return (
                  <div
                    key={vehicle.id}
                    onClick={() => isUsed && setSelectedVehicle(vehicle.id)}
                    className={`bg-dark-600/80 rounded-lg p-3 border border-gray/10 transition-all ${isUsed ? 'cursor-pointer hover:border-primary/30' : 'opacity-60'}`}
                  >
                    <div className="flex items-center justify-between mb-2">
                      <span className="font-medium text-white">{vehicle.id}</span>
                      <span className="text-xs text-gray">{vehicle.mode}</span>
                    </div>
                    <div className="flex gap-2 mb-2">
                      <span className="px-2 py-0.5 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                        {vehicle.fuelType}
                      </span>
                    </div>
                    <div className="mt-2">
                      <div className="flex items-center justify-between text-xs mb-1">
                        <span className="text-gray">Avg Used | Total</span>
                        <span className="font-medium text-white">{avgUsed.toFixed(1)} | {totalCapacity}</span>
                      </div>
                      <div className="h-2 bg-dark-500 rounded-full overflow-hidden">
                        <div
                          className="h-full bg-primary rounded-full transition-all duration-500"
                          style={{ width: `${totalCapacity > 0 ? (avgUsed / totalCapacity) * 100 : 0}%` }}
                        />
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          </motion.div>
        </div>

        {/* Route Details Section */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.3 }}>
          <h2 className="text-2xl font-bold mb-6 flex items-center gap-3">
            <RouteIcon className="w-6 h-6 text-primary" />
            Detailed Route Information
          </h2>
          
          {/* Vehicle Selector */}
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6 mb-6">
            <div className="flex flex-wrap gap-3">
              <button
                onClick={() => setSelectedVehicle(null)}
                className={`px-4 py-2 rounded-lg border transition-all ${
                  !selectedVehicle ? 'bg-primary/20 border-primary/40 text-white' : 'bg-dark-700 border-gray/30 text-gray hover:bg-dark-600'
                }`}
              >
                All Routes ({currentResult.trips.length} trips)
              </button>
              {vehicleIds.map((vehicleId, idx) => {
                const trips = vehicleTrips.get(vehicleId) || [];
                const totalEmp = new Set(trips.flatMap(t => t.employees)).size;
                return (
                  <button
                    key={vehicleId}
                    onClick={() => setSelectedVehicle(vehicleId)}
                    className={`px-4 py-2 rounded-lg border transition-all flex items-center gap-2 ${
                      selectedVehicle === vehicleId ? 'bg-primary/20 border-primary/40 text-white' : 'bg-dark-700 border-gray/30 text-gray hover:bg-dark-600'
                    }`}
                  >
                    <div className="w-3 h-3 rounded-full" style={{ backgroundColor: colors[idx % colors.length] }} />
                    <span className="font-medium">{vehicleId}</span>
                    <span className="text-xs opacity-75">({trips.length} trips, {totalEmp} emp)</span>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Route Details */}
          <div className="space-y-4">
            {selectedTrips.length === 0 ? (
              <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-12 text-center">
                <MapPin className="w-16 h-16 text-gray/30 mx-auto mb-4" />
                <p className="text-gray">No routes available for this selection</p>
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
                    className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6"
                  >
                    <div className="flex items-center gap-3 mb-4">
                      <div className="w-4 h-4 rounded-full" style={{ backgroundColor: color }} />
                      <h3 className="font-bold text-lg">{trip.vehicleId} - Trip {trip.tripNumber}</h3>
                      <div className="ml-auto flex gap-3 text-sm">
                        <span className="px-2 py-1 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          <Users className="w-3 h-3 inline mr-1" />{trip.employees.length} passengers
                        </span>
                        <span className="px-2 py-1 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          {formatNumber(Math.round(trip.distance))} km
                        </span>
                        <span className="px-2 py-1 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          Rs. {formatNumber(Math.round(trip.cost))}
                        </span>
                      </div>
                    </div>

                    {/* Route sequence - 2 column layout */}
                    <div className="grid grid-cols-1 lg:grid-cols-2 gap-x-8">
                      {/* Left: Stop timeline */}
                      <div className="relative pl-8">
                        <p className="text-xs text-gray font-medium uppercase tracking-wider mb-3">Route Timeline</p>
                        {trip.route.map((point, pIdx) => {
                          const isPickup = point.type === 'pickup';
                          const isDropoff = point.type === 'dropoff';
                          const dotColor = isPickup ? 'bg-green-400 border-green-400' : isDropoff ? 'bg-red-400 border-red-400' : 'bg-primary border-primary';
                          const textColor = isPickup ? 'text-green-400' : isDropoff ? 'text-red-400' : 'text-primary';

                          // Use real distance_from_prev from next stop, fallback to haversine
                          let distToNext: number | null = null;
                          if (pIdx < trip.route.length - 1) {
                            const nextPoint = trip.route[pIdx + 1];
                            distToNext = nextPoint.distanceFromPrev > 0
                              ? nextPoint.distanceFromPrev
                              : haversineDistance(point.lat, point.lng, nextPoint.lat, nextPoint.lng);
                          }

                          return (
                            <div key={pIdx} className="relative">
                              {/* Stop row */}
                              <div className="relative pb-2">
                                {/* Dot */}
                                <div className={`absolute left-[-24px] top-1.5 w-3 h-3 rounded-full border-2 ${dotColor}`} />
                                <div className="flex items-center justify-between gap-2">
                                  <div className="flex items-center gap-2 min-w-0">
                                    <span className={`text-xs font-medium uppercase shrink-0 ${textColor}`}>
                                      {point.type === 'dropoff' ? 'dropoff' : point.type}
                                    </span>
                                    <span className="text-sm font-medium text-white truncate">
                                      {point.employeeId || 'Office'}
                                    </span>
                                  </div>
                                  <div className="flex items-center gap-3 text-xs shrink-0">
                                    {point.arrivalTime && (
                                      <div className="flex items-center gap-1 text-gray">
                                        <Clock className="w-3 h-3 text-green-400" />
                                        <span className="text-white">{point.arrivalTime}</span>
                                      </div>
                                    )}
                                    {point.departureTime && (
                                      <div className="flex items-center gap-1 text-gray">
                                        <Clock className="w-3 h-3 text-red-400" />
                                        <span className="text-white">{point.departureTime}</span>
                                      </div>
                                    )}
                                    {!point.arrivalTime && !point.departureTime && point.time && (
                                      <div className="flex items-center gap-1 text-gray">
                                        <Clock className="w-3 h-3" />
                                        <span>{point.time}</span>
                                      </div>
                                    )}
                                  </div>
                                </div>
                              </div>

                              {/* Connecting line with distance */}
                              {pIdx < trip.route.length - 1 && (
                                <div className="relative ml-[-20px] pl-[20px] pb-2">
                                  <div className="absolute left-[-20px] top-0 bottom-0 w-0.5 bg-gray/20" />
                                  {distToNext !== null && distToNext > 0 && (
                                    <div className="flex items-center gap-1.5 py-1">
                                      <div className="w-4 border-t border-dashed border-gray/30" />
                                      <span className="text-[10px] text-gray/60 bg-dark-800/80 px-1.5 py-0.5 rounded">
                                        {distToNext < 1 ? `${Math.round(distToNext * 1000)} m` : `${distToNext.toFixed(1)} km`}
                                      </span>
                                    </div>
                                  )}
                                </div>
                              )}
                            </div>
                          );
                        })}
                      </div>

                      {/* Right: Stop details table */}
                      <div>
                        <p className="text-xs text-gray font-medium uppercase tracking-wider mb-3">Stop Details</p>
                        <div className="overflow-hidden rounded-lg border border-gray/10">
                          <table className="w-full text-xs">
                            <thead>
                              <tr className="bg-dark-600/80">
                                <th className="px-3 py-2 text-left text-gray/70 font-medium">Stop</th>
                                <th className="px-3 py-2 text-left text-gray/70 font-medium">Type</th>
                                <th className="px-3 py-2 text-right text-gray/70 font-medium">Arrival</th>
                                <th className="px-3 py-2 text-right text-gray/70 font-medium">Departure</th>
                                <th className="px-3 py-2 text-right text-gray/70 font-medium">Dist.</th>
                              </tr>
                            </thead>
                            <tbody>
                              {trip.route.map((point, pIdx) => {
                                const isPickup = point.type === 'pickup';
                                const isDropoff = point.type === 'dropoff';
                                const textColor = isPickup ? 'text-green-400' : isDropoff ? 'text-red-400' : 'text-primary';
                                return (
                                  <tr key={pIdx} className="border-t border-gray/10 hover:bg-dark-600/30">
                                    <td className="px-3 py-2 font-medium text-white">
                                      {point.employeeId || 'Office'}
                                    </td>
                                    <td className={`px-3 py-2 uppercase font-medium ${textColor}`}>
                                      {point.type === 'dropoff' ? 'dropoff' : point.type}
                                    </td>
                                    <td className="px-3 py-2 text-right text-white">
                                      {point.arrivalTime || '-'}
                                    </td>
                                    <td className="px-3 py-2 text-right text-white">
                                      {point.departureTime || '-'}
                                    </td>
                                    <td className="px-3 py-2 text-right text-gray">
                                      {point.distanceFromPrev > 0
                                        ? point.distanceFromPrev < 1
                                          ? `${Math.round(point.distanceFromPrev * 1000)} m`
                                          : `${point.distanceFromPrev.toFixed(1)} km`
                                        : '-'}
                                    </td>
                                  </tr>
                                );
                              })}
                            </tbody>
                          </table>
                        </div>
                      </div>
                    </div>
                  </motion.div>
                );
              })
            )}
          </div>
        </motion.div>
      </div>
    </div>
  );
}
