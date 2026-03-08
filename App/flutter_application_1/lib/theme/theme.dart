import 'package:flutter/material.dart';

class AppColors {
  // --- BRAND COLORS (Petronas defaults – widgets that can't access context use these) ---
  static const Color primaryBrand = Color(0xFF00D2BE);
  static const Color darkBrand = Color(0xFF00A19C);

  // --- BACKGROUNDS ---
  static const Color darkBackground = Color(0xFF090909);
  static const Color darkSurface = Color(0xFF1A1A1A);
  static const Color lightBackground = Color(0xFFEAEAEA);
  static const Color lightSurface = Color(0xFFF5F5F5);

  // --- STATE COLORS ---
  static const Color success = Color(0xFF00D2BE);
  static const Color error = Color(0xFFEF4444);
  static const Color warning = Color(0xFFF59E0B);

  // --- BORDERS & NEUTRALS ---
  static const Color borderColor = Color(0xFFB0B0B0);
  static const Color darkBorderColor = Color(0xFF2E2E2E);

  static const Color textPrimaryLight = Color(0xFF0A0A0A);
  static const Color textSecondaryLight = Color(0xFF5A5A5A);

  static const Color silver = Color(0xFFC0C0C0);
  static const Color chromeSilver = Color(0xFFE8E8E8);

  // --- MAP COLORS ---
  static const Color routeLine = primaryBrand;
  static const Color markerEmployee = silver;
  static const Color markerPremium = primaryBrand;
  static const Color markerNormal = silver;
  static const Color markerCompany = primaryBrand;

  // --- ROUTE COLORS ---
  static const Color tripAccent = Color(0xFF9E9E9E);
  static const Color officeAccent = Color(0xFF9E9E9E);
}

class AppThemeData {
  // Map tile ColorFilter matrices (used in map rendering)
  static const List<double> mapDarkMatrix = [
    -0.164,
    -0.323,
    -0.063,
    0,
    140,
    -0.215,
    -0.423,
    -0.082,
    0,
    184,
    -0.197,
    -0.387,
    -0.075,
    0,
    168,
    0,
    0,
    0,
    1,
    0,
  ];

  static const List<double> mapLightMatrix = [
    0.68,
    0.22,
    0.06,
    0,
    0,
    0.14,
    0.78,
    0.08,
    0,
    0,
    0.14,
    0.22,
    0.64,
    0,
    12,
    0,
    0,
    0,
    1,
    0,
  ];

  // Neutral dark matrix — balanced charcoal/grey for Orange & Yellow themes
  static const List<double> mapDarkNeutralMatrix = [
    -0.2,
    -0.35,
    -0.08,
    0,
    150,
    -0.2,
    -0.35,
    -0.08,
    0,
    150,
    -0.2,
    -0.35,
    -0.08,
    0,
    150,
    0,
    0,
    0,
    1,
    0,
  ];
}

// F1-inspired colour themes: 0 = Petronas/Mercedes, 1 = McLaren, 2 = Velora
enum AppThemeVariant { petronas, mclaren, velora }

class AppTheme {
  // Accent primary and dark container per variant (index matches AppThemeVariant)
  static const _primary = [
    Color(0xFF00D2BE),
    Color(0xFFFF8000),
    Color(0xFFF5B800),
  ];
  static const _container = [
    Color(0xFF00A19C),
    Color(0xFFCC6600),
    Color(0xFFCC9800),
  ];

  static ThemeData lightTheme(AppThemeVariant v) => _build(v, Brightness.light);
  static ThemeData darkTheme(AppThemeVariant v) => _build(v, Brightness.dark);

  // Index-based helpers for use with ValueNotifier<int>
  static ThemeData lightThemeAt(int i) => lightTheme(AppThemeVariant.values[i]);
  static ThemeData darkThemeAt(int i) => darkTheme(AppThemeVariant.values[i]);

  static ThemeData _build(AppThemeVariant v, Brightness brightness) {
    final p = _primary[v.index];
    final con = _container[v.index];
    final isDark = brightness == Brightness.dark;

    return ThemeData(
      useMaterial3: true,
      brightness: brightness,
      primaryColor: p,
      scaffoldBackgroundColor: isDark
          ? AppColors.darkBackground
          : AppColors.lightBackground,
      colorScheme: isDark
          ? ColorScheme.dark(
              primary: p,
              onPrimary: Colors.black,
              primaryContainer: con,
              secondary: AppColors.silver,
              onSecondary: Colors.black,
              error: AppColors.error,
              surface: AppColors.darkSurface,
              onSurface: AppColors.chromeSilver,
              outline: AppColors.darkBorderColor,
            )
          : ColorScheme.light(
              primary: p,
              onPrimary: Colors.black,
              primaryContainer: con,
              secondary: AppColors.darkBackground,
              onSecondary: Colors.white,
              error: AppColors.error,
              surface: AppColors.lightSurface,
              onSurface: AppColors.textPrimaryLight,
              outline: AppColors.borderColor,
            ),
      textTheme: isDark ? _darkText : _lightText,
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: isDark ? p : AppColors.darkBackground,
          foregroundColor: isDark ? Colors.black : AppColors.chromeSilver,
          elevation: 0,
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(4)),
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
          minimumSize: const Size(double.infinity, 54),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: isDark ? AppColors.darkSurface : AppColors.lightSurface,
        contentPadding: const EdgeInsets.symmetric(
          horizontal: 20,
          vertical: 16,
        ),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(6),
          borderSide: BorderSide(
            color: isDark ? AppColors.darkBorderColor : AppColors.borderColor,
          ),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(6),
          borderSide: BorderSide(
            color: isDark ? AppColors.darkBorderColor : AppColors.borderColor,
          ),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(6),
          borderSide: BorderSide(color: p, width: 2),
        ),
        hintStyle: TextStyle(
          color: isDark ? AppColors.silver : AppColors.textSecondaryLight,
        ),
      ),
      iconTheme: IconThemeData(color: isDark ? p : AppColors.darkBackground),
    );
  }

  static const _lightText = TextTheme(
    headlineLarge: TextStyle(
      color: AppColors.textPrimaryLight,
      fontWeight: FontWeight.w800,
      fontSize: 32,
      letterSpacing: -1.0,
    ),
    headlineSmall: TextStyle(
      color: AppColors.textPrimaryLight,
      fontWeight: FontWeight.w700,
      fontSize: 20,
      letterSpacing: -0.3,
    ),
    bodyMedium: TextStyle(color: AppColors.textSecondaryLight, fontSize: 16),
    labelLarge: TextStyle(fontWeight: FontWeight.w700, letterSpacing: 1.2),
  );

  static const _darkText = TextTheme(
    headlineLarge: TextStyle(
      color: AppColors.chromeSilver,
      fontWeight: FontWeight.w800,
      fontSize: 32,
      letterSpacing: -1.0,
    ),
    headlineSmall: TextStyle(
      color: AppColors.chromeSilver,
      fontWeight: FontWeight.w700,
      fontSize: 20,
      letterSpacing: -0.3,
    ),
    bodyMedium: TextStyle(color: AppColors.silver, fontSize: 16),
    labelLarge: TextStyle(fontWeight: FontWeight.w700, letterSpacing: 1.2),
  );
}

/// Convenience extension so widgets can write `context.primary` instead of
/// `Theme.of(context).colorScheme.primary`.
extension AppThemeExt on BuildContext {
  Color get primary => Theme.of(this).colorScheme.primary;
  Color get primaryDark => Theme.of(this).colorScheme.primaryContainer;
}
