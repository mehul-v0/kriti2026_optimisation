import 'package:flutter/material.dart';

class AppSnackbar {
  static const Duration _snackBarDuration = Duration(seconds: 2);

  /// [message] is the raw text (success or error).
  /// [isError] controls the color scheme.
  static void show(
    BuildContext context, {
    required String message,
    bool isError = false,
  }) {
    ScaffoldMessenger.of(context).clearSnackBars();

    final screenWidth = MediaQuery.of(context).size.width;
    final bool isWideScreen = screenWidth > 600;

    // 4. Color Logic
    final backgroundColor = isError
        ? Theme.of(context).colorScheme.error
        : const Color(0xFF616161);
    final icon = isError ? Icons.error_outline : Icons.check_circle_outline;

    final snackBar = SnackBar(
      behavior: SnackBarBehavior.floating,
      elevation: 6,
      backgroundColor: backgroundColor,
      duration: _snackBarDuration,

      // Responsive Width for Web/Tablet
      width: isWideScreen ? 400 : null,

      // Standard margins for Mobile
      margin: isWideScreen ? null : const EdgeInsets.all(16),

      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),

      content: Row(
        children: [
          Icon(icon, color: Colors.white, size: 28),

          SizedBox(width: screenWidth * 0.03),

          //Text Handling: Flexible allows text to wrap if error is long
          Expanded(
            child: Text(
              // Clean up the error message before displaying
              _formatMessage(message),
              style: const TextStyle(
                color: Colors.white,
                fontSize: 16,
                fontWeight: FontWeight.w500,
              ),
              // Scale text for accessibility settings on device
              textScaler: MediaQuery.of(context).textScaler,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
            ),
          ),

          // Close button
          GestureDetector(
            onTap: () => ScaffoldMessenger.of(context).hideCurrentSnackBar(),
            child: Container(
              margin: const EdgeInsets.only(left: 8),
              padding: const EdgeInsets.all(4),
              decoration: BoxDecoration(
                color: Colors.white.withOpacity(0.2),
                borderRadius: BorderRadius.circular(6),
              ),
              child: const Icon(Icons.close, color: Colors.white, size: 16),
            ),
          ),
        ],
      ),
    );

    ScaffoldMessenger.of(context).showSnackBar(snackBar);
  }

  static String _formatMessage(String rawMsg) {
    if (rawMsg.contains("SocketException")) {
      return "Network Error: Check your connection.";
    }
    // Remove "Exception:" prefix if present for cleaner look
    return rawMsg.replaceAll("Exception: ", "");
  }
}
