class MapConfig {
  // Currently using OpenStreetMap (No key required)
  static const String tileUrlTemplate =
      'https://tile.openstreetmap.org/{z}/{x}/{y}.png';

  static const String packageName = 'com.example.flutter_application_1';

  // Default center if data is empty
  static const double defaultLat = 12.9716;
  static const double defaultLng = 77.5946;
  static const double defaultZoom = 13.0;
}
