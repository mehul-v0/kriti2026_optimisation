import { useState } from 'react';
import { MapPin, Navigation, Home } from 'lucide-react';
import type { Route, EmployeeRequest } from '../types';

interface MapVisualizationProps {
  routes: Route[];
  employees: EmployeeRequest[];
  showOptimized: boolean;
}

export function MapVisualization({ routes, employees, showOptimized }: MapVisualizationProps) {
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);

  const colors = [
    '#ef4444',
    '#3b82f6',
    '#10b981',
    '#f59e0b',
    '#8b5cf6',
    '#ec4899',
    '#14b8a6',
    '#f97316',
  ];

  const allLats = [
    ...employees.map((e) => e.pickup_lat),
    ...employees.map((e) => e.drop_lat),
    ...routes.flatMap((r) => r.route_points.map((p) => p.lat)),
  ];
  const allLngs = [
    ...employees.map((e) => e.pickup_lng),
    ...employees.map((e) => e.drop_lng),
    ...routes.flatMap((r) => r.route_points.map((p) => p.lng)),
  ];

  const minLat = Math.min(...allLats);
  const maxLat = Math.max(...allLats);
  const minLng = Math.min(...allLngs);
  const maxLng = Math.max(...allLngs);

  const padding = 40;
  const width = 800;
  const height = 600;

  const scaleX = (lng: number) => {
    return padding + ((lng - minLng) / (maxLng - minLng)) * (width - 2 * padding);
  };

  const scaleY = (lat: number) => {
    return height - (padding + ((lat - minLat) / (maxLat - minLat)) * (height - 2 * padding));
  };

  const filteredRoutes = selectedVehicle
    ? routes.filter((r) => r.vehicle_id === selectedVehicle)
    : routes;

  return (
    <div className="bg-white rounded-xl shadow-lg p-6">
      <div className="flex items-center justify-between mb-4">
        <h2 className="text-xl font-bold text-slate-800">Route Visualization</h2>
        <div className="flex items-center space-x-4">
          <div className="flex items-center space-x-2">
            <MapPin className="w-4 h-4 text-blue-600" />
            <span className="text-sm text-slate-600">Pickup</span>
          </div>
          <div className="flex items-center space-x-2">
            <Home className="w-4 h-4 text-emerald-600" />
            <span className="text-sm text-slate-600">Dropoff</span>
          </div>
        </div>
      </div>

      <div className="mb-4 flex flex-wrap gap-2">
        <button
          onClick={() => setSelectedVehicle(null)}
          className={`px-3 py-1 rounded-lg text-sm font-medium transition-colors ${
            selectedVehicle === null
              ? 'bg-slate-800 text-white'
              : 'bg-slate-100 text-slate-700 hover:bg-slate-200'
          }`}
        >
          All Vehicles
        </button>
        {routes.map((route, index) => (
          <button
            key={route.vehicle_id}
            onClick={() => setSelectedVehicle(route.vehicle_id)}
            className={`px-3 py-1 rounded-lg text-sm font-medium transition-colors ${
              selectedVehicle === route.vehicle_id
                ? 'text-white'
                : 'bg-slate-100 text-slate-700 hover:bg-slate-200'
            }`}
            style={{
              backgroundColor:
                selectedVehicle === route.vehicle_id ? colors[index % colors.length] : undefined,
            }}
          >
            {route.vehicle_id}
          </button>
        ))}
      </div>

      <div className="bg-slate-50 rounded-lg border-2 border-slate-200 overflow-hidden">
        <svg width="100%" height="600" viewBox={`0 0 ${width} ${height}`}>
          <defs>
            <pattern id="grid" width="40" height="40" patternUnits="userSpaceOnUse">
              <path d="M 40 0 L 0 0 0 40" fill="none" stroke="#e2e8f0" strokeWidth="1" />
            </pattern>
          </defs>

          <rect width={width} height={height} fill="url(#grid)" />

          {!showOptimized &&
            employees.map((emp, index) => (
              <g key={`emp-${index}`}>
                <circle
                  cx={scaleX(emp.pickup_lng)}
                  cy={scaleY(emp.pickup_lat)}
                  r="5"
                  fill="#3b82f6"
                  opacity="0.7"
                />
                <circle
                  cx={scaleX(emp.drop_lng)}
                  cy={scaleY(emp.drop_lat)}
                  r="5"
                  fill="#10b981"
                  opacity="0.7"
                />
              </g>
            ))}

          {showOptimized &&
            filteredRoutes.map((route, routeIndex) => {
              const color = colors[routes.indexOf(route) % colors.length];
              return (
                <g key={`route-${routeIndex}`}>
                  <polyline
                    points={route.route_points
                      .map((p) => `${scaleX(p.lng)},${scaleY(p.lat)}`)
                      .join(' ')}
                    fill="none"
                    stroke={color}
                    strokeWidth="3"
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    opacity="0.8"
                  />

                  {route.route_points.map((point, pointIndex) => (
                    <g key={`point-${routeIndex}-${pointIndex}`}>
                      {point.type === 'start' && (
                        <circle cx={scaleX(point.lng)} cy={scaleY(point.lat)} r="8" fill={color} />
                      )}
                      {point.type === 'pickup' && (
                        <circle
                          cx={scaleX(point.lng)}
                          cy={scaleY(point.lat)}
                          r="6"
                          fill="#3b82f6"
                          stroke="white"
                          strokeWidth="2"
                        />
                      )}
                      {point.type === 'dropoff' && (
                        <circle
                          cx={scaleX(point.lng)}
                          cy={scaleY(point.lat)}
                          r="6"
                          fill="#10b981"
                          stroke="white"
                          strokeWidth="2"
                        />
                      )}
                    </g>
                  ))}
                </g>
              );
            })}
        </svg>
      </div>

      {showOptimized && (
        <div className="mt-4 grid grid-cols-2 md:grid-cols-4 gap-3">
          {filteredRoutes.map((route, index) => (
            <div
              key={route.vehicle_id}
              className="bg-slate-50 rounded-lg p-3 border border-slate-200"
              style={{ borderLeftWidth: '4px', borderLeftColor: colors[routes.indexOf(route) % colors.length] }}
            >
              <p className="font-medium text-slate-800 text-sm mb-1">{route.vehicle_id}</p>
              <div className="space-y-1">
                <p className="text-xs text-slate-600">
                  <span className="font-medium">{route.passengers_count}</span> passengers
                </p>
                <p className="text-xs text-slate-600">
                  <span className="font-medium">{route.total_distance.toFixed(1)}</span> km
                </p>
                <p className="text-xs text-slate-600">
                  <span className="font-medium">₹{route.total_cost.toFixed(0)}</span>
                </p>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
