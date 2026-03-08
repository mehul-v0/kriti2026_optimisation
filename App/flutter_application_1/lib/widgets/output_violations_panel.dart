import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class OutputViolationsPanel extends StatelessWidget {
  final Map<String, dynamic> data;

  const OutputViolationsPanel({super.key, required this.data});

  static int _toMin(String t) {
    final p = t.split(':');
    if (p.length != 2) return 0;
    return (int.tryParse(p[0]) ?? 0) * 60 + (int.tryParse(p[1]) ?? 0);
  }

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final surface = dark ? AppColors.darkSurface : AppColors.lightSurface;
    final textPrimary = dark ? Colors.white : AppColors.textPrimaryLight;
    final textSecondary = dark ? Colors.white54 : AppColors.textSecondaryLight;
    final border = dark ? AppColors.darkBorderColor : AppColors.borderColor;

    final result = (data['result'] as Map<String, dynamic>?) ?? {};
    final stats = (data['stats'] as Map<String, dynamic>?) ?? {};
    final violationDetails =
        (data['violation_details'] as Map<String, dynamic>?) ?? {};
    final routes = (data['routes'] as List?) ?? [];

    final hardViol =
        (result['hard_violations'] as num?)?.toInt() ??
        (stats['hard_violations'] as num?)?.toInt() ??
        0;
    final softViol =
        (result['soft_violations'] as num?)?.toInt() ??
        (stats['soft_violations'] as num?)?.toInt() ??
        0;

    // 5 violation categories
    final capViolations =
        (violationDetails['capacity_violations'] as List?) ?? [];
    final unassignedEmps =
        (violationDetails['unassigned_employees'] as List?) ?? [];
    final timeWindowViol =
        (violationDetails['time_window_violations'] as List?) ?? [];
    final sharingPrefViol =
        (violationDetails['sharing_pref_violations'] as List?) ?? [];
    final vehiclePrefViol =
        (violationDetails['vehicle_pref_violations'] as List?) ?? [];

    // Compute employee dwell (wait) times from route_points
    final List<_EmployeeWait> waitTimes = [];
    for (final r in routes) {
      final route = r as Map<String, dynamic>;
      final vehicleId = route['vehicle_id']?.toString() ?? '';
      for (final pt in (route['route_points'] as List?) ?? []) {
        final p = pt as Map<String, dynamic>;
        if (p['type'] != 'pickup') continue;
        final empId = p['employee_id']?.toString() ?? '';
        if (empId.isEmpty) continue;
        final arr = _toMin(p['arrival_time']?.toString() ?? '');
        final dep = _toMin(p['departure_time']?.toString() ?? '');
        final dwell = dep - arr;
        if (dwell > 0) {
          waitTimes.add(
            _EmployeeWait(
              employeeId: empId,
              vehicleId: vehicleId,
              arrivalTime: p['arrival_time']?.toString() ?? '',
              departureTime: p['departure_time']?.toString() ?? '',
              dwellMins: dwell,
            ),
          );
        }
      }
    }
    waitTimes.sort((a, b) => b.dwellMins.compareTo(a.dwellMins));

    final bottomPad = MediaQuery.of(context).padding.bottom;
    return SingleChildScrollView(
      padding: EdgeInsets.fromLTRB(16, 14, 16, 28 + bottomPad),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // ── Feasibility status header ──
          _FeasibilityHeader(
            hardViol: hardViol,
            softViol: softViol,
            surface: surface,
            border: border,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
          const SizedBox(height: 16),

          // ── Constraint Categories ──
          _SectionLabel(
            icon: Icons.checklist_rounded,
            label: 'Constraint Categories',
            textPrimary: textPrimary,
          ),
          const SizedBox(height: 8),
          _ConstraintTable(
            categories: [
              _ConstraintRow(
                label: 'Capacity Violations',
                count: capViolations.length,
                description: 'vehicle overloaded beyond capacity',
                isHard: true,
              ),
              _ConstraintRow(
                label: 'Unassigned Employees',
                count: unassignedEmps.length,
                description: 'employees not assigned to any vehicle',
                isHard: true,
              ),
              _ConstraintRow(
                label: 'Time Window Violations',
                count: timeWindowViol.length,
                description: 'arrival outside allowed window',
                isHard: true,
              ),
              _ConstraintRow(
                label: 'Sharing Preference',
                count: sharingPrefViol.length,
                description: 'employee carpooling preferences not met',
                isHard: false,
              ),
              _ConstraintRow(
                label: 'Vehicle Preference',
                count: vehiclePrefViol.length,
                description: 'employee vehicle preferences not met',
                isHard: false,
              ),
            ],
            surface: surface,
            border: border,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),

          // ── Violation detail lists (only shown if violations exist) ──
          if (unassignedEmps.isNotEmpty) ...[
            const SizedBox(height: 16),
            _ViolationDetailList(
              title: 'Unassigned Employees',
              color: AppColors.error,
              items: unassignedEmps.map((e) => e.toString()).toList(),
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
          ],
          if (capViolations.isNotEmpty) ...[
            const SizedBox(height: 16),
            _ViolationDetailList(
              title: 'Capacity Violations',
              color: AppColors.error,
              items: capViolations.map((e) => e.toString()).toList(),
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
          ],
          if (timeWindowViol.isNotEmpty) ...[
            const SizedBox(height: 16),
            _ViolationDetailList(
              title: 'Time Window Violations',
              color: AppColors.error,
              items: timeWindowViol.map((e) => e.toString()).toList(),
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
          ],

          // ── Employee dwell (wait) times ──
          if (waitTimes.isNotEmpty) ...[
            const SizedBox(height: 20),
            _SectionLabel(
              icon: Icons.hourglass_top_rounded,
              label: 'Employee Wait Times at Pickup',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 4),
            Text(
              'Time between vehicle arrival and scheduled pickup departure',
              style: TextStyle(color: textSecondary, fontSize: 11),
            ),
            const SizedBox(height: 10),
            _WaitTimeChart(
              waitTimes: waitTimes,
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
          ],
        ],
      ),
    );
  }
}

// ─── Feasibility header ─────────────────────────────────────────────────────

class _FeasibilityHeader extends StatelessWidget {
  final int hardViol, softViol;
  final Color surface, border, textPrimary, textSecondary;

  const _FeasibilityHeader({
    required this.hardViol,
    required this.softViol,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final isOptimal = hardViol == 0 && softViol == 0;

    final statusColor = hardViol > 0
        ? AppColors.error
        : softViol > 0
        ? AppColors.warning
        : context.primary;

    final statusLabel = hardViol > 0
        ? 'Infeasible'
        : softViol > 0
        ? 'Feasible'
        : 'Optimal';

    final statusDesc = hardViol > 0
        ? 'Hard constraints violated — solution may not be operationally valid'
        : softViol > 0
        ? 'All hard constraints met, but some preferences were not honored'
        : 'All hard and soft constraints satisfied';

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: statusColor.withOpacity(0.07),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: statusColor.withOpacity(0.3)),
      ),
      child: Column(
        children: [
          Row(
            children: [
              Container(
                width: 44,
                height: 44,
                decoration: BoxDecoration(
                  color: statusColor.withOpacity(0.15),
                  shape: BoxShape.circle,
                ),
                child: Icon(
                  isOptimal
                      ? Icons.verified_rounded
                      : hardViol > 0
                      ? Icons.error_outline_rounded
                      : Icons.warning_amber_rounded,
                  color: statusColor,
                  size: 24,
                ),
              ),
              const SizedBox(width: 14),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      '$statusLabel Solution',
                      style: TextStyle(
                        color: statusColor,
                        fontSize: 16,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                    const SizedBox(height: 2),
                    Text(
                      statusDesc,
                      style: TextStyle(color: textSecondary, fontSize: 11),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 14),
          // Counts row
          Row(
            children: [
              Expanded(
                child: _CountBadge(
                  count: hardViol,
                  label: 'Hard Violations',
                  color: hardViol > 0 ? AppColors.error : context.primary,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _CountBadge(
                  count: softViol,
                  label: 'Soft Violations',
                  color: softViol > 0 ? AppColors.warning : context.primary,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _CountBadge extends StatelessWidget {
  final int count;
  final String label;
  final Color color;

  const _CountBadge({
    required this.count,
    required this.label,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 10, horizontal: 14),
      decoration: BoxDecoration(
        color: color.withOpacity(0.1),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withOpacity(0.3)),
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Text(
            '$count',
            style: TextStyle(
              color: color,
              fontSize: 20,
              fontWeight: FontWeight.w800,
            ),
          ),
          const SizedBox(width: 6),
          Text(
            label,
            style: TextStyle(
              color: color.withOpacity(0.8),
              fontSize: 11,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }
}

// ─── Constraint table ────────────────────────────────────────────────────────

class _ConstraintRow {
  final String label, description;
  final int count;
  final bool isHard;

  const _ConstraintRow({
    required this.label,
    required this.description,
    required this.count,
    required this.isHard,
  });
}

class _ConstraintTable extends StatelessWidget {
  final List<_ConstraintRow> categories;
  final Color surface, border, textPrimary, textSecondary;

  const _ConstraintTable({
    required this.categories,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Column(
        children: List.generate(categories.length, (i) {
          final cat = categories[i];
          final isLast = i == categories.length - 1;
          final hasViolations = cat.count > 0;
          final color = hasViolations
              ? (cat.isHard ? AppColors.error : AppColors.warning)
              : context.primary;

          return Column(
            children: [
              Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: 14,
                  vertical: 12,
                ),
                child: Row(
                  children: [
                    // Status icon
                    Container(
                      width: 28,
                      height: 28,
                      decoration: BoxDecoration(
                        color: color.withOpacity(0.1),
                        shape: BoxShape.circle,
                      ),
                      child: Icon(
                        hasViolations
                            ? (cat.isHard
                                  ? Icons.error_rounded
                                  : Icons.warning_rounded)
                            : Icons.check_circle_rounded,
                        color: color,
                        size: 16,
                      ),
                    ),
                    const SizedBox(width: 12),
                    // Label + description
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            children: [
                              Text(
                                cat.label,
                                style: TextStyle(
                                  color: textPrimary,
                                  fontSize: 13,
                                  fontWeight: FontWeight.w600,
                                ),
                              ),
                              const SizedBox(width: 6),
                              // Hard / Soft badge
                              Container(
                                padding: const EdgeInsets.symmetric(
                                  horizontal: 5,
                                  vertical: 1,
                                ),
                                decoration: BoxDecoration(
                                  color:
                                      (cat.isHard
                                              ? AppColors.error
                                              : AppColors.warning)
                                          .withOpacity(0.1),
                                  borderRadius: BorderRadius.circular(4),
                                ),
                                child: Text(
                                  cat.isHard ? 'HARD' : 'SOFT',
                                  style: TextStyle(
                                    color: cat.isHard
                                        ? AppColors.error
                                        : AppColors.warning,
                                    fontSize: 8,
                                    fontWeight: FontWeight.w800,
                                    letterSpacing: 0.5,
                                  ),
                                ),
                              ),
                            ],
                          ),
                          Text(
                            cat.description,
                            style: TextStyle(
                              color: textSecondary,
                              fontSize: 10,
                            ),
                          ),
                        ],
                      ),
                    ),
                    // Count
                    Container(
                      padding: const EdgeInsets.symmetric(
                        horizontal: 8,
                        vertical: 4,
                      ),
                      decoration: BoxDecoration(
                        color: color.withOpacity(0.1),
                        borderRadius: BorderRadius.circular(20),
                      ),
                      child: Text(
                        '${cat.count}',
                        style: TextStyle(
                          color: color,
                          fontSize: 12,
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              if (!isLast) Divider(height: 1, color: border.withOpacity(0.25)),
            ],
          );
        }),
      ),
    );
  }
}

// ─── Violation detail list ──────────────────────────────────────────────────

class _ViolationDetailList extends StatelessWidget {
  final String title;
  final Color color;
  final List<String> items;
  final Color surface, border, textPrimary, textSecondary;

  const _ViolationDetailList({
    required this.title,
    required this.color,
    required this.items,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionLabel(
          icon: Icons.list_alt_rounded,
          label: title,
          textPrimary: textPrimary,
        ),
        const SizedBox(height: 8),
        Container(
          decoration: BoxDecoration(
            color: surface,
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: color.withOpacity(0.3)),
          ),
          child: Column(
            children: List.generate(items.length, (i) {
              final isLast = i == items.length - 1;
              return Column(
                children: [
                  Padding(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 14,
                      vertical: 10,
                    ),
                    child: Row(
                      children: [
                        Icon(Icons.circle, size: 6, color: color),
                        const SizedBox(width: 10),
                        Expanded(
                          child: Text(
                            items[i],
                            style: TextStyle(
                              color: textPrimary,
                              fontSize: 12,
                              fontWeight: FontWeight.w500,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                  if (!isLast)
                    Divider(height: 1, color: border.withOpacity(0.2)),
                ],
              );
            }),
          ),
        ),
      ],
    );
  }
}

// ─── Employee wait chart ────────────────────────────────────────────────────

class _EmployeeWait {
  final String employeeId, vehicleId, arrivalTime, departureTime;
  final int dwellMins;

  const _EmployeeWait({
    required this.employeeId,
    required this.vehicleId,
    required this.arrivalTime,
    required this.departureTime,
    required this.dwellMins,
  });
}

class _WaitTimeChart extends StatelessWidget {
  final List<_EmployeeWait> waitTimes;
  final Color surface, border, textPrimary, textSecondary;

  const _WaitTimeChart({
    required this.waitTimes,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final maxWait = waitTimes.fold<int>(
      0,
      (m, w) => w.dwellMins > m ? w.dwellMins : m,
    );
    // 30 min threshold for a "long wait" flag
    const longWaitThreshold = 30;

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Column(
        children: [
          // Legend
          Row(
            children: [
              Container(
                width: 12,
                height: 5,
                decoration: BoxDecoration(
                  color: context.primary,
                  borderRadius: BorderRadius.circular(3),
                ),
              ),
              const SizedBox(width: 5),
              Text(
                'Wait ≤ 30 min',
                style: TextStyle(color: textSecondary, fontSize: 10),
              ),
              const SizedBox(width: 14),
              Container(
                width: 12,
                height: 5,
                decoration: BoxDecoration(
                  color: AppColors.warning,
                  borderRadius: BorderRadius.circular(3),
                ),
              ),
              const SizedBox(width: 5),
              Text(
                'Long wait > 30 min',
                style: TextStyle(color: textSecondary, fontSize: 10),
              ),
            ],
          ),
          const SizedBox(height: 12),
          ...waitTimes.map((w) {
            final frac = maxWait > 0
                ? (w.dwellMins / maxWait).clamp(0.0, 1.0)
                : 0.0;
            final isLong = w.dwellMins > longWaitThreshold;
            final barColor = isLong ? AppColors.warning : context.primary;

            return Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Row(
                children: [
                  // Employee ID
                  SizedBox(
                    width: 36,
                    child: Text(
                      w.employeeId,
                      style: TextStyle(
                        color: textPrimary,
                        fontSize: 11,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
                  const SizedBox(width: 6),
                  // Bar
                  Expanded(
                    child: Stack(
                      children: [
                        Container(
                          height: 14,
                          decoration: BoxDecoration(
                            color: border.withOpacity(0.25),
                            borderRadius: BorderRadius.circular(7),
                          ),
                        ),
                        FractionallySizedBox(
                          widthFactor: frac,
                          child: Container(
                            height: 14,
                            decoration: BoxDecoration(
                              color: barColor,
                              borderRadius: BorderRadius.circular(7),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(width: 8),
                  // Value + flag
                  SizedBox(
                    width: 64,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.end,
                      children: [
                        Text(
                          '${w.dwellMins} min',
                          style: TextStyle(
                            color: isLong ? AppColors.warning : textPrimary,
                            fontSize: 11,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                        if (isLong) ...[
                          const SizedBox(width: 3),
                          Icon(
                            Icons.warning_rounded,
                            size: 12,
                            color: AppColors.warning,
                          ),
                        ],
                      ],
                    ),
                  ),
                ],
              ),
            );
          }),
          Divider(height: 16, color: border.withOpacity(0.25)),
          // Sub-row showing times
          ...waitTimes.map(
            (w) => Padding(
              padding: const EdgeInsets.only(bottom: 4),
              child: Row(
                children: [
                  SizedBox(
                    width: 36,
                    child: Text(
                      w.employeeId,
                      style: TextStyle(color: textSecondary, fontSize: 9),
                    ),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    '${w.arrivalTime} → ${w.departureTime}',
                    style: TextStyle(color: textSecondary, fontSize: 10),
                  ),
                  SizedBox(width: 4),
                  Text(
                    '(${w.vehicleId})',
                    style: TextStyle(
                      color: context.primary.withOpacity(0.7),
                      fontSize: 10,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// ─── Shared section label ────────────────────────────────────────────────────

class _SectionLabel extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color textPrimary;

  const _SectionLabel({
    required this.icon,
    required this.label,
    required this.textPrimary,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Icon(icon, size: 15, color: context.primary),
        const SizedBox(width: 6),
        Flexible(
          child: Text(
            label,
            style: TextStyle(
              color: textPrimary,
              fontSize: 13,
              fontWeight: FontWeight.w700,
            ),
          ),
        ),
      ],
    );
  }
}
