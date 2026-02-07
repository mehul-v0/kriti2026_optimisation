import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { Download, FileSpreadsheet, FileJson, Check } from 'lucide-react';
import { downloadJSON } from '../utils/helpers';
import { downloadSolution } from '../services/api';
import * as XLSX from 'xlsx';

export default function ExportReports() {
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

  const handleExport = async (type: string) => {
    switch (type) {
      case 'json':
        downloadJSON(currentResult, `optimization_results_${Date.now()}.json`);
        break;
      case 'solution': {
        try {
          const blob = await downloadSolution();
          const url = URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = 'solution.json';
          document.body.appendChild(a);
          a.click();
          document.body.removeChild(a);
          URL.revokeObjectURL(url);
        } catch {
          alert('No solution file available on the server. Export the JSON results instead.');
        }
        break;
      }
      case 'employees': {
        const ws = XLSX.utils.json_to_sheet(
          currentResult.assignments.map(a => {
            const emp = currentResult.employees.find(e => e.id === a.employeeId);
            return {
              'Employee ID': a.employeeId,
              'Priority': emp?.priority || '',
              'Vehicle ID': a.vehicleId,
              'Trip #': a.tripNumber,
              'Pickup Time': a.pickupTime,
              'Dropoff Time': a.dropoffTime,
              'Vehicle Pref Met': a.vehiclePreferenceMet ? 'Yes' : 'No',
              'Sharing Pref Met': a.sharingPreferenceMet ? 'Yes' : 'No',
              'Time Window Met': a.timeWindowMet ? 'Yes' : 'No',
              'Pickup Location': emp?.pickupLocation || '',
              'Destination': emp?.destination || '',
              'Baseline Cost': emp?.baselineCost || 0,
            };
          })
        );
        const wb = XLSX.utils.book_new();
        XLSX.utils.book_append_sheet(wb, ws, 'Employee Assignments');
        XLSX.writeFile(wb, `employee_assignments_${Date.now()}.xlsx`);
        break;
      }
      case 'vehicles': {
        const ws = XLSX.utils.json_to_sheet(
          currentResult.trips.map(t => ({
            'Vehicle ID': t.vehicleId,
            'Trip #': t.tripNumber,
            'Employees': t.employees.join(', '),
            'Distance (km)': t.distance.toFixed(2),
            'Duration': t.duration,
            'Cost': t.cost.toFixed(2),
            'Start Time': t.startTime,
            'End Time': t.endTime,
          }))
        );
        const wb = XLSX.utils.book_new();
        XLSX.utils.book_append_sheet(wb, ws, 'Vehicle Routes');
        XLSX.writeFile(wb, `vehicle_routes_${Date.now()}.xlsx`);
        break;
      }
      default:
        break;
    }
  };

  const exportCards = [
    {
      title: 'Employee Assignments (Excel)',
      description: 'Full employee assignment table with all columns',
      icon: FileSpreadsheet,
      format: '.xlsx',
      color: 'text-green-400',
      action: () => handleExport('employees'),
    },
    {
      title: 'Vehicle Routes (Excel)',
      description: 'All vehicles with their trip details, sequences, distances, costs',
      icon: FileSpreadsheet,
      format: '.xlsx',
      color: 'text-green-400',
      action: () => handleExport('vehicles'),
    },
    {
      title: 'Solver Solution (JSON)',
      description: 'Raw solver output from the backend — exact C++ solver results',
      icon: FileJson,
      format: '.json',
      color: 'text-blue-400',
      action: () => handleExport('solution'),
    },
    {
      title: 'Complete Results (JSON)',
      description: 'All optimization outputs in structured format for developers',
      icon: FileJson,
      format: '.json',
      color: 'text-yellow-400',
      action: () => handleExport('json'),
    },
  ];

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-6xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Export & Reports</h1>
        <p className="text-gray mb-8">Download your optimization results in various formats</p>

        {/* Quick Exports */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }}>
          <h2 className="text-2xl font-bold mb-6">Quick Exports</h2>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 mb-8">
            {exportCards.map((card, index) => (
              <motion.div
                key={card.title}
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: index * 0.1 }}
                className="card hover:shadow-lg hover:shadow-primary/10 transition-all"
              >
                <card.icon className={`w-12 h-12 ${card.color} mb-4`} />
                <h3 className="font-bold mb-2">{card.title}</h3>
                <p className="text-sm text-gray mb-4">{card.description}</p>
                <div className="flex items-center justify-between">
                  <span className="text-sm text-gray">{card.format}</span>
                  <button onClick={card.action} className="btn-primary text-sm">
                    <Download className="w-4 h-4 mr-2" />
                    Download
                  </button>
                </div>
              </motion.div>
            ))}
          </div>
        </motion.div>

        {/* Custom Report Builder */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.3 }}
          className="card mb-8"
        >
          <h2 className="text-2xl font-bold mb-6">Custom Report Builder</h2>
          <p className="text-gray mb-6">Select sections to include in your custom report</p>
          
          <div className="grid grid-cols-2 gap-4 mb-6">
            {[
              'Executive Summary',
              'Constraint Compliance',
              'Route Visualizations',
              'Vehicle Fleet Analysis',
              'Employee Assignments',
              'Cost Breakdown',
              'Methodology Notes',
            ].map((section) => (
              <label key={section} className="flex items-center gap-3 p-4 bg-dark-600 rounded-lg cursor-pointer hover:bg-dark-500 transition-colors">
                <input type="checkbox" defaultChecked className="w-5 h-5 text-primary" />
                <span>{section}</span>
              </label>
            ))}
          </div>

          <div className="flex items-center gap-4">
            <select className="input-field flex-1">
              <option>PDF Format</option>
              <option>Excel Format</option>
              <option>Both Formats</option>
            </select>
            <button className="btn-primary">
              Generate Custom Report
            </button>
          </div>
        </motion.div>

        {/* Data Persistence Note */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.4 }}
          className="card bg-blue-500/10 border-blue-500/30"
        >
          <div className="flex items-start gap-4">
            <Check className="w-6 h-6 text-blue-400 flex-shrink-0 mt-1" />
            <div>
              <p className="font-medium text-blue-400 mb-2">Data Persistence</p>
              <p className="text-sm text-gray">
                Your optimization results are saved in your browser's local storage and will be available 
                in Session History on your next visit. To save results permanently, please download the 
                exports above.
              </p>
              <button className="text-sm text-red-400 hover:text-red-300 transition-colors mt-3">
                Clear local storage →
              </button>
            </div>
          </div>
        </motion.div>
      </div>
    </div>
  );
}
