import { Link } from 'react-router-dom';
import { motion, useInView } from 'framer-motion';
import { useApp } from '../context/AppContext';
import {
  Users, Search, CheckCircle, XCircle, Clock, 
  DollarSign, AlertTriangle, UserCheck, Star,
  ChevronRight, Navigation, Info, TrendingUp, Activity,
  Gauge, Target, Truck
} from 'lucide-react';
import { useState, useMemo, useRef } from 'react';
import { formatNumber } from '../utils/helpers';
import { 
  BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, 
  ResponsiveContainer, Cell, AreaChart, Area
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
      className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6"
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
      <div className="min-h-screen bg-dark p-6">
        <div className="max-w-7xl mx-auto">
          <div className="text-center py-20">
            <h1 className="text-3xl font-bold mb-4">Employee Assignments</h1>
            <p className="text-gray">No optimization results available</p>
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
          const pickupMins = pickup[0] * 60 + pickup[1];
          const dropoffMins = dropoff[0] * 60 + dropoff[1];
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
        const duration = (dropoff[0] * 60 + dropoff[1]) - (pickup[0] * 60 + pickup[1]);
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

  const cardClass = "bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-float hover:shadow-float-lg transition-all duration-300 ";

  return (
    <div className="min-h-screen bg-dark p-6 lg:p-8">
      <div className="max-w-7xl mx-auto space-y-6">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
          className="flex items-center justify-between"
        >
          <div>
            <h1 className="text-3xl lg:text-4xl font-bold bg-gradient-to-r from-white to-gray-400 bg-clip-text text-transparent">
              Employee Assignments
            </h1>
            <p className="text-gray mt-1">Complete traceability for every employee journey</p>
          </div>
        </motion.div>

        {/* Fleet-Style Summary Section */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 mb-2">
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-6 text-center">
            {/* Employees Assigned */}
            <div>
              <UserCheck className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Assigned</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    Employees assigned to vehicles out of total employees
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">
                {metrics.assignedCount}/{metrics.totalEmployees}
              </p>
            </div>

            {/* Commute Efficiency Ratio */}
            <div>
              <Activity className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">CER</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Commute Efficiency Ratio</div>
                    Direct drive time ÷ Actual time in vehicle. 1.0 = taxi-style direct ride.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{metrics.commuteEfficiencyRatio.toFixed(2)}</p>
            </div>

            {/* 95th Percentile Commute */}
            <div>
              <Clock className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">P95 Commute</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">95th Percentile Max Commute</div>
                    95% of staff commute in under this time. Shows worst reasonable case.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{metrics.p95Commute}m</p>
            </div>

            {/* Detour Consumption */}
            <div>
              <TrendingUp className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Detour Used</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Detour Tolerance Consumption</div>
                    % of allowable detour time used. Lower = better quality of life.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{metrics.detourConsumption.toFixed(0)}%</p>
            </div>

            {/* Priority Score */}
            <div>
              <Star className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Priority Score</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Priority Fulfillment Score</div>
                    Score 0-100 based on high priority employee constraint satisfaction.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{metrics.priorityScore}</p>
            </div>

            {/* Time Compliance */}
            <div>
              <Activity className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Time Compliance</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Time Window Compliance</div>
                    % of employees arriving within their preferred time window.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{metrics.timeWindowRate.toFixed(0)}%</p>
            </div>
          </div>
        </motion.div>

        {/* Arrival Wave Chart + Distribution Stats */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-2">
          {/* Arrival Wave Histogram */}
          <AnimatedChart delay={0.1}>
            <div className="lg:col-span-2">
              <div className="flex items-center gap-2 mb-4">
                <Activity className="w-5 h-5 text-primary-muted" />
                <h3 className="font-bold text-lg">The Arrival Wave</h3>
                <div className="group relative ml-auto">
                  <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <p className="font-semibold text-primary mb-1">The Arrival Wave</p>
                    <p>Employee arrivals in 5-minute buckets - helps Facility Management plan elevator traffic.</p>
                  </div>
                </div>
              </div>
              <div className="h-52">
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={metrics.arrivalWaveData} margin={{ top: 5, right: 5, left: 5, bottom: 5 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.2} vertical={false} />
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
                        backgroundColor: '#1F2937', 
                        border: '1px solid #374151',
                        borderRadius: '8px',
                        fontSize: '11px'
                      }}
                      itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                      labelStyle={{ color: '#ffffff' }}
                      formatter={(value) => [`${value} employees`, 'Arrivals']}
                    />
                    <Bar 
                      dataKey="count" 
                      fill="#3B82F6" 
                      radius={[4, 4, 0, 0]}
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
              <Star className="w-5 h-5 text-primary-muted" />
              <h3 className="font-bold text-lg">Priority Distribution</h3>
              <div className="group relative ml-auto">
                <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Priority Distribution</p>
                  <p>Distribution of employees by the level (High, Medium, Low).</p>
                </div>
              </div>
            </div>
            <div className='h-52'>
              <ResponsiveContainer width="100%" height="100%">
                <BarChart 
                  data={[
                    { name: 'High', value: metrics.priorityDist.High, fill: '#b91c1c' },
                    { name: 'Medium', value: metrics.priorityDist.Medium, fill: '#a16207' },
                    { name: 'Low', value: metrics.priorityDist.Low, fill: '#1e40af' }
                  ]} 
                  layout="vertical"
                  margin={{ top: 5, right: 5, left: 5, bottom: 0 }}
                >
                  <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.2} horizontal={false} />
                  <XAxis type="number" axisLine={false} tickLine={false} tick={{ fill: '#9CA3AF', fontSize: 10 }} />
                  <YAxis dataKey="name" type="category" axisLine={false} tickLine={false} tick={{ fill: '#9CA3AF', fontSize: 11 }} width={45} />
                  <Tooltip 
                    contentStyle={{ backgroundColor: '#1F2937', border: '1px solid #374151', borderRadius: '8px', fontSize: '11px' }}
                    itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                    labelStyle={{ color: '#ffffff' }}
                    formatter={(value, _name, props) => {
                      const fillColor = props.payload.fill;
                      return [`${value} employees`, <span key="label" style={{ color: fillColor }}>Count</span>];
                    }}
                  />
                  <Bar dataKey="value" radius={[0, 4, 4, 0]} animationDuration={1500} animationBegin={0}>
                    {[
                      { name: 'High', fill: '#b91c1c' },
                      { name: 'Medium', fill: '#a16207' },
                      { name: 'Low', fill: '#1e40af' }
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
              <Users className="w-5 h-5 text-primary-muted" />
              <h3 className="font-bold text-lg">Ride Sharing</h3>
              <div className="group relative ml-auto">
                <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Ride Sharing</p>
                  <p>Distribution of solo rides vs. shared rides (+1 or +2 passengers).</p>
                </div>
              </div>
            </div>
            <div className="h-52">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart 
                  data={[
                    { name: 'Solo', value: metrics.sharingCounts.Single, fill: '#8B5CF6' },
                    { name: 'Shared +1', value: metrics.sharingCounts.Double, fill: '#10B981' },
                    { name: 'Shared +2', value: metrics.sharingCounts.Triple, fill: '#F59E0B' }
                  ]} 
                  layout="vertical"
                  margin={{ top: 5, right: 5, left: 5, bottom: 0 }}
                >
                  <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.2} horizontal={false} />
                  <XAxis type="number" axisLine={false} tickLine={false} tick={{ fill: '#9CA3AF', fontSize: 10 }} />
                  <YAxis dataKey="name" type="category" axisLine={false} tickLine={false} tick={{ fill: '#9CA3AF', fontSize: 11 }} width={55} />
                  <Tooltip 
                    contentStyle={{ backgroundColor: '#1F2937', border: '1px solid #374151', borderRadius: '8px', fontSize: '11px' }}
                    itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                    labelStyle={{ color: '#ffffff' }}
                    formatter={(value) => [`${value} rides`, 'Count']}
                  />
                  <Bar dataKey="value" radius={[0, 4, 4, 0]} animationDuration={1500} animationBegin={0}>
                    {['#8B5CF6', '#10B981', '#F59E0B'].map((color, index) => (
                      <Cell key={`cell-${index}`} fill={color} />
                    ))}
                  </Bar>
                </BarChart>
              </ResponsiveContainer>
            </div>
          </AnimatedChart>
        </div>

        {/* Advanced Analytics Row - Pain Distribution and Arrival Gap */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-6">
          {/* Pain Distribution Histogram */}
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6">
            <div className="flex items-center gap-2 mb-4">
              <Clock className="w-5 h-5 text-primary-muted" />
              <h3 className="font-bold text-lg">Pain Distribution</h3>
              <div className="group relative ml-auto">
                <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Pain Distribution</p>
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
                          <stop offset="0%" stopColor="#3B82F6" stopOpacity={0.8}/>
                          <stop offset="50%" stopColor="#3B82F6" stopOpacity={0.3}/>
                          <stop offset="100%" stopColor="#3B82F6" stopOpacity={0}/>
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.2} />
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
                        contentStyle={{ backgroundColor: '#1F2937', border: '1px solid #374151', borderRadius: '8px', fontSize: '10px' }}
                        itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                        labelStyle={{ color: '#ffffff' }}
                        formatter={(value, name) => [`${value} employees`, `${name === 'count' ? 'Delay' : name}`]}
                        labelFormatter={(label) => `${label} min`}
                      />
                      <Area
                        type="monotone" 
                        dataKey="count" 
                        stroke="#3B82F6" 
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
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6">
            <div className="flex items-center gap-2 mb-4">
              <TrendingUp className="w-5 h-5 text-primary-muted" />
              <h3 className="font-bold text-lg">Arrival Gap Analysis</h3>
              <div className="group relative ml-auto">
                <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Arrival Gap Analysis</p>
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
                          <stop offset="0%" stopColor="#3B82F6" stopOpacity={0.8}/>
                          <stop offset="50%" stopColor="#3B82F6" stopOpacity={0.3}/>
                          <stop offset="100%" stopColor="#3B82F6" stopOpacity={0}/>
                        </linearGradient>
                      </defs>
                      <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.2} />
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
                        contentStyle={{ backgroundColor: '#1F2937', border: '1px solid #374151', borderRadius: '8px', fontSize: '10px' }}
                        itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                        labelStyle={{ color: '#ffffff' }}
                        formatter={(value, name) => [`${value} employees`, `${name === 'count' ? 'Gap' : name}`]}
                        labelFormatter={(label) => `${label} min`}
                      />
                      <Area
                        type="monotone" 
                        dataKey="count" 
                        stroke="#3B82F6" 
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
          <div className={`${cardClass} p-4 lg:col-span-1 flex flex-col max-h-[800px]`}>
            <div className="flex items-center justify-between mb-4">
              <h3 className="font-bold text-lg flex items-center gap-2">
                <Users className="w-5 h-5 text-primary" />
                Employees
              </h3>
              <span className="text-xs text-gray px-2 py-1 bg-dark-700 rounded-lg">{filteredEmployees.length} found</span>
            </div>

            {/* Search and Filter */}
            <div className="space-y-2 mb-4">
              <div className="relative">
                <Search className="w-4 h-4 text-gray absolute left-3 top-1/2 -translate-y-1/2" />
                <input
                  type="text"
                  placeholder="Search by ID or location..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="w-full pl-9 pr-3 py-2 bg-dark-700 border border-gray/20 rounded-lg text-sm text-white placeholder-gray focus:outline-none focus:border-primary/50"
                />
              </div>
              <select
                value={priorityFilter}
                onChange={(e) => setPriorityFilter(e.target.value)}
                className="w-full px-3 py-2 bg-dark-700 border border-gray/20 rounded-lg text-sm text-white focus:outline-none focus:border-primary/50"
              >
                <option value="All">All Priorities</option>
                <option value="High">High Priority</option>
                <option value="Medium">Medium Priority</option>
                <option value="Low">Low Priority</option>
              </select>
            </div>

            {/* Employee List */}
            <div className="flex-1 overflow-y-auto space-y-1.5 scrollbar-thin pr-1">
              {filteredEmployees.map((emp) => {
                const isSelected = selectedEmployeeId === emp.id;
                
                return (
                  <div
                    key={emp.id}
                    onClick={() => setSelectedEmployeeId(emp.id)}
                    className={`p-3 rounded-lg cursor-pointer transition-all ${
                      isSelected 
                        ? 'bg-primary/20 border border-primary/40' 
                        : 'bg-dark-700/40 border border-transparent hover:bg-dark-600/50 hover:border-gray/20'
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-2 min-w-0">
                        <div className={`w-2 h-2 rounded-full flex-shrink-0 ${
                          emp.priority === 'High' ? 'bg-red-400' : 
                          emp.priority === 'Medium' ? 'bg-yellow-400' : 'bg-blue-400'
                        }`} />
                        <div className="min-w-0">
                          <p className="font-medium text-sm truncate">{emp.id}</p>
                          <p className="text-xs text-gray/60 truncate">{emp.pickupLocation.substring(0, 25)}...</p>
                        </div>
                      </div>
                      <ChevronRight className={`w-4 h-4 flex-shrink-0 ${isSelected ? 'text-primary' : 'text-gray/40'}`} />
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Right Panel - Employee Details (Redesigned) */}
          <div className={`${cardClass} p-6 lg:col-span-2 flex flex-col`}>
            {selectedEmployee && selectedAssignment ? (
              <div className="space-y-4">
                {/* Header */}
                <div className="flex items-center justify-between pb-4 border-b border-gray/10">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 rounded-xl bg-primary/20 flex items-center justify-center">
                      <Users className="w-6 h-6 text-primary" />
                    </div>
                    <div>
                      <h3 className="text-xl font-bold">{selectedEmployee.id}</h3>
                      <p className="text-sm text-gray truncate max-w-[300px]">{selectedEmployee.pickupLocation}</p>
                    </div>
                  </div>
                  <span className={`px-3 py-1 rounded-full text-xs font-semibold ${
                    selectedEmployee.priority === 'High' ? 'bg-red-500/20 text-red-400 border border-red-500/30' :
                    selectedEmployee.priority === 'Medium' ? 'bg-yellow-500/20 text-yellow-400 border border-yellow-500/30' :
                    'bg-blue-500/20 text-blue-400 border border-blue-500/30'
                  }`}>
                    {selectedEmployee.priority} Priority
                  </span>
                </div>

                {/* Individual Savings Contribution - Highlighted */}
                {(() => {
                  const savings = (selectedEmployee.baselineCost || 150) - (selectedTrip ? selectedTrip.cost / selectedTrip.employees.length : 100);
                  return (
                    <div className="bg-gradient-to-r from-primary/20 to-primary/5 rounded-xl p-4 border border-primary/30">
                      <div className="flex items-center justify-between">
                        <div className="flex items-center gap-3">
                          <DollarSign className="w-8 h-8 text-primary" />
                          <div>
                            <div className="flex items-center gap-2">
                              <p className="text-xs text-gray">Individual Savings Contribution</p>
                              <div className="group relative">
                                <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                                <div className="absolute left-0 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
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
                      <div className="relative bg-dark-700/50 rounded-xl p-4 flex flex-col border border-gray/5">
                        <Gauge className="absolute right-3 bottom-3 w-16 h-16 text-primary/5" />
                        <div className="relative z-10 flex flex-col h-full">
                          <div className="flex items-center gap-2">
                            <p className="text-xs text-white/80">Circuity Index</p>
                            <div className="group relative">
                              <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                              <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
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
                      <div className="relative bg-dark-700/50 rounded-xl p-4 flex flex-col border border-gray/5">
                        <Target className="absolute right-3 bottom-3 w-16 h-16 text-primary/5" />
                        <div className="relative z-10 flex flex-col h-full">
                          <div className="flex items-center gap-2">
                            <p className="text-xs text-white/80">Pickup Slot</p>
                            <div className="group relative">
                              <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                              <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
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
                      <div className="relative bg-dark-700/50 rounded-xl p-4 flex flex-col border border-gray/5">
                        <Clock className="absolute right-3 bottom-3 w-16 h-16 text-primary/5" />
                        <div className="relative z-10 flex flex-col h-full">
                          <div className="flex items-center gap-2">
                            <p className="text-xs text-white/80">Commute Time</p>
                            <div className="group relative">
                              <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                              <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
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
                  <div className="relative bg-dark-700/50 rounded-xl p-4 flex flex-col border border-gray/5">
                    <Truck className="absolute right-3 bottom-3 w-16 h-16 text-primary/5" />
                    <div className="relative z-10 flex flex-col h-full">
                      <div className="flex items-center gap-2">
                        <p className="text-xs text-white/80">Trip Number</p>
                        <div className="group relative">
                          <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                          <div className="absolute left-0 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
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
                  <div className="bg-dark-700/40 rounded-xl p-4 space-y-3">
                    <h4 className="text-sm font-semibold text-primary flex items-center gap-2">
                      <Navigation className="w-4 h-4" />
                      Journey Details
                    </h4>
                    <div className="space-y-2">
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-gray">Vehicle ID</span>
                        <span className="font-medium text-primary">{selectedAssignment.vehicleId}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-gray">Distance</span>
                        <span className="font-medium">{selectedTrip ? `${selectedTrip.distance.toFixed(1)} km` : 'N/A'}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-gray">Pickup Time</span>
                        <span className="font-medium text-primary">{selectedAssignment.pickupTime}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-gray">Drop-off Time</span>
                        <span className="font-medium text-primary">{selectedAssignment.dropoffTime}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-gray">Mode</span>
                        <span className="font-medium">{selectedVehicle?.mode || 'N/A'}</span>
                      </div>
                      <div className="flex items-center justify-between text-sm">
                        <span className="text-gray">Sharing</span>
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
                  <div className="bg-dark-700/40 rounded-xl p-4 space-y-3">
                    <h4 className="text-sm font-semibold text-primary flex items-center gap-2">
                      <CheckCircle className="w-4 h-4" />
                      Preference Compliance
                    </h4>
                    <div className="space-y-2">
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Vehicle Type</span>
                        <span className={`flex items-center gap-1 text-sm font-medium ${selectedAssignment.vehiclePreferenceMet ? 'text-primary' : 'text-gray/50'}`}>
                          {selectedAssignment.vehiclePreferenceMet ? <CheckCircle className="w-4 h-4" /> : <XCircle className="w-4 h-4" />}
                          {selectedAssignment.vehiclePreferenceMet ? 'Met' : 'Unmet'}
                        </span>
                      </div>
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Sharing Pref</span>
                        <span className={`flex items-center gap-1 text-sm font-medium ${selectedAssignment.sharingPreferenceMet ? 'text-primary' : 'text-gray/50'}`}>
                          {selectedAssignment.sharingPreferenceMet ? <CheckCircle className="w-4 h-4" /> : <XCircle className="w-4 h-4" />}
                          {selectedAssignment.sharingPreferenceMet ? 'Met' : 'Unmet'}
                        </span>
                      </div>
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Time Window</span>
                        <span className={`flex items-center gap-1 text-sm font-medium ${selectedAssignment.timeWindowMet ? 'text-primary' : 'text-gray/50'}`}>
                          {selectedAssignment.timeWindowMet ? <CheckCircle className="w-4 h-4" /> : <XCircle className="w-4 h-4" />}
                          {selectedAssignment.timeWindowMet ? 'Met' : 'Unmet'}
                        </span>
                      </div>
                    </div>
                    
                    {/* Overall Status */}
                    {(() => {
                      const status = getAssignmentStatus(selectedEmployee.id);
                      const allMet = selectedAssignment.vehiclePreferenceMet && selectedAssignment.sharingPreferenceMet && selectedAssignment.timeWindowMet;
                      return (
                        <div className={`mt-3 pt-3 border-t border-gray/10`}>
                          <div className="flex items-center gap-2">
                            {allMet ? (
                              <CheckCircle className="w-4 h-4 text-primary" />
                            ) : (
                              <AlertTriangle className="w-4 h-4 text-yellow-400" />
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
              <div className="bg-dark-700/40 rounded-xl p-4">
                <h4 className="text-sm font-semibold text-primary mb-3">Cost Analysis</h4>
                <div className="flex items-center gap-4">
                  <div className="flex-1">
                    <p className="text-xs text-primary mb-1">Baseline (Uber Rate)</p>
                    <p className="text-lg font-bold text-primary">₹{formatNumber((selectedEmployee?.baselineCost || 150))}</p>
                  </div>
                  <TrendingUp className="w-6 h-6 text-primary rotate-180" />
                  <div className="flex-1 text-right">
                    <p className="text-xs text-primary mb-1">Allocated Share</p>
                    <p className="text-lg font-bold text-primary">
                      ₹{formatNumber(selectedTrip ? Math.round(selectedTrip.cost / selectedTrip.employees.length) : 0)}
                    </p>
                  </div>
                </div>
              </div>
            </div>
            ) : (
              <div className="flex-1 flex items-center justify-center">
                <div className="text-center">
                  <Users className="w-16 h-16 text-gray/20 mx-auto mb-4" />
                  <p className="text-gray">Select an employee to view details</p>
                  <p className="text-xs text-gray/50 mt-1">Click on any employee from the list</p>
                </div>
              </div>
            )}
          </div>
        </motion.div>
      </div>
    </div>
  );
}

