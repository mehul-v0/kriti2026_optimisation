import { useState, useMemo, useEffect, useRef } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../context/AppContext';
import { MapContainer, TileLayer, Polyline, Marker, Popup, CircleMarker, useMap } from 'react-leaflet';
import { Map as MapIcon, Download, Play, Pause, Focus, Maximize2, Minimize2 } from 'lucide-react';
import { getRouteColors } from '../utils/helpers';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

// Fix for default marker icons in react-leaflet
delete (L.Icon.Default.prototype as any)._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png',
  iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png',
  shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png',
});

// Vehicle depot icon (where vehicle starts - shown only once per vehicle)
const createDepotIcon = (_color: string) => L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 24px;
    height: 24px;
    background-color: #CC6666;
    border: 2px solid white;
    border-radius: 4px;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: bold;
    font-size: 10px;
    color: white;
    text-shadow: 0 0 2px rgba(0,0,0,0.8);
  ">D</div>`,
  iconSize: [24, 24],
  iconAnchor: [12, 12],
});

// Office/building icon
const officeIcon = L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 32px;
    height: 32px;
    background-color: #BFFF00;
    border: 3px solid #1a1a1a;
    border-radius: 4px;
    display: flex;
    align-items: center;
    justify-content: center;
    box-shadow: 0 2px 8px rgba(0,0,0,0.3);
  ">
    <svg width="18" height="18" viewBox="0 0 24 24" fill="#1a273a">
      <path d="M12 3L4 9v12h16V9l-8-6zm0 2.5L18 10v9H6v-9l6-4.5zM8 12h2v2H8v-2zm3 0h2v2h-2v-2zm3 0h2v2h-2v-2zm-6 3h2v2H8v-2zm3 0h2v2h-2v-2zm3 0h2v2h-2v-2z"/>
    </svg>
  </div>`,
  iconSize: [32, 32],
  iconAnchor: [16, 16],
});

// Colored pickup icon per vehicle
const createPickupIcon = (color: string) => L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 12px;
    height: 12px;
    background-color: ${color};
    border: 2px solid white;
    border-radius: 50%;
  "></div>`,
  iconSize: [12, 12],
  iconAnchor: [6, 6],
});

// Component to fit map bounds
function FitBounds({ bounds, mapRef }: { bounds: L.LatLngBoundsExpression; mapRef: React.MutableRefObject<L.Map | null> }) {
  const map = useMap();
  
  useEffect(() => {
    mapRef.current = map;
  }, [map, mapRef]);
  
  useEffect(() => {
    if (bounds) {
      map.fitBounds(bounds, { padding: [50, 50] });
    }
  }, [map, bounds]);
  return null;
}

// Create trip number label icon (simple text without box) with rotation
const createTripLabelIcon = (tripNumber: number, color: string, rotation: number) => L.divIcon({
  className: 'trip-label',
  html: `<div style="
    color: ${color};
    font-size: 11px;
    font-weight: bold;
    white-space: nowrap;
    text-shadow: 1px 1px 2px rgba(0,0,0,0.9), -1px -1px 2px rgba(0,0,0,0.9), 0 0 4px rgba(0,0,0,0.8);
    letter-spacing: 1px;
    transform: rotate(${rotation}deg);
    transform-origin: center center;
  ">Trip ${tripNumber}</div>`,
  iconSize: [50, 16],
  iconAnchor: [25, 8],
});

// Calculate midpoint and angle between two positions
const getMidpointAndAngle = (pos1: [number, number], pos2: [number, number]) => {
  const midLat = (pos1[0] + pos2[0]) / 2;
  const midLng = (pos1[1] + pos2[1]) / 2;
  
  // Calculate angle for CSS rotation (degrees, clockwise from horizontal)
  // On map: lat = y, lng = x, so we use atan2(deltaLat, deltaLng) then convert
  const deltaLng = pos2[1] - pos1[1];
  const deltaLat = pos2[0] - pos1[0];
  
  // atan2 gives angle from positive x-axis (east), counter-clockwise
  // CSS rotation is clockwise from horizontal, so we negate
  let angle = -Math.atan2(deltaLat, deltaLng) * (180 / Math.PI);
  
  // Keep text readable (not upside down)
  if (angle > 90) angle -= 180;
  if (angle < -90) angle += 180;
  
  return { midpoint: [midLat, midLng] as [number, number], angle };
};

// Animated sequence of moving dots along path (like a moving dotted line)
function AnimatedDots({ positions, color, isPlaying }: { positions: [number, number][]; color: string; isPlaying: boolean }) {
  const [progress, setProgress] = useState(0);
  const dotCount = 8; // Number of dots in the sequence
  const dotSpacing = 100 / dotCount; // Spacing between dots as percentage
  
  useEffect(() => {
    if (!isPlaying || positions.length < 2) return;
    
    const interval = setInterval(() => {
      setProgress(prev => (prev + 0.3) % dotSpacing);
    }, 30);
    
    return () => clearInterval(interval);
  }, [isPlaying, positions.length, dotSpacing]);
  
  if (positions.length < 2) return null;
  
  // Calculate total path length for even distribution
  const getPointAtProgress = (prog: number) => {
    const totalSegments = positions.length - 1;
    const progressPerSegment = 100 / totalSegments;
    const currentSegment = Math.floor(prog / progressPerSegment);
    const segmentProgress = (prog % progressPerSegment) / progressPerSegment;
    
    const clampedSegment = Math.min(currentSegment, totalSegments - 1);
    const start = positions[clampedSegment];
    const end = positions[clampedSegment + 1] || start;
    
    return {
      lat: start[0] + (end[0] - start[0]) * segmentProgress,
      lng: start[1] + (end[1] - start[1]) * segmentProgress,
    };
  };
  
  // Generate multiple dots at evenly spaced intervals
  const dots = [];
  for (let i = 0; i < dotCount; i++) {
    const dotProgress = (progress + i * dotSpacing) % 100;
    const point = getPointAtProgress(dotProgress);
    dots.push(
      <CircleMarker
        key={i}
        center={[point.lat, point.lng]}
        radius={4}
        pathOptions={{
          fillColor: color,
          fillOpacity: 0.9,
          color: color,
          weight: 1,
        }}
      />
    );
  }
  
  return <>{dots}</>;
}

export default function RouteExplorer() {
  const { currentResult, geometryStatus } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);
  const [showMotion, setShowMotion] = useState(false);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const mapRef = useRef<L.Map | null>(null);
  const containerRef = useRef<HTMLDivElement | null>(null);

  // Focus map on selected bounds
  const handleFocus = () => {
    if (mapRef.current && selectedBounds) {
      mapRef.current.fitBounds(selectedBounds, { padding: [50, 50] });
    }
  };

  // Toggle fullscreen
  const toggleFullscreen = () => {
    if (!document.fullscreenElement) {
      containerRef.current?.requestFullscreen();
      setIsFullscreen(true);
    } else {
      document.exitFullscreen();
      setIsFullscreen(false);
    }
  };

  // Listen for fullscreen change (e.g., user presses Esc)
  useEffect(() => {
    const handleFullscreenChange = () => {
      setIsFullscreen(!!document.fullscreenElement);
    };
    document.addEventListener('fullscreenchange', handleFullscreenChange);
    return () => document.removeEventListener('fullscreenchange', handleFullscreenChange);
  }, []);

  if (!currentResult) {
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center p-8">
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-12 text-center shadow-float">
          <MapIcon className="w-16 h-16 text-gray/30 mx-auto mb-4" />
          <p className="text-gray mb-6">No optimization results available</p>
          <Link to="/upload" className="btn-primary">Start New Optimization</Link>
        </div>
      </div>
    );
  }

  // Group trips by vehicle
  const vehicleTrips = new Map<string, typeof currentResult.trips>();
  for (const trip of currentResult.trips) {
    if (!vehicleTrips.has(trip.vehicleId)) vehicleTrips.set(trip.vehicleId, []);
    vehicleTrips.get(trip.vehicleId)!.push(trip);
  }

  const vehicleIds = Array.from(vehicleTrips.keys());
  const colors = getRouteColors(vehicleIds.length);
  const selectedTrips = selectedVehicle ? vehicleTrips.get(selectedVehicle) || [] : currentResult.trips;

  // Get office location - check for office, dropoff, or last point in any trip
  const officeLocation = useMemo(() => {
    // First, try to find an office or dropoff point
    for (const trip of currentResult.trips) {
      const officePoint = trip.route.find(p => p.type === 'office' || p.type === 'dropoff');
      if (officePoint) {
        return [officePoint.lat, officePoint.lng] as [number, number];
      }
    }
    
    // If no office/dropoff found, use the last point of the first trip as office
    if (currentResult.trips.length > 0 && currentResult.trips[0].route.length > 0) {
      const lastPoint = currentResult.trips[0].route[currentResult.trips[0].route.length - 1];
      return [lastPoint.lat, lastPoint.lng] as [number, number];
    }
    
    return null;
  }, [currentResult.trips]);

  // Check if geometry might still be loading
  const geometryLoadingStatus = useMemo(() => {
    // Check if any routes have points that should have geometry but don't
    let totalPoints = 0;
    let pointsWithGeometry = 0;
    
    for (const trip of currentResult.trips) {
      for (const point of trip.route) {
        // Only check non-starting points (they should have geometry from previous point)
        if (point.distanceFromPrev && point.distanceFromPrev > 0) {
          totalPoints++;
          if (point.geometry && point.geometry.length > 0) {
            pointsWithGeometry++;
          }
        }
      }
    }
    
    // If we have route points but no geometry at all, geometry might be loading
    // or wasn't fetched (haversine mode)
    const _hasNoGeometry = totalPoints > 0 && pointsWithGeometry === 0;
    const hasPartialGeometry = totalPoints > 0 && pointsWithGeometry > 0 && pointsWithGeometry < totalPoints;
    
    return {
      isLoading: hasPartialGeometry && geometryStatus !== 'complete',  // Override loading when backend reports complete
      hasGeometry: pointsWithGeometry > 0,
      total: totalPoints,
      loaded: pointsWithGeometry,
    };
  }, [currentResult.trips, geometryStatus]);

  // Calculate map bounds
  const mapBounds = useMemo(() => {
    const allPoints: [number, number][] = [];
    
    currentResult.trips.forEach(trip => {
      trip.route.forEach(point => {
        allPoints.push([point.lat, point.lng]);
      });
    });
    
    if (allPoints.length === 0) {
      return L.latLngBounds([12.9, 77.5], [13.1, 77.7]); // Default Bangalore bounds
    }
    
    return L.latLngBounds(allPoints);
  }, [currentResult.trips]);

  // Calculate bounds for selected trips (include vehicle depots and office)
  const selectedBounds = useMemo(() => {
    const allPoints: [number, number][] = [];
    
    // Add trip route points
    selectedTrips.forEach(trip => {
      trip.route.forEach(point => {
        allPoints.push([point.lat, point.lng]);
      });
    });
    
    // Add vehicle depot points
    const selectedVehicleIds = selectedVehicle ? [selectedVehicle] : vehicleIds;
    selectedVehicleIds.forEach(vehicleId => {
      const vehicle = currentResult.vehicles.find(v => v.id === vehicleId);
      if (vehicle) {
        allPoints.push([vehicle.currentLat, vehicle.currentLng]);
      }
    });
    
    // Add office location
    if (officeLocation) {
      allPoints.push(officeLocation);
    }
    
    if (allPoints.length === 0) {
      return mapBounds;
    }
    
    return L.latLngBounds(allPoints);
  }, [selectedTrips, mapBounds, currentResult.vehicles, vehicleIds, selectedVehicle, officeLocation]);

  // const cardClass removed - unused

  return (
    <div ref={containerRef} className="h-screen bg-dark flex flex-col">
      {/* Header */}
      {!isFullscreen && (
        <div className="p-6 border-b border-gray/10">
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-3xl font-bold">
                Map Section
              </h1>
              <p className="text-gray mt-1">
                {currentResult.trips.length} trips across {vehicleIds.length} vehicles
              </p>
            </div>
            <div className="flex gap-3">
              <button className="btn-secondary flex items-center gap-2">
                <Download className="w-4 h-4" />
                Export Routes
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Vehicle Selector Bar */}
      <div className="px-6 py-4 border-b border-gray/10 bg-dark-800/50">
        <div className="flex items-center gap-4 flex-wrap">
          {/* All Vehicles Button */}
          <button
            onClick={() => setSelectedVehicle(null)}
            className={`px-4 py-2 rounded-lg text-sm font-medium transition-all ${
              !selectedVehicle 
                ? 'bg-primary text-dark' 
                : 'bg-dark-700 text-gray hover:bg-dark-600 hover:text-white'
            }`}
          >
            All Vehicles
          </button>
          
          {/* Individual Vehicle Buttons */}
          {vehicleIds.map((vehicleId, idx) => {
            const trips = vehicleTrips.get(vehicleId) || [];
            const color = colors[idx % colors.length];
            const isSelected = selectedVehicle === vehicleId;
            
            return (
              <button
                key={vehicleId}
                onClick={() => setSelectedVehicle(isSelected ? null : vehicleId)}
                className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                  isSelected 
                    ? 'bg-primary text-dark' 
                    : 'bg-dark-700 text-gray hover:bg-dark-600 hover:text-white'
                }`}
              >
                <div 
                  className="w-3 h-3 rounded-full" 
                  style={{ backgroundColor: color }}
                />
                {vehicleId}
                <span className="text-xs opacity-70">({trips.length})</span>
              </button>
            );
          })}
          
          {/* Controls */}
          <div className="ml-auto flex gap-2">
            <button
              onClick={toggleFullscreen}
              className="flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all bg-dark-700 text-gray hover:bg-dark-600 hover:text-white"
            >
              {isFullscreen ? <Minimize2 className="w-4 h-4" /> : <Maximize2 className="w-4 h-4" />}
              {isFullscreen ? 'Exit' : 'Fullscreen'}
            </button>
            <button
              onClick={handleFocus}
              className="flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all bg-dark-700 text-gray hover:bg-dark-600 hover:text-white"
            >
              <Focus className="w-4 h-4" />
              Focus
            </button>
            <button
              onClick={() => setShowMotion(!showMotion)}
              className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                showMotion 
                  ? 'bg-primary text-dark' 
                  : 'bg-dark-700 text-gray hover:bg-dark-600 hover:text-white'
              }`}
            >
              {showMotion ? <Pause className="w-4 h-4" /> : <Play className="w-4 h-4" />}
              {showMotion ? 'Stop Motion' : 'Show Motion'}
            </button>
          </div>
        </div>
      </div>

      {/* Map Container */}
      <div className="flex-1 relative">
        {/* Loading overlay for geometry */}
        {geometryLoadingStatus.isLoading && (
          <div className="absolute inset-0 z-10 flex items-center justify-center bg-dark-900/50 backdrop-blur-sm">
            <div className="bg-dark-800/90 backdrop-blur-xl rounded-2xl border border-gray/10 p-8 text-center shadow-float">
              <div className="w-12 h-12 mx-auto mb-4 border-4 border-primary/20 border-t-primary rounded-full animate-spin" />
              <p className="text-white font-medium">Loading Map Routes...</p>
            </div>
          </div>
        )}
        
        <MapContainer
          center={officeLocation || [12.9716, 77.5946]}
          zoom={12}
          style={{ height: '100%', width: '100%' }}
          className="z-0"
        >
          <TileLayer
            attribution='&copy; <a href="https://stadiamaps.com/">Stadia Maps</a>'
            url="https://tiles.stadiamaps.com/tiles/alidade_smooth_dark/{z}/{x}/{y}{r}.png"
          />
          
          <FitBounds bounds={mapBounds} mapRef={mapRef} />
          
          {/* Route lines or animated dots */}
          {selectedTrips.map((trip) => {
            const vIdx = vehicleIds.indexOf(trip.vehicleId);
            const color = colors[vIdx % colors.length];
            const isSelected = selectedVehicle === trip.vehicleId || !selectedVehicle;
            
            if (!isSelected || trip.route.length < 1) return null;
            
            // Get vehicle to add depot as starting point
            const vehicle = currentResult.vehicles.find(v => v.id === trip.vehicleId);
            
            // Build positions array starting from depot (for first trip) or from route start
            const positions: [number, number][] = [];
            
            // For first trip of vehicle, add depot as starting point
            const vehicleTripsArray = vehicleTrips.get(trip.vehicleId) || [];
            const isFirstTrip = vehicleTripsArray[0]?.tripNumber === trip.tripNumber;
            
            if (isFirstTrip && vehicle) {
              positions.push([vehicle.currentLat, vehicle.currentLng]);
            }
            
            // Add route points with geometry support
            trip.route.forEach((point, idx) => {
              // If this point has geometry (actual road route from previous point), use it
              if (point.geometry && point.geometry.length > 0) {
                // Add all geometry points except the last one (to avoid duplication)
                point.geometry.slice(0, -1).forEach(coord => positions.push(coord));
              } else if (idx > 0) {
                // Log when geometry is missing for non-first points
                console.log(`Route ${trip.vehicleId}-${trip.tripNumber}: Point ${idx} (${point.type}) has NO geometry - using straight line`);
              }
              // Always add the actual point location
              positions.push([point.lat, point.lng]);
            });
            
            if (positions.length < 2) return null;
            
            return (
              <div key={`route-${trip.vehicleId}-${trip.tripNumber}`}>
                {/* Route line (always show, dimmer when motion is on) */}
                <Polyline
                  positions={positions}
                  pathOptions={{
                    color: color,
                    weight: showMotion ? 2 : 4,
                    opacity: showMotion ? 0.3 : 0.8,
                  }}
                />
                
                {/* Animated dots when motion is enabled */}
                {showMotion && (
                  <AnimatedDots 
                    positions={positions} 
                    color={color} 
                    isPlaying={showMotion}
                  />
                )}
                
                {/* Trip number label positioned along the line between two points */}
                {positions.length >= 2 && (() => {
                  // Find the middle segment of the route
                  const midIndex = Math.floor(positions.length / 2);
                  const startIdx = Math.max(0, midIndex - 1);
                  const endIdx = Math.min(positions.length - 1, midIndex);
                  
                  const { midpoint, angle } = getMidpointAndAngle(positions[startIdx], positions[endIdx]);
                  
                  return (
                    <Marker
                      position={midpoint}
                      icon={createTripLabelIcon(trip.tripNumber, color, angle)}
                      interactive={false}
                    />
                  );
                })()}
              </div>
            );
          })}
          
          {/* Pickup markers */}
          {selectedTrips.map((trip) => {
            const vIdx = vehicleIds.indexOf(trip.vehicleId);
            const color = colors[vIdx % colors.length];
            const isSelected = selectedVehicle === trip.vehicleId || !selectedVehicle;
            if (!isSelected) return null;
            
            return trip.route
              .filter(point => point.type === 'pickup')
              .map((point, pIdx) => (
                <Marker
                  key={`pickup-${trip.vehicleId}-${trip.tripNumber}-${pIdx}`}
                  position={[point.lat, point.lng]}
                  icon={createPickupIcon(color)}
                  zIndexOffset={100}
                >
                  <Popup>
                    <div className="text-sm">
                      <strong>{point.employeeId || 'Pickup'}</strong>
                      <br />
                      Time: {point.time}
                      <br />
                      {point.address}
                    </div>
                  </Popup>
                </Marker>
              ));
          })}
          
          {/* Office marker */}
          {officeLocation && (
            <Marker 
              position={officeLocation} 
              icon={officeIcon}
              zIndexOffset={1000}
            >
              <Popup>
                <div className="text-sm">
                  <strong>Office</strong>
                  <br />
                  Drop-off Point
                </div>
              </Popup>
            </Marker>
          )}
          
          {/* Vehicle depot markers - show vehicle's actual starting location */}
          {vehicleIds.map((vehicleId, idx) => {
            const vehicle = currentResult.vehicles.find(v => v.id === vehicleId);
            const trips = vehicleTrips.get(vehicleId) || [];
            const isSelected = selectedVehicle === vehicleId || !selectedVehicle;
            const color = colors[idx % colors.length];
            
            if (!vehicle || trips.length === 0 || !isSelected) return null;
            
            // Use vehicle's actual current location as depot
            return (
              <Marker
                key={`depot-${vehicleId}`}
                position={[vehicle.currentLat, vehicle.currentLng]}
                icon={createDepotIcon(color)}
                zIndexOffset={500}
              >
                <Popup>
                  <div className="text-sm">
                    <strong>{vehicleId} Depot</strong>
                    <br />
                    Location: {vehicle.currentLocation}
                    <br />
                    {trips.length} trip{trips.length !== 1 ? 's' : ''}
                    <br />
                    Total: {trips.reduce((sum, t) => sum + t.distance, 0).toFixed(1)} km
                  </div>
                </Popup>
              </Marker>
            );
          })}
        </MapContainer>
      </div>
    </div>
  );
}

