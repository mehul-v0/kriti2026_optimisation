import { useState } from 'react';
import { Car, Users, Navigation, DollarSign, Gauge, ChevronDown, ChevronUp } from 'lucide-react';
import type { Route, Vehicle, VehicleAssignment } from '../types';

interface VehiclePanelProps {
  routes: Route[];
  vehicles: Vehicle[];
  assignments: VehicleAssignment[];
}

export function VehiclePanel({ routes, vehicles, assignments }: VehiclePanelProps) {
  const [expandedVehicle, setExpandedVehicle] = useState<string | null>(null);

  const getVehicleInfo = (vehicleId: string) => {
    return vehicles.find((v) => v.vehicle_id === vehicleId);
  };

  const getVehicleAssignments = (vehicleId: string) => {
    return assignments
      .filter((a) => a.vehicle_id === vehicleId)
      .sort((a, b) => a.sequence_order - b.sequence_order);
  };

  const getCategoryColor = (category: string) => {
    switch (category?.toLowerCase()) {
      case 'electric':
        return 'text-emerald-600 bg-emerald-50';
      case 'premium':
        return 'text-amber-600 bg-amber-50';
      case 'normal':
        return 'text-blue-600 bg-blue-50';
      default:
        return 'text-slate-600 bg-slate-50';
    }
  };

  return (
    <div className="bg-white rounded-xl shadow-lg p-6">
      <h2 className="text-xl font-bold text-slate-800 border-b pb-3 mb-6">Vehicle Fleet View</h2>

      <div className="space-y-3">
        {routes.map((route) => {
          const vehicle = getVehicleInfo(route.vehicle_id);
          const vehicleAssignments = getVehicleAssignments(route.vehicle_id);
          const isExpanded = expandedVehicle === route.vehicle_id;

          if (!vehicle) return null;

          return (
            <div
              key={route.vehicle_id}
              className="border border-slate-200 rounded-lg overflow-hidden hover:shadow-md transition-shadow"
            >
              <div
                className="bg-slate-50 p-4 cursor-pointer"
                onClick={() => setExpandedVehicle(isExpanded ? null : route.vehicle_id)}
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center space-x-4">
                    <div className="bg-slate-200 p-3 rounded-lg">
                      <Car className="w-6 h-6 text-slate-700" />
                    </div>
                    <div>
                      <h3 className="font-bold text-slate-800">{route.vehicle_id}</h3>
                      <div className="flex items-center space-x-2 mt-1">
                        <span className={`text-xs px-2 py-1 rounded-full font-medium ${getCategoryColor(vehicle.category)}`}>
                          {vehicle.category.toUpperCase()}
                        </span>
                        <span className="text-xs text-slate-600">Capacity: {vehicle.capacity}</span>
                      </div>
                    </div>
                  </div>

                  <div className="flex items-center space-x-6">
                    <div className="text-center">
                      <div className="flex items-center space-x-1">
                        <Users className="w-4 h-4 text-slate-600" />
                        <span className="text-sm font-bold text-slate-800">
                          {route.passengers_count}/{vehicle.capacity}
                        </span>
                      </div>
                      <p className="text-xs text-slate-500">Capacity</p>
                    </div>

                    <div className="text-center">
                      <div className="flex items-center space-x-1">
                        <Navigation className="w-4 h-4 text-slate-600" />
                        <span className="text-sm font-bold text-slate-800">{route.total_distance.toFixed(1)} km</span>
                      </div>
                      <p className="text-xs text-slate-500">Distance</p>
                    </div>

                    <div className="text-center">
                      <div className="flex items-center space-x-1">
                        <DollarSign className="w-4 h-4 text-slate-600" />
                        <span className="text-sm font-bold text-slate-800">₹{route.total_cost.toFixed(0)}</span>
                      </div>
                      <p className="text-xs text-slate-500">Cost</p>
                    </div>

                    <div className="text-center">
                      <div className="flex items-center space-x-1">
                        <Gauge className="w-4 h-4 text-slate-600" />
                        <span className="text-sm font-bold text-slate-800">{route.capacity_utilization.toFixed(0)}%</span>
                      </div>
                      <p className="text-xs text-slate-500">Utilization</p>
                    </div>

                    {isExpanded ? (
                      <ChevronUp className="w-5 h-5 text-slate-600" />
                    ) : (
                      <ChevronDown className="w-5 h-5 text-slate-600" />
                    )}
                  </div>
                </div>
              </div>

              {isExpanded && (
                <div className="p-4 bg-white border-t border-slate-200">
                  <h4 className="font-medium text-slate-700 mb-3">Route Sequence</h4>
                  <div className="space-y-2">
                    {vehicleAssignments.map((assignment, index) => (
                      <div
                        key={`${assignment.vehicle_id}-${assignment.employee_id}-${index}`}
                        className="flex items-center space-x-3 p-3 bg-slate-50 rounded-lg"
                      >
                        <div className="flex-shrink-0 w-8 h-8 bg-slate-200 rounded-full flex items-center justify-center">
                          <span className="text-xs font-bold text-slate-700">{index + 1}</span>
                        </div>
                        <div className="flex-1">
                          <p className="font-medium text-slate-800">{assignment.employee_id}</p>
                          <p className="text-xs text-slate-600">
                            Pickup: {assignment.pickup_time} → Dropoff: {assignment.dropoff_time}
                          </p>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
