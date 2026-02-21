import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

enum SortOption { dateNewest, dateOldest, nameAsc, nameDesc }

/// Bottom sheet widget for filtering and sorting test cases
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
    return Container(
      decoration: BoxDecoration(
        color: Theme.of(context).scaffoldBackgroundColor,
        borderRadius: const BorderRadius.vertical(top: Radius.circular(20)),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          // Drag Handle
          Container(
            margin: const EdgeInsets.only(top: 12, bottom: 8),
            width: 40,
            height: 4,
            decoration: BoxDecoration(
              color: Colors.grey[300],
              borderRadius: BorderRadius.circular(2),
            ),
          ),

          // Title
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
            child: Row(
              children: [
                const Icon(Icons.sort, color: AppColors.primaryBrand),
                const SizedBox(width: 12),
                Text(
                  "Sort By",
                  style: Theme.of(
                    context,
                  ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.bold),
                ),
              ],
            ),
          ),

          const Divider(height: 1),

          // Sort Options
          _buildSortOption(
            context,
            icon: Icons.calendar_month,
            title: "Date: Newest First",
            sortOption: SortOption.dateNewest,
          ),
          _buildSortOption(
            context,
            icon: Icons.calendar_month_outlined,
            title: "Date: Oldest First",
            sortOption: SortOption.dateOldest,
          ),
          _buildSortOption(
            context,
            icon: Icons.sort_by_alpha,
            title: "Name: A-Z",
            sortOption: SortOption.nameAsc,
          ),
          _buildSortOption(
            context,
            icon: Icons.sort_by_alpha,
            title: "Name: Z-A",
            sortOption: SortOption.nameDesc,
          ),

          SizedBox(height: MediaQuery.of(context).padding.bottom + 16),
        ],
      ),
    );
  }

  Widget _buildSortOption(
    BuildContext context, {
    required IconData icon,
    required String title,
    required SortOption sortOption,
  }) {
    final isSelected = currentSort == sortOption;

    return ListTile(
      leading: Icon(
        icon,
        color: isSelected ? AppColors.primaryBrand : Colors.grey[600],
      ),
      title: Text(
        title,
        style: TextStyle(
          fontWeight: isSelected ? FontWeight.bold : FontWeight.normal,
          color: isSelected ? AppColors.primaryBrand : null,
        ),
      ),
      trailing: isSelected
          ? const Icon(Icons.check_circle, color: AppColors.primaryBrand)
          : null,
      onTap: () {
        onSortChanged(sortOption);
        Navigator.pop(context);
      },
    );
  }

  /// Static method to show the bottom sheet
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
