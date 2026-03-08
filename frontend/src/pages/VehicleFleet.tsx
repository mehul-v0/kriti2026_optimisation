import { Link } from 'react-router-dom';
import { motion } from 'framer-motion';
import { useState, useRef, useEffect } from 'react';
import { useApp } from '../context/AppContext';
import { 
  ResponsiveContainer, ScatterChart, Scatter, ZAxis,
  CartesianGrid, XAxis, YAxis, Tooltip, Cell 
} from 'recharts';
import { formatNumber } from '../utils/helpers';

export default function VehicleFleet() {
  const { currentResult } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [showScrollToTop, setShowScrollToTop] = useState(false);
  const routeDetailsRef = useRef<HTMLDivElement>(null);

  if (!currentResult) {
    return (
      <div className="min-h-screen text-white p-6">
        <div className="max-w-[1400px] mx-auto">
          <div className="text-center py-20">
            <span className="material-symbols-outlined text-white/15 text-6xl block mb-4">local_shipping</span>
            <h1 className="text-3xl font-bold mb-4">Vehicle Fleet Analysis</h1>
            <p className="text-white/30">No optimization results available</p>
            <Link 
              to="/upload" 
              className="inline-block mt-6 px-6 py-3 bg-primary text-background-dark font-bold hover:brightness-110 transition-all"
            >
              Start New Optimization
            </Link>
          </div>
        </div>
      </div>
    );
  }

  const vehiclesUsed = [...new Set(currentResult.trips.map(t => t.vehicleId))].length;

  const fuelDist = {
    Electric: currentResult.vehicles.filter(v => v.fuelType === 'Electric').length,
    Petrol: currentResult.vehicles.filter(v => v.fuelType === 'Petrol').length,
    Diesel: currentResult.vehicles.filter(v => v.fuelType === 'Diesel').length,
  };

  // Group trips by vehicle for route details
  const vehicleTrips = new Map<string, typeof currentResult.trips>();
  for (const trip of currentResult.trips) {
    if (!vehicleTrips.has(trip.vehicleId)) vehicleTrips.set(trip.vehicleId, []);
    vehicleTrips.get(trip.vehicleId)!.push(trip);
  }

  const vehicleIds = Array.from(vehicleTrips.keys());
  const selectedTrips = selectedVehicle ? vehicleTrips.get(selectedVehicle) || [] : currentResult.trips;

  // Scroll to top handler
  const scrollToRouteTop = () => {
    routeDetailsRef.current?.scrollIntoView({ behavior: 'smooth', block: 'start' });
  };

  // Detect scroll position to show/hide scroll-to-top button
  useEffect(() => {
    const handleScroll = () => {
      if (routeDetailsRef.current) {
        const rect = routeDetailsRef.current.getBoundingClientRect();
        const scrolledPast = rect.top < -500; // Show button after scrolling 500px past the section start
        setShowScrollToTop(scrolledPast);
      }
    };

    window.addEventListener('scroll', handleScroll);
    return () => window.removeEventListener('scroll', handleScroll);
  }, []);

  // Haversine distance between two lat/lng points in km
  const haversineDistance = (lat1: number, lng1: number, lat2: number, lng2: number): number => {
    const R = 6371;
    const dLat = ((lat2 - lat1) * Math.PI) / 180;
    const dLng = ((lng2 - lng1) * Math.PI) / 180;
    const a =
      Math.sin(dLat / 2) * Math.sin(dLat / 2) +
      Math.cos((lat1 * Math.PI) / 180) * Math.cos((lat2 * Math.PI) / 180) *
      Math.sin(dLng / 2) * Math.sin(dLng / 2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  };

  // Calculate average capacity utilization per vehicle (moved before chartData)
  const getVehicleCapacityInfo = (vehicleId: string) => {
    const vehicle = currentResult.vehicles.find(v => v.id === vehicleId);
    const trips = vehicleTrips.get(vehicleId) || [];
    if (!vehicle) return { avgUsed: 0, totalCapacity: 0 };
    
    const totalCapacity = vehicle.capacity || 0;
    const avgUsed = trips.length > 0 
      ? trips.reduce((sum, trip) => sum + trip.employees.length, 0) / trips.length
      : 0;
    
    return { avgUsed, totalCapacity };
  };

  // Chart color schemes - vibrant colors for better visualization
  const CHART_COLORS = {
    fuel: ['#3B82F6', '#10B981', '#F59E0B'],  // Blue, Green, Amber
    mode: ['#8B5CF6', '#EC4899', '#06B6D4'],  // Purple, Pink, Cyan
    occupancy: ['#3B82F6', '#8B5CF6', '#10B981', '#F59E0B', '#EF4444', '#EC4899', '#06B6D4', '#14B8A6', '#F97316', '#6366F1'],
  };

  // Enhanced data processing for charts
  const fuelData = Object.entries(fuelDist).map(([type, count], index) => ({
    name: type,
    value: count,
    color: CHART_COLORS.fuel[index % CHART_COLORS.fuel.length],
    percentage: Math.round((count / currentResult.vehicles.length) * 100)
  }));

  // Vehicle type distribution - Always show all 3 types
  const typeDist = currentResult.vehicles.reduce((acc, v) => {
    const vehicleMode = v.mode || 'Unknown';
    // Map mode values to display names
    if (vehicleMode === '2-Wheeler') {
      acc['2 Wheeler'] = (acc['2 Wheeler'] || 0) + 1;
    } else if (vehicleMode === 'Van') {
      acc['Van'] = (acc['Van'] || 0) + 1;
    } else if (vehicleMode === '4-Wheeler') {
      acc['4 Wheeler'] = (acc['4 Wheeler'] || 0) + 1;
    } else {
      // Fallback for unknown modes
      acc['4 Wheeler'] = (acc['4 Wheeler'] || 0) + 1;
    }
    return acc;
  }, {} as Record<string, number>);

  // Ensure all three types are always present
  const allTypes = ['2 Wheeler', '4 Wheeler', 'Van'];
  const typeData = allTypes.map((type, index) => ({
    name: type,
    value: typeDist[type] || 0,
    color: CHART_COLORS.mode[index % CHART_COLORS.mode.length],
    percentage: Math.round(((typeDist[type] || 0) / currentResult.vehicles.length) * 100)
  }));
  
  // For pie chart, we need at least some data - filter out zero values for rendering
  const typeDataForChart = typeData.filter(m => m.value > 0);
  // If all are 0, show a placeholder
  const hasAnyTypeData = typeDataForChart.length > 0;

  // Trip occupancy distribution by percentage ranges
  const occupancyRanges = [
    { range: '0-10%', min: 0, max: 0.1 },
    { range: '10-20%', min: 0.1, max: 0.2 },
    { range: '20-30%', min: 0.2, max: 0.3 },
    { range: '30-40%', min: 0.3, max: 0.4 },
    { range: '40-50%', min: 0.4, max: 0.5 },
    { range: '50-60%', min: 0.5, max: 0.6 },
    { range: '60-70%', min: 0.6, max: 0.7 },
    { range: '70-80%', min: 0.7, max: 0.8 },
    { range: '80-90%', min: 0.8, max: 0.9 },
    { range: '90-100%', min: 0.9, max: 1.0 }
  ];

  const occupancyData = occupancyRanges.map((range, index) => {
    const count = currentResult.trips.filter(trip => {
      const vehicle = currentResult.vehicles.find(v => v.id === trip.vehicleId);
      const capacity = vehicle?.capacity || 1;
      const utilization = trip.employees.length / capacity;
      // Fix: Include lower boundary for first range, exclude for others
      if (index === 0) {
        return utilization >= range.min && utilization <= range.max;
      }
      return utilization > range.min && utilization <= range.max;
    }).length;
    
    return {
      range: range.range,
      count: count,
      color: CHART_COLORS.occupancy[index % CHART_COLORS.occupancy.length]
    };
  });

  const chartData = { fuelData, typeData, typeDataForChart, hasAnyTypeData, occupancyData };

  // Filter vehicles based on search query
  const filteredVehicles = currentResult.vehicles.filter(vehicle => 
    vehicle.id.toLowerCase().includes(searchQuery.toLowerCase())
  );

  // ====== COMPLEX FLEET ANALYTICS ======
  
  // 1. Seat-Kilometer Utility (SKU%)
  const calculateSKU = () => {
    let totalPassengerKm = 0;
    let totalCapacityKm = 0;
    
    currentResult.trips.forEach(trip => {
      const vehicle = currentResult.vehicles.find(v => v.id === trip.vehicleId);
      const capacity = vehicle?.capacity || 1;
      const distance = trip.distance || 0;
      const passengers = trip.employees.length;
      
      totalPassengerKm += passengers * distance;
      totalCapacityKm += capacity * distance;
    });
    
    return totalCapacityKm > 0 ? (totalPassengerKm / totalCapacityKm) * 100 : 0;
  };
  
  // 2. System Circuity Factor (Detour Index)
  const calculateCircuity = () => {
    let totalActualDistance = 0;
    let totalDirectDistance = 0;
    
    currentResult.trips.forEach(trip => {
      totalActualDistance += trip.distance || 0;
      
      // Calculate direct haversine distances for all employees to office
      trip.employees.forEach(empId => {
        const employee = currentResult.employees.find(e => e.id === (typeof empId === 'string' ? empId : empId));
        if (employee && employee.pickupLat && employee.pickupLng) {
          const officeDistance = haversineDistance(
            employee.pickupLat,
            employee.pickupLng,
            77.5946, // Office lat (default Bangalore)
            12.9716  // Office lng
          );
          totalDirectDistance += officeDistance;
        }
      });
    });
    
    return totalDirectDistance > 0 ? totalActualDistance / totalDirectDistance : 1.0;
  };
  
  // 3. Fleet Duty Cycle
  const calculateDutyCycle = () => {
    // Calculate total time vehicles were moving vs planning window
    const planningWindowMinutes = 270; // 6:00-10:30 = 4.5 hours = 270 min
    
    let totalMovingMinutes = 0;
    currentResult.trips.forEach(trip => {
      // Estimate moving time: distance / average speed (30 km/h in city)
      const movingTime = (trip.distance || 0) / 30 * 60; // in minutes
      totalMovingMinutes += movingTime;
    });
    
    const totalPossibleMinutes = vehiclesUsed * planningWindowMinutes;
    return totalPossibleMinutes > 0 ? (totalMovingMinutes / totalPossibleMinutes) * 100 : 0;
  };
  
  const skuPercent = calculateSKU();
  const circuityFactor = calculateCircuity();
  const dutyCycle = calculateDutyCycle();
  
  // 4. Average Occupancy Percentage
  const calculateAverageOccupancy = () => {
    if (currentResult.trips.length === 0) return 0;
    
    let totalOccupancyPercent = 0;
    currentResult.trips.forEach(trip => {
      const vehicle = currentResult.vehicles.find(v => v.id === trip.vehicleId);
      const capacity = vehicle?.capacity || 1;
      const occupancyPercent = (trip.employees.length / capacity) * 100;
      totalOccupancyPercent += occupancyPercent;
    });
    
    return totalOccupancyPercent / currentResult.trips.length;
  };
  
  const averageOccupancy = calculateAverageOccupancy();

  // Fuel doughnut calculations
  const dominantFuel = chartData.fuelData.length > 0
    ? chartData.fuelData.reduce((a, b) => (b.value > a.value ? b : a))
    : null;
  const dominantFuelPct = dominantFuel ? dominantFuel.percentage : 0;
  const doughnutCircumference = 2 * Math.PI * 54;
  const doughnutOffset = doughnutCircumference * (1 - dominantFuelPct / 100);

  // Vehicle type bar max
  const maxTypeCount = Math.max(...chartData.typeData.map(t => t.value), 1);
  const typeBarColors = ['bg-primary', 'bg-primary/70', 'bg-primary/40'];

  // Occupancy bar max
  const maxOccCount = Math.max(...chartData.occupancyData.map(d => d.count), 1);

  return (
    <div className="min-h-screen text-white">
      <div className="max-w-[1400px] mx-auto p-6 md:p-8">

        {/* Metrics Bar */}
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4 mb-8">
          {/* Vehicles Used */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">directions_car</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Vehicles Used</span><br/>
                  Vehicles actively deployed out of total fleet.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Vehicles Used</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{vehiclesUsed}/{currentResult.vehicles.length}</h3>
            </div>
          </motion.div>

          {/* Total Trips */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.05 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">route</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-48 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Total Trips</span><br/>
                  Total number of trips completed by all vehicles.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Total Trips</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{currentResult.trips.length}</h3>
            </div>
          </motion.div>

          {/* SKU% */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.1 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">event_seat</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Seat-Kilometer Utility</span><br/>
                  Passenger-km ÷ Capacity-km. Measures seat utilization over distance.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">SKU%</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{skuPercent.toFixed(1)}%</h3>
            </div>
          </motion.div>

          {/* Circuity */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.15 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">fork_right</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Circuity Factor</span><br/>
                  Actual ÷ Direct Distance. 1.0-1.4 is excellent.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Circuity</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{circuityFactor.toFixed(2)}x</h3>
            </div>
          </motion.div>

          {/* Duty Cycle */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.2 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">speed</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-56 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Duty Cycle</span><br/>
                  Active Time ÷ Planning Window. Validates fleet size.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Duty Cycle</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{dutyCycle.toFixed(1)}%</h3>
            </div>
          </motion.div>

          {/* Avg Occupancy */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.25 }}
            className="bg-panel-dark border border-white/10 p-5 flex flex-col justify-between">
            <div className="flex justify-between items-start mb-4">
              <span className="p-2 bg-primary/10 text-primary material-symbols-outlined">group</span>
              <div className="group relative">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-52 p-2 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <span className="font-semibold text-primary">Average Occupancy</span><br/>
                  Average passenger occupancy across all trips as % of capacity.
                </div>
              </div>
            </div>
            <div>
              <p className="text-[11px] font-label uppercase tracking-widest text-white/30">Avg Occupancy</p>
              <h3 className="text-2xl font-bold font-mono text-white mt-1">{averageOccupancy.toFixed(1)}%</h3>
            </div>
          </motion.div>
        </div>

        {/* Distributions Row */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-8">
          {/* Fuel Type Doughnut */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.1 }}
            className="bg-panel-dark border border-white/10 p-6">
            <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-6">Fuel Type Distribution</h4>
            <div className="flex items-center justify-between">
              <div className="relative size-32">
                <svg className="size-full transform -rotate-90">
                  <circle className="text-white/5" cx="64" cy="64" fill="transparent" r="54" stroke="currentColor" strokeWidth="12" />
                  <circle className="text-primary" cx="64" cy="64" fill="transparent" r="54" stroke="currentColor"
                    strokeDasharray={doughnutCircumference} strokeDashoffset={doughnutOffset} strokeWidth="12"
                    style={{ transition: 'stroke-dashoffset 1s ease-in-out' }} />
                </svg>
                <div className="absolute inset-0 flex flex-col items-center justify-center">
                  <span className="text-xl font-bold text-white">{dominantFuelPct}%</span>
                  <span className="text-[10px] text-white/30 uppercase">{dominantFuel?.name || 'N/A'}</span>
                </div>
              </div>
              <div className="space-y-3">
                {chartData.fuelData.map((f, i) => (
                  <div key={i} className="flex items-center gap-2">
                    <span className={`size-2 rounded-full ${f.name === 'Electric' ? 'bg-primary' : f.name === 'Petrol' ? 'bg-slate-400' : 'bg-slate-600'}`} />
                    <span className="text-xs text-white/50">{f.name} ({f.percentage}%)</span>
                  </div>
                ))}
              </div>
            </div>
          </motion.div>

          {/* Vehicle Type Breakdown - Bar Chart */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.15 }}
            className="bg-panel-dark border border-white/10 p-6">
            <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-6">Vehicle Type Breakdown</h4>
            <div className="space-y-4">
              {chartData.typeData.map((type, idx) => (
                <div key={type.name}>
                  <div className="flex justify-between text-[11px] mb-1.5">
                    <span className="text-white/50">{type.name}</span>
                    <span className="text-white font-bold">{type.value}</span>
                  </div>
                  <div className="w-full bg-white/[0.04] h-2 rounded-full overflow-hidden">
                    <motion.div
                      initial={{ width: 0 }}
                      animate={{ width: `${type.value > 0 ? (type.value / maxTypeCount) * 100 : 0}%` }}
                      transition={{ duration: 0.8, delay: idx * 0.15 }}
                      className={`${typeBarColors[idx] || 'bg-primary/20'} h-full rounded-full`}
                    />
                  </div>
                </div>
              ))}
            </div>
          </motion.div>

          {/* Trip Occupancy Distribution - Vertical Bars */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.2 }}
            className="bg-panel-dark border border-white/10 p-6">
            <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Trip Occupancy Distribution</h4>
            <div className="h-36 flex items-end justify-between gap-1 mt-4">
              {chartData.occupancyData.map((d, idx) => {
                const heightPct = maxOccCount > 0 ? (d.count / maxOccCount) * 100 : 0;
                const opacity = 0.2 + (heightPct / 100) * 0.8;
                return (
                  <motion.div
                    key={idx}
                    initial={{ height: 0 }}
                    animate={{ height: `${Math.max(heightPct, 2)}%` }}
                    transition={{ duration: 0.6, delay: idx * 0.05 }}
                    className="w-full rounded-t-sm relative group cursor-pointer"
                    style={{ backgroundColor: `rgba(255, 184, 0, ${opacity})` }}
                  >
                    <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-1 px-2 py-1 bg-panel-dark border border-white/10 text-[10px] text-white font-bold whitespace-nowrap opacity-0 group-hover:opacity-100 transition-opacity pointer-events-none z-10">
                      {d.range}: {d.count} trips
                    </div>
                  </motion.div>
                );
              })}
            </div>
            <div className="flex justify-between text-[10px] text-white/20 mt-2 font-bold px-1">
              <span>0-10%</span>
              <span>40-50%</span>
              <span>90-100%</span>
            </div>
          </motion.div>
        </div>

        {/* Advanced Vehicle Analytics */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
          {/* Vehicle DNA Strip */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.2 }}
            className="bg-panel-dark border border-white/10 p-6">
            <div className="flex items-center gap-2 mb-4">
              <span className="material-symbols-outlined text-primary">graphic_eq</span>
              <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Vehicle DNA Strip</h4>
              <div className="group relative ml-auto">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Vehicle DNA Strip</p>
                  <p>Width = distance traveled, colors = employee groups picked up.</p>
                </div>
              </div>
            </div>
            <div className="space-y-3">
              {(() => {
                // Calculate total distance for each vehicle
                const vehicleDistances = currentResult.vehicles.map(vehicle => {
                  const trips = vehicleTrips.get(vehicle.id) || [];
                  const totalDistance = trips.reduce((sum, t) => sum + (t.distance || 0), 0);
                  const tripCount = trips.length;
                  const employees = new Set(trips.flatMap(t => t.employees.map((e: string | { id: string }) => typeof e === 'string' ? e : e.id))).size;
                  return { vehicle, totalDistance, tripCount, employees, trips };
                }).filter(v => v.totalDistance > 0).sort((a, b) => b.totalDistance - a.totalDistance);
                
                const maxDistance = Math.max(...vehicleDistances.map(v => v.totalDistance), 1);
                
                // Amber theme palette — more passengers = deeper amber
                const employeeColors = ['#FFB800', '#E6A600', '#CC9300', '#B38100', '#997000', '#805E00', '#664B00', '#4D3800', '#332600', '#1A1300'];
                
                return vehicleDistances.slice(0, 10).map((item, idx) => {
                  const widthPercent = (item.totalDistance / maxDistance) * 100;
                  
                  // Generate segments for each route point to show color changes when employees board
                  const allSegments: any[] = [];
                  
                  item.trips.forEach((trip) => {
                    let currentPassengers = 0;
                    const route = trip.route;
                    
                    for (let i = 0; i < route.length - 1; i++) {
                      const point = route[i];
                      const nextPoint = route[i + 1];
                      
                      // Update passenger count after this stop
                      if (point.type === 'pickup') currentPassengers++;
                      
                      const segmentDistance = nextPoint.distanceFromPrev > 0 
                        ? nextPoint.distanceFromPrev 
                        : haversineDistance(point.lat, point.lng, nextPoint.lat, nextPoint.lng);
                      
                      const segmentPercent = (segmentDistance / item.totalDistance) * 100;
                      
                      // Assign color based on current passenger count — amber gradient
                      const color = currentPassengers > 0 ? employeeColors[(currentPassengers - 1) % employeeColors.length] : 'rgba(255,255,255,0.04)';
                      
                      allSegments.push({
                        percent: segmentPercent,
                        color: color,
                        passengers: currentPassengers,
                        distance: segmentDistance
                      });
                    }
                  });
                  
                  return (
                    <div key={idx} className="space-y-1 relative hover:z-50">
                      <div className="flex items-center justify-between text-xs">
                        <span className="text-white font-medium w-20">{item.vehicle.id}</span>
                        <span className="text-white/30">{item.tripCount} trips • {item.employees} employees</span>
                        <span className="text-primary font-bold">{item.totalDistance.toFixed(1)} km</span>
                      </div>
                      <div className="h-6 bg-white/[0.03] border border-white/5 relative" style={{ overflow: 'visible' }}>
                        <motion.div 
                          initial={{ width: 0 }}
                          animate={{ width: `${widthPercent}%` }}
                          transition={{ duration: 1, delay: idx * 0.08 }}
                          className="h-full flex relative"
                        >
                          {allSegments.map((segment, sIdx) => (
                            <div 
                              key={sIdx}
                              className="h-full relative group/segment transition-all hover:brightness-125 hover:z-10"
                              style={{ 
                                width: `${segment.percent}%`,
                                backgroundColor: segment.color,
                                borderRight: sIdx < allSegments.length - 1 ? '1px solid rgba(0,0,0,0.2)' : 'none'
                              }}
                            >
                              {/* Hover tooltip */}
                              <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 px-2 py-1 bg-panel-dark border border-white/10 text-xs whitespace-nowrap opacity-0 invisible group-hover/segment:opacity-100 group-hover/segment:visible transition-all duration-150 z-50 shadow-xl pointer-events-none">
                                <span className="text-white font-medium">{segment.passengers} passenger{segment.passengers !== 1 ? 's' : ''}</span>
                                <span className="text-white/30 mx-1">•</span>
                                <span className="text-primary">{segment.distance.toFixed(1)} km</span>
                                <div className="absolute top-full left-1/2 -translate-x-1/2 border-4 border-transparent border-t-[rgb(25,45,38)]"></div>
                              </div>
                              {segment.percent > 5 && segment.passengers > 0 && (
                                <div className="absolute inset-0 flex items-center justify-center">
                                  <span className="text-[9px] font-bold text-black">{segment.passengers}p</span>
                                </div>
                              )}
                            </div>
                          ))}
                        </motion.div>
                      </div>
                    </div>
                  );
                });
              })()}
            </div>
          </motion.div>

          {/* Efficiency Matrix (Scatter Plot) */}
          <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4, delay: 0.25 }}
            className="bg-panel-dark border border-white/10 p-6">
            <div className="flex items-center gap-2 mb-4">
              <span className="material-symbols-outlined text-primary">scatter_plot</span>
              <h4 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40">Efficiency Matrix</h4>
              <div className="group relative ml-auto">
                <span className="material-symbols-outlined text-white/20 text-sm cursor-help hover:text-primary transition-colors">info</span>
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-panel-dark border border-white/10 text-xs text-white/50 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Understanding the Matrix</p>
                  <p>Bubble size = capacity. Quadrants identify best/worst performers.</p>
                </div>
              </div>
            </div>
            {(() => {
              // Prepare scatter plot data
              const scatterData = currentResult.vehicles
                .filter(vehicle => {
                  const trips = vehicleTrips.get(vehicle.id) || [];
                  return trips.length > 0;
                })
                .map(vehicle => {
                  const trips = vehicleTrips.get(vehicle.id) || [];
                  const totalDistance = trips.reduce((sum, t) => sum + (t.distance || 0), 0);
                  const totalCost = trips.reduce((sum, t) => sum + (t.cost || 0), 0);
                  const totalPassengers = trips.reduce((sum, t) => sum + t.employees.length, 0);
                  const costPerPassenger = totalPassengers > 0 ? totalCost / totalPassengers : 0;
                  const capacity = vehicle.capacity || 10;
                  
                  return {
                    vehicleId: vehicle.id,
                    distance: totalDistance,
                    costPerPassenger: costPerPassenger,
                    capacity: capacity,
                    mode: vehicle.mode,
                    trips: trips.length
                  };
                });
              
              // Determine quadrant colors
              const avgDistance = scatterData.reduce((sum, v) => sum + v.distance, 0) / scatterData.length;
              const avgCost = scatterData.reduce((sum, v) => sum + v.costPerPassenger, 0) / scatterData.length;
              
              return (
                <div>
                  <div className="h-80 w-full">
                    <ResponsiveContainer width="100%" height="100%">
                      <ScatterChart margin={{ top: 5, right: 10, bottom: 10, left: 10 }}>
                        <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.3} />
                        <XAxis 
                          type="number" 
                          dataKey="distance" 
                          name="Distance" 
                          unit=" km"
                          axisLine={false}
                          tickLine={false}
                          tick={{ fill: '#9CA3AF', fontSize: 10 }}
                          label={{ value: 'Total Distance Traveled (km)', position: 'bottom', offset: 0, fill: '#9CA3AF', fontSize: 11 }}
                        />
                        <YAxis 
                          type="number" 
                          dataKey="costPerPassenger" 
                          name="Cost Per Passenger" 
                          unit=" ₹"
                          axisLine={false}
                          tickLine={false}
                          tick={{ fill: '#9CA3AF', fontSize: 10 }}
                          label={{ value: 'Cost per Passenger (₹)', angle: -90, position: 'left', offset: 0, fill: '#9CA3AF', fontSize: 11 }}
                        />
                        <ZAxis type="number" dataKey="capacity" range={[50, 400]} name="Capacity" />
                        <Tooltip 
                          cursor={{ strokeDasharray: '3 3' }}
                          content={({ active, payload }) => {
                            if (active && payload && payload.length > 0) {
                              const data = payload[0].payload;
                              // Determine performance rating
                              let rating = '';
                              let ratingColor = '';
                              if (data.distance > avgDistance && data.costPerPassenger < avgCost) {
                                rating = 'Efficiency Winner';
                                ratingColor = '#10B981';
                              } else if (data.distance > avgDistance && data.costPerPassenger > avgCost) {
                                rating = 'High Mileage - High Cost';
                                ratingColor = '#F59E0B';
                              } else if (data.distance < avgDistance && data.costPerPassenger < avgCost) {
                                rating = 'Low Utilization';
                                ratingColor = '#3B82F6';
                              } else {
                                rating = 'Needs Optimization';
                                ratingColor = '#EF4444';
                              }
                              return (
                                <div className="bg-panel-dark border border-white/10 p-3 shadow-2xl min-w-[200px]">
                                  <div className="flex items-center justify-between mb-2 pb-2 border-b border-white/5">
                                    <span className="text-white font-bold text-sm">{data.vehicleId}</span>
                                    <span className="text-xs px-2 py-0.5 rounded-full" style={{ backgroundColor: `${ratingColor}20`, color: ratingColor }}>{rating}</span>
                                  </div>
                                  <div className="space-y-1.5 text-xs">
                                    <div className="flex justify-between">
                                      <span className="text-white/30">Distance</span>
                                      <span className="text-white font-medium">{data.distance.toFixed(1)} km</span>
                                    </div>
                                    <div className="flex justify-between">
                                      <span className="text-white/30">Cost/Passenger</span>
                                      <span className="text-white font-medium">₹{Math.round(data.costPerPassenger)}</span>
                                    </div>
                                    <div className="flex justify-between">
                                      <span className="text-white/30">Capacity</span>
                                      <span className="text-white font-medium">{data.capacity} seats</span>
                                    </div>
                                    <div className="flex justify-between">
                                      <span className="text-white/30">Trips</span>
                                      <span className="text-white font-medium">{data.trips}</span>
                                    </div>
                                    <div className="flex justify-between">
                                      <span className="text-white/30">Type</span>
                                      <span className="text-white font-medium">{data.mode || 'N/A'}</span>
                                    </div>
                                  </div>
                                </div>
                              );
                            }
                            return null;
                          }}
                        />
                        <Scatter 
                          name="Vehicles" 
                          data={scatterData} 
                          fill="#8B5CF6"
                          animationDuration={1500}
                          animationBegin={0}
                        >
                          {scatterData.map((entry, index) => {
                            // Color based on quadrant
                            let fillColor = '#8B5CF6'; // default purple
                            if (entry.distance > avgDistance && entry.costPerPassenger < avgCost) {
                              fillColor = '#10B981'; // emerald - efficiency winner
                            } else if (entry.distance > avgDistance && entry.costPerPassenger > avgCost) {
                              fillColor = '#F59E0B'; // amber - trade-off
                            } else if (entry.distance < avgDistance && entry.costPerPassenger < avgCost) {
                              fillColor = '#3B82F6'; // blue - low usage
                            } else {
                              fillColor = '#EF4444'; // red - inefficient
                            }
                            return <Cell key={`cell-${index}`} fill={fillColor} />;
                          })}
                        </Scatter>
                      </ScatterChart>
                    </ResponsiveContainer>
                  </div>
                </div>
              );
            })()}
          </motion.div>
        </div>

        {/* === Fleet Status + Vehicle Detail Split === */}
        <div ref={routeDetailsRef} className="grid grid-cols-1 xl:grid-cols-12 gap-6 items-start">
          {/* Left: Fleet Status List */}
          <div className="xl:col-span-4 space-y-4 xl:sticky xl:top-4 xl:max-h-[calc(100vh-6rem)] xl:overflow-y-auto" style={{ scrollbarWidth: 'thin', scrollbarColor: 'rgba(255,255,255,0.1) transparent' }}>
            <div className="flex items-center justify-between mb-2">
              <h2 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 flex items-center gap-2">
                Fleet Status
                <span className="bg-primary/10 text-primary text-[9px] font-mono px-2 py-0.5 border border-primary/20">{currentResult.vehicles.length} Total</span>
              </h2>
            </div>
            {/* Search */}
            <div className="relative">
              <span className="material-symbols-outlined absolute left-3 top-1/2 -translate-y-1/2 text-white/30 text-lg">search</span>
              <input
                type="text"
                placeholder="Search vehicles..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full bg-white/[0.02] border border-white/10 py-2 pl-10 pr-4 text-sm font-mono focus:border-primary focus:outline-none text-white/70 placeholder-white/20"
              />
            </div>
            {/* Vehicle Cards */}
            <div className="space-y-3 max-h-[700px] overflow-y-auto pr-1" style={{ scrollbarWidth: 'thin', scrollbarColor: 'rgba(255,255,255,0.1) transparent' }}>
              {filteredVehicles.map((vehicle) => {
                const isUsed = vehicleTrips.has(vehicle.id);
                const trips = vehicleTrips.get(vehicle.id) || [];
                const totalDistance = trips.reduce((sum, t) => sum + (t.distance || 0), 0);
                const _totalEmployees = new Set(trips.flatMap(t => t.employees)).size;
                const { avgUsed, totalCapacity } = getVehicleCapacityInfo(vehicle.id);
                const utilizationPercent = totalCapacity > 0 ? (avgUsed / totalCapacity) * 100 : 0;
                const isSelected = selectedVehicle === vehicle.id;

                return (
                  <div
                    key={vehicle.id}
                    onClick={() => isUsed && setSelectedVehicle(vehicle.id)}
                    className={`bg-panel-dark p-4 border transition-all ${
                      isSelected
                        ? 'border-primary/40 bg-primary/5'
                        : 'border-white/10 hover:border-primary/30'
                    } ${isUsed ? 'cursor-pointer' : 'opacity-60'} relative overflow-hidden group`}
                  >
                    {isSelected && (
                      <div className="absolute top-0 right-0 p-2 opacity-0 group-hover:opacity-100 transition-opacity">
                        <span className="material-symbols-outlined text-primary text-sm">open_in_new</span>
                      </div>
                    )}
                    <div className="flex items-center gap-4 mb-3">
                      <div className="size-10 border border-white/10 bg-white/[0.03] flex items-center justify-center">
                        <span className={`material-symbols-outlined ${isUsed ? 'text-primary' : 'text-white/30'}`}>
                          {vehicle.fuelType === 'Electric' ? 'electric_car' : vehicle.mode === 'Van' ? 'airport_shuttle' : vehicle.mode === '2-Wheeler' ? 'two_wheeler' : 'directions_car'}
                        </span>
                      </div>
                      <div>
                        <h5 className="text-sm font-bold text-white leading-tight">{vehicle.id}</h5>
                        <div className="flex items-center gap-2 mt-1">
                          <span className={`size-2 rounded-full ${isUsed ? 'bg-primary animate-pulse' : 'bg-slate-500'}`} />
                          <span className={`text-[10px] font-bold uppercase tracking-wider ${isUsed ? 'text-primary' : 'text-white/30'}`}>
                            {isUsed ? `Active • ${trips.length} trips` : 'Idle'}
                          </span>
                        </div>
                      </div>
                    </div>
                    <div className="grid grid-cols-4 gap-2 pt-3 border-t border-white/5">
                      <div>
                        <p className="text-[9px] text-white/20 uppercase font-bold">Fuel</p>
                        <p className="text-xs font-bold text-white">{vehicle.fuelType.slice(0, 4)}</p>
                      </div>
                      <div>
                        <p className="text-[9px] text-white/20 uppercase font-bold">Trips</p>
                        <p className="text-xs font-bold text-white">{trips.length}</p>
                      </div>
                      <div>
                        <p className="text-[9px] text-white/20 uppercase font-bold">Dist</p>
                        <p className="text-xs font-bold text-white">{totalDistance.toFixed(0)}km</p>
                      </div>
                      <div>
                        <p className="text-[9px] text-white/20 uppercase font-bold">Util</p>
                        <p className="text-xs font-bold text-white">{Math.round(utilizationPercent)}%</p>
                      </div>
                    </div>
                  </div>
                );
              })}
              {searchQuery && filteredVehicles.length === 0 && (
                <div className="text-center py-8 text-white/30 text-sm">
                  No vehicles found matching &ldquo;{searchQuery}&rdquo;
                </div>
              )}
            </div>
          </div>

          {/* Right: Vehicle Detail + Trip Timeline */}
          <div className="xl:col-span-8">
            <div className="bg-panel-dark border border-white/10 overflow-hidden">
              {/* Header */}
              <div className="p-6 border-b border-white/5 flex flex-col md:flex-row md:items-center justify-between gap-4">
                <div className="flex items-center gap-4">
                  <h3 className="text-sm font-label font-bold uppercase tracking-wider text-white/60">
                    Vehicle Detail: {selectedVehicle || 'All Vehicles'}
                  </h3>
                  <span className="bg-primary/10 text-primary text-[9px] font-mono px-2 py-0.5 border border-primary/20 uppercase tracking-wider">
                    {selectedTrips.length} Trips
                  </span>
                </div>
                {/* Vehicle selector pills */}
                <div className="flex items-center gap-2 flex-wrap">
                  <button
                    onClick={() => setSelectedVehicle(null)}
                    className={`px-3 py-1.5 text-xs font-bold transition-all ${
                      !selectedVehicle
                        ? 'bg-primary/20 text-primary border border-primary/30'
                        : 'text-white/30 hover:text-white hover:bg-white/5'
                    }`}
                  >
                    All
                  </button>
                  {vehicleIds.map((vid) => (
                    <button
                      key={vid}
                      onClick={() => setSelectedVehicle(vid)}
                      className={`px-3 py-1.5 text-xs font-bold transition-all ${
                        selectedVehicle === vid
                          ? 'bg-primary/20 text-primary border border-primary/30'
                          : 'text-white/30 hover:text-white hover:bg-white/5'
                      }`}
                    >
                      {vid}
                    </button>
                  ))}
                </div>
              </div>

              {/* Trip Timeline */}
              <div className="p-8">
                {selectedTrips.length === 0 ? (
                  <div className="text-center py-12">
                    <span className="material-symbols-outlined text-white/15 text-5xl block mb-4">route</span>
                    <p className="text-white/30">No routes available for this selection</p>
                  </div>
                ) : (
                  <div className="relative border-l border-white/10 ml-4 pl-10 space-y-12 pb-4">
                    {selectedTrips.map((trip, tIdx) => (
                      <motion.div
                        key={`${trip.vehicleId}-${trip.tripNumber}`}
                        className="relative"
                        initial={{ opacity: 0, x: -10 }}
                        animate={{ opacity: 1, x: 0 }}
                        transition={{ delay: tIdx * 0.05 }}
                      >
                        {/* Number circle */}
                        <div className="absolute -left-[60px] top-0 size-10 bg-panel-dark border-2 border-primary/30 flex items-center justify-center">
                          <span className="text-xs font-bold text-primary">{String(tIdx + 1).padStart(2, '0')}</span>
                        </div>

                        {/* Trip header */}
                        <div className="flex flex-col md:flex-row md:items-center justify-between gap-4 mb-4">
                          <div>
                            <h4 className="text-base font-bold text-white">{trip.vehicleId} - Trip {trip.tripNumber}</h4>
                            <p className="text-xs text-white/30">
                              {trip.startTime && trip.endTime
                                ? `${trip.startTime} - ${trip.endTime}`
                                : 'Schedule pending'}
                              {trip.duration ? ` (${Math.round(trip.duration)} min)` : ''}
                            </p>
                          </div>
                          <div className="flex items-center gap-3">
                            <div className="text-right">
                              <p className="text-[10px] text-white/20 font-bold uppercase">Distance</p>
                              <p className="text-sm font-bold text-white">{trip.distance.toFixed(1)} km</p>
                            </div>
                            <div className="w-px h-6 bg-white/5" />
                            <div className="text-right">
                              <p className="text-[10px] text-white/20 font-bold uppercase">Cost</p>
                              <p className="text-sm font-bold text-white">₹{formatNumber(Math.round(trip.cost))}</p>
                            </div>
                            <div className="w-px h-6 bg-white/5" />
                            <div className="flex items-center gap-1">
                              <span className="material-symbols-outlined text-primary text-sm">group</span>
                              <span className="text-sm font-bold text-white">{trip.employees.length}</span>
                            </div>
                          </div>
                        </div>

                        {/* Route points */}
                        <div className="bg-white/[0.02] border border-white/5 p-4">
                          <div className="space-y-2">
                            {trip.route.map((point, pIdx) => {
                              const isPickup = point.type === 'pickup';
                              const isDropoff = point.type === 'dropoff';

                              let distToNext: number | null = null;
                              if (pIdx < trip.route.length - 1) {
                                const nextPoint = trip.route[pIdx + 1];
                                distToNext = nextPoint.distanceFromPrev > 0
                                  ? nextPoint.distanceFromPrev
                                  : haversineDistance(point.lat, point.lng, nextPoint.lat, nextPoint.lng);
                              }

                              return (
                                <div key={pIdx}>
                                  <div className="flex items-center gap-3">
                                    <span className={`material-symbols-outlined text-sm ${
                                      isPickup ? 'text-primary' : isDropoff ? 'text-red-400' : 'text-blue-400'
                                    }`}>
                                      {isPickup ? 'location_on' : isDropoff ? 'flag' : 'business'}
                                    </span>
                                    <div className="flex-1 flex items-center justify-between min-w-0">
                                      <div className="flex items-center gap-2 min-w-0">
                                        <span className={`text-[10px] font-bold uppercase w-14 shrink-0 ${
                                          isPickup ? 'text-primary' : isDropoff ? 'text-red-400' : 'text-blue-400'
                                        }`}>{point.type}</span>
                                        <span className="text-sm text-white/60 truncate">{point.employeeId || point.address || 'Office'}</span>
                                      </div>
                                      <div className="flex items-center gap-2 text-[10px] text-white/20 shrink-0">
                                        {point.arrivalTime && <span>{point.arrivalTime}</span>}
                                        {point.departureTime && <span className="text-white/15">→ {point.departureTime}</span>}
                                        {!point.arrivalTime && !point.departureTime && point.time && <span>{point.time}</span>}
                                      </div>
                                    </div>
                                  </div>
                                  {distToNext !== null && distToNext > 0 && pIdx < trip.route.length - 1 && (
                                    <div className="ml-8 my-1">
                                      <span className="text-[10px] text-white/15 bg-white/[0.03] px-1.5 py-0.5 border border-white/5">
                                        {distToNext < 1 ? `${Math.round(distToNext * 1000)} m` : `${distToNext.toFixed(1)} km`}
                                      </span>
                                    </div>
                                  )}
                                </div>
                              );
                            })}
                          </div>
                        </div>
                      </motion.div>
                    ))}
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>

        {/* Floating Scroll to Top Button */}
        {showScrollToTop && (
          <motion.button
            initial={{ opacity: 0, scale: 0.8 }}
            animate={{ opacity: 1, scale: 1 }}
            exit={{ opacity: 0, scale: 0.8 }}
            onClick={scrollToRouteTop}
            className="fixed bottom-8 right-8 z-50 p-4 bg-primary rounded-full shadow-2xl border border-primary/30 transition-all duration-300 hover:scale-110 hover:brightness-110 group"
            whileHover={{ y: -4 }}
            whileTap={{ scale: 0.95 }}
          >
            <span className="material-symbols-outlined text-background-dark text-xl">arrow_upward</span>
            <span className="absolute right-full mr-3 top-1/2 -translate-y-1/2 bg-panel-dark border border-white/10 text-white text-sm px-3 py-2 opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap pointer-events-none">
              Back to Route Info
            </span>
          </motion.button>
        )}
      </div>
    </div>
  );
}

