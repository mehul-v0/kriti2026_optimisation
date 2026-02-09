import { useState } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../context/AppContext';
import { motion } from 'framer-motion';
import { Map as MapIcon, MapPin, Navigation, Layers, Search, Filter, Download } from 'lucide-react';
import { getRouteColors, formatNumber } from '../utils/helpers';

export default function RouteExplorer() {
  const { currentResult } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);
  const [mapStyle, setMapStyle] = useState('dark');
  const [showTraffic, setShowTraffic] = useState(false);
  const [showCompletedRoutes, setShowCompletedRoutes] = useState(true);

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center p-8 network-bg">
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-12 text-center">
          <MapIcon className="w-16 h-16 text-gray/30 mx-auto mb-4" />
          <p className="text-gray mb-6">No optimization results available</p>
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
      {/* Header */}
      <div className="p-6 border-b border-gray/10">
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-3xl font-bold flex items-center gap-3">
              <MapIcon className="w-8 h-8 text-primary" />
              Advanced Route Map
            </h1>
            <p className="text-gray mt-1">
              Interactive visualization of {currentResult.trips.length} trips across {vehicleIds.length} vehicles
            </p>
          </div>
          <div className="flex gap-3">
            <button className="btn-secondary flex items-center gap-2">
              <Download className="w-4 h-4" />
              Export Routes
            </button>
          </div>
        </div>
      </div>

      {/* Map Controls */}
      <div className="px-6 py-4 border-b border-gray/10 bg-dark-900/50">
        <div className="flex items-center gap-4">
          {/* Vehicle Filter */}
          <div className="flex items-center gap-2">
            <Filter className="w-4 h-4 text-gray" />
            <select 
              value={selectedVehicle || ''}
              onChange={(e) => setSelectedVehicle(e.target.value || null)}
              className="bg-dark-700 border border-gray/30 rounded-lg px-3 py-1 text-sm"
            >
              <option value="">All Vehicles</option>
              {vehicleIds.map((id) => (
                <option key={id} value={id}>{id}</option>
              ))}
            </select>
          </div>

          {/* Map Style Toggle */}
          <div className="flex items-center gap-2">
            <Layers className="w-4 h-4 text-gray" />
            <button 
              onClick={() => setMapStyle(mapStyle === 'dark' ? 'satellite' : 'dark')}
              className="bg-dark-700 border border-gray/30 rounded-lg px-3 py-1 text-sm hover:bg-dark-600"
            >
              {mapStyle === 'dark' ? 'Dark' : 'Satellite'}
            </button>
          </div>

          {/* Toggle Options */}
          <label className="flex items-center gap-2 text-sm">
            <input 
              type="checkbox" 
              checked={showTraffic}
              onChange={(e) => setShowTraffic(e.target.checked)}
              className="rounded"
            />
            Traffic Layer
          </label>

          <label className="flex items-center gap-2 text-sm">
            <input 
              type="checkbox" 
              checked={showCompletedRoutes}
              onChange={(e) => setShowCompletedRoutes(e.target.checked)}
              className="rounded"
            />
            Show Completed
          </label>

          {/* Search */}
          <div className="ml-auto flex items-center gap-2">
            <Search className="w-4 h-4 text-gray" />
            <input 
              type="text" 
              placeholder="Search locations..."
              className="bg-dark-700 border border-gray/30 rounded-lg px-3 py-1 text-sm w-48 focus:border-primary focus:outline-none"
            />
          </div>
        </div>
      </div>

      {/* Map Container */}
      <div className="flex-1 relative bg-dark-900">
        {/* Map Placeholder with dark theme */}
        <div className="absolute inset-0 bg-gradient-to-br from-slate-800 via-gray-900 to-black overflow-hidden">
          {/* Simulated map grid */}
          <div className="absolute inset-0" 
               style={{
                 backgroundImage: `
                   linear-gradient(rgba(255,255,255,0.08) 1px, transparent 1px),
                   linear-gradient(90deg, rgba(255,255,255,0.08) 1px, transparent 1px)
                 `,
                 backgroundSize: '30px 30px'
               }}>
          </div>
          
          {/* Simulated street lines */}
          <svg className="absolute inset-0 w-full h-full opacity-40">
            <defs>
              <pattern id="streets" patternUnits="userSpaceOnUse" width="100" height="100">
                <path d="M 0 50 L 100 50" stroke="#4B5563" strokeWidth="1.5" />
                <path d="M 50 0 L 50 100" stroke="#4B5563" strokeWidth="1.5" />
                <path d="M 15 25 L 85 25" stroke="#374151" strokeWidth="1" />
                <path d="M 15 75 L 85 75" stroke="#374151" strokeWidth="1" />
                <path d="M 25 15 L 25 85" stroke="#374151" strokeWidth="1" />
                <path d="M 75 15 L 75 85" stroke="#374151" strokeWidth="1" />
                <circle cx="25" cy="25" r="1.5" fill="#6B7280" />
                <circle cx="75" cy="25" r="1.5" fill="#6B7280" />
                <circle cx="25" cy="75" r="1.5" fill="#6B7280" />
                <circle cx="75" cy="75" r="1.5" fill="#6B7280" />
              </pattern>
            </defs>
            <rect width="100%" height="100%" fill="url(#streets)" />
          </svg>

          {/* Route visualization */}
          <div className="absolute inset-0 p-8">
            {/* Background landmarks */}
            <div className="absolute inset-0">
              <div className="absolute top-[20%] left-[15%] w-3 h-3 bg-blue-400 rounded-full opacity-60" title="Office HQ" />
              <div className="absolute top-[45%] left-[80%] w-2 h-2 bg-yellow-400 rounded-full opacity-60" title="Branch A" />
              <div className="absolute top-[70%] left-[25%] w-2 h-2 bg-purple-400 rounded-full opacity-60" title="Branch B" />
              <div className="absolute top-[30%] left-[60%] w-2 h-2 bg-orange-400 rounded-full opacity-60" title="Depot" />
            </div>

            {/* Route paths first (behind markers) */}
            <svg className="absolute inset-0 w-full h-full pointer-events-none">
              {selectedTrips.map((trip, idx) => {
                const vIdx = vehicleIds.indexOf(trip.vehicleId);
                const color = colors[vIdx % colors.length];
                const isSelected = selectedVehicle === trip.vehicleId || !selectedVehicle;
                
                if (!isSelected) return null;
                
                // Generate route path through multiple points
                const baseX = 100 + (idx % 6) * 150;
                const baseY = 80 + Math.floor(idx / 6) * 100;
                const points = trip.route.map((point, pIdx) => ({
                  x: baseX + (pIdx * 80) + (Math.sin(pIdx) * 30),
                  y: baseY + (pIdx * 40) + (Math.cos(pIdx) * 20)
                }));
                
                return (
                  <g key={`route-${trip.vehicleId}-${trip.tripNumber}`}>
                    {/* Main route line */}
                    <path
                      d={`M ${points.map(p => `${p.x},${p.y}`).join(' L ')}`}
                      stroke={color}
                      strokeWidth="3"
                      fill="none"
                      opacity="0.8"
                      strokeLinecap="round"
                      strokeDasharray="0"
                    />
                    {/* Route animation overlay */}
                    <path
                      d={`M ${points.map(p => `${p.x},${p.y}`).join(' L ')}`}
                      stroke={color}
                      strokeWidth="2"
                      fill="none"
                      opacity="0.6"
                      strokeLinecap="round"
                      strokeDasharray="8,8"
                      className="animate-pulse"
                    />
                    {/* Pickup/dropoff indicators */}
                    {points.map((point, pIdx) => (
                      <g key={pIdx}>
                        <circle
                          cx={point.x}
                          cy={point.y}
                          r="4"
                          fill={trip.route[pIdx]?.type === 'pickup' ? '#10B981' : '#EF4444'}
                          stroke="white"
                          strokeWidth="1"
                          opacity="0.9"
                        />
                        {/* Pickup/dropoff labels */}
                        <text
                          x={point.x}
                          y={point.y - 8}
                          fill="white"
                          fontSize="8"
                          textAnchor="middle"
                          className="font-medium"
                        >
                          {trip.route[pIdx]?.type === 'pickup' ? 'P' : 'D'}
                        </text>
                      </g>
                    ))}
                  </g>
                );
              })}
            </svg>

            {selectedTrips.map((trip, idx) => {
              const vIdx = vehicleIds.indexOf(trip.vehicleId);
              const color = colors[vIdx % colors.length];
              const isSelected = selectedVehicle === trip.vehicleId || !selectedVehicle;
              
              return (
                <motion.div
                  key={`${trip.vehicleId}-${trip.tripNumber}`}
                  initial={{ opacity: 0, scale: 0 }}
                  animate={{ opacity: isSelected ? 1 : 0.3, scale: 1 }}
                  className="absolute"
                  style={{
                    left: `${100 + (idx % 6) * 150}px`,
                    top: `${80 + Math.floor(idx / 6) * 100}px`,
                  }}
                >
                  {/* Vehicle marker */}
                  <div className="relative group">
                    <div 
                      className="w-8 h-8 rounded-full border-3 border-white shadow-xl flex items-center justify-center cursor-pointer hover:scale-125 transition-all duration-200 z-10 relative"
                      style={{ backgroundColor: isSelected ? color : '#6b7280' }}
                    >
                      <div className="w-3 h-3 bg-white rounded-full" />
                    </div>
                    
                    {/* Vehicle ID badge */}
                    <div 
                      className="absolute -top-2 -right-2 bg-white text-black text-xs font-bold px-1 rounded-full shadow-lg z-20"
                      style={{ fontSize: '10px' }}
                    >
                      {trip.vehicleId.slice(-2)}
                    </div>
                    
                    {/* Tooltip */}
                    <div className="absolute bottom-full left-1/2 transform -translate-x-1/2 mb-3 px-3 py-2 bg-dark-800/95 backdrop-blur-sm border border-gray/20 rounded-lg opacity-0 group-hover:opacity-100 transition-all duration-200 pointer-events-none whitespace-nowrap z-30 shadow-xl">
                      <div className="text-sm font-medium">{trip.vehicleId}</div>
                      <div className="text-xs text-gray">
                        Trip {trip.tripNumber} • {trip.employees.length} passengers
                      </div>
                      <div className="text-xs text-primary">
                        {formatNumber(Math.round(trip.distance))} km • ₹{formatNumber(Math.round(trip.cost))}
                      </div>
                    </div>

                    {/* Pulse animation for active vehicles */}
                    {isSelected && (
                      <div 
                        className="absolute inset-0 rounded-full animate-ping"
                        style={{ backgroundColor: color, opacity: 0.2 }}
                      />
                    )}
                  </div>
                </motion.div>
              );
            })}
          </div>

          {/* Live tracking indicator */}
          <motion.div 
            className="absolute top-4 right-4"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
          >
            <div className="bg-dark-800/90 backdrop-blur-sm border border-gray/20 rounded-lg p-3">
              <div className="flex items-center gap-2 mb-2">
                <div className="w-2 h-2 bg-green-400 rounded-full animate-pulse" />
                <span className="text-sm font-medium">Live Tracking</span>
              </div>
              <div className="text-xs text-gray mb-1">Active Routes: {selectedTrips.length}</div>
              <div className="text-xs text-gray">Total Distance: {formatNumber(Math.round(selectedTrips.reduce((sum, trip) => sum + trip.distance, 0)))} km</div>
            </div>
          </motion.div>

          {/* Legend */}
          <div className="absolute bottom-4 left-4">
            <div className="bg-dark-800/90 backdrop-blur-sm border border-gray/20 rounded-lg p-3">
              <div className="text-sm font-medium mb-2">Vehicle Legend</div>
              <div className="space-y-1">
                {vehicleIds.slice(0, 6).map((vehicleId, idx) => {
                  const trips = vehicleTrips.get(vehicleId) || [];
                  const isSelected = selectedVehicle === vehicleId || !selectedVehicle;
                  return (
                    <button
                      key={vehicleId}
                      onClick={() => setSelectedVehicle(selectedVehicle === vehicleId ? null : vehicleId)}
                      className={`flex items-center gap-2 text-xs hover:bg-dark-700 rounded px-2 py-1 transition-colors ${
                        isSelected ? 'text-white' : 'text-gray'
                      }`}
                    >
                      <div 
                        className="w-3 h-3 rounded-full border border-white" 
                        style={{ backgroundColor: colors[idx % colors.length] }}
                      />
                      <span>{vehicleId} ({trips.length})</span>
                    </button>
                  );
                })}
                {vehicleIds.length > 6 && (
                  <div className="text-xs text-gray px-2 py-1">+{vehicleIds.length - 6} more vehicles</div>
                )}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
