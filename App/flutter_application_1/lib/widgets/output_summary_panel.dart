import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class OutputSummaryPanel extends StatelessWidget {
  final Map<String, dynamic> data;

  const OutputSummaryPanel({super.key, required this.data});

  // Parse "HH:MM" → total minutes
  static int _toMin(String t) {
    final p = t.split(':');
    if (p.length != 2) return 0;
    return (int.tryParse(p[0]) ?? 0) * 60 + (int.tryParse(p[1]) ?? 0);
  }

  static String _fmtMins(int m) {
    final h = m ~/ 60;
    final min = m % 60;
    if (h > 0) return '${h}h ${min}m';
    return '${min}m';
  }

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final size = MediaQuery.of(context).size;
    final isWide = size.width > 500;

    final surface = dark ? AppColors.darkSurface : AppColors.lightSurface;
    final textPrimary = dark ? Colors.white : AppColors.textPrimaryLight;
    final textSecondary = dark ? Colors.white54 : AppColors.textSecondaryLight;
    final border = dark ? AppColors.darkBorderColor : AppColors.borderColor;

    // ── Extract result block ──
    final result = (data['result'] as Map<String, dynamic>?) ?? {};
    final routes = (data['routes'] as List?) ?? [];

    final totalCost = (result['total_cost'] as num?)?.toDouble() ?? 0.0;
    final baselineCost = (result['baseline_cost'] as num?)?.toDouble() ?? 0.0;
    final costSavings =
        (result['cost_savings'] as num?)?.toDouble() ??
        (baselineCost - totalCost).clamp(0.0, double.infinity);
    final costSavingsPct =
        (result['cost_savings_percent'] as num?)?.toDouble() ??
        (baselineCost > 0 ? costSavings / baselineCost * 100 : 0.0);
    final totalTime = (result['total_time'] as num?)?.toDouble() ?? 0.0;
    final baselineTime = (result['baseline_time'] as num?)?.toDouble() ?? 0.0;
    final timeSaved = (baselineTime - totalTime).clamp(0.0, double.infinity);
    final timeSavedPct = baselineTime > 0
        ? (timeSaved / baselineTime * 100)
        : 0.0;
    final totalDist = (result['total_distance'] as num?)?.toDouble() ?? 0.0;
    final vehiclesUsed =
        (result['vehicles_used'] as num?)?.toInt() ?? routes.length;
    final vehiclesAvailable =
        (result['vehicles_available'] as num?)?.toInt() ?? vehiclesUsed;
    final hardViol = (result['hard_violations'] as num?)?.toInt() ?? 0;
    final softViol = (result['soft_violations'] as num?)?.toInt() ?? 0;
    final optimizationId = data['optimization_id']?.toString() ?? '';

    // ── Derive per-route stats ──
    final Set<String> employeeIds = {};
    int totalTrips = 0;
    double sumCapUtil = 0;
    int capUtilCount = 0;
    int earliestMin = 24 * 60;
    int latestMin = 0;

    for (final r in routes) {
      final route = r as Map<String, dynamic>;
      totalTrips += (route['trips_count'] as num?)?.toInt() ?? 0;
      final cu = (route['capacity_utilization'] as num?)?.toDouble() ?? 0.0;
      if (cu > 0) {
        sumCapUtil += cu;
        capUtilCount++;
      }
      for (final pt in (route['route_points'] as List?) ?? []) {
        final p = pt as Map<String, dynamic>;
        if (p['type'] == 'pickup') {
          final id = p['employee_id']?.toString() ?? '';
          if (id.isNotEmpty) employeeIds.add(id);
          final arr = _toMin(p['arrival_time']?.toString() ?? '');
          if (arr > 0 && arr < earliestMin) earliestMin = arr;
        }
        if (p['type'] == 'office') {
          final arr = _toMin(p['arrival_time']?.toString() ?? '');
          if (arr > latestMin) latestMin = arr;
        }
      }
    }

    final employeeCount = employeeIds.length;
    final avgCapUtil = capUtilCount > 0 ? sumCapUtil / capUtilCount : 0.0;
    final fleetUtil = vehiclesAvailable > 0
        ? vehiclesUsed / vehiclesAvailable * 100
        : 0.0;
    final windowMins = (latestMin > earliestMin && earliestMin < 24 * 60)
        ? latestMin - earliestMin
        : 0;

    // Efficiency ratios
    final costPerKm = totalDist > 0 ? totalCost / totalDist : 0.0;
    final costPerEmp = employeeCount > 0 ? totalCost / employeeCount : 0.0;
    final distPerEmp = employeeCount > 0 ? totalDist / employeeCount : 0.0;
    final costPerTrip = totalTrips > 0 ? totalCost / totalTrips : 0.0;

    final isOptimal = hardViol == 0 && softViol == 0;
    final qualityColor = hardViol > 0
        ? AppColors.error
        : softViol > 0
        ? AppColors.warning
        : context.primary;

    final bottomPad = MediaQuery.of(context).padding.bottom;
    return SingleChildScrollView(
      padding: EdgeInsets.fromLTRB(16, 14, 16, 28 + bottomPad),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // ── Solution quality header ──
          _QualityHeader(
            isOptimal: isOptimal,
            hardViol: hardViol,
            softViol: softViol,
            qualityColor: qualityColor,
            optimizationId: optimizationId,
            textSecondary: textSecondary,
          ),
          const SizedBox(height: 14),

          // ── Big savings card ──
          if (costSavings > 0)
            _SavingsHeroCard(
              costSavings: costSavings,
              costSavingsPct: costSavingsPct,
              baselineCost: baselineCost,
              totalCost: totalCost,
            ),
          if (costSavings > 0) const SizedBox(height: 14),

          // ── 2 × 2 KPI grid ──
          _KpiGrid(
            isWide: isWide,
            totalCost: totalCost,
            totalDist: totalDist,
            timeSaved: timeSaved,
            timeSavedPct: timeSavedPct,
            totalTime: totalTime,
            windowMins: windowMins,
            surface: surface,
            border: border,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
          const SizedBox(height: 14),

          // ── Fleet snapshot ──
          _FleetSnapshot(
            vehiclesUsed: vehiclesUsed,
            vehiclesAvailable: vehiclesAvailable,
            fleetUtil: fleetUtil,
            employeeCount: employeeCount,
            totalTrips: totalTrips,
            avgCapUtil: avgCapUtil,
            surface: surface,
            border: border,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
          const SizedBox(height: 14),

          // ── Efficiency ratios ──
          _EfficiencyRatios(
            costPerKm: costPerKm,
            costPerEmp: costPerEmp,
            distPerEmp: distPerEmp,
            costPerTrip: costPerTrip,
            surface: surface,
            border: border,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),

          // ── Service window ──
          if (windowMins > 0) ...[
            const SizedBox(height: 14),
            _ServiceWindow(
              earliestMin: earliestMin,
              latestMin: latestMin,
              windowMins: windowMins,
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

// ─── Quality header bar ─────────────────────────────────────────────────────

class _QualityHeader extends StatelessWidget {
  final bool isOptimal;
  final int hardViol, softViol;
  final Color qualityColor;
  final String optimizationId;
  final Color textSecondary;

  const _QualityHeader({
    required this.isOptimal,
    required this.hardViol,
    required this.softViol,
    required this.qualityColor,
    required this.optimizationId,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final label = hardViol > 0
        ? 'Infeasible — $hardViol hard violation${hardViol > 1 ? 's' : ''}'
        : softViol > 0
        ? 'Feasible — $softViol soft violation${softViol > 1 ? 's' : ''}'
        : 'Optimal — all constraints satisfied';

    return Row(
      children: [
        Flexible(
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
            decoration: BoxDecoration(
              color: qualityColor.withOpacity(0.1),
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: qualityColor.withOpacity(0.35)),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(
                  isOptimal
                      ? Icons.verified_rounded
                      : hardViol > 0
                      ? Icons.error_rounded
                      : Icons.warning_rounded,
                  color: qualityColor,
                  size: 15,
                ),
                const SizedBox(width: 6),
                Flexible(
                  child: Text(
                    label,
                    style: TextStyle(
                      color: qualityColor,
                      fontSize: 12,
                      fontWeight: FontWeight.w700,
                    ),
                    overflow: TextOverflow.ellipsis,
                  ),
                ),
              ],
            ),
          ),
        ),
        if (optimizationId.isNotEmpty) ...[
          const SizedBox(width: 8),
          Text(
            'ID: …${optimizationId.length > 8 ? optimizationId.substring(optimizationId.length - 8) : optimizationId}',
            style: TextStyle(color: textSecondary, fontSize: 10),
          ),
        ],
      ],
    );
  }
}

// ─── Savings hero ───────────────────────────────────────────────────────────

class _SavingsHeroCard extends StatelessWidget {
  final double costSavings, costSavingsPct, baselineCost, totalCost;

  const _SavingsHeroCard({
    required this.costSavings,
    required this.costSavingsPct,
    required this.baselineCost,
    required this.totalCost,
  });

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(18),
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: [
            context.primary.withOpacity(dark ? 0.18 : 0.12),
            context.primary.withOpacity(dark ? 0.06 : 0.04),
          ],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: context.primary.withOpacity(0.3)),
      ),
      child: Row(
        children: [
          // Savings amount
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Icon(
                      Icons.trending_down_rounded,
                      color: context.primary,
                      size: 16,
                    ),
                    SizedBox(width: 5),
                    Text(
                      'Cost Savings',
                      style: TextStyle(
                        color: context.primary.withOpacity(0.8),
                        fontSize: 11,
                        fontWeight: FontWeight.w600,
                        letterSpacing: 0.4,
                      ),
                    ),
                  ],
                ),
                SizedBox(height: 4),
                Text(
                  '₹${_fmt(costSavings)}',
                  style: TextStyle(
                    color: context.primary,
                    fontSize: 28,
                    fontWeight: FontWeight.w800,
                    height: 1.1,
                  ),
                ),
                SizedBox(height: 3),
                Text(
                  'vs ₹${_fmt(baselineCost)} baseline',
                  style: TextStyle(
                    color: context.primary.withOpacity(0.6),
                    fontSize: 11,
                  ),
                ),
              ],
            ),
          ),
          // Percentage badge
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
            decoration: BoxDecoration(
              color: context.primary.withOpacity(0.15),
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: context.primary.withOpacity(0.3)),
            ),
            child: Column(
              children: [
                Text(
                  '${costSavingsPct.toStringAsFixed(1)}%',
                  style: TextStyle(
                    color: context.primary,
                    fontSize: 22,
                    fontWeight: FontWeight.w800,
                  ),
                ),
                Text(
                  'saved',
                  style: TextStyle(
                    color: context.primary.withOpacity(0.7),
                    fontSize: 10,
                    fontWeight: FontWeight.w500,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  String _fmt(double v) {
    if (v >= 1000) return '${(v / 1000).toStringAsFixed(1)}k';
    return v.toStringAsFixed(0);
  }
}

// ─── KPI 2×2 grid ───────────────────────────────────────────────────────────

class _KpiGrid extends StatelessWidget {
  final bool isWide;
  final double totalCost, totalDist, timeSaved, timeSavedPct, totalTime;
  final int windowMins;
  final Color surface, border, textPrimary, textSecondary;

  const _KpiGrid({
    required this.isWide,
    required this.totalCost,
    required this.totalDist,
    required this.timeSaved,
    required this.timeSavedPct,
    required this.totalTime,
    required this.windowMins,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final tiles = [
      _KpiTile(
        icon: Icons.currency_rupee_rounded,
        label: 'Optimized Cost',
        value: '₹${totalCost.toStringAsFixed(0)}',
        sub: 'total fleet cost',
        surface: surface,
        border: border,
        textPrimary: textPrimary,
        textSecondary: textSecondary,
      ),
      _KpiTile(
        icon: Icons.route_rounded,
        label: 'Total Distance',
        value: '${totalDist.toStringAsFixed(1)} km',
        sub: 'across all routes',
        surface: surface,
        border: border,
        textPrimary: textPrimary,
        textSecondary: textSecondary,
      ),
      _KpiTile(
        icon: Icons.schedule_rounded,
        label: 'Time Saved',
        value: timeSaved > 0 ? '${timeSaved.toInt()} min' : '—',
        sub: timeSaved > 0
            ? '${timeSavedPct.toStringAsFixed(1)}% reduction'
            : '${totalTime.toInt()} min total',
        surface: surface,
        border: border,
        textPrimary: textPrimary,
        textSecondary: textSecondary,
      ),
      _KpiTile(
        icon: Icons.access_time_rounded,
        label: 'Service Window',
        value: windowMins > 0 ? OutputSummaryPanel._fmtMins(windowMins) : '—',
        sub: 'first pickup → last drop',
        surface: surface,
        border: border,
        textPrimary: textPrimary,
        textSecondary: textSecondary,
      ),
    ];

    return GridView.count(
      crossAxisCount: isWide ? 4 : 2,
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      crossAxisSpacing: 10,
      mainAxisSpacing: 10,
      childAspectRatio: isWide ? 1.0 : 1.2,
      children: tiles,
    );
  }
}

class _KpiTile extends StatelessWidget {
  final IconData icon;
  final String label, value, sub;
  final Color surface, border, textPrimary, textSecondary;

  const _KpiTile({
    required this.icon,
    required this.label,
    required this.value,
    required this.sub,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(icon, color: context.primary, size: 18),
          const Spacer(),
          Text(
            value,
            style: TextStyle(
              color: textPrimary,
              fontSize: 16,
              fontWeight: FontWeight.w800,
              height: 1.1,
            ),
          ),
          const SizedBox(height: 2),
          Text(
            label,
            style: TextStyle(
              color: textPrimary,
              fontSize: 11,
              fontWeight: FontWeight.w600,
            ),
          ),
          const SizedBox(height: 1),
          Text(
            sub,
            style: TextStyle(color: textSecondary, fontSize: 10),
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
        ],
      ),
    );
  }
}

// ─── Fleet snapshot row ─────────────────────────────────────────────────────

class _FleetSnapshot extends StatelessWidget {
  final int vehiclesUsed, vehiclesAvailable, employeeCount, totalTrips;
  final double fleetUtil, avgCapUtil;
  final Color surface, border, textPrimary, textSecondary;

  const _FleetSnapshot({
    required this.vehiclesUsed,
    required this.vehiclesAvailable,
    required this.fleetUtil,
    required this.employeeCount,
    required this.totalTrips,
    required this.avgCapUtil,
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
          icon: Icons.local_shipping_rounded,
          label: 'Fleet & Coverage',
          textPrimary: textPrimary,
        ),
        const SizedBox(height: 8),
        Container(
          decoration: BoxDecoration(
            color: surface,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: border.withOpacity(0.35)),
          ),
          child: Column(
            children: [
              // Row of 4 stat items
              IntrinsicHeight(
                child: Row(
                  children: [
                    _SnapshotItem(
                      value: '$vehiclesUsed / $vehiclesAvailable',
                      label: 'Vehicles',
                      sub: '${fleetUtil.toStringAsFixed(0)}% utilized',
                      textPrimary: textPrimary,
                      textSecondary: textSecondary,
                    ),
                    _divider(),
                    _SnapshotItem(
                      value: '$employeeCount',
                      label: 'Employees',
                      sub: 'served today',
                      textPrimary: textPrimary,
                      textSecondary: textSecondary,
                    ),
                    _divider(),
                    _SnapshotItem(
                      value: '$totalTrips',
                      label: 'Trips',
                      sub: 'total runs',
                      textPrimary: textPrimary,
                      textSecondary: textSecondary,
                    ),
                    _divider(),
                    _SnapshotItem(
                      value: '${avgCapUtil.toStringAsFixed(0)}%',
                      label: 'Avg Load',
                      sub: 'capacity util',
                      textPrimary: textPrimary,
                      textSecondary: textSecondary,
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _divider() =>
      VerticalDivider(width: 1, thickness: 1, color: border.withOpacity(0.3));
}

class _SnapshotItem extends StatelessWidget {
  final String value, label, sub;
  final Color textPrimary, textSecondary;

  const _SnapshotItem({
    required this.value,
    required this.label,
    required this.sub,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 12, horizontal: 8),
        child: Column(
          children: [
            Text(
              value,
              style: TextStyle(
                color: textPrimary,
                fontSize: 17,
                fontWeight: FontWeight.w800,
              ),
            ),
            const SizedBox(height: 2),
            Text(
              label,
              style: TextStyle(
                color: textPrimary,
                fontSize: 10,
                fontWeight: FontWeight.w600,
              ),
              textAlign: TextAlign.center,
            ),
            Text(
              sub,
              style: TextStyle(color: textSecondary, fontSize: 9),
              textAlign: TextAlign.center,
            ),
          ],
        ),
      ),
    );
  }
}

// ─── Efficiency ratios ──────────────────────────────────────────────────────

class _EfficiencyRatios extends StatelessWidget {
  final double costPerKm, costPerEmp, distPerEmp, costPerTrip;
  final Color surface, border, textPrimary, textSecondary;

  const _EfficiencyRatios({
    required this.costPerKm,
    required this.costPerEmp,
    required this.distPerEmp,
    required this.costPerTrip,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final metrics = [
      ('₹${costPerKm.toStringAsFixed(2)}', 'Cost / km', 'fleet average'),
      ('₹${costPerEmp.toStringAsFixed(1)}', 'Cost / employee', 'fully loaded'),
      (
        '${distPerEmp.toStringAsFixed(1)} km',
        'Dist / employee',
        'door to office',
      ),
      ('₹${costPerTrip.toStringAsFixed(1)}', 'Cost / trip', 'avg run cost'),
    ];

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionLabel(
          icon: Icons.speed_rounded,
          label: 'Efficiency Ratios',
          textPrimary: textPrimary,
        ),
        const SizedBox(height: 8),
        GridView.builder(
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: 2,
            crossAxisSpacing: 10,
            mainAxisSpacing: 10,
            childAspectRatio: 2.2,
          ),
          itemCount: metrics.length,
          itemBuilder: (_, i) {
            final m = metrics[i];
            return Container(
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
              decoration: BoxDecoration(
                color: surface,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: border.withOpacity(0.3)),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Text(
                    m.$1,
                    style: TextStyle(
                      color: context.primary,
                      fontSize: 14,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  Text(
                    m.$2,
                    style: TextStyle(
                      color: textPrimary,
                      fontSize: 10,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  Text(
                    m.$3,
                    style: TextStyle(color: textSecondary, fontSize: 9),
                  ),
                ],
              ),
            );
          },
        ),
      ],
    );
  }
}

// ─── Service window timeline ────────────────────────────────────────────────

class _ServiceWindow extends StatelessWidget {
  final int earliestMin, latestMin, windowMins;
  final Color surface, border, textPrimary, textSecondary;

  const _ServiceWindow({
    required this.earliestMin,
    required this.latestMin,
    required this.windowMins,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  static String _hhmm(int m) {
    final h = (m ~/ 60).toString().padLeft(2, '0');
    final min = (m % 60).toString().padLeft(2, '0');
    return '$h:$min';
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _SectionLabel(
          icon: Icons.timeline_rounded,
          label: 'Service Window',
          textPrimary: textPrimary,
        ),
        const SizedBox(height: 8),
        Container(
          padding: const EdgeInsets.all(14),
          decoration: BoxDecoration(
            color: surface,
            borderRadius: BorderRadius.circular(14),
            border: Border.all(color: border.withOpacity(0.35)),
          ),
          child: Column(
            children: [
              Row(
                children: [
                  Text(
                    _hhmm(earliestMin),
                    style: TextStyle(
                      color: textPrimary,
                      fontSize: 13,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const Spacer(),
                  Text(
                    OutputSummaryPanel._fmtMins(windowMins),
                    style: TextStyle(
                      color: context.primary,
                      fontSize: 12,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const Spacer(),
                  Text(
                    _hhmm(latestMin),
                    style: TextStyle(
                      color: textPrimary,
                      fontSize: 13,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              // Timeline bar
              Container(
                height: 8,
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [context.primary, context.primary.withOpacity(0.4)],
                  ),
                  borderRadius: BorderRadius.circular(4),
                ),
              ),
              const SizedBox(height: 6),
              Row(
                children: [
                  Text(
                    'First pickup',
                    style: TextStyle(color: textSecondary, fontSize: 10),
                  ),
                  const Spacer(),
                  Text(
                    'Last office drop',
                    style: TextStyle(color: textSecondary, fontSize: 10),
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    );
  }
}

// ─── Shared section label ───────────────────────────────────────────────────

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
        Text(
          label,
          style: TextStyle(
            color: textPrimary,
            fontSize: 13,
            fontWeight: FontWeight.w700,
          ),
        ),
      ],
    );
  }
}
