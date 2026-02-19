import 'dart:ui'; // Required for ImageFilter
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
      title: 'Roxio Design System',
      theme: AppTheme.lightTheme,
      darkTheme: AppTheme.darkTheme,
      themeMode: _themeMode,
      // SMOOTH TRANSITION SETTINGS
      themeAnimationDuration: const Duration(milliseconds: 600),
      themeAnimationCurve: Curves.easeInOutCubic,
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
  final TextEditingController _textController = TextEditingController();

  // --- Dummy Data for Map Demo ---
  final List<Map<String, dynamic>> _dummyEmployees = [
    {
      "pickup_lat": 12.935,
      "pickup_lng": 77.625,
      "drop_lat": 12.9716,
      "drop_lng": 77.5946,
    },
  ];

  final List<Map<String, dynamic>> _dummyVehicles = [
    {"current_lat": 12.938, "current_lng": 77.620, "category": "premium"},
  ];
  // -------------------------------

  void _triggerLoading() async {
    setState(() => _isLoading = true);
    await Future.delayed(const Duration(seconds: 2));
    if (mounted) {
      setState(() => _isLoading = false);
      AppSnackbar.show(context, message: "Data refreshed successfully!");
    }
  }

  void _triggerError() {
    AppSnackbar.show(
      context,
      message: "Connection Timeout: Unable to reach server.",
      isError: true,
    );
  }

  @override
  Widget build(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;

    return LoadingOverlay(
      isLoading: _isLoading,
      child: Scaffold(
        // Extending body behind app bar for better glass effect visibility if needed
        extendBodyBehindAppBar: false,
        appBar: AppBar(
          title: Text(
            "Roxio UI Kit",
            style: Theme.of(context).textTheme.headlineSmall,
          ),
          centerTitle: true,
          backgroundColor: Theme.of(context).scaffoldBackgroundColor,
          elevation: 0,
          actions: [
            // Smooth rotation animation for the icon could be added here
            IconButton(
              icon: Icon(
                isDark ? Icons.light_mode : Icons.dark_mode,
                color: Theme.of(context).iconTheme.color,
              ),
              onPressed: widget.onThemeToggle,
            ),
          ],
        ),
        body: SingleChildScrollView(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // 1. TYPOGRAPHY
              _buildSectionHeader(context, "Typography"),
              Text(
                "Headline Large (32px)",
                style: Theme.of(context).textTheme.headlineLarge,
              ),
              const SizedBox(height: 8),
              Text(
                "Headline Small (20px)",
                style: Theme.of(context).textTheme.headlineSmall,
              ),
              const SizedBox(height: 8),
              Text(
                "Body Medium (16px) - Standard text used for descriptions.",
                style: Theme.of(context).textTheme.bodyMedium,
              ),

              const SizedBox(height: 32),

              // 2. INPUTS
              _buildSectionHeader(context, "Inputs"),
              TextField(
                controller: _textController,
                decoration: const InputDecoration(
                  hintText: "Enter your email...",
                  prefixIcon: Icon(Icons.email_outlined),
                ),
              ),
              const SizedBox(height: 16),
              TextField(
                obscureText: true,
                decoration: const InputDecoration(
                  hintText: "Password",
                  prefixIcon: Icon(Icons.lock_outline),
                  suffixIcon: Icon(Icons.visibility_off_outlined),
                ),
              ),

              const SizedBox(height: 32),

              // 3. BUTTONS
              _buildSectionHeader(context, "Buttons & Actions"),
              ElevatedButton(
                onPressed: () {},
                child: const Text("Primary Action"),
              ),
              const SizedBox(height: 16),
              ElevatedButton.icon(
                onPressed: _triggerLoading,
                style: ElevatedButton.styleFrom(
                  backgroundColor: isDark
                      ? AppColors.darkSurface
                      : Colors.grey[200],
                  foregroundColor: isDark ? Colors.white : Colors.black87,
                ),
                icon: const Icon(Icons.refresh),
                label: const Text("Test Spinner Overlay"),
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  Expanded(
                    child: ElevatedButton(
                      onPressed: () =>
                          AppSnackbar.show(context, message: "Success!"),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: AppColors.success,
                      ),
                      child: const Text("Success"),
                    ),
                  ),
                  const SizedBox(width: 16),
                  Expanded(
                    child: ElevatedButton(
                      onPressed: _triggerError,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: AppColors.error,
                      ),
                      child: const Text("Error"),
                    ),
                  ),
                ],
              ),

              const SizedBox(height: 32),

              // 4. MAP WIDGET
              _buildSectionHeader(context, "Live Map Style"),

              // Map Container
              Container(
                height: 300,
                width: double.infinity,
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(20),
                  border: Border.all(
                    color: isDark
                        ? AppColors.darkBorderColor
                        : AppColors.borderColor,
                    width: 2,
                  ),
                  boxShadow: [
                    BoxShadow(
                      color: Colors.black.withOpacity(0.05),
                      blurRadius: 10,
                      offset: const Offset(0, 4),
                    ),
                  ],
                ),
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(18),
                  child: MapViewWidget(
                    employees: _dummyEmployees,
                    vehicles: _dummyVehicles,
                  ),
                ),
              ),

              const SizedBox(height: 20),

              // GLASS BUTTON (Outside Map)
              _buildGlassButton(context, isDark),

              const SizedBox(height: 50),
            ],
          ),
        ),
      ),
    );
  }

  // Helper to build the Blurry Glass Button
  Widget _buildGlassButton(BuildContext context, bool isDark) {
    return ClipRRect(
      borderRadius: BorderRadius.circular(30), // Pill shape
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 10.0, sigmaY: 10.0),
        child: Container(
          width: double.infinity,
          height: 54, // Match standard button height
          decoration: BoxDecoration(
            // Semi-transparent color
            color: isDark
                ? Colors.white.withOpacity(0.1)
                : Colors.black.withOpacity(0.05),
            borderRadius: BorderRadius.circular(30),
            border: Border.all(
              color: isDark
                  ? Colors.white.withOpacity(0.2)
                  : Colors.black.withOpacity(0.1),
              width: 1.5,
            ),
          ),
          child: Material(
            color: Colors.transparent,
            child: InkWell(
              onTap: () =>
                  AppSnackbar.show(context, message: "Glass Button Pressed"),
              child: Center(
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(
                      Icons.map_outlined,
                      color: Theme.of(context).textTheme.headlineSmall?.color,
                    ),
                    const SizedBox(width: 8),
                    Text(
                      "Find Nearby",
                      style: TextStyle(
                        color: Theme.of(context).textTheme.headlineSmall?.color,
                        fontWeight: FontWeight.bold,
                        fontSize: 16,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildSectionHeader(BuildContext context, String title) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            title.toUpperCase(),
            style: TextStyle(
              color: AppColors.primaryBrand,
              fontWeight: FontWeight.bold,
              letterSpacing: 1.2,
              fontSize: 12,
            ),
          ),
          const SizedBox(height: 4),
          Container(
            height: 2,
            width: 40,
            color: AppColors.primaryBrand.withOpacity(0.3),
          ),
        ],
      ),
    );
  }
}
