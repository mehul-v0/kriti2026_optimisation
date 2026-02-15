import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class DownloadDialog extends StatelessWidget {
  final Function(String type) onSelect;

  const DownloadDialog({super.key, required this.onSelect});

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: Container(
        padding: const EdgeInsets.fromLTRB(
          24,
          24,
          24,
          40,
        ), // Extra bottom padding (40)
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Handle Bar for visual cue
            Center(
              child: Container(
                width: 40,
                height: 4,
                margin: const EdgeInsets.only(bottom: 20),
                decoration: BoxDecoration(
                  color: Colors.grey[300],
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
            ),
            Text(
              "Download Report",
              style: Theme.of(context).textTheme.headlineSmall,
            ),
            const SizedBox(height: 8),
            Text(
              "Select a format to save to your device storage.",
              style: Theme.of(context).textTheme.bodyMedium,
            ),
            const SizedBox(height: 24),
            _buildOption(context, "JSON", Icons.code, "json"),
            const SizedBox(height: 12),
            _buildOption(context, "Excel (.xlsx)", Icons.table_chart, "excel"),
            const SizedBox(height: 12),
            _buildOption(context, "PDF Report", Icons.picture_as_pdf, "pdf"),
          ],
        ),
      ),
    );
  }

  Widget _buildOption(
    BuildContext context,
    String title,
    IconData icon,
    String type,
  ) {
    return InkWell(
      onTap: () {
        Navigator.pop(context); // Close dialog
        onSelect(type); // Trigger download
      },
      borderRadius: BorderRadius.circular(12),
      child: Container(
        padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 20),
        decoration: BoxDecoration(
          border: Border.all(color: AppColors.borderColor),
          borderRadius: BorderRadius.circular(12),
          color: Theme.of(context).cardColor,
        ),
        child: Row(
          children: [
            Icon(icon, color: AppColors.primaryBrand),
            const SizedBox(width: 16),
            Text(
              title,
              style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
            ),
            const Spacer(),
            const Icon(Icons.arrow_forward_ios, size: 16, color: Colors.grey),
          ],
        ),
      ),
    );
  }
}
