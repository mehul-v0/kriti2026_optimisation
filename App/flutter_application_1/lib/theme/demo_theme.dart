import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/widgets/map_view.dart';

void main() {
  runApp(const ThemeDemoApp());
}

class ThemeDemoApp extends StatefulWidget {
  const ThemeDemoApp({super.key});

  @override
  State<ThemeDemoApp> createState() => _ThemeDemoAppState();
}

class _ThemeDemoAppState extends State<ThemeDemoApp> {
  ThemeMode _themeMode = ThemeMode.light;

  void toggleTheme() {
    setState(() {
      _themeMode = _themeMode == ThemeMode.light
          ? ThemeMode.dark
          : ThemeMode.light;
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Theme Demo',
      theme: AppTheme.lightTheme,
      darkTheme: AppTheme.darkTheme,
      themeMode: _themeMode,
      home: DemoPage(onThemeToggle: toggleTheme),
    );
  }
}

class DemoPage extends StatefulWidget {
  final VoidCallback onThemeToggle;
  const DemoPage({super.key, required this.onThemeToggle});

  @override
  State<DemoPage> createState() => _DemoPageState();
}

class _DemoPageState extends State<DemoPage> {
  bool _isLoading = false;

  // --- Dummy Data for Map Demo ---
  final List<Map<String, dynamic>> _dummyEmployees = [
    {
      "pickup_lat": 12.935,
      "pickup_lng": 77.625,
      "drop_lat": 12.9716,
      "drop_lng": 77.5946,
    },
    {
      "pickup_lat": 12.942,
      "pickup_lng": 77.610,
      "drop_lat": 12.9716,
      "drop_lng": 77.5946,
    },
  ];

  final List<Map<String, dynamic>> _dummyVehicles = [
    {"current_lat": 12.938, "current_lng": 77.620, "category": "premium"},
    {"current_lat": 12.950, "current_lng": 77.630, "category": "normal"},
  ];
  // -------------------------------

  void _triggerLoading() async {
    setState(() => _isLoading = true);
    await Future.delayed(const Duration(seconds: 3));
    if (mounted) {
      setState(() => _isLoading = false);
      AppSnackbar.show(context, message: "Process Finished!");
    }
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;

    return LoadingOverlay(
      isLoading: _isLoading,
      child: Scaffold(
        appBar: AppBar(
          title: const Text("Theme & Map Demo"),
          actions: [
            IconButton(
              icon: const Icon(Icons.brightness_6),
              onPressed: widget.onThemeToggle,
              tooltip: "Toggle Dark Mode",
            ),
          ],
        ),
        body: SingleChildScrollView(
          padding: EdgeInsets.all(size.width * 0.05),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // 1. Text Styles
              Text(
                "Typography",
                style: Theme.of(context).textTheme.headlineLarge,
              ),
              const SizedBox(height: 10),
              Text(
                "Body text changes color based on theme.",
                style: Theme.of(context).textTheme.bodyMedium,
              ),

              const Divider(height: 40),

              // 2. Buttons
              Text("Buttons", style: Theme.of(context).textTheme.headlineLarge),
              const SizedBox(height: 15),
              Wrap(
                spacing: 10,
                runSpacing: 10,
                children: [
                  ElevatedButton(
                    onPressed: () {},
                    child: const Text("Primary"),
                  ),
                  OutlinedButton(
                    onPressed: () {},
                    child: const Text("Outlined"),
                  ),
                  IconButton(
                    onPressed: () {},
                    icon: const Icon(Icons.thumb_up),
                    color: Theme.of(context).colorScheme.primary,
                  ),
                ],
              ),

              const Divider(height: 40),

              // 3. Components
              Text(
                "Components",
                style: Theme.of(context).textTheme.headlineLarge,
              ),
              const SizedBox(height: 15),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  style: ElevatedButton.styleFrom(
                    backgroundColor: AppColors.success,
                  ),
                  icon: const Icon(Icons.check),
                  label: const Text("Success Snackbar"),
                  onPressed: () {
                    AppSnackbar.show(context, message: "Success!");
                  },
                ),
              ),
              const SizedBox(height: 10),
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  icon: const Icon(Icons.hourglass_bottom),
                  label: const Text("Test Spinner (3s)"),
                  onPressed: _triggerLoading,
                ),
              ),

              const Divider(height: 40),

              // 4. Map Integration Demo
              Text(
                "Map Widget",
                style: Theme.of(context).textTheme.headlineLarge,
              ),
              const SizedBox(height: 10),
              Text(
                "Toggling theme inverts map colors. Use buttons to Relocate/Rotate.",
                style: Theme.of(context).textTheme.bodyMedium,
              ),
              const SizedBox(height: 15),

              // Map Container
              ClipRRect(
                borderRadius: BorderRadius.circular(16),
                child: Container(
                  height: 400, // Fixed height for map in scroll view
                  decoration: BoxDecoration(
                    border: Border.all(color: AppColors.borderColor),
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: MapViewWidget(
                    employees: _dummyEmployees,
                    vehicles: _dummyVehicles,
                  ),
                ),
              ),

              const SizedBox(height: 50), // Bottom padding
            ],
          ),
        ),
      ),
    );
  }
}

// Simple Mock for SocketException
class SocketException implements Exception {
  final String message;
  const SocketException(this.message);
  @override
  String toString() => "SocketException: $message";
}
