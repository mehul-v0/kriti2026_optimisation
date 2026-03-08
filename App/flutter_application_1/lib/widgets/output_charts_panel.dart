import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class OutputChartsPanel extends StatelessWidget {
  final Map<String, dynamic> data;

  const OutputChartsPanel({super.key, required this.data});

  @override
  Widget build(BuildContext context) {
    final dark = Theme.of(context).brightness == Brightness.dark;
    final surface = dark ? AppColors.darkSurface : AppColors.lightSurface;
    final textPrimary = dark ? Colors.white : AppColors.textPrimaryLight;
    final textSecondary = dark ? Colors.white54 : AppColors.textSecondaryLight;
    final border = dark ? AppColors.darkBorderColor : AppColors.borderColor;

    final result = (data['result'] as Map<String, dynamic>?) ?? {};
    final stats = (data['stats'] as Map<String, dynamic>?) ?? {};
    final routes = (data['routes'] as List?) ?? [];

    final totalCost =
        (result['total_cost'] as num?)?.toDouble() ??
        (stats['total_cost'] as num?)?.toDouble() ??
        0.0;
    final baselineCost =
        (result['baseline_cost'] as num?)?.toDouble() ??
        (stats['baseline_cost'] as num?)?.toDouble() ??
        0.0;
    final totalTime =
        (result['total_time'] as num?)?.toDouble() ??
        (stats['total_time'] as num?)?.toDouble() ??
        0.0;
    final baselineTime =
        (result['baseline_time'] as num?)?.toDouble() ??
        (stats['baseline_time'] as num?)?.toDouble() ??
        0.0;

    // Per-vehicle data
    final List<_VehicleStats> vehicleStats = [];
    for (final r in routes) {
      final route = r as Map<String, dynamic>;
      vehicleStats.add(
        _VehicleStats(
          vehicleId: route['vehicle_id']?.toString() ?? '?',
          cost: (route['total_cost'] as num?)?.toDouble() ?? 0.0,
          distance: (route['total_distance'] as num?)?.toDouble() ?? 0.0,
          capUtil: (route['capacity_utilization'] as num?)?.toDouble() ?? 0.0,
          passengers: (route['passengers_count'] as num?)?.toInt() ?? 0,
          trips: (route['trips_count'] as num?)?.toInt() ?? 0,
        ),
      );
    }

    final bottomPad = MediaQuery.of(context).padding.bottom;
    return SingleChildScrollView(
      padding: EdgeInsets.fromLTRB(16, 14, 16, 28 + bottomPad),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // ── Cost Comparison ──
          if (baselineCost > 0) ...[
            _SectionLabel(
              icon: Icons.savings_rounded,
              label: 'Cost Comparison',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 10),
            _CostComparisonChart(
              baseline: baselineCost,
              optimized: totalCost,
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),
          ],

          // ── Time Comparison ──
          if (baselineTime > 0) ...[
            _SectionLabel(
              icon: Icons.timer_rounded,
              label: 'Time Comparison',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 10),
            _TimeComparisonChart(
              baselineMins: baselineTime.toInt(),
              optimizedMins: totalTime.toInt(),
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),
          ],

          // ── Fleet Composition ──
          if (vehicleStats.isNotEmpty) ...[
            _SectionLabel(
              icon: Icons.pie_chart_rounded,
              label: 'Fleet Composition',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 10),
            _FleetCompositionChart(
              data: data,
              routes: routes,
              vehicleStats: vehicleStats,
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),
          ],

          // ── Per-Vehicle Cost ──
          if (vehicleStats.isNotEmpty) ...[
            _SectionLabel(
              icon: Icons.directions_bus_rounded,
              label: 'Cost per Vehicle',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 10),
            _VehicleBarChart(
              vehicles: vehicleStats,
              valueSelector: (v) => v.cost,
              labelFormatter: (v) => '₹${v.toStringAsFixed(1)}',
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),

            // ── Per-Vehicle Distance ──
            _SectionLabel(
              icon: Icons.route_rounded,
              label: 'Distance per Vehicle',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 10),
            _VehicleBarChart(
              vehicles: vehicleStats,
              valueSelector: (v) => v.distance,
              labelFormatter: (v) => '${v.toStringAsFixed(1)} km',
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),

            // ── Capacity Utilization ──
            _SectionLabel(
              icon: Icons.people_rounded,
              label: 'Capacity Utilization',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 4),
            Text(
              'Passengers carried vs total seat capacity',
              style: TextStyle(color: textSecondary, fontSize: 11),
            ),
            const SizedBox(height: 10),
            _CapacityUtilChart(
              vehicles: vehicleStats,
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),

            // ── Passengers per Vehicle ──
            _SectionLabel(
              icon: Icons.person_rounded,
              label: 'Passengers per Vehicle',
              textPrimary: textPrimary,
            ),
            const SizedBox(height: 10),
            _VehicleBarChart(
              vehicles: vehicleStats,
              valueSelector: (v) => v.passengers.toDouble(),
              labelFormatter: (v) => '${v.toInt()} emp',
              surface: surface,
              border: border,
              textPrimary: textPrimary,
              textSecondary: textSecondary,
            ),
            const SizedBox(height: 20),
          ],
        ],
      ),
    );
  }
}

// ─── Data model ──────────────────────────────────────────────────────────────

class _VehicleStats {
  final String vehicleId;
  final double cost, distance, capUtil;
  final int passengers, trips;

  const _VehicleStats({
    required this.vehicleId,
    required this.cost,
    required this.distance,
    required this.capUtil,
    required this.passengers,
    required this.trips,
  });
}

// ─── Cost comparison chart ──────────────────────────────────────────────────

class _CostComparisonChart extends StatelessWidget {
  final double baseline, optimized;
  final Color surface, border, textPrimary, textSecondary;

  const _CostComparisonChart({
    required this.baseline,
    required this.optimized,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final saved = (baseline - optimized).clamp(0.0, double.infinity);
    final savedPct = baseline > 0 ? (saved / baseline * 100) : 0.0;
    final optimizedFrac = baseline > 0
        ? (optimized / baseline).clamp(0.0, 1.0)
        : 0.0;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Column(
        children: [
          // Savings callout
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
            margin: const EdgeInsets.only(bottom: 16),
            decoration: BoxDecoration(
              color: context.primary.withOpacity(0.1),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(
                  Icons.trending_down_rounded,
                  size: 16,
                  color: context.primary,
                ),
                const SizedBox(width: 6),
                RichText(
                  text: TextSpan(
                    children: [
                      TextSpan(
                        text: 'Saved ₹${saved.toStringAsFixed(1)}',
                        style: TextStyle(
                          color: context.primary,
                          fontWeight: FontWeight.w800,
                          fontSize: 14,
                        ),
                      ),
                      TextSpan(
                        text: '  (${savedPct.toStringAsFixed(1)}% reduction)',
                        style: TextStyle(
                          color: context.primary.withOpacity(0.75),
                          fontSize: 12,
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
          // Baseline bar
          _CompBar(
            label: 'Baseline',
            value: '₹${baseline.toStringAsFixed(1)}',
            fraction: 1.0,
            color: border.withOpacity(0.5),
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
          SizedBox(height: 10),
          // Optimized bar
          _CompBar(
            label: 'Optimized',
            value: '₹${optimized.toStringAsFixed(1)}',
            fraction: optimizedFrac,
            color: context.primary,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
        ],
      ),
    );
  }
}

// ─── Time comparison chart ──────────────────────────────────────────────────

class _TimeComparisonChart extends StatelessWidget {
  final int baselineMins, optimizedMins;
  final Color surface, border, textPrimary, textSecondary;

  const _TimeComparisonChart({
    required this.baselineMins,
    required this.optimizedMins,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  String _fmt(int m) {
    if (m < 60) return '${m}m';
    return '${m ~/ 60}h ${m % 60}m';
  }

  @override
  Widget build(BuildContext context) {
    final savedMins = (baselineMins - optimizedMins).clamp(0, 999999);
    final fraction = baselineMins > 0
        ? (optimizedMins / baselineMins).clamp(0.0, 1.0)
        : 0.0;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Column(
        children: [
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
            margin: const EdgeInsets.only(bottom: 16),
            decoration: BoxDecoration(
              color: context.primary.withOpacity(0.1),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(
                  Icons.trending_down_rounded,
                  size: 16,
                  color: context.primary,
                ),
                SizedBox(width: 6),
                Text(
                  'Saved ${_fmt(savedMins)}',
                  style: TextStyle(
                    color: context.primary,
                    fontWeight: FontWeight.w800,
                    fontSize: 14,
                  ),
                ),
              ],
            ),
          ),
          _CompBar(
            label: 'Baseline',
            value: _fmt(baselineMins),
            fraction: 1.0,
            color: border.withOpacity(0.5),
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
          SizedBox(height: 10),
          _CompBar(
            label: 'Optimized',
            value: _fmt(optimizedMins),
            fraction: fraction,
            color: context.primary,
            textPrimary: textPrimary,
            textSecondary: textSecondary,
          ),
        ],
      ),
    );
  }
}

// ─── Shared comparison bar ───────────────────────────────────────────────────

class _CompBar extends StatelessWidget {
  final String label, value;
  final double fraction;
  final Color color, textPrimary, textSecondary;

  const _CompBar({
    required this.label,
    required this.value,
    required this.fraction,
    required this.color,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        SizedBox(
          width: 72,
          child: Text(
            label,
            style: TextStyle(
              color: textSecondary,
              fontSize: 12,
              fontWeight: FontWeight.w600,
            ),
          ),
        ),
        Expanded(
          child: Stack(
            children: [
              Container(
                height: 22,
                decoration: BoxDecoration(
                  color: color.withOpacity(0.2),
                  borderRadius: BorderRadius.circular(6),
                ),
              ),
              FractionallySizedBox(
                widthFactor: fraction,
                child: Container(
                  height: 22,
                  decoration: BoxDecoration(
                    color: color,
                    borderRadius: BorderRadius.circular(6),
                  ),
                ),
              ),
              // Value label overlaid on bar
              Positioned.fill(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 8),
                  child: Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      value,
                      style: TextStyle(
                        color: Colors.white,
                        fontSize: 11,
                        fontWeight: FontWeight.w700,
                        shadows: [Shadow(color: Colors.black45, blurRadius: 4)],
                      ),
                    ),
                  ),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}

// ─── Generic vehicle bar chart ───────────────────────────────────────────────

class _VehicleBarChart extends StatelessWidget {
  final List<_VehicleStats> vehicles;
  final double Function(_VehicleStats) valueSelector;
  final String Function(double) labelFormatter;
  final Color surface, border, textPrimary, textSecondary;

  const _VehicleBarChart({
    required this.vehicles,
    required this.valueSelector,
    required this.labelFormatter,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    final maxVal = vehicles.fold<double>(0, (m, v) {
      final val = valueSelector(v);
      return val > m ? val : m;
    });

    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Column(
        children: List.generate(vehicles.length, (i) {
          final v = vehicles[i];
          final val = valueSelector(v);
          final frac = maxVal > 0 ? (val / maxVal).clamp(0.0, 1.0) : 0.0;
          // Slightly desaturate non-largest bars for visual hierarchy
          final opacity = frac >= 0.9
              ? 1.0
              : frac >= 0.6
              ? 0.8
              : 0.65;

          return Padding(
            padding: const EdgeInsets.only(bottom: 10),
            child: Row(
              children: [
                SizedBox(
                  width: 36,
                  child: Text(
                    v.vehicleId,
                    style: TextStyle(
                      color: textPrimary,
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ),
                const SizedBox(width: 6),
                Expanded(
                  child: Stack(
                    children: [
                      Container(
                        height: 20,
                        decoration: BoxDecoration(
                          color: context.primary.withOpacity(0.1),
                          borderRadius: BorderRadius.circular(6),
                        ),
                      ),
                      FractionallySizedBox(
                        widthFactor: frac,
                        child: Container(
                          height: 20,
                          decoration: BoxDecoration(
                            color: context.primary.withOpacity(opacity),
                            borderRadius: BorderRadius.circular(6),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 8),
                SizedBox(
                  width: 66,
                  child: Text(
                    labelFormatter(val),
                    textAlign: TextAlign.right,
                    style: TextStyle(
                      color: textPrimary,
                      fontSize: 11,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ),
              ],
            ),
          );
        }),
      ),
    );
  }
}

// ─── Capacity utilization chart ──────────────────────────────────────────────

class _CapacityUtilChart extends StatelessWidget {
  final List<_VehicleStats> vehicles;
  final Color surface, border, textPrimary, textSecondary;

  const _CapacityUtilChart({
    required this.vehicles,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
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
                width: 10,
                height: 10,
                decoration: BoxDecoration(
                  color: context.primary,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
              const SizedBox(width: 4),
              Text(
                'Normal',
                style: TextStyle(color: textSecondary, fontSize: 10),
              ),
              const SizedBox(width: 12),
              Container(
                width: 10,
                height: 10,
                decoration: BoxDecoration(
                  color: AppColors.warning,
                  borderRadius: BorderRadius.circular(2),
                ),
              ),
              const SizedBox(width: 4),
              Text(
                'Overloaded (>100%)',
                style: TextStyle(color: textSecondary, fontSize: 10),
              ),
            ],
          ),
          SizedBox(height: 12),
          ...vehicles.map((v) {
            final isOverloaded = v.capUtil > 100;
            final barColor = isOverloaded ? AppColors.warning : context.primary;
            // Cap at 130% visually
            final frac = (v.capUtil / 130).clamp(0.0, 1.0);

            return Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Row(
                children: [
                  SizedBox(
                    width: 36,
                    child: Text(
                      v.vehicleId,
                      style: TextStyle(
                        color: textPrimary,
                        fontSize: 11,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
                  const SizedBox(width: 6),
                  Expanded(
                    child: Stack(
                      children: [
                        Container(
                          height: 20,
                          decoration: BoxDecoration(
                            color: barColor.withOpacity(0.1),
                            borderRadius: BorderRadius.circular(6),
                          ),
                        ),
                        // 100% marker line
                        FractionallySizedBox(
                          widthFactor: (100 / 130).clamp(0.0, 1.0),
                          child: Align(
                            alignment: Alignment.centerRight,
                            child: Container(
                              width: 1.5,
                              height: 20,
                              color: border.withOpacity(0.5),
                            ),
                          ),
                        ),
                        FractionallySizedBox(
                          widthFactor: frac,
                          child: Container(
                            height: 20,
                            decoration: BoxDecoration(
                              color: barColor,
                              borderRadius: BorderRadius.circular(6),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(width: 8),
                  SizedBox(
                    width: 72,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.end,
                      children: [
                        Text(
                          '${v.capUtil.toStringAsFixed(0)}%',
                          style: TextStyle(
                            color: isOverloaded
                                ? AppColors.warning
                                : textPrimary,
                            fontSize: 11,
                            fontWeight: FontWeight.w800,
                          ),
                        ),
                        if (isOverloaded) ...[
                          const SizedBox(width: 2),
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
        ],
      ),
    );
  }
}

// ─── Fleet composition chart ─────────────────────────────────────────────────

class _DonutSegment {
  final String label;
  final double value;
  final Color color;
  const _DonutSegment({
    required this.label,
    required this.value,
    required this.color,
  });
}

class _DonutPainter extends CustomPainter {
  final List<_DonutSegment> segments;
  final double total;

  const _DonutPainter({required this.segments, required this.total});

  static const double _strokeWidth = 22;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2 - _strokeWidth / 2;
    double startAngle = -math.pi / 2;

    for (final seg in segments) {
      if (seg.value <= 0) continue;
      final sweepAngle = (seg.value / total) * 2 * math.pi;
      canvas.drawArc(
        Rect.fromCircle(center: center, radius: radius),
        startAngle,
        sweepAngle - 0.04,
        false,
        Paint()
          ..color = seg.color
          ..style = PaintingStyle.stroke
          ..strokeWidth = _strokeWidth
          ..strokeCap = StrokeCap.butt,
      );
      startAngle += sweepAngle;
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class _FleetCompositionChart extends StatelessWidget {
  final Map<String, dynamic> data;
  final List routes;
  final List<_VehicleStats> vehicleStats;
  final Color surface, border, textPrimary, textSecondary;

  const _FleetCompositionChart({
    required this.data,
    required this.routes,
    required this.vehicleStats,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    // Prefer input_vehicles (contains fuel_type/category from input data)
    final inputVehicles =
        (data['input_vehicles'] as List?) ?? (data['vehicles'] as List?);

    // If input vehicles data is available → fuel type breakdown
    if (inputVehicles != null && inputVehicles.isNotEmpty) {
      return _buildFuelTypeChart(context, inputVehicles);
    }
    // Fallback → vehicle mode breakdown inferred from capacity utilization
    return _buildModeChart(context);
  }

  Widget _buildFuelTypeChart(BuildContext context, List vehicles) {
    int electric = 0, diesel = 0, petrol = 0;
    for (final v in vehicles) {
      final vMap = v as Map<String, dynamic>;
      // Use explicit fuel_type field if present, else infer from category
      final fuelType = vMap['fuel_type']?.toString().toLowerCase();
      if (fuelType != null) {
        if (fuelType == 'electric')
          electric++;
        else if (fuelType == 'diesel')
          diesel++;
        else
          petrol++;
      } else {
        final cat = vMap['category']?.toString().toLowerCase() ?? 'normal';
        if (cat == 'electric')
          electric++;
        else if (cat == 'premium')
          diesel++;
        else
          petrol++;
      }
    }
    final segments = <_DonutSegment>[
      if (electric > 0)
        _DonutSegment(
          label: 'Electric',
          value: electric.toDouble(),
          color: context.primary,
        ),
      if (diesel > 0)
        _DonutSegment(
          label: 'Diesel',
          value: diesel.toDouble(),
          color: AppColors.warning,
        ),
      if (petrol > 0)
        _DonutSegment(
          label: 'Petrol',
          value: petrol.toDouble(),
          color: AppColors.silver,
        ),
    ];
    return _DonutWithLegend(
      segments: segments,
      title: 'Fleet Type',
      subtitle: '${vehicles.length} vehicles total',
      surface: surface,
      border: border,
      textPrimary: textPrimary,
      textSecondary: textSecondary,
    );
  }

  Widget _buildModeChart(BuildContext context) {
    int twoWheeler = 0, fourWheeler = 0, van = 0;
    for (final r in routes) {
      final route = r as Map<String, dynamic>;
      final passengers = (route['passengers_count'] as num?)?.toInt() ?? 0;
      final capUtil =
          (route['capacity_utilization'] as num?)?.toDouble() ?? 0.0;
      final capacity = capUtil > 0 ? (passengers / (capUtil / 100)).round() : 4;
      if (capacity <= 2)
        twoWheeler++;
      else if (capacity <= 4)
        fourWheeler++;
      else
        van++;
    }
    final segments = <_DonutSegment>[
      if (twoWheeler > 0)
        _DonutSegment(
          label: '2-Wheeler',
          value: twoWheeler.toDouble(),
          color: context.primary.withOpacity(0.6),
        ),
      if (fourWheeler > 0)
        _DonutSegment(
          label: '4-Wheeler',
          value: fourWheeler.toDouble(),
          color: context.primary,
        ),
      if (van > 0)
        _DonutSegment(
          label: 'Van',
          value: van.toDouble(),
          color: context.primaryDark,
        ),
    ];
    return _DonutWithLegend(
      segments: segments,
      title: 'Vehicle Mode',
      subtitle: '${routes.length} vehicles in use',
      surface: surface,
      border: border,
      textPrimary: textPrimary,
      textSecondary: textSecondary,
    );
  }
}

class _DonutWithLegend extends StatelessWidget {
  final List<_DonutSegment> segments;
  final String title, subtitle;
  final Color surface, border, textPrimary, textSecondary;

  const _DonutWithLegend({
    required this.segments,
    required this.title,
    required this.subtitle,
    required this.surface,
    required this.border,
    required this.textPrimary,
    required this.textSecondary,
  });

  @override
  Widget build(BuildContext context) {
    if (segments.isEmpty) return const SizedBox.shrink();
    final total = segments.fold(0.0, (s, e) => s + e.value);

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: surface,
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: border.withOpacity(0.35)),
      ),
      child: Row(
        children: [
          // Donut
          SizedBox(
            width: 100,
            height: 100,
            child: CustomPaint(
              painter: _DonutPainter(segments: segments, total: total),
              size: const Size(100, 100),
            ),
          ),
          const SizedBox(width: 20),
          // Legend
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  title,
                  style: TextStyle(
                    color: textPrimary,
                    fontSize: 13,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                Text(
                  subtitle,
                  style: TextStyle(color: textSecondary, fontSize: 10),
                ),
                const SizedBox(height: 10),
                ...segments.map((seg) {
                  final pct = total > 0 ? (seg.value / total * 100) : 0.0;
                  return Padding(
                    padding: const EdgeInsets.only(bottom: 6),
                    child: Row(
                      children: [
                        Container(
                          width: 10,
                          height: 10,
                          decoration: BoxDecoration(
                            color: seg.color,
                            borderRadius: BorderRadius.circular(2),
                          ),
                        ),
                        const SizedBox(width: 6),
                        Expanded(
                          child: Text(
                            seg.label,
                            style: TextStyle(
                              color: textPrimary,
                              fontSize: 11,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ),
                        Text(
                          '${seg.value.toInt()}  (${pct.toStringAsFixed(0)}%)',
                          style: TextStyle(color: textSecondary, fontSize: 10),
                        ),
                      ],
                    ),
                  );
                }),
              ],
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
