import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

// Theme helpers
bool _isDark(BuildContext ctx) => Theme.of(ctx).brightness == Brightness.dark;
Color _bgColor(BuildContext ctx) =>
    _isDark(ctx) ? AppColors.darkBackground : AppColors.lightBackground;
Color _surfaceColor(BuildContext ctx) =>
    _isDark(ctx) ? AppColors.darkSurface : AppColors.lightSurface;
Color _textPrimary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white : AppColors.textPrimaryLight;
Color _textSecondary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white54 : AppColors.textSecondaryLight;
Color _borderColor(BuildContext ctx) =>
    _isDark(ctx) ? AppColors.darkBorderColor : AppColors.borderColor;

// Human-readable labels and icons for each optimization mode
const Map<String, String> kModeLabels = {
  'quick': 'Quick (~15s)',
  'standard': 'Standard (~1m)',
  'advanced': 'Advanced (~5m)',
};

const Map<String, IconData> kModeIcons = {
  'quick': Icons.bolt_rounded,
  'standard': Icons.balance_rounded,
  'advanced': Icons.science_rounded,
};

/// Bottom action bar shown on both mobile and desktop layouts.
/// Contains the mode picker button and run/view CTAs.
class InputActionBar extends StatelessWidget {
  final String mode;
  final bool isLoading;
  final bool hasExistingSolution;
  final VoidCallback onRunOptimization;
  final VoidCallback onViewResults;
  final ValueChanged<String> onModeChanged;

  const InputActionBar({
    super.key,
    required this.mode,
    required this.isLoading,
    required this.hasExistingSolution,
    required this.onRunOptimization,
    required this.onViewResults,
    required this.onModeChanged,
  });

  void _showModePicker(BuildContext context) {
    final dark = _isDark(context);
    showModalBottomSheet(
      context: context,
      backgroundColor: _surfaceColor(context),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) => SafeArea(
        child: Padding(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Drag handle
              Center(
                child: Container(
                  width: 36,
                  height: 4,
                  margin: const EdgeInsets.only(bottom: 16),
                  decoration: BoxDecoration(
                    color: dark ? Colors.white24 : Colors.black12,
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
              ),
              Text(
                'Optimization Mode',
                style: TextStyle(
                  color: _textPrimary(context),
                  fontSize: 16,
                  fontWeight: FontWeight.w700,
                ),
              ),
              const SizedBox(height: 12),
              for (final entry in kModeLabels.entries)
                ListTile(
                  contentPadding: const EdgeInsets.symmetric(horizontal: 4),
                  leading: Container(
                    width: 38,
                    height: 38,
                    decoration: BoxDecoration(
                      color: mode == entry.key
                          ? context.primary.withOpacity(0.15)
                          : (dark ? Colors.white10 : Colors.black12),
                      borderRadius: BorderRadius.circular(10),
                    ),
                    child: Icon(
                      kModeIcons[entry.key]!,
                      color: mode == entry.key
                          ? context.primary
                          : _textSecondary(context),
                      size: 20,
                    ),
                  ),
                  title: Text(
                    entry.value,
                    style: TextStyle(
                      color: mode == entry.key
                          ? context.primary
                          : _textPrimary(context),
                      fontWeight: mode == entry.key
                          ? FontWeight.w700
                          : FontWeight.w500,
                      fontSize: 14,
                    ),
                  ),
                  trailing: mode == entry.key
                      ? Icon(Icons.check_rounded, color: context.primary)
                      : null,
                  onTap: () {
                    onModeChanged(entry.key);
                    Navigator.pop(ctx);
                  },
                ),
              const SizedBox(height: 4),
            ],
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 6, 12, 10),
      decoration: BoxDecoration(
        color: _bgColor(context),
        border: Border(
          top: BorderSide(color: _borderColor(context).withOpacity(0.4)),
        ),
      ),
      child: SafeArea(
        top: false,
        child: Row(
          children: [
            // Mode icon opens the picker sheet
            GestureDetector(
              onTap: () => _showModePicker(context),
              child: Container(
                padding: const EdgeInsets.all(9),
                decoration: BoxDecoration(
                  color: _surfaceColor(context),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                    color: _borderColor(context).withOpacity(0.5),
                  ),
                ),
                child: Icon(
                  kModeIcons[mode] ?? Icons.balance_rounded,
                  color: context.primary,
                  size: 22,
                ),
              ),
            ),
            const SizedBox(width: 8),

            // View existing results (only visible when a solution already exists)
            if (hasExistingSolution) ...[
              Expanded(
                child: OutlinedButton.icon(
                  onPressed: isLoading ? null : onViewResults,
                  icon: const Icon(Icons.visibility_rounded, size: 15),
                  label: const Text('View'),
                  style: OutlinedButton.styleFrom(
                    foregroundColor: context.primary,
                    side: BorderSide(color: context.primary),
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(30),
                    ),
                    minimumSize: const Size(0, 40),
                    padding: const EdgeInsets.symmetric(horizontal: 8),
                    textStyle: const TextStyle(
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ),
              ),
              const SizedBox(width: 6),
            ],

            // Primary CTA: run (or re-run) the optimization
            Expanded(
              child: ElevatedButton.icon(
                onPressed: isLoading ? null : onRunOptimization,
                icon: isLoading
                    ? const SizedBox(
                        width: 14,
                        height: 14,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: Colors.white,
                        ),
                      )
                    : const Icon(
                        Icons.route_rounded,
                        size: 16,
                        color: Colors.white,
                      ),
                label: Text(
                  hasExistingSolution ? 'Run Again' : 'Run Optimization',
                  style: const TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w700,
                    color: Colors.white,
                  ),
                ),
                style: ElevatedButton.styleFrom(
                  backgroundColor: context.primary,
                  disabledBackgroundColor: context.primary.withOpacity(0.5),
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(30),
                  ),
                  minimumSize: const Size(0, 40),
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                  elevation: 3,
                  shadowColor: context.primary.withOpacity(0.4),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
