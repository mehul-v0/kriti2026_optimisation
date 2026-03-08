import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

// Theme helpers
bool _isDark(BuildContext ctx) => Theme.of(ctx).brightness == Brightness.dark;
Color _textSecondary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white54 : AppColors.textSecondaryLight;
Color _textTertiary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white38 : Colors.black38;
Color _borderColor(BuildContext ctx) =>
    _isDark(ctx) ? AppColors.darkBorderColor : AppColors.borderColor;
Color _priorityColor(BuildContext ctx, dynamic p) =>
    _isDark(ctx) ? AppColors.silver : const Color(0xFFCCCCCC);

String _priorityLabel(dynamic p) {
  final v = int.tryParse(p?.toString() ?? '') ?? 0;
  switch (v) {
    case 1:
      return 'CRITICAL';
    case 2:
      return 'HIGH';
    case 3:
      return 'MEDIUM';
    case 4:
      return 'LOW';
    case 5:
      return 'FLEX';
    default:
      return 'P$v';
  }
}

/// Collapsible filter row for the employees list.
class EmployeeFilterBar extends StatelessWidget {
  final Set<int> availablePriorities;
  final int? filterPriority;
  final bool? sortByCost;
  final bool isExpanded;
  final bool hasActiveFilters;
  final VoidCallback onToggleExpand;
  final ValueChanged<int?> onPriorityChanged;
  final ValueChanged<bool?> onSortCostChanged;
  final VoidCallback onClear;

  const EmployeeFilterBar({
    super.key,
    required this.availablePriorities,
    required this.filterPriority,
    required this.sortByCost,
    required this.isExpanded,
    required this.hasActiveFilters,
    required this.onToggleExpand,
    required this.onPriorityChanged,
    required this.onSortCostChanged,
    required this.onClear,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _FilterHeader(
            hasActiveFilters: hasActiveFilters,
            isExpanded: isExpanded,
            onToggle: onToggleExpand,
            onClear: onClear,
          ),
          if (isExpanded) ...[
            const SizedBox(height: 4),
            Wrap(
              spacing: 8,
              runSpacing: 6,
              children: [
                // Only show priorities that actually appear in the data
                for (int p in [1, 2, 3, 4, 5])
                  if (availablePriorities.contains(p))
                    _FilterChip(
                      label: _priorityLabel(p),
                      isActive: filterPriority == p,
                      activeColor: _priorityColor(context, p),
                      onTap: () =>
                          onPriorityChanged(filterPriority == p ? null : p),
                    ),
                _SortChip(
                  label: 'Cost',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Low → High',
                  isActive: sortByCost == true,
                  activeColor: context.primary,
                  onTap: () =>
                      onSortCostChanged(sortByCost == true ? null : true),
                ),
                _SortChip(
                  label: 'Cost',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'High → Low',
                  isActive: sortByCost == false,
                  activeColor: context.primary,
                  onTap: () =>
                      onSortCostChanged(sortByCost == false ? null : false),
                ),
              ],
            ),
            const SizedBox(height: 4),
          ],
        ],
      ),
    );
  }
}

/// Collapsible filter row for the vehicles list.
class VehicleFilterBar extends StatelessWidget {
  final List<int> seatBreakpoints;
  final int? filterMinSeats;
  final bool? sortByCostPerKm;
  final bool? sortBySpeed;
  final bool? sortByTime;
  final bool isExpanded;
  final bool hasActiveFilters;
  final VoidCallback onToggleExpand;
  final ValueChanged<int?> onMinSeatsChanged;
  final ValueChanged<bool?> onSortCostPerKmChanged;
  final ValueChanged<bool?> onSortSpeedChanged;
  final ValueChanged<bool?> onSortTimeChanged;
  final VoidCallback onClear;

  const VehicleFilterBar({
    super.key,
    required this.seatBreakpoints,
    required this.filterMinSeats,
    required this.sortByCostPerKm,
    required this.sortBySpeed,
    required this.sortByTime,
    required this.isExpanded,
    required this.hasActiveFilters,
    required this.onToggleExpand,
    required this.onMinSeatsChanged,
    required this.onSortCostPerKmChanged,
    required this.onSortSpeedChanged,
    required this.onSortTimeChanged,
    required this.onClear,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _FilterHeader(
            hasActiveFilters: hasActiveFilters,
            isExpanded: isExpanded,
            onToggle: onToggleExpand,
            onClear: onClear,
          ),
          if (isExpanded) ...[
            const SizedBox(height: 4),
            Wrap(
              spacing: 8,
              runSpacing: 6,
              children: [
                for (final s in seatBreakpoints)
                  _FilterChip(
                    label: '≥ $s Seats',
                    isActive: filterMinSeats == s,
                    activeColor: context.primary,
                    onTap: () =>
                        onMinSeatsChanged(filterMinSeats == s ? null : s),
                  ),
                _SortChip(
                  label: '₹/km',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Low → High',
                  isActive: sortByCostPerKm == true,
                  activeColor: context.primary,
                  onTap: () => onSortCostPerKmChanged(
                    sortByCostPerKm == true ? null : true,
                  ),
                ),
                _SortChip(
                  label: '₹/km',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'High → Low',
                  isActive: sortByCostPerKm == false,
                  activeColor: context.primary,
                  onTap: () => onSortCostPerKmChanged(
                    sortByCostPerKm == false ? null : false,
                  ),
                ),
                _SortChip(
                  label: 'Speed',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Slow → Fast',
                  isActive: sortBySpeed == true,
                  activeColor: context.primary,
                  onTap: () =>
                      onSortSpeedChanged(sortBySpeed == true ? null : true),
                ),
                _SortChip(
                  label: 'Speed',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'Fast → Slow',
                  isActive: sortBySpeed == false,
                  activeColor: context.primary,
                  onTap: () =>
                      onSortSpeedChanged(sortBySpeed == false ? null : false),
                ),
                _SortChip(
                  label: 'Time',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Earliest first',
                  isActive: sortByTime == true,
                  activeColor: context.primary,
                  onTap: () =>
                      onSortTimeChanged(sortByTime == true ? null : true),
                ),
                _SortChip(
                  label: 'Time',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'Latest first',
                  isActive: sortByTime == false,
                  activeColor: context.primary,
                  onTap: () =>
                      onSortTimeChanged(sortByTime == false ? null : false),
                ),
              ],
            ),
            const SizedBox(height: 4),
          ],
        ],
      ),
    );
  }
}

// Toggle row shared by both filter bars.
class _FilterHeader extends StatelessWidget {
  final bool hasActiveFilters;
  final bool isExpanded;
  final VoidCallback onToggle;
  final VoidCallback onClear;

  const _FilterHeader({
    required this.hasActiveFilters,
    required this.isExpanded,
    required this.onToggle,
    required this.onClear,
  });

  @override
  Widget build(BuildContext context) {
    return InkWell(
      borderRadius: BorderRadius.circular(8),
      onTap: onToggle,
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 6),
        child: Row(
          children: [
            Icon(
              Icons.filter_list_rounded,
              size: 16,
              color: hasActiveFilters
                  ? context.primary
                  : _textSecondary(context),
            ),
            const SizedBox(width: 6),
            Text(
              'Filters',
              style: TextStyle(
                fontSize: 12,
                fontWeight: FontWeight.w600,
                color: hasActiveFilters
                    ? context.primary
                    : _textSecondary(context),
              ),
            ),
            // Dot indicator when a filter is active
            if (hasActiveFilters) ...[
              const SizedBox(width: 6),
              Container(
                width: 6,
                height: 6,
                decoration: BoxDecoration(
                  color: context.primary,
                  shape: BoxShape.circle,
                ),
              ),
            ],
            const Spacer(),
            if (hasActiveFilters)
              GestureDetector(
                onTap: onClear,
                child: Text(
                  'Clear',
                  style: TextStyle(
                    fontSize: 11,
                    color: _textTertiary(context),
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ),
            const SizedBox(width: 8),
            Icon(
              isExpanded
                  ? Icons.expand_less_rounded
                  : Icons.expand_more_rounded,
              size: 18,
              color: _textTertiary(context),
            ),
          ],
        ),
      ),
    );
  }
}

// Pill chip for filter values (e.g. priority level, min seats).
class _FilterChip extends StatelessWidget {
  final String label;
  final bool isActive;
  final Color activeColor;
  final VoidCallback onTap;

  const _FilterChip({
    required this.label,
    required this.isActive,
    required this.activeColor,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
        decoration: BoxDecoration(
          color: isActive ? activeColor.withOpacity(0.15) : Colors.transparent,
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: isActive
                ? activeColor.withOpacity(0.5)
                : _borderColor(context),
          ),
        ),
        child: Text(
          label,
          style: TextStyle(
            color: isActive ? activeColor : _textSecondary(context),
            fontSize: 11,
            fontWeight: isActive ? FontWeight.w700 : FontWeight.w500,
          ),
        ),
      ),
    );
  }
}

// Arrow chip for ascending / descending sort options.
class _SortChip extends StatelessWidget {
  final String label;
  final IconData icon;
  final String tooltip;
  final bool isActive;
  final Color activeColor;
  final VoidCallback onTap;

  const _SortChip({
    required this.label,
    required this.icon,
    required this.tooltip,
    required this.isActive,
    required this.activeColor,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tooltip,
      child: GestureDetector(
        onTap: onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 200),
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 5),
          decoration: BoxDecoration(
            color: isActive
                ? activeColor.withOpacity(0.15)
                : Colors.transparent,
            borderRadius: BorderRadius.circular(20),
            border: Border.all(
              color: isActive
                  ? activeColor.withOpacity(0.5)
                  : _borderColor(context),
            ),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                icon,
                size: 12,
                color: isActive ? activeColor : _textSecondary(context),
              ),
              const SizedBox(width: 3),
              Text(
                label,
                style: TextStyle(
                  color: isActive ? activeColor : _textSecondary(context),
                  fontSize: 11,
                  fontWeight: isActive ? FontWeight.w700 : FontWeight.w500,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
