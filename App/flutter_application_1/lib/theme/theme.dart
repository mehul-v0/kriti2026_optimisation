import 'package:flutter/material.dart';

// 1. Define your raw colors here (Centralized Control)
class AppColors {
  // --- BRAND COLORS (Mercedes-AMG F1 x Petronas) ---
  // Iconic Petronas teal — the signature colour of the W-series livery
  static const Color primaryBrand = Color(0xFF00D2BE);
  static const Color darkBrand = Color(
    0xFF00A19C,
  ); // Deeper teal for gradients / press states

  // --- BACKGROUNDS ---
  // Carbon-black cockpit feel
  static const Color darkBackground = Color(0xFF090909);
  static const Color darkSurface = Color(
    0xFF1A1A1A,
  ); // Slightly lighter for cards / panels

  // Silver livery-inspired light mode
  static const Color lightBackground = Color(0xFFEAEAEA); // Brushed silver
  static const Color lightSurface = Color(0xFFF5F5F5); // Near-white silver

  // --- STATE COLORS ---
  static const Color success = Color(0xFF00D2BE); // Petronas teal
  static const Color error = Color(0xFFEF4444); // Red flag
  static const Color warning = Color(0xFFF59E0B); // Safety-car amber

  // --- BORDERS & NEUTRALS ---
  static const Color borderColor = Color(0xFFB0B0B0); // Silver trim
  static const Color darkBorderColor = Color(0xFF2E2E2E); // Dark carbon border

  static const Color textPrimaryLight = Color(
    0xFF0A0A0A,
  ); // Near-black on silver
  static const Color textSecondaryLight = Color(0xFF5A5A5A); // Mid-grey

  // Silver accents
  static const Color silver = Color(0xFFC0C0C0);
  static const Color chromeSilver = Color(0xFFE8E8E8);

  // --- MAP COLORS ---
  static const Color routeLine = primaryBrand; // Petronas teal route line
  static const Color markerEmployee =
      silver; // Whitish silver for all employees
  static const Color markerPremium =
      primaryBrand; // Petronas teal for premium vehicles
  static const Color markerNormal =
      silver; // Whitish silver for normal vehicles
  static const Color markerCompany = primaryBrand; // Teal for HQ

  // --- ROUTE / TRIP SECTION COLORS ---
  // Neutral grey accents for trip labels and office indicators in route cards
  static const Color tripAccent = Color(
    0xFF9E9E9E,
  ); // Grey for trip header badges
  static const Color officeAccent = Color(
    0xFF9E9E9E,
  ); // Grey for office stop indicators
}

class AppThemeData {
  // ── Map tile color-filter matrices ───────────────────────────────────────
  // Centralised here so changing the map style only requires editing one place.
  //
  // Dark mode: luminance-invert → steel-silver base with light teal cast
  //   R: 0.55 × invLum  (high silver → dark steel-grey)
  //   G: 0.72 × invLum  (slightly higher → gentle teal tint)
  //   B: 0.66 × invLum  (slightly higher → gentle teal tint)
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

  // Light mode: partial desaturation → brushed-silver / cool-grey cast
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
}

class AppTheme {
  // ================= LIGHT THEME (Silver / Petronas) =================
  static final ThemeData lightTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.light,
    primaryColor: AppColors.primaryBrand,
    scaffoldBackgroundColor: AppColors.lightBackground,

    // Color Scheme — silver livery, Petronas teal accents
    colorScheme: const ColorScheme.light(
      primary: AppColors.primaryBrand,
      onPrimary: Colors.black, // Black text on Petronas teal
      secondary: AppColors.darkBackground, // Carbon-black accent
      onSecondary: Colors.white,
      error: AppColors.error,
      surface: AppColors.lightSurface,
      onSurface: AppColors.textPrimaryLight,
      outline: AppColors.borderColor,
    ),

    // Typography — bold, tight spacing like Mercedes race graphics
    textTheme: const TextTheme(
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
      labelLarge: TextStyle(
        fontWeight: FontWeight.w700,
        letterSpacing: 1.2, // Wide tracking for race-style labels
      ),
    ),

    // Buttons — carbon-black with silver text
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: AppColors.darkBackground,
        foregroundColor: AppColors.chromeSilver,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(4), // Sharp, race-car geometry
        ),
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
        minimumSize: const Size(double.infinity, 54),
      ),
    ),

    // Input Fields
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: AppColors.lightSurface,
      contentPadding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(6),
        borderSide: const BorderSide(color: AppColors.borderColor),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(6),
        borderSide: const BorderSide(color: AppColors.borderColor),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(6),
        borderSide: const BorderSide(color: AppColors.primaryBrand, width: 2),
      ),
      hintStyle: const TextStyle(color: AppColors.textSecondaryLight),
    ),

    // Icon Theme
    iconTheme: const IconThemeData(color: AppColors.darkBackground),
  );

  // ================= DARK THEME (Carbon-Black / Petronas) =================
  static final ThemeData darkTheme = ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    primaryColor: AppColors.primaryBrand,
    scaffoldBackgroundColor: AppColors.darkBackground,

    colorScheme: const ColorScheme.dark(
      primary: AppColors.primaryBrand, // Petronas teal
      onPrimary: Colors.black, // Black on teal — looks sharp
      secondary: AppColors.silver, // Silver as dark-mode secondary
      onSecondary: Colors.black,
      error: AppColors.error,
      surface: AppColors.darkSurface,
      onSurface: AppColors.chromeSilver, // Silver text on dark surfaces
      outline: AppColors.darkBorderColor,
    ),

    textTheme: const TextTheme(
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
      bodyMedium: TextStyle(
        color: AppColors.silver, // Muted silver body text
        fontSize: 16,
      ),
      labelLarge: TextStyle(fontWeight: FontWeight.w700, letterSpacing: 1.2),
    ),

    // Buttons — Petronas teal with black text
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: AppColors.primaryBrand,
        foregroundColor: Colors.black,
        elevation: 0,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(4)),
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
        minimumSize: const Size(double.infinity, 54),
      ),
    ),

    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: AppColors.darkSurface,
      contentPadding: const EdgeInsets.symmetric(horizontal: 20, vertical: 16),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(6),
        borderSide: const BorderSide(color: AppColors.darkBorderColor),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(6),
        borderSide: const BorderSide(color: AppColors.darkBorderColor),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(6),
        borderSide: const BorderSide(color: AppColors.primaryBrand, width: 2),
      ),
      hintStyle: const TextStyle(color: AppColors.silver),
    ),

    iconTheme: const IconThemeData(color: AppColors.primaryBrand),
  );
}
