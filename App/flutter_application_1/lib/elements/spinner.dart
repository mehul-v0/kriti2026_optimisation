import 'package:flutter/material.dart';
import 'package:lottie/lottie.dart';

class GlobalLoader {
  static const String _animationPath = 'assets/loading_animation.json';
}

/// It puts the loader on top of your content when [isLoading] is true.
class LoadingOverlay extends StatelessWidget {
  final Widget child;
  final bool isLoading;
  final String? lottiePath;

  const LoadingOverlay({
    Key? key,
    required this.child,
    required this.isLoading,
    this.lottiePath,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Stack(
      children: [
        child,
        if (isLoading)
          Container(
            color: Colors.black.withOpacity(0.5),
            child: Center(child: UniversalSpinner(lottiePath: lottiePath)),
          ),
      ],
    );
  }
}

/// The actual spinner widget.
/// It handles sizing and falls back to standard spinner if Lottie fails.
class UniversalSpinner extends StatelessWidget {
  final String? lottiePath;

  const UniversalSpinner({Key? key, this.lottiePath}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final screenWidth = MediaQuery.of(context).size.width;

    // Responsive Size: 30% of screen width, but capped at 150px max
    final double spinnerSize = (screenWidth * 0.3).clamp(50.0, 150.0);

    return SizedBox(
      width: spinnerSize,
      height: spinnerSize,
      child: Lottie.asset(
        lottiePath ?? GlobalLoader._animationPath,
        fit: BoxFit.contain,

        // Safety: If file not found or corrupt, use default Flutter spinner
        errorBuilder: (context, error, stackTrace) {
          return CircularProgressIndicator(
            valueColor: AlwaysStoppedAnimation<Color>(
              Theme.of(context).primaryColor,
            ),
            strokeWidth: 3,
          );
        },
      ),
    );
  }
}
