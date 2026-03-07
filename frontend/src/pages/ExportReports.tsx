import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useApp } from '../context/AppContext';
// Material Symbols icon helper
const MIcon = ({ name, className = '' }: { name: string; className?: string }) => (
  <span className={`material-symbols-outlined ${className}`}>{name}</span>
);
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
      <div className="min-h-screen p-6 lg:p-8">
        <div className="max-w-[1400px] mx-auto">
          <div className="text-center py-20">
            <h1 className="text-3xl font-bold mb-4">Export & Reports</h1>
            <p className="text-xs font-mono text-white/30 mb-6">No optimization results available</p>
            <Link to="/upload" className="inline-flex items-center gap-2 px-6 py-3 bg-primary text-background-dark font-label font-bold uppercase tracking-widest glow-amber transition-all">
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
      icon: 'table_chart',
      format: '.xlsx',
      color: 'text-primary-bright',
      action: () => handleExport('employees'),
    },
    {
      title: 'Vehicle Routes (Excel)',
      description: 'All vehicles with their trip details, sequences, distances, costs',
      icon: 'table_chart',
      format: '.xlsx',
      color: 'text-primary-bright',
      action: () => handleExport('vehicles'),
    },
    {
      title: 'Solver Solution (JSON)',
      description: 'Raw solver output from the backend — exact C++ solver results',
      icon: 'data_object',
      format: '.json',
      color: 'text-primary-bright',
      action: () => handleExport('solution'),
    },
    {
      title: 'Complete Results (JSON)',
      description: 'All optimization outputs in structured format for developers',
      icon: 'data_object',
      format: '.json',
      color: 'text-primary-bright',
      action: () => handleExport('json'),
    },
  ];

  return (
    <div className="min-h-screen p-6 lg:p-8">
      <div className="max-w-[1400px] mx-auto space-y-6">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
        >
          <h1 className="text-2xl font-black uppercase tracking-tight text-white">
            Export & Reports
          </h1>
          <p className="text-xs font-mono text-white/30 mt-1">Download your optimization results in various formats</p>
        </motion.div>

        {/* Quick Exports */}
        <motion.div 
          initial={{ opacity: 0, y: 20 }} 
          animate={{ opacity: 1, y: 0 }}
          className="bg-panel-dark border border-white/10 p-6"
        >
          <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-6">Quick Exports</h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
            {exportCards.map((card, index) => (
              <motion.div
                key={card.title}
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: index * 0.1 }}
                className="bg-white/[0.02] border border-white/10 p-5"
              >
                <MIcon name={card.icon} className={`text-4xl ${card.color} mb-3`} />
                <h3 className="font-label font-bold mb-2 text-xs uppercase tracking-widest text-white/70">{card.title}</h3>
                <p className="text-[10px] font-mono text-white/30 mb-4 line-clamp-2">{card.description}</p>
                <div className="flex items-center justify-between">
                  <span className="text-[9px] font-mono text-white/20">{card.format}</span>
                  <button 
                    onClick={card.action} 
                    className="flex items-center gap-1.5 px-3 py-1.5 bg-primary text-background-dark text-xs font-label font-bold uppercase tracking-widest hover:brightness-110 transition-all duration-200"
                  >
                    <MIcon name="download" className="text-sm" />
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
          className="bg-panel-dark border border-white/10 p-6"
        >
          <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Custom Report Builder</h2>
          <p className="text-xs font-mono text-white/30 mb-6">Select sections to include in your custom report</p>
          
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
                className="flex items-center gap-3 p-3 bg-white/[0.02] border border-white/10 cursor-pointer hover:bg-white/[0.04] hover:border-white/10 border border-transparent transition-all"
              >
                <input 
                  type="checkbox" 
                  checked={selectedSections.includes(section)}
                  onChange={() => toggleSection(section)}
                  className="w-4 h-4 text-primary rounded focus:ring-primary focus:ring-offset-0 focus:ring-2 bg-white/[0.04] border-white/10"
                />
                <span className="text-xs font-mono text-white/60">{section}</span>
              </label>
            ))}
          </div>

          <div className="flex flex-col sm:flex-row items-stretch sm:items-center gap-3">
            <select 
              value={reportFormat}
              onChange={(e) => setReportFormat(e.target.value)}
              className="flex-1 px-4 py-2.5 bg-white/[0.02] border border-white/10 text-xs font-mono text-white focus:outline-none focus:border-primary/40 transition-colors"
            >
              <option>PDF Format</option>
              <option>Excel Format</option>
              <option>Both Formats</option>
            </select>
            <button 
              onClick={handleGenerateCustomReport}
              className="px-6 py-2.5 bg-primary text-background-dark font-label font-bold uppercase tracking-widest glow-amber transition-all duration-200 whitespace-nowrap"
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
          className="bg-panel-dark border border-primary/20 border-t-2 border-t-primary p-6"
        >
          <div className="flex items-start gap-4">
            <MIcon name="check_circle" className="text-2xl text-primary flex-shrink-0 mt-1" />
            <div>
              <p className="font-label font-bold text-primary/80 mb-2 text-[10px] uppercase tracking-widest">Data Persistence</p>
              <p className="text-xs font-mono text-white/30">
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

