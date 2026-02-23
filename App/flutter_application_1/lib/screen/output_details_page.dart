import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';

import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/services/file_export_service.dart';
import 'package:flutter_application_1/widgets/download_dialog.dart';

// ─────────────────────────────────────────────────────────────
//  OutputDetailsPage — Analytics, Violations & Charts
// ─────────────────────────────────────────────────────────────

class OutputDetailsPage extends StatefulWidget {
  final String testCaseId;
  final String testCaseName;

  /// Pre-loaded result data (passed from OutputPage).
  /// If null, the page fetches from Supabase.
  final Map<String, dynamic>? resultData;

  const OutputDetailsPage({
    super.key,
    required this.testCaseId,
    required this.testCaseName,
    this.resultData,
  });

  @override
  State<OutputDetailsPage> createState() => _OutputDetailsPageState();
}

class _OutputDetailsPageState extends State<OutputDetailsPage>
    with SingleTickerProviderStateMixin {
  // ── Services ──
  final DataService _dataService = DataService();
  final FileExportService _fileExportService = FileExportService();

  // ── State ──
  Map<String, dynamic>? _data;
  bool _isLoading = true;
  String? _error;
  bool _isExporting = false;

  // ── Tabs ──
  late TabController _tabController;
  static const List<String> _tabLabels = ['Overview', 'Violations', 'Charts'];

  // ── Derived metrics (computed once after data loads) ──
  _DerivedMetrics? _metrics;

  // ── Theme helpers ──
  bool _isDark(BuildContext ctx) => Theme.of(ctx).brightness == Brightness.dark;
  Color _bg(BuildContext ctx) =>
      _isDark(ctx) ? AppColors.darkBackground : AppColors.lightBackground;
  Color _surface(BuildContext ctx) =>
      _isDark(ctx) ? AppColors.darkSurface : AppColors.lightSurface;
  Color _textPrimary(BuildContext ctx) =>
      _isDark(ctx) ? Colors.white : AppColors.textPrimaryLight;
  Color _textSecondary(BuildContext ctx) =>
      _isDark(ctx) ? Colors.white54 : AppColors.textSecondaryLight;
  Color _border(BuildContext ctx) =>
      _isDark(ctx) ? AppColors.darkBorderColor : AppColors.borderColor;

  /// Returns a "nice" round interval for chart axes so we always show
  /// ~4 labels without them crowding.  E.g. 63 → 25, 180 → 50, 750 → 200.
  double _niceInterval(double raw) {
    if (raw <= 0) return 1;
    final magnitude = math
        .pow(10, (math.log(raw) / math.ln10).floor())
        .toDouble();
    final residual = raw / magnitude;
    double nice;
    if (residual < 1.5) {
      nice = 1 * magnitude;
    } else if (residual < 3.5) {
      nice = 2 * magnitude;
    } else if (residual < 7.5) {
      nice = 5 * magnitude;
    } else {
      nice = 10 * magnitude;
    }
    return nice;
  }

  // ── Route palette (synced with output_page) ──
  static const List<Color> _palette = [
    Color(0xFF00C569),
    Color(0xFF3B82F6),
    Color(0xFFF59E0B),
    Color(0xFFEF4444),
    Color(0xFF8B5CF6),
    Color(0xFF06B6D4),
    Color(0xFFEC4899),
    Color(0xFFD97706),
  ];

  // ══════════════════════════════════════════════════════════
  //  LIFECYCLE
  // ══════════════════════════════════════════════════════════

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: _tabLabels.length, vsync: this);
    _loadData();
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }

  Future<void> _loadData() async {
    setState(() {
      _isLoading = true;
      _error = null;
    });

    try {
      Map<String, dynamic>? data = widget.resultData;

      // If not passed directly, fetch from Supabase
      if (data == null || data.isEmpty) {
        data = await _dataService.fetchSolution(widget.testCaseId);
      }

      if (data == null || data.isEmpty) {
        if (mounted) {
          setState(() {
            _error = 'No output data available for this test case.';
            _isLoading = false;
          });
        }
        return;
      }

      if (mounted) {
        setState(() {
          _data = data;
          _metrics = _DerivedMetrics.compute(data!);
          _isLoading = false;
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _error = 'Failed to load output data: $e';
          _isLoading = false;
        });
      }
    }
  }

  // ══════════════════════════════════════════════════════════
  //  DOWNLOAD
  // ══════════════════════════════════════════════════════════

  void _openDownloadDialog() {
    showModalBottomSheet(
      context: context,
      backgroundColor: Theme.of(context).scaffoldBackgroundColor,
      useSafeArea: true,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) =>
          DownloadDialog(onSelect: (type) => _handleDownload(type)),
    );
  }

  Future<void> _handleDownload(String type) async {
    if (_data == null) return;
    setState(() => _isExporting = true);
    await Future.delayed(const Duration(milliseconds: 200));

    await _fileExportService.exportFile(
      context,
      type,
      widget.testCaseName,
      _data!,
    );

    if (mounted) setState(() => _isExporting = false);
  }

  // ══════════════════════════════════════════════════════════
  //  BUILD
  // ══════════════════════════════════════════════════════════

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: _bg(context),
      appBar: _buildAppBar(context),
      body: SafeArea(child: _buildBody(context)),
    );
  }

  PreferredSizeWidget _buildAppBar(BuildContext context) {
    final dark = _isDark(context);
    return AppBar(
      backgroundColor: _bg(context),
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      leading: IconButton(
        icon: Icon(
          Icons.arrow_back_ios_new_rounded,
          color: dark ? Colors.white : AppColors.textPrimaryLight,
          size: 20,
        ),
        onPressed: () => Navigator.pop(context),
      ),
      title: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Output Details',
            style: TextStyle(
              color: _textPrimary(context),
              fontSize: 18,
              fontWeight: FontWeight.w700,
            ),
          ),
          Text(
            widget.testCaseName,
            style: TextStyle(
              color: _textSecondary(context),
              fontSize: 12,
              fontWeight: FontWeight.w400,
            ),
          ),
        ],
      ),
      actions: [
        if (_data != null)
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: _appBarButton(
              context,
              icon: Icons.download_rounded,
              label: 'Export',
              onTap: _openDownloadDialog,
            ),
          ),
      ],
      bottom: _data != null
          ? TabBar(
              controller: _tabController,
              indicatorColor: AppColors.primaryBrand,
              labelColor: AppColors.primaryBrand,
              unselectedLabelColor: _textSecondary(context),
              labelStyle: const TextStyle(
                fontSize: 13,
                fontWeight: FontWeight.w600,
              ),
              tabs: _tabLabels.map((l) => Tab(text: l)).toList(),
            )
          : null,
    );
  }

  Widget _appBarButton(
    BuildContext context, {
    required IconData icon,
    required String label,
    required VoidCallback onTap,
  }) {
    return Material(
      color: Colors.transparent,
      child: InkWell(
        borderRadius: BorderRadius.circular(12),
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(12),
            border: Border.all(color: AppColors.primaryBrand.withOpacity(0.5)),
            color: AppColors.primaryBrand.withOpacity(0.08),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(icon, size: 18, color: AppColors.primaryBrand),
              const SizedBox(width: 6),
              Text(
                label,
                style: const TextStyle(
                  color: AppColors.primaryBrand,
                  fontSize: 12,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildBody(BuildContext context) {
    if (_isLoading) {
      return const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(color: AppColors.primaryBrand),
            SizedBox(height: 16),
            Text(
              'Loading output data…',
              style: TextStyle(color: AppColors.textSecondaryLight),
            ),
          ],
        ),
      );
    }

    if (_error != null) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(
                Icons.error_outline_rounded,
                size: 64,
                color: AppColors.error,
              ),
              const SizedBox(height: 16),
              Text(
                _error!,
                textAlign: TextAlign.center,
                style: TextStyle(color: _textSecondary(context), fontSize: 16),
              ),
              const SizedBox(height: 24),
              ElevatedButton.icon(
                onPressed: _loadData,
                icon: const Icon(Icons.refresh_rounded, size: 18),
                label: const Text('Retry'),
                style: ElevatedButton.styleFrom(
                  backgroundColor: AppColors.primaryBrand,
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(30),
                  ),
                ),
              ),
            ],
          ),
        ),
      );
    }

    return Stack(
      children: [
        TabBarView(
          controller: _tabController,
          children: [
            _buildOverviewTab(context),
            _buildViolationsTab(context),
            _buildChartsTab(context),
          ],
        ),
        if (_isExporting)
          Container(
            color: Colors.black38,
            child: const Center(
              child: CircularProgressIndicator(color: AppColors.primaryBrand),
            ),
          ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  TAB 1 — OVERVIEW (Summary Metrics)
  // ══════════════════════════════════════════════════════════

  Widget _buildOverviewTab(BuildContext context) {
    final m = _metrics!;
    return ListView(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
      children: [
        // ── Solution Status Banner ──
        _buildSolutionBanner(context),
        const SizedBox(height: 20),

        // ── Core Metrics Grid ──
        _sectionHeader(
          context,
          icon: Icons.analytics_rounded,
          title: 'Core Metrics',
        ),
        const SizedBox(height: 12),
        _buildMetricsGrid(context, m),
        const SizedBox(height: 24),

        // ── Cost Breakdown ──
        if (m.totalCost > 0) ...[
          _sectionHeader(
            context,
            icon: Icons.attach_money_rounded,
            title: 'Cost Breakdown',
          ),
          const SizedBox(height: 12),
          _buildCostBreakdown(context, m),
          const SizedBox(height: 24),
        ],

        // ── Per-Vehicle Summary Table ──
        _sectionHeader(
          context,
          icon: Icons.directions_car_rounded,
          title: 'Vehicle Summary',
        ),
        const SizedBox(height: 12),
        _buildVehicleTable(context, m),
        const SizedBox(height: 24),

        // ── Quick Violation Summary ──
        _sectionHeader(
          context,
          icon: Icons.warning_amber_rounded,
          title: 'Violation Summary',
        ),
        const SizedBox(height: 12),
        _buildViolationSummaryChips(context, m),
      ],
    );
  }

  Widget _buildSolutionBanner(BuildContext context) {
    final m = _metrics!;
    final isOptimal = m.hardViolations == 0 && m.softViolations == 0;
    final hasHard = m.hardViolations > 0;

    final Color bannerColor = isOptimal
        ? AppColors.primaryBrand
        : hasHard
        ? AppColors.error
        : AppColors.warning;

    final String statusText = isOptimal
        ? 'OPTIMAL — No Violations'
        : hasHard
        ? '${m.hardViolations} Hard, ${m.softViolations} Soft Violation${m.totalViolations > 1 ? 's' : ''}'
        : '${m.softViolations} Soft Violation${m.softViolations > 1 ? 's' : ''}';

    final IconData statusIcon = isOptimal
        ? Icons.check_circle_rounded
        : hasHard
        ? Icons.error_rounded
        : Icons.warning_rounded;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: bannerColor.withOpacity(0.1),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: bannerColor.withOpacity(0.3)),
      ),
      child: Row(
        children: [
          Icon(statusIcon, color: bannerColor, size: 28),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Solution Status',
                  style: TextStyle(
                    color: _textSecondary(context),
                    fontSize: 12,
                    fontWeight: FontWeight.w500,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  statusText,
                  style: TextStyle(
                    color: bannerColor,
                    fontSize: 16,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ],
            ),
          ),
          if (m.solutionType.isNotEmpty)
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
              decoration: BoxDecoration(
                color: bannerColor.withOpacity(0.15),
                borderRadius: BorderRadius.circular(20),
              ),
              child: Text(
                m.solutionType.length > 20
                    ? m.solutionType.substring(0, 20)
                    : m.solutionType,
                style: TextStyle(
                  color: bannerColor,
                  fontSize: 10,
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildMetricsGrid(BuildContext context, _DerivedMetrics m) {
    final metrics = [
      _MetricItem(
        Icons.route_rounded,
        'Total Distance',
        '${m.totalDistance.toStringAsFixed(1)} km',
        AppColors.primaryBrand,
      ),
      _MetricItem(
        Icons.directions_car_filled_rounded,
        'Vehicles Used',
        '${m.vehiclesUsed}${m.vehiclesAvailable > 0 ? ' / ${m.vehiclesAvailable}' : ''}',
        const Color(0xFF3B82F6),
      ),
      _MetricItem(
        Icons.people_alt_rounded,
        'Employees Served',
        '${m.employeesServed}',
        const Color(0xFF8B5CF6),
      ),
      _MetricItem(
        Icons.speed_rounded,
        'Avg Utilization',
        '${m.avgUtilization.toStringAsFixed(1)}%',
        const Color(0xFF06B6D4),
      ),
      _MetricItem(
        Icons.timer_rounded,
        'Total Time',
        '${m.totalTime.toStringAsFixed(0)} min',
        const Color(0xFFF59E0B),
      ),
      _MetricItem(
        Icons.money_rounded,
        'Total Cost',
        m.totalCost > 0 ? m.totalCost.toStringAsFixed(1) : 'N/A',
        const Color(0xFFEC4899),
      ),
      _MetricItem(
        Icons.inventory_2_rounded,
        'Max Load',
        '${m.maxLoad} pax',
        const Color(0xFFD97706),
      ),
      _MetricItem(
        Icons.trending_up_rounded,
        'Route Efficiency',
        '${m.routeEfficiency.toStringAsFixed(1)}%',
        AppColors.primaryBrand,
      ),
    ];

    return LayoutBuilder(
      builder: (ctx, constraints) {
        final crossCount = constraints.maxWidth > 600 ? 4 : 2;
        return GridView.builder(
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: crossCount,
            mainAxisSpacing: 12,
            crossAxisSpacing: 12,
            // Fixed height per tile — avoids overflow regardless of screen size
            mainAxisExtent: 100,
          ),
          itemCount: metrics.length,
          itemBuilder: (ctx, i) => _buildMetricCard(ctx, metrics[i]),
        );
      },
    );
  }

  Widget _buildMetricCard(BuildContext context, _MetricItem item) {
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 12),
      decoration: BoxDecoration(
        color: _surface(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: _border(context).withOpacity(0.5)),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(_isDark(context) ? 0.3 : 0.05),
            blurRadius: 8,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Container(
            padding: const EdgeInsets.all(6),
            decoration: BoxDecoration(
              color: item.color.withOpacity(0.12),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Icon(item.icon, size: 15, color: item.color),
          ),
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(
                item.value,
                style: TextStyle(
                  color: _textPrimary(context),
                  fontSize: 17,
                  fontWeight: FontWeight.w800,
                  height: 1.1,
                ),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              const SizedBox(height: 2),
              Text(
                item.label,
                style: TextStyle(
                  color: _textSecondary(context),
                  fontSize: 10,
                  fontWeight: FontWeight.w500,
                ),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildCostBreakdown(BuildContext context, _DerivedMetrics m) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: _surface(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: _border(context).withOpacity(0.5)),
      ),
      child: Column(
        children: [
          _costRow(
            context,
            'Total Cost',
            m.totalCost.toStringAsFixed(2),
            _textPrimary(context),
            true,
          ),
          if (m.baselineCost > 0) ...[
            const Divider(height: 20),
            _costRow(
              context,
              'Baseline Cost',
              m.baselineCost.toStringAsFixed(2),
              _textSecondary(context),
              false,
            ),
            const SizedBox(height: 8),
            _costRow(
              context,
              'Savings',
              '${m.costSavings.toStringAsFixed(2)} (${m.costSavingsPercent.toStringAsFixed(1)}%)',
              m.costSavings > 0 ? AppColors.primaryBrand : AppColors.error,
              true,
            ),
          ],
        ],
      ),
    );
  }

  Widget _costRow(
    BuildContext context,
    String label,
    String value,
    Color valueColor,
    bool bold,
  ) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceBetween,
      children: [
        Text(
          label,
          style: TextStyle(color: _textSecondary(context), fontSize: 14),
        ),
        Text(
          value,
          style: TextStyle(
            color: valueColor,
            fontSize: 14,
            fontWeight: bold ? FontWeight.w700 : FontWeight.w400,
          ),
        ),
      ],
    );
  }

  Widget _buildVehicleTable(BuildContext context, _DerivedMetrics m) {
    if (m.vehicleStats.isEmpty) {
      return _emptyState(context, 'No vehicle data available');
    }

    return Container(
      decoration: BoxDecoration(
        color: _surface(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: _border(context).withOpacity(0.5)),
      ),
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: DataTable(
          headingRowColor: WidgetStateProperty.all(
            _isDark(context)
                ? Colors.white.withOpacity(0.05)
                : Colors.grey.withOpacity(0.06),
          ),
          columnSpacing: 20,
          columns: const [
            DataColumn(
              label: Text(
                'Vehicle',
                style: TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
              ),
            ),
            DataColumn(
              label: Text(
                'Trips',
                style: TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
              ),
            ),
            DataColumn(
              label: Text(
                'Pax',
                style: TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
              ),
            ),
            DataColumn(
              label: Text(
                'Distance',
                style: TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
              ),
            ),
            DataColumn(
              label: Text(
                'Cost',
                style: TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
              ),
            ),
            DataColumn(
              label: Text(
                'Util %',
                style: TextStyle(fontWeight: FontWeight.w700, fontSize: 12),
              ),
            ),
          ],
          rows: m.vehicleStats.asMap().entries.map((entry) {
            final i = entry.key;
            final v = entry.value;
            final color = _palette[i % _palette.length];
            return DataRow(
              cells: [
                DataCell(
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Container(
                        width: 10,
                        height: 10,
                        decoration: BoxDecoration(
                          color: color,
                          shape: BoxShape.circle,
                        ),
                      ),
                      const SizedBox(width: 8),
                      Text(
                        v.vehicleId,
                        style: TextStyle(
                          color: _textPrimary(context),
                          fontWeight: FontWeight.w600,
                          fontSize: 13,
                        ),
                      ),
                    ],
                  ),
                ),
                DataCell(
                  Text(
                    '${v.tripsCount}',
                    style: TextStyle(
                      color: _textPrimary(context),
                      fontSize: 13,
                    ),
                  ),
                ),
                DataCell(
                  Text(
                    '${v.passengers}',
                    style: TextStyle(
                      color: _textPrimary(context),
                      fontSize: 13,
                    ),
                  ),
                ),
                DataCell(
                  Text(
                    '${v.distance.toStringAsFixed(1)} km',
                    style: TextStyle(
                      color: _textPrimary(context),
                      fontSize: 13,
                    ),
                  ),
                ),
                DataCell(
                  Text(
                    v.cost.toStringAsFixed(1),
                    style: TextStyle(
                      color: _textPrimary(context),
                      fontSize: 13,
                    ),
                  ),
                ),
                DataCell(
                  Text(
                    '${v.utilization.toStringAsFixed(0)}%',
                    style: TextStyle(
                      color: v.utilization >= 70
                          ? AppColors.primaryBrand
                          : v.utilization >= 40
                          ? AppColors.warning
                          : AppColors.error,
                      fontWeight: FontWeight.w600,
                      fontSize: 13,
                    ),
                  ),
                ),
              ],
            );
          }).toList(),
        ),
      ),
    );
  }

  Widget _buildViolationSummaryChips(BuildContext context, _DerivedMetrics m) {
    if (m.totalViolations == 0) {
      return _successBanner(
        context,
        'No constraint violations — solution is clean',
      );
    }

    final vd = m.violationDetails;
    final chips = <Widget>[];

    void addChip(String label, int count, Color color) {
      if (count > 0) {
        chips.add(_violationCountChip(context, label, count, color));
      }
    }

    addChip('Capacity', vd.capacityViolations.length, AppColors.error);
    addChip('Time Window', vd.timeWindowViolations.length, AppColors.error);
    addChip('Unassigned', vd.unassignedEmployees.length, AppColors.error);
    addChip('Vehicle Pref', vd.vehiclePrefViolations.length, AppColors.warning);
    addChip('Sharing Pref', vd.sharingPrefViolations.length, AppColors.warning);

    return Wrap(spacing: 10, runSpacing: 10, children: chips);
  }

  Widget _violationCountChip(
    BuildContext context,
    String label,
    int count,
    Color color,
  ) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
      decoration: BoxDecoration(
        color: color.withOpacity(0.1),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withOpacity(0.3)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.circle, size: 8, color: color),
          const SizedBox(width: 6),
          Text(
            '$label: $count',
            style: TextStyle(
              color: color,
              fontSize: 13,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }

  // ══════════════════════════════════════════════════════════
  //  TAB 2 — VIOLATIONS
  // ══════════════════════════════════════════════════════════

  Widget _buildViolationsTab(BuildContext context) {
    final m = _metrics!;
    final vd = m.violationDetails;

    if (m.totalViolations == 0) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.verified_rounded,
              size: 72,
              color: AppColors.primaryBrand.withOpacity(0.7),
            ),
            const SizedBox(height: 16),
            Text(
              'No Constraint Violations',
              style: TextStyle(
                color: _textPrimary(context),
                fontSize: 20,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              'All constraints are satisfied in this solution.',
              style: TextStyle(color: _textSecondary(context), fontSize: 14),
            ),
          ],
        ),
      );
    }

    return ListView(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
      children: [
        _sectionHeader(
          context,
          icon: Icons.warning_amber_rounded,
          title: 'Constraint Violations',
          subtitle: '${m.totalViolations} total',
        ),
        const SizedBox(height: 16),

        // ── Capacity Violations ──
        if (vd.capacityViolations.isNotEmpty) ...[
          _violationCategory(
            context,
            'Capacity Violations',
            Icons.group_rounded,
            AppColors.error,
          ),
          const SizedBox(height: 8),
          ...vd.capacityViolations.map(
            (v) => _buildCapacityViolationCard(context, v),
          ),
          const SizedBox(height: 20),
        ],

        // ── Time Window Violations ──
        if (vd.timeWindowViolations.isNotEmpty) ...[
          _violationCategory(
            context,
            'Time Window Violations',
            Icons.access_time_rounded,
            AppColors.error,
          ),
          const SizedBox(height: 8),
          ...vd.timeWindowViolations.map(
            (v) => _buildTimeWindowViolationCard(context, v),
          ),
          const SizedBox(height: 20),
        ],

        // ── Unassigned Employees ──
        if (vd.unassignedEmployees.isNotEmpty) ...[
          _violationCategory(
            context,
            'Unassigned Employees',
            Icons.person_off_rounded,
            AppColors.error,
          ),
          const SizedBox(height: 8),
          ...vd.unassignedEmployees.map(
            (v) => _buildUnassignedCard(context, v),
          ),
          const SizedBox(height: 20),
        ],

        // ── Vehicle Preference Violations (soft) ──
        if (vd.vehiclePrefViolations.isNotEmpty) ...[
          _violationCategory(
            context,
            'Vehicle Preference Violations',
            Icons.directions_car_rounded,
            AppColors.warning,
          ),
          const SizedBox(height: 8),
          ...vd.vehiclePrefViolations.map(
            (v) => _buildVehiclePrefViolationCard(context, v),
          ),
          const SizedBox(height: 20),
        ],

        // ── Sharing Preference Violations (soft) ──
        if (vd.sharingPrefViolations.isNotEmpty) ...[
          _violationCategory(
            context,
            'Sharing Preference Violations',
            Icons.share_rounded,
            AppColors.warning,
          ),
          const SizedBox(height: 8),
          ...vd.sharingPrefViolations.map(
            (v) => _buildSharingPrefViolationCard(context, v),
          ),
          const SizedBox(height: 20),
        ],
      ],
    );
  }

  Widget _violationCategory(
    BuildContext context,
    String title,
    IconData icon,
    Color color,
  ) {
    return Row(
      children: [
        Icon(icon, size: 18, color: color),
        const SizedBox(width: 8),
        Text(
          title,
          style: TextStyle(
            color: _textPrimary(context),
            fontSize: 15,
            fontWeight: FontWeight.w700,
          ),
        ),
      ],
    );
  }

  Widget _buildCapacityViolationCard(
    BuildContext context,
    Map<String, dynamic> v,
  ) {
    return _violationCard(
      context,
      severity: 'HARD',
      color: AppColors.error,
      title: 'Capacity Exceeded',
      details: [
        _vRow('Vehicle', v['vehicle']?.toString() ?? 'N/A'),
        _vRow('Trip', '#${v['trip'] ?? '?'}'),
        _vRow('Passengers', '${v['passengers'] ?? '?'}'),
        _vRow('Capacity Limit', '${v['capacity'] ?? '?'}'),
        _vRow('Employees', v['employees']?.toString() ?? 'N/A'),
      ],
      description:
          'Vehicle ${v['vehicle']} on trip ${v['trip']} carries ${v['passengers']} passengers, '
          'exceeding its capacity of ${v['capacity']}.',
    );
  }

  Widget _buildTimeWindowViolationCard(
    BuildContext context,
    Map<String, dynamic> v,
  ) {
    return _violationCard(
      context,
      severity: 'HARD',
      color: AppColors.error,
      title: 'Late Office Arrival',
      details: [
        _vRow('Employee', v['employee']?.toString() ?? 'N/A'),
        _vRow('Vehicle', v['vehicle']?.toString() ?? 'N/A'),
        _vRow('Trip', '#${v['trip'] ?? '?'}'),
        _vRow('Arrived', v['office_arrival']?.toString() ?? 'N/A'),
        _vRow('Deadline', v['deadline']?.toString() ?? 'N/A'),
        _vRow('Delay', '${v['delay_min'] ?? '?'} min'),
      ],
      description:
          'Employee ${v['employee']} arrived at office at ${v['office_arrival']}, '
          'which is ${v['delay_min']} minutes past the ${v['deadline']} deadline.',
    );
  }

  Widget _buildUnassignedCard(BuildContext context, Map<String, dynamic> v) {
    return _violationCard(
      context,
      severity: 'HARD',
      color: AppColors.error,
      title: 'Unassigned Employee',
      details: [_vRow('Employee', v['employee']?.toString() ?? 'N/A')],
      description:
          'Employee ${v['employee']} could not be assigned to any vehicle.',
    );
  }

  Widget _buildVehiclePrefViolationCard(
    BuildContext context,
    Map<String, dynamic> v,
  ) {
    return _violationCard(
      context,
      severity: 'SOFT',
      color: AppColors.warning,
      title: 'Vehicle Type Mismatch',
      details: [
        _vRow('Employee', v['employee']?.toString() ?? 'N/A'),
        _vRow('Vehicle', v['vehicle']?.toString() ?? 'N/A'),
        _vRow('Preferred', v['preferred']?.toString() ?? 'N/A'),
        _vRow('Assigned', v['assigned']?.toString() ?? 'N/A'),
      ],
      description:
          'Employee ${v['employee']} prefers ${v['preferred']} but was assigned '
          'to ${v['vehicle']} (${v['assigned']}).',
    );
  }

  Widget _buildSharingPrefViolationCard(
    BuildContext context,
    Map<String, dynamic> v,
  ) {
    return _violationCard(
      context,
      severity: 'SOFT',
      color: AppColors.warning,
      title: 'Sharing Preference Exceeded',
      details: [
        _vRow('Employee', v['employee']?.toString() ?? 'N/A'),
        _vRow('Vehicle', v['vehicle']?.toString() ?? 'N/A'),
        _vRow('Trip', '#${v['trip'] ?? '?'}'),
        _vRow('Preferred', v['preferred']?.toString() ?? 'N/A'),
        _vRow('Actual Riders', '${v['actual_riders'] ?? '?'}'),
      ],
      description:
          'Employee ${v['employee']} prefers ${v['preferred']} sharing but '
          'trip has ${v['actual_riders']} riders.',
    );
  }

  /// Generic violation card builder.
  Widget _violationCard(
    BuildContext context, {
    required String severity,
    required Color color,
    required String title,
    required List<Widget> details,
    required String description,
  }) {
    return Container(
      margin: const EdgeInsets.only(bottom: 12),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: _surface(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: color.withOpacity(0.3)),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(_isDark(context) ? 0.3 : 0.05),
            blurRadius: 8,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              // Severity badge
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                decoration: BoxDecoration(
                  color: color.withOpacity(0.15),
                  borderRadius: BorderRadius.circular(6),
                ),
                child: Text(
                  severity,
                  style: TextStyle(
                    color: color,
                    fontSize: 10,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 0.8,
                  ),
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: Text(
                  title,
                  style: TextStyle(
                    color: _textPrimary(context),
                    fontSize: 15,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ),
              Icon(Icons.circle, size: 10, color: color),
            ],
          ),
          const SizedBox(height: 12),

          // Key-value details
          Wrap(spacing: 16, runSpacing: 6, children: details),
          const SizedBox(height: 12),

          // Description
          Container(
            padding: const EdgeInsets.all(10),
            decoration: BoxDecoration(
              color: color.withOpacity(0.05),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(Icons.info_outline_rounded, size: 14, color: color),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    description,
                    style: TextStyle(
                      color: _textSecondary(context),
                      fontSize: 12,
                      height: 1.4,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _vRow(String label, String value) {
    return Builder(
      builder: (context) => RichText(
        text: TextSpan(
          children: [
            TextSpan(
              text: '$label: ',
              style: TextStyle(
                color: _textSecondary(context),
                fontSize: 12,
                fontWeight: FontWeight.w500,
              ),
            ),
            TextSpan(
              text: value,
              style: TextStyle(
                color: _textPrimary(context),
                fontSize: 12,
                fontWeight: FontWeight.w700,
              ),
            ),
          ],
        ),
      ),
    );
  }

  // ══════════════════════════════════════════════════════════
  //  TAB 3 — CHARTS
  // ══════════════════════════════════════════════════════════

  Widget _buildChartsTab(BuildContext context) {
    final m = _metrics!;
    if (m.vehicleStats.isEmpty) {
      return Center(child: _emptyState(context, 'No vehicle data to chart'));
    }

    return ListView(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
      children: [
        // ── Vehicle Utilization Bar Chart ──
        _sectionHeader(
          context,
          icon: Icons.bar_chart_rounded,
          title: 'Vehicle Utilization (%)',
        ),
        const SizedBox(height: 12),
        _chartContainer(context, child: _buildUtilizationChart(context, m)),
        const SizedBox(height: 28),

        // ── Distance per Vehicle Bar Chart ──
        _sectionHeader(
          context,
          icon: Icons.stacked_bar_chart_rounded,
          title: 'Distance per Vehicle (km)',
        ),
        const SizedBox(height: 12),
        _chartContainer(context, child: _buildDistanceChart(context, m)),
        const SizedBox(height: 28),

        // ── Load Distribution Pie Chart ──
        _sectionHeader(
          context,
          icon: Icons.pie_chart_rounded,
          title: 'Passenger Load Distribution',
        ),
        const SizedBox(height: 12),
        _chartContainer(
          context,
          height: 280,
          child: _buildLoadPieChart(context, m),
        ),
        const SizedBox(height: 28),

        // ── Violations by Type ──
        if (m.totalViolations > 0) ...[
          _sectionHeader(
            context,
            icon: Icons.warning_rounded,
            title: 'Violations by Type',
          ),
          const SizedBox(height: 12),
          _chartContainer(
            context,
            height: 280,
            child: _buildViolationsPieChart(context, m),
          ),
          const SizedBox(height: 28),
        ],

        // ── Cost per Vehicle Bar Chart ──
        if (m.vehicleStats.any((v) => v.cost > 0)) ...[
          _sectionHeader(
            context,
            icon: Icons.attach_money_rounded,
            title: 'Cost per Vehicle',
          ),
          const SizedBox(height: 12),
          _chartContainer(context, child: _buildCostChart(context, m)),
          const SizedBox(height: 28),
        ],
      ],
    );
  }

  Widget _chartContainer(
    BuildContext context, {
    required Widget child,
    double height = 220,
  }) {
    return Container(
      height: height,
      padding: const EdgeInsets.fromLTRB(8, 16, 16, 8),
      decoration: BoxDecoration(
        color: _surface(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: _border(context).withOpacity(0.5)),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(_isDark(context) ? 0.3 : 0.05),
            blurRadius: 8,
            offset: const Offset(0, 2),
          ),
        ],
      ),
      child: child,
    );
  }

  // ── Utilization Bar Chart ──
  // Utilization can legitimately exceed 100% when a capacity violation exists
  // (i.e. vehicle carried more passengers than its rated capacity).
  // We show the real value and draw a dashed reference line at 100%.
  Widget _buildUtilizationChart(BuildContext context, _DerivedMetrics m) {
    final maxUtil = m.vehicleStats
        .map((v) => v.utilization)
        .fold<double>(0, (a, b) => a > b ? a : b);
    // Always show at least up to 110; if any exceed 100, give 20% headroom
    final chartMax = (maxUtil > 100 ? maxUtil * 1.2 : 110).ceilToDouble();
    // Choose interval so we get ~4-5 labels
    final rawInterval = chartMax / 4;
    final utilInterval = _niceInterval(rawInterval);

    return BarChart(
      BarChartData(
        alignment: BarChartAlignment.spaceAround,
        maxY: chartMax,
        barTouchData: BarTouchData(
          touchTooltipData: BarTouchTooltipData(
            getTooltipItem: (group, groupIdx, rod, rodIdx) {
              final v = m.vehicleStats[group.x.toInt()];
              final label = v.utilization > 100
                  ? '${v.vehicleId}\n${v.utilization.toStringAsFixed(1)}%  ⚠ over capacity'
                  : '${v.vehicleId}\n${v.utilization.toStringAsFixed(1)}%';
              return BarTooltipItem(
                label,
                TextStyle(
                  color: _textPrimary(context),
                  fontSize: 11,
                  fontWeight: FontWeight.w600,
                ),
              );
            },
          ),
        ),
        extraLinesData: ExtraLinesData(
          horizontalLines: [
            HorizontalLine(
              y: 100,
              color: AppColors.warning.withOpacity(0.7),
              strokeWidth: 1.5,
              dashArray: [6, 4],
              label: HorizontalLineLabel(
                show: true,
                alignment: Alignment.topRight,
                padding: const EdgeInsets.only(right: 4, bottom: 2),
                style: TextStyle(
                  color: AppColors.warning,
                  fontSize: 9,
                  fontWeight: FontWeight.w700,
                ),
                labelResolver: (_) => '100%',
              ),
            ),
          ],
        ),
        titlesData: FlTitlesData(
          show: true,
          topTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          rightTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              getTitlesWidget: (val, meta) {
                final idx = val.toInt();
                if (idx < 0 || idx >= m.vehicleStats.length) {
                  return const SizedBox.shrink();
                }
                return Padding(
                  padding: const EdgeInsets.only(top: 6),
                  child: Text(
                    m.vehicleStats[idx].vehicleId,
                    style: TextStyle(
                      color: _textSecondary(context),
                      fontSize: 10,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                );
              },
              reservedSize: 28,
            ),
          ),
          leftTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 38,
              interval: utilInterval,
              getTitlesWidget: (val, meta) {
                // Skip labels that don't land on our interval
                if (val % utilInterval != 0) return const SizedBox.shrink();
                return Text(
                  '${val.toInt()}%',
                  style: TextStyle(color: _textSecondary(context), fontSize: 9),
                );
              },
            ),
          ),
        ),
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: utilInterval,
          getDrawingHorizontalLine: (val) =>
              FlLine(color: _border(context).withOpacity(0.3), strokeWidth: 1),
        ),
        borderData: FlBorderData(show: false),
        barGroups: m.vehicleStats.asMap().entries.map((entry) {
          final i = entry.key;
          final v = entry.value;
          // Bars over 100% are shown in red to indicate a capacity violation
          final color = v.utilization > 100
              ? AppColors.error
              : _palette[i % _palette.length];
          return BarChartGroupData(
            x: i,
            barRods: [
              BarChartRodData(
                toY: v.utilization,
                color: color,
                width: m.vehicleStats.length > 6 ? 12 : 20,
                borderRadius: const BorderRadius.vertical(
                  top: Radius.circular(6),
                ),
              ),
            ],
          );
        }).toList(),
      ),
    );
  }

  // ── Distance Bar Chart ──
  Widget _buildDistanceChart(BuildContext context, _DerivedMetrics m) {
    final maxDist = m.vehicleStats
        .map((v) => v.distance)
        .fold<double>(0, (a, b) => a > b ? a : b);
    final chartMax = (maxDist * 1.2).ceilToDouble();
    final distInterval = _niceInterval(chartMax / 4);

    return BarChart(
      BarChartData(
        alignment: BarChartAlignment.spaceAround,
        maxY: chartMax,
        barTouchData: BarTouchData(
          touchTooltipData: BarTouchTooltipData(
            getTooltipItem: (group, groupIdx, rod, rodIdx) {
              final v = m.vehicleStats[group.x.toInt()];
              return BarTooltipItem(
                '${v.vehicleId}\n${v.distance.toStringAsFixed(1)} km',
                TextStyle(
                  color: _textPrimary(context),
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              );
            },
          ),
        ),
        titlesData: FlTitlesData(
          show: true,
          topTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          rightTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              getTitlesWidget: (val, meta) {
                final idx = val.toInt();
                if (idx < 0 || idx >= m.vehicleStats.length) {
                  return const SizedBox.shrink();
                }
                return Padding(
                  padding: const EdgeInsets.only(top: 6),
                  child: Text(
                    m.vehicleStats[idx].vehicleId,
                    style: TextStyle(
                      color: _textSecondary(context),
                      fontSize: 10,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                );
              },
              reservedSize: 28,
            ),
          ),
          leftTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 38,
              interval: distInterval,
              getTitlesWidget: (val, meta) {
                if (val % distInterval != 0) return const SizedBox.shrink();
                return Text(
                  val.toStringAsFixed(0),
                  style: TextStyle(color: _textSecondary(context), fontSize: 9),
                );
              },
            ),
          ),
        ),
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: distInterval,
          getDrawingHorizontalLine: (val) =>
              FlLine(color: _border(context).withOpacity(0.3), strokeWidth: 1),
        ),
        borderData: FlBorderData(show: false),
        barGroups: m.vehicleStats.asMap().entries.map((entry) {
          final i = entry.key;
          final v = entry.value;
          final color = _palette[i % _palette.length];
          return BarChartGroupData(
            x: i,
            barRods: [
              BarChartRodData(
                toY: v.distance,
                color: color,
                width: m.vehicleStats.length > 6 ? 12 : 20,
                borderRadius: const BorderRadius.vertical(
                  top: Radius.circular(6),
                ),
              ),
            ],
          );
        }).toList(),
      ),
    );
  }

  // ── Cost Bar Chart ──
  Widget _buildCostChart(BuildContext context, _DerivedMetrics m) {
    final maxCost = m.vehicleStats
        .map((v) => v.cost)
        .fold<double>(0, (a, b) => a > b ? a : b);
    final chartMax = (maxCost * 1.2).ceilToDouble();
    final costInterval = _niceInterval(chartMax / 4);

    return BarChart(
      BarChartData(
        alignment: BarChartAlignment.spaceAround,
        maxY: chartMax,
        barTouchData: BarTouchData(
          touchTooltipData: BarTouchTooltipData(
            getTooltipItem: (group, groupIdx, rod, rodIdx) {
              final v = m.vehicleStats[group.x.toInt()];
              return BarTooltipItem(
                '${v.vehicleId}\n${v.cost.toStringAsFixed(1)}',
                TextStyle(
                  color: _textPrimary(context),
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              );
            },
          ),
        ),
        titlesData: FlTitlesData(
          show: true,
          topTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          rightTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          bottomTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              getTitlesWidget: (val, meta) {
                final idx = val.toInt();
                if (idx < 0 || idx >= m.vehicleStats.length) {
                  return const SizedBox.shrink();
                }
                return Padding(
                  padding: const EdgeInsets.only(top: 6),
                  child: Text(
                    m.vehicleStats[idx].vehicleId,
                    style: TextStyle(
                      color: _textSecondary(context),
                      fontSize: 10,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                );
              },
              reservedSize: 28,
            ),
          ),
          leftTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 46,
              interval: costInterval,
              getTitlesWidget: (val, meta) {
                if (val % costInterval != 0) return const SizedBox.shrink();
                return Text(
                  val.toStringAsFixed(0),
                  style: TextStyle(color: _textSecondary(context), fontSize: 9),
                );
              },
            ),
          ),
        ),
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: costInterval,
          getDrawingHorizontalLine: (val) =>
              FlLine(color: _border(context).withOpacity(0.3), strokeWidth: 1),
        ),
        borderData: FlBorderData(show: false),
        barGroups: m.vehicleStats.asMap().entries.map((entry) {
          final i = entry.key;
          final v = entry.value;
          final color = _palette[i % _palette.length];
          return BarChartGroupData(
            x: i,
            barRods: [
              BarChartRodData(
                toY: v.cost,
                color: color,
                width: m.vehicleStats.length > 6 ? 12 : 20,
                borderRadius: const BorderRadius.vertical(
                  top: Radius.circular(6),
                ),
              ),
            ],
          );
        }).toList(),
      ),
    );
  }

  // ── Load Distribution Pie Chart ──
  Widget _buildLoadPieChart(BuildContext context, _DerivedMetrics m) {
    final total = m.vehicleStats.fold<int>(0, (s, v) => s + v.passengers);
    if (total == 0) return _emptyState(context, 'No passengers assigned');

    return Row(
      children: [
        Expanded(
          flex: 3,
          child: PieChart(
            PieChartData(
              sectionsSpace: 2,
              centerSpaceRadius: 36,
              sections: m.vehicleStats.asMap().entries.map((entry) {
                final i = entry.key;
                final v = entry.value;
                final pct = (v.passengers / total * 100);
                return PieChartSectionData(
                  color: _palette[i % _palette.length],
                  value: v.passengers.toDouble(),
                  title: pct >= 8 ? '${pct.toStringAsFixed(0)}%' : '',
                  radius: 50,
                  titleStyle: const TextStyle(
                    fontSize: 11,
                    fontWeight: FontWeight.w700,
                    color: Colors.white,
                  ),
                );
              }).toList(),
            ),
          ),
        ),
        const SizedBox(width: 12),
        // Legend
        Expanded(
          flex: 2,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: m.vehicleStats.asMap().entries.map((entry) {
              final i = entry.key;
              final v = entry.value;
              return Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Row(
                  children: [
                    Container(
                      width: 10,
                      height: 10,
                      decoration: BoxDecoration(
                        color: _palette[i % _palette.length],
                        shape: BoxShape.circle,
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        '${v.vehicleId} (${v.passengers})',
                        style: TextStyle(
                          color: _textSecondary(context),
                          fontSize: 11,
                          fontWeight: FontWeight.w500,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
              );
            }).toList(),
          ),
        ),
      ],
    );
  }

  // ── Violations Pie Chart ──
  Widget _buildViolationsPieChart(BuildContext context, _DerivedMetrics m) {
    final vd = m.violationDetails;
    final items = <_PieItem>[];

    void add(String label, int count, Color color) {
      if (count > 0) items.add(_PieItem(label, count, color));
    }

    add('Capacity', vd.capacityViolations.length, AppColors.error);
    add('Time Window', vd.timeWindowViolations.length, const Color(0xFFDC2626));
    add('Unassigned', vd.unassignedEmployees.length, const Color(0xFF991B1B));
    add('Vehicle Pref', vd.vehiclePrefViolations.length, AppColors.warning);
    add(
      'Sharing Pref',
      vd.sharingPrefViolations.length,
      const Color(0xFFD97706),
    );

    if (items.isEmpty) return _emptyState(context, 'No violations');

    final total = items.fold<int>(0, (s, i) => s + i.count);

    return Row(
      children: [
        Expanded(
          flex: 3,
          child: PieChart(
            PieChartData(
              sectionsSpace: 2,
              centerSpaceRadius: 36,
              sections: items.map((item) {
                final pct = (item.count / total * 100);
                return PieChartSectionData(
                  color: item.color,
                  value: item.count.toDouble(),
                  title: pct >= 10 ? '${pct.toStringAsFixed(0)}%' : '',
                  radius: 50,
                  titleStyle: const TextStyle(
                    fontSize: 11,
                    fontWeight: FontWeight.w700,
                    color: Colors.white,
                  ),
                );
              }).toList(),
            ),
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          flex: 2,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: items.map((item) {
              return Padding(
                padding: const EdgeInsets.symmetric(vertical: 3),
                child: Row(
                  children: [
                    Container(
                      width: 10,
                      height: 10,
                      decoration: BoxDecoration(
                        color: item.color,
                        shape: BoxShape.circle,
                      ),
                    ),
                    const SizedBox(width: 8),
                    Expanded(
                      child: Text(
                        '${item.label} (${item.count})',
                        style: TextStyle(
                          color: _textSecondary(context),
                          fontSize: 11,
                          fontWeight: FontWeight.w500,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
              );
            }).toList(),
          ),
        ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  SHARED WIDGETS
  // ══════════════════════════════════════════════════════════

  Widget _sectionHeader(
    BuildContext context, {
    required IconData icon,
    required String title,
    String? subtitle,
  }) {
    return Row(
      children: [
        Icon(icon, size: 20, color: AppColors.primaryBrand),
        const SizedBox(width: 8),
        Text(
          title,
          style: TextStyle(
            color: _textPrimary(context),
            fontSize: 16,
            fontWeight: FontWeight.w700,
          ),
        ),
        if (subtitle != null) ...[
          const SizedBox(width: 8),
          Text(
            subtitle,
            style: TextStyle(color: _textSecondary(context), fontSize: 12),
          ),
        ],
      ],
    );
  }

  Widget _emptyState(BuildContext context, String message) {
    return Container(
      height: 120,
      alignment: Alignment.center,
      child: Text(
        message,
        style: TextStyle(color: _textSecondary(context), fontSize: 14),
      ),
    );
  }

  Widget _successBanner(BuildContext context, String message) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: AppColors.primaryBrand.withOpacity(0.08),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(color: AppColors.primaryBrand.withOpacity(0.3)),
      ),
      child: Row(
        children: [
          const Icon(
            Icons.check_circle_rounded,
            color: AppColors.primaryBrand,
            size: 22,
          ),
          const SizedBox(width: 10),
          Text(
            message,
            style: const TextStyle(
              color: AppColors.primaryBrand,
              fontSize: 14,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }
}

// ═══════════════════════════════════════════════════════════════
//  DATA MODELS — Derived Metrics (computed once from JSON)
// ═══════════════════════════════════════════════════════════════

/// Immutable metrics derived from the raw output JSON.
/// Handles both Format A (solver) and Format B (backend API).
class _DerivedMetrics {
  final double totalDistance;
  final double totalCost;
  final double totalTime;
  final int vehiclesUsed;
  final int vehiclesAvailable;
  final int employeesServed;
  final double avgUtilization;
  final int maxLoad;
  final double avgLoad;
  final double routeEfficiency;
  final int hardViolations;
  final int softViolations;
  final double baselineCost;
  final double costSavings;
  final double costSavingsPercent;
  final String solutionType;
  final List<_VehicleStat> vehicleStats;
  final _ViolationDetails violationDetails;

  int get totalViolations => hardViolations + softViolations;

  const _DerivedMetrics({
    required this.totalDistance,
    required this.totalCost,
    required this.totalTime,
    required this.vehiclesUsed,
    required this.vehiclesAvailable,
    required this.employeesServed,
    required this.avgUtilization,
    required this.maxLoad,
    required this.avgLoad,
    required this.routeEfficiency,
    required this.hardViolations,
    required this.softViolations,
    required this.baselineCost,
    required this.costSavings,
    required this.costSavingsPercent,
    required this.solutionType,
    required this.vehicleStats,
    required this.violationDetails,
  });

  /// Parse + compute metrics from raw output JSON.
  /// Supports both Format A (solver) and Format B (backend API).
  factory _DerivedMetrics.compute(Map<String, dynamic> data) {
    // ── Detect format ──
    final bool isFormatB = data.containsKey('routes') && data['routes'] is List;
    final bool isFormatA =
        data.containsKey('vehicles') && data['vehicles'] is List;

    // ── Result / Stats ──
    final Map<String, dynamic> result =
        (data['result'] as Map<String, dynamic>?) ?? {};
    final Map<String, dynamic> stats =
        (data['stats'] as Map<String, dynamic>?) ?? {};

    // ── Total metrics ──
    final totalCost =
        (result['total_cost'] as num?)?.toDouble() ??
        (stats['cost'] as num?)?.toDouble() ??
        (data['cost'] as num?)?.toDouble() ??
        0.0;
    final totalTime =
        (result['total_time'] as num?)?.toDouble() ??
        (stats['time'] as num?)?.toDouble() ??
        (data['total_time'] as num?)?.toDouble() ??
        0.0;
    final totalDist = (result['total_distance'] as num?)?.toDouble() ?? 0.0;
    final vehiclesUsed = (result['vehicles_used'] as num?)?.toInt() ?? 0;
    final vehiclesAvailable =
        (result['vehicles_available'] as num?)?.toInt() ?? 0;
    final hardViolations =
        (result['hard_violations'] as num?)?.toInt() ??
        (stats['hard_violations'] as num?)?.toInt() ??
        0;
    final softViolations =
        (result['soft_violations'] as num?)?.toInt() ??
        (stats['soft_violations'] as num?)?.toInt() ??
        0;
    final baselineCost = (result['baseline_cost'] as num?)?.toDouble() ?? 0.0;
    final costSavings = (result['cost_savings'] as num?)?.toDouble() ?? 0.0;
    final costSavingsPct =
        (result['cost_savings_percent'] as num?)?.toDouble() ?? 0.0;
    final solutionType = data['solution_type']?.toString() ?? '';

    // ── Build per-vehicle stats ──
    final List<_VehicleStat> vehicleStatsList = [];
    int totalEmployees = 0;
    int maxPax = 0;
    double distSum = 0;

    if (isFormatB) {
      final List routes = data['routes'] ?? [];
      for (int i = 0; i < routes.length; i++) {
        final r = routes[i] as Map<String, dynamic>;
        final vid = r['vehicle_id']?.toString() ?? 'V${i + 1}';
        final dist = (r['total_distance'] as num?)?.toDouble() ?? 0.0;
        final cost = (r['total_cost'] as num?)?.toDouble() ?? 0.0;
        final pax = (r['passengers_count'] as num?)?.toInt() ?? 0;
        final util = (r['capacity_utilization'] as num?)?.toDouble() ?? 0.0;
        final trips = (r['trips_count'] as num?)?.toInt() ?? 1;

        vehicleStatsList.add(
          _VehicleStat(
            vehicleId: vid,
            distance: dist,
            cost: cost,
            passengers: pax,
            utilization: util,
            tripsCount: trips,
          ),
        );
        totalEmployees += pax;
        distSum += dist;
        if (pax > maxPax) maxPax = pax;
      }
    } else if (isFormatA) {
      final List vehicles = data['vehicles'] ?? [];
      for (int i = 0; i < vehicles.length; i++) {
        final v = vehicles[i] as Map<String, dynamic>;
        final vid = v['vehicle_id']?.toString() ?? 'V${i + 1}';
        final dist = (v['total_distance'] as num?)?.toDouble() ?? 0.0;
        final cost = (v['total_cost'] as num?)?.toDouble() ?? 0.0;
        final trips = (v['trips'] as List?)?.length ?? 0;

        // Count unique passengers by scanning pickups
        final Set<String> paxSet = {};
        for (var trip in (v['trips'] as List? ?? [])) {
          for (var stop in (trip['stops'] as List? ?? [])) {
            final loc = stop['location']?.toString() ?? '';
            final match = RegExp(r'(E\d+)').firstMatch(loc);
            if (match != null && loc.contains('Pickup')) {
              paxSet.add(match.group(1)!);
            }
          }
        }

        vehicleStatsList.add(
          _VehicleStat(
            vehicleId: vid,
            distance: dist,
            cost: cost,
            passengers: paxSet.length,
            utilization: 0, // will compute below
            tripsCount: trips,
          ),
        );
        totalEmployees += paxSet.length;
        distSum += dist;
        if (paxSet.length > maxPax) maxPax = paxSet.length;
      }
    }

    // ── Compute average utilization (for Format A fallback) ──
    double avgUtil = 0;
    if (vehicleStatsList.isNotEmpty) {
      if (isFormatB) {
        avgUtil =
            vehicleStatsList.fold<double>(0, (s, v) => s + v.utilization) /
            vehicleStatsList.length;
      } else {
        // Format A: rough estimate — assume capacity 4 per vehicle
        const defaultCap = 4;
        for (var vs in vehicleStatsList) {
          vs._utilization =
              vs.passengers / defaultCap * 100; // may exceed 100 on violations
        }
        avgUtil =
            vehicleStatsList.fold<double>(0, (s, v) => s + v.utilization) /
            vehicleStatsList.length;
      }
    }

    // ── Route Efficiency = (employees / distance) * 10 scaled to % ──
    // Higher is better. Capped at 100.
    double routeEff = 0;
    final effectiveDist = distSum > 0
        ? distSum
        : (totalDist > 0 ? totalDist : 1);
    if (totalEmployees > 0 && effectiveDist > 0) {
      routeEff = ((totalEmployees / effectiveDist) * 10).clamp(0, 100);
    }

    // ── Parse violation details ──
    final vd = _ViolationDetails.parse(data);

    return _DerivedMetrics(
      totalDistance: distSum > 0 ? distSum : totalDist,
      totalCost: totalCost,
      totalTime: totalTime,
      vehiclesUsed: vehicleStatsList.isNotEmpty
          ? vehicleStatsList.length
          : vehiclesUsed,
      vehiclesAvailable: vehiclesAvailable,
      employeesServed: totalEmployees,
      avgUtilization: avgUtil,
      maxLoad: maxPax,
      avgLoad: vehicleStatsList.isNotEmpty
          ? totalEmployees / vehicleStatsList.length
          : 0,
      routeEfficiency: routeEff,
      hardViolations: hardViolations,
      softViolations: softViolations,
      baselineCost: baselineCost,
      costSavings: costSavings,
      costSavingsPercent: costSavingsPct,
      solutionType: solutionType,
      vehicleStats: vehicleStatsList,
      violationDetails: vd,
    );
  }
}

/// Per-vehicle statistics for table and charts.
class _VehicleStat {
  final String vehicleId;
  final double distance;
  final double cost;
  final int passengers;
  double _utilization;
  final int tripsCount;

  double get utilization => _utilization;

  _VehicleStat({
    required this.vehicleId,
    required this.distance,
    required this.cost,
    required this.passengers,
    required double utilization,
    required this.tripsCount,
  }) : _utilization = utilization;
}

/// Parsed violation details — supports Backend Format B and fallback for Format A.
class _ViolationDetails {
  final List<Map<String, dynamic>> capacityViolations;
  final List<Map<String, dynamic>> timeWindowViolations;
  final List<Map<String, dynamic>> unassignedEmployees;
  final List<Map<String, dynamic>> vehiclePrefViolations;
  final List<Map<String, dynamic>> sharingPrefViolations;

  const _ViolationDetails({
    required this.capacityViolations,
    required this.timeWindowViolations,
    required this.unassignedEmployees,
    required this.vehiclePrefViolations,
    required this.sharingPrefViolations,
  });

  /// Parse violation_details from output JSON.
  /// Format B: { violation_details: { capacity_violations: [...], ... } }
  /// Format A: no detailed violations → fallback empty.
  factory _ViolationDetails.parse(Map<String, dynamic> data) {
    final vd = (data['violation_details'] as Map<String, dynamic>?) ?? {};

    List<Map<String, dynamic>> _safeList(dynamic val) {
      if (val is List) {
        return val
            .map((e) => e is Map<String, dynamic> ? e : <String, dynamic>{})
            .toList();
      }
      return [];
    }

    return _ViolationDetails(
      capacityViolations: _safeList(vd['capacity_violations']),
      timeWindowViolations: _safeList(vd['time_window_violations']),
      unassignedEmployees: _safeList(vd['unassigned_employees']),
      vehiclePrefViolations: _safeList(vd['vehicle_pref_violations']),
      sharingPrefViolations: _safeList(vd['sharing_pref_violations']),
    );
  }
}

/// Small helper for metric card data.
class _MetricItem {
  final IconData icon;
  final String label;
  final String value;
  final Color color;
  const _MetricItem(this.icon, this.label, this.value, this.color);
}

/// Small helper for pie chart slices.
class _PieItem {
  final String label;
  final int count;
  final Color color;
  const _PieItem(this.label, this.count, this.color);
}
