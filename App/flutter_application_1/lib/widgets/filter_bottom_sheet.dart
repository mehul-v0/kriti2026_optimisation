import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

enum SortOption { dateNewest, dateOldest, nameAsc, nameDesc }

class FilterBottomSheet extends StatelessWidget {
  final SortOption currentSort;
  final Function(SortOption) onSortChanged;

  const FilterBottomSheet({
    Key? key,
    required this.currentSort,
    required this.onSortChanged,
  }) : super(key: key);

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
      padding: EdgeInsets.fromLTRB(16, 0, 16, bottomPadding + 12),
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
                Icons.sort_rounded,
                size: 16,
                color: AppColors.primaryBrand,
              ),
              const SizedBox(width: 8),
              Text(
                "SORT BY",
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

          // Options grid — 2x2
          GridView.count(
            crossAxisCount: 2,
            shrinkWrap: true,
            physics: const NeverScrollableScrollPhysics(),
            mainAxisSpacing: 8,
            crossAxisSpacing: 8,
            childAspectRatio: 3.2,
            children: [
              _buildChip(
                context,
                Icons.arrow_downward_rounded,
                "Newest First",
                SortOption.dateNewest,
              ),
              _buildChip(
                context,
                Icons.arrow_upward_rounded,
                "Oldest First",
                SortOption.dateOldest,
              ),
              _buildChip(
                context,
                Icons.sort_by_alpha_rounded,
                "Name A → Z",
                SortOption.nameAsc,
              ),
              _buildChip(
                context,
                Icons.sort_by_alpha_rounded,
                "Name Z → A",
                SortOption.nameDesc,
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildChip(
    BuildContext context,
    IconData icon,
    String label,
    SortOption option,
  ) {
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;
    final isSelected = currentSort == option;

    final selectedBg = AppColors.primaryBrand.withOpacity(isDark ? 0.18 : 0.10);
    final unselectedBg = isDark
        ? const Color(0xFF2A2A2A)
        : const Color(0xFFF3F3F3);

    return GestureDetector(
      onTap: () {
        onSortChanged(option);
        Navigator.pop(context);
      },
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 150),
        decoration: BoxDecoration(
          color: isSelected ? selectedBg : unselectedBg,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(
            color: isSelected
                ? AppColors.primaryBrand
                : theme.colorScheme.outline.withOpacity(0.18),
            width: isSelected ? 1.5 : 1,
          ),
        ),
        padding: const EdgeInsets.symmetric(horizontal: 10),
        child: Row(
          children: [
            Icon(
              icon,
              size: 14,
              color: isSelected
                  ? AppColors.primaryBrand
                  : theme.colorScheme.onSurface.withOpacity(0.5),
            ),
            const SizedBox(width: 6),
            Expanded(
              child: Text(
                label,
                style: TextStyle(
                  fontSize: 12,
                  fontWeight: isSelected ? FontWeight.w600 : FontWeight.normal,
                  color: isSelected
                      ? AppColors.primaryBrand
                      : theme.colorScheme.onSurface.withOpacity(0.75),
                ),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ),
            if (isSelected)
              const Icon(
                Icons.check_rounded,
                size: 13,
                color: AppColors.primaryBrand,
              ),
          ],
        ),
      ),
    );
  }

  static void show(
    BuildContext context, {
    required SortOption currentSort,
    required Function(SortOption) onSortChanged,
  }) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (ctx) => FilterBottomSheet(
        currentSort: currentSort,
        onSortChanged: onSortChanged,
      ),
    );
  }
}
