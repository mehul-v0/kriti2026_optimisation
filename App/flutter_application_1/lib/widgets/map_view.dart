import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:flutter_application_1/config/info.dart';
import 'package:flutter_application_1/theme/theme.dart';

class MapViewWidget extends StatefulWidget {
  final List employees;
  final List vehicles;
  final void Function(Map<String, dynamic> employee)? onEmployeeTap;
  final void Function(Map<String, dynamic> vehicle)? onVehicleTap;

  const MapViewWidget({
    super.key,
    required this.employees,
    required this.vehicles,
    this.onEmployeeTap,
    this.onVehicleTap,
  });

  @override
  State<MapViewWidget> createState() => _MapViewWidgetState();
}

class _MapViewWidgetState extends State<MapViewWidget> {
  final MapController _mapController = MapController();
  late LatLng _center;
  bool _mapReady = false;
  bool _showLegend = false;

  // ── Dark / Light mode tile colour filters — defined in theme.dart ──
  // Use AppThemeData.mapDarkMatrix / AppThemeData.mapLightMatrix directly.

  @override
  void initState() {
    super.initState();
    _center = _computeCenter();
  }

  LatLng _computeCenter() {
    final allPoints = <LatLng>[];
    for (var emp in widget.employees) {
      final lat = _toDouble(emp['pickup_lat']);
      final lng = _toDouble(emp['pickup_lng']);
      if (lat != 0 && lng != 0) allPoints.add(LatLng(lat, lng));
    }
    for (var veh in widget.vehicles) {
      final lat = _toDouble(veh['current_lat']);
      final lng = _toDouble(veh['current_lng']);
      if (lat != 0 && lng != 0) allPoints.add(LatLng(lat, lng));
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

  double _toDouble(dynamic val) => (val as num?)?.toDouble() ?? 0.0;

  @override
  Widget build(BuildContext context) {
    final isDarkMode = Theme.of(context).brightness == Brightness.dark;
    List<Marker> markers = _buildMarkers(context);

    return Stack(
      children: [
        FlutterMap(
          mapController: _mapController,
          options: MapOptions(
            initialCenter: _center,
            initialZoom: MapConfig.defaultZoom,
            onMapReady: () => _mapReady = true,
            interactionOptions: const InteractionOptions(
              flags: InteractiveFlag.all,
            ),
            onTap: (_, __) {
              if (_showLegend) setState(() => _showLegend = false);
            },
          ),
          children: [
            ColorFiltered(
              colorFilter: isDarkMode
                  ? ColorFilter.matrix(
                      context.primary == const Color(0xFF00D2BE)
                          ? AppThemeData.mapDarkMatrix
                          : AppThemeData.mapDarkNeutralMatrix,
                    )
                  : const ColorFilter.matrix(AppThemeData.mapLightMatrix),
              child: TileLayer(
                urlTemplate: MapConfig.tileUrlTemplate,
                userAgentPackageName: MapConfig.packageName,
              ),
            ),
            MarkerLayer(markers: markers),
          ],
        ),

        // ── North / Reset Rotation ──
        Positioned(
          top: 16,
          right: 16,
          child: _mapButton(
            context,
            icon: Icons.navigation_rounded,
            onPressed: () {
              if (_mapReady) _mapController.rotate(0);
            },
          ),
        ),

        // ── Recenter ──
        Positioned(
          top: 70,
          right: 16,
          child: _mapButton(
            context,
            icon: Icons.my_location_rounded,
            onPressed: () {
              if (_mapReady) {
                final freshCenter = _computeCenter();
                _mapController.move(freshCenter, MapConfig.defaultZoom);
              }
            },
          ),
        ),

        // ── Legend / Info button ──
        Positioned(
          top: 124,
          right: 16,
          child: _mapButton(
            context,
            icon: Icons.info_outline_rounded,
            onPressed: () => setState(() => _showLegend = !_showLegend),
          ),
        ),

        // ── Legend overlay ──
        if (_showLegend)
          Positioned(top: 124, right: 62, child: _buildLegend(context)),
      ],
    );
  }

  Widget _mapButton(
    BuildContext context, {
    required IconData icon,
    required VoidCallback onPressed,
  }) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
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
          child: Icon(icon, color: context.primary, size: 20),
        ),
      ),
    );
  }

  // ── Legend panel ───────────────────────────────────────
  Widget _buildLegend(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    final bg = isDark ? AppColors.darkSurface : Colors.white;
    final textC = isDark ? Colors.white : AppColors.textPrimaryLight;
    final subC = isDark ? Colors.white54 : AppColors.textSecondaryLight;
    // Employee marker colour — mirrors _buildMarkers logic
    final empColor = isDark ? AppColors.silver : const Color(0xFFA0A0A0);

    return Material(
      elevation: 6,
      borderRadius: BorderRadius.circular(14),
      color: bg,
      child: Container(
        padding: const EdgeInsets.all(14),
        constraints: const BoxConstraints(maxWidth: 200),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              'Map Legend',
              style: TextStyle(
                color: textC,
                fontSize: 13,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 10),

            // Company
            _legendRow(
              context,
              child: Container(
                width: 20,
                height: 20,
                decoration: BoxDecoration(
                  color: context.primary,
                  shape: BoxShape.circle,
                ),
                child: const Icon(
                  Icons.business_rounded,
                  color: Colors.white,
                  size: 12,
                ),
              ),
              label: 'Company / Drop',
              color: subC,
            ),
            const SizedBox(height: 8),

            // Employees — single silver row
            _legendRow(
              context,
              child: Container(
                width: 20,
                height: 14,
                decoration: BoxDecoration(
                  color: empColor,
                  borderRadius: BorderRadius.circular(4),
                ),
                child: Center(
                  child: Icon(
                    Icons.person_rounded,
                    color: isDark ? Colors.black87 : Colors.white,
                    size: 10,
                  ),
                ),
              ),
              label: 'Employee',
              color: subC,
            ),
            const SizedBox(height: 6),

            // Vehicles
            Text(
              'Vehicles',
              style: TextStyle(
                color: subC,
                fontSize: 10,
                fontWeight: FontWeight.w600,
              ),
            ),
            const SizedBox(height: 4),
            // Premium — Petronas teal filled car
            _legendRow(
              context,
              child: Container(
                width: 20,
                height: 14,
                decoration: BoxDecoration(
                  color: context.primary,
                  borderRadius: BorderRadius.circular(4),
                ),
                child: const Center(
                  child: Icon(
                    Icons.directions_car_filled_rounded,
                    color: Colors.black87,
                    size: 10,
                  ),
                ),
              ),
              label: 'Premium',
              color: subC,
            ),
            const SizedBox(height: 4),
            // Normal — silver outline car
            _legendRow(
              context,
              child: Container(
                width: 20,
                height: 14,
                decoration: BoxDecoration(
                  color: empColor,
                  borderRadius: BorderRadius.circular(4),
                ),
                child: const Center(
                  child: Icon(
                    Icons.directions_car_rounded,
                    color: Colors.black87,
                    size: 10,
                  ),
                ),
              ),
              label: 'Normal',
              color: subC,
            ),
          ],
        ),
      ),
    );
  }

  Widget _legendRow(
    BuildContext context, {
    required Widget child,
    required String label,
    required Color color,
  }) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        SizedBox(width: 24, child: Center(child: child)),
        const SizedBox(width: 8),
        Flexible(
          child: Text(label, style: TextStyle(color: color, fontSize: 11)),
        ),
      ],
    );
  }

  List<Marker> _buildMarkers(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    List<Marker> markers = [];
    bool companyAdded = false;

    // A. Employees — colored by priority, tappable
    for (var emp in widget.employees) {
      double pLat = _toDouble(emp['pickup_lat']);
      double pLng = _toDouble(emp['pickup_lng']);
      double dLat = _toDouble(emp['drop_lat']);
      double dLng = _toDouble(emp['drop_lng']);
      // Uniform silver for all employees; lighter silver on light map tiles
      final color = isDark ? AppColors.silver : const Color(0xFFA0A0A0);
      final shortId = (emp['employee_id']?.toString() ?? '');
      final empData = Map<String, dynamic>.from(emp as Map);

      if (pLat != 0 && pLng != 0) {
        markers.add(
          Marker(
            point: LatLng(pLat, pLng),
            width: 64,
            height: 36,
            child: GestureDetector(
              onTap: () => widget.onEmployeeTap?.call(empData),
              child: _EmployeeMarker(
                label: shortId,
                color: color,
                isDark: isDark,
              ),
            ),
          ),
        );
      }

      if (!companyAdded && dLat != 0 && dLng != 0) {
        markers.add(
          Marker(
            point: LatLng(dLat, dLng),
            width: 40,
            height: 40,
            child: Container(
              decoration: BoxDecoration(
                color: context.primary,
                shape: BoxShape.circle,
                boxShadow: [
                  BoxShadow(
                    color: context.primary.withOpacity(0.4),
                    blurRadius: 8,
                    spreadRadius: 2,
                  ),
                ],
              ),
              child: const Icon(
                Icons.business_rounded,
                color: Colors.white,
                size: 22,
              ),
            ),
          ),
        );
        companyAdded = true;
      }
    }

    // B. Vehicles — pill label style, tappable
    for (var veh in widget.vehicles) {
      final isPremium = veh['category'].toString().toLowerCase() == 'premium';
      double cLat = _toDouble(veh['current_lat']);
      double cLng = _toDouble(veh['current_lng']);
      final vId = veh['vehicle_id']?.toString() ?? '';
      final vehData = Map<String, dynamic>.from(veh as Map);

      if (cLat != 0 && cLng != 0) {
        markers.add(
          Marker(
            point: LatLng(cLat, cLng),
            width: 64,
            height: 36,
            child: GestureDetector(
              onTap: () => widget.onVehicleTap?.call(vehData),
              child: _VehicleMarker(
                label: vId,
                isPremium: isPremium,
                isDark: isDark,
              ),
            ),
          ),
        );
      }
    }

    return markers;
  }
}

// ── Custom Employee Marker ───────────────────────────────
class _EmployeeMarker extends StatelessWidget {
  final String label;
  final Color color;
  final bool isDark;

  const _EmployeeMarker({
    required this.label,
    required this.color,
    required this.isDark,
  });

  @override
  Widget build(BuildContext context) {
    // Light mode: silver (#A0A0A0) bg → white text for contrast
    // Dark mode: silver (#C0C0C0) bg → black text for contrast
    final textColor = isDark ? Colors.black87 : Colors.white;
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
            style: TextStyle(
              color: textColor,
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

// ── Custom Vehicle Marker — pill label style (like employee) ─
class _VehicleMarker extends StatelessWidget {
  final String label;
  final bool isPremium;
  final bool isDark;

  const _VehicleMarker({
    required this.label,
    required this.isPremium,
    required this.isDark,
  });

  @override
  Widget build(BuildContext context) {
    // Premium → theme accent; Normal → silver (dark mode) / medium grey (light mode)
    final color = isPremium
        ? context.primary
        : (isDark ? AppColors.markerNormal : const Color(0xFFA0A0A0));
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
              Icon(
                isPremium
                    ? Icons.directions_car_filled_rounded
                    : Icons.directions_car_rounded,
                color: color,
                size: 11,
              ),
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

// ── Triangle pointer painter ─────────────────────────────
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
