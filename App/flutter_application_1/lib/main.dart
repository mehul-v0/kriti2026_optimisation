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
  final ValueNotifier<ThemeMode> _themeNotifier = ValueNotifier(ThemeMode.dark);
  final ValueNotifier<int> _themeIndexNotifier = ValueNotifier(0);

  // Cached to avoid recreating the subtree on theme changes
  late final Widget _cachedAuthGate = AuthGate(
    themeNotifier: _themeNotifier,
    themeIndexNotifier: _themeIndexNotifier,
  );

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<ThemeMode>(
      valueListenable: _themeNotifier,
      child: _cachedAuthGate,
      builder: (context, themeMode, child) {
        return ValueListenableBuilder<int>(
          valueListenable: _themeIndexNotifier,
          child: child,
          builder: (context, themeIndex, innerChild) {
            return MaterialApp(
              debugShowCheckedModeBanner: false,
              theme: AppTheme.lightThemeAt(themeIndex),
              darkTheme: AppTheme.darkThemeAt(themeIndex),
              themeMode: themeMode,
              home: innerChild,
            );
          },
        );
      },
    );
  }

  @override
  void dispose() {
    _themeNotifier.dispose();
    _themeIndexNotifier.dispose();
    super.dispose();
  }
}
