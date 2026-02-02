import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:flutter_application_1/config/info.dart';
import 'package:flutter_application_1/theme/theme.dart';

class MapViewWidget extends StatefulWidget {
  final List employees;
  final List vehicles;

  const MapViewWidget({
    super.key,
    required this.employees,
    required this.vehicles,
  });

  @override
  State<MapViewWidget> createState() => _MapViewWidgetState();
}

class _MapViewWidgetState extends State<MapViewWidget> {
  final MapController _mapController = MapController();

  // Color Filter Matrix to invert map colors for Dark Mode
  // This turns white backgrounds to dark grey/black
  static const List<double> _darkModeMatrix = [
    -1, 0, 0, 0, 255, // Red
    0, -1, 0, 0, 255, // Green
    0, 0, -1, 0, 255, // Blue
    0, 0, 0, 1, 0, // Alpha
  ];

  @override
  Widget build(BuildContext context) {
    final isDarkMode = Theme.of(context).brightness == Brightness.dark;

    // 1. Generate Markers
    List<Marker> markers = _buildMarkers(context);

    // 2. Calculate Center
    LatLng center = markers.isNotEmpty
        ? _calculateCenter(markers)
        : const LatLng(MapConfig.defaultLat, MapConfig.defaultLng);

    return Stack(
      children: [
        FlutterMap(
          mapController: _mapController,
          options: MapOptions(
            initialCenter: center,
            initialZoom: MapConfig.defaultZoom,
            interactionOptions: const InteractionOptions(
              flags: InteractiveFlag.all,
            ),
          ),
          children: [
            // Dark Mode Logic: Invert colors if dark theme is active
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
            MarkerLayer(markers: markers),
          ],
        ),

        // --- CONTROLS OVERLAY ---

        // 1. North / Reset Rotation
        Positioned(
          top: 16,
          right: 16,
          child: FloatingActionButton.small(
            heroTag: "btn_north",
            backgroundColor: Theme.of(context).cardColor,
            child: const Icon(Icons.navigation, color: AppColors.primaryBrand),
            onPressed: () {
              _mapController.rotate(0);
            },
          ),
        ),

        // 2. Relocate / Re-center Button
        Positioned(
          top: 70, // Below the North button
          right: 16,
          child: FloatingActionButton.small(
            heroTag: "btn_relocate",
            backgroundColor: Theme.of(context).cardColor,
            child: const Icon(Icons.my_location, color: AppColors.primaryBrand),
            onPressed: () {
              // Smoothly move back to center
              _mapController.move(center, MapConfig.defaultZoom);
            },
          ),
        ),

        // 3. Zoom Controls
        Positioned(
          bottom: 16,
          right: 16,
          child: Column(
            children: [
              FloatingActionButton.small(
                heroTag: "btn_zoom_in",
                backgroundColor: Theme.of(context).cardColor,
                child: const Icon(Icons.add, color: AppColors.primaryBrand),
                onPressed: () {
                  final zoom = _mapController.camera.zoom + 1;
                  _mapController.move(_mapController.camera.center, zoom);
                },
              ),
              const SizedBox(height: 8),
              FloatingActionButton.small(
                heroTag: "btn_zoom_out",
                backgroundColor: Theme.of(context).cardColor,
                child: const Icon(Icons.remove, color: AppColors.primaryBrand),
                onPressed: () {
                  final zoom = _mapController.camera.zoom - 1;
                  _mapController.move(_mapController.camera.center, zoom);
                },
              ),
            ],
          ),
        ),
      ],
    );
  }

  LatLng _calculateCenter(List<Marker> markers) {
    if (markers.isEmpty) return const LatLng(0, 0);
    double latSum = 0;
    double lngSum = 0;
    for (var m in markers) {
      latSum += m.point.latitude;
      lngSum += m.point.longitude;
    }
    return LatLng(latSum / markers.length, lngSum / markers.length);
  }

  List<Marker> _buildMarkers(BuildContext context) {
    List<Marker> markers = [];
    bool companyAdded = false;

    double getLat(dynamic val) => (val as num?)?.toDouble() ?? 0.0;
    double getLng(dynamic val) => (val as num?)?.toDouble() ?? 0.0;

    // A. Employees
    for (var emp in widget.employees) {
      double pLat = getLat(emp['pickup_lat']);
      double pLng = getLng(emp['pickup_lng']);
      double dLat = getLat(emp['drop_lat']);
      double dLng = getLng(emp['drop_lng']);

      if (pLat != 0 && pLng != 0) {
        markers.add(
          Marker(
            point: LatLng(pLat, pLng),
            width: 40,
            height: 40,
            child: const Icon(
              Icons.person_pin_circle,
              color: AppColors.markerEmployee,
              size: 35,
            ),
          ),
        );
      }

      if (!companyAdded && dLat != 0 && dLng != 0) {
        markers.add(
          Marker(
            point: LatLng(dLat, dLng),
            width: 50,
            height: 50,
            child: const Icon(
              Icons.business,
              color: AppColors.markerCompany,
              size: 45,
            ),
          ),
        );
        companyAdded = true;
      }
    }

    // B. Vehicles
    for (var veh in widget.vehicles) {
      final isPremium = veh['category'].toString().toLowerCase() == 'premium';
      double cLat = getLat(veh['current_lat']);
      double cLng = getLng(veh['current_lng']);

      if (cLat != 0 && cLng != 0) {
        markers.add(
          Marker(
            point: LatLng(cLat, cLng),
            width: 40,
            height: 40,
            child: Icon(
              Icons.directions_car,
              color: isPremium
                  ? AppColors.markerPremium
                  : AppColors.markerNormal,
              size: 35,
            ),
          ),
        );
      }
    }

    return markers;
  }
}
