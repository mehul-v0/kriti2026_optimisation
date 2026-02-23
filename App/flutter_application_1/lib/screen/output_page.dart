import 'dart:async';
import 'dart:math';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import 'package:flutter_application_1/config/info.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/services/optimization_service.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/services/file_export_service.dart';
import 'package:flutter_application_1/widgets/download_dialog.dart';
import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/screen/output_details_page.dart';

// ─────────────────────────────────────────────────────────────
//  OutputPage — Roxio Theme, Route Visualization & Animation
// ─────────────────────────────────────────────────────────────

class OutputPage extends StatefulWidget {
  final String testCaseId;
  final String testCaseName;
  final Map<String, dynamic> resultData;

  const OutputPage({
    super.key,
    required this.testCaseId,
    required this.testCaseName,
    required this.resultData,
  });

  @override
  State<OutputPage> createState() => _OutputPageState();
}

class _OutputPageState extends State<OutputPage> with TickerProviderStateMixin {
  // ── Data ────────────────────────────────────────────────
  late Map<String, dynamic> _currentData;
  late List<_VehicleRoute> _vehicleRoutes;

  // ── Services ────────────────────────────────────────────
  final OptimizationService _optimizationService = OptimizationService();
  final DataService _dataService = DataService();
  final FileExportService _fileExportService = FileExportService();

  // ── Map ─────────────────────────────────────────────────
  final MapController _mapController = MapController();
  bool _mapReady = false;

  // ── Animation ───────────────────────────────────────────
  late AnimationController _playbackController;
  bool _isPlaying = false;
  double _playbackProgress = 0.0; // 0.0 → 1.0
  int? _focusedVehicleIndex; // null = show all
  final Set<int> _expandedCards = {};

  // ── UI State ────────────────────────────────────────────
  bool _isLoading = false;
  bool _isDownloading = false; // true only during file export/download
  final TextEditingController _searchController = TextEditingController();

  // ── Fleet Filter ────────────────────────────────────────
  bool _showFleetFilter = false;
  String? _filterVehicleId;
  bool? _sortByCost; // true = asc, false = desc
  bool? _sortByDistance; // true = asc, false = desc
  String _vehicleSearchQuery = '';

  // ── Summary Toggle ──────────────────────────────────────
  bool _summaryExpanded = true;

  // ── Draggable Summary Panel (mobile) ────────────────────
  double _summarySheetExtent = 0.5;

  // ── Playback Speed ─────────────────────────────────────
  static const List<double> _speedOptions = [0.5, 1.0, 2.0, 4.0];
  int _speedIndex = 1; // default x1
  double get _playbackSpeed => _speedOptions[_speedIndex];

  // ── Route Colors ────────────────────────────────────────
  static const List<Color> _routePalette = [
    Color(0xFF00C569), // Brand green
    Color(0xFF3B82F6), // Blue
    Color(0xFFF59E0B), // Amber
    Color(0xFFEF4444), // Red
    Color(0xFF8B5CF6), // Violet
    Color(0xFF06B6D4), // Cyan
    Color(0xFFEC4899), // Pink
    Color(0xFFD97706), // Orange
  ];

  // ── Dark-mode matrix for map tiles ──────────────────────
  static const List<double> _darkModeMatrix = [
    -1,
    0,
    0,
    0,
    255,
    0,
    -1,
    0,
    0,
    255,
    0,
    0,
    -1,
    0,
    255,
    0,
    0,
    0,
    1,
    0,
  ];

  // ══════════════════════════════════════════════════════════
  //  THEME HELPERS (mirror ShowInputPage)
  // ══════════════════════════════════════════════════════════
  bool _isDark(BuildContext context) =>
      Theme.of(context).brightness == Brightness.dark;

  Color _bgColor(BuildContext context) =>
      _isDark(context) ? AppColors.darkBackground : AppColors.lightBackground;

  Color _surfaceColor(BuildContext context) =>
      _isDark(context) ? AppColors.darkSurface : AppColors.lightSurface;

  Color _textPrimary(BuildContext context) =>
      _isDark(context) ? Colors.white : AppColors.textPrimaryLight;

  Color _textSecondary(BuildContext context) =>
      _isDark(context) ? Colors.white54 : AppColors.textSecondaryLight;

  Color _textTertiary(BuildContext context) =>
      _isDark(context) ? Colors.white38 : Colors.black38;

  Color _borderColorThemed(BuildContext context) =>
      _isDark(context) ? AppColors.darkBorderColor : AppColors.borderColor;

  Color _shadowColor(BuildContext context) =>
      _isDark(context) ? Colors.black54 : Colors.black26;

  // ══════════════════════════════════════════════════════════
  //  LIFECYCLE
  // ══════════════════════════════════════════════════════════
  @override
  void initState() {
    super.initState();
    _currentData = widget.resultData;
    _vehicleRoutes = _parseVehicleRoutes(_currentData);

    _playbackController =
        AnimationController(vsync: this, duration: const Duration(seconds: 15))
          ..addListener(() {
            setState(() => _playbackProgress = _playbackController.value);
          });
  }

  @override
  void dispose() {
    _playbackController.dispose();
    _searchController.dispose();
    super.dispose();
  }

  // ══════════════════════════════════════════════════════════
  //  DATA PARSING — handles BOTH formats:
  //  Format A (raw solver): { vehicles: [{ trips: [{ stops: [...] }] }] }
  //  Format B (backend API): { result: {...}, routes: [...], assignments: [...] }
  // ══════════════════════════════════════════════════════════
  List<_VehicleRoute> _parseVehicleRoutes(Map<String, dynamic> data) {
    // Detect which format we have
    if (data.containsKey('vehicles') && data['vehicles'] is List) {
      return _parseFromSolverFormat(data);
    } else if (data.containsKey('routes') && data['routes'] is List) {
      return _parseFromBackendFormat(data);
    }
    return [];
  }

  /// Parse Format A: raw solver output with vehicles → trips → stops.
  List<_VehicleRoute> _parseFromSolverFormat(Map<String, dynamic> data) {
    final List vehicles = data['vehicles'] ?? [];
    final List<_VehicleRoute> routes = [];

    for (int i = 0; i < vehicles.length; i++) {
      final veh = vehicles[i] as Map<String, dynamic>;
      final vehicleId = veh['vehicle_id']?.toString() ?? 'V${i + 1}';
      final totalCost = (veh['total_cost'] as num?)?.toDouble() ?? 0;
      final totalDist = (veh['total_distance'] as num?)?.toDouble() ?? 0;
      final totalTime = (veh['total_time'] as num?)?.toInt() ?? 0;
      final color = _routePalette[i % _routePalette.length];

      final List trips = veh['trips'] ?? [];
      final List<_TripInfo> parsedTrips = [];
      final List<_StopInfo> allStops = [];

      for (var trip in trips) {
        final tripNum = (trip['trip_number'] as num?)?.toInt() ?? 0;
        final tripCost = (trip['total_cost'] as num?)?.toDouble() ?? 0;
        final tripDist = (trip['total_distance'] as num?)?.toDouble() ?? 0;
        final tripTime = (trip['total_time'] as num?)?.toInt() ?? 0;
        final List stops = trip['stops'] ?? [];
        final List<_StopInfo> tripStops = [];

        for (var stop in stops) {
          final loc = stop['location']?.toString() ?? '';
          // Extract employee ID from location like "E01 Pickup"
          String? empId;
          final pickupMatch = RegExp(r'^(E\d+)\s').firstMatch(loc);
          if (pickupMatch != null) {
            empId = pickupMatch.group(1);
          }

          tripStops.add(
            _StopInfo(
              location: loc,
              arrivalTime: stop['arrival_time']?.toString() ?? '',
              departureTime: stop['departure_time']?.toString() ?? '',
              distanceFromPrev:
                  (stop['distance_from_prev'] as num?)?.toDouble() ?? 0,
              waitTime: (stop['wait_time'] as num?)?.toInt() ?? 0,
              lat: null,
              lng: null,
              employeeId: empId,
            ),
          );
        }

        // Skip phantom trips: a trip whose every stop is office/depot and
        // which has 0 distance — these are solver artefacts where the vehicle
        // just sits at the office between runs.
        final isPhantom =
            tripDist == 0 &&
            tripStops.isNotEmpty &&
            tripStops.every((s) {
              final l = s.location.toLowerCase();
              return l.contains('office') ||
                  l.contains('drop') ||
                  l.contains('depot');
            });

        if (!isPhantom) {
          parsedTrips.add(
            _TripInfo(
              tripNumber: tripNum,
              totalCost: tripCost,
              totalDistance: tripDist,
              totalTime: tripTime,
              stops: tripStops,
            ),
          );
        }
        allStops.addAll(tripStops);
      }

      // Re-number trips sequentially after phantom filtering
      for (int n = 0; n < parsedTrips.length; n++) {
        parsedTrips[n] = _TripInfo(
          tripNumber: n + 1,
          totalCost: parsedTrips[n].totalCost,
          totalDistance: parsedTrips[n].totalDistance,
          totalTime: parsedTrips[n].totalTime,
          stops: parsedTrips[n].stops,
        );
      }

      // Prepend office departure stop to each trip after the first
      final finalTripsA = _addOfficeCarryOvers(parsedTrips);

      routes.add(
        _VehicleRoute(
          vehicleId: vehicleId,
          totalCost: totalCost,
          totalDistance: totalDist,
          totalTime: totalTime,
          color: color,
          trips: finalTripsA,
          allStops: allStops,
        ),
      );
    }

    return routes;
  }

  /// Parse Format B: backend API response with routes + assignments.
  /// route_points already carry arrival_time, departure_time, distance_from_prev
  /// and lat/lng directly — use them instead of secondary lookups.
  List<_VehicleRoute> _parseFromBackendFormat(Map<String, dynamic> data) {
    final List routesList = data['routes'] ?? [];
    final List<_VehicleRoute> routes = [];

    for (int i = 0; i < routesList.length; i++) {
      final r = routesList[i] as Map<String, dynamic>;
      final vehicleId = r['vehicle_id']?.toString() ?? 'V${i + 1}';
      final totalCost = (r['total_cost'] as num?)?.toDouble() ?? 0;
      final totalDist = (r['total_distance'] as num?)?.toDouble() ?? 0;
      final tripsCount = (r['trips_count'] as num?)?.toInt() ?? 1;
      final color = _routePalette[i % _routePalette.length];

      // Build stops from route_points using data already embedded in each point
      final List routePoints = r['route_points'] ?? [];
      final List<_StopInfo> allStops = [];

      for (var pt in routePoints) {
        final type = pt['type']?.toString() ?? '';
        final empId = pt['employee_id']?.toString() ?? '';

        String location;
        if (type == 'pickup') {
          location = '$empId Pickup';
        } else if (type == 'office') {
          location = 'Office (Drop-off)';
        } else if (type == 'depot') {
          location = 'Vehicle Depot';
        } else {
          location = type.isNotEmpty ? type : 'Stop';
        }

        // Use times and distance directly from route_point — they are correct
        final arrTime = pt['arrival_time']?.toString() ?? '';
        final depTime = pt['departure_time']?.toString() ?? '';
        final distPrev = (pt['distance_from_prev'] as num?)?.toDouble() ?? 0.0;

        allStops.add(
          _StopInfo(
            location: location,
            arrivalTime: arrTime,
            departureTime: depTime,
            distanceFromPrev: distPrev,
            waitTime: 0, // not available in route_points
            lat: (pt['lat'] as num?)?.toDouble(),
            lng: (pt['lng'] as num?)?.toDouble(),
            employeeId: (type == 'pickup' && empId.isNotEmpty) ? empId : null,
          ),
        );
      }

      // Group stops into trips by splitting at every office drop-off.
      // Also tally per-trip distance from distanceFromPrev so each card
      // shows accurate km and proportional cost.
      final List<_TripInfo> parsedTrips = [];
      List<_StopInfo> currentTripStops = [];
      int tripNum = 1;

      for (var stop in allStops) {
        currentTripStops.add(stop);
        if (stop.location.contains('Drop-off') ||
            stop.location.contains('Office')) {
          // sum distance from the stops in this trip
          final tripDist = currentTripStops.fold(
            0.0,
            (sum, s) => sum + s.distanceFromPrev,
          );
          // Cost proportional to distance share
          final tripCost = totalDist > 0
              ? totalCost * (tripDist / totalDist)
              : 0.0;
          parsedTrips.add(
            _TripInfo(
              tripNumber: tripNum,
              totalCost: tripCost,
              totalDistance: tripDist,
              totalTime: 0, // not available in Format B
              stops: List.from(currentTripStops),
            ),
          );
          currentTripStops = [];
          tripNum++;
        }
      }
      // Remaining stops that didn't end with a drop-off
      if (currentTripStops.isNotEmpty) {
        final tripDist = currentTripStops.fold(
          0.0,
          (sum, s) => sum + s.distanceFromPrev,
        );
        parsedTrips.add(
          _TripInfo(
            tripNumber: tripNum,
            totalCost: totalDist > 0 ? totalCost * (tripDist / totalDist) : 0.0,
            totalDistance: tripDist,
            totalTime: 0,
            stops: currentTripStops,
          ),
        );
      }

      // Use tripsCount from backend if parsedTrips is empty
      // (vehicle in solution but no route_points means unassigned)
      // Remove any solo office/depot trips produced by Format B grouping
      // (e.g. a trip that starts and ends with just an office stop at 0 km).
      final cleanedTrips = parsedTrips.where((t) {
        if (t.stops.isEmpty) return false;
        if (t.stops.length == 1) {
          final l = t.stops.first.location.toLowerCase();
          return !l.contains('office') &&
              !l.contains('drop') &&
              !l.contains('depot');
        }
        return true;
      }).toList();
      // Re-number trips after filtering
      for (int n = 0; n < cleanedTrips.length; n++) {
        cleanedTrips[n] = _TripInfo(
          tripNumber: n + 1,
          totalCost: cleanedTrips[n].totalCost,
          totalDistance: cleanedTrips[n].totalDistance,
          totalTime: cleanedTrips[n].totalTime,
          stops: cleanedTrips[n].stops,
        );
      }

      // Prepend office departure stop to each trip after the first
      final finalTripsB = _addOfficeCarryOvers(cleanedTrips);

      routes.add(
        _VehicleRoute(
          vehicleId: vehicleId,
          totalCost: totalCost,
          totalDistance: totalDist,
          totalTime: 0,
          color: color,
          trips: finalTripsB,
          allStops: allStops,
        ),
      );
    }

    return routes;
  }

  /// Prepends the office/depot departure from the previous trip as the
  /// first stop of every subsequent trip, so each trip reads:
  ///   Office (Departure) → Pickup(s) → Office (Drop-off)
  List<_TripInfo> _addOfficeCarryOvers(List<_TripInfo> trips) {
    for (int t = 1; t < trips.length; t++) {
      final prevStops = trips[t - 1].stops;
      if (prevStops.isEmpty) continue;
      final prevLast = prevStops.last;
      final l = prevLast.location.toLowerCase();
      if (!l.contains('office') &&
          !l.contains('drop') &&
          !l.contains('depot')) {
        continue;
      }
      // Build the departure stop from the previous trip's last stop
      final departureStop = _StopInfo(
        location: 'Office (Departure)',
        arrivalTime: prevLast.departureTime.isNotEmpty
            ? prevLast.departureTime
            : prevLast.arrivalTime,
        departureTime: prevLast.departureTime,
        distanceFromPrev: 0,
        waitTime: 0,
        lat: prevLast.lat,
        lng: prevLast.lng,
        employeeId: null,
      );
      trips[t] = _TripInfo(
        tripNumber: trips[t].tripNumber,
        totalCost: trips[t].totalCost,
        totalDistance: trips[t].totalDistance,
        totalTime: trips[t].totalTime,
        stops: [departureStop, ...trips[t].stops],
      );
    }
    return trips;
  }

  /// Parse time string "HH:MM" → minutes since midnight.
  int _timeToMinutes(String t) {
    final parts = t.split(':');
    if (parts.length != 2) return 0;
    return (int.tryParse(parts[0]) ?? 0) * 60 + (int.tryParse(parts[1]) ?? 0);
  }

  /// Compute map center from all stop lat/lng or fallback to default.
  LatLng _computeCenter() {
    final allPoints = <LatLng>[];
    for (var route in _vehicleRoutes) {
      for (var stop in route.allStops) {
        if (stop.lat != null &&
            stop.lng != null &&
            stop.lat != 0 &&
            stop.lng != 0) {
          allPoints.add(LatLng(stop.lat!, stop.lng!));
        }
      }
    }
    if (allPoints.isEmpty) {
      return const LatLng(MapConfig.defaultLat, MapConfig.defaultLng);
    }
    double latSum = 0, lngSum = 0;
    for (var p in allPoints) {
      latSum += p.latitude;
      lngSum += p.longitude;
    }
    return LatLng(latSum / allPoints.length, lngSum / allPoints.length);
  }

  /// Compute the global time range across all routes.
  ({int startMin, int endMin}) _globalTimeRange() {
    int earliest = 24 * 60;
    int latest = 0;
    for (var route in _vehicleRoutes) {
      for (var stop in route.allStops) {
        final arr = _timeToMinutes(stop.arrivalTime);
        final dep = _timeToMinutes(stop.departureTime);
        if (arr < earliest) earliest = arr;
        if (dep > latest) latest = dep;
        if (arr > latest) latest = arr;
      }
    }
    if (earliest >= latest) {
      earliest = 8 * 60;
      latest = 12 * 60;
    }
    return (startMin: earliest, endMin: latest);
  }

  String _minutesToTime(int m) {
    final h = (m ~/ 60).toString().padLeft(2, '0');
    final min = (m % 60).toString().padLeft(2, '0');
    return '$h:$min';
  }

  // ══════════════════════════════════════════════════════════
  //  TIME-BASED ANIMATION HELPERS
  // ══════════════════════════════════════════════════════════

  /// Returns the list of stops aligned with route points.
  /// For real-coord routes, only stops that have lat/lng are included.
  List<_StopInfo> _getAlignedStops(_VehicleRoute route) {
    final hasRealCoords = route.allStops.any(
      (s) => s.lat != null && s.lng != null,
    );
    if (hasRealCoords) {
      return route.allStops
          .where((s) => s.lat != null && s.lng != null)
          .toList();
    }
    return route.allStops;
  }

  /// Computes how far (0.0–1.0) through the route the vehicle is at
  /// [currentTimeMin], using per-stop arrival/departure times.
  /// Falls back to global [_playbackProgress] when time data is absent.
  double _routeProgressAtTime(double currentTimeMin, List<_StopInfo> stops) {
    if (stops.length < 2) return _playbackProgress;
    final firstArr = _timeToMinutes(stops.first.arrivalTime).toDouble();
    final lastArr = _timeToMinutes(stops.last.arrivalTime).toDouble();
    // Guard: unusable time data
    if (lastArr <= firstArr) return _playbackProgress;

    if (currentTimeMin <= firstArr) return 0.0;
    if (currentTimeMin >= lastArr) return 1.0;

    final n = stops.length - 1;
    for (int s = 0; s < n; s++) {
      final arrS = _timeToMinutes(stops[s].arrivalTime).toDouble();
      final depS =
          stops[s].departureTime.isNotEmpty &&
              _timeToMinutes(stops[s].departureTime) > 0
          ? _timeToMinutes(stops[s].departureTime).toDouble()
          : arrS;
      final arrNext = _timeToMinutes(stops[s + 1].arrivalTime).toDouble();

      // Vehicle is dwelling at stop s
      if (currentTimeMin <= depS) return s / n;
      // Vehicle is travelling to stop s+1
      if (currentTimeMin <= arrNext) {
        final travelTime = arrNext - depS;
        final frac = travelTime > 0
            ? (currentTimeMin - depS) / travelTime
            : 1.0;
        return (s + frac.clamp(0.0, 1.0)) / n;
      }
    }
    return 1.0;
  }

  /// Returns the interpolated [LatLng] position for a vehicle on [stops]/[points]
  /// at [currentTimeMin], honouring dwell times between arrival and departure.
  LatLng _interpolateVehiclePosition(
    double currentTimeMin,
    List<_StopInfo> stops,
    List<LatLng> points,
  ) {
    assert(stops.length == points.length);
    if (stops.isEmpty) return points.first;

    final firstArr = _timeToMinutes(stops.first.arrivalTime).toDouble();
    final lastArr = _timeToMinutes(stops.last.arrivalTime).toDouble();

    if (currentTimeMin <= firstArr) return points.first;
    if (currentTimeMin >= lastArr) return points.last;

    for (int s = 0; s < stops.length - 1; s++) {
      if (s >= points.length - 1) break;
      final arrS = _timeToMinutes(stops[s].arrivalTime).toDouble();
      final depS =
          stops[s].departureTime.isNotEmpty &&
              _timeToMinutes(stops[s].departureTime) > 0
          ? _timeToMinutes(stops[s].departureTime).toDouble()
          : arrS;
      final arrNext = _timeToMinutes(stops[s + 1].arrivalTime).toDouble();

      // Dwelling at stop
      if (currentTimeMin >= arrS && currentTimeMin <= depS) return points[s];
      // Travelling to next stop
      if (currentTimeMin > depS && currentTimeMin <= arrNext) {
        final travelTime = arrNext - depS;
        final frac = travelTime > 0
            ? ((currentTimeMin - depS) / travelTime).clamp(0.0, 1.0)
            : 1.0;
        return LatLng(
          points[s].latitude +
              (points[s + 1].latitude - points[s].latitude) * frac,
          points[s].longitude +
              (points[s + 1].longitude - points[s].longitude) * frac,
        );
      }
    }
    return points.last;
  }

  // ══════════════════════════════════════════════════════════
  //  PLAYBACK
  // ══════════════════════════════════════════════════════════
  void _togglePlayback() {
    if (_isPlaying) {
      _playbackController.stop();
    } else {
      if (_playbackController.value >= 1.0) {
        _playbackController.reset();
      }
      _playbackController.forward();
    }
    setState(() => _isPlaying = !_isPlaying);
  }

  void _cycleSpeed() {
    setState(() {
      _speedIndex = (_speedIndex + 1) % _speedOptions.length;
      final currentValue = _playbackController.value;
      final baseDuration = 15; // seconds at x1
      final newDuration = Duration(
        milliseconds: (baseDuration * 1000 / _playbackSpeed).round(),
      );
      _playbackController.duration = newDuration;
      // Restart from current position if playing
      if (_isPlaying) {
        _playbackController.forward(from: currentValue);
      }
    });
  }

  void _onScrub(double value) {
    _playbackController.value = value;
    setState(() {
      _playbackProgress = value;
      if (_isPlaying) {
        _playbackController.stop();
        _isPlaying = false;
      }
    });
  }

  void _focusVehicle(int index) {
    setState(() {
      _focusedVehicleIndex = _focusedVehicleIndex == index ? null : index;
    });
  }

  // ══════════════════════════════════════════════════════════
  //  FLEET FILTER (mirrors ShowInputPage vehicle filter)
  // ══════════════════════════════════════════════════════════
  List<_VehicleRoute> get _filteredRoutes {
    var list = List<_VehicleRoute>.from(_vehicleRoutes);

    if (_filterVehicleId != null) {
      list = list.where((r) => r.vehicleId == _filterVehicleId).toList();
    }

    if (_vehicleSearchQuery.isNotEmpty) {
      final q = _vehicleSearchQuery.toLowerCase();
      list = list.where((r) => r.vehicleId.toLowerCase().contains(q)).toList();
    }

    if (_sortByCost != null) {
      list.sort(
        (a, b) => _sortByCost!
            ? a.totalCost.compareTo(b.totalCost)
            : b.totalCost.compareTo(a.totalCost),
      );
    }

    if (_sortByDistance != null) {
      list.sort(
        (a, b) => _sortByDistance!
            ? a.totalDistance.compareTo(b.totalDistance)
            : b.totalDistance.compareTo(a.totalDistance),
      );
    }

    return list;
  }

  bool get _hasActiveFleetFilters =>
      _filterVehicleId != null ||
      _sortByCost != null ||
      _sortByDistance != null ||
      _vehicleSearchQuery.isNotEmpty;

  void _clearFleetFilters() {
    setState(() {
      _filterVehicleId = null;
      _sortByCost = null;
      _sortByDistance = null;
      _vehicleSearchQuery = '';
    });
  }

  /// Get drop-off (office arrival) time for a vehicle route.
  String _getDropOffTime(_VehicleRoute route) {
    for (var trip in route.trips) {
      for (var stop in trip.stops) {
        if (stop.location.contains('Drop-off') ||
            stop.location.contains('Office')) {
          if (stop.arrivalTime.isNotEmpty) return stop.arrivalTime;
        }
      }
    }
    return '--:--';
  }

  // ══════════════════════════════════════════════════════════
  //  BUSINESS LOGIC
  // ══════════════════════════════════════════════════════════
  Future<void> _handleRetry() async {
    setState(() => _isLoading = true);
    try {
      // Fetch the input JSON data from Supabase
      final inputData = await _dataService.fetchInputData(widget.testCaseId);

      if (inputData == null) {
        throw Exception(
          "Input data not found. Please re-upload the test case.",
        );
      }

      // Run optimization by sending JSON directly to backend
      final newData = await _optimizationService.runOptimization(inputData);
      await _dataService.saveSolution(widget.testCaseId, newData);

      if (mounted) {
        setState(() {
          _currentData = newData;
          _vehicleRoutes = _parseVehicleRoutes(newData);
          _playbackController.reset();
          _isPlaying = false;
          _playbackProgress = 0.0;
          _focusedVehicleIndex = null;
        });
        AppSnackbar.show(context, message: "Result Updated Successfully!");
      }
    } catch (e) {
      if (mounted) {
        AppSnackbar.show(context, message: e.toString(), isError: true);
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  void _openDownloadDialog() {
    showModalBottomSheet(
      context: context,
      backgroundColor: Theme.of(context).scaffoldBackgroundColor,
      useSafeArea: true,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) =>
          DownloadDialog(onSelect: (type) => _handleDownload(type)),
    );
  }

  Future<void> _handleDownload(String type) async {
    setState(() {
      _isLoading = true;
      _isDownloading = true;
    });
    await Future.delayed(const Duration(milliseconds: 300));

    await _fileExportService.exportFile(
      context,
      type,
      widget.testCaseName,
      _currentData,
    );

    if (mounted) {
      setState(() {
        _isLoading = false;
        _isDownloading = false;
      });
    }
  }

  // ══════════════════════════════════════════════════════════
  //  BUILD
  // ══════════════════════════════════════════════════════════
  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    final isDesktop = size.width > 800;

    return Scaffold(
      backgroundColor: _bgColor(context),
      appBar: _buildAppBar(context),
      body: LoadingOverlay(
        isLoading: _isLoading,
        spinnerType: _isDownloading
            ? SpinnerType.downloading
            : SpinnerType.loading,
        child: isDesktop
            ? _buildDesktopLayout(context)
            : _buildMobileLayout(context, size),
      ),
    );
  }

  // ── AppBar ─────────────────────────────────────────────
  /// Derive a human-readable solution type from the stored data.
  /// Format B (backend) has no top-level solution_type — derive from violations.
  String get _solutionType {
    // Format B: result.hard_violations / soft_violations
    final result = _currentData['result'] as Map<String, dynamic>?;
    if (result != null) {
      final hard = (result['hard_violations'] as num?)?.toInt() ?? 0;
      final soft = (result['soft_violations'] as num?)?.toInt() ?? 0;
      if (hard == 0 && soft == 0) return 'OPTIMAL - No violations';
      if (hard == 0)
        return 'Feasible - $soft soft violation${soft > 1 ? 's' : ''}';
      return 'Infeasible - $hard hard violation${hard > 1 ? 's' : ''}';
    }
    // Format A: top-level solution_type
    return _currentData['solution_type']?.toString() ?? 'Completed';
  }

  PreferredSizeWidget _buildAppBar(BuildContext context) {
    final dark = _isDark(context);
    final solutionType = _solutionType;

    return AppBar(
      backgroundColor: _bgColor(context),
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      centerTitle: false,
      leading: IconButton(
        icon: Icon(
          Icons.arrow_back_ios_new_rounded,
          color: dark ? Colors.white : AppColors.textPrimaryLight,
          size: 20,
        ),
        onPressed: () => Navigator.pop(context),
      ),
      title: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            widget.testCaseName,
            style: TextStyle(
              color: _textPrimary(context),
              fontSize: 18,
              fontWeight: FontWeight.w700,
            ),
          ),
          Text(
            solutionType,
            style: TextStyle(
              color: solutionType.toLowerCase().contains('optimal')
                  ? AppColors.primaryBrand
                  : AppColors.warning,
              fontSize: 12,
              fontWeight: FontWeight.w500,
            ),
          ),
        ],
      ),
      actions: [
        Padding(
          padding: const EdgeInsets.only(right: 8),
          child: Material(
            color: Colors.transparent,
            child: InkWell(
              borderRadius: BorderRadius.circular(12),
              onTap: _openDownloadDialog,
              child: Container(
                padding: const EdgeInsets.symmetric(
                  horizontal: 12,
                  vertical: 8,
                ),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(
                    color: AppColors.primaryBrand.withOpacity(0.5),
                  ),
                  color: AppColors.primaryBrand.withOpacity(0.08),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(
                      Icons.download_rounded,
                      size: 18,
                      color: AppColors.primaryBrand,
                    ),
                    const SizedBox(width: 6),
                    Text(
                      'Export',
                      style: TextStyle(
                        color: AppColors.primaryBrand,
                        fontSize: 12,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  MOBILE LAYOUT: Full-screen map + draggable sheet (player + summary) + fixed action bar
  // ══════════════════════════════════════════════════════════
  Widget _buildMobileLayout(BuildContext context, Size size) {
    final actionBarHeight = 72.0 + MediaQuery.of(context).padding.bottom;

    return Stack(
      children: [
        // ── Full-screen map ──
        Positioned.fill(
          child: Padding(
            padding: EdgeInsets.only(bottom: actionBarHeight),
            child: _buildMap(context),
          ),
        ),

        // ── Map buttons (north + recenter) ──
        Positioned(
          top: 12,
          right: 12,
          child: Column(
            children: [
              _mapButton(
                context,
                icon: Icons.navigation_rounded,
                onPressed: () {
                  if (_mapReady) _mapController.rotate(0);
                },
              ),
              const SizedBox(height: 8),
              _mapButton(
                context,
                icon: Icons.my_location_rounded,
                onPressed: () {
                  if (_mapReady) {
                    _mapController.move(
                      _computeCenter(),
                      MapConfig.defaultZoom,
                    );
                  }
                },
              ),
            ],
          ),
        ),

        // ── Draggable sheet (player + summary move together) ──
        Positioned.fill(
          bottom: actionBarHeight,
          child: DraggableScrollableSheet(
            initialChildSize: 0.25,
            minChildSize: 0.22,
            maxChildSize: 0.85,
            snap: true,
            snapSizes: const [0.22, 0.45, 0.85],
            builder: (context, scrollController) {
              return Container(
                decoration: BoxDecoration(
                  color: _bgColor(context),
                  borderRadius: const BorderRadius.vertical(
                    top: Radius.circular(20),
                  ),
                  boxShadow: [
                    BoxShadow(
                      color: Colors.black.withOpacity(0.25),
                      blurRadius: 12,
                      offset: const Offset(0, -3),
                    ),
                  ],
                ),
                child: CustomScrollView(
                  controller: scrollController,
                  slivers: [
                    // Drag handle
                    SliverToBoxAdapter(
                      child: Padding(
                        padding: const EdgeInsets.symmetric(vertical: 8),
                        child: Center(
                          child: Container(
                            width: 40,
                            height: 4,
                            decoration: BoxDecoration(
                              color: _textTertiary(context),
                              borderRadius: BorderRadius.circular(2),
                            ),
                          ),
                        ),
                      ),
                    ),

                    // Playback controls (moves with the sheet)
                    SliverToBoxAdapter(
                      child: Padding(
                        padding: const EdgeInsets.fromLTRB(12, 0, 12, 8),
                        child: _buildPlaybackControls(context),
                      ),
                    ),

                    // Scrollable summary + filter + vehicle cards
                    ..._buildScrollableSlivers(context),
                  ],
                ),
              );
            },
          ),
        ),

        // ── Fixed bottom action bar ──
        Positioned(
          left: 0,
          right: 0,
          bottom: 0,
          child: _buildActionBar(context),
        ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  DESKTOP LAYOUT: Left 60% Map, Right 40% Panel
  // ══════════════════════════════════════════════════════════
  Widget _buildDesktopLayout(BuildContext context) {
    return Row(
      children: [
        // ── Left: Map ──
        Expanded(
          flex: 6,
          child: Stack(
            children: [
              Positioned.fill(child: _buildMap(context)),

              Positioned(
                bottom: 16,
                left: 16,
                right: 16,
                child: _buildPlaybackControls(context),
              ),

              Positioned(
                top: 16,
                right: 16,
                child: Column(
                  children: [
                    _mapButton(
                      context,
                      icon: Icons.navigation_rounded,
                      onPressed: () {
                        if (_mapReady) _mapController.rotate(0);
                      },
                    ),
                    const SizedBox(height: 8),
                    _mapButton(
                      context,
                      icon: Icons.my_location_rounded,
                      onPressed: () {
                        if (_mapReady) {
                          _mapController.move(
                            _computeCenter(),
                            MapConfig.defaultZoom,
                          );
                        }
                      },
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),

        // ── Right: Results Panel ──
        Expanded(
          flex: 4,
          child: Container(
            color: _bgColor(context),
            child: _buildResultsPanel(context),
          ),
        ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  MAP WIDGET
  // ══════════════════════════════════════════════════════════
  Widget _buildMap(BuildContext context) {
    final isDarkMode = _isDark(context);
    final staticMarkers = _buildStaticMarkers(context);
    final vehicleMarkers = _buildAnimatedVehicleMarkers();
    final polylines = _buildPolylines();

    return FlutterMap(
      mapController: _mapController,
      options: MapOptions(
        initialCenter: _computeCenter(),
        initialZoom: MapConfig.defaultZoom,
        onMapReady: () => _mapReady = true,
        interactionOptions: const InteractionOptions(
          flags: InteractiveFlag.all,
        ),
      ),
      children: [
        ColorFiltered(
          colorFilter: isDarkMode
              ? const ColorFilter.matrix(_darkModeMatrix)
              : const ColorFilter.mode(
                  Colors.transparent,
                  BlendMode.saturation,
                ),
          child: TileLayer(
            urlTemplate: MapConfig.tileUrlTemplate,
            userAgentPackageName: MapConfig.packageName,
          ),
        ),
        PolylineLayer(polylines: polylines),
        // Static stop/office markers — rendered first (below)
        MarkerLayer(markers: staticMarkers),
        // Animated vehicle icons — always on top
        MarkerLayer(markers: vehicleMarkers),
      ],
    );
  }

  /// Build polylines visualizing each vehicle's route path.
  /// Uses simulated waypoints radiating from center since solution
  /// data contains stop names but not lat/lng coordinates.
  List<Polyline> _buildPolylines() {
    final List<Polyline> polylines = [];
    final center = _computeCenter();
    final timeRange = _globalTimeRange();
    final currentTimeMin =
        (timeRange.startMin +
                (timeRange.endMin - timeRange.startMin) * _playbackProgress)
            .toDouble();

    for (int i = 0; i < _vehicleRoutes.length; i++) {
      if (_focusedVehicleIndex != null && _focusedVehicleIndex != i) continue;

      final route = _vehicleRoutes[i];
      final points = _generateRoutePoints(center, i, route);

      if (points.length >= 2) {
        // Use time-based progress so the drawn trail matches real stop times
        final alignedStops = _getAlignedStops(route);
        final hasTimedData =
            alignedStops.isNotEmpty &&
            alignedStops.first.arrivalTime.isNotEmpty &&
            _timeToMinutes(alignedStops.first.arrivalTime) > 0;

        final routeProgress = hasTimedData
            ? _routeProgressAtTime(currentTimeMin, alignedStops)
            : _playbackProgress;

        final totalSegments = points.length - 1;
        final segmentsToDraw = (totalSegments * routeProgress).ceil().clamp(
          0,
          totalSegments,
        );

        if (segmentsToDraw > 0) {
          // Animated (drawn) portion
          polylines.add(
            Polyline(
              points: points.sublist(0, segmentsToDraw + 1),
              strokeWidth: 4.0,
              color: route.color,
            ),
          );
        }

        // Ghost (undrawn) portion
        polylines.add(
          Polyline(
            points: points,
            strokeWidth: 2.0,
            color: route.color.withOpacity(0.2),
            pattern: const StrokePattern.dotted(),
          ),
        );
      }
    }

    return polylines;
  }

  /// Generate map points for a vehicle's route.
  /// Uses actual lat/lng from stops when available (backend format),
  /// otherwise creates a deterministic fan of points around center.
  List<LatLng> _generateRoutePoints(
    LatLng center,
    int vehicleIdx,
    _VehicleRoute route,
  ) {
    final List<LatLng> points = [];
    final totalStops = route.allStops.length;
    if (totalStops == 0) return points;

    // Check if stops have actual coordinates (backend format)
    final hasRealCoords = route.allStops.any(
      (s) => s.lat != null && s.lng != null,
    );

    if (hasRealCoords) {
      // Use actual coordinates from route_points
      for (final stop in route.allStops) {
        if (stop.lat != null && stop.lng != null) {
          points.add(LatLng(stop.lat!, stop.lng!));
        }
      }
    } else {
      // Fallback: simulate fan of points around center (solver format)
      final baseAngle = (vehicleIdx * 2 * pi / max(_vehicleRoutes.length, 1));
      final random = Random(vehicleIdx * 42); // deterministic seed

      for (int s = 0; s < totalStops; s++) {
        final fraction = s / max(totalStops - 1, 1);
        final angle = baseAngle + (fraction - 0.5) * 0.6;
        final dist = fraction * 0.025 * (1 + random.nextDouble() * 0.5);

        points.add(
          LatLng(
            center.latitude + dist * cos(angle),
            center.longitude + dist * sin(angle),
          ),
        );
      }
    }

    return points;
  }

  /// Build static markers (office/stop/employee) for each route.
  /// These are rendered BELOW the animated vehicle layer.
  List<Marker> _buildStaticMarkers(BuildContext context) {
    final List<Marker> markers = [];
    final center = _computeCenter();

    for (int i = 0; i < _vehicleRoutes.length; i++) {
      if (_focusedVehicleIndex != null && _focusedVehicleIndex != i) continue;

      final route = _vehicleRoutes[i];
      final hasRealCoords = route.allStops.any(
        (s) => s.lat != null && s.lng != null,
      );

      // Build stop-to-point mapping, consistent with _generateRoutePoints
      final List<MapEntry<_StopInfo, LatLng>> stopPoints = [];
      if (hasRealCoords) {
        for (final stop in route.allStops) {
          if (stop.lat != null && stop.lng != null) {
            stopPoints.add(MapEntry(stop, LatLng(stop.lat!, stop.lng!)));
          }
        }
      } else {
        final points = _generateRoutePoints(center, i, route);
        for (int s = 0; s < min(points.length, route.allStops.length); s++) {
          stopPoints.add(MapEntry(route.allStops[s], points[s]));
        }
      }

      if (stopPoints.isEmpty) continue;

      // First stop
      final firstStop = stopPoints.first.key;
      final firstLoc = firstStop.location.toLowerCase();
      final firstIsOffice =
          firstLoc.contains('office') ||
          firstLoc.contains('depot') ||
          firstLoc.contains('drop') ||
          firstLoc.contains('company');
      if (firstIsOffice) {
        markers.add(
          Marker(
            point: stopPoints.first.value,
            width: 40,
            height: 40,
            child: _OfficeMarkerWidget(),
          ),
        );
      } else {
        markers.add(
          Marker(
            point: stopPoints.first.value,
            width: 64,
            height: 36,
            child: _OutputVehicleMarker(label: 'V${i + 1}', color: route.color),
          ),
        );
      }

      // Intermediate stops
      for (int s = 1; s < stopPoints.length; s++) {
        final stop = stopPoints[s].key;
        final loc = stop.location.toLowerCase();
        final isDropoff =
            loc.contains('drop') ||
            loc.contains('office') ||
            loc.contains('depot') ||
            loc.contains('company');

        if (isDropoff) {
          markers.add(
            Marker(
              point: stopPoints[s].value,
              width: 40,
              height: 40,
              child: _OfficeMarkerWidget(),
            ),
          );
        } else {
          markers.add(
            Marker(
              point: stopPoints[s].value,
              width: 64,
              height: 36,
              child: _EmployeeMarkerWidget(
                label: stop.employeeId ?? '',
                color: route.color,
              ),
            ),
          );
        }
      }
    }

    return markers;
  }

  /// Build only the animated vehicle position markers.
  /// Rendered in a separate MarkerLayer ABOVE static markers so vehicles
  /// are never hidden behind office/stop markers during playback.
  /// Vehicle position is time-accurate: it uses each stop's arrival/departure
  /// time so the vehicle reaches the office exactly at the scheduled time.
  List<Marker> _buildAnimatedVehicleMarkers() {
    final List<Marker> markers = [];
    final center = _computeCenter();
    final timeRange = _globalTimeRange();
    final currentTimeMin =
        (timeRange.startMin +
                (timeRange.endMin - timeRange.startMin) * _playbackProgress)
            .toDouble();

    for (int i = 0; i < _vehicleRoutes.length; i++) {
      if (_focusedVehicleIndex != null && _focusedVehicleIndex != i) continue;

      final route = _vehicleRoutes[i];
      final hasRealCoords = route.allStops.any(
        (s) => s.lat != null && s.lng != null,
      );

      // Build aligned (stop info, point) pairs so time lookup and position
      // are always in sync.
      final List<_StopInfo> alignedStops = [];
      final List<LatLng> allPoints = [];
      if (hasRealCoords) {
        for (final stop in route.allStops) {
          if (stop.lat != null && stop.lng != null) {
            alignedStops.add(stop);
            allPoints.add(LatLng(stop.lat!, stop.lng!));
          }
        }
      } else {
        alignedStops.addAll(route.allStops);
        allPoints.addAll(_generateRoutePoints(center, i, route));
      }

      if (allPoints.length < 2) continue;

      // Check if usable time data is present
      final hasTimedData =
          alignedStops.isNotEmpty &&
          alignedStops.first.arrivalTime.isNotEmpty &&
          _timeToMinutes(alignedStops.first.arrivalTime) > 0;

      final LatLng vehiclePos;
      if (hasTimedData) {
        // Time-accurate: vehicle arrives at each stop exactly on schedule
        vehiclePos = _interpolateVehiclePosition(
          currentTimeMin,
          alignedStops,
          allPoints,
        );
      } else {
        // Fallback: linear distribution when no time data is available
        final totalSegments = allPoints.length - 1;
        final exactPos = _playbackProgress * totalSegments;
        final segIdx = exactPos.floor().clamp(0, totalSegments - 1);
        final segFrac = exactPos - segIdx;
        final from = allPoints[segIdx];
        final to = allPoints[min(segIdx + 1, allPoints.length - 1)];
        vehiclePos = LatLng(
          from.latitude + (to.latitude - from.latitude) * segFrac,
          from.longitude + (to.longitude - from.longitude) * segFrac,
        );
      }

      markers.add(
        Marker(
          point: vehiclePos,
          width: 64,
          height: 42,
          child: _AnimatedVehicleMarker(color: route.color, label: 'V${i + 1}'),
        ),
      );
    }

    return markers;
  }

  Widget _mapButton(
    BuildContext context, {
    required IconData icon,
    required VoidCallback onPressed,
  }) {
    final isDark = _isDark(context);
    return Material(
      elevation: 3,
      borderRadius: BorderRadius.circular(12),
      color: isDark ? AppColors.darkSurface : Colors.white,
      child: InkWell(
        borderRadius: BorderRadius.circular(12),
        onTap: onPressed,
        child: SizedBox(
          width: 42,
          height: 42,
          child: Icon(icon, color: AppColors.primaryBrand, size: 20),
        ),
      ),
    );
  }

  // ══════════════════════════════════════════════════════════
  //  GLASSMORPHISM PLAYBACK CONTROLS
  // ══════════════════════════════════════════════════════════
  Widget _buildPlaybackControls(BuildContext context) {
    final timeRange = _globalTimeRange();
    final currentMin =
        (timeRange.startMin +
                (timeRange.endMin - timeRange.startMin) * _playbackProgress)
            .round();

    return ClipRRect(
      borderRadius: BorderRadius.circular(16),
      child: BackdropFilter(
        filter: ui.ImageFilter.blur(sigmaX: 20, sigmaY: 20),
        child: Container(
          padding: const EdgeInsets.fromLTRB(16, 10, 16, 12),
          decoration: BoxDecoration(
            color: (_isDark(context) ? Colors.black : Colors.white).withOpacity(
              0.55,
            ),
            borderRadius: BorderRadius.circular(16),
            border: Border.all(
              color: (_isDark(context) ? Colors.white : Colors.black)
                  .withOpacity(0.1),
            ),
          ),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              // Time label + Play button
              Row(
                children: [
                  // Time badge
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 10,
                      vertical: 4,
                    ),
                    decoration: BoxDecoration(
                      color: AppColors.primaryBrand.withOpacity(0.15),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Text(
                      _minutesToTime(currentMin),
                      style: const TextStyle(
                        color: AppColors.primaryBrand,
                        fontWeight: FontWeight.w700,
                        fontSize: 14,
                        fontFamily: 'Courier',
                      ),
                    ),
                  ),
                  const Spacer(),
                  // Play/Pause
                  Material(
                    color: AppColors.primaryBrand,
                    borderRadius: BorderRadius.circular(24),
                    child: InkWell(
                      borderRadius: BorderRadius.circular(24),
                      onTap: _togglePlayback,
                      child: SizedBox(
                        width: 40,
                        height: 40,
                        child: Icon(
                          _isPlaying
                              ? Icons.pause_rounded
                              : Icons.play_arrow_rounded,
                          color: Colors.white,
                          size: 22,
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  // Reset
                  Material(
                    color: Colors.transparent,
                    borderRadius: BorderRadius.circular(24),
                    child: InkWell(
                      borderRadius: BorderRadius.circular(24),
                      onTap: () {
                        _playbackController.reset();
                        setState(() {
                          _isPlaying = false;
                          _playbackProgress = 0.0;
                        });
                      },
                      child: SizedBox(
                        width: 36,
                        height: 36,
                        child: Icon(
                          Icons.replay_rounded,
                          color: _textPrimary(context),
                          size: 20,
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 4),
                  // Speed button
                  Material(
                    color: AppColors.primaryBrand.withOpacity(0.12),
                    borderRadius: BorderRadius.circular(20),
                    child: InkWell(
                      borderRadius: BorderRadius.circular(20),
                      onTap: _cycleSpeed,
                      child: Container(
                        padding: const EdgeInsets.symmetric(
                          horizontal: 10,
                          vertical: 6,
                        ),
                        child: Text(
                          'x${_playbackSpeed == _playbackSpeed.roundToDouble() ? _playbackSpeed.toInt().toString() : _playbackSpeed.toString()}',
                          style: const TextStyle(
                            color: AppColors.primaryBrand,
                            fontWeight: FontWeight.w800,
                            fontSize: 12,
                          ),
                        ),
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 6),
              // Scrubber
              Row(
                children: [
                  Text(
                    _minutesToTime(timeRange.startMin),
                    style: TextStyle(
                      color: _textTertiary(context),
                      fontSize: 10,
                    ),
                  ),
                  Expanded(
                    child: SliderTheme(
                      data: SliderThemeData(
                        activeTrackColor: AppColors.primaryBrand,
                        inactiveTrackColor: AppColors.primaryBrand.withOpacity(
                          0.2,
                        ),
                        thumbColor: AppColors.primaryBrand,
                        thumbShape: const RoundSliderThumbShape(
                          enabledThumbRadius: 6,
                        ),
                        trackHeight: 3,
                        overlayShape: const RoundSliderOverlayShape(
                          overlayRadius: 14,
                        ),
                      ),
                      child: Slider(
                        value: _playbackProgress,
                        onChanged: _onScrub,
                      ),
                    ),
                  ),
                  Text(
                    _minutesToTime(timeRange.endMin),
                    style: TextStyle(
                      color: _textTertiary(context),
                      fontSize: 10,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  // ══════════════════════════════════════════════════════════
  //  SCROLLABLE SLIVERS (for embedding in DraggableScrollableSheet)
  // ══════════════════════════════════════════════════════════
  List<Widget> _buildScrollableSlivers(BuildContext context) {
    final routes = _filteredRoutes;
    return [
      // ── Summary Card (collapsible) ──
      SliverToBoxAdapter(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(16, 4, 16, 8),
          child: _buildCollapsibleSummary(context),
        ),
      ),

      // ── Search bar (always visible) ──
      SliverToBoxAdapter(child: _buildSearchBar(context)),

      // ── Fleet Filter Bar ──
      SliverToBoxAdapter(child: _buildFleetFilterBar(context)),

      // ── Vehicle Route Cards ──
      SliverPadding(
        padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        sliver: routes.isEmpty
            ? SliverToBoxAdapter(
                child: Padding(
                  padding: const EdgeInsets.symmetric(vertical: 32),
                  child: Center(
                    child: Text(
                      'No vehicles match filters',
                      style: TextStyle(
                        color: _textSecondary(context),
                        fontSize: 13,
                      ),
                    ),
                  ),
                ),
              )
            : SliverList.builder(
                itemCount: routes.length,
                itemBuilder: (ctx, i) {
                  final originalIndex = _vehicleRoutes.indexOf(routes[i]);
                  return _buildVehicleRouteCard(context, originalIndex);
                },
              ),
      ),
    ];
  }

  // ══════════════════════════════════════════════════════════
  //  SCROLLABLE CONTENT (summary + filter + vehicle cards)
  // ══════════════════════════════════════════════════════════
  Widget _buildScrollableContent(
    BuildContext context, {
    ScrollController? scrollController,
  }) {
    final routes = _filteredRoutes;
    return CustomScrollView(
      controller: scrollController,
      slivers: [
        // ── Summary Card (collapsible) ──
        SliverToBoxAdapter(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(16, 4, 16, 8),
            child: _buildCollapsibleSummary(context),
          ),
        ),

        // ── Search bar (always visible) ──
        SliverToBoxAdapter(child: _buildSearchBar(context)),

        // ── Fleet Filter Bar ──
        SliverToBoxAdapter(child: _buildFleetFilterBar(context)),

        // ── Vehicle Route Cards ──
        SliverPadding(
          padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
          sliver: routes.isEmpty
              ? SliverToBoxAdapter(
                  child: Padding(
                    padding: const EdgeInsets.symmetric(vertical: 32),
                    child: Center(
                      child: Text(
                        'No vehicles match filters',
                        style: TextStyle(
                          color: _textSecondary(context),
                          fontSize: 13,
                        ),
                      ),
                    ),
                  ),
                )
              : SliverList.builder(
                  itemCount: routes.length,
                  itemBuilder: (ctx, i) {
                    final originalIndex = _vehicleRoutes.indexOf(routes[i]);
                    return _buildVehicleRouteCard(context, originalIndex);
                  },
                ),
        ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  RESULTS PANEL (Desktop side panel — scroll + fixed bar)
  // ══════════════════════════════════════════════════════════
  Widget _buildResultsPanel(BuildContext context) {
    return Column(
      children: [
        Expanded(child: _buildScrollableContent(context)),
        _buildActionBar(context),
      ],
    );
  }

  // ── Search Bar Widget (shared between mobile + desktop) ─
  Widget _buildSearchBar(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
      child: Container(
        height: 38,
        decoration: BoxDecoration(
          color: _surfaceColor(context),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: _borderColorThemed(context).withOpacity(0.4),
          ),
        ),
        child: TextField(
          controller: _searchController,
          onChanged: (v) => setState(() => _vehicleSearchQuery = v),
          style: TextStyle(color: _textPrimary(context), fontSize: 12),
          decoration: InputDecoration(
            hintText: 'Search vehicle...',
            hintStyle: TextStyle(color: _textTertiary(context), fontSize: 12),
            prefixIcon: Icon(
              Icons.search_rounded,
              size: 18,
              color: _textTertiary(context),
            ),
            suffixIcon: _vehicleSearchQuery.isNotEmpty
                ? GestureDetector(
                    onTap: () {
                      _searchController.clear();
                      setState(() => _vehicleSearchQuery = '');
                    },
                    child: Icon(
                      Icons.close_rounded,
                      size: 18,
                      color: _textTertiary(context),
                    ),
                  )
                : null,
            border: InputBorder.none,
            contentPadding: const EdgeInsets.symmetric(vertical: 10),
            isDense: true,
          ),
        ),
      ),
    );
  }

  // ── Fleet Filter Bar (mirrors ShowInputPage) ───────────
  Widget _buildFleetFilterBar(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          InkWell(
            borderRadius: BorderRadius.circular(8),
            onTap: () => setState(() => _showFleetFilter = !_showFleetFilter),
            child: Padding(
              padding: const EdgeInsets.symmetric(vertical: 6),
              child: Row(
                children: [
                  Icon(
                    Icons.filter_list_rounded,
                    size: 16,
                    color: _hasActiveFleetFilters
                        ? AppColors.primaryBrand
                        : _textSecondary(context),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    'Filters',
                    style: TextStyle(
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                      color: _hasActiveFleetFilters
                          ? AppColors.primaryBrand
                          : _textSecondary(context),
                    ),
                  ),
                  if (_hasActiveFleetFilters) ...[
                    const SizedBox(width: 6),
                    Container(
                      width: 6,
                      height: 6,
                      decoration: const BoxDecoration(
                        color: AppColors.primaryBrand,
                        shape: BoxShape.circle,
                      ),
                    ),
                  ],
                  const Spacer(),
                  if (_hasActiveFleetFilters)
                    GestureDetector(
                      onTap: _clearFleetFilters,
                      child: Text(
                        'Clear',
                        style: TextStyle(
                          fontSize: 11,
                          color: _textTertiary(context),
                          fontWeight: FontWeight.w500,
                        ),
                      ),
                    ),
                  const SizedBox(width: 8),
                  Icon(
                    _showFleetFilter
                        ? Icons.expand_less_rounded
                        : Icons.expand_more_rounded,
                    size: 18,
                    color: _textTertiary(context),
                  ),
                ],
              ),
            ),
          ),
          if (_showFleetFilter) ...[
            const SizedBox(height: 6),
            Wrap(
              spacing: 8,
              runSpacing: 6,
              children: [
                // Vehicle ID chips
                for (final route in _vehicleRoutes)
                  _fleetFilterChip(
                    context,
                    label: route.vehicleId,
                    isActive: _filterVehicleId == route.vehicleId,
                    activeColor: route.color,
                    onTap: () => setState(() {
                      _filterVehicleId = _filterVehicleId == route.vehicleId
                          ? null
                          : route.vehicleId;
                    }),
                  ),
                // Sort by cost asc
                _fleetSortChip(
                  context,
                  label: 'Cost',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Low → High',
                  isActive: _sortByCost == true,
                  activeColor: AppColors.primaryBrand,
                  onTap: () => setState(
                    () => _sortByCost = _sortByCost == true ? null : true,
                  ),
                ),
                // Sort by cost desc
                _fleetSortChip(
                  context,
                  label: 'Cost',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'High → Low',
                  isActive: _sortByCost == false,
                  activeColor: AppColors.primaryBrand,
                  onTap: () => setState(
                    () => _sortByCost = _sortByCost == false ? null : false,
                  ),
                ),
                // Sort by distance asc
                _fleetSortChip(
                  context,
                  label: 'Distance',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Short → Long',
                  isActive: _sortByDistance == true,
                  activeColor: const Color(0xFF3B82F6),
                  onTap: () => setState(
                    () =>
                        _sortByDistance = _sortByDistance == true ? null : true,
                  ),
                ),
                // Sort by distance desc
                _fleetSortChip(
                  context,
                  label: 'Distance',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'Long → Short',
                  isActive: _sortByDistance == false,
                  activeColor: const Color(0xFF3B82F6),
                  onTap: () => setState(
                    () => _sortByDistance = _sortByDistance == false
                        ? null
                        : false,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
          ],
        ],
      ),
    );
  }

  Widget _fleetFilterChip(
    BuildContext context, {
    required String label,
    required bool isActive,
    required Color activeColor,
    required VoidCallback onTap,
  }) {
    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
        decoration: BoxDecoration(
          color: isActive ? activeColor.withOpacity(0.15) : Colors.transparent,
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: isActive
                ? activeColor.withOpacity(0.5)
                : _borderColorThemed(context),
          ),
        ),
        child: Text(
          label,
          style: TextStyle(
            color: isActive ? activeColor : _textSecondary(context),
            fontSize: 11,
            fontWeight: isActive ? FontWeight.w700 : FontWeight.w500,
          ),
        ),
      ),
    );
  }

  Widget _fleetSortChip(
    BuildContext context, {
    required String label,
    required IconData icon,
    required String tooltip,
    required bool isActive,
    required Color activeColor,
    required VoidCallback onTap,
  }) {
    return Tooltip(
      message: tooltip,
      child: GestureDetector(
        onTap: onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 200),
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
          decoration: BoxDecoration(
            color: isActive
                ? activeColor.withOpacity(0.15)
                : Colors.transparent,
            borderRadius: BorderRadius.circular(20),
            border: Border.all(
              color: isActive
                  ? activeColor.withOpacity(0.5)
                  : _borderColorThemed(context),
            ),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                icon,
                size: 12,
                color: isActive ? activeColor : _textSecondary(context),
              ),
              const SizedBox(width: 3),
              Text(
                label,
                style: TextStyle(
                  color: isActive ? activeColor : _textSecondary(context),
                  fontSize: 11,
                  fontWeight: isActive ? FontWeight.w700 : FontWeight.w500,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  // ── Collapsible Summary Wrapper ─────────────────────────
  Widget _buildCollapsibleSummary(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        // Toggle header
        InkWell(
          borderRadius: BorderRadius.circular(12),
          onTap: () => setState(() => _summaryExpanded = !_summaryExpanded),
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
            decoration: BoxDecoration(
              gradient: LinearGradient(
                colors: [
                  AppColors.primaryBrand.withOpacity(0.12),
                  AppColors.primaryBrand.withOpacity(0.04),
                ],
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
              ),
              borderRadius: _summaryExpanded
                  ? const BorderRadius.vertical(top: Radius.circular(16))
                  : BorderRadius.circular(16),
              border: Border.all(
                color: AppColors.primaryBrand.withOpacity(0.2),
              ),
            ),
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(6),
                  decoration: BoxDecoration(
                    color: AppColors.primaryBrand.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: const Icon(
                    Icons.analytics_rounded,
                    color: AppColors.primaryBrand,
                    size: 18,
                  ),
                ),
                const SizedBox(width: 10),
                Text(
                  'Optimization Summary',
                  style: TextStyle(
                    color: _textPrimary(context),
                    fontWeight: FontWeight.w700,
                    fontSize: 15,
                  ),
                ),
                const Spacer(),
                AnimatedRotation(
                  turns: _summaryExpanded ? 0.5 : 0.0,
                  duration: const Duration(milliseconds: 250),
                  child: Icon(
                    Icons.keyboard_arrow_down_rounded,
                    color: _textTertiary(context),
                    size: 22,
                  ),
                ),
              ],
            ),
          ),
        ),
        // Animated body
        AnimatedCrossFade(
          firstChild: _buildSummaryCard(context),
          secondChild: const SizedBox.shrink(),
          crossFadeState: _summaryExpanded
              ? CrossFadeState.showFirst
              : CrossFadeState.showSecond,
          duration: const Duration(milliseconds: 300),
          sizeCurve: Curves.easeInOut,
        ),
      ],
    );
  }

  // ── Summary Card ───────────────────────────────────────
  Widget _buildSummaryCard(BuildContext context) {
    // Handle both data formats:
    // Format A (raw solver): { cost, total_time, stats: { hard_violations, ... } }
    // Format B (backend API): { result: { total_cost, total_time, hard_violations, ... } }
    final result = (_currentData['result'] as Map<String, dynamic>?) ?? {};
    final stats = (_currentData['stats'] as Map<String, dynamic>?) ?? {};

    final cost =
        (result['total_cost'] as num?)?.toDouble() ??
        (_currentData['cost'] as num?)?.toDouble() ??
        0;
    final totalTime =
        (result['total_time'] as num?)?.toDouble() ??
        (_currentData['total_time'] as num?)?.toDouble() ??
        0;
    final hardViolations =
        (result['hard_violations'] as num?)?.toInt() ??
        (stats['hard_violations'] as num?)?.toInt() ??
        0;
    final softViolations =
        (result['soft_violations'] as num?)?.toInt() ??
        (stats['soft_violations'] as num?)?.toInt() ??
        0;

    // Total distance: prefer result, else sum from routes
    double totalDist = (result['total_distance'] as num?)?.toDouble() ?? 0;
    if (totalDist == 0) {
      for (var r in _vehicleRoutes) {
        totalDist += r.totalDistance;
      }
    }

    // Cost savings (Format B only)
    final costSavings = (result['cost_savings'] as num?)?.toDouble();
    final costSavingsPct = (result['cost_savings_percent'] as num?)?.toDouble();

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: [
            AppColors.primaryBrand.withOpacity(0.12),
            AppColors.primaryBrand.withOpacity(0.04),
          ],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: const BorderRadius.vertical(bottom: Radius.circular(16)),
        border: Border.all(color: AppColors.primaryBrand.withOpacity(0.2)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Stat grid
          Row(
            children: [
              Expanded(
                child: _summaryStatTile(
                  context,
                  icon: Icons.currency_rupee_rounded,
                  label: 'Total Cost',
                  value: cost.toStringAsFixed(1),
                  color: AppColors.primaryBrand,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _summaryStatTile(
                  context,
                  icon: Icons.route_rounded,
                  label: 'Total Km',
                  value: totalDist.toStringAsFixed(1),
                  color: const Color(0xFF3B82F6),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              Expanded(
                child: _summaryStatTile(
                  context,
                  icon: Icons.timer_rounded,
                  label: 'Total Time',
                  value: '${totalTime.toStringAsFixed(0)} min',
                  color: const Color(0xFFF59E0B),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _summaryStatTile(
                  context,
                  icon: Icons.local_shipping_rounded,
                  label: 'Vehicles Used',
                  value:
                      '${(result['vehicles_used'] as num?)?.toInt() ?? _vehicleRoutes.length}',
                  color: const Color(0xFF8B5CF6),
                ),
              ),
            ],
          ),

          // Violations
          if (hardViolations > 0 || softViolations > 0) ...[
            const SizedBox(height: 12),
            Row(
              children: [
                if (hardViolations > 0)
                  _violationChip('Hard: $hardViolations', AppColors.error),
                if (hardViolations > 0 && softViolations > 0)
                  const SizedBox(width: 8),
                if (softViolations > 0)
                  _violationChip('Soft: $softViolations', AppColors.warning),
              ],
            ),
          ],

          // Cost savings (Format B only)
          if (costSavings != null && costSavings > 0) ...[
            const SizedBox(height: 10),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
              decoration: BoxDecoration(
                color: AppColors.primaryBrand.withOpacity(0.08),
                borderRadius: BorderRadius.circular(8),
                border: Border.all(
                  color: AppColors.primaryBrand.withOpacity(0.25),
                ),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const Icon(
                    Icons.trending_down_rounded,
                    color: AppColors.primaryBrand,
                    size: 14,
                  ),
                  const SizedBox(width: 6),
                  Text(
                    'Saved ₹${costSavings.toStringAsFixed(0)}'
                    '${costSavingsPct != null ? ' (${costSavingsPct.toStringAsFixed(1)}%)' : ''}'
                    ' vs baseline',
                    style: const TextStyle(
                      color: AppColors.primaryBrand,
                      fontSize: 11,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
    );
  }

  Widget _summaryStatTile(
    BuildContext context, {
    required IconData icon,
    required String label,
    required String value,
    required Color color,
  }) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: _surfaceColor(context),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: _borderColorThemed(context).withOpacity(0.3)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: color, size: 18),
          const SizedBox(height: 8),
          Text(
            value,
            style: TextStyle(
              color: _textPrimary(context),
              fontWeight: FontWeight.w800,
              fontSize: 16,
            ),
          ),
          const SizedBox(height: 2),
          Text(
            label,
            style: TextStyle(color: _textSecondary(context), fontSize: 11),
          ),
        ],
      ),
    );
  }

  Widget _violationChip(String label, Color color) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: color.withOpacity(0.12),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withOpacity(0.3)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.warning_rounded, color: color, size: 13),
          const SizedBox(width: 4),
          Text(
            label,
            style: TextStyle(
              color: color,
              fontSize: 11,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }

  // ── Vehicle Route Card ─────────────────────────────────
  Widget _buildVehicleRouteCard(BuildContext context, int index) {
    final route = _vehicleRoutes[index];
    final isExpanded = _expandedCards.contains(index);
    final isFocused = _focusedVehicleIndex == index;

    // Count passengers (unique pickup stops)
    int passengerCount = 0;
    for (var stop in route.allStops) {
      if (stop.location.toLowerCase().contains('pickup')) passengerCount++;
    }

    final dropOffTime = _getDropOffTime(route);

    return Container(
      margin: const EdgeInsets.only(bottom: 10),
      decoration: BoxDecoration(
        color: _surfaceColor(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: isFocused
              ? route.color.withOpacity(0.6)
              : route.color.withOpacity(0.2),
          width: isFocused ? 2 : 1,
        ),
        boxShadow: isFocused
            ? [
                BoxShadow(
                  color: route.color.withOpacity(0.15),
                  blurRadius: 12,
                  spreadRadius: 1,
                ),
              ]
            : null,
      ),
      child: Column(
        children: [
          // ── Header ──
          InkWell(
            borderRadius: const BorderRadius.vertical(top: Radius.circular(14)),
            onTap: () {
              setState(() {
                if (isExpanded) {
                  _expandedCards.remove(index);
                } else {
                  _expandedCards.add(index);
                }
              });
            },
            child: Padding(
              padding: const EdgeInsets.all(14),
              child: Row(
                children: [
                  // Vehicle color indicator
                  Container(
                    width: 42,
                    height: 42,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: route.color.withOpacity(0.15),
                      border: Border.all(color: route.color.withOpacity(0.4)),
                    ),
                    child: Icon(
                      Icons.directions_car_rounded,
                      color: route.color,
                      size: 22,
                    ),
                  ),
                  const SizedBox(width: 12),

                  // ID + stats
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          route.vehicleId,
                          style: TextStyle(
                            color: _textPrimary(context),
                            fontWeight: FontWeight.w700,
                            fontSize: 15,
                          ),
                        ),
                        const SizedBox(height: 4),
                        Row(
                          children: [
                            _miniStat(
                              Icons.people_rounded,
                              '$passengerCount',
                              _textSecondary(context),
                            ),
                            const SizedBox(width: 12),
                            _miniStat(
                              Icons.route_rounded,
                              '${route.totalDistance.toStringAsFixed(1)} km',
                              _textSecondary(context),
                            ),
                            const SizedBox(width: 12),
                            _miniStat(
                              Icons.currency_rupee_rounded,
                              route.totalCost.toStringAsFixed(0),
                              AppColors.primaryBrand,
                            ),
                          ],
                        ),
                        const SizedBox(height: 4),
                        Row(
                          children: [
                            _miniStat(
                              Icons.business_rounded,
                              'Office: $dropOffTime',
                              const Color(0xFFEF4444),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),

                  // Trip count badge
                  Container(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 8,
                      vertical: 3,
                    ),
                    decoration: BoxDecoration(
                      color: route.color.withOpacity(0.12),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: Text(
                      '${route.trips.length} trip${route.trips.length > 1 ? 's' : ''}',
                      style: TextStyle(
                        color: route.color,
                        fontSize: 10,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),

                  // Expand arrow
                  Icon(
                    isExpanded
                        ? Icons.expand_less_rounded
                        : Icons.expand_more_rounded,
                    color: _textTertiary(context),
                    size: 20,
                  ),
                ],
              ),
            ),
          ),

          // ── Action Row (always visible) ──
          Padding(
            padding: const EdgeInsets.fromLTRB(14, 0, 14, 10),
            child: Row(
              children: [
                // Focus on map button
                _actionMiniButton(
                  context,
                  icon: isFocused
                      ? Icons.visibility_off_rounded
                      : Icons.visibility_rounded,
                  label: isFocused ? 'Show All' : 'Focus',
                  color: route.color,
                  onTap: () => _focusVehicle(index),
                ),
                const SizedBox(width: 8),
                // Play single vehicle
                _actionMiniButton(
                  context,
                  icon: Icons.play_circle_rounded,
                  label: 'Animate',
                  color: route.color,
                  onTap: () {
                    _focusVehicle(index);
                    if (!_isPlaying) _togglePlayback();
                  },
                ),
              ],
            ),
          ),

          // ── Expanded: Trip Timeline ──
          if (isExpanded)
            Container(
              decoration: BoxDecoration(
                border: Border(
                  top: BorderSide(
                    color: _borderColorThemed(context).withOpacity(0.3),
                  ),
                ),
              ),
              child: Column(
                children: [
                  for (int t = 0; t < route.trips.length; t++)
                    _buildTripTimeline(context, route.trips[t], route.color),
                ],
              ),
            ),
        ],
      ),
    );
  }

  Widget _miniStat(IconData icon, String label, Color color) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, color: color, size: 13),
        const SizedBox(width: 3),
        Text(
          label,
          style: TextStyle(
            color: color,
            fontWeight: FontWeight.w500,
            fontSize: 11,
          ),
        ),
      ],
    );
  }

  Widget _actionMiniButton(
    BuildContext context, {
    required IconData icon,
    required String label,
    required Color color,
    required VoidCallback onTap,
  }) {
    return InkWell(
      borderRadius: BorderRadius.circular(8),
      onTap: onTap,
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: color.withOpacity(0.08),
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: color.withOpacity(0.2)),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, color: color, size: 14),
            const SizedBox(width: 4),
            Text(
              label,
              style: TextStyle(
                color: color,
                fontSize: 11,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
      ),
    );
  }

  // ── Trip Timeline ──────────────────────────────────────
  Widget _buildTripTimeline(BuildContext context, _TripInfo trip, Color color) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(14, 12, 14, 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Trip header
          Row(
            children: [
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                decoration: BoxDecoration(
                  color: color.withOpacity(0.12),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Text(
                  'Trip ${trip.tripNumber}',
                  style: TextStyle(
                    color: color,
                    fontSize: 11,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ),
              const SizedBox(width: 8),
              Text(
                '${trip.totalDistance.toStringAsFixed(1)} km · ${trip.totalTime} min · ₹${trip.totalCost.toStringAsFixed(0)}',
                style: TextStyle(color: _textTertiary(context), fontSize: 10),
              ),
            ],
          ),
          const SizedBox(height: 10),

          // Timeline stops
          for (int s = 0; s < trip.stops.length; s++)
            _buildTimelineStop(
              context,
              trip.stops[s],
              color,
              isFirst: s == 0,
              isLast: s == trip.stops.length - 1,
            ),
        ],
      ),
    );
  }

  Widget _buildTimelineStop(
    BuildContext context,
    _StopInfo stop,
    Color color, {
    required bool isFirst,
    required bool isLast,
  }) {
    final locLower = stop.location.toLowerCase();
    final isDeparture = locLower.contains('departure');
    final isDropoff =
        !isDeparture &&
        (locLower.contains('drop') || locLower.contains('office'));
    final isDepot = locLower.contains('depot');

    final dotColor = isDepot || isDeparture
        ? color
        : isDropoff
        ? AppColors.error
        : AppColors.primaryBrand;

    return IntrinsicHeight(
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Timeline line + dot
          SizedBox(
            width: 24,
            child: Column(
              children: [
                if (!isFirst)
                  Container(
                    width: 1.5,
                    height: 6,
                    color: color.withOpacity(0.3),
                  ),
                Container(
                  width: 10,
                  height: 10,
                  decoration: BoxDecoration(
                    color: dotColor,
                    shape: BoxShape.circle,
                    border: Border.all(
                      color: dotColor.withOpacity(0.3),
                      width: 2,
                    ),
                  ),
                ),
                if (!isLast)
                  Expanded(
                    child: Container(width: 1.5, color: color.withOpacity(0.3)),
                  ),
              ],
            ),
          ),
          const SizedBox(width: 8),

          // Stop info
          Expanded(
            child: Padding(
              padding: const EdgeInsets.only(bottom: 14),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    stop.location,
                    style: TextStyle(
                      color: _textPrimary(context),
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Row(
                    children: [
                      Icon(
                        Icons.schedule_rounded,
                        color: _textTertiary(context),
                        size: 11,
                      ),
                      const SizedBox(width: 3),
                      Text(
                        'Arr: ${stop.arrivalTime}',
                        style: TextStyle(
                          color: _textSecondary(context),
                          fontSize: 10,
                        ),
                      ),
                      if (stop.arrivalTime != stop.departureTime) ...[
                        Text(
                          '  →  Dep: ${stop.departureTime}',
                          style: TextStyle(
                            color: _textSecondary(context),
                            fontSize: 10,
                          ),
                        ),
                      ],
                      if (stop.waitTime > 0) ...[
                        const SizedBox(width: 6),
                        Container(
                          padding: const EdgeInsets.symmetric(
                            horizontal: 5,
                            vertical: 1,
                          ),
                          decoration: BoxDecoration(
                            color: AppColors.warning.withOpacity(0.12),
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: Text(
                            'wait ${stop.waitTime}m',
                            style: const TextStyle(
                              color: AppColors.warning,
                              fontSize: 9,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                      ],
                    ],
                  ),
                  if (stop.distanceFromPrev > 0)
                    Padding(
                      padding: const EdgeInsets.only(top: 2),
                      child: Text(
                        '${stop.distanceFromPrev.toStringAsFixed(1)} km from prev',
                        style: TextStyle(
                          color: _textTertiary(context),
                          fontSize: 9,
                        ),
                      ),
                    ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  // ── Action Bar ─────────────────────────────────────────
  Widget _buildActionBar(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(16, 12, 16, 16),
      decoration: BoxDecoration(
        color: _bgColor(context),
        border: Border(
          top: BorderSide(color: _borderColorThemed(context).withOpacity(0.4)),
        ),
      ),
      child: SafeArea(
        top: false,
        child: Row(
          children: [
            // ── Details ──
            Expanded(
              child: OutlinedButton.icon(
                onPressed: () {
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (_) => OutputDetailsPage(
                        testCaseId: widget.testCaseId,
                        testCaseName: widget.testCaseName,
                        resultData: _currentData,
                      ),
                    ),
                  );
                },
                icon: const Icon(Icons.info_outline_rounded, size: 18),
                label: const Text(
                  'DETAILS',
                  style: TextStyle(fontSize: 12, fontWeight: FontWeight.w700),
                ),
                style: OutlinedButton.styleFrom(
                  foregroundColor: AppColors.primaryBrand,
                  side: const BorderSide(color: AppColors.primaryBrand),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(30),
                  ),
                  minimumSize: const Size(0, 50),
                ),
              ),
            ),
            const SizedBox(width: 12),

            // ── Re-Run ──
            Expanded(
              child: ElevatedButton.icon(
                onPressed: _isLoading ? null : _handleRetry,
                icon: _isLoading
                    ? const SizedBox(
                        width: 18,
                        height: 18,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(
                        Icons.rocket_launch_rounded,
                        size: 18,
                        color: Colors.white,
                      ),
                label: const Text(
                  'RE-RUN',
                  style: TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                    color: Colors.white,
                  ),
                ),
                style: ElevatedButton.styleFrom(
                  backgroundColor: AppColors.primaryBrand,
                  disabledBackgroundColor: AppColors.primaryBrand.withOpacity(
                    0.5,
                  ),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(30),
                  ),
                  minimumSize: const Size(0, 50),
                  elevation: 4,
                  shadowColor: AppColors.primaryBrand.withOpacity(0.4),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// ═══════════════════════════════════════════════════════════════
//  DATA MODELS
// ═══════════════════════════════════════════════════════════════

class _VehicleRoute {
  final String vehicleId;
  final double totalCost;
  final double totalDistance;
  final int totalTime;
  final Color color;
  final List<_TripInfo> trips;
  final List<_StopInfo> allStops;

  _VehicleRoute({
    required this.vehicleId,
    required this.totalCost,
    required this.totalDistance,
    required this.totalTime,
    required this.color,
    required this.trips,
    required this.allStops,
  });
}

class _TripInfo {
  final int tripNumber;
  final double totalCost;
  final double totalDistance;
  final int totalTime;
  final List<_StopInfo> stops;

  _TripInfo({
    required this.tripNumber,
    required this.totalCost,
    required this.totalDistance,
    required this.totalTime,
    required this.stops,
  });
}

class _StopInfo {
  final String location;
  final String arrivalTime;
  final String departureTime;
  final double distanceFromPrev;
  final int waitTime;
  final double? lat;
  final double? lng;
  final String? employeeId;

  _StopInfo({
    required this.location,
    required this.arrivalTime,
    required this.departureTime,
    required this.distanceFromPrev,
    required this.waitTime,
    this.lat,
    this.lng,
    this.employeeId,
  });
}

// ═══════════════════════════════════════════════════════════════
//  CUSTOM MAP MARKERS (matching show_input_page / map_view style)
// ═══════════════════════════════════════════════════════════════

/// Employee pickup marker — colored pill with label + triangle pointer.
class _EmployeeMarkerWidget extends StatelessWidget {
  final String label;
  final Color color;

  const _EmployeeMarkerWidget({required this.label, required this.color});

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
          decoration: BoxDecoration(
            color: color,
            borderRadius: BorderRadius.circular(8),
            boxShadow: [
              BoxShadow(
                color: color.withOpacity(0.4),
                blurRadius: 4,
                offset: const Offset(0, 1),
              ),
            ],
          ),
          child: Text(
            label,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 9,
              fontWeight: FontWeight.w800,
              letterSpacing: 0.3,
              height: 1.2,
            ),
          ),
        ),
        CustomPaint(
          size: const Size(8, 5),
          painter: _TrianglePainter(color: color),
        ),
      ],
    );
  }
}

/// Office / drop-off marker — green circle with building icon.
class _OfficeMarkerWidget extends StatelessWidget {
  const _OfficeMarkerWidget();

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: AppColors.primaryBrand,
        shape: BoxShape.circle,
        boxShadow: [
          BoxShadow(
            color: AppColors.primaryBrand.withOpacity(0.4),
            blurRadius: 8,
            spreadRadius: 2,
          ),
        ],
      ),
      child: const Icon(Icons.business_rounded, color: Colors.white, size: 22),
    );
  }
}

/// Static vehicle marker — bordered pill with car icon + V-label + triangle.
class _OutputVehicleMarker extends StatelessWidget {
  final String label;
  final Color color;

  const _OutputVehicleMarker({required this.label, required this.color});

  @override
  Widget build(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    final bg = isDark ? AppColors.darkSurface : Colors.white;

    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
          decoration: BoxDecoration(
            color: bg,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: color, width: 2),
            boxShadow: [
              BoxShadow(
                color: color.withOpacity(0.3),
                blurRadius: 4,
                offset: const Offset(0, 1),
              ),
            ],
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.directions_car_rounded, color: color, size: 11),
              const SizedBox(width: 2),
              Text(
                label,
                style: TextStyle(
                  color: color,
                  fontSize: 9,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 0.3,
                  height: 1.2,
                ),
              ),
            ],
          ),
        ),
        CustomPaint(
          size: const Size(8, 5),
          painter: _TrianglePainter(color: color),
        ),
      ],
    );
  }
}

/// Animated vehicle marker — pill style with pulsing effect.
class _AnimatedVehicleMarker extends StatefulWidget {
  final Color color;
  final String label;

  const _AnimatedVehicleMarker({required this.color, required this.label});

  @override
  State<_AnimatedVehicleMarker> createState() => _AnimatedVehicleMarkerState();
}

class _AnimatedVehicleMarkerState extends State<_AnimatedVehicleMarker>
    with SingleTickerProviderStateMixin {
  late AnimationController _pulseController;

  @override
  void initState() {
    super.initState();
    _pulseController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1200),
    )..repeat(reverse: true);
    _pulseController.addListener(() {
      if (mounted) setState(() {});
    });
  }

  @override
  void dispose() {
    _pulseController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    final bg = isDark ? AppColors.darkSurface : Colors.white;
    final scale = 1.0 + _pulseController.value * 0.12;

    return Transform.scale(
      scale: scale,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
            decoration: BoxDecoration(
              color: bg,
              borderRadius: BorderRadius.circular(8),
              border: Border.all(color: widget.color, width: 2),
              boxShadow: [
                BoxShadow(
                  color: widget.color.withOpacity(0.5),
                  blurRadius: 8,
                  spreadRadius: 2,
                ),
              ],
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(
                  Icons.directions_car_rounded,
                  color: widget.color,
                  size: 12,
                ),
                const SizedBox(width: 2),
                Text(
                  widget.label,
                  style: TextStyle(
                    color: widget.color,
                    fontSize: 9,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 0.3,
                    height: 1.2,
                  ),
                ),
              ],
            ),
          ),
          CustomPaint(
            size: const Size(8, 5),
            painter: _TrianglePainter(color: widget.color),
          ),
        ],
      ),
    );
  }
}

/// Triangle pointer painter for markers.
class _TrianglePainter extends CustomPainter {
  final Color color;
  _TrianglePainter({required this.color});

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()..color = color;
    final path = ui.Path()
      ..moveTo(0, 0)
      ..lineTo(size.width / 2, size.height)
      ..lineTo(size.width, 0)
      ..close();
    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
