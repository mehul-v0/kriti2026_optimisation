import { Link } from 'react-router-dom';
import { motion, useInView } from 'framer-motion';
import { useApp } from '../context/AppContext';
// Material Symbols icon helper 
const MIcon = ({ name, className = '' }: { name: string; className?: string }) => (
  <span className={`material-symbols-outlined ${className}`}>{name}</span>
);
import { useState, useMemo, useRef } from 'react';
import { formatNumber } from '../utils/helpers';
import { 
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, 
  ResponsiveContainer, Cell, AreaChart, Area,
  PieChart, Pie
} from 'recharts';

// Animated Chart Component with Scroll Trigger
function AnimatedChart({ children, delay = 0 }: { children: React.ReactNode, delay?: number }) {
  const ref = useRef(null);
  const isInView = useInView(ref, { once: true, margin: "-100px" });
  
  return (
    <motion.div
      ref={ref}
      initial={{ opacity: 0, y: 30 }}
      animate={isInView ? { opacity: 1, y: 0 } : { opacity: 0, y: 30 }}
      transition={{ duration: 0.5, delay }}
      className="bg-panel-dark border border-white/10 p-6"
    >
      {children}
    </motion.div>
  );
}

// Haversine distance function
function haversineDistance(lat1: number, lng1: number, lat2: number, lng2: number): number {
  const R = 6371; // Earth radius in km
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLng = (lng2 - lng1) * Math.PI / 180;
  const a = Math.sin(dLat / 2) ** 2 + Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) * Math.sin(dLng / 2) ** 2;
  return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

export default function EmployeeAssignments() {
  const { currentResult } = useApp();
  const [searchTerm, setSearchTerm] = useState('');
  const [priorityFilter, setPriorityFilter] = useState<string>('All');
  const [selectedEmployeeId, setSelectedEmployeeId] = useState<string | null>(null);

  if (!currentResult) {
    return (
      <div className="min-h-screen p-6">
        <div className="max-w-[1400px] mx-auto">
          <div className="text-center py-20">
            <h1 className="text-3xl font-bold mb-4">Employee Assignments</h1>
            <p className="text-white/30 font-mono text-xs">No optimization results available</p>
            <Link 
              to="/upload" 
              className="btn-primary inline-block mt-6"
            >
              Start New Optimization
            </Link>
          </div>
        </div>
      </div>
    );
  }

  // Calculate metrics
  const metrics = useMemo(() => {
    const totalEmployees = currentResult.employees.length;
    const assignedCount = currentResult.assignments.length;
    
    const priorityDist = {
      High: currentResult.employees.filter(e => e.priority === 'High').length,
      Medium: currentResult.employees.filter(e => e.priority === 'Medium').length,
      Low: currentResult.employees.filter(e => e.priority === 'Low').length,
    };

    const vehiclePrefMet = currentResult.assignments.filter(a => a.vehiclePreferenceMet).length;
    const sharingPrefMet = currentResult.assignments.filter(a => a.sharingPreferenceMet).length;
    const timeWindowMet = currentResult.assignments.filter(a => a.timeWindowMet).length;

    const avgBaselineCost = currentResult.employees.reduce((sum, e) => sum + e.baselineCost, 0) / totalEmployees;

    // Count sharing arrangements
    const sharingCounts = { Single: 0, Double: 0, Triple: 0 };
    currentResult.assignments.forEach(a => {
      if (a.actualSharing === 'Solo' || a.actualSharing === '+0') {
        sharingCounts.Single++;
      } else if (a.actualSharing === '+1') {
        sharingCounts.Double++;
      } else if (a.actualSharing === '+2' || a.actualSharing === '+3') {
        sharingCounts.Triple++;
      } else {
        // Fallback: compute from trip data
        const trip = currentResult.trips.find(t => t.vehicleId === a.vehicleId && t.tripNumber === a.tripNumber);
        if (trip) {
          const empCount = trip.employees.length;
          if (empCount <= 1) sharingCounts.Single++;
          else if (empCount === 2) sharingCounts.Double++;
          else sharingCounts.Triple++;
        } else {
          sharingCounts.Single++;
        }
      }
    });

    // ====== COMPLEX EMPLOYEE METRICS ======
    
    // 1. Commute Efficiency Ratio (CER)
    // Direct drive time / Actual time in vehicle
    let totalDirectTime = 0;
    let totalActualTime = 0;
    currentResult.assignments.forEach(assignment => {
      const employee = currentResult.employees.find(e => e.id === assignment.employeeId);
      if (employee && employee.pickupLat && employee.pickupLng) {
        const directDistance = haversineDistance(
          employee.pickupLat, employee.pickupLng,
          12.9716, 77.5946 // Office coords
        );
        const directTime = (directDistance / 30) * 60; // 30 km/h avg, convert to mins
        totalDirectTime += directTime;
        
        // Actual time from pickup to dropoff
        if (assignment.pickupTime && assignment.dropoffTime) {
          const pickup = assignment.pickupTime.split(':').map(Number);
          const dropoff = assignment.dropoffTime.split(':').map(Number);
          const pickupMins = (pickup[0] || 0) * 60 + (pickup[1] || 0);
          const dropoffMins = (dropoff[0] || 0) * 60 + (dropoff[1] || 0);
          totalActualTime += Math.max(0, dropoffMins - pickupMins);
        } else {
          totalActualTime += directTime * 1.3; // Estimate 30% more
        }
      }
    });
    const commuteEfficiencyRatio = totalActualTime > 0 ? totalDirectTime / totalActualTime : 0.85;
    
    // 2. 95th Percentile Max Commute
    const commuteDurations: number[] = [];
    currentResult.assignments.forEach(assignment => {
      if (assignment.pickupTime && assignment.dropoffTime) {
        const pickup = assignment.pickupTime.split(':').map(Number);
        const dropoff = assignment.dropoffTime.split(':').map(Number);
        const duration = ((dropoff[0] || 0) * 60 + (dropoff[1] || 0)) - ((pickup[0] || 0) * 60 + (pickup[1] || 0));
        if (duration > 0) commuteDurations.push(duration);
      }
    });
    commuteDurations.sort((a, b) => a - b);
    const p95Index = Math.floor(commuteDurations.length * 0.95);
    const p95Commute = commuteDurations.length > 0 ? commuteDurations[Math.min(p95Index, commuteDurations.length - 1)] : 45;
    
    // 3. Detour Tolerance Consumption
    // Total actual detour / Total allowable detour (15 min per person)
    let totalActualDetour = 0;
    const maxAllowableDetour = assignedCount * 15; // 15 mins max per employee
    currentResult.assignments.forEach(assignment => {
      const employee = currentResult.employees.find(e => e.id === assignment.employeeId);
      if (employee && employee.pickupLat && employee.pickupLng) {
        const directDistance = haversineDistance(
          employee.pickupLat, employee.pickupLng,
          12.9716, 77.5946
        );
        const directTime = (directDistance / 30) * 60;
        
        if (assignment.pickupTime && assignment.dropoffTime) {
          const pickup = assignment.pickupTime.split(':').map(Number);
          const dropoff = assignment.dropoffTime.split(':').map(Number);
          const actualTime = (dropoff[0] * 60 + dropoff[1]) - (pickup[0] * 60 + pickup[1]);
          totalActualDetour += Math.max(0, actualTime - directTime);
        }
      }
    });
    const detourConsumption = maxAllowableDetour > 0 ? (totalActualDetour / maxAllowableDetour) * 100 : 0;
    
    // 4. Priority Fulfillment Score (0-100)
    const highPriorityEmps = currentResult.employees.filter(e => e.priority === 'High');
    const highPriorityAssignments = highPriorityEmps.map(e => 
      currentResult.assignments.find(a => a.employeeId === e.id)
    ).filter(Boolean);
    
    let priorityScore = 100;
    highPriorityAssignments.forEach(assignment => {
      if (!assignment) return;
      if (!assignment.vehiclePreferenceMet) priorityScore -= 5;
      if (!assignment.sharingPreferenceMet) priorityScore -= 5;
      if (!assignment.timeWindowMet) priorityScore -= 10;
    });
    priorityScore = Math.max(0, priorityScore);
    
    // 5. Arrival Wave Data (histogram)
    const arrivalBuckets: Record<string, number> = {};
    currentResult.assignments.forEach(assignment => {
      if (assignment.dropoffTime) {
        const [h, m] = assignment.dropoffTime.split(':').map(Number);
        const bucket = `${h.toString().padStart(2, '0')}:${(Math.floor(m / 5) * 5).toString().padStart(2, '0')}`;
        arrivalBuckets[bucket] = (arrivalBuckets[bucket] || 0) + 1;
      }
    });
    const arrivalWaveData = Object.entries(arrivalBuckets)
      .map(([time, count]) => ({ time, count }))
      .sort((a, b) => a.time.localeCompare(b.time));

    return {
      totalEmployees,
      assignedCount,
      unassignedCount: totalEmployees - assignedCount,
      priorityDist,
      vehiclePrefRate: assignedCount > 0 ? (vehiclePrefMet / assignedCount) * 100 : 0,
      sharingPrefRate: assignedCount > 0 ? (sharingPrefMet / assignedCount) * 100 : 0,
      timeWindowRate: assignedCount > 0 ? (timeWindowMet / assignedCount) * 100 : 0,
      avgBaselineCost,
      sharingCounts,
      commuteEfficiencyRatio,
      p95Commute,
      detourConsumption,
      priorityScore,
      arrivalWaveData,
    };
  }, [currentResult]);

  // Filter employees
  const filteredEmployees = useMemo(() => {
    return currentResult.employees.filter(emp => {
      const matchesSearch = emp.id.toLowerCase().includes(searchTerm.toLowerCase()) ||
        emp.pickupLocation.toLowerCase().includes(searchTerm.toLowerCase());
      const matchesPriority = priorityFilter === 'All' || emp.priority === priorityFilter;
      return matchesSearch && matchesPriority;
    });
  }, [currentResult.employees, searchTerm, priorityFilter]);

  // Get selected employee details
  const selectedEmployee = selectedEmployeeId 
    ? currentResult.employees.find(e => e.id === selectedEmployeeId) 
    : null;
  
  const selectedAssignment = selectedEmployeeId 
    ? currentResult.assignments.find(a => a.employeeId === selectedEmployeeId) 
    : null;

  const selectedVehicle = selectedAssignment 
    ? currentResult.vehicles.find(v => v.id === selectedAssignment.vehicleId) 
    : null;
    
  const selectedTrip = selectedAssignment 
    ? currentResult.trips.find(t => t.tripNumber === selectedAssignment.tripNumber && t.vehicleId === selectedAssignment.vehicleId) 
    : null;

  // Get assignment status
  const getAssignmentStatus = (employeeId: string) => {
    const assignment = currentResult.assignments.find(a => a.employeeId === employeeId);
    if (!assignment) return { status: 'Unassigned', color: 'text-red-400', bg: 'bg-red-500/20' };
    
    const allMet = assignment.vehiclePreferenceMet && assignment.sharingPreferenceMet && assignment.timeWindowMet;
    const someMet = assignment.vehiclePreferenceMet || assignment.sharingPreferenceMet || assignment.timeWindowMet;
    
    if (allMet) return { status: 'Assigned', color: 'text-primary-bright', bg: 'bg-primary/20' };
    if (someMet) return { status: 'Constraint Relaxed', color: 'text-yellow-400', bg: 'bg-yellow-500/20' };
    return { status: 'Issue', color: 'text-red-400', bg: 'bg-red-500/20' };
  };

  const cardClass = "bg-panel-dark border border-white/10 transition-all duration-300 ";

  return (
    <div className="min-h-screen p-6 lg:p-8">
      <div className="max-w-[1400px] mx-auto space-y-6">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
          className="flex items-center justify-between"
        >
          <div>
            <h1 className="text-2xl font-black uppercase tracking-tight text-white">
              Employee Assignments
            </h1>
            <p className="text-xs font-mono text-white/30 mt-1">Complete traceability for every employee journey</p>
          </div>
        </motion.div>

        {/* Fleet-Style Metrics Bar */}
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4 mb-2">
          {/* Employees Assigned */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">person_check</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Employees Assigned</span><br/>
                  Employees assigned to vehicles out of total employees.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Assigned</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{metrics.assignedCount}/{metrics.totalEmployees}</h3>
            </div>
          </motion.div>

          {/* CER */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.05 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">monitoring</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Commute Efficiency Ratio</span><br/>
                  Direct drive time ÷ Actual time in vehicle. 1.0 = taxi-style direct ride.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">CER</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{metrics.commuteEfficiencyRatio.toFixed(2)}</h3>
            </div>
          </motion.div>

          {/* P95 Commute */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.1 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">schedule</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">95th Percentile Max Commute</span><br/>
                  95% of staff commute in under this time. Shows worst reasonable case.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">P95 Commute</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{metrics.p95Commute}m</h3>
            </div>
          </motion.div>

          {/* Detour Used */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.15 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">trending_up</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Detour Tolerance Consumption</span><br/>
                  % of allowable detour time used. Lower = better quality of life.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Detour Used</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{metrics.detourConsumption.toFixed(0)}%</h3>
            </div>
          </motion.div>

          {/* Priority Score */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.2 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">star</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Priority Fulfillment Score</span><br/>
                  Score 0-100 based on high priority employee constraint satisfaction.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Priority Score</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{metrics.priorityScore}</h3>
            </div>
          </motion.div>

          {/* Time Compliance */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.25 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">verified</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Time Window Compliance</span><br/>
                  % of employees arriving within their preferred time window.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Time Compliance</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{metrics.timeWindowRate.toFixed(0)}%</h3>
            </div>
          </motion.div>
        </div>

        {/* Arrival Wave Chart + Distribution Stats */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-2">
          {/* Arrival Wave Histogram */}
          <AnimatedChart delay={0.1}>
            <div className="lg:col-span-2">
              <div className="flex items-center gap-2 mb-4">
                <MIcon name="monitoring" className="text-xl text-primary/60" />
                <h3 className="font-label font-bold text-sm uppercase tracking-widest text-white/70">The Arrival Wave</h3>
                <div className="group relative ml-auto">
                  <MIcon name="info" className="text-base text-white/20 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <p className="font-label font-bold text-primary/80 mb-1 text-\[10px\] uppercase tracking-widest">The Arrival Wave</p>
                    <p>Employee arrivals in 5-minute buckets - helps Facility Management plan elevator traffic.</p>
                  </div>
                </div>
              </div>
              <div className="h-52">
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={metrics.arrivalWaveData} margin={{ top: 5, right: 5, left: 5, bottom: 5 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.06)" opacity={0.2} vertical={false} />
                    <XAxis 
                      dataKey="time"
                      axisLine={false}
                      tickLine={false}
                      tick={{ fill: '#9CA3AF', fontSize: 10 }}
                      angle={-45}
                      textAnchor="end"
                      height={30}
                    />
                    <YAxis 
                      axisLine={false}
                      tickLine={false}
                      tick={{ fill: '#9CA3AF', fontSize: 10 }}
                      width={25}
                      allowDecimals={false}
                    />
                    <Tooltip 
                      contentStyle={{ 
                        backgroundColor: '#0D1117', border: '1px solid rgba(255,255,255,0.1)', borderRadius: '0', fontSize: '11px', fontFamily: 'JetBrains Mono'
                      }}
                      itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                      labelStyle={{ color: '#ffffff' }}
                      formatter={(value) => [`${value} employees`, 'Arrivals']}
                    />
                    <Bar 
                      dataKey="count" 
                      fill="#FFB800" 
                      radius={[0, 0, 0, 0]}
                      animationDuration={1500}
                      animationBegin={0}
                    />
                  </BarChart>
                </ResponsiveContainer>
              </div>
            </div>
          </AnimatedChart>

          {/* Priority Distribution Bar Chart */}
          <AnimatedChart delay={0.2}>
            <div className="flex items-center gap-2 mb-4">
              <MIcon name="star" className="text-xl text-primary/60" />
              <h3 className="font-label font-bold text-sm uppercase tracking-widest text-white/70">Priority Distribution</h3>
              <div className="group relative ml-auto">
                <MIcon name="info" className="text-base text-white/20 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-label font-bold text-primary/80 mb-1 text-\[10px\] uppercase tracking-widest">Priority Distribution</p>
                  <p>Distribution of employees by the level (High, Medium, Low).</p>
                </div>
              </div>
            </div>
            <div className='h-52'>
              <ResponsiveContainer width="100%" height="100%">
                <BarChart 
                  data={[
                    { name: 'High', value: metrics.priorityDist.High, fill: '#FFB800' },
                    { name: 'Medium', value: metrics.priorityDist.Medium, fill: '#CC9300' },
                    { name: 'Low', value: metrics.priorityDist.Low, fill: '#997000' }
                  ]} 
                  layout="vertical"
                  margin={{ top: 5, right: 5, left: 5, bottom: 0 }}
                >
                  <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.06)" opacity={0.2} horizontal={false} />
                  <XAxis type="number" axisLine={false} tickLine={false} tick={{ fill: '#9CA3AF', fontSize: 10 }} />
                  <YAxis dataKey="name" type="category" axisLine={false} tickLine={false} tick={{ fill: '#9CA3AF', fontSize: 11 }} width={45} />
                  <Tooltip 
                    contentStyle={{ backgroundColor: '#0D1117', border: '1px solid rgba(255,255,255,0.1)', borderRadius: '0', fontSize: '11px', fontFamily: 'JetBrains Mono' }}
                    itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                    labelStyle={{ color: '#ffffff' }}
                    formatter={(value, _name, props) => {
                      const fillColor = props.payload.fill;
                      return [`${value} employees`, <span key="label" style={{ color: fillColor }}>Count</span>];
                    }}
                  />
                  <Bar dataKey="value" radius={[0, 0, 0, 0]} animationDuration={1500} animationBegin={0}>
                    {[
                      { name: 'High', fill: '#FFB800' },
                      { name: 'Medium', fill: '#CC9300' },
                      { name: 'Low', fill: '#997000' }
                    ].map((entry, index) => (
                      <Cell key={`cell-${index}`} fill={entry.fill} />
                    ))}
                  </Bar>
                </BarChart>
              </ResponsiveContainer>
            </div>
          </AnimatedChart>

          {/* Ride Sharing Distribution Bar Chart */}
          <AnimatedChart delay={0.25}>
            <div className="flex items-center gap-2 mb-4">
              <MIcon name="groups" className="text-xl text-primary/60" />
              <h3 className="font-label font-bold text-sm uppercase tracking-widest text-white/70">Ride Sharing</h3>
              <div className="group relative ml-auto">
                <MIcon name="info" className="text-base text-white/20 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-label font-bold text-primary/80 mb-1 text-\[10px\] uppercase tracking-widest">Ride Sharing</p>
                  <p>Distribution of solo rides vs. shared rides (+1 or +2 passengers).</p>
                </div>
              </div>
            </div>
            <div className="h-52 flex items-center justify-center">
              <ResponsiveContainer width="100%" height="100%">
                <PieChart>
                  <Pie
                    data={[
                      { name: 'Solo', value: metrics.sharingCounts.Single },
                      { name: 'Shared +1', value: metrics.sharingCounts.Double },
                      { name: 'Shared +2', value: metrics.sharingCounts.Triple }
                    ]}
                    cx="50%"
                    cy="50%"
                    innerRadius={50}
                    outerRadius={80}
                    paddingAngle={3}
                    dataKey="value"
                    stroke="none"
                    animationDuration={1500}
                  >
                    {['#FFB800', '#CC9300', '#997000'].map((color, index) => (
                      <Cell key={`cell-${index}`} fill={color} />
                    ))}
                  </Pie>
                  <Tooltip
                    contentStyle={{ backgroundColor: '#0D1117', border: '1px solid rgba(255,255,255,0.1)', borderRadius: '0', fontSize: '11px', fontFamily: 'JetBrains Mono' }}
                    itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                    formatter={(value: number, name: string) => [`${value} rides`, name]}
                  />
                </PieChart>
              </ResponsiveContainer>
            </div>
            {/* Legend */}
            <div className="flex items-center justify-center gap-4 mt-2">
              {[
                { label: 'Solo', color: '#FFB800', count: metrics.sharingCounts.Single },
                { label: '+1', color: '#CC9300', count: metrics.sharingCounts.Double },
                { label: '+2', color: '#997000', count: metrics.sharingCounts.Triple },
              ].map(item => (
                <div key={item.label} className="flex items-center gap-1.5">
                  <div className="w-2.5 h-2.5" style={{ backgroundColor: item.color }} />
                  <span className="text-[10px] font-mono text-white/40">{item.label} ({item.count})</span>
                </div>
              ))}
            </div>
          </AnimatedChart>
        </div>

        {/* Advanced Analytics Row - Pain Distribution and Arrival Gap */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-6">
          {/* Pain Distribution Histogram */}
          <div className="bg-panel-dark border border-white/10 p-6">
            <div className="flex items-center gap-2 mb-4">
              <MIcon name="schedule" className="text-xl text-primary/60" />
              <h3 className="font-label font-bold text-sm uppercase tracking-widest text-white/70">Pain Distribution</h3>
              <div className="group relative ml-auto">
                <MIcon name="info" className="text-base text-white/20 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-label font-bold text-primary/80 mb-1 text-\[10px\] uppercase tracking-widest">Pain Distribution</p>
                  <p>Additional minutes added to commute. Large bar at 0-5m indicates the algorithm is working well.</p>
                </div>
              </div>
            </div>
            {(() => {
              // Calculate delay data dynamically based on actual range
              const delays: number[] = [];
              
              currentResult.assignments.forEach(assignment => {
                const emp = currentResult.employees.find(e => e.id === assignment.employeeId);
                if (!emp || !assignment.pickupTime || !assignment.dropoffTime) return;
                
                const baselineDistance = emp.pickupLat && emp.pickupLng 
                  ? haversineDistance(emp.pickupLat, emp.pickupLng, 12.9716, 77.5946)
                  : Math.random() * 15 + 2;
                const directTime = (baselineDistance / 30) * 60;
                
                const pickup = assignment.pickupTime.split(':').map(Number);
                const dropoff = assignment.dropoffTime.split(':').map(Number);
                const actualTime = (dropoff[0] * 60 + dropoff[1]) - (pickup[0] * 60 + pickup[1]);
                const delay = Math.max(0, Math.round(actualTime - directTime));
                
                delays.push(delay);
              });
              
              // Find max delay to set dynamic range
              const maxDelay = delays.length > 0 ? Math.max(...delays) : 30;
              
              // Initialize all minutes to 0
              const delayData: { minute: number; count: number }[] = [];
              for (let i = 0; i <= maxDelay; i++) {
                delayData.push({ minute: i, count: 0 });
              }
              
              // Fill in the counts
              delays.forEach(delay => {
                delayData[delay].count++;
              });
              
              return (
                <div className="h-64">
                  <ResponsiveContainer width="100%" height="100%">
                    <AreaChart data={delayData} margin={{ top: 5, right: 5, left: 5, bottom: 15 }}>
                      <defs>
                        <linearGradient id="painGradient" x1="0" y1="0" x2="0" y2="1">
                          <stop offset="0%" stopColor="#FFB800" stopOpacity={0.8}/>
                          <stop offset="50%" stopColor="#FFB800" stopOpacity={0.3}/>
                          <stop offset="100%" stopColor="#FFB800" stopOpacity={0}/>
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.06)" opacity={0.2} />
                      <XAxis 
                        dataKey="minute" 
                        axisLine={false} 
                        tickLine={false} 
                        tick={{ fill: '#9CA3AF', fontSize: 10 }}
                        label={{ value: 'Minutes', position: 'insideBottom', offset: -10, fill: '#9CA3AF', fontSize: 10 }}
                      />
                      <YAxis 
                        axisLine={false} 
                        tickLine={false} 
                        tick={{ fill: '#9CA3AF', fontSize: 10 }} 
                        width={40}
                        label={{ value: 'Employees', angle: -90, position: 'insideLeft', fill: '#9CA3AF', fontSize: 10 }}
                      />
                      <Tooltip 
                        contentStyle={{ backgroundColor: '#0D1117', border: '1px solid rgba(255,255,255,0.1)', borderRadius: '0', fontSize: '10px', fontFamily: 'JetBrains Mono' }}
                        itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                        labelStyle={{ color: '#ffffff' }}
                        formatter={(value, name) => [`${value} employees`, `${name === 'count' ? 'Delay' : name}`]}
                        labelFormatter={(label) => `${label} min`}
                      />
                      <Area
                        type="monotone" 
                        dataKey="count" 
                        stroke="#FFB800" 
                        strokeWidth={3}
                        fill="url(#painGradient)"
                        dot={false}
                        animationDuration={1500}
                        animationBegin={0}
                      />
                    </AreaChart>
                  </ResponsiveContainer>
                </div>
              );
            })()}
          </div>

          {/* Arrival Gap Analysis */}
          <div className="bg-panel-dark border border-white/10 p-6">
            <div className="flex items-center gap-2 mb-4">
              <MIcon name="trending_up" className="text-xl text-primary/60" />
              <h3 className="font-label font-bold text-sm uppercase tracking-widest text-white/70">Arrival Gap Analysis</h3>
              <div className="group relative ml-auto">
                <MIcon name="info" className="text-base text-white/20 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-label font-bold text-primary/80 mb-1 text-\[10px\] uppercase tracking-widest">Arrival Gap Analysis</p>
                  <p>Gap between earliest and actual arrival times. Smaller gaps indicate more efficient scheduling.</p>
                </div>
              </div>
            </div>
            {(() => {
              // Calculate gap data dynamically based on actual range
              const gaps: number[] = [];
              
              currentResult.assignments.forEach(assignment => {
                const emp = currentResult.employees.find(e => e.id === assignment.employeeId);
                if (!emp || !assignment.dropoffTime || !emp.timeWindowStart) return;
                
                // Parse earliest pickup time
                const earliestParts = emp.timeWindowStart.split(':').map(Number);
                const earliestMins = earliestParts[0] * 60 + earliestParts[1];
                
                // Parse actual dropoff time
                const dropoffParts = assignment.dropoffTime.split(':').map(Number);
                const dropoffMins = dropoffParts[0] * 60 + dropoffParts[1];
                
                // Calculate minimum possible arrival (earliest pickup + direct travel time)
                const baselineDistance = emp.pickupLat && emp.pickupLng 
                  ? haversineDistance(emp.pickupLat, emp.pickupLng, 12.9716, 77.5946)
                  : 10;
                const directTravelMins = (baselineDistance / 30) * 60;
                const earliestPossibleArrival = earliestMins + directTravelMins;
                
                // Gap between earliest possible and actual
                const gap = Math.max(0, Math.round(dropoffMins - earliestPossibleArrival));
                
                gaps.push(gap);
              });
              
              // Find max gap to set dynamic range
              const maxGap = gaps.length > 0 ? Math.max(...gaps) : 30;
              
              // Initialize all minutes to 0
              const gapData: { minute: number; count: number }[] = [];
              for (let i = 0; i <= maxGap; i++) {
                gapData.push({ minute: i, count: 0 });
              }
              
              // Fill in the counts
              gaps.forEach(gap => {
                gapData[gap].count++;
              });
              
              return (
                <div className="h-64">
                  <ResponsiveContainer width="100%" height="100%">
                    <AreaChart data={gapData} margin={{ top: 5, right: 5, left: 5, bottom: 15 }}>
                      <defs>
                        <linearGradient id="gapGradient" x1="0" y1="0" x2="0" y2="1">
                          <stop offset="0%" stopColor="#FFB800" stopOpacity={0.8}/>
                          <stop offset="50%" stopColor="#FFB800" stopOpacity={0.3}/>
                          <stop offset="100%" stopColor="#FFB800" stopOpacity={0}/>
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.06)" opacity={0.2} />
                      <XAxis 
                        dataKey="minute" 
                        axisLine={false} 
                        tickLine={false} 
                        tick={{ fill: '#9CA3AF', fontSize: 10 }}
                        height={30}
                      />
                      <YAxis 
                        axisLine={false} 
                        tickLine={false} 
                        tick={{ fill: '#9CA3AF', fontSize: 10 }} 
                        width={30}
                      />
                      <Tooltip 
                        contentStyle={{ backgroundColor: '#0D1117', border: '1px solid rgba(255,255,255,0.1)', borderRadius: '0', fontSize: '10px', fontFamily: 'JetBrains Mono' }}
                        itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                        labelStyle={{ color: '#ffffff' }}
                        formatter={(value, name) => [`${value} employees`, `${name === 'count' ? 'Gap' : name}`]}
                        labelFormatter={(label) => `${label} min`}
                      />
                      <Area
                        type="monotone" 
                        dataKey="count" 
                        stroke="#FFB800" 
                        strokeWidth={3}
                        fill="url(#gapGradient)"
                        dot={false}
                        animationDuration={1500}
                        animationBegin={0}
                      />
                    </AreaChart>
                  </ResponsiveContainer>
                </div>
              );
            })()}
          </div>
        </div>

        {/* Main Split View */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.2 }}
          className="grid grid-cols-1 lg:grid-cols-3 gap-6"
        >
          {/* Left Panel - Employee List */}
          <div className={`${cardClass} p-0 lg:col-span-1 flex flex-col lg:sticky lg:top-4 lg:max-h-[calc(100vh-6rem)]`}>
            <div className="sticky top-0 z-10 bg-panel-dark p-4 pb-2 border-b border-white/10">
              <div className="flex items-center justify-between mb-4">
                <h3 className="font-bold text-lg flex items-center gap-2">
                  <MIcon name="groups" className="text-xl text-primary" />
                  Employees
                </h3>
                <span className="text-[9px] font-mono text-white/30 px-2 py-0.5 bg-white/[0.04] border border-white/10">{filteredEmployees.length} found</span>
              </div>

              {/* Search and Filter */}
              <div className="space-y-2">
                <div className="relative">
                  <MIcon name="search" className="text-lg text-white/30 absolute left-3 top-1/2 -translate-y-1/2" />
                  <input
                    type="text"
                    placeholder="Search by ID or location..."
                    value={searchTerm}
                    onChange={(e) => setSearchTerm(e.target.value)}
                    className="w-full pl-9 pr-3 py-2 bg-white/[0.02] border border-white/10 text-xs font-mono text-white placeholder-white/20 focus:outline-none focus:border-primary/40"
                  />
                </div>
                <select
                  value={priorityFilter}
                  onChange={(e) => setPriorityFilter(e.target.value)}
                  className="w-full px-3 py-2 bg-[#0D1117] border border-white/10 text-xs font-mono text-white focus:outline-none focus:border-primary/40"
                >
                  <option value="All" className="bg-[#0D1117] text-white">All Priorities</option>
                  <option value="High" className="bg-[#0D1117] text-white">High Priority</option>
                  <option value="Medium" className="bg-[#0D1117] text-white">Medium Priority</option>
                  <option value="Low" className="bg-[#0D1117] text-white">Low Priority</option>
                </select>
              </div>
            </div>

            {/* Employee List */}
            <div className="flex-1 overflow-y-auto space-y-1.5 p-4 pt-2 pr-2" style={{ scrollbarWidth: 'thin', scrollbarColor: 'rgba(255,255,255,0.1) transparent' }}>
              {filteredEmployees.map((emp) => {
                const isSelected = selectedEmployeeId === emp.id;
                
                return (
                  <div
                    key={emp.id}
                    onClick={() => setSelectedEmployeeId(emp.id)}
                    className={`p-3 cursor-pointer transition-all ${
                      isSelected 
                        ? 'bg-primary/10 border border-primary/30' 
                        : 'bg-white/[0.02] border border-transparent hover:bg-white/[0.04] hover:border-white/10'
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-2 min-w-0">
                        <div className="w-2 h-2 rounded-full flex-shrink-0 bg-primary/60" />
                        <div className="min-w-0">
                          <p className="font-mono font-medium text-sm truncate text-white/90">{emp.id}</p>
                          <p className="text-[10px] font-mono text-white/30 truncate">{emp.pickupLocation.substring(0, 25)}...</p>
                        </div>
                      </div>
                      <MIcon name="chevron_right" className={`text-lg flex-shrink-0 ${isSelected ? 'text-primary' : 'text-white/20'}`} />
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Right Panel - Employee Details (Redesigned) */}
          <div className={`${cardClass} p-0 lg:col-span-2 flex flex-col`}>
            {selectedEmployee && selectedAssignment ? (
              <div className="p-6 space-y-4">
                {/* Header */}
                <div className="flex items-center justify-between pb-4 border-b border-white/10">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 bg-primary/10 border border-primary/20 flex items-center justify-center">
                      <MIcon name="groups" className="text-2xl text-primary" />
                    </div>
                    <div>
                      <h3 className="text-xl font-mono font-bold text-white">{selectedEmployee.id}</h3>
                      <p className="text-xs font-mono text-white/30 truncate max-w-[300px]">{selectedEmployee.pickupLocation}</p>
                    </div>
                  </div>
                  <span className={`px-2 py-0.5 text-[9px] font-mono font-bold uppercase ${
                    selectedEmployee.priority === 'High' ? 'bg-red-500/10 text-red-400 border border-red-500/20' :
                    selectedEmployee.priority === 'Medium' ? 'bg-yellow-500/10 text-yellow-400 border border-yellow-500/20' :
                    'bg-blue-500/10 text-blue-400 border border-blue-500/20'
                  }`}>
                    {selectedEmployee.priority} Priority
                  </span>
                </div>

                {/* Individual Savings Contribution - Highlighted */}
                {(() => {
                  const savings = (selectedEmployee.baselineCost || 150) - (selectedTrip ? selectedTrip.cost / selectedTrip.employees.length : 100);
                  return (
                    <div className="bg-primary/5 p-4 border border-primary/20 border-t-2 border-t-primary">
                      <div className="flex items-center justify-between">
                        <div className="flex items-center gap-3">
                          <MIcon name="payments" className="text-3xl text-primary" />
                          <div>
                            <div className="flex items-center gap-2">
                              <p className="text-xs text-white/30 font-mono text-xs">Individual Savings Contribution</p>
                              <div className="group relative">
                                <MIcon name="info" className="text-sm text-white/20 hover:text-primary transition-colors cursor-help" />
                                <div className="absolute left-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                                  By riding in this pool, you saved the company this amount compared to baseline individual cost.
                                </div>
                              </div>
                            </div>
                            <p className="text-2xl font-bold text-primary">₹{formatNumber(Math.max(0, savings))}</p>
                          </div>
                        </div>
                      </div>
                    </div>
                  );
                })()}

                {/* Key Stats Row */}
                <div className="grid grid-cols-2 gap-3 mb-4">
                  {/* Circuity Index */}
                  {(() => {
                    const directDist = selectedEmployee.pickupLat && selectedEmployee.pickupLng
                      ? haversineDistance(selectedEmployee.pickupLat, selectedEmployee.pickupLng, 12.9716, 77.5946)
                      : 10;
                    const actualDist = selectedTrip?.distance || directDist;
                    const circuity = (actualDist / directDist).toFixed(2);
                    return (
                      <div className="relative bg-white/[0.02] p-4 flex flex-col border border-white/10">
                        <MIcon name="speed" className="absolute right-3 bottom-3 text-6xl text-primary/5" />
                        <div className="relative z-10 flex flex-col h-full">
                          <div className="flex items-center gap-2">
                            <p className="text-xs text-white/80">Circuity Index</p>
                            <div className="group relative">
                              <MIcon name="info" className="text-sm text-white/20 hover:text-primary transition-colors cursor-help" />
                              <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                                Actual distance / Direct distance. Lower is better.
                              </div>
                            </div>
                          </div>
                          <p className="text-2xl font-bold text-primary mt-auto">{circuity}x</p>
                        </div>
                      </div>
                    );
                  })()}

                  {/* Pickup Slot */}
                  {(() => {
                    const tripEmployees = selectedTrip?.employees || [];
                    const myIndex = tripEmployees.findIndex((e: string | { id: string }) => (typeof e === 'string' ? e : e.id) === selectedEmployee.id);
                    const position = myIndex >= 0 ? myIndex + 1 : 1;
                    const total = tripEmployees.length || 1;
                    return (
                      <div className="relative bg-white/[0.02] p-4 flex flex-col border border-white/10">
                        <MIcon name="target" className="absolute right-3 bottom-3 text-6xl text-primary/5" />
                        <div className="relative z-10 flex flex-col h-full">
                          <div className="flex items-center gap-2">
                            <p className="text-xs text-white/80">Pickup Slot</p>
                            <div className="group relative">
                              <MIcon name="info" className="text-sm text-white/20 hover:text-primary transition-colors cursor-help" />
                              <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                                Your pickup sequence position in this trip route.
                              </div>
                            </div>
                          </div>
                          <p className="text-2xl font-bold text-primary mt-auto">{position}/{total}</p>
                        </div>
                      </div>
                    );
                  })()}

                  {/* Commute Time */}
                  {(() => {
                    let duration = 0;
                    if (selectedAssignment.pickupTime && selectedAssignment.dropoffTime) {
                      const pickup = selectedAssignment.pickupTime.split(':').map(Number);
                      const dropoff = selectedAssignment.dropoffTime.split(':').map(Number);
                      duration = (dropoff[0] * 60 + dropoff[1]) - (pickup[0] * 60 + pickup[1]);
                    }
                    return (
                      <div className="relative bg-white/[0.02] p-4 flex flex-col border border-white/10">
                        <MIcon name="schedule" className="absolute right-3 bottom-3 text-6xl text-primary/5" />
                        <div className="relative z-10 flex flex-col h-full">
                          <div className="flex items-center gap-2">
                            <p className="text-xs text-white/80">Commute Time</p>
                            <div className="group relative">
                              <MIcon name="info" className="text-sm text-white/20 hover:text-primary transition-colors cursor-help" />
                              <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                                Total time from pickup to dropoff at office.
                              </div>
                            </div>
                          </div>
                          <p className="text-2xl font-bold text-primary mt-auto">{duration > 0 ? `${duration}m` : 'N/A'}</p>
                        </div>
                      </div>
                    );
                  })()}

                  {/* Trip Number */}
                  <div className="relative bg-white/[0.02] p-4 flex flex-col border border-white/10">
                    <MIcon name="local_shipping" className="absolute right-3 bottom-3 text-6xl text-primary/5" />
                    <div className="relative z-10 flex flex-col h-full">
                      <div className="flex items-center gap-2">
                        <p className="text-xs text-white/80">Trip Number</p>
                        <div className="group relative">
                          <MIcon name="info" className="text-sm text-white/20 hover:text-primary transition-colors cursor-help" />
                          <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/60 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                            Trip sequence number for vehicle {selectedAssignment.vehicleId}.
                          </div>
                        </div>
                      </div>
                      <p className="text-4xl font-bold text-primary mt-auto">#{selectedAssignment.tripNumber}</p>
                    </div>
                  </div>
                </div>

                {/* Trip & Location Details */}
                <div className="grid grid-cols-1 md:grid-cols-2 gap-3">
                  {/* Journey Info */}
                  <div className="bg-white/[0.02] p-4 border border-white/10 space-y-3">
                    <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 flex items-center gap-2">
                      <MIcon name="navigation" className="text-base" />
                      Journey Details
                    </h4>
                    <div className="space-y-2">
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-xs font-mono text-white/30">Vehicle ID</span>
                        <span className="font-medium text-primary">{selectedAssignment.vehicleId}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-xs font-mono text-white/30">Distance</span>
                        <span className="font-medium">{selectedTrip ? `${selectedTrip.distance.toFixed(1)} km` : 'N/A'}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-xs font-mono text-white/30">Pickup Time</span>
                        <span className="font-medium text-primary">{selectedAssignment.pickupTime}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-xs font-mono text-white/30">Drop-off Time</span>
                        <span className="font-medium text-primary">{selectedAssignment.dropoffTime}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-xs font-mono text-white/30">Mode</span>
                        <span className="font-medium">{selectedVehicle?.mode || 'N/A'}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-xs font-mono text-white/30">Sharing</span>
                        <span className="font-medium">
                          {selectedAssignment.actualSharing === 'Solo' || selectedAssignment.actualSharing === '+0' ? 'Solo Ride' :
                           selectedAssignment.actualSharing === '+1' ? 'Shared (+1)' :
                           selectedAssignment.actualSharing === '+2' ? 'Shared (+2)' :
                           selectedAssignment.actualSharing || 'Solo Ride'}
                        </span>
                      </div>
                    </div>
                  </div>

                  {/* Preference Compliance */}
                  <div className="bg-white/[0.02] p-4 border border-white/10 space-y-3">
                    <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 flex items-center gap-2">
                      <MIcon name="check_circle" className="text-base" />
                      Preference Compliance
                    </h4>
                    <div className="space-y-2">
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-xs font-mono text-white/30">Vehicle Type</span>
                        <span className={`flex items-center gap-1 text-sm font-medium ${selectedAssignment.vehiclePreferenceMet ? 'text-primary' : 'text-white/30'}`}>
                          {selectedAssignment.vehiclePreferenceMet ? <MIcon name="check_circle" className="text-base" /> : <MIcon name="cancel" className="text-base" />}
                          {selectedAssignment.vehiclePreferenceMet ? 'Met' : 'Unmet'}
                        </span>
                      </div>
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-xs font-mono text-white/30">Sharing Pref</span>
                        <span className={`flex items-center gap-1 text-sm font-medium ${selectedAssignment.sharingPreferenceMet ? 'text-primary' : 'text-white/30'}`}>
                          {selectedAssignment.sharingPreferenceMet ? <MIcon name="check_circle" className="text-base" /> : <MIcon name="cancel" className="text-base" />}
                          {selectedAssignment.sharingPreferenceMet ? 'Met' : 'Unmet'}
                        </span>
                      </div>
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-xs font-mono text-white/30">Time Window</span>
                        <span className={`flex items-center gap-1 text-sm font-medium ${selectedAssignment.timeWindowMet ? 'text-primary' : 'text-white/30'}`}>
                          {selectedAssignment.timeWindowMet ? <MIcon name="check_circle" className="text-base" /> : <MIcon name="cancel" className="text-base" />}
                          {selectedAssignment.timeWindowMet ? 'Met' : 'Unmet'}
                        </span>
                      </div>
                    </div>
                    
                    {/* Overall Status */}
                    {(() => {
                      const status = getAssignmentStatus(selectedEmployee.id);
                      const allMet = selectedAssignment.vehiclePreferenceMet && selectedAssignment.sharingPreferenceMet && selectedAssignment.timeWindowMet;
                      return (
                        <div className={`mt-3 pt-3 border-t border-white/10`}>
                          <div className="flex items-center gap-2">
                            {allMet ? (
                              <MIcon name="check_circle" className="text-base text-primary" />
                            ) : (
                              <MIcon name="warning" className="text-base text-yellow-400" />
                            )}
                            <span className={`text-sm font-medium ${allMet ? 'text-primary' : 'text-yellow-400'}`}>
                              {status.status}
                            </span>
                          </div>
                        </div>
                      );
                    })()}
                  </div>
                </div>

              {/* Cost Comparison */}
              <div className="bg-white/[0.02] p-4 border border-white/10">
                <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-3">Cost Analysis</h4>
                <div className="flex items-center gap-4">
                  <div className="flex-1">
                    <p className="text-[10px] font-label font-bold uppercase tracking-widest text-white/40 mb-1">Baseline (Uber Rate)</p>
                    <p className="text-lg font-mono font-bold text-white">₹{formatNumber((selectedEmployee?.baselineCost || 150))}</p>
                  </div>
                  <MIcon name="trending_down" className="text-2xl text-primary" />
                  <div className="flex-1 text-right">
                    <p className="text-[10px] font-label font-bold uppercase tracking-widest text-white/40 mb-1">Allocated Share</p>
                    <p className="text-lg font-mono font-bold text-white">
                      ₹{formatNumber(selectedTrip ? Math.round(selectedTrip.cost / selectedTrip.employees.length) : 0)}
                    </p>
                  </div>
                </div>
              </div>
            </div>
            ) : (
              <div className="flex-1 flex items-center justify-center p-6">
                <div className="text-center">
                  <MIcon name="groups" className="text-6xl text-white/10 mx-auto mb-4" />
                  <p className="text-white/30 font-mono">Select an employee to view details</p>
                  <p className="text-[10px] font-mono text-white/20 mt-1">Click on any employee from the list</p>
                </div>
              </div>
            )}
          </div>
        </motion.div>
      </div>
    </div>
  );
}

