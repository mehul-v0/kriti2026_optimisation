import 'package:flutter/material.dart';

// 1. Define your raw colors here (Centralized Control)
class AppColors {
  // --- BRAND COLORS (Extracted from "Roxio" UI) ---
  // The vibrant green seen on the car, buttons, and logo.
  static const Color primaryBrand = Color(0xFF00C569);
  static const Color darkBrand = Color(
    0xFF00964F,
  ); // A darker shade for gradients/press states

  // --- BACKGROUNDS ---
  // The deep midnight blue/black seen on the Splash screen and Dark cards
  static const Color darkBackground = Color(0xFF10151C);
  static const Color darkSurface = Color(
    0xFF1A212B,
  ); // Slightly lighter for cards in dark mode

  static const Color lightBackground = Color(0xFFF5F7FA); // Light greyish-white
  static const Color lightSurface = Colors.white;

  // --- STATE COLORS ---
  static const Color success = Color(0xFF00C569); // Matches Brand Green
  static const Color error = Color(0xFFEF4444); // Modern Red
  static const Color warning = Color(0xFFF59E0B); // Amber

  // --- BORDERS & NEUTRALS ---
  static const Color borderColor = Color(0xFFE2E8F0);
  static const Color darkBorderColor = Color(0xFF2D3748);

  static const Color textPrimaryLight = Color(0xFF111827); // Nearly black
  static const Color textSecondaryLight = Color(0xFF6B7280); // Grey text

  // --- MAP COLORS ---
  // Specific colors for your map implementation
  static const Color routeLine = primaryBrand; // The green line in the image
  static const Color markerEmployee = Color(0xFF3B82F6); // Blue
  static const Color markerPremium = Color(0xFFF59E0B); // Gold
  static const Color markerNormal = Color(0xFF64748B); // Slate Grey
  static const Color markerCompany = primaryBrand; // Green for brand
}

class AppTheme {
  // ================= LIGHT THEME =================
  static final ThemeData lightTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.light,
    primaryColor: AppColors.primaryBrand,
    scaffoldBackgroundColor: AppColors.lightBackground,

    // Color Scheme defines the palette for standard widgets (Fab, SnackBar, etc.)
    colorScheme: const ColorScheme.light(
      primary: AppColors.primaryBrand,
      onPrimary: Colors.white, // Text on green buttons
      secondary: AppColors.darkBackground, // Used for dark buttons/accents
      onSecondary: Colors.white,
      error: AppColors.error,
      surface: AppColors.lightSurface,
      onSurface: AppColors.textPrimaryLight,
      outline: AppColors.borderColor,
    ),

    // Typography matching the clean, geometric sans-serif style
    textTheme: const TextTheme(
      headlineLarge: TextStyle(
        color: AppColors.textPrimaryLight,
        fontWeight: FontWeight.w700,
        fontSize: 32,
        letterSpacing: -0.5,
      ),
      headlineSmall: TextStyle(
        color: AppColors.textPrimaryLight,
        fontWeight: FontWeight.w600,
        fontSize: 20,
      ),
      bodyMedium: TextStyle(color: AppColors.textSecondaryLight, fontSize: 16),
      labelLarge: TextStyle(
        // Button text
        fontWeight: FontWeight.w600,
        letterSpacing: 0.5,
      ),
    ),

    // Button Styling (Pill/Rounded shapes as seen in "Get Started")
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: AppColors
            .darkBackground, // Dark buttons on light mode (Common in this UI)
        foregroundColor: Colors.white,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(30),
        ), // High radius for pill shape
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
        minimumSize: const Size(
          double.infinity,
          54,
        ), // Tall, full-width buttons
      ),
    ),

    // Input Fields (Clean, minimal borders)
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: Colors.white,
      contentPadding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: BorderSide.none, // Cleaner look
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: Colors.transparent),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: AppColors.primaryBrand, width: 1.5),
      ),
      hintStyle: TextStyle(color: Colors.grey.shade400),
    ),

    // Icon Theme
    iconTheme: const IconThemeData(color: AppColors.darkBackground),
  );

  // ================= DARK THEME =================
  static final ThemeData darkTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    primaryColor: AppColors.primaryBrand,
    scaffoldBackgroundColor: AppColors.darkBackground,

    colorScheme: const ColorScheme.dark(
      primary: AppColors.primaryBrand,
      onPrimary: Colors.black, // Green is bright, so black text looks better
      secondary: Colors.white,
      onSecondary: Colors.black,
      error: AppColors.error,
      surface: AppColors.darkSurface,
      onSurface: Colors.white,
      outline: AppColors.darkBorderColor,
    ),

    textTheme: const TextTheme(
      headlineLarge: TextStyle(
        color: Colors.white,
        fontWeight: FontWeight.w700,
        fontSize: 32,
        letterSpacing: -0.5,
      ),
      headlineSmall: TextStyle(
        color: Colors.white,
        fontWeight: FontWeight.w600,
        fontSize: 20,
      ),
      bodyMedium: TextStyle(
        color: Color(0xFF9CA3AF), // Lighter grey for dark mode
        fontSize: 16,
      ),
    ),

    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor:
            AppColors.primaryBrand, // Green buttons pop in dark mode
        foregroundColor: Colors.white,
        elevation: 0,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(30)),
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
        minimumSize: const Size(double.infinity, 54),
      ),
    ),

    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: AppColors.darkSurface,
      contentPadding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: BorderSide.none,
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: Colors.transparent),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: AppColors.primaryBrand, width: 1.5),
      ),
      hintStyle: TextStyle(color: Colors.grey.shade600),
    ),

    iconTheme: const IconThemeData(color: Colors.white),
  );
}
