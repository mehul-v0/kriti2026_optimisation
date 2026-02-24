import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
import { Download, FileSpreadsheet, FileJson, Check } from 'lucide-react';
import { downloadJSON } from '../utils/helpers';
import { downloadSolution } from '../services/api';
import * as XLSX from 'xlsx';
import { useState } from 'react';

export default function ExportReports() {
  const { currentResult } = useApp();
  const [selectedSections, setSelectedSections] = useState<string[]>([
    'Executive Summary',
    'Constraint Compliance',
    'Route Visualizations',
    'Vehicle Fleet Analysis',
    'Employee Assignments',
    'Cost Breakdown',
    'Methodology Notes',
  ]);
  const [reportFormat, setReportFormat] = useState('PDF Format');

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark p-6 lg:p-8">
        <div className="max-w-7xl mx-auto">
          <div className="text-center py-20">
            <h1 className="text-3xl font-bold mb-4">Export & Reports</h1>
            <p className="text-gray mb-6">No optimization results available</p>
            <Link to="/upload" className="inline-flex items-center gap-2 px-6 py-3 bg-primary rounded-xl text-dark font-semibold hover:bg-button-hover transition-all hover:scale-105">
              Start New Optimization
            </Link>
          </div>
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

  const toggleSection = (section: string) => {
    setSelectedSections(prev => 
      prev.includes(section) 
        ? prev.filter(s => s !== section)
        : [...prev, section]
    );
  };

  const handleGenerateCustomReport = () => {
    if (selectedSections.length === 0) {
      alert('Please select at least one section for the report');
      return;
    }

    // Create a comprehensive custom report
    const reportData = {
      metadata: {
        generatedAt: new Date().toISOString(),
        format: reportFormat,
        sections: selectedSections,
      },
      data: {} as any,
    };

    if (selectedSections.includes('Executive Summary')) {
      reportData.data.executiveSummary = {
        totalEmployees: currentResult.employees.length,
        totalAssignments: currentResult.assignments.length,
        totalVehicles: currentResult.vehicles.length,
        totalTrips: currentResult.trips.length,
        totalDistance: currentResult.trips.reduce((sum, t) => sum + t.distance, 0),
        totalCost: currentResult.trips.reduce((sum, t) => sum + t.cost, 0),
      };
    }

    if (selectedSections.includes('Constraint Compliance')) {
      reportData.data.constraintCompliance = {
        vehiclePreferenceMet: currentResult.assignments.filter(a => a.vehiclePreferenceMet).length,
        sharingPreferenceMet: currentResult.assignments.filter(a => a.sharingPreferenceMet).length,
        timeWindowMet: currentResult.assignments.filter(a => a.timeWindowMet).length,
      };
    }

    if (selectedSections.includes('Vehicle Fleet Analysis')) {
      reportData.data.vehicleFleetAnalysis = currentResult.vehicles;
    }

    if (selectedSections.includes('Employee Assignments')) {
      reportData.data.employeeAssignments = currentResult.assignments;
    }

    if (selectedSections.includes('Cost Breakdown')) {
      reportData.data.costBreakdown = currentResult.trips.map(t => ({
        vehicleId: t.vehicleId,
        tripNumber: t.tripNumber,
        cost: t.cost,
        distance: t.distance,
      }));
    }

    // Download as JSON (PDF generation would require additional library)
    downloadJSON(reportData, `custom_report_${Date.now()}.json`);
    alert(`Custom report with ${selectedSections.length} section(s) has been generated!`);
  };

  const exportCards = [
    {
      title: 'Employee Assignments (Excel)',
      description: 'Full employee assignment table with all columns',
      icon: FileSpreadsheet,
      format: '.xlsx',
      color: 'text-primary-bright',
      action: () => handleExport('employees'),
    },
    {
      title: 'Vehicle Routes (Excel)',
      description: 'All vehicles with their trip details, sequences, distances, costs',
      icon: FileSpreadsheet,
      format: '.xlsx',
      color: 'text-primary-bright',
      action: () => handleExport('vehicles'),
    },
    {
      title: 'Solver Solution (JSON)',
      description: 'Raw solver output from the backend — exact C++ solver results',
      icon: FileJson,
      format: '.json',
      color: 'text-primary-bright',
      action: () => handleExport('solution'),
    },
    {
      title: 'Complete Results (JSON)',
      description: 'All optimization outputs in structured format for developers',
      icon: FileJson,
      format: '.json',
      color: 'text-primary-bright',
      action: () => handleExport('json'),
    },
  ];

  return (
    <div className="min-h-screen bg-dark p-6 lg:p-8">
      <div className="max-w-7xl mx-auto space-y-6">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
        >
          <h1 className="text-3xl lg:text-4xl font-bold bg-gradient-to-r from-white to-gray-400 bg-clip-text text-transparent">
            Export & Reports
          </h1>
          <p className="text-gray mt-1">Download your optimization results in various formats</p>
        </motion.div>

        {/* Quick Exports */}
        <motion.div 
          initial={{ opacity: 0, y: 20 }} 
          animate={{ opacity: 1, y: 0 }}
          className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6"
        >
          <h2 className="text-2xl font-bold mb-6">Quick Exports</h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
            {exportCards.map((card, index) => (
              <motion.div
                key={card.title}
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: index * 0.1 }}
                className="bg-dark-700/50 backdrop-blur-xl rounded-xl border border-gray/10 p-5"
              >
                <card.icon className={`w-10 h-10 ${card.color} mb-3`} />
                <h3 className="font-bold mb-2 text-sm">{card.title}</h3>
                <p className="text-xs text-gray mb-4 line-clamp-2">{card.description}</p>
                <div className="flex items-center justify-between">
                  <span className="text-xs text-gray/60">{card.format}</span>
                  <button 
                    onClick={card.action} 
                    className="flex items-center gap-1.5 px-3 py-1.5 bg-primary text-dark rounded-lg text-xs font-semibold hover:bg-button-hover hover:scale-105 transition-all duration-200"
                  >
                    <Download className="w-3 h-3" />
                    Export
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
          className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6"
        >
          <h2 className="text-2xl font-bold mb-4">Custom Report Builder</h2>
          <p className="text-gray mb-6">Select sections to include in your custom report</p>
          
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-3 mb-6">
            {[
              'Executive Summary',
              'Constraint Compliance',
              'Route Visualizations',
              'Vehicle Fleet Analysis',
              'Employee Assignments',
              'Cost Breakdown',
              'Methodology Notes',
            ].map((section) => (
              <label 
                key={section} 
                className="flex items-center gap-3 p-3 bg-dark-700/40 rounded-lg cursor-pointer hover:bg-dark-600/50 hover:border-primary/30 border border-transparent transition-all"
              >
                <input 
                  type="checkbox" 
                  checked={selectedSections.includes(section)}
                  onChange={() => toggleSection(section)}
                  className="w-4 h-4 text-primary rounded focus:ring-primary focus:ring-offset-0 focus:ring-2 bg-dark-600 border-gray"
                />
                <span className="text-sm">{section}</span>
              </label>
            ))}
          </div>

          <div className="flex flex-col sm:flex-row items-stretch sm:items-center gap-3">
            <select 
              value={reportFormat}
              onChange={(e) => setReportFormat(e.target.value)}
              className="flex-1 px-4 py-2.5 bg-dark-700 border border-gray/20 rounded-lg text-sm text-white focus:outline-none focus:border-primary/50 transition-colors"
            >
              <option>PDF Format</option>
              <option>Excel Format</option>
              <option>Both Formats</option>
            </select>
            <button 
              onClick={handleGenerateCustomReport}
              className="px-6 py-2.5 bg-primary text-dark hover:bg-button-hover hover:scale-105 rounded-lg font-semibold transition-all duration-200 whitespace-nowrap"
            >
              Generate Custom Report
            </button>
          </div>
        </motion.div>

        {/* Data Persistence Note */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.4 }}
          className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-primary/20 p-6"
        >
          <div className="flex items-start gap-4">
            <Check className="w-6 h-6 text-primary flex-shrink-0 mt-1" />
            <div>
              <p className="font-semibold text-primary mb-2">Data Persistence</p>
              <p className="text-sm text-gray">
                Your optimization results are saved in your browser's local storage and will be available 
                in Session History on your next visit. To save results permanently, please download the 
                exports above.
              </p>
              <button 
                onClick={() => {
                  if (confirm('Are you sure you want to clear local storage? This will remove all saved optimization results.')) {
                    localStorage.clear();
                    alert('Local storage cleared successfully!');
                  }
                }}
                className="text-sm text-red-400 hover:text-red-300 transition-colors mt-3 hover:underline"
              >
                Clear local storage →
              </button>
            </div>
          </div>
        </motion.div>
      </div>
    </div>
  );
}

