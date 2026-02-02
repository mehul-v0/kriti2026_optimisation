import 'package:flutter/material.dart';

// 1. Define your raw colors here (Centralized Control)
class AppColors {
  // Brand Colors
  static const Color primaryBrand = Color(0xFF2563EB); // Royal Blue
  static const Color darkBrand = Color(0xFF1E40AF);

  // State Colors (Success/Error)
  static const Color success = Color(0xFF16A34A); // Modern Green
  static const Color error = Color(0xFFDC2626); // Modern Red
  static const Color warning = Color(0xFFD97706); // Amber

  // Neutral Colors
  static const Color lightBackground = Color(0xFFF8FAFC); // Very light grey
  static const Color darkBackground = Color(0xFF111827); // Deep blue-black

  static const Color lightSurface = Colors.white;
  static const Color darkSurface = Color(0xFF1F2937);

  static const Color borderColor = Color(0xFFE2E8F0);
  static const Color darkBorderColor = Color(0xFF374151);

  // --- NEW: Marker Colors for Map ---
  static const Color markerEmployee = Color(0xFF3B82F6); // Blue
  static const Color markerPremium = Color(0xFFF59E0B); // Gold
  static const Color markerNormal = Color(0xFF64748B); // Slate Grey
  static const Color markerCompany = Color(0xFFEF4444); // Red
}

class AppTheme {
  // ... (Your existing Light and Dark theme code remains exactly the same)
  // LIGHT THEME
  static final ThemeData lightTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.light,
    primaryColor: AppColors.primaryBrand,
    scaffoldBackgroundColor: AppColors.lightBackground,
    colorScheme: const ColorScheme.light(
      primary: AppColors.primaryBrand,
      secondary: AppColors.darkBrand,
      error: AppColors.error,
      surface: AppColors.lightSurface,
      outline: AppColors.borderColor,
    ),
    textTheme: const TextTheme(
      headlineLarge: TextStyle(
        color: Colors.black87,
        fontWeight: FontWeight.bold,
      ),
      bodyMedium: TextStyle(color: Colors.black87),
      headlineSmall: TextStyle(
        color: Colors.black87,
        fontWeight: FontWeight.bold,
      ),
    ),
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: AppColors.primaryBrand,
        foregroundColor: Colors.white,
        elevation: 0,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: Colors.white,
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(8),
        borderSide: const BorderSide(color: AppColors.borderColor),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(8),
        borderSide: const BorderSide(color: AppColors.borderColor),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(8),
        borderSide: const BorderSide(color: AppColors.primaryBrand, width: 2),
      ),
    ),
  );

  // DARK THEME
  static final ThemeData darkTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    primaryColor: AppColors.primaryBrand,
    scaffoldBackgroundColor: AppColors.darkBackground,
    colorScheme: const ColorScheme.dark(
      primary: AppColors.primaryBrand,
      secondary: AppColors.darkBrand,
      error: AppColors.error,
      surface: AppColors.darkSurface,
      outline: AppColors.darkBorderColor,
    ),
    textTheme: const TextTheme(
      headlineLarge: TextStyle(
        color: Colors.white,
        fontWeight: FontWeight.bold,
      ),
      bodyMedium: TextStyle(color: Color(0xFFE5E7EB)),
      headlineSmall: TextStyle(
        color: Colors.white,
        fontWeight: FontWeight.bold,
      ),
    ),
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: AppColors.primaryBrand,
        foregroundColor: Colors.white,
        elevation: 0,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: AppColors.darkSurface,
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(8),
        borderSide: const BorderSide(color: AppColors.darkBorderColor),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(8),
        borderSide: const BorderSide(color: AppColors.darkBorderColor),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(8),
        borderSide: const BorderSide(color: AppColors.primaryBrand, width: 2),
      ),
    ),
  );
}
