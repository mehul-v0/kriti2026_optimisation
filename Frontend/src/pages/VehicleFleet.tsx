import { Link } from 'react-router-dom';
import { motion, useInView } from 'framer-motion';
import { useState, useRef, useEffect } from 'react';
import { useApp } from '../context/AppContext';
import { Truck, Car as CarIcon, Route as RouteIcon, MapPin, Users, Clock, Gauge, BarChart3, Search, Info, TrendingUp, Activity, ArrowUp } from 'lucide-react';
import { 
  PieChart, Pie, Cell, XAxis, YAxis, CartesianGrid, Tooltip, 
  ResponsiveContainer, ScatterChart, Scatter, ZAxis, Area, AreaChart
} from 'recharts';
import { formatNumber } from '../utils/helpers';

// Animated Chart Component with Scroll Trigger
function AnimatedChart({ children, delay = 0 }: { children: React.ReactNode, delay?: number }) {
  const ref = useRef(null);
  const isInView = useInView(ref, { once: true, margin: "0px" });
  
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

export default function VehicleFleet() {
  const { currentResult } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [showScrollToTop, setShowScrollToTop] = useState(false);
  const routeDetailsRef = useRef<HTMLDivElement>(null);

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark-900 text-white p-6">
        <div className="max-w-7xl mx-auto">
          <div className="text-center py-20">
            <h1 className="text-3xl font-bold mb-4">Vehicle Fleet Analysis</h1>
            <p className="text-gray-400">No optimization results available</p>
            <Link 
              to="/upload" 
              className="inline-block mt-6 px-6 py-3 bg-primary hover:bg-primary/80 rounded-lg transition-colors"
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

  // Custom tooltip components
  const CustomTooltip = ({ active, payload }: any) => {
    if (active && payload && payload.length) {
      const data = payload[0].payload;
      return (
        <div className="bg-dark-800/95 backdrop-blur-xl border border-gray/20 rounded-lg p-3 shadow-xl">
          <p className="text-white font-medium">{data.name}</p>
          <p style={{ color: data.color }} className="font-semibold text-sm">
            {data.value} vehicles ({data.percentage}%)
          </p>
        </div>
      );
    }
    return null;
  };

  return (
    <div className="min-h-screen bg-dark text-white p-3">
      <div className="max-w-7xl mx-auto">
        <h1 className="text-4xl font-bold mb-2">Vehicle Fleet Analysis</h1>
        <p className="text-gray mb-8">Deep dive into vehicle utilization and performance</p>

        {/* Fleet Summary */}
        <motion.div initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 mb-8 shadow-float">
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4 text-center">
            {/* Vehicles Used (a/b format) */}
            <div>
              <CarIcon className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Vehicles Used</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    Number of vehicles actively deployed out of total fleet
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">
                {vehiclesUsed}/{currentResult.vehicles.length}
              </p>
            </div>

            {/* Total Trips */}
            <div>
              <RouteIcon className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Total Trips</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-48 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    Total number of trips completed by all vehicles
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{currentResult.trips.length}</p>
            </div>

            {/* Seat-Kilometer Utility */}
            <div>
              <Users className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">SKU%</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Seat-Kilometer Utility</div>
                    Passenger-km ÷ Capacity-km. Measures if you're carrying passengers or empty air over distance.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{skuPercent.toFixed(1)}%</p>
            </div>

            {/* System Circuity Factor */}
            <div>
              <TrendingUp className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Circuity</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">System Circuity Factor</div>
                    Actual Distance ÷ Direct Distance. Proves route intelligence. 1.0-1.4 is excellent.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{circuityFactor.toFixed(2)}x</p>
            </div>

            {/* Duty Cycle */}
            <div>
              <Gauge className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Duty Cycle</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Fleet Duty Cycle</div>
                    Active Time ÷ Planning Window. Validates if fleet size is optimal.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{dutyCycle.toFixed(1)}%</p>
            </div>

            {/* Average Occupancy */}
            <div>
              <Users className="w-8 h-8 text-primary mx-auto mb-2" />
              <div className="flex items-center justify-center gap-1 mb-1">
                <p className="text-sm text-primary">Avg Occupancy</p>
                <div className="group relative">
                  <Info className="w-3 h-3 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute left-1/2 -translate-x-1/2 top-full mt-2 w-56 p-2 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <div className="font-semibold text-primary mb-1">Average Occupancy Percentage</div>
                    Average passenger occupancy across all trips as a percentage of vehicle capacity.
                  </div>
                </div>
              </div>
              <p className="text-3xl font-bold text-primary">{averageOccupancy.toFixed(1)}%</p>
            </div>
          </div>
        </motion.div>

        {/* Enhanced Fleet Analytics */}
        <div className="flex flex-col xl:flex-row gap-6 mb-8">
          {/* Fuel Type Distribution - Donut Chart */}
          <div className="xl:w-[30%]">
            <AnimatedChart delay={0.1}>
              <div className="flex items-center gap-2 mb-4">
                <Gauge className="w-5 h-5 text-primary-muted" />
                <h3 className="font-bold text-lg">Fuel Types</h3>
                <div className="group relative ml-auto">
                  <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <p className="font-semibold text-primary mb-1">Fuel Types</p>
                    <p>Distribution of vehicles by fuel type (Electric, Petrol, Diesel).</p>
                  </div>
                </div>
              </div>
              <div className="h-48 flex items-center justify-center">
                <ResponsiveContainer width="100%" height="100%">
                  <PieChart>
                    <Pie
                      data={chartData.fuelData}
                      cx="50%"
                      cy="50%"
                      innerRadius={45}
                      outerRadius={75}
                      paddingAngle={2}
                      dataKey="value"
                      animationBegin={0}
                      animationDuration={1000}
                    >
                      {chartData.fuelData.map((entry, index) => (
                        <Cell key={`fuel-cell-${index}`} fill={entry.color} />
                      ))}
                    </Pie>
                    <Tooltip content={<CustomTooltip />} />
                  </PieChart>
                </ResponsiveContainer>
              </div>
              <div className="mt-4 space-y-2">
                {chartData.fuelData.map((item, index) => (
                  <div key={index} className="flex items-center justify-between text-sm">
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 rounded-full" style={{ backgroundColor: item.color }} />
                      <span className="text-gray">{item.name}</span>
                    </div>
                    <span className="font-medium text-white">{item.value} ({item.percentage}%)</span>
                  </div>
                ))}
              </div>
            </AnimatedChart>
          </div>

          {/* Vehicle Type Distribution - Donut Chart */}
          <div className="xl:w-[30%]">
            <AnimatedChart delay={0.2}>
              <div className="flex items-center gap-2 mb-4">
                <CarIcon className="w-5 h-5 text-primary-muted" />
                <h3 className="font-bold text-lg">Vehicle Types</h3>
                <div className="group relative ml-auto">
                  <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                  <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                    <p className="font-semibold text-primary mb-1">Vehicle Types</p>
                    <p>Distribution of vehicle modes (Car, Van, Bus) used in the fleet.</p>
                  </div>
                </div>
              </div>
              <div className="h-48 flex items-center justify-center">
                {chartData.hasAnyTypeData ? (
                  <ResponsiveContainer width="100%" height="100%">
                    <PieChart>
                      <Pie
                        data={chartData.typeDataForChart}
                        cx="50%"
                        cy="50%"
                        innerRadius={45}
                        outerRadius={75}
                        paddingAngle={2}
                        dataKey="value"
                        animationBegin={0}
                        animationDuration={1000}
                      >
                        {chartData.typeDataForChart.map((entry, index) => (
                          <Cell key={`type-cell-${index}`} fill={entry.color} />
                        ))}
                      </Pie>
                      <Tooltip content={<CustomTooltip />} />
                    </PieChart>
                  </ResponsiveContainer>
                ) : (
                  <div className="text-center text-gray">
                    <CarIcon className="w-12 h-12 mx-auto mb-2 opacity-30" />
                    <p className="text-sm">No vehicle type data</p>
                  </div>
                )}
              </div>
              <div className="mt-4 space-y-2">
                {chartData.typeData.map((item, index) => (
                  <div key={index} className="flex items-center justify-between text-sm">
                    <div className="flex items-center gap-2">
                      <div className="w-3 h-3 rounded-full" style={{ backgroundColor: item.color }} />
                      <span className="text-gray">{item.name}</span>
                    </div>
                    <span className="font-medium text-white">{item.value} ({item.percentage}%)</span>
                  </div>
                ))}
              </div>
            </AnimatedChart>
          </div>

          {/* Vehicle Occupancy - Line Chart (40% width) */}
          <div className="xl:w-[40%]">
            <AnimatedChart delay={0.3}>
              <div className="flex flex-col h-full">
                <div className="flex items-center gap-2 mb-4">
                  <BarChart3 className="w-5 h-5 text-primary-muted" />
                  <h3 className="font-bold text-lg">Trip Occupancy Distribution</h3>
                  <div className="group relative ml-auto">
                    <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                    <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                      <p className="font-semibold text-primary mb-1">Trip Occupancy Distribution</p>
                      <p>Shows how efficiently vehicle capacity is utilized across all trips.</p>
                    </div>
                  </div>
                </div>
                {chartData.occupancyData && chartData.occupancyData.length > 0 ? (
                  <div className="w-full h-72">
                    <ResponsiveContainer width="100%" height="100%">
                      <AreaChart data={chartData.occupancyData} margin={{ top: 10, right: 20, left: 10, bottom: 50 }}>
                        <defs>
                          <linearGradient id="occupancyGreenGradient" x1="0" y1="0" x2="0" y2="1">
                            <stop offset="0%" stopColor="#10B981" stopOpacity={0.8}/>
                            <stop offset="50%" stopColor="#10B981" stopOpacity={0.3}/>
                            <stop offset="100%" stopColor="#10B981" stopOpacity={0}/>
                          </linearGradient>
                        </defs>
                        <CartesianGrid strokeDasharray="3 3" stroke="#374151" opacity={0.2} horizontal={true} vertical={false} />
                        <XAxis 
                          dataKey="range"
                          axisLine={false}
                          tickLine={false}
                          tick={{ fill: '#9CA3AF', fontSize: 9 }}
                          interval={0}
                          angle={-45}
                          textAnchor="end"
                          height={60}
                        />
                        <YAxis 
                          axisLine={false}
                          tickLine={false}
                          tick={{ fill: '#9CA3AF', fontSize: 10 }}
                          width={40}
                          allowDecimals={false}
                          label={{ value: 'Trips', angle: -90, position: 'insideLeft', fill: '#9CA3AF', fontSize: 11 }}
                        />
                        <Tooltip 
                          contentStyle={{ 
                            backgroundColor: '#1F2937', 
                            border: '1px solid #374151',
                            borderRadius: '8px',
                            color: '#ffffff'
                          }}
                          itemStyle={{ color: '#10B981', fontWeight: 'bold' }}
                          labelStyle={{ color: '#ffffff' }}
                          formatter={(value: any) => [`${value} trips`, 'Count']}
                          labelFormatter={(label) => `Occupancy: ${label}`}
                        />
                        <Area
                          type="monotone"
                          dataKey="count"
                          stroke="#10B981"
                          strokeWidth={3}
                          fill="url(#occupancyGreenGradient)"
                          dot={{ fill: '#10B981', strokeWidth: 2, r: 5, stroke: '#1F2937' }}
                          activeDot={{ fill: '#10B981', strokeWidth: 3, r: 7, stroke: '#fff' }}
                          animationBegin={0}
                          animationDuration={1500}
                        />
                      </AreaChart>
                    </ResponsiveContainer>
                  </div>
                ) : (
                  <div className="w-full h-72 flex items-center justify-center">
                    <p className="text-gray/60">No occupancy data available</p>
                  </div>
                )}
              </div>
            </AnimatedChart>
          </div>
        </div>

        {/* Advanced Vehicle Analytics - Vehicle DNA, Load Profile, Vehicle Scorecard */}
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
          {/* Vehicle DNA Strip - Distance proportional bars with employee color coding */}
          <AnimatedChart delay={0.4}>
            <div className="flex items-center gap-2 mb-4">
              <Activity className="w-5 h-5 text-primary-muted" />
              <h3 className="font-bold text-lg">Vehicle DNA Strip</h3>
              <div className="group relative ml-auto">
                <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Vehicle DNA Strip</p>
                  <p>Visual representation of vehicle usage. Width represents distance traveled, colors show employee groups picked up.</p>
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
                
                // Color palette for different employee pickups
                const employeeColors = ['#3B82F6', '#10B981', '#F59E0B', '#8B5CF6', '#EC4899', '#06B6D4', '#EF4444', '#14B8A6', '#F97316', '#6366F1'];
                
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
                      
                      // Assign color based on current passenger count
                      const color = currentPassengers > 0 ? employeeColors[(currentPassengers - 1) % employeeColors.length] : '#4a5568';
                      
                      allSegments.push({
                        percent: segmentPercent,
                        color: color,
                        passengers: currentPassengers,
                        distance: segmentDistance
                      });
                    }
                  });
                  
                  return (
                    <div key={idx} className="space-y-1">
                      <div className="flex items-center justify-between text-xs">
                        <span className="text-white font-medium w-20">{item.vehicle.id}</span>
                        <span className="text-gray/60">{item.tripCount} trips • {item.employees} employees</span>
                        <span className="text-primary font-bold">{item.totalDistance.toFixed(1)} km</span>
                      </div>
                      <div className="h-6 rounded-lg overflow-hidden bg-dark-700/50 border border-gray/10 relative">
                        <motion.div 
                          initial={{ width: 0 }}
                          animate={{ width: `${widthPercent}%` }}
                          transition={{ duration: 1, delay: idx * 0.08 }}
                          className="h-full flex relative"
                        >
                          {allSegments.map((segment, sIdx) => (
                            <div 
                              key={sIdx}
                              className="h-full relative group transition-all hover:brightness-110"
                              style={{ 
                                width: `${segment.percent}%`,
                                backgroundColor: segment.color,
                                borderRight: sIdx < allSegments.length - 1 ? '1px solid rgba(0,0,0,0.2)' : 'none'
                              }}
                              title={`${segment.passengers} passenger(s) - ${segment.distance.toFixed(1)} km`}
                            >
                              {segment.percent > 5 && segment.passengers > 0 && (
                                <div className="absolute inset-0 flex items-center justify-center">
                                  <span className="text-[9px] font-bold text-white opacity-80">{segment.passengers}p</span>
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
          </AnimatedChart>

          {/* Efficiency Matrix (Scatter Plot) */}
          <AnimatedChart delay={0.5}>
            <div className="flex items-center gap-2 mb-4">
              <BarChart3 className="w-5 h-5 text-primary-muted" />
              <h3 className="font-bold text-lg">Efficiency Matrix - Vehicle Performance</h3>
              <div className="group relative ml-auto">
                <Info className="w-4 h-4 text-gray/40 hover:text-primary transition-colors cursor-help" />
                <div className="absolute right-0 top-full mt-2 w-64 p-3 bg-dark-700 border border-gray/20 rounded-lg text-xs text-gray/90 opacity-0 invisible group-hover:opacity-100 group-hover:visible transition-all duration-200 z-50 shadow-xl pointer-events-none">
                  <p className="font-semibold text-primary mb-1">Understanding the Matrix</p>
                  <p>Bubble size represents vehicle seating capacity</p>
                  <p className="mt-1">Quadrants help identify best/worst performers</p>
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
                          contentStyle={{ 
                            backgroundColor: '#1F2937', 
                            border: '1px solid #374151',
                            borderRadius: '8px',
                            color: '#ffffff'
                          }}
                          itemStyle={{ color: '#ffffff', fontWeight: 'bold' }}
                          labelStyle={{ color: '#bbe5a9' }}
                          formatter={(value: any, name: string) => {
                            if (name === 'Distance') return [<span key="val" style={{ color: '#ffffff', fontWeight: 'bold' }}>{value.toFixed(1)} km</span>, 'Distance'];
                            if (name === 'Cost Per Passenger') return [<span key="val" style={{ color: '#ffffff', fontWeight: 'bold' }}>₹{Math.round(value)}</span>, 'Cost/Passenger'];
                            if (name === 'Capacity') return [<span key="val" style={{ color: '#ffffff', fontWeight: 'bold' }}>{value} seats</span>, 'Capacity'];
                            return [value, name];
                          }}
                          labelFormatter={(_label, payload) => {
                            if (payload && payload.length > 0) {
                              return `Vehicle: ${payload[0].payload.vehicleId}`;
                            }
                            return '';
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
          </AnimatedChart>
        </div>

        {/* Enhanced Vehicle List */}
        <motion.div 
          initial={{ opacity: 0, y: 20 }} 
          animate={{ opacity: 1, y: 0 }} 
          transition={{ delay: 0.5 }} 
          className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 mb-8 shadow-float"
        >
          <div className="flex items-center justify-between mb-6">
            <h3 className="font-bold text-xl flex items-center gap-3">
              <Truck className="w-6 h-6 text-primary" />
              Vehicle Fleet Summary
            </h3>
            <div className="relative">
              <Search className="w-4 h-4 absolute left-3 top-1/2 transform -translate-y-1/2 text-gray" />
              <input
                type="text"
                placeholder="Search vehicles..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="pl-10 pr-4 py-2 bg-dark-600 border border-gray/20 rounded-lg text-white placeholder-gray text-sm focus:outline-none focus:border-primary/50 transition-colors"
              />
            </div>
          </div>
          <div className="overflow-x-auto scrollbar-bright">
            <div className="flex gap-4 min-w-max pb-2">
              {filteredVehicles.map((vehicle) => {
                const { avgUsed, totalCapacity } = getVehicleCapacityInfo(vehicle.id);
                const isUsed = vehicleTrips.has(vehicle.id);
                const trips = vehicleTrips.get(vehicle.id) || [];
                const totalEmployees = new Set(trips.flatMap(t => t.employees)).size;
                const utilizationPercent = totalCapacity > 0 ? (avgUsed / totalCapacity) * 100 : 0;
                const totalCost = trips.reduce((sum, t) => sum + (t.cost || 0), 0);
                const totalDistance = trips.reduce((sum, t) => sum + (t.distance || 0), 0);
                
                // Get work time range
                let workTimeRange = 'N/A';
                if (trips.length > 0) {
                  const startTimes = trips.map(t => t.startTime).filter(Boolean);
                  const endTimes = trips.map(t => t.endTime).filter(Boolean);
                  if (startTimes.length > 0 && endTimes.length > 0) {
                    const earliestStart = startTimes.sort()[0];
                    const latestEnd = endTimes.sort()[endTimes.length - 1];
                    workTimeRange = `${earliestStart} - ${latestEnd}`;
                  }
                }
                
                return (
                  <div
                    key={vehicle.id}
                    onClick={() => isUsed && setSelectedVehicle(vehicle.id)}
                    className={`bg-[#191919] rounded-xl border border-gray/10 p-4 transition-all hover:border-primary/30 min-w-64 flex-shrink-0 ${
                      isUsed ? 'cursor-pointer hover:bg-[#1a1a1a]' : 'opacity-60'
                    }`}
                  >
                    <div className="flex items-center justify-between mb-3">
                      <span className="font-bold text-white text-lg">{vehicle.id}</span>
                      <div className="flex gap-1">
                        <span className="px-2 py-1 rounded-md text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          ₹{formatNumber(totalCost)}
                        </span>
                        <span className="px-2 py-1 rounded-md text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          {totalDistance.toFixed(1)} km
                        </span>
                      </div>
                    </div>
                    
                    <div className="space-y-3">
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Fuel Type</span>
                        <span className="text-sm font-medium text-white">{vehicle.fuelType}</span>
                      </div>
                      
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Trips</span>
                        <span className="text-sm font-medium text-white">{trips.length}</span>
                      </div>
                      
                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Employees</span>
                        <span className="text-sm font-medium text-white">{totalEmployees}</span>
                      </div>

                      <div className="flex items-center justify-between">
                        <span className="text-sm text-gray">Work Time</span>
                        <span className="text-xs font-medium text-white">
                          {workTimeRange}
                        </span>
                      </div>

                      <div className="mt-3">
                        <div className="flex items-center justify-between text-xs mb-2">
                          <span className="text-gray">Capacity Usage</span>
                          <span className="font-medium text-white">
                            {avgUsed.toFixed(1)} / {totalCapacity}
                          </span>
                        </div>
                        <div className="flex items-center gap-2">
                          <span className="text-xs font-medium text-primary w-10">
                            {Math.round(utilizationPercent)}%
                          </span>
                          <div className="flex-1 flex gap-0.5">
                            {Array.from({ length: 10 }, (_, i) => {
                              const segmentFilled = utilizationPercent >= (i + 1) * 10;
                              return (
                                <div
                                  key={i}
                                  className={`h-2.5 flex-1 rounded-sm transition-colors duration-300 ${
                                    segmentFilled ? 'bg-primary' : 'bg-dark-500/60'
                                  }`}
                                />
                              );
                            })}
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
          {searchQuery && filteredVehicles.length === 0 && (
            <div className="text-center py-8 text-gray">
              No vehicles found matching "{searchQuery}"
            </div>
          )}
        </motion.div>

        {/* Route Details Section */}
        <motion.div ref={routeDetailsRef} initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.3 }}>
          <div className="flex items-center justify-between mb-6">
            <h2 className="text-2xl font-bold flex items-center gap-3">
              <RouteIcon className="w-6 h-6 text-primary" />
              Detailed Route Information
            </h2>
            <div className="flex items-center gap-4 text-xs">
              <div className="flex items-center gap-1.5">
                <Clock className="w-3.5 h-3.5 text-primary-bright" />
                <span className="text-gray">Arrival Time</span>
              </div>
              <div className="flex items-center gap-1.5">
                <Clock className="w-3.5 h-3.5 text-red-400" />
                <span className="text-gray">Departure Time</span>
              </div>
            </div>
          </div>
          
          {/* Vehicle Selector */}
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 mb-6 shadow-float">
            <div className="flex flex-wrap gap-3">
              <button
                onClick={() => setSelectedVehicle(null)}
                className={`px-4 py-2 rounded-lg border transition-all ${
                  !selectedVehicle ? 'bg-primary/20 border-primary/40 text-white' : 'bg-dark-700 border-gray/30 text-gray hover:bg-dark-600'
                }`}
              >
                All Routes ({currentResult.trips.length} trips)
              </button>
              {vehicleIds.map((vehicleId) => {
                return (
                  <button
                    key={vehicleId}
                    onClick={() => setSelectedVehicle(vehicleId)}
                    className={`px-4 py-2 rounded-lg border transition-all flex items-center gap-2 ${
                      selectedVehicle === vehicleId ? 'bg-primary/20 border-primary/40 text-white' : 'bg-dark-700 border-gray/30 text-gray hover:bg-dark-600'
                    }`}
                  >
                    <div className="w-3 h-3 rounded-full bg-primary" />
                    <span className="font-medium">{vehicleId}</span>
                  </button>
                );
              })}
            </div>
          </div>

          {/* Route Details */}
          <div className="space-y-4">
            {selectedTrips.length === 0 ? (
              <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10  p-12 text-center">
                <MapPin className="w-16 h-16 text-gray/30 mx-auto mb-4" />
                <p className="text-gray">No routes available for this selection</p>
              </div>
            ) : (
              selectedTrips.map((trip, tIdx) => {
                return (
                  <motion.div
                    key={`${trip.vehicleId}-${trip.tripNumber}`}
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ delay: tIdx * 0.05 }}
                    className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10  p-6"
                  >
                    <div className="flex items-center gap-3 mb-4">
                      <div className="w-4 h-4 rounded-full bg-primary" />
                      <h3 className="font-bold text-lg">{trip.vehicleId} - Trip {trip.tripNumber}</h3>
                      <div className="ml-auto flex gap-3 text-sm">
                        <span className="px-2 py-1 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          <Users className="w-3 h-3 inline mr-1" />{trip.employees.length} passengers
                        </span>
                        <span className="px-2 py-1 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          {formatNumber(Math.round(trip.distance))} km
                        </span>
                        <span className="px-2 py-1 rounded text-xs font-medium bg-primary/20 text-primary border border-primary/30">
                          Rs. {formatNumber(Math.round(trip.cost))}
                        </span>
                      </div>
                    </div>

                    {/* Route sequence - 2 column layout */}
                    {/* Route Timeline */}
                    <div className="relative pl-8">
                      <p className="text-xs text-gray font-medium uppercase tracking-wider mb-3">Route Timeline</p>
                        {trip.route.map((point, pIdx) => {
                          const isPickup = point.type === 'pickup';
                          const isDropoff = point.type === 'dropoff';
                          const dotColor = isPickup ? 'bg-blue-400 border-blue-400' : isDropoff ? 'bg-red-400 border-red-400' : 'bg-primary border-primary';
                          const textColor = isPickup ? 'text-blue-400' : isDropoff ? 'text-red-400' : 'text-primary';

                          // Use real distance_from_prev from next stop, fallback to haversine
                          let distToNext: number | null = null;
                          if (pIdx < trip.route.length - 1) {
                            const nextPoint = trip.route[pIdx + 1];
                            distToNext = nextPoint.distanceFromPrev > 0
                              ? nextPoint.distanceFromPrev
                              : haversineDistance(point.lat, point.lng, nextPoint.lat, nextPoint.lng);
                          }

                          return (
                            <div key={pIdx} className="relative">
                              {/* Stop row */}
                              <div className="relative pb-2">
                                {/* Dot */}
                                <div className={`absolute left-[-24px] top-1.5 w-3 h-3 rounded-full border-2 ${dotColor}`} />
                                <div className="flex items-center justify-between gap-2">
                                  <div className="flex items-center gap-2 min-w-0">
                                    <span className={`text-xs font-medium uppercase shrink-0 ${textColor}`}>
                                      {point.type === 'dropoff' ? 'dropoff' : point.type}
                                    </span>
                                    <span className="text-sm font-medium text-white truncate">
                                      {point.employeeId || 'Office'}
                                    </span>
                                  </div>
                                  <div className="flex items-center gap-3 text-xs shrink-0">
                                    {point.arrivalTime && (
                                      <div className="flex items-center gap-1 text-gray">
                                        <Clock className="w-3 h-3 text-primary-bright" />
                                        <span className="text-white">{point.arrivalTime}</span>
                                      </div>
                                    )}
                                    {point.departureTime && (
                                      <div className="flex items-center gap-1 text-gray">
                                        <Clock className="w-3 h-3 text-red-400" />
                                        <span className="text-white">{point.departureTime}</span>
                                      </div>
                                    )}
                                    {!point.arrivalTime && !point.departureTime && point.time && (
                                      <div className="flex items-center gap-1 text-gray">
                                        <Clock className="w-3 h-3" />
                                        <span>{point.time}</span>
                                      </div>
                                    )}
                                  </div>
                                </div>
                              </div>

                              {/* Connecting line with distance */}
                              {pIdx < trip.route.length - 1 && (
                                <div className="relative ml-[-20px] pl-[20px] pb-2">
                                  <div className="absolute left-[-20px] top-0 bottom-0 w-0.5 bg-gray/20" />
                                  {distToNext !== null && distToNext > 0 && (
                                    <div className="flex items-center gap-1.5 py-1">
                                      <div className="w-4 border-t border-dashed border-gray/30" />
                                      <span className="text-[10px] text-gray/60 bg-dark-800/80 px-1.5 py-0.5 rounded">
                                        {distToNext < 1 ? `${Math.round(distToNext * 1000)} m` : `${distToNext.toFixed(1)} km`}
                                      </span>
                                    </div>
                                  )}
                                </div>
                              )}
                            </div>
                          );
                        })}
                    </div>
                  </motion.div>
                );
              })
            )}
          </div>
        </motion.div>

        {/* Floating Scroll to Top Button */}
        {showScrollToTop && (
          <motion.button
            initial={{ opacity: 0, scale: 0.8 }}
            animate={{ opacity: 1, scale: 1 }}
            exit={{ opacity: 0, scale: 0.8 }}
            onClick={scrollToRouteTop}
            className="fixed bottom-8 right-8 z-50 p-4 bg-primary hover:bg-primary-bright rounded-full shadow-2xl border border-primary/30 transition-all duration-300 hover:scale-110 group"
            whileHover={{ y: -4 }}
            whileTap={{ scale: 0.95 }}
          >
            <ArrowUp className="w-6 h-6 text-gray-900" />
            <span className="absolute right-full mr-3 top-1/2 -translate-y-1/2 bg-dark-700 text-white text-sm px-3 py-2 rounded-lg opacity-0 group-hover:opacity-100 transition-opacity whitespace-nowrap pointer-events-none">
              Back to Route Info
            </span>
          </motion.button>
        )}
      </div>
    </div>
  );
}

