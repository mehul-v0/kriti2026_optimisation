import { useState } from 'react';
import { User, Clock, Car, MapPin, Home } from 'lucide-react';
import type { EmployeeRequest, VehicleAssignment } from '../types';

interface EmployeeViewProps {
  employees: EmployeeRequest[];
  assignments: VehicleAssignment[];
}

export function EmployeeView({ employees, assignments }: EmployeeViewProps) {
  const [selectedEmployee, setSelectedEmployee] = useState<string | null>(null);

  const getEmployeeAssignment = (employeeId: string) => {
    return assignments.find((a) => a.employee_id === employeeId && a.is_pickup);
  };

  const getPriorityColor = (priority: number) => {
    switch (priority) {
      case 1:
        return 'bg-red-100 text-red-700 border-red-200';
      case 2:
        return 'bg-amber-100 text-amber-700 border-amber-200';
      case 3:
        return 'bg-blue-100 text-blue-700 border-blue-200';
      default:
        return 'bg-slate-100 text-slate-700 border-slate-200';
    }
  };

  const getPriorityLabel = (priority: number) => {
    switch (priority) {
      case 1:
        return 'HIGH';
      case 2:
        return 'MEDIUM';
      case 3:
        return 'LOW';
      default:
        return 'NORMAL';
    }
  };

  const selectedEmp = employees.find((e) => e.employee_id === selectedEmployee);
  const selectedAssignment = selectedEmployee ? getEmployeeAssignment(selectedEmployee) : null;

  return (
    <div className="bg-white rounded-xl shadow-lg p-6">
      <h2 className="text-xl font-bold text-slate-800 border-b pb-3 mb-6">Employee View</h2>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div>
          <h3 className="font-medium text-slate-700 mb-3">All Employees</h3>
          <div className="space-y-2 max-h-96 overflow-y-auto">
            {employees.map((employee) => {
              const assignment = getEmployeeAssignment(employee.employee_id);
              return (
                <div
                  key={employee.employee_id}
                  className={`p-3 rounded-lg border cursor-pointer transition-all ${
                    selectedEmployee === employee.employee_id
                      ? 'bg-slate-100 border-slate-400 shadow-sm'
                      : 'bg-white border-slate-200 hover:border-slate-300'
                  }`}
                  onClick={() => setSelectedEmployee(employee.employee_id)}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex items-center space-x-3">
                      <div className="bg-slate-200 p-2 rounded-full">
                        <User className="w-4 h-4 text-slate-700" />
                      </div>
                      <div>
                        <p className="font-medium text-slate-800">{employee.employee_id}</p>
                        <p className="text-xs text-slate-600">{assignment?.vehicle_id || 'Not assigned'}</p>
                      </div>
                    </div>
                    <span
                      className={`text-xs px-2 py-1 rounded-full font-medium border ${getPriorityColor(employee.priority)}`}
                    >
                      {getPriorityLabel(employee.priority)}
                    </span>
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        <div>
          {selectedEmp && selectedAssignment ? (
            <div className="space-y-4">
              <div className="bg-gradient-to-br from-slate-50 to-slate-100 rounded-lg p-4 border border-slate-200">
                <div className="flex items-center justify-between mb-4">
                  <h3 className="font-bold text-slate-800 text-lg">{selectedEmp.employee_id}</h3>
                  <span
                    className={`text-xs px-3 py-1 rounded-full font-medium border ${getPriorityColor(selectedEmp.priority)}`}
                  >
                    {getPriorityLabel(selectedEmp.priority)}
                  </span>
                </div>

                <div className="space-y-3">
                  <div className="flex items-start space-x-3">
                    <MapPin className="w-5 h-5 text-blue-600 mt-1 flex-shrink-0" />
                    <div>
                      <p className="text-xs font-medium text-slate-600">Pickup Location</p>
                      <p className="text-xs text-slate-500 mt-1">
                        {selectedEmp.pickup_lat.toFixed(4)}, {selectedEmp.pickup_lng.toFixed(4)}
                      </p>
                    </div>
                  </div>

                  <div className="flex items-start space-x-3">
                    <Home className="w-5 h-5 text-emerald-600 mt-1 flex-shrink-0" />
                    <div>
                      <p className="text-xs font-medium text-slate-600">Dropoff Location</p>
                      <p className="text-xs text-slate-500 mt-1">
                        {selectedEmp.drop_lat.toFixed(4)}, {selectedEmp.drop_lng.toFixed(4)}
                      </p>
                    </div>
                  </div>

                  <div className="flex items-start space-x-3">
                    <Clock className="w-5 h-5 text-amber-600 mt-1 flex-shrink-0" />
                    <div>
                      <p className="text-xs font-medium text-slate-600">Time Window</p>
                      <p className="text-sm text-slate-800">
                        {selectedEmp.earliest_pickup} - {selectedEmp.latest_drop}
                      </p>
                    </div>
                  </div>

                  <div className="flex items-start space-x-3">
                    <Car className="w-5 h-5 text-purple-600 mt-1 flex-shrink-0" />
                    <div>
                      <p className="text-xs font-medium text-slate-600">Assigned Vehicle</p>
                      <p className="text-sm text-slate-800">{selectedAssignment.vehicle_id}</p>
                    </div>
                  </div>
                </div>
              </div>

              <div className="bg-white rounded-lg p-4 border border-slate-200">
                <h4 className="font-medium text-slate-700 mb-3">Journey Details</h4>
                <div className="space-y-2">
                  <div className="flex items-center justify-between p-2 bg-blue-50 rounded">
                    <span className="text-sm text-blue-700">Pickup Time</span>
                    <span className="text-sm font-medium text-blue-900">{selectedAssignment.pickup_time}</span>
                  </div>
                  <div className="flex items-center justify-between p-2 bg-emerald-50 rounded">
                    <span className="text-sm text-emerald-700">Dropoff Time</span>
                    <span className="text-sm font-medium text-emerald-900">{selectedAssignment.dropoff_time}</span>
                  </div>
                </div>
              </div>

              <div className="bg-gradient-to-r from-emerald-50 to-blue-50 rounded-lg p-4 border border-emerald-200">
                <div className="grid grid-cols-2 gap-3">
                  <div>
                    <p className="text-xs text-slate-600">Vehicle Preference</p>
                    <p className="text-sm font-medium text-slate-800">{selectedEmp.vehicle_preference}</p>
                  </div>
                  <div>
                    <p className="text-xs text-slate-600">Sharing Preference</p>
                    <p className="text-sm font-medium text-slate-800">{selectedEmp.sharing_preference}</p>
                  </div>
                </div>
              </div>
            </div>
          ) : (
            <div className="flex items-center justify-center h-full">
              <div className="text-center text-slate-400">
                <User className="w-12 h-12 mx-auto mb-2 opacity-50" />
                <p>Select an employee to view details</p>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
