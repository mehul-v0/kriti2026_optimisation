import 'package:flutter/material.dart';
import 'package:lottie/lottie.dart';

/// Determines which Lottie animation the spinner displays.
enum SpinnerType {
  /// Regular loading (e.g. fetching data, running optimisation).
  /// Uses `assets/loading_animation.json`.
  loading,

  /// File export / download action triggered by the download button.
  /// Uses `assets/Downloading.json`.
  downloading,
}

class GlobalLoader {
  static const String _loadingPath = 'assets/loading_animation.json';
  static const String _downloadingPath = 'assets/Downloading.json';

  static String pathFor(SpinnerType type) {
    switch (type) {
      case SpinnerType.downloading:
        return _downloadingPath;
      case SpinnerType.loading:
        return _loadingPath;
    }
  }
}

/// Puts the loader on top of your content when [isLoading] is true.
///
/// Pass [spinnerType] to control which animation is shown:
/// - [SpinnerType.loading]     → `loading_animation.json`  (default)
/// - [SpinnerType.downloading] → `Downloading.json`
class LoadingOverlay extends StatelessWidget {
  final Widget child;
  final bool isLoading;
  final SpinnerType spinnerType;

  const LoadingOverlay({
    Key? key,
    required this.child,
    required this.isLoading,
    this.spinnerType = SpinnerType.loading,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    return Stack(
      children: [
        child,
        if (isLoading)
          Container(
            color: isDark
                ? Colors.black.withOpacity(0.65)
                : Colors.black.withOpacity(0.45),
            child: Center(child: UniversalSpinner(spinnerType: spinnerType)),
          ),
      ],
    );
  }
}

/// The actual spinner widget.
///
/// Picks the correct Lottie file based on [spinnerType], wraps it in a
/// themed card box, and falls back to a [CircularProgressIndicator] if
/// the asset cannot be loaded.
class UniversalSpinner extends StatelessWidget {
  final SpinnerType spinnerType;

  const UniversalSpinner({Key? key, this.spinnerType = SpinnerType.loading})
    : super(key: key);

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;
    final screenWidth = MediaQuery.of(context).size.width;

    // Responsive Size: 30% of screen width, capped between 50 – 150 px
    final double spinnerSize = (screenWidth * 0.3).clamp(50.0, 150.0);

    // Always use a white box so the Lottie animations (dark outlines) are
    // legible regardless of the app theme.  In dark mode we add a white glow
    // shadow so the box lifts off the dark overlay.
    const Color boxColor = Colors.white;

    final Color shadowColor = isDark
        ? Colors.white.withOpacity(0.25) // white glow in dark mode
        : Colors.black.withOpacity(0.12);

    return Container(
      width: spinnerSize,
      height: spinnerSize,
      decoration: BoxDecoration(
        color: boxColor,
        borderRadius: BorderRadius.circular(12),
        boxShadow: [
          BoxShadow(
            color: shadowColor,
            blurRadius: isDark ? 24 : 16,
            spreadRadius: isDark ? 4 : 2,
            offset: const Offset(0, 4),
          ),
        ],
        border: Border.all(
          color: isDark
              ? Colors.white.withOpacity(0.6) // visible white outline
              : Colors.black.withOpacity(0.06),
          width: isDark ? 1.5 : 1,
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.all(10.0),
        child: Lottie.asset(
          GlobalLoader.pathFor(spinnerType),
          fit: BoxFit.contain,

          // Safety: falls back to a themed CircularProgressIndicator
          errorBuilder: (context, error, stackTrace) {
            return Center(
              child: CircularProgressIndicator(
                valueColor: AlwaysStoppedAnimation<Color>(
                  theme.colorScheme.primary,
                ),
                strokeWidth: 3,
              ),
            );
          },
        ),
      ),
    );
  }
}
