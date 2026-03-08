import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/screen/output_page.dart';
import 'package:flutter_application_1/widgets/map_view.dart';
import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/services/optimization_service.dart';
import 'package:flutter_application_1/services/data_service.dart';

// ─────────────────────────────────────────────────────────────
//  ShowInputPage — Roxio Theme-Aware Redesign (v3)
//  • Filters for Employees & Vehicles
//  • Persistent action bar (outside scroll)
//  • Full-width tab titles
// ─────────────────────────────────────────────────────────────

class ShowInputPage extends StatefulWidget {
  final String testCaseId;
  final String testCaseName;
  final Map<String, dynamic> data;

  const ShowInputPage({
    super.key,
    required this.testCaseId,
    required this.testCaseName,
    required this.data,
  });

  @override
  State<ShowInputPage> createState() => _ShowInputPageState();
}

class _ShowInputPageState extends State<ShowInputPage>
    with TickerProviderStateMixin {
  // ── Controllers & Services ──────────────────────────────
  final ScrollController _scrollController = ScrollController();
  final DraggableScrollableController _sheetController =
      DraggableScrollableController();
  final OptimizationService _optimizationService = OptimizationService();
  final DataService _dataService = DataService();
  late TabController _tabController;
  late TabController _outerTabController; // Data | Map

  // ── State ───────────────────────────────────────────────
  bool _showBackToTop = false;
  bool _isLoading = false;
  bool _hasExistingSolution = false;
  int _mobileTabIndex = 0;
  String _optimizationMode = 'standard'; // quick | standard | advanced
  String? _highlightedId; // employee/vehicle ID to highlight on marker tap

  // ── Card keys for scroll-to ─────────────────────────────
  final Map<String, GlobalKey> _cardKeys = {};

  // ── Filter state — Employee ─────────────────────────────
  int? _filterPriority;
  bool? _sortEmpByCost; // true = asc, false = desc, null = none

  // ── Filter state — Vehicle ──────────────────────────────
  int? _filterMinSeats;
  bool? _sortVehByCostPerKm; // true = asc, false = desc, null = none
  bool? _sortVehByTime; // true = asc, false = desc, null = none
  bool? _sortVehBySpeed; // true = asc, false = desc, null = none
  bool _showEmpFilters = false;
  bool _showVehFilters = false;

  // ── Parsed data (set once in initState) ─────────────────
  late final List<Map<String, dynamic>> _employees;
  late final List<Map<String, dynamic>> _vehicles;
  late final Map<String, dynamic> _metadata;
  late final List<Map<String, dynamic>> _baseline;

  // ── Dynamic filter breakpoints (computed from data) ─────
  late final List<int> _seatBreakpoints;
  late final Set<int> _availablePriorities;

  // ── Priority colour helper ──────────────────────────────
  // All employees share the same silver accent (no per-priority colour)
  Color _priorityColor(BuildContext context, dynamic p) =>
      _isDark(context) ? AppColors.silver : const Color(0xFFCCCCCC);

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

  // ── Theme-aware color helpers ───────────────────────────
  bool _isDark(BuildContext context) =>
      Theme.of(context).brightness == Brightness.dark;

  Color _bgColor(BuildContext context) =>
      _isDark(context) ? AppColors.darkBackground : AppColors.lightBackground;

  Color _surfaceColor(BuildContext context) =>
      _isDark(context) ? AppColors.darkSurface : AppColors.lightSurface;

  Color _textPrimary(BuildContext context) =>
      _isDark(context) ? Colors.white : AppColors.textPrimaryLight;

  Color _textSecondary(BuildContext context) =>
      _isDark(context) ? Colors.white54 : AppColors.textSecondaryLight;

  Color _textTertiary(BuildContext context) =>
      _isDark(context) ? Colors.white38 : Colors.black38;

  Color _borderColorThemed(BuildContext context) =>
      _isDark(context) ? AppColors.darkBorderColor : AppColors.borderColor;

  // ── Lifecycle ───────────────────────────────────────────
  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
    _outerTabController = TabController(length: 2, vsync: this);

    // Rebuild on outer tab change (show/hide FAB etc.)
    _outerTabController.addListener(() => setState(() {}));

    // Sync tab controller with mobile sliver tab index
    _tabController.addListener(() {
      if (!_tabController.indexIsChanging) {
        setState(() => _mobileTabIndex = _tabController.index);
      }
    });

    // Safe parsing — filter null / empty IDs
    _employees = _filterList(widget.data['employees'], 'employee_id');
    _vehicles = _filterList(widget.data['vehicles'], 'vehicle_id');
    _metadata = (widget.data['metadata'] as Map<String, dynamic>?) ?? {};
    _baseline = _filterList(widget.data['baseline'], 'employee_id');

    // Compute dynamic filter breakpoints from actual data
    _computeFilterBreakpoints();

    _checkExistingSolution();

    _scrollController.addListener(() {
      final show = _scrollController.offset > 300;
      if (show != _showBackToTop) setState(() => _showBackToTop = show);
    });
  }

  void _computeFilterBreakpoints() {
    // ── Vehicle seats ──
    final seats =
        _vehicles
            .map((v) => int.tryParse(v['capacity']?.toString() ?? '') ?? 0)
            .where((s) => s > 0)
            .toSet()
            .toList()
          ..sort();
    _seatBreakpoints = _pickBreakpoints(seats);

    // ── Available priorities ──
    _availablePriorities = _employees
        .map((e) => int.tryParse(e['priority']?.toString() ?? '') ?? 0)
        .where((p) => p >= 1 && p <= 5)
        .toSet();
  }

  /// Pick up to 3 meaningful int breakpoints from sorted unique values.
  List<int> _pickBreakpoints(List<int> sorted) {
    if (sorted.isEmpty) return [];
    final unique = sorted.toSet().toList()..sort();
    if (unique.length <= 3) return unique;
    return [unique.first, unique[unique.length ~/ 2], unique.last];
  }

  List<Map<String, dynamic>> _filterList(dynamic raw, String idKey) {
    if (raw is! List) return [];
    return raw.whereType<Map<String, dynamic>>().where((m) {
      final id = m[idKey]?.toString() ?? '';
      return id.isNotEmpty && id.toLowerCase() != 'null';
    }).toList();
  }

  /// Resolve city name for display.
  /// Priority: metadata.city → first employee's drop_city field →
  /// city inferred from drop coordinates → 'Bengaluru' fallback.
  String _resolveCity() {
    // 1. metadata.city
    final metaCity = _metadata['city']?.toString().trim() ?? '';
    if (metaCity.isNotEmpty && metaCity.toLowerCase() != 'null') {
      return metaCity;
    }

    // 2. Any employee with an explicit `city` / `drop_city` field
    for (final emp in _employees) {
      for (final key in ['city', 'drop_city', 'location_city']) {
        final v = emp[key]?.toString().trim() ?? '';
        if (v.isNotEmpty && v.toLowerCase() != 'null') return v;
      }
    }

    // 3. Infer from common drop coordinates (bounding-box check)
    if (_employees.isNotEmpty) {
      final lat = ((_employees.first['drop_lat']) as num?)?.toDouble();
      final lng = ((_employees.first['drop_lng']) as num?)?.toDouble();
      if (lat != null && lng != null) {
        if (lat >= 12.8 && lat <= 13.2 && lng >= 77.4 && lng <= 77.8) {
          return 'Bengaluru';
        }
        if (lat >= 28.4 && lat <= 28.9 && lng >= 76.8 && lng <= 77.4) {
          return 'Delhi';
        }
        if (lat >= 19.0 && lat <= 19.3 && lng >= 72.7 && lng <= 73.1) {
          return 'Mumbai';
        }
        if (lat >= 17.3 && lat <= 17.6 && lng >= 78.3 && lng <= 78.7) {
          return 'Hyderabad';
        }
        if (lat >= 12.8 && lat <= 13.2 && lng >= 80.1 && lng <= 80.4) {
          return 'Chennai';
        }
        if (lat >= 22.4 && lat <= 22.7 && lng >= 88.2 && lng <= 88.5) {
          return 'Kolkata';
        }
      }
    }

    // 4. Final fallback
    return 'Bengaluru';
  }

  @override
  void dispose() {
    _scrollController.dispose();
    _sheetController.dispose();
    _tabController.dispose();
    _outerTabController.dispose();
    super.dispose();
  }

  void _scrollToTop() {
    _scrollController.animateTo(
      0,
      duration: const Duration(milliseconds: 500),
      curve: Curves.easeInOut,
    );
  }

  /// Scroll the list so the card for [id] is visible.
  void _scrollToId(String id) {
    if (!_scrollController.hasClients) return;
    final key = _cardKeys[id];
    if (key?.currentContext == null) return;
    final renderObj = key!.currentContext!.findRenderObject();
    if (renderObj == null) return;
    // Use ScrollPosition.ensureVisible directly — this only scrolls THIS
    // specific position and never walks up to ancestor scrollables (PageView).
    _scrollController.position.ensureVisible(
      renderObj,
      alignment: 0.15,
      duration: const Duration(milliseconds: 400),
      curve: Curves.easeOutCubic,
      alignmentPolicy: ScrollPositionAlignmentPolicy.explicit,
    );
  }

  // ── Marker tap handlers ─────────────────────────────────
  void _onEmployeeMarkerTap(Map<String, dynamic> emp) {
    final empId = emp['employee_id']?.toString() ?? '';
    setState(() {
      _mobileTabIndex = 0;
      _tabController.animateTo(0);
      _highlightedId = empId;
    });
    // Switch to Data tab so the card is visible
    _outerTabController.animateTo(
      0,
      duration: const Duration(milliseconds: 300),
      curve: Curves.easeOutCubic,
    );
    // Scroll to the card after tab + list rebuild
    Future.delayed(const Duration(milliseconds: 420), () {
      if (mounted) _scrollToId(empId);
    });
    // Clear highlight after a few seconds
    Future.delayed(const Duration(seconds: 3), () {
      if (mounted && _highlightedId == empId) {
        setState(() => _highlightedId = null);
      }
    });
  }

  void _onVehicleMarkerTap(Map<String, dynamic> veh) {
    final vehId = veh['vehicle_id']?.toString() ?? '';
    setState(() {
      _mobileTabIndex = 1;
      _tabController.animateTo(1);
      _highlightedId = vehId;
    });
    // Switch to Data tab so the card is visible
    _outerTabController.animateTo(
      0,
      duration: const Duration(milliseconds: 300),
      curve: Curves.easeOutCubic,
    );
    // Scroll to the card after tab + list rebuild
    Future.delayed(const Duration(milliseconds: 420), () {
      if (mounted) _scrollToId(vehId);
    });
    // Clear highlight after a few seconds
    Future.delayed(const Duration(seconds: 3), () {
      if (mounted && _highlightedId == vehId) {
        setState(() => _highlightedId = null);
      }
    });
  }

  // ── Filtered lists (computed) ───────────────────────────
  List<Map<String, dynamic>> get _filteredEmployees {
    var list = List<Map<String, dynamic>>.from(_employees);
    if (_filterPriority != null) {
      list = list.where((e) {
        final p = int.tryParse(e['priority']?.toString() ?? '') ?? 0;
        return p == _filterPriority;
      }).toList();
    }
    if (_sortEmpByCost != null) {
      list.sort((a, b) {
        final blA = _baseline.firstWhere(
          (bl) => bl['employee_id'] == a['employee_id'],
          orElse: () => <String, dynamic>{},
        );
        final blB = _baseline.firstWhere(
          (bl) => bl['employee_id'] == b['employee_id'],
          orElse: () => <String, dynamic>{},
        );
        final cA = (blA['baseline_cost'] as num?)?.toDouble() ?? 0;
        final cB = (blB['baseline_cost'] as num?)?.toDouble() ?? 0;
        return _sortEmpByCost! ? cA.compareTo(cB) : cB.compareTo(cA);
      });
    }
    return list;
  }

  List<Map<String, dynamic>> get _filteredVehicles {
    var list = List<Map<String, dynamic>>.from(_vehicles);
    if (_filterMinSeats != null) {
      list = list.where((v) {
        final s = int.tryParse(v['capacity']?.toString() ?? '') ?? 0;
        return s >= _filterMinSeats!;
      }).toList();
    }
    // Apply sorting (last sort wins if multiple active)
    if (_sortVehByCostPerKm != null) {
      list.sort((a, b) {
        final cA = (a['cost_per_km'] as num?)?.toDouble() ?? 0;
        final cB = (b['cost_per_km'] as num?)?.toDouble() ?? 0;
        return _sortVehByCostPerKm! ? cA.compareTo(cB) : cB.compareTo(cA);
      });
    }
    if (_sortVehByTime != null) {
      list.sort((a, b) {
        final tA = a['available_from']?.toString() ?? '';
        final tB = b['available_from']?.toString() ?? '';
        return _sortVehByTime! ? tA.compareTo(tB) : tB.compareTo(tA);
      });
    }
    if (_sortVehBySpeed != null) {
      list.sort((a, b) {
        final sA = (a['avg_speed_kmph'] as num?)?.toDouble() ?? 0;
        final sB = (b['avg_speed_kmph'] as num?)?.toDouble() ?? 0;
        return _sortVehBySpeed! ? sA.compareTo(sB) : sB.compareTo(sA);
      });
    }
    return list;
  }

  bool get _hasActiveEmpFilters =>
      _filterPriority != null || _sortEmpByCost != null;

  bool get _hasActiveVehFilters =>
      _filterMinSeats != null ||
      _sortVehByCostPerKm != null ||
      _sortVehByTime != null ||
      _sortVehBySpeed != null;

  void _clearEmpFilters() {
    setState(() {
      _filterPriority = null;
      _sortEmpByCost = null;
    });
  }

  void _clearVehFilters() {
    setState(() {
      _filterMinSeats = null;
      _sortVehByCostPerKm = null;
      _sortVehByTime = null;
      _sortVehBySpeed = null;
    });
  }

  // ── Business logic (unchanged) ──────────────────────────
  Future<void> _checkExistingSolution() async {
    final sol = await _dataService.fetchSolution(widget.testCaseId);
    if (sol != null && mounted) {
      setState(() => _hasExistingSolution = true);
    }
  }

  Future<void> _handleOptimizationAction({bool forceRun = false}) async {
    setState(() => _isLoading = true);

    try {
      Map<String, dynamic>? resultData;

      if (!forceRun && _hasExistingSolution) {
        resultData = await _dataService.fetchSolution(widget.testCaseId);
        if (mounted && resultData != null) {
          AppSnackbar.show(context, message: "Loaded existing solution");
        }
      }

      if (resultData == null || forceRun) {
        // Fetch the input JSON data from Supabase
        final inputData = await _dataService.fetchInputData(widget.testCaseId);

        if (inputData == null) {
          throw Exception(
            "Input data not found. Please re-upload the test case.",
          );
        }

        // Run optimization by sending JSON directly to backend
        resultData = await _optimizationService.runOptimization(
          inputData,
          mode: _optimizationMode,
        );
        await _dataService.saveSolution(widget.testCaseId, resultData);
        if (mounted) {
          AppSnackbar.show(context, message: "Optimization Completed & Saved!");
          setState(() => _hasExistingSolution = true);
        }
      }

      if (!mounted) return;

      Navigator.push(
        context,
        MaterialPageRoute(
          builder: (context) => OutputPage(
            testCaseId: widget.testCaseId,
            testCaseName: widget.testCaseName,
            resultData: resultData!,
          ),
        ),
      );
    } catch (e) {
      if (mounted) {
        AppSnackbar.show(context, message: e.toString(), isError: true);
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  // ══════════════════════════════════════════════════════════
  //  BUILD
  // ══════════════════════════════════════════════════════════
  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    final isDesktop = size.width > 800;

    return Scaffold(
      backgroundColor: _bgColor(context),
      appBar: _buildAppBar(context),
      floatingActionButton: (isDesktop && _showBackToTop)
          ? GestureDetector(
              onTap: _scrollToTop,
              child: ClipRRect(
                borderRadius: BorderRadius.circular(16),
                child: BackdropFilter(
                  filter: ImageFilter.blur(sigmaX: 14, sigmaY: 14),
                  child: Container(
                    width: 40,
                    height: 40,
                    decoration: BoxDecoration(
                      color: AppColors.primaryBrand.withOpacity(0.25),
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(
                        color: AppColors.primaryBrand.withOpacity(0.6),
                        width: 1.2,
                      ),
                    ),
                    child: const Icon(
                      Icons.arrow_upward_rounded,
                      color: Colors.white,
                      size: 20,
                    ),
                  ),
                ),
              ),
            )
          : null,
      body: LoadingOverlay(
        isLoading: _isLoading,
        child: isDesktop
            ? _buildDesktopLayout(context)
            : _buildMobileLayout(context, size),
      ),
    );
  }

  // ── AppBar ─────────────────────────────────────────────
  PreferredSizeWidget _buildAppBar(BuildContext context) {
    final dark = _isDark(context);
    return AppBar(
      backgroundColor: _bgColor(context),
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      centerTitle: false,
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
            'Input Preview',
            style: TextStyle(
              color: _textSecondary(context),
              fontSize: 11,
              fontWeight: FontWeight.w500,
              letterSpacing: 0.6,
            ),
          ),
          Text(
            widget.testCaseName,
            style: TextStyle(
              color: _textPrimary(context),
              fontSize: 17,
              fontWeight: FontWeight.w700,
            ),
          ),
        ],
      ),
      actions: [
        if (_hasExistingSolution)
          Padding(
            padding: const EdgeInsets.only(right: 8),
            child: _GlowChip(
              label: 'Solution Available',
              color: AppColors.primaryBrand,
              icon: Icons.check_circle_outline_rounded,
            ),
          ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  MOBILE LAYOUT  — Data / Map top-level tabs
  // ══════════════════════════════════════════════════════════
  Widget _buildMobileLayout(BuildContext context, Size size) {
    return Column(
      children: [
        // ── Top-level Data | Map tab bar ──
        _buildOuterTabBar(context),

        // ── Tab content ──
        Expanded(
          child: TabBarView(
            controller: _outerTabController,
            physics: const NeverScrollableScrollPhysics(),
            children: [
              _buildMobileDataTab(context, size),
              _buildMobileMapTab(context),
            ],
          ),
        ),

        // ── Persistent action bar ──
        _buildActionBar(context),
      ],
    );
  }

  // ── Outer tab bar (Data | Map) ─────────────────────────
  Widget _buildOuterTabBar(BuildContext context) {
    final dark = _isDark(context);
    final size = MediaQuery.of(context).size;
    return Container(
      color: _bgColor(context),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          TabBar(
            controller: _outerTabController,
            indicator: UnderlineTabIndicator(
              borderSide: const BorderSide(
                color: AppColors.primaryBrand,
                width: 3,
              ),
              borderRadius: const BorderRadius.only(
                topLeft: Radius.circular(2),
                topRight: Radius.circular(2),
              ),
            ),
            indicatorSize: TabBarIndicatorSize.tab,
            labelColor: AppColors.primaryBrand,
            unselectedLabelColor: dark
                ? Colors.white54
                : AppColors.textSecondaryLight,
            labelStyle: TextStyle(
              fontSize: size.width < 360 ? 12 : 14,
              fontWeight: FontWeight.w600,
            ),
            unselectedLabelStyle: TextStyle(
              fontSize: size.width < 360 ? 12 : 14,
              fontWeight: FontWeight.w500,
            ),
            tabs: const [
              Tab(
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.list_alt_rounded, size: 18),
                    SizedBox(width: 6),
                    Text('Data'),
                  ],
                ),
              ),
              Tab(
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.map_rounded, size: 18),
                    SizedBox(width: 6),
                    Text('Map'),
                  ],
                ),
              ),
            ],
          ),
          Divider(
            height: 1,
            thickness: 1,
            color: _borderColorThemed(context).withOpacity(0.4),
          ),
        ],
      ),
    );
  }

  // ── Mobile Data tab ────────────────────────────────────
  Widget _buildMobileDataTab(BuildContext context, Size size) {
    final isEmpTab = _mobileTabIndex == 0;
    final items = isEmpTab ? _filteredEmployees : _filteredVehicles;
    final hPad = size.width < 360 ? 8.0 : 12.0;

    return Stack(
      children: [
        Column(
          children: [
            // Metadata strip
            Padding(
              padding: EdgeInsets.fromLTRB(hPad, 10, hPad, 0),
              child: _buildMetadataStrip(context),
            ),
            const SizedBox(height: 8),

            // Employees / Vehicles segmented tabs
            _buildSegmentedTabs(context, isMobile: true),
            const SizedBox(height: 4),

            // Filter bar
            isEmpTab
                ? _buildEmployeeFilterBar(context)
                : _buildVehicleFilterBar(context),
            const SizedBox(height: 4),

            // List
            Expanded(
              child: items.isEmpty
                  ? _emptyState(
                      context,
                      isEmpTab
                          ? 'No employees match filters'
                          : 'No vehicles match filters',
                    )
                  : ListView(
                      controller: _scrollController,
                      padding: EdgeInsets.fromLTRB(hPad, 4, hPad, 16),
                      children: [
                        for (final item in items)
                          isEmpTab
                              ? _buildEmployeeCard(context, item)
                              : _buildVehicleCard(context, item),
                      ],
                    ),
            ),
          ],
        ),

        // ── Back-to-top FAB inside the data tab ──
        if (_showBackToTop)
          Positioned(
            right: 12,
            bottom: 12,
            child: GestureDetector(
              onTap: _scrollToTop,
              child: ClipRRect(
                borderRadius: BorderRadius.circular(14),
                child: BackdropFilter(
                  filter: ImageFilter.blur(sigmaX: 14, sigmaY: 14),
                  child: Container(
                    width: 36,
                    height: 36,
                    decoration: BoxDecoration(
                      color: AppColors.primaryBrand.withOpacity(0.25),
                      borderRadius: BorderRadius.circular(14),
                      border: Border.all(
                        color: AppColors.primaryBrand.withOpacity(0.6),
                        width: 1.2,
                      ),
                    ),
                    child: const Icon(
                      Icons.arrow_upward_rounded,
                      color: Colors.white,
                      size: 18,
                    ),
                  ),
                ),
              ),
            ),
          ),
      ],
    );
  }

  // ── Mobile Map tab ─────────────────────────────────────
  Widget _buildMobileMapTab(BuildContext context) {
    return MapViewWidget(
      employees: _employees,
      vehicles: _vehicles,
      onEmployeeTap: _onEmployeeMarkerTap,
      onVehicleTap: _onVehicleMarkerTap,
    );
  }

  // ══════════════════════════════════════════════════════════
  //  DESKTOP LAYOUT
  // ══════════════════════════════════════════════════════════
  Widget _buildDesktopLayout(BuildContext context) {
    return Row(
      children: [
        // ── Left 60 %: Map ──
        Expanded(
          flex: 6,
          child: MapViewWidget(
            employees: _employees,
            vehicles: _vehicles,
            onEmployeeTap: _onEmployeeMarkerTap,
            onVehicleTap: _onVehicleMarkerTap,
          ),
        ),

        // ── Right 40 %: Data panel ──
        Expanded(
          flex: 4,
          child: Container(
            color: _bgColor(context),
            child: Column(
              children: [
                // Metadata
                Padding(
                  padding: const EdgeInsets.fromLTRB(20, 16, 20, 8),
                  child: _buildMetadataStrip(context),
                ),

                // Tabs
                _buildSegmentedTabs(context),
                const SizedBox(height: 4),

                // List
                Expanded(
                  child: TabBarView(
                    controller: _tabController,
                    children: [
                      _buildEmployeeListDesktop(context),
                      _buildVehicleListDesktop(context),
                    ],
                  ),
                ),

                // Action bar
                _buildActionBar(context),
              ],
            ),
          ),
        ),
      ],
    );
  }

  // ══════════════════════════════════════════════════════════
  //  SHARED BUILDING BLOCKS
  // ══════════════════════════════════════════════════════════

  // ── Metadata strip (city, counts) ──────────────────────
  Widget _buildMetadataStrip(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: _surfaceColor(context),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: _borderColorThemed(context), width: 0.5),
      ),
      child: Row(
        children: [
          // City badge
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
            decoration: BoxDecoration(
              color: AppColors.primaryBrand.withOpacity(0.12),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(
                  Icons.location_city_rounded,
                  color: AppColors.primaryBrand,
                  size: 16,
                ),
                const SizedBox(width: 6),
                Text(
                  _resolveCity(),
                  style: const TextStyle(
                    color: AppColors.primaryBrand,
                    fontWeight: FontWeight.w600,
                    fontSize: 13,
                  ),
                ),
              ],
            ),
          ),
          const Spacer(),
          _miniStat(
            Icons.groups_rounded,
            '${_employees.length}',
            AppColors.markerEmployee,
          ),
          const SizedBox(width: 14),
          _miniStat(
            Icons.directions_car_rounded,
            '${_vehicles.length}',
            AppColors.markerPremium,
          ),
        ],
      ),
    );
  }

  Widget _miniStat(IconData icon, String label, Color color) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(icon, color: color, size: 16),
        const SizedBox(width: 4),
        Text(
          label,
          style: TextStyle(
            color: color,
            fontWeight: FontWeight.w600,
            fontSize: 13,
          ),
        ),
      ],
    );
  }

  // ── Segmented tab bar — full width ─────────────────────
  Widget _buildSegmentedTabs(BuildContext context, {bool isMobile = false}) {
    final dark = _isDark(context);
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Container(
        height: 46,
        decoration: BoxDecoration(
          color: _surfaceColor(context),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(
            color: _borderColorThemed(context).withOpacity(0.3),
          ),
        ),
        child: isMobile
            ? Row(
                children: [
                  _mobileTabButton(
                    context,
                    index: 0,
                    icon: Icons.groups_rounded,
                    label: 'Employees (${_filteredEmployees.length})',
                  ),
                  _mobileTabButton(
                    context,
                    index: 1,
                    icon: Icons.directions_car_rounded,
                    label: 'Fleet (${_filteredVehicles.length})',
                  ),
                ],
              )
            : TabBar(
                controller: _tabController,
                indicator: BoxDecoration(
                  color: AppColors.primaryBrand,
                  borderRadius: BorderRadius.circular(10),
                ),
                indicatorSize: TabBarIndicatorSize.tab,
                indicatorPadding: const EdgeInsets.all(3),
                dividerColor: Colors.transparent,
                labelColor: Colors.white,
                unselectedLabelColor: dark
                    ? Colors.white54
                    : AppColors.textSecondaryLight,
                labelStyle: const TextStyle(
                  fontSize: 13,
                  fontWeight: FontWeight.w600,
                ),
                tabs: [
                  Tab(
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        const Icon(Icons.groups_rounded, size: 18),
                        const SizedBox(width: 6),
                        Flexible(
                          child: Text(
                            'Employees (${_filteredEmployees.length})',
                            overflow: TextOverflow.ellipsis,
                          ),
                        ),
                      ],
                    ),
                  ),
                  Tab(
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        const Icon(Icons.directions_car_rounded, size: 18),
                        const SizedBox(width: 6),
                        Flexible(
                          child: Text(
                            'Fleet (${_filteredVehicles.length})',
                            overflow: TextOverflow.ellipsis,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
      ),
    );
  }

  Widget _mobileTabButton(
    BuildContext context, {
    required int index,
    required IconData icon,
    required String label,
  }) {
    final isSelected = _mobileTabIndex == index;
    return Expanded(
      child: GestureDetector(
        onTap: () {
          setState(() => _mobileTabIndex = index);
          _tabController.animateTo(index);
        },
        child: Container(
          margin: const EdgeInsets.all(3),
          decoration: BoxDecoration(
            color: isSelected ? AppColors.primaryBrand : Colors.transparent,
            borderRadius: BorderRadius.circular(10),
          ),
          child: Center(
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(
                  icon,
                  size: 18,
                  color: isSelected ? Colors.white : _textSecondary(context),
                ),
                const SizedBox(width: 6),
                Flexible(
                  child: Text(
                    label,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                      fontSize: 13,
                      fontWeight: FontWeight.w600,
                      color: isSelected
                          ? Colors.white
                          : _textSecondary(context),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  // ══════════════════════════════════════════════════════════
  //  FILTER BARS
  // ══════════════════════════════════════════════════════════

  Widget _buildEmployeeFilterBar(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          InkWell(
            borderRadius: BorderRadius.circular(8),
            onTap: () => setState(() => _showEmpFilters = !_showEmpFilters),
            child: Padding(
              padding: const EdgeInsets.symmetric(vertical: 6),
              child: Row(
                children: [
                  Icon(
                    Icons.filter_list_rounded,
                    size: 16,
                    color: _hasActiveEmpFilters
                        ? AppColors.primaryBrand
                        : _textSecondary(context),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    'Filters',
                    style: TextStyle(
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                      color: _hasActiveEmpFilters
                          ? AppColors.primaryBrand
                          : _textSecondary(context),
                    ),
                  ),
                  if (_hasActiveEmpFilters) ...[
                    const SizedBox(width: 6),
                    Container(
                      width: 6,
                      height: 6,
                      decoration: const BoxDecoration(
                        color: AppColors.primaryBrand,
                        shape: BoxShape.circle,
                      ),
                    ),
                  ],
                  const Spacer(),
                  if (_hasActiveEmpFilters)
                    GestureDetector(
                      onTap: _clearEmpFilters,
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
                    _showEmpFilters
                        ? Icons.expand_less_rounded
                        : Icons.expand_more_rounded,
                    size: 18,
                    color: _textTertiary(context),
                  ),
                ],
              ),
            ),
          ),
          if (_showEmpFilters) ...[
            const SizedBox(height: 4),
            Wrap(
              spacing: 8,
              runSpacing: 6,
              children: [
                // Priority chips — only priorities present in data
                for (int p in [1, 2, 3, 4, 5])
                  if (_availablePriorities.contains(p))
                    _filterChip(
                      context,
                      label: _priorityLabel(p),
                      isActive: _filterPriority == p,
                      activeColor: _priorityColor(context, p),
                      onTap: () => setState(
                        () => _filterPriority = _filterPriority == p ? null : p,
                      ),
                    ),
                // Sort by baseline cost — ascending
                _sortChip(
                  context,
                  label: 'Cost',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Low → High',
                  isActive: _sortEmpByCost == true,
                  activeColor: AppColors.primaryBrand,
                  onTap: () => setState(
                    () => _sortEmpByCost = _sortEmpByCost == true ? null : true,
                  ),
                ),
                // Sort by baseline cost — descending
                _sortChip(
                  context,
                  label: 'Cost',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'High → Low',
                  isActive: _sortEmpByCost == false,
                  activeColor: AppColors.primaryBrand,
                  onTap: () => setState(
                    () =>
                        _sortEmpByCost = _sortEmpByCost == false ? null : false,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
          ],
        ],
      ),
    );
  }

  Widget _buildVehicleFilterBar(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          InkWell(
            borderRadius: BorderRadius.circular(8),
            onTap: () => setState(() => _showVehFilters = !_showVehFilters),
            child: Padding(
              padding: const EdgeInsets.symmetric(vertical: 6),
              child: Row(
                children: [
                  Icon(
                    Icons.filter_list_rounded,
                    size: 16,
                    color: _hasActiveVehFilters
                        ? AppColors.primaryBrand
                        : _textSecondary(context),
                  ),
                  const SizedBox(width: 6),
                  Text(
                    'Filters',
                    style: TextStyle(
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                      color: _hasActiveVehFilters
                          ? AppColors.primaryBrand
                          : _textSecondary(context),
                    ),
                  ),
                  if (_hasActiveVehFilters) ...[
                    const SizedBox(width: 6),
                    Container(
                      width: 6,
                      height: 6,
                      decoration: const BoxDecoration(
                        color: AppColors.primaryBrand,
                        shape: BoxShape.circle,
                      ),
                    ),
                  ],
                  const Spacer(),
                  if (_hasActiveVehFilters)
                    GestureDetector(
                      onTap: _clearVehFilters,
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
                    _showVehFilters
                        ? Icons.expand_less_rounded
                        : Icons.expand_more_rounded,
                    size: 18,
                    color: _textTertiary(context),
                  ),
                ],
              ),
            ),
          ),
          if (_showVehFilters) ...[
            const SizedBox(height: 4),
            Wrap(
              spacing: 8,
              runSpacing: 6,
              children: [
                // Seat chips — data-driven
                for (final s in _seatBreakpoints)
                  _filterChip(
                    context,
                    label: '≥ $s Seats',
                    isActive: _filterMinSeats == s,
                    activeColor: AppColors.primaryBrand,
                    onTap: () => setState(
                      () => _filterMinSeats = _filterMinSeats == s ? null : s,
                    ),
                  ),
                // Sort by cost/km — ascending
                _sortChip(
                  context,
                  label: '₹/km',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Low → High',
                  isActive: _sortVehByCostPerKm == true,
                  activeColor: AppColors.markerPremium,
                  onTap: () => setState(
                    () => _sortVehByCostPerKm = _sortVehByCostPerKm == true
                        ? null
                        : true,
                  ),
                ),
                // Sort by cost/km — descending
                _sortChip(
                  context,
                  label: '₹/km',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'High → Low',
                  isActive: _sortVehByCostPerKm == false,
                  activeColor: AppColors.markerPremium,
                  onTap: () => setState(
                    () => _sortVehByCostPerKm = _sortVehByCostPerKm == false
                        ? null
                        : false,
                  ),
                ),
                // Sort by speed — ascending
                _sortChip(
                  context,
                  label: 'Speed',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Slow → Fast',
                  isActive: _sortVehBySpeed == true,
                  activeColor: AppColors.markerEmployee,
                  onTap: () => setState(
                    () =>
                        _sortVehBySpeed = _sortVehBySpeed == true ? null : true,
                  ),
                ),
                // Sort by speed — descending
                _sortChip(
                  context,
                  label: 'Speed',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'Fast → Slow',
                  isActive: _sortVehBySpeed == false,
                  activeColor: AppColors.markerEmployee,
                  onTap: () => setState(
                    () => _sortVehBySpeed = _sortVehBySpeed == false
                        ? null
                        : false,
                  ),
                ),
                // Sort by availability time — ascending
                _sortChip(
                  context,
                  label: 'Time',
                  icon: Icons.arrow_upward_rounded,
                  tooltip: 'Earliest first',
                  isActive: _sortVehByTime == true,
                  activeColor: AppColors.primaryBrand,
                  onTap: () => setState(
                    () => _sortVehByTime = _sortVehByTime == true ? null : true,
                  ),
                ),
                // Sort by availability time — descending
                _sortChip(
                  context,
                  label: 'Time',
                  icon: Icons.arrow_downward_rounded,
                  tooltip: 'Latest first',
                  isActive: _sortVehByTime == false,
                  activeColor: AppColors.primaryBrand,
                  onTap: () => setState(
                    () =>
                        _sortVehByTime = _sortVehByTime == false ? null : false,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 4),
          ],
        ],
      ),
    );
  }

  Widget _filterChip(
    BuildContext context, {
    required String label,
    required bool isActive,
    required Color activeColor,
    required VoidCallback onTap,
  }) {
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
                : _borderColorThemed(context),
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

  Widget _sortChip(
    BuildContext context, {
    required String label,
    required IconData icon,
    required String tooltip,
    required bool isActive,
    required Color activeColor,
    required VoidCallback onTap,
  }) {
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
                  : _borderColorThemed(context),
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

  // ── Desktop Employee list (with filter) ────────────────
  Widget _buildEmployeeListDesktop(BuildContext context) {
    final items = _filteredEmployees;
    return Column(
      children: [
        _buildEmployeeFilterBar(context),
        Expanded(
          child: items.isEmpty
              ? _emptyState(context, 'No employees match filters')
              : ListView.builder(
                  controller: _scrollController,
                  padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
                  itemCount: items.length,
                  itemBuilder: (ctx, i) =>
                      _buildEmployeeCard(context, items[i]),
                ),
        ),
      ],
    );
  }

  // ── Desktop Vehicle list (with filter) ─────────────────
  Widget _buildVehicleListDesktop(BuildContext context) {
    final items = _filteredVehicles;
    return Column(
      children: [
        _buildVehicleFilterBar(context),
        Expanded(
          child: items.isEmpty
              ? _emptyState(context, 'No vehicles match filters')
              : ListView.builder(
                  controller: _scrollController,
                  padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
                  itemCount: items.length,
                  itemBuilder: (ctx, i) => _buildVehicleCard(context, items[i]),
                ),
        ),
      ],
    );
  }

  // ── Employee card ──────────────────────────────────────
  Widget _buildEmployeeCard(BuildContext context, Map<String, dynamic> emp) {
    final id = emp['employee_id']?.toString() ?? '';
    final shortId = id.length > 1 ? id.substring(1) : id;
    final priority = emp['priority'];
    final pColor = _priorityColor(context, priority);
    final pickup = emp['earliest_pickup']?.toString() ?? '--';
    final drop = emp['latest_drop']?.toString() ?? '--';
    final vPref = emp['vehicle_preference']?.toString() ?? 'any';
    final sPref = emp['sharing_preference']?.toString() ?? '';
    final isHighlighted = _highlightedId == id;

    // find baseline cost for this employee
    final bl = _baseline.firstWhere(
      (b) => b['employee_id'] == id,
      orElse: () => <String, dynamic>{},
    );
    final baselineCost = bl['baseline_cost'];

    return AnimatedContainer(
      key: _cardKeys.putIfAbsent(id, () => GlobalKey()),
      duration: const Duration(milliseconds: 400),
      margin: const EdgeInsets.only(bottom: 10),
      decoration: BoxDecoration(
        color: isHighlighted
            ? AppColors.primaryBrand.withOpacity(0.08)
            : _surfaceColor(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: isHighlighted
              ? AppColors.primaryBrand
              : pColor.withOpacity(0.25),
          width: isHighlighted ? 2 : 1,
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          children: [
            // ── Row 1: Avatar + ID + Priority badge ──
            Row(
              children: [
                // Circular avatar with priority ring
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
                        // Black text is legible on both silver shades
                        color: Colors.black87,
                        fontWeight: FontWeight.w800,
                        fontSize: 15,
                      ),
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                // ID + timing
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

            // ── Row 2: Chips for preferences ──
            Row(
              children: [
                _infoChip(
                  context,
                  Icons.directions_car_outlined,
                  vPref.toUpperCase(),
                  vPref == 'premium'
                      ? AppColors.markerPremium
                      : _textSecondary(context),
                ),
                const SizedBox(width: 8),
                _infoChip(
                  context,
                  Icons.people_outline_rounded,
                  sPref.toUpperCase(),
                  _textSecondary(context),
                ),
                const Spacer(),
                if (baselineCost != null)
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      const Icon(
                        Icons.currency_rupee_rounded,
                        color: AppColors.primaryBrand,
                        size: 14,
                      ),
                      Text(
                        '${(baselineCost as num).toStringAsFixed(0)}',
                        style: const TextStyle(
                          color: AppColors.primaryBrand,
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

  Widget _infoChip(
    BuildContext context,
    IconData icon,
    String label,
    Color color,
  ) {
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

  // ── Vehicle card ───────────────────────────────────────
  Widget _buildVehicleCard(BuildContext context, Map<String, dynamic> veh) {
    final id = veh['vehicle_id']?.toString() ?? '';
    final category = veh['category']?.toString() ?? 'normal';
    final isPremium = category.toLowerCase() == 'premium';
    final capacity = veh['capacity']?.toString() ?? '-';
    final costKm = veh['cost_per_km'];
    final speed = veh['avg_speed_kmph'];
    final availFrom = veh['available_from']?.toString() ?? '--';
    final isHighlighted = _highlightedId == id;

    final accentColor = isPremium
        ? AppColors
              .markerPremium // Petronas teal for premium
        : (_isDark(context)
              ? AppColors.markerNormal
              : const Color(0xFF6E6E6E)); // Silver / grey for normal

    return AnimatedContainer(
      key: _cardKeys.putIfAbsent(id, () => GlobalKey()),
      duration: const Duration(milliseconds: 400),
      margin: const EdgeInsets.only(bottom: 10),
      decoration: BoxDecoration(
        color: isHighlighted
            ? AppColors.primaryBrand.withOpacity(0.08)
            : _surfaceColor(context),
        borderRadius: BorderRadius.circular(14),
        border: Border.all(
          color: isHighlighted
              ? AppColors.primaryBrand
              : accentColor.withOpacity(0.25),
          width: isHighlighted ? 2 : 1,
        ),
      ),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          children: [
            // Row 1: Icon + ID + Category
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

            // Row 2: Stats row
            Row(
              children: [
                _vehicleStat(
                  Icons.event_seat_rounded,
                  '$capacity Seats',
                  _isDark(context) ? Colors.white70 : Colors.black54,
                ),
                const SizedBox(width: 16),
                if (costKm != null)
                  _vehicleStat(
                    Icons.currency_rupee_rounded,
                    '${(costKm as num).toStringAsFixed(1)}/km',
                    AppColors.primaryBrand,
                  ),
                const Spacer(),
                if (speed != null)
                  _vehicleStat(
                    Icons.speed_rounded,
                    '${(speed as num).toStringAsFixed(0)} km/h',
                    _textTertiary(context),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _vehicleStat(IconData icon, String text, Color color) {
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

  // ── Empty state ────────────────────────────────────────
  Widget _emptyState(BuildContext context, String msg) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(40),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.inbox_rounded, color: _textTertiary(context), size: 48),
            const SizedBox(height: 12),
            Text(
              msg,
              style: TextStyle(color: _textSecondary(context), fontSize: 14),
            ),
          ],
        ),
      ),
    );
  }

  // ── Action bar ─────────────────────────────────────────
  static const Map<String, String> _modeLabels = {
    'quick': 'Quick (~15s)',
    'standard': 'Standard (~1m)',
    'advanced': 'Advanced (~5m)',
  };

  static const Map<String, IconData> _modeIcons = {
    'quick': Icons.bolt_rounded,
    'standard': Icons.balance_rounded,
    'advanced': Icons.science_rounded,
  };

  void _showModePicker(BuildContext context) {
    final dark = _isDark(context);
    showModalBottomSheet(
      context: context,
      backgroundColor: _surfaceColor(context),
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (ctx) {
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Center(
                  child: Container(
                    width: 36,
                    height: 4,
                    margin: const EdgeInsets.only(bottom: 16),
                    decoration: BoxDecoration(
                      color: dark ? Colors.white24 : Colors.black12,
                      borderRadius: BorderRadius.circular(2),
                    ),
                  ),
                ),
                Text(
                  'Optimization Mode',
                  style: TextStyle(
                    color: _textPrimary(context),
                    fontSize: 16,
                    fontWeight: FontWeight.w700,
                  ),
                ),
                const SizedBox(height: 12),
                for (final entry in _modeLabels.entries)
                  ListTile(
                    contentPadding: const EdgeInsets.symmetric(horizontal: 4),
                    leading: Container(
                      width: 38,
                      height: 38,
                      decoration: BoxDecoration(
                        color: _optimizationMode == entry.key
                            ? AppColors.primaryBrand.withOpacity(0.15)
                            : (dark ? Colors.white10 : Colors.black12),
                        borderRadius: BorderRadius.circular(10),
                      ),
                      child: Icon(
                        _modeIcons[entry.key]!,
                        color: _optimizationMode == entry.key
                            ? AppColors.primaryBrand
                            : _textSecondary(context),
                        size: 20,
                      ),
                    ),
                    title: Text(
                      entry.value,
                      style: TextStyle(
                        color: _optimizationMode == entry.key
                            ? AppColors.primaryBrand
                            : _textPrimary(context),
                        fontWeight: _optimizationMode == entry.key
                            ? FontWeight.w700
                            : FontWeight.w500,
                        fontSize: 14,
                      ),
                    ),
                    trailing: _optimizationMode == entry.key
                        ? const Icon(
                            Icons.check_rounded,
                            color: AppColors.primaryBrand,
                          )
                        : null,
                    onTap: () {
                      setState(() => _optimizationMode = entry.key);
                      Navigator.pop(ctx);
                    },
                  ),
                const SizedBox(height: 4),
              ],
            ),
          ),
        );
      },
    );
  }

  Widget _buildActionBar(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 6, 12, 10),
      decoration: BoxDecoration(
        color: _bgColor(context),
        border: Border(
          top: BorderSide(color: _borderColorThemed(context).withOpacity(0.4)),
        ),
      ),
      child: SafeArea(
        top: false,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            // ── Dropdown + Buttons in one row ──
            Row(
              children: [
                // ── Compact mode dropdown ──
                GestureDetector(
                  onTap: () => _showModePicker(context),
                  child: Container(
                    padding: const EdgeInsets.all(9),
                    decoration: BoxDecoration(
                      color: _surfaceColor(context),
                      borderRadius: BorderRadius.circular(10),
                      border: Border.all(
                        color: _borderColorThemed(context).withOpacity(0.5),
                      ),
                    ),
                    child: Icon(
                      _modeIcons[_optimizationMode] ?? Icons.balance_rounded,
                      color: AppColors.primaryBrand,
                      size: 22,
                    ),
                  ),
                ),
                const SizedBox(width: 8),

                // ── View Existing Results (only if solution exists) ──
                if (_hasExistingSolution) ...[
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _isLoading
                          ? null
                          : () => _handleOptimizationAction(forceRun: false),
                      icon: const Icon(Icons.visibility_rounded, size: 15),
                      label: const Text('View'),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: AppColors.primaryBrand,
                        side: const BorderSide(color: AppColors.primaryBrand),
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(30),
                        ),
                        minimumSize: const Size(0, 40),
                        padding: const EdgeInsets.symmetric(horizontal: 8),
                        textStyle: const TextStyle(
                          fontSize: 11,
                          fontWeight: FontWeight.w700,
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 6),
                ],

                // ── Run Optimization (primary CTA) ──
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: _isLoading
                        ? null
                        : () => _handleOptimizationAction(forceRun: true),
                    icon: _isLoading
                        ? const SizedBox(
                            width: 14,
                            height: 14,
                            child: CircularProgressIndicator(
                              strokeWidth: 2,
                              color: Colors.white,
                            ),
                          )
                        : const Icon(
                            Icons.route_rounded,
                            size: 16,
                            color: Colors.white,
                          ),
                    label: Text(
                      _hasExistingSolution ? 'Run Again' : 'Run Optimization',
                      style: const TextStyle(
                        fontSize: 12,
                        fontWeight: FontWeight.w700,
                        color: Colors.white,
                      ),
                    ),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.primaryBrand,
                      disabledBackgroundColor: AppColors.primaryBrand
                          .withOpacity(0.5),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(30),
                      ),
                      minimumSize: const Size(0, 40),
                      padding: const EdgeInsets.symmetric(horizontal: 12),
                      elevation: 3,
                      shadowColor: AppColors.primaryBrand.withOpacity(0.4),
                    ),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  Tiny helper widget — animated glow chip for "Solution Available"
// ─────────────────────────────────────────────────────────────
class _GlowChip extends StatelessWidget {
  final String label;
  final Color color;
  final IconData icon;

  const _GlowChip({
    required this.label,
    required this.color,
    required this.icon,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: color.withOpacity(0.12),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withOpacity(0.3)),
        boxShadow: [BoxShadow(color: color.withOpacity(0.15), blurRadius: 8)],
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(icon, color: color, size: 14),
          const SizedBox(width: 4),
          Text(
            label,
            style: TextStyle(
              color: color,
              fontSize: 11,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }
}
