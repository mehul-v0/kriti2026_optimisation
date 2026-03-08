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

    return Card(
      elevation: isSelected ? 2 : 1,
      margin: EdgeInsets.zero,
      color: theme.cardColor,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: isSelected
            ? BorderSide(color: context.primary, width: 2)
            : BorderSide(
                color: theme.colorScheme.outline.withOpacity(0.12),
                width: 1,
              ),
      ),
      child: InkWell(
        onTap: onTap,
        onLongPress: onLongPress,
        borderRadius: BorderRadius.circular(12),
        splashColor: context.primary.withOpacity(0.08),
        highlightColor: context.primary.withOpacity(0.06),
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
                      ? context.primary
                      : context.primary.withOpacity(0.1),
                  shape: BoxShape.circle,
                ),
                child: isSelectionMode
                    ? Icon(
                        isSelected ? Icons.check : Icons.circle_outlined,
                        color: isSelected ? Colors.white : Colors.grey,
                      )
                    : Icon(Icons.folder_outlined, color: context.primary),
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
                        color: isPinned ? context.primary : null,
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
              if (!isSelectionMode)
                PopupMenuButton<_CardAction>(
                  icon: Icon(
                    Icons.more_vert,
                    size: 20,
                    color: theme.colorScheme.onSurface.withOpacity(0.45),
                  ),
                  padding: EdgeInsets.zero,
                  splashRadius: 20,
                  color: theme.brightness == Brightness.dark
                      ? const Color(0xFF2C2C2C)
                      : Colors.white,
                  elevation: 8,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(10),
                    side: BorderSide(
                      color: theme.colorScheme.outline.withOpacity(0.15),
                      width: 1,
                    ),
                  ),
                  onSelected: (action) {
                    switch (action) {
                      case _CardAction.rename:
                        onRename();
                      case _CardAction.pin:
                        onPinToggle();
                      case _CardAction.delete:
                        onDelete();
                    }
                  },
                  itemBuilder: (_) => [
                    PopupMenuItem(
                      value: _CardAction.rename,
                      child: Row(
                        children: [
                          Icon(
                            Icons.edit_outlined,
                            size: 18,
                            color: theme.colorScheme.onSurface.withOpacity(0.7),
                          ),
                          const SizedBox(width: 10),
                          const Text("Rename"),
                        ],
                      ),
                    ),
                    PopupMenuItem(
                      value: _CardAction.pin,
                      child: Row(
                        children: [
                          Icon(
                            isPinned ? Icons.push_pin : Icons.push_pin_outlined,
                            size: 18,
                            color: isPinned
                                ? AppColors.warning
                                : theme.colorScheme.onSurface.withOpacity(0.7),
                          ),
                          const SizedBox(width: 10),
                          Text(isPinned ? "Unpin" : "Pin to top"),
                        ],
                      ),
                    ),
                    PopupMenuItem(
                      value: _CardAction.delete,
                      child: Row(
                        children: [
                          Icon(
                            Icons.delete_outline,
                            size: 18,
                            color: AppColors.error,
                          ),
                          const SizedBox(width: 10),
                          Text(
                            "Delete",
                            style: TextStyle(color: AppColors.error),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
            ],
          ),
        ),
      ),
    );
  }
}

enum _CardAction { rename, pin, delete }
