import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

// Theme helpers — used by both card types
bool _isDark(BuildContext ctx) => Theme.of(ctx).brightness == Brightness.dark;
Color _textPrimary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white : AppColors.textPrimaryLight;
Color _textSecondary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white54 : AppColors.textSecondaryLight;
Color _textTertiary(BuildContext ctx) =>
    _isDark(ctx) ? Colors.white38 : Colors.black38;
Color _surfaceColor(BuildContext ctx) =>
    _isDark(ctx) ? AppColors.darkSurface : AppColors.lightSurface;
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

/// Card for a single employee in the input preview list.
class EmployeeCard extends StatelessWidget {
  final Map<String, dynamic> emp;
  final List<Map<String, dynamic>> baseline;
  final String? highlightedId;
  final Map<String, GlobalKey> cardKeys;

  const EmployeeCard({
    super.key,
    required this.emp,
    required this.baseline,
    this.highlightedId,
    required this.cardKeys,
  });

  @override
  Widget build(BuildContext context) {
    final id = emp['employee_id']?.toString() ?? '';
    final shortId = id.length > 1 ? id.substring(1) : id;
    final priority = emp['priority'];
    final pColor = _priorityColor(context, priority);
    final pickup = emp['earliest_pickup']?.toString() ?? '--';
    final drop = emp['latest_drop']?.toString() ?? '--';
    final vPref = emp['vehicle_preference']?.toString() ?? 'any';
    final sPref = emp['sharing_preference']?.toString() ?? '';
    final isHighlighted = highlightedId == id;

    final bl = baseline.firstWhere(
      (b) => b['employee_id'] == id,
      orElse: () => <String, dynamic>{},
    );
    final baselineCost = bl['baseline_cost'];

    return AnimatedContainer(
      key: cardKeys.putIfAbsent(id, () => GlobalKey()),
      duration: const Duration(milliseconds: 400),
      margin: const EdgeInsets.only(bottom: 10),
      decoration: BoxDecoration(
        color: isHighlighted
            ? context.primary.withOpacity(0.08)
            : _surfaceColor(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: isHighlighted ? context.primary : pColor.withOpacity(0.25),
          width: isHighlighted ? 2 : 1,
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          children: [
            Row(
              children: [
                // Avatar circle
                Container(
                  width: 42,
                  height: 42,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: pColor,
                  ),
                  child: Center(
                    child: Text(
                      shortId,
                      style: const TextStyle(
                        color: Colors.black87,
                        fontWeight: FontWeight.w800,
                        fontSize: 15,
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                // ID + pickup → drop time
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        id,
                        style: TextStyle(
                          color: _textPrimary(context),
                          fontWeight: FontWeight.w700,
                          fontSize: 15,
                        ),
                      ),
                      const SizedBox(height: 2),
                      Row(
                        children: [
                          Icon(
                            Icons.schedule_rounded,
                            color: _textTertiary(context),
                            size: 13,
                          ),
                          const SizedBox(width: 4),
                          Text(
                            '$pickup  →  $drop',
                            style: TextStyle(
                              color: _textSecondary(context),
                              fontSize: 12,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                // Priority badge
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 10,
                    vertical: 4,
                  ),
                  decoration: BoxDecoration(
                    color: pColor.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(color: pColor.withOpacity(0.4)),
                  ),
                  child: Text(
                    _priorityLabel(priority),
                    style: TextStyle(
                      color: pColor,
                      fontSize: 10,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 0.8,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 10),
            // Preference chips + baseline cost
            Row(
              children: [
                _InfoChip(
                  icon: Icons.directions_car_outlined,
                  label: vPref.toUpperCase(),
                  color: vPref == 'premium'
                      ? context.primary
                      : _textSecondary(context),
                ),
                const SizedBox(width: 8),
                _InfoChip(
                  icon: Icons.people_outline_rounded,
                  label: sPref.toUpperCase(),
                  color: _textSecondary(context),
                ),
                const Spacer(),
                if (baselineCost != null)
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(
                        Icons.currency_rupee_rounded,
                        color: context.primary,
                        size: 14,
                      ),
                      Text(
                        '${(baselineCost as num).toStringAsFixed(0)}',
                        style: TextStyle(
                          color: context.primary,
                          fontWeight: FontWeight.w600,
                          fontSize: 13,
                        ),
                      ),
                    ],
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

/// Card for a single vehicle in the input preview list.
class VehicleCard extends StatelessWidget {
  final Map<String, dynamic> veh;
  final String? highlightedId;
  final Map<String, GlobalKey> cardKeys;

  const VehicleCard({
    super.key,
    required this.veh,
    this.highlightedId,
    required this.cardKeys,
  });

  @override
  Widget build(BuildContext context) {
    final id = veh['vehicle_id']?.toString() ?? '';
    final category = veh['category']?.toString() ?? 'normal';
    final isPremium = category.toLowerCase() == 'premium';
    final capacity = veh['capacity']?.toString() ?? '-';
    final costKm = veh['cost_per_km'];
    final speed = veh['avg_speed_kmph'];
    final availFrom = veh['available_from']?.toString() ?? '--';
    final isHighlighted = highlightedId == id;

    final accentColor = isPremium
        ? context.primary
        : (_isDark(context) ? AppColors.markerNormal : const Color(0xFF6E6E6E));

    return AnimatedContainer(
      key: cardKeys.putIfAbsent(id, () => GlobalKey()),
      duration: const Duration(milliseconds: 400),
      margin: const EdgeInsets.only(bottom: 10),
      decoration: BoxDecoration(
        color: isHighlighted
            ? context.primary.withOpacity(0.08)
            : _surfaceColor(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: isHighlighted
              ? context.primary
              : accentColor.withOpacity(0.25),
          width: isHighlighted ? 2 : 1,
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          children: [
            Row(
              children: [
                Container(
                  width: 42,
                  height: 42,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: accentColor.withOpacity(0.15),
                  ),
                  child: Icon(
                    isPremium
                        ? Icons.directions_car_filled_rounded
                        : Icons.directions_car_rounded,
                    color: accentColor,
                    size: 22,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        id,
                        style: TextStyle(
                          color: _textPrimary(context),
                          fontWeight: FontWeight.w700,
                          fontSize: 15,
                        ),
                      ),
                      const SizedBox(height: 2),
                      Row(
                        children: [
                          Icon(
                            Icons.schedule_rounded,
                            color: _textTertiary(context),
                            size: 13,
                          ),
                          const SizedBox(width: 4),
                          Text(
                            'Available from $availFrom',
                            style: TextStyle(
                              color: _textSecondary(context),
                              fontSize: 12,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                // Category badge
                Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 10,
                    vertical: 4,
                  ),
                  decoration: BoxDecoration(
                    color: accentColor.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(20),
                    border: Border.all(color: accentColor.withOpacity(0.4)),
                  ),
                  child: Text(
                    category.toUpperCase(),
                    style: TextStyle(
                      color: accentColor,
                      fontSize: 10,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 0.8,
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            // Stats row: seats, cost/km, speed
            Row(
              children: [
                _VehicleStat(
                  icon: Icons.event_seat_rounded,
                  text: '$capacity Seats',
                  color: _isDark(context) ? Colors.white70 : Colors.black54,
                ),
                const SizedBox(width: 16),
                if (costKm != null)
                  _VehicleStat(
                    icon: Icons.currency_rupee_rounded,
                    text: '${(costKm as num).toStringAsFixed(1)}/km',
                    color: context.primary,
                  ),
                const Spacer(),
                if (speed != null)
                  _VehicleStat(
                    icon: Icons.speed_rounded,
                    text: '${(speed as num).toStringAsFixed(0)} km/h',
                    color: _textTertiary(context),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

// Small labeled icon chip used in EmployeeCard preferences row.
class _InfoChip extends StatelessWidget {
  final IconData icon;
  final String label;
  final Color color;

  const _InfoChip({
    required this.icon,
    required this.label,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      decoration: BoxDecoration(
        color: color.withOpacity(0.08),
        borderRadius: BorderRadius.circular(6),
        border: Border.all(color: color.withOpacity(0.2)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, color: color, size: 13),
          const SizedBox(width: 4),
          Text(
            label,
            style: TextStyle(
              color: color,
              fontSize: 10,
              fontWeight: FontWeight.w600,
              letterSpacing: 0.5,
            ),
          ),
        ],
      ),
    );
  }
}

// Icon + text stat row used in VehicleCard.
class _VehicleStat extends StatelessWidget {
  final IconData icon;
  final String text;
  final Color color;

  const _VehicleStat({
    required this.icon,
    required this.text,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, color: color, size: 15),
        const SizedBox(width: 4),
        Text(
          text,
          style: TextStyle(
            color: color,
            fontSize: 12,
            fontWeight: FontWeight.w500,
          ),
        ),
      ],
    );
  }
}
