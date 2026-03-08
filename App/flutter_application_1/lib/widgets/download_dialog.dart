import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class DownloadDialog extends StatelessWidget {
  final Function(String type) onSelect;

  const DownloadDialog({super.key, required this.onSelect});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;
    final sheetColor = isDark ? const Color(0xFF1E1E1E) : Colors.white;
    final bottomPadding = MediaQuery.paddingOf(context).bottom;

    return Container(
      decoration: BoxDecoration(
        color: sheetColor,
        borderRadius: const BorderRadius.vertical(top: Radius.circular(16)),
        border: Border(
          top: BorderSide(
            color: AppColors.primaryBrand.withOpacity(0.35),
            width: 1.5,
          ),
        ),
      ),
      padding: EdgeInsets.fromLTRB(16, 0, 16, bottomPadding + 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          // Drag handle
          Container(
            margin: const EdgeInsets.only(top: 10, bottom: 14),
            width: 32,
            height: 3,
            decoration: BoxDecoration(
              color: theme.colorScheme.outline.withOpacity(0.35),
              borderRadius: BorderRadius.circular(2),
            ),
          ),

          // Header row
          Row(
            children: [
              const Icon(
                Icons.download_rounded,
                size: 16,
                color: AppColors.primaryBrand,
              ),
              const SizedBox(width: 8),
              Text(
                'EXPORT AS',
                style: theme.textTheme.labelSmall?.copyWith(
                  color: AppColors.primaryBrand,
                  fontWeight: FontWeight.w700,
                  letterSpacing: 1.4,
                  fontSize: 11,
                ),
              ),
            ],
          ),

          const SizedBox(height: 10),

          // Options — 3 equal chips in a single column
          _buildChip(context, Icons.code_rounded, 'JSON', 'json', isDark),
          const SizedBox(height: 8),
          _buildChip(
            context,
            Icons.table_chart_rounded,
            'Excel (.xlsx)',
            'excel',
            isDark,
          ),
          const SizedBox(height: 8),
          _buildChip(
            context,
            Icons.picture_as_pdf_rounded,
            'PDF Report',
            'pdf',
            isDark,
          ),
        ],
      ),
    );
  }

  Widget _buildChip(
    BuildContext context,
    IconData icon,
    String label,
    String type,
    bool isDark,
  ) {
    final theme = Theme.of(context);
    final bg = isDark ? const Color(0xFF2A2A2A) : const Color(0xFFF3F3F3);

    return GestureDetector(
      onTap: () {
        Navigator.pop(context);
        onSelect(type);
      },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 120),
        width: double.infinity,
        decoration: BoxDecoration(
          color: bg,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: theme.colorScheme.outline.withOpacity(0.18),
          ),
        ),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
        child: Row(
          children: [
            Icon(icon, size: 16, color: AppColors.primaryBrand),
            const SizedBox(width: 10),
            Text(
              label,
              style: TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w600,
                color: theme.colorScheme.onSurface.withOpacity(0.85),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
