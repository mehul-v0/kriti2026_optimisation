import { useState } from 'react';
import { MapContainer, TileLayer, Marker, Popup, Polyline, useMap } from 'react-leaflet';
import { Icon, LatLngBounds } from 'leaflet';
import 'leaflet/dist/leaflet.css';
import { MapPin, Navigation, Home } from 'lucide-react';
import type { Route, EmployeeRequest } from '../types';

interface MapVisualizationProps {
  routes: Route[];
  employees: EmployeeRequest[];
  showOptimized: boolean;
}

// Fix for default marker icons in Leaflet
import markerIcon2x from 'leaflet/dist/images/marker-icon-2x.png';
import markerIcon from 'leaflet/dist/images/marker-icon.png';
import markerShadow from 'leaflet/dist/images/marker-shadow.png';

delete (Icon.Default.prototype as any)._getIconUrl;
Icon.Default.mergeOptions({
  iconRetinaUrl: markerIcon2x,
  iconUrl: markerIcon,
  shadowUrl: markerShadow,
});

// Custom marker icons
const createCustomIcon = (color: string, isPickup: boolean) => {
  const svgIcon = `
    <svg width="25" height="41" viewBox="0 0 25 41" xmlns="http://www.w3.org/2000/svg">
      <path fill="${color}" stroke="white" stroke-width="2" d="M12.5 0C5.6 0 0 5.6 0 12.5c0 1.9 0.4 3.7 1.2 5.3L12.5 41l11.3-23.2c0.8-1.6 1.2-3.4 1.2-5.3C25 5.6 19.4 0 12.5 0z"/>
      <circle cx="12.5" cy="12.5" r="6" fill="white"/>
      <text x="12.5" y="16" text-anchor="middle" font-size="10" fill="${color}" font-weight="bold">
        ${isPickup ? 'P' : 'D'}
      </text>
    </svg>
  `;
  return new Icon({
    iconUrl: `data:image/svg+xml;base64,${btoa(svgIcon)}`,
    iconSize: [25, 41],
    iconAnchor: [12, 41],
    popupAnchor: [0, -41],
  });
};

function MapBounds({ routes, employees }: { routes: Route[]; employees: EmployeeRequest[] }) {
  const map = useMap();
  
  if (routes.length > 0) {
    const allPoints = routes.flatMap(r => r.route_points);
    if (allPoints.length > 0) {
      const bounds = new LatLngBounds(
        allPoints.map(p => [p.lat, p.lng] as [number, number])
      );
      map.fitBounds(bounds, { padding: [50, 50] });
    }
  } else if (employees.length > 0) {
    const allPoints = employees.flatMap(e => [
      [e.pickup_lat, e.pickup_lng] as [number, number],
      [e.destination_lat, e.destination_lng] as [number, number]
    ]);
    const bounds = new LatLngBounds(allPoints);
    map.fitBounds(bounds, { padding: [50, 50] });
  }
  
  return null;
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

  const filteredRoutes = selectedVehicle
    ? routes.filter((r) => r.vehicle_id === selectedVehicle)
    : routes;

  // Default center (Bangalore)
  const defaultCenter: [number, number] = employees.length > 0
    ? [employees[0].pickup_lat, employees[0].pickup_lng]
    : [12.9716, 77.5946];

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

      {showOptimized && routes.length > 0 && (
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
      )}

      <div className="rounded-lg overflow-hidden border-2 border-slate-200" style={{ height: '600px' }}>
        <MapContainer
          center={defaultCenter}
          zoom={12}
          style={{ height: '100%', width: '100%' }}
          scrollWheelZoom={true}
        >
          <TileLayer
            attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
            url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
          />
          
          <MapBounds routes={filteredRoutes} employees={employees} />

          {/* Show unoptimized employee locations */}
          {!showOptimized &&
            employees.map((emp, index) => (
              <div key={`emp-${index}`}>
                <Marker
                  position={[emp.pickup_lat, emp.pickup_lng]}
                  icon={createCustomIcon('#3b82f6', true)}
                >
                  <Popup>
                    <div className="text-sm">
                      <p className="font-bold">{emp.employee_id}</p>
                      <p className="text-slate-600">Pickup Location</p>
                      <p className="text-xs text-slate-500 mt-1">{emp.pickup_address}</p>
                    </div>
                  </Popup>
                </Marker>
                <Marker
                  position={[emp.destination_lat, emp.destination_lng]}
                  icon={createCustomIcon('#10b981', false)}
                >
                  <Popup>
                    <div className="text-sm">
                      <p className="font-bold">{emp.employee_id}</p>
                      <p className="text-slate-600">Dropoff Location</p>
                      <p className="text-xs text-slate-500 mt-1">{emp.destination_address}</p>
                    </div>
                  </Popup>
                </Marker>
              </div>
            ))}

          {/* Show optimized routes */}
          {showOptimized &&
            filteredRoutes.map((route, routeIndex) => {
              const color = colors[routes.indexOf(route) % colors.length];
              const positions = route.route_points.map(p => [p.lat, p.lng] as [number, number]);
              
              return (
                <div key={`route-${routeIndex}`}>
                  {/* Draw route line */}
                  <Polyline
                    positions={positions}
                    color={color}
                    weight={4}
                    opacity={0.7}
                  />

                  {/* Draw markers for each point */}
                  {route.route_points.map((point, pointIndex) => {
                    let markerColor = color;
                    let displayType = point.type;
                    
                    // Use different colors for different point types
                    if (point.type === 'pickup') {
                      markerColor = '#3b82f6';
                      displayType = 'Pickup';
                    } else if (point.type === 'office') {
                      markerColor = '#10b981';
                      displayType = 'Office';
                    } else if (point.type === 'dropoff') {
                      markerColor = '#10b981';
                      displayType = 'Dropoff';
                    }
                    
                    return (
                      <Marker
                        key={`point-${routeIndex}-${pointIndex}`}
                        position={[point.lat, point.lng]}
                        icon={createCustomIcon(markerColor, point.type === 'pickup')}
                      >
                        <Popup>
                          <div className="text-sm">
                            <p className="font-bold" style={{ color }}>{route.vehicle_id}</p>
                            <p className="text-slate-600">{displayType}</p>
                            {point.employee_id && (
                              <p className="text-slate-600">Employee: {point.employee_id}</p>
                            )}
                            {point.trip_number && (
                              <p className="text-xs text-purple-600">Trip #{point.trip_number}</p>
                            )}
                            <p className="text-xs text-slate-500 mt-1">Stop #{pointIndex + 1}</p>
                          </div>
                        </Popup>
                      </Marker>
                    );
                  })}
                </div>
              );
            })}
        </MapContainer>
      </div>

      {showOptimized && filteredRoutes.length > 0 && (
        <div className="mt-4 grid grid-cols-2 md:grid-cols-4 gap-3">
          {filteredRoutes.map((route, index) => {
            const routeColor = colors[routes.indexOf(route) % colors.length];
            return (
              <div
                key={route.vehicle_id}
                className="bg-slate-50 rounded-lg p-3 border border-slate-200 hover:shadow-md transition-shadow cursor-pointer"
                style={{ borderLeftWidth: '4px', borderLeftColor: routeColor }}
              >
                <p className="font-medium text-slate-800 text-sm mb-1" style={{ color: routeColor }}>
                  {route.vehicle_id}
                </p>
                <div className="space-y-1">
                  <p className="text-xs text-slate-600">
                    <span className="font-medium">{route.passengers_count}</span> passengers
                  </p>
                  {route.trips_count && (
                    <p className="text-xs text-slate-600">
                      <span className="font-medium">{route.trips_count}</span> trips
                    </p>
                  )}
                  <p className="text-xs text-slate-600">
                    <span className="font-medium">{route.total_distance.toFixed(1)}</span> km
                  </p>
                  <p className="text-xs text-slate-600">
                    <span className="font-medium">₹{route.total_cost.toFixed(0)}</span>
                  </p>
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
