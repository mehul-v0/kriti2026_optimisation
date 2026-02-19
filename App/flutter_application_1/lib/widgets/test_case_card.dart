import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class TestCaseCard extends StatelessWidget {
  final Map<String, dynamic> data;
  final bool isSelected;
  final bool isSelectionMode;
  final bool isPinned;
  final VoidCallback onTap;
  final VoidCallback onLongPress;
  final VoidCallback onPinToggle;
  final VoidCallback onDelete;
  final VoidCallback onRename;

  const TestCaseCard({
    super.key,
    required this.data,
    required this.isSelected,
    required this.isSelectionMode,
    required this.isPinned,
    required this.onTap,
    required this.onLongPress,
    required this.onPinToggle,
    required this.onDelete,
    required this.onRename,
  });

  String _formatDate(String? rawDate) {
    if (rawDate == null) return "Unknown Date";
    try {
      final dt = DateTime.parse(rawDate);
      const months = [
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec",
      ];
      return "${months[dt.month - 1]} ${dt.day}, ${dt.year}";
    } catch (_) {
      return rawDate.split('T')[0];
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;

    return Card(
      elevation: isSelected ? 4 : 1,
      margin: EdgeInsets.zero,
      color: isSelected
          ? (isDark ? const Color(0x3300C569) : const Color(0x1A00C569))
          : theme.cardColor,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: isSelected
            ? const BorderSide(color: AppColors.primaryBrand, width: 2)
            : BorderSide.none,
      ),
      child: InkWell(
        onTap: onTap,
        onLongPress: onLongPress,
        borderRadius: BorderRadius.circular(12),
        child: Padding(
          padding: const EdgeInsets.all(12.0),
          child: Row(
            children: [
              // Icon Section
              Container(
                width: 48,
                height: 48,
                decoration: BoxDecoration(
                  color: isSelectionMode && isSelected
                      ? AppColors.primaryBrand
                      : const Color(0x1A00C569),
                  shape: BoxShape.circle,
                ),
                child: isSelectionMode
                    ? Icon(
                        isSelected ? Icons.check : Icons.circle_outlined,
                        color: isSelected ? Colors.white : Colors.grey,
                      )
                    : const Icon(
                        Icons.folder_outlined,
                        color: AppColors.primaryBrand,
                      ),
              ),
              const SizedBox(width: 16),

              // Text Content
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Text(
                      data['case_name'] ?? "Untitled",
                      style: theme.textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.bold,
                        color: isPinned ? AppColors.primaryBrand : null,
                      ),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    const SizedBox(height: 4),
                    Text(
                      _formatDate(data['created_at']),
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: Colors.grey,
                      ),
                    ),
                  ],
                ),
              ),

              // Action Buttons
              if (!isSelectionMode) ...[
                IconButton(
                  icon: Icon(
                    Icons.edit_outlined,
                    color: Colors.grey[400],
                    size: 20,
                  ),
                  onPressed: onRename,
                  tooltip: "Rename",
                ),
                IconButton(
                  icon: Icon(
                    isPinned ? Icons.push_pin : Icons.push_pin_outlined,
                    color: isPinned ? AppColors.warning : Colors.grey[400],
                    size: 20,
                  ),
                  onPressed: onPinToggle,
                  tooltip: isPinned ? "Unpin" : "Pin to top",
                ),
                IconButton(
                  icon: const Icon(Icons.delete_outline, size: 20),
                  color: AppColors.error,
                  onPressed: onDelete,
                  tooltip: "Delete",
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}
