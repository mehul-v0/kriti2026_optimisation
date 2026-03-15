import { useState, useMemo, useEffect, useRef } from 'react';
import { Link } from 'react-router-dom';
import { useApp } from '../context/AppContext';
import { MapContainer, TileLayer, Polyline, Marker, Popup, useMap } from 'react-leaflet';
import { getRouteColors } from '../utils/helpers';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

/* ── icon helper ── */
const Icon = ({ name, className = '' }: { name: string; className?: string }) => (
  <span className={`material-symbols-outlined ${className}`}>{name}</span>
);

// Fix for default marker icons in react-leaflet
delete (L.Icon.Default.prototype as any)._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon-2x.png',
  iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-icon.png',
  shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/images/marker-shadow.png',
});

// Vehicle depot icon
const createDepotIcon = (_color: string) => L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 18px; height: 18px;
    background: #0D1117;
    border: 1.5px solid #FFB800;
    border-radius: 4px;
    display: flex; align-items: center; justify-content: center;
    font-weight: 700; font-size: 8px; color: #FFB800;
  ">D</div>`,
  iconSize: [18, 18],
  iconAnchor: [9, 9],
});

// Office/building icon
const officeIcon = L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 36px; height: 36px;
    background: #FFB800;
    border: 3px solid rgba(10,21,17,0.8);
    border-radius: 10px;
    display: flex; align-items: center; justify-content: center;
  ">
    <span class="material-symbols-outlined" style="font-size:20px;color:#05070A;">corporate_fare</span>
  </div>`,
  iconSize: [36, 36],
  iconAnchor: [18, 18],
});

// Pickup icon — uniform yellow for all vehicles
const pickupIcon = L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 14px; height: 14px;
    background-color: #FFB800;
    border: 2px solid rgba(255,255,255,0.9);
    border-radius: 50%;
  "></div>`,
  iconSize: [14, 14],
  iconAnchor: [7, 7],
});

// Component to fit map bounds
function FitBounds({ bounds, mapRef }: { bounds: L.LatLngBoundsExpression; mapRef: React.MutableRefObject<L.Map | null> }) {
  const map = useMap();
  useEffect(() => { mapRef.current = map; }, [map, mapRef]);
  useEffect(() => { if (bounds) map.fitBounds(bounds, { padding: [50, 50] }); }, [map, bounds]);
  return null;
}

// Trip number label icon
const createTripLabelIcon = (tripNumber: number, color: string, rotation: number) => L.divIcon({
  className: 'trip-label',
  html: `<div style="
    color: ${color}; font-size: 11px; font-weight: bold; white-space: nowrap;
    text-shadow: 1px 1px 2px rgba(0,0,0,0.9), -1px -1px 2px rgba(0,0,0,0.9), 0 0 4px rgba(0,0,0,0.8);
    letter-spacing: 1px; transform: rotate(${rotation}deg); transform-origin: center center;
  ">Trip ${tripNumber}</div>`,
  iconSize: [50, 16],
  iconAnchor: [25, 8],
});

// Calculate midpoint and angle
const getMidpointAndAngle = (pos1: [number, number], pos2: [number, number]) => {
  const midLat = (pos1[0] + pos2[0]) / 2;
  const midLng = (pos1[1] + pos2[1]) / 2;
  const deltaLng = pos2[1] - pos1[1];
  const deltaLat = pos2[0] - pos1[0];
  let angle = -Math.atan2(deltaLat, deltaLng) * (180 / Math.PI);
  if (angle > 90) angle -= 180;
  if (angle < -90) angle += 180;
  return { midpoint: [midLat, midLng] as [number, number], angle };
};

/* ── Parse "HH:MM" to minutes since midnight ── */
const parseTimeToMinutes = (t: string): number => {
  if (!t || t === '--') return 0;
  const parts = t.split(':');
  if (parts.length < 2) return 0;
  const h = parseInt(parts[0], 10);
  const m = parseInt(parts[1], 10);
  return (isNaN(h) ? 0 : h) * 60 + (isNaN(m) ? 0 : m);
};

/* ── format minutes → "HH:MM" ── */
const minutesToHHMM = (m: number): string => {
  const h = Math.floor(m / 60) % 24;
  const mm = Math.round(m % 60);
  return `${String(h).padStart(2, '0')}:${String(mm).padStart(2, '0')}`;
};

/* ── format minutes → "Xh Ym" ── */
const fmtDuration = (mins: number) => {
  const h = Math.floor(mins / 60);
  const m = Math.round(mins % 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
};

/* ── Car marker icon for timeline ── */
const createCarIcon = () => L.divIcon({
  className: 'custom-marker',
  html: `<div style="
    width: 18px; height: 18px;
    background: #FFB800;
    border: 2px solid #05070A;
    border-radius: 50%;
    display: flex; align-items: center; justify-content: center;
  ">
    <span class="material-symbols-outlined" style="font-size:10px;color:#05070A;">directions_car</span>
  </div>`,
  iconSize: [18, 18],
  iconAnchor: [9, 9],
});

const carIcon = createCarIcon();

/* ── Build timeline segments per vehicle ── */
interface TimelineSegment {
  type: 'travel' | 'rest';
  startMin: number;
  endMin: number;
  tripNumber: number;
  positions: [number, number][]; // only for travel
}

function buildVehicleTimeline(
  _vehicleId: string,
  trips: { tripNumber: number; vehicleId: string; employees: string[]; route: { type: string; lat: number; lng: number; time: string; arrivalTime: string; departureTime: string; geometry?: [number, number][]; distanceFromPrev: number; address: string; employeeId?: string }[]; distance: number; duration: number; cost: number; startTime: string; endTime: string }[],
  vehicle: { currentLat: number; currentLng: number; availabilityTime: string } | undefined
): TimelineSegment[] {
  if (!trips.length) return [];
  const segments: TimelineSegment[] = [];

  trips.forEach((trip, tIdx) => {
    const tripStartMin = parseTimeToMinutes(trip.startTime);
    const tripEndMin = parseTimeToMinutes(trip.endTime);

    // Rest segment before this trip
    if (tIdx === 0 && vehicle) {
      // No rest before first trip - vehicle starts at its scheduled time
    } else if (tIdx > 0) {
      const prevEnd = parseTimeToMinutes(trips[tIdx - 1].endTime);
      if (tripStartMin > prevEnd) {
        segments.push({ type: 'rest', startMin: prevEnd, endMin: tripStartMin, tripNumber: trip.tripNumber, positions: [] });
      }
    }

    // Travel segment
    const positions: [number, number][] = [];
    const isFirstTrip = tIdx === 0;
    if (isFirstTrip && vehicle) positions.push([vehicle.currentLat, vehicle.currentLng]);

    trip.route.forEach((point) => {
      if (point.geometry && point.geometry.length > 0) {
        point.geometry.slice(0, -1).forEach(coord => positions.push(coord));
      }
      positions.push([point.lat, point.lng]);
    });

    segments.push({ type: 'travel', startMin: tripStartMin, endMin: tripEndMin, tripNumber: trip.tripNumber, positions });
  });

  return segments;
}

/* ── Get vehicle position at a given minute on its timeline ── */
function getVehiclePositionAtTime(
  segments: TimelineSegment[],
  currentMin: number,
  vehicle: { currentLat: number; currentLng: number } | undefined
): { lat: number; lng: number; active: boolean; tripNumber: number } | null {
  if (!segments.length) return null;

  const firstSeg = segments[0];
  // Before any trip starts
  if (currentMin < firstSeg.startMin) return null;

  // After all trips end
  const lastSeg = segments[segments.length - 1];
  if (currentMin > lastSeg.endMin) {
    // Park at last position
    if (lastSeg.type === 'travel' && lastSeg.positions.length > 0) {
      const p = lastSeg.positions[lastSeg.positions.length - 1];
      return { lat: p[0], lng: p[1], active: false, tripNumber: lastSeg.tripNumber };
    }
    return null;
  }

  for (const seg of segments) {
    if (currentMin >= seg.startMin && currentMin <= seg.endMin) {
      if (seg.type === 'rest') {
        // Parked at last known position (depot or office)
        const prevTravel = segments.filter(s => s.type === 'travel' && s.endMin <= seg.startMin);
        if (prevTravel.length > 0) {
          const lastTravel = prevTravel[prevTravel.length - 1];
          if (lastTravel.positions.length > 0) {
            const p = lastTravel.positions[lastTravel.positions.length - 1];
            return { lat: p[0], lng: p[1], active: false, tripNumber: seg.tripNumber };
          }
        }
        if (vehicle) return { lat: vehicle.currentLat, lng: vehicle.currentLng, active: false, tripNumber: seg.tripNumber };
        return null;
      }

      // Travel segment — interpolate along positions
      if (seg.positions.length < 2) {
        if (seg.positions.length === 1) return { lat: seg.positions[0][0], lng: seg.positions[0][1], active: true, tripNumber: seg.tripNumber };
        return null;
      }

      const duration = seg.endMin - seg.startMin;
      if (duration <= 0) return { lat: seg.positions[0][0], lng: seg.positions[0][1], active: true, tripNumber: seg.tripNumber };

      const fraction = Math.min(1, Math.max(0, (currentMin - seg.startMin) / duration));

      // Interpolate along polyline
      let totalDist = 0;
      const segDists: number[] = [];
      for (let i = 1; i < seg.positions.length; i++) {
        const dx = seg.positions[i][0] - seg.positions[i - 1][0];
        const dy = seg.positions[i][1] - seg.positions[i - 1][1];
        const d = Math.sqrt(dx * dx + dy * dy);
        segDists.push(d);
        totalDist += d;
      }

      if (totalDist === 0) return { lat: seg.positions[0][0], lng: seg.positions[0][1], active: true, tripNumber: seg.tripNumber };

      const targetDist = fraction * totalDist;
      let accumulated = 0;
      for (let i = 0; i < segDists.length; i++) {
        if (accumulated + segDists[i] >= targetDist) {
          const segFraction = segDists[i] > 0 ? (targetDist - accumulated) / segDists[i] : 0;
          const lat = seg.positions[i][0] + (seg.positions[i + 1][0] - seg.positions[i][0]) * segFraction;
          const lng = seg.positions[i][1] + (seg.positions[i + 1][1] - seg.positions[i][1]) * segFraction;
          return { lat, lng, active: true, tripNumber: seg.tripNumber };
        }
        accumulated += segDists[i];
      }
      const last = seg.positions[seg.positions.length - 1];
      return { lat: last[0], lng: last[1], active: true, tripNumber: seg.tripNumber };
    }
  }

  return null;
}

/* ── Map component: Animated vehicle markers driven by timeline ── */
function TimelineVehicleMarkers({ vehicleTimelines, currentTimeMin, vehicles, vehicleIds }: {
  vehicleTimelines: Map<string, TimelineSegment[]>;
  currentTimeMin: number;
  vehicles: { id: string; currentLat: number; currentLng: number }[];
  vehicleIds: string[];
}) {
  const markers: React.JSX.Element[] = [];
  for (const vid of vehicleIds) {
    const segs = vehicleTimelines.get(vid);
    if (!segs) continue;
    const veh = vehicles.find(v => v.id === vid);
    const pos = getVehiclePositionAtTime(segs, currentTimeMin, veh);
    if (pos) {
      markers.push(
        <Marker key={`car-${vid}`} position={[pos.lat, pos.lng]} icon={carIcon} zIndexOffset={2000}>
          <Popup><div className="text-sm"><strong>{vid}</strong><br />{pos.active ? `Trip #${pos.tripNumber}` : 'Resting'}<br />{minutesToHHMM(currentTimeMin)}</div></Popup>
        </Marker>
      );
    }
  }
  return <>{markers}</>;
}

/* ══════════════════════ MAIN COMPONENT ══════════════════════ */
export default function RouteExplorer() {
  const { currentResult, geometryStatus } = useApp();
  const [selectedVehicle, setSelectedVehicle] = useState<string | null>(null);
  const [showMotion, setShowMotion] = useState(false);
  const [motionSpeed, setMotionSpeed] = useState(1);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [rightPanelOpen, setRightPanelOpen] = useState(true);
  const [animationProgress, setAnimationProgress] = useState(0); // 0-1
  const [loopMode, setLoopMode] = useState(true); // true = infinite loop, false = play once
  const endPauseRef = useRef<number | null>(null); // timer for 1s pause at end in loop mode
  const mapRef = useRef<L.Map | null>(null);
  const containerRef = useRef<HTMLDivElement | null>(null);
  const animFrameRef = useRef<number | null>(null);
  const lastTickRef = useRef<number>(0);
  const tripScrollRef = useRef<HTMLDivElement | null>(null);

  const handleFocus = () => {
    if (mapRef.current && selectedBounds) mapRef.current.fitBounds(selectedBounds, { padding: [50, 50] });
  };

  const handleZoom = (delta: number) => {
    if (mapRef.current) mapRef.current.setZoom(mapRef.current.getZoom() + delta);
  };

  const toggleFullscreen = () => {
    if (!document.fullscreenElement) { containerRef.current?.requestFullscreen(); setIsFullscreen(true); }
    else { document.exitFullscreen(); setIsFullscreen(false); }
  };

  useEffect(() => {
    const h = () => setIsFullscreen(!!document.fullscreenElement);
    document.addEventListener('fullscreenchange', h);
    return () => document.removeEventListener('fullscreenchange', h);
  }, []);

  // Auto-open right panel when vehicle selected
  useEffect(() => { if (selectedVehicle) setRightPanelOpen(true); }, [selectedVehicle]);

  // Reset animation progress when switching vehicles
  useEffect(() => { setAnimationProgress(0); }, [selectedVehicle]);

  if (!currentResult) {
    return (
      <div className="min-h-screen flex items-center justify-center" style={{ background: '#05070A' }}>
        <div className="text-center">
          <Icon name="map" className="text-5xl text-primary/30 mb-4 block mx-auto" />
          <p className="text-white/50 mb-4">No optimization results available</p>
          <Link to="/upload" className="btn-primary inline-flex items-center gap-2">
            <Icon name="upload" className="text-lg" /> Start New Optimization
          </Link>
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

  // Get office location
  const officeLocation = useMemo(() => {
    for (const trip of currentResult.trips) {
      const officePoint = trip.route.find(p => p.type === 'office' || p.type === 'dropoff');
      if (officePoint) return [officePoint.lat, officePoint.lng] as [number, number];
    }
    if (currentResult.trips.length > 0 && currentResult.trips[0].route.length > 0) {
      const lastPoint = currentResult.trips[0].route[currentResult.trips[0].route.length - 1];
      return [lastPoint.lat, lastPoint.lng] as [number, number];
    }
    return null;
  }, [currentResult.trips]);

  // Geometry loading check
  const geometryLoadingStatus = useMemo(() => {
    let totalPoints = 0, pointsWithGeometry = 0;
    for (const trip of currentResult.trips) {
      for (const point of trip.route) {
        if (point.distanceFromPrev && point.distanceFromPrev > 0) {
          totalPoints++;
          if (point.geometry && point.geometry.length > 0) pointsWithGeometry++;
        }
      }
    }
    const hasPartialGeometry = totalPoints > 0 && pointsWithGeometry > 0 && pointsWithGeometry < totalPoints;
    return { isLoading: hasPartialGeometry && geometryStatus !== 'complete', hasGeometry: pointsWithGeometry > 0, total: totalPoints, loaded: pointsWithGeometry };
  }, [currentResult.trips, geometryStatus]);

  // Map bounds
  const mapBounds = useMemo(() => {
    const allPoints: [number, number][] = [];
    currentResult.trips.forEach(trip => trip.route.forEach(point => allPoints.push([point.lat, point.lng])));
    return allPoints.length === 0 ? L.latLngBounds([12.9, 77.5], [13.1, 77.7]) : L.latLngBounds(allPoints);
  }, [currentResult.trips]);

  const selectedBounds = useMemo(() => {
    const allPoints: [number, number][] = [];
    selectedTrips.forEach(trip => trip.route.forEach(point => allPoints.push([point.lat, point.lng])));
    const vIds = selectedVehicle ? [selectedVehicle] : vehicleIds;
    vIds.forEach(vid => { const v = currentResult.vehicles.find(x => x.id === vid); if (v) allPoints.push([v.currentLat, v.currentLng]); });
    if (officeLocation) allPoints.push(officeLocation);
    return allPoints.length === 0 ? mapBounds : L.latLngBounds(allPoints);
  }, [selectedTrips, mapBounds, currentResult.vehicles, vehicleIds, selectedVehicle, officeLocation]);

  // Selected vehicle detail
  const selectedVehicleTrips = selectedVehicle ? vehicleTrips.get(selectedVehicle) || [] : [];
  const selTotalDist = selectedVehicleTrips.reduce((s, t) => s + t.distance, 0);
  const selTotalDur = selectedVehicleTrips.reduce((s, t) => s + t.duration, 0);
  const selTotalEmployees = selectedVehicleTrips.reduce((s, t) => s + t.employees.length, 0);
  const selVehicleIdx = selectedVehicle ? vehicleIds.indexOf(selectedVehicle) : -1;
  const selColor = selVehicleIdx >= 0 ? colors[selVehicleIdx % colors.length] : '#FFB800';

  // Build all route sequence items for right panel
  const tripSequenceItems = useMemo(() => {
    if (!selectedVehicle) return [];
    const items: { type: 'depot' | 'pickup' | 'office'; label: string; time: string; address: string; employeeId?: string; tripNum: number; isActive?: boolean }[] = [];
    const vehicle = currentResult.vehicles.find(v => v.id === selectedVehicle);
    if (vehicle) {
      items.push({ type: 'depot', label: 'Starting Point', time: vehicle.availabilityTime || '--', address: vehicle.currentLocation, tripNum: 0 });
    }
    let pickupCounter = 0;
    selectedVehicleTrips.forEach((trip, tripIdx) => {
      trip.route.forEach((point, pointIdx) => {
        if (point.type === 'pickup') {
          pickupCounter++;
          items.push({ type: 'pickup', label: `Pickup #${pickupCounter}`, time: point.time, address: point.address, employeeId: point.employeeId, tripNum: trip.tripNumber });
        } else if (point.type === 'office' || point.type === 'dropoff') {
          // Skip office/depot at the START of trips after the first — it duplicates the previous trip's drop-off
          if (pointIdx === 0 && tripIdx > 0) return;
          items.push({ type: 'office', label: 'Office Drop-off', time: point.time, address: point.address, tripNum: trip.tripNumber });
        }
      });
    });
    return items;
  }, [selectedVehicle, selectedVehicleTrips, currentResult.vehicles]);

  /* ── Build per-vehicle timelines and global time range ── */
  const { vehicleTimelines, globalStartMin, globalEndMin } = useMemo(() => {
    const timelines = new Map<string, TimelineSegment[]>();
    let gStart = Infinity, gEnd = -Infinity;
    for (const vid of vehicleIds) {
      const trips = vehicleTrips.get(vid) || [];
      const vehicle = currentResult.vehicles.find(v => v.id === vid);
      const segs = buildVehicleTimeline(vid, trips, vehicle);
      timelines.set(vid, segs);
      for (const s of segs) {
        if (s.startMin < gStart) gStart = s.startMin;
        if (s.endMin > gEnd) gEnd = s.endMin;
      }
    }
    if (!isFinite(gStart)) gStart = 0;
    if (!isFinite(gEnd)) gEnd = gStart + 60;
    return { vehicleTimelines: timelines, globalStartMin: gStart, globalEndMin: gEnd };
  }, [vehicleIds, vehicleTrips, currentResult.vehicles]);

  /* ── Per-vehicle or global time window ── */
  const activeStartMin = useMemo(() => {
    if (!selectedVehicle) return globalStartMin;
    const segs = vehicleTimelines.get(selectedVehicle);
    if (!segs || !segs.length) return globalStartMin;
    return Math.min(...segs.map(s => s.startMin));
  }, [selectedVehicle, vehicleTimelines, globalStartMin]);

  const activeEndMin = useMemo(() => {
    if (!selectedVehicle) return globalEndMin;
    const segs = vehicleTimelines.get(selectedVehicle);
    if (!segs || !segs.length) return globalEndMin;
    return Math.max(...segs.map(s => s.endMin));
  }, [selectedVehicle, vehicleTimelines, globalEndMin]);

  const totalTimelineMinutes = activeEndMin - activeStartMin || 1;
  const currentTimeMin = activeStartMin + animationProgress * totalTimelineMinutes;

  /* ── Animation loop ── */
  useEffect(() => {
    if (!showMotion) {
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
      animFrameRef.current = null;
      if (endPauseRef.current) { clearTimeout(endPauseRef.current); endPauseRef.current = null; }
      return;
    }
    // If pausing at end (loop mode), don't start tick yet
    if (endPauseRef.current) return;
    lastTickRef.current = performance.now();
    const tick = (now: number) => {
      const dt = (now - lastTickRef.current) / 1000;
      lastTickRef.current = now;
      const minuteStep = dt * motionSpeed * 2;
      setAnimationProgress(prev => {
        const next = prev + minuteStep / totalTimelineMinutes;
        if (next >= 1) {
          if (loopMode) {
            // Pause 1 second at the end, then restart
            if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
            animFrameRef.current = null;
            endPauseRef.current = window.setTimeout(() => {
              endPauseRef.current = null;
              setAnimationProgress(0);
              // Re-trigger by forcing a state update (loop effect will re-run)
            }, 1000);
            return 1; // hold at end during pause
          } else {
            // Play once: stop at end
            setShowMotion(false);
            return 1;
          }
        }
        return next;
      });
      if (animFrameRef.current !== null) {
        animFrameRef.current = requestAnimationFrame(tick);
      }
    };
    animFrameRef.current = requestAnimationFrame(tick);
    return () => {
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
      animFrameRef.current = null;
    };
  }, [showMotion, motionSpeed, totalTimelineMinutes, loopMode, animationProgress]);

  /* ── Compute per-item reached state AND fractional line fill to next item ── */
  /*   Uses currentTimeMin (same clock as map car + bottom bar) so the right
       pane is perfectly in sync with both the playback bar and the map.       */
  const sequenceProgress = useMemo(() => {
    if (!selectedVehicle || !tripSequenceItems.length) return [] as { reached: boolean; lineFill: number }[];
    if (!showMotion) return tripSequenceItems.map(() => ({ reached: false, lineFill: 0 }));

    const times = tripSequenceItems.map(item => parseTimeToMinutes(item.time));

    return tripSequenceItems.map((_item, i) => {
      const reached = currentTimeMin >= times[i];

      let lineFill = 0;
      if (i < tripSequenceItems.length - 1) {
        const segStart = times[i];
        const segEnd = times[i + 1];
        const segLen = segEnd - segStart;
        if (currentTimeMin >= segEnd) {
          lineFill = 1;
        } else if (currentTimeMin > segStart && segLen > 0) {
          lineFill = (currentTimeMin - segStart) / segLen;
        }
      }
      return { reached, lineFill };
    });
  }, [selectedVehicle, tripSequenceItems, showMotion, currentTimeMin]);

  /* ── Auto-scroll the right pane to the current active item ── */
  useEffect(() => {
    if (!showMotion || !tripScrollRef.current || !sequenceProgress.length) return;
    // Find the last reached item index
    let lastReachedIdx = -1;
    for (let i = sequenceProgress.length - 1; i >= 0; i--) {
      if (sequenceProgress[i].reached) { lastReachedIdx = i; break; }
    }
    if (lastReachedIdx < 0) return;
    const container = tripScrollRef.current;
    const target = container.querySelector(`[data-seq-idx="${lastReachedIdx}"]`) as HTMLElement | null;
    if (target) {
      const containerRect = container.getBoundingClientRect();
      const targetRect = target.getBoundingClientRect();
      const offsetTop = targetRect.top - containerRect.top + container.scrollTop;
      container.scrollTo({ top: offsetTop - containerRect.height / 3, behavior: 'smooth' });
    }
  }, [showMotion, sequenceProgress]);

  return (
    <div ref={containerRef} className="h-full flex overflow-hidden" style={{ background: '#05070A' }}>

      {/* ═══ LEFT SIDEBAR: Active Vehicles ═══ */}
      <aside className="w-72 flex-shrink-0 flex flex-col border-r border-white/[0.06] bg-panel-dark/95 backdrop-blur-sm z-30">
        {/* Header */}
        <div className="px-4 py-3.5 border-b border-white/[0.06] flex items-center">
          <h3 className="font-bold text-white/90 text-sm">Active Vehicles</h3>
        </div>

        {/* Vehicle List */}
        <div className="flex-1 overflow-y-auto p-2 space-y-1.5" style={{ scrollbarWidth: 'thin', scrollbarColor: 'rgba(255,255,255,0.1) transparent' }}>
          {vehicleIds.map((vehicleId) => {
            const trips = vehicleTrips.get(vehicleId) || [];
            const vehicle = currentResult.vehicles.find(v => v.id === vehicleId);
            const isActive = selectedVehicle === vehicleId;
            const totalDist = trips.reduce((s, t) => s + t.distance, 0);
            const totalEmps = trips.reduce((s, t) => s + t.employees.length, 0);
            const capacity = vehicle?.capacity || 1;
            const utilization = Math.min(100, Math.round((totalEmps / (capacity * trips.length || 1)) * 100));

            return (
              <button
                key={vehicleId}
                onClick={() => setSelectedVehicle(isActive ? null : vehicleId)}
                className={`w-full text-left p-3 flex flex-col gap-2 transition-all duration-200 ${
                  isActive
                    ? 'bg-primary/[0.08] border border-primary/20 ring-1 ring-primary/20'
                    : 'border border-transparent hover:bg-white/[0.03]'
                }`}
              >
                <div className="flex justify-between items-start">
                  <div className="flex items-center gap-2.5">
                    <div className="w-2.5 h-2.5 rounded-full flex-shrink-0 bg-primary" style={{ boxShadow: 'none' }} />
                    <div>
                      <p className="text-sm font-bold text-white/90">{vehicleId}</p>
                      <p className="text-[11px] text-white/35">{vehicle?.fuelType} {vehicle?.mode} &middot; {trips.length} trip{trips.length !== 1 ? 's' : ''}</p>
                    </div>
                  </div>
                  <span className={`text-[9px] font-bold px-2 py-0.5 uppercase ${
                    trips.length > 0 ? 'bg-primary/10 text-primary' : 'bg-white/[0.06] text-white/30'
                  }`}>
                    {trips.length > 0 ? 'Active' : 'Idle'}
                  </span>
                </div>
                {isActive && (
                  <div className="grid grid-cols-2 gap-2 mt-1">
                    <div className="bg-white/[0.04] p-2">
                      <p className="text-[9px] text-white/30 uppercase">Utilization</p>
                      <p className="text-xs font-bold text-white/80">{utilization}%</p>
                    </div>
                    <div className="bg-white/[0.04] p-2">
                      <p className="text-[9px] text-white/30 uppercase">Distance</p>
                      <p className="text-xs font-bold text-white/80">{totalDist.toFixed(1)}km</p>
                    </div>
                  </div>
                )}
              </button>
            );
          })}
        </div>

        {/* Footer */}
        <div className="p-3 border-t border-white/[0.06]">
          <div className="text-[10px] text-white/25 text-center mb-2">{vehicleIds.length} vehicles &middot; {currentResult.trips.length} trips</div>
        </div>
      </aside>

      {/* ═══ CENTER: Map Area ═══ */}
      <main className="flex-1 relative overflow-hidden">

        {/* Geometry loading overlay */}
        {geometryLoadingStatus.isLoading && (
          <div className="absolute inset-0 z-20 flex items-center justify-center bg-panel-dark/70">
            <div className="bg-panel-dark border border-white/10 p-8 text-center">
              <div className="w-12 h-12 mx-auto mb-4 border-4 border-primary/20 border-t-primary rounded-full animate-spin" />
              <p className="text-white font-medium">Loading Map Routes...</p>
              <p className="text-xs text-white/30 mt-1">{geometryLoadingStatus.loaded}/{geometryLoadingStatus.total} segments</p>
            </div>
          </div>
        )}

        {/* Top Overlay: Filters (left) + Controls (right) */}
        <div className="absolute top-4 left-4 right-4 flex justify-between items-start pointer-events-none z-10">
          {/* Left: Filter tags */}
          <div className="flex gap-2 pointer-events-auto">
            <button
              onClick={() => { setSelectedVehicle(null); }}
              className={`flex h-8 items-center gap-2 px-3 text-xs font-semibold transition-all border ${
                !selectedVehicle
                  ? 'bg-primary text-background-dark border-primary/40'
                  : 'bg-panel-dark/90 text-white/70 border-white/[0.08] hover:border-white/15'
              }`}
            >
              <Icon name="visibility" className="text-sm" />
              All Routes
            </button>
            {vehicleIds.map((vid) => {
              const active = selectedVehicle === vid;
              return (
                <button
                  key={vid}
                  onClick={() => setSelectedVehicle(active ? null : vid)}
                  className={`flex h-8 items-center gap-2 px-3 text-xs font-semibold transition-all border ${
                    active
                      ? 'bg-primary text-background-dark border-primary/40'
                      : 'bg-panel-dark/90 text-white/70 border-white/[0.08] hover:border-white/15'
                  }`}
                >
                  <div className={`w-2 h-2 rounded-full ${active ? 'bg-background-dark' : 'bg-primary'}`} />
                  {vid}
                </button>
              );
            })}
          </div>

          {/* Right: Map controls */}
          <div className="flex flex-col gap-1.5 pointer-events-auto">
            <button onClick={() => handleZoom(1)} className="size-8 flex items-center justify-center bg-panel-dark/90 border border-white/[0.08] text-white/60 hover:text-white hover:bg-white/[0.05] transition-colors">
              <Icon name="add" className="text-base" />
            </button>
            <button onClick={() => handleZoom(-1)} className="size-8 flex items-center justify-center bg-panel-dark/90 border border-white/[0.08] text-white/60 hover:text-white hover:bg-white/[0.05] transition-colors">
              <Icon name="remove" className="text-base" />
            </button>
            <button
              onClick={handleFocus}
              className="size-8 flex items-center justify-center bg-panel-dark/90 border border-white/[0.08] text-white/60 hover:text-white hover:bg-white/[0.05] transition-colors"
              title="Center Map"
            >
              <Icon name="my_location" className="text-base" />
            </button>
            <button
              onClick={toggleFullscreen}
              className="size-8 flex items-center justify-center bg-primary text-background-dark border border-primary/30 hover:bg-primary/90 transition-colors"
              title={isFullscreen ? 'Exit Fullscreen' : 'Fullscreen'}
            >
              <Icon name={isFullscreen ? 'fullscreen_exit' : 'fullscreen'} className="text-base" />
            </button>
          </div>
        </div>

        {/* Leaflet Map */}
        <MapContainer
          center={officeLocation || [12.9716, 77.5946]}
          zoom={12}
          style={{ height: '100%', width: '100%' }}
          className="z-0"
          zoomControl={false}
        >
          <TileLayer
            attribution='&copy; <a href="https://stadiamaps.com/">Stadia Maps</a>'
            url="https://tiles.stadiamaps.com/tiles/alidade_smooth_dark/{z}/{x}/{y}{r}.png"
          />
          <FitBounds bounds={mapBounds} mapRef={mapRef} />

          {/* Route lines */}
          {selectedTrips.map((trip) => {
            const vIdx = vehicleIds.indexOf(trip.vehicleId);
            const color = colors[vIdx % colors.length];
            const isSelected = selectedVehicle === trip.vehicleId || !selectedVehicle;
            if (!isSelected || trip.route.length < 1) return null;

            const vehicle = currentResult.vehicles.find(v => v.id === trip.vehicleId);
            const positions: [number, number][] = [];
            const vehicleTripsArray = vehicleTrips.get(trip.vehicleId) || [];
            const isFirstTrip = vehicleTripsArray[0]?.tripNumber === trip.tripNumber;
            if (isFirstTrip && vehicle) positions.push([vehicle.currentLat, vehicle.currentLng]);

            trip.route.forEach((point) => {
              if (point.geometry && point.geometry.length > 0) {
                point.geometry.slice(0, -1).forEach(coord => positions.push(coord));
              }
              positions.push([point.lat, point.lng]);
            });

            if (positions.length < 2) return null;

            return (
              <div key={`route-${trip.vehicleId}-${trip.tripNumber}`}>
                <Polyline positions={positions}
                  pathOptions={{ color, weight: showMotion ? 1.5 : 2.5, opacity: showMotion ? 0.3 : 0.8 }} />
                {positions.length >= 2 && (() => {
                  const midIndex = Math.floor(positions.length / 2);
                  const startIdx = Math.max(0, midIndex - 1);
                  const endIdx = Math.min(positions.length - 1, midIndex);
                  const { midpoint, angle } = getMidpointAndAngle(positions[startIdx], positions[endIdx]);
                  return <Marker position={midpoint} icon={createTripLabelIcon(trip.tripNumber, color, angle)} interactive={false} />;
                })()}
              </div>
            );
          })}

          {/* Timeline vehicle car markers */}
          {showMotion && (
            <TimelineVehicleMarkers
              vehicleTimelines={vehicleTimelines}
              currentTimeMin={currentTimeMin}
              vehicles={currentResult.vehicles}
              vehicleIds={selectedVehicle ? [selectedVehicle] : vehicleIds}
            />
          )}

          {/* Pickup markers */}
          {selectedTrips.map((trip) => {
            const isSelected = selectedVehicle === trip.vehicleId || !selectedVehicle;
            if (!isSelected) return null;
            return trip.route.filter(p => p.type === 'pickup').map((point, pIdx) => (
              <Marker key={`pickup-${trip.vehicleId}-${trip.tripNumber}-${pIdx}`}
                position={[point.lat, point.lng]} icon={pickupIcon} zIndexOffset={100}>
                <Popup><div className="text-sm"><strong>{point.employeeId || 'Pickup'}</strong><br />Time: {point.time}<br />{point.address}</div></Popup>
              </Marker>
            ));
          })}

          {/* Office marker */}
          {officeLocation && (
            <Marker position={officeLocation} icon={officeIcon} zIndexOffset={1000}>
              <Popup><div className="text-sm"><strong>Office</strong><br />Drop-off Point</div></Popup>
            </Marker>
          )}

          {/* Vehicle depot markers */}
          {vehicleIds.map((vehicleId, idx) => {
            const vehicle = currentResult.vehicles.find(v => v.id === vehicleId);
            const trips = vehicleTrips.get(vehicleId) || [];
            const isSelected = selectedVehicle === vehicleId || !selectedVehicle;
            const color = colors[idx % colors.length];
            if (!vehicle || trips.length === 0 || !isSelected) return null;
            return (
              <Marker key={`depot-${vehicleId}`} position={[vehicle.currentLat, vehicle.currentLng]}
                icon={createDepotIcon(color)} zIndexOffset={500}>
                <Popup>
                  <div className="text-sm">
                    <strong>{vehicleId} Depot</strong><br />
                    Location: {vehicle.currentLocation}<br />
                    {trips.length} trip{trips.length !== 1 ? 's' : ''}<br />
                    Total: {trips.reduce((sum, t) => sum + t.distance, 0).toFixed(1)} km
                  </div>
                </Popup>
              </Marker>
            );
          })}
        </MapContainer>

        {/* ── Bottom Playback Timeline ── */}
        <div className="absolute bottom-5 left-1/2 -translate-x-1/2 w-[92%] max-w-3xl z-10">
          <div className="bg-panel-dark/95 border border-white/[0.08] px-5 py-3">
            <div className="flex items-center gap-4">
              {/* Play / Pause */}
              <button
                onClick={() => { if (!showMotion) { setAnimationProgress(0); } setShowMotion(!showMotion); }}
                className={`flex-shrink-0 size-10 flex items-center justify-center transition-all ${
                  showMotion
                    ? 'bg-primary text-background-dark'
                    : 'bg-white/[0.08] text-white/70 hover:bg-white/[0.12]'
                }`}
              >
                <Icon name={showMotion ? 'pause' : 'play_arrow'} className="text-xl" />
              </button>

              {/* Timeline bar + info */}
              <div className="flex-1 min-w-0">
                <div className="flex justify-between items-center mb-1.5">
                  <div className="flex items-center gap-3">
                    <span className="text-xs font-mono font-bold text-primary">{minutesToHHMM(currentTimeMin)}</span>
                    <span className={`text-[9px] font-label font-bold uppercase tracking-widest ${
                      showMotion ? 'text-primary' : 'text-white/30'
                    }`}>
                      {showMotion ? 'Playing' : 'Paused'}
                    </span>
                  </div>
                  <div className="flex items-center gap-4 text-[9px] font-mono text-white/25">
                    <span>{minutesToHHMM(activeStartMin)}</span>
                    <span>→</span>
                    <span>{minutesToHHMM(activeEndMin)}</span>
                  </div>
                </div>
                {/* Clickable progress bar */}
                <div
                  className="group relative h-4 flex items-center cursor-pointer"
                  onClick={(e) => {
                    const rect = e.currentTarget.getBoundingClientRect();
                    const frac = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
                    setAnimationProgress(frac);
                  }}
                >
                  <div className="absolute w-full h-1.5 bg-white/[0.06] overflow-hidden">
                    <div className="h-full bg-primary/70"
                      style={{ width: `${animationProgress * 100}%` }} />
                  </div>
                  {/* Thumb */}
                  <div className="absolute -translate-x-1/2 size-3 bg-primary border-2 border-background-dark"
                    style={{ left: `${animationProgress * 100}%` }} />
                </div>
              </div>

              {/* Speed controls — 2×2 grid */}
              <div className="flex-shrink-0 flex flex-col items-center gap-0.5">
                <span className="text-[7px] font-bold text-white/20 uppercase tracking-widest">Speed</span>
                <div className="grid grid-cols-2 gap-0.5">
                  {[1, 2, 5, 10].map(s => (
                    <button
                      key={s}
                      onClick={() => setMotionSpeed(s)}
                      className={`w-9 py-0.5 text-[10px] font-bold transition-all ${
                        motionSpeed === s
                          ? 'bg-primary text-background-dark'
                          : 'text-white/40 hover:text-white/60 bg-white/[0.04]'
                      }`}
                    >
                      {s}x
                    </button>
                  ))}
                </div>
              </div>

              {/* Loop / Once toggle */}
              <button
                onClick={() => setLoopMode(!loopMode)}
                className={`flex-shrink-0 size-10 flex items-center justify-center transition-all ${
                  loopMode
                    ? 'bg-primary/20 text-primary border border-primary/30'
                    : 'bg-white/[0.04] text-white/30 border border-white/[0.06] hover:text-white/50'
                }`}
                title={loopMode ? 'Loop: ON' : 'Loop: OFF'}
              >
                <Icon name="repeat" className="text-lg" />
              </button>
            </div>
          </div>
        </div>
      </main>

      {/* ═══ RIGHT SIDEBAR: Route Detail ═══ */}
      {rightPanelOpen && selectedVehicle && (
        <aside className="w-80 flex-shrink-0 flex flex-col border-l border-white/[0.06] bg-panel-dark/95 backdrop-blur-sm z-30">
          {/* Header */}
          <div className="px-5 py-4 border-b border-white/[0.06] flex justify-between items-center bg-white/[0.02]">
            <div>
              <h3 className="text-base font-bold text-white/90">{selectedVehicle}</h3>
              <p className="text-[11px] text-white/35 font-medium">Route Detail &middot; {selectedVehicleTrips.length} Trip{selectedVehicleTrips.length !== 1 ? 's' : ''}</p>
            </div>
            <button onClick={() => { setRightPanelOpen(false); setSelectedVehicle(null); }}
              className="p-1.5 hover:bg-white/[0.05] text-white/30 hover:text-white/60 transition-colors">
              <Icon name="close" className="text-lg" />
            </button>
          </div>

          {/* Stats Grid */}
          <div className="px-5 py-4 grid grid-cols-2 gap-3">
            <div className="flex flex-col border border-white/[0.06] p-3 bg-white/[0.02]">
              <Icon name="distance" className="text-primary text-lg mb-1" />
              <span className="text-[9px] text-white/30 uppercase tracking-wider font-bold">Total Distance</span>
              <span className="text-base font-bold text-white/90">{selTotalDist.toFixed(1)} km</span>
            </div>
            <div className="flex flex-col border border-white/[0.06] p-3 bg-white/[0.02]">
              <Icon name="timer" className="text-primary text-lg mb-1" />
              <span className="text-[9px] text-white/30 uppercase tracking-wider font-bold">Est. Time</span>
              <span className="text-base font-bold text-white/90">{fmtDuration(selTotalDur)}</span>
            </div>
            <div className="flex flex-col border border-white/[0.06] p-3 bg-white/[0.02]">
              <Icon name="group" className="text-primary text-lg mb-1" />
              <span className="text-[9px] text-white/30 uppercase tracking-wider font-bold">Employees</span>
              <span className="text-base font-bold text-white/90">{selTotalEmployees}</span>
            </div>
            <div className="flex flex-col border border-white/[0.06] p-3 bg-white/[0.02]">
              <Icon name="payments" className="text-primary text-lg mb-1" />
              <span className="text-[9px] text-white/30 uppercase tracking-wider font-bold">Trip Cost</span>
              <span className="text-base font-bold text-white/90">₹{selectedVehicleTrips.reduce((s, t) => s + t.cost, 0).toFixed(0)}</span>
            </div>
          </div>

          {/* Trip Sequence */}
          <div ref={tripScrollRef} className="flex-1 overflow-y-auto px-5 pb-5" style={{ scrollbarWidth: 'thin', scrollbarColor: 'rgba(255,255,255,0.1) transparent' }}>
            <h4 className="text-[10px] font-bold text-white/25 uppercase tracking-widest mb-3">Trip Sequence</h4>
            <div className="space-y-0">
              {tripSequenceItems.map((item, i) => {
                const isLast = i === tripSequenceItems.length - 1;
                const isDepot = item.type === 'depot';
                const isOffice = item.type === 'office';
                const isPickup = item.type === 'pickup';
                const progress = sequenceProgress[i] || { reached: false, lineFill: 0 };
                const reached = progress.reached;
                const fill = progress.lineFill;

                // Compute gradient background for the connecting line
                const lineStyle: React.CSSProperties = !isLast ? {
                  background: fill >= 1
                    ? '#FFB800'
                    : fill > 0
                      ? `linear-gradient(to bottom, #FFB800 ${fill * 100}%, rgba(255,255,255,0.08) ${fill * 100}%)`
                      : 'rgba(255,255,255,0.08)'
                } : {};

                return (
                  <div key={i} data-seq-idx={i} className={`relative pl-6 ${!isLast ? 'pb-4' : ''}`}
                    style={{ marginLeft: '7px' }}>
                    {/* Gradient connecting line */}
                    {!isLast && (
                      <div className="absolute left-0 top-0 bottom-0 w-[2px]" style={lineStyle} />
                    )}

                    {/* Timeline dot */}
                    <div className={`absolute -left-[9px] top-0 size-4 rounded-full border-[3px] ${
                      reached
                        ? 'bg-primary border-[#0D1117]'
                        : 'border-[#0D1117]'
                    }`}
                      style={{
                        boxShadow: reached ? `0 0 8px ${selColor}66` : 'none',
                        backgroundColor: reached ? undefined : 'rgba(255,255,255,0.08)',
                        outline: reached ? 'none' : '1.5px solid rgba(255,255,255,0.15)'
                      }}
                    />

                    {/* Card */}
                    <div className={`p-3 transition-all ${
                      reached
                        ? 'border border-primary/20 bg-primary/[0.06]'
                        : isDepot ? 'border border-primary/15 bg-primary/[0.04]' : isPickup ? 'border border-white/[0.06] hover:bg-white/[0.02] cursor-default' : 'border border-white/[0.05] bg-white/[0.01]'
                    }`}>
                      <div className="flex justify-between mb-1">
                        <span className={`text-[10px] font-bold uppercase ${
                          reached ? 'text-primary' : isDepot ? 'text-primary/60' : isOffice ? 'text-white/35' : 'text-white/50'
                        }`}>
                          {item.label}
                        </span>
                        <span className={`text-[10px] ${reached ? 'text-primary/60' : isDepot ? 'text-white/40' : 'text-white/30'}`}>{item.time}</span>
                      </div>
                      <p className={`text-xs font-semibold mb-0.5 truncate ${reached ? 'text-white/90' : 'text-white/80'}`}>{item.address}</p>
                      {isPickup && item.employeeId && (
                        <div className="flex items-center gap-2 mt-2">
                          <div className="size-6 rounded-full flex items-center justify-center text-[10px] font-bold" style={{ backgroundColor: `${selColor}22`, color: selColor }}>
                            {item.employeeId.charAt(0)}
                          </div>
                          <div>
                            <p className="text-[11px] font-semibold text-white/70">{item.employeeId}</p>
                            <p className="text-[9px] text-white/30">Trip #{item.tripNum}</p>
                          </div>
                        </div>
                      )}
                    </div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Footer */}
          <div className="px-5 py-3.5 border-t border-white/[0.06] flex gap-2">
            <button
              onClick={() => { setShowMotion(!showMotion); }}
              className="flex-1 py-2.5 bg-white/[0.05] text-white/60 border border-white/[0.08] hover:bg-white/[0.08] transition-colors flex items-center justify-center gap-2"
            >
              <Icon name={showMotion ? 'pause' : 'play_arrow'} className="text-lg" />
              <span className="text-xs font-bold">{showMotion ? 'Pause' : 'Play'}</span>
            </button>
          </div>
        </aside>
      )}

      {/* Right panel closed: vehicle info badge */}
      {(!rightPanelOpen || !selectedVehicle) && selectedVehicle && (
        <button
          onClick={() => setRightPanelOpen(true)}
          className="absolute right-4 top-4 z-30 px-3 py-2 bg-panel-dark/95 border border-white/[0.08] shadow-xl flex items-center gap-2 text-white/70 hover:text-white transition-colors"
        >
          <Icon name="info" className="text-lg" />
          <span className="text-xs font-semibold">{selectedVehicle}</span>
        </button>
      )}
    </div>
  );
}

