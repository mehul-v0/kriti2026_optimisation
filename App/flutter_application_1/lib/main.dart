import 'package:flutter/material.dart';
import 'package:flutter_application_1/screen/auth_gate.dart';
import 'package:flutter_application_1/services/auth_service.dart';
import 'theme/theme.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await SupabaseConfig.initialize();

  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  // Global theme notifier
  final ValueNotifier<ThemeMode> _themeNotifier = ValueNotifier(ThemeMode.dark);

  // Cache the child widget so theme changes don't recreate the subtree
  late final Widget _cachedAuthGate = AuthGate(themeNotifier: _themeNotifier);

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<ThemeMode>(
      valueListenable: _themeNotifier,
      // Pass cached child so it's NOT rebuilt on theme toggle
      child: _cachedAuthGate,
      builder: (context, themeMode, child) {
        return MaterialApp(
          debugShowCheckedModeBanner: false,
          theme: AppTheme.lightTheme,
          darkTheme: AppTheme.darkTheme,
          themeMode: themeMode,
          home: child,
        );
      },
    );
  }

  @override
  void dispose() {
    _themeNotifier.dispose();
    super.dispose();
  }
}
