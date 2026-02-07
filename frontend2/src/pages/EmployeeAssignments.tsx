import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { Users, Search, Filter } from 'lucide-react';
import { useState } from 'react';
import { getPriorityColor } from '../utils/helpers';

export default function EmployeeAssignments() {
  const { currentResult } = useApp();
  const [searchTerm, setSearchTerm] = useState('');

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

  const filteredEmployees = currentResult.employees.filter(emp =>
    emp.id.toLowerCase().includes(searchTerm.toLowerCase()) ||
    emp.pickupLocation.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const priorityDist = {
    High: currentResult.employees.filter(e => e.priority === 'High').length,
    Medium: currentResult.employees.filter(e => e.priority === 'Medium').length,
    Low: currentResult.employees.filter(e => e.priority === 'Low').length,
  };

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-7xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Employee Assignment Details</h1>
        <p className="text-gray mb-8">Complete traceability for every employee journey</p>

        {/* Summary Statistics */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="card mb-6">
          <div className="grid grid-cols-6 gap-6 text-center">
            <div>
              <Users className="w-6 h-6 text-primary mx-auto mb-2" />
              <p className="text-sm text-gray mb-1">Total Employees</p>
              <p className="text-2xl font-bold">{currentResult.employees.length}</p>
            </div>
            <div>
              <p className="text-sm text-gray mb-1">Successfully Assigned</p>
              <p className="text-2xl font-bold text-green-400">{currentResult.assignments.length}</p>
            </div>
            <div>
              <p className="text-sm text-gray mb-1">High Priority</p>
              <p className="text-2xl font-bold text-red-400">{priorityDist.High}</p>
            </div>
            <div>
              <p className="text-sm text-gray mb-1">Medium Priority</p>
              <p className="text-2xl font-bold text-yellow-400">{priorityDist.Medium}</p>
            </div>
            <div>
              <p className="text-sm text-gray mb-1">Low Priority</p>
              <p className="text-2xl font-bold text-green-400">{priorityDist.Low}</p>
            </div>
            <div>
              <p className="text-sm text-gray mb-1">Time Compliance</p>
              <p className="text-2xl font-bold text-primary">100%</p>
            </div>
          </div>
        </motion.div>

        {/* Search and Filter */}
        <div className="flex gap-4 mb-6">
          <div className="flex-1 relative">
            <Search className="w-5 h-5 text-gray absolute left-3 top-1/2 -translate-y-1/2" />
            <input
              type="text"
              placeholder="Search by employee ID or location..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              className="input-field pl-10"
            />
          </div>
          <button className="btn-secondary">
            <Filter className="w-5 h-5 mr-2" />
            Filters
          </button>
        </div>

        {/* Employee Table */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="card">
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead className="bg-dark-600">
                <tr>
                  <th className="px-4 py-3 text-left">Employee ID</th>
                  <th className="px-4 py-3 text-left">Priority</th>
                  <th className="px-4 py-3 text-left">Pickup Location</th>
                  <th className="px-4 py-3 text-left">Destination</th>
                  <th className="px-4 py-3 text-right">Baseline Cost</th>
                  <th className="px-4 py-3 text-center">Vehicle Pref</th>
                  <th className="px-4 py-3 text-center">Sharing Pref</th>
                  <th className="px-4 py-3 text-center">Status</th>
                </tr>
              </thead>
              <tbody>
                {filteredEmployees.slice(0, 50).map((emp, index) => (
                  <tr key={emp.id} className={`border-t border-gray/10 ${index % 2 === 0 ? 'bg-dark-700/30' : ''} hover:bg-dark-600/50 transition-colors`}>
                    <td className="px-4 py-3 font-medium">{emp.id}</td>
                    <td className="px-4 py-3">
                      <span className={`badge ${getPriorityColor(emp.priority)}`}>
                        {emp.priority}
                      </span>
                    </td>
                    <td className="px-4 py-3 text-gray">{emp.pickupLocation}</td>
                    <td className="px-4 py-3 text-gray">{emp.destination}</td>
                    <td className="px-4 py-3 text-right font-medium">₹{emp.baselineCost}</td>
                    <td className="px-4 py-3 text-center">
                      <span className="badge badge-info text-xs">{emp.vehiclePreference}</span>
                    </td>
                    <td className="px-4 py-3 text-center">
                      <span className="badge badge-primary text-xs">{emp.sharingPreference}</span>
                    </td>
                    <td className="px-4 py-3 text-center">
                      <span className="badge badge-success text-xs">Assigned</span>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          {filteredEmployees.length > 50 && (
            <div className="mt-4 text-center text-gray text-sm">
              Showing 50 of {filteredEmployees.length} employees
            </div>
          )}
        </motion.div>
      </div>
    </div>
  );
}
