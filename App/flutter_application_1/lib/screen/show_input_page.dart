import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/screen/output_page.dart';
import 'package:flutter_application_1/widgets/map_view.dart';
import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/services/optimization_service.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/widgets/input_page/input_cards.dart';
import 'package:flutter_application_1/widgets/input_page/input_filter_bars.dart';
import 'package:flutter_application_1/widgets/input_page/input_action_bar.dart';

// Solver time limits sent to the backend for each mode
const Map<String, int> _kModeDurations = {
  'quick': 15,
  'standard': 60,
  'advanced': 300,
};

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
  // Controllers
  final ScrollController _scrollController = ScrollController();
  final DraggableScrollableController _sheetController =
      DraggableScrollableController();
  final OptimizationService _optimizationService = OptimizationService();
  final DataService _dataService = DataService();
  late TabController _tabController;
  late TabController _outerTabController; // Data | Map

  // UI state
  bool _showBackToTop = false;
  bool _isLoading = false;
  bool _hasExistingSolution = false;
  int _mobileTabIndex = 0;
  String _optimizationMode = 'quick'; // default to fastest mode
  String? _highlightedId; // employee/vehicle tapped on the map

  // Card keys used to scroll-to on map marker tap
  final Map<String, GlobalKey> _cardKeys = {};

  // Employee filter state
  int? _filterPriority;
  bool? _sortEmpByCost; // true = asc, false = desc

  // Vehicle filter state
  int? _filterMinSeats;
  bool? _sortVehByCostPerKm;
  bool? _sortVehByTime;
  bool? _sortVehBySpeed;
  bool _showEmpFilters = false;
  bool _showVehFilters = false;

  // Parsed input data (fixed after init)
  late final List<Map<String, dynamic>> _employees;
  late final List<Map<String, dynamic>> _vehicles;
  late final Map<String, dynamic> _metadata;
  late final List<Map<String, dynamic>> _baseline;

  // Filter breakpoints computed from the actual data
  late final List<int> _seatBreakpoints;
  late final Set<int> _availablePriorities;

  // --------------- Theme helpers ---------------

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

  // --------------- Lifecycle ---------------

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
    _outerTabController = TabController(length: 2, vsync: this);

    _outerTabController.addListener(() => setState(() {}));
    _tabController.addListener(() {
      if (!_tabController.indexIsChanging) {
        setState(() => _mobileTabIndex = _tabController.index);
      }
    });

    _employees = _filterList(widget.data['employees'], 'employee_id');
    _vehicles = _filterList(widget.data['vehicles'], 'vehicle_id');
    _metadata = (widget.data['metadata'] as Map<String, dynamic>?) ?? {};
    _baseline = _filterList(widget.data['baseline'], 'employee_id');

    _computeFilterBreakpoints();
    _checkExistingSolution();

    _scrollController.addListener(() {
      final show = _scrollController.offset > 300;
      if (show != _showBackToTop) setState(() => _showBackToTop = show);
    });
  }

  void _computeFilterBreakpoints() {
    final seats =
        _vehicles
            .map((v) => int.tryParse(v['capacity']?.toString() ?? '') ?? 0)
            .where((s) => s > 0)
            .toSet()
            .toList()
          ..sort();
    _seatBreakpoints = _pickBreakpoints(seats);

    _availablePriorities = _employees
        .map((e) => int.tryParse(e['priority']?.toString() ?? '') ?? 0)
        .where((p) => p >= 1 && p <= 5)
        .toSet();
  }

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

  // Resolves the city name from metadata, employee fields, or GPS bounding box.
  String _resolveCity() {
    final metaCity = _metadata['city']?.toString().trim() ?? '';
    if (metaCity.isNotEmpty && metaCity.toLowerCase() != 'null') {
      return metaCity;
    }

    for (final emp in _employees) {
      for (final key in ['city', 'drop_city', 'location_city']) {
        final v = emp[key]?.toString().trim() ?? '';
        if (v.isNotEmpty && v.toLowerCase() != 'null') return v;
      }
    }

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

  // --------------- Scroll helpers ---------------

  void _scrollToTop() {
    _scrollController.animateTo(
      0,
      duration: const Duration(milliseconds: 500),
      curve: Curves.easeInOut,
    );
  }

  // Scrolls the list so the card for [id] is visible without touching the PageView.
  void _scrollToId(String id) {
    if (!_scrollController.hasClients) return;
    final key = _cardKeys[id];
    if (key?.currentContext == null) return;
    final renderObj = key!.currentContext!.findRenderObject();
    if (renderObj == null) return;
    _scrollController.position.ensureVisible(
      renderObj,
      alignment: 0.15,
      duration: const Duration(milliseconds: 400),
      curve: Curves.easeOutCubic,
      alignmentPolicy: ScrollPositionAlignmentPolicy.explicit,
    );
  }

  // --------------- Map marker tap handlers ---------------

  void _onEmployeeMarkerTap(Map<String, dynamic> emp) {
    final empId = emp['employee_id']?.toString() ?? '';
    setState(() {
      _mobileTabIndex = 0;
      _tabController.animateTo(0);
      _highlightedId = empId;
    });
    _outerTabController.animateTo(
      0,
      duration: const Duration(milliseconds: 300),
      curve: Curves.easeOutCubic,
    );
    Future.delayed(const Duration(milliseconds: 420), () {
      if (mounted) _scrollToId(empId);
    });
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
    _outerTabController.animateTo(
      0,
      duration: const Duration(milliseconds: 300),
      curve: Curves.easeOutCubic,
    );
    Future.delayed(const Duration(milliseconds: 420), () {
      if (mounted) _scrollToId(vehId);
    });
    Future.delayed(const Duration(seconds: 3), () {
      if (mounted && _highlightedId == vehId) {
        setState(() => _highlightedId = null);
      }
    });
  }

  // --------------- Filtered / sorted lists ---------------

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

  void _clearEmpFilters() => setState(() {
    _filterPriority = null;
    _sortEmpByCost = null;
  });

  void _clearVehFilters() => setState(() {
    _filterMinSeats = null;
    _sortVehByCostPerKm = null;
    _sortVehByTime = null;
    _sortVehBySpeed = null;
  });

  // --------------- Business logic ---------------

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
      Map<String, dynamic>? inputData;

      if (!forceRun && _hasExistingSolution) {
        resultData = await _dataService.fetchSolution(widget.testCaseId);
        if (mounted && resultData != null) {
          AppSnackbar.show(context, message: 'Loaded existing solution');
        }
      }

      if (resultData == null || forceRun) {
        inputData = await _dataService.fetchInputData(widget.testCaseId);
        if (inputData == null) {
          throw Exception(
            'Input data not found. Please re-upload the test case.',
          );
        }

        // Extract optimization parameters from stored metadata so that
        // cost/time weights and priority delays from the Excel file are sent
        // explicitly in the request body (the backend uses body values when
        // present, falling back to server-side state as defaults).
        final meta = (inputData['metadata'] as Map<String, dynamic>?) ?? {};
        final costWeight = (meta['objective_cost_weight'] as num?)?.toDouble();
        final timeWeight = (meta['objective_time_weight'] as num?)?.toDouble();
        Map<int, int>? priorityDelays;
        final delayKeys = {
          1: 'priority_1_max_delay_min',
          2: 'priority_2_max_delay_min',
          3: 'priority_3_max_delay_min',
          4: 'priority_4_max_delay_min',
          5: 'priority_5_max_delay_min',
        };
        final hasDelays = delayKeys.values.any((k) => meta[k] != null);
        if (hasDelays) {
          priorityDelays = {
            for (final e in delayKeys.entries)
              e.key:
                  (meta[e.value] as num?)?.toInt() ??
                  (e.key <= 2
                      ? 5
                      : e.key == 3
                      ? 10
                      : 15),
          };
        }

        // Pass the solver time limit that matches the selected mode
        resultData = await _optimizationService.runOptimization(
          inputData,
          mode: _optimizationMode,
          solverDurationSeconds: _kModeDurations[_optimizationMode],
          costWeight: costWeight,
          timeWeight: timeWeight,
          priorityDelays: priorityDelays,
        );

        await _dataService.saveSolution(widget.testCaseId, resultData);
        if (mounted) {
          AppSnackbar.show(context, message: 'Optimization Completed & Saved!');
          setState(() => _hasExistingSolution = true);
        }
      }

      // Enrich result with vehicle metadata needed for output charts
      inputData ??= await _dataService.fetchInputData(widget.testCaseId);
      if (inputData != null && inputData['vehicles'] != null) {
        resultData['input_vehicles'] = inputData['vehicles'];
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
        final raw = e.toString();
        final msg = raw.contains('No data uploaded yet')
            ? 'Server session expired. Please re-upload the test case to the server.'
            : raw.startsWith('Exception: ')
            ? raw.substring(11)
            : raw;
        AppSnackbar.show(context, message: msg, isError: true);
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  // ============================================================
  //  BUILD
  // ============================================================

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
                      color: context.primary.withOpacity(0.25),
                      borderRadius: BorderRadius.circular(16),
                      border: Border.all(
                        color: context.primary.withOpacity(0.6),
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

  // --------------- AppBar ---------------

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
              color: context.primary,
              icon: Icons.check_circle_outline_rounded,
            ),
          ),
      ],
    );
  }

  // --------------- Mobile layout ---------------

  Widget _buildMobileLayout(BuildContext context, Size size) {
    return Column(
      children: [
        _buildOuterTabBar(context),
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
        _buildActionBar(context),
      ],
    );
  }

  // Top-level Data | Map tab bar (mobile only).
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
              borderSide: BorderSide(color: context.primary, width: 3),
              borderRadius: const BorderRadius.only(
                topLeft: Radius.circular(2),
                topRight: Radius.circular(2),
              ),
            ),
            indicatorSize: TabBarIndicatorSize.tab,
            labelColor: context.primary,
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

  Widget _buildMobileDataTab(BuildContext context, Size size) {
    final isEmpTab = _mobileTabIndex == 0;
    final items = isEmpTab ? _filteredEmployees : _filteredVehicles;
    final hPad = size.width < 360 ? 8.0 : 12.0;

    return Stack(
      children: [
        Column(
          children: [
            Padding(
              padding: EdgeInsets.fromLTRB(hPad, 10, hPad, 0),
              child: _buildMetadataStrip(context),
            ),
            const SizedBox(height: 8),
            _buildSegmentedTabs(context, isMobile: true),
            const SizedBox(height: 4),
            isEmpTab
                ? _buildEmployeeFilterBar(context)
                : _buildVehicleFilterBar(context),
            const SizedBox(height: 4),
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
                              ? EmployeeCard(
                                  emp: item,
                                  baseline: _baseline,
                                  highlightedId: _highlightedId,
                                  cardKeys: _cardKeys,
                                )
                              : VehicleCard(
                                  veh: item,
                                  highlightedId: _highlightedId,
                                  cardKeys: _cardKeys,
                                ),
                      ],
                    ),
            ),
          ],
        ),
        // Back-to-top button floating inside the scroll area
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
                      color: context.primary.withOpacity(0.25),
                      borderRadius: BorderRadius.circular(14),
                      border: Border.all(
                        color: context.primary.withOpacity(0.6),
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

  Widget _buildMobileMapTab(BuildContext context) {
    return MapViewWidget(
      employees: _employees,
      vehicles: _vehicles,
      onEmployeeTap: _onEmployeeMarkerTap,
      onVehicleTap: _onVehicleMarkerTap,
    );
  }

  // --------------- Desktop layout ---------------

  Widget _buildDesktopLayout(BuildContext context) {
    return Row(
      children: [
        // Left 60%: map
        Expanded(
          flex: 6,
          child: MapViewWidget(
            employees: _employees,
            vehicles: _vehicles,
            onEmployeeTap: _onEmployeeMarkerTap,
            onVehicleTap: _onVehicleMarkerTap,
          ),
        ),
        // Right 40%: data panel
        Expanded(
          flex: 4,
          child: Container(
            color: _bgColor(context),
            child: Column(
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(20, 16, 20, 8),
                  child: _buildMetadataStrip(context),
                ),
                _buildSegmentedTabs(context),
                const SizedBox(height: 4),
                Expanded(
                  child: TabBarView(
                    controller: _tabController,
                    children: [
                      _buildEmployeeListDesktop(context),
                      _buildVehicleListDesktop(context),
                    ],
                  ),
                ),
                _buildActionBar(context),
              ],
            ),
          ),
        ),
      ],
    );
  }

  // --------------- Shared building blocks ---------------

  // Summary strip: city badge + employee/vehicle counts.
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
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
            decoration: BoxDecoration(
              color: context.primary.withOpacity(0.12),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Icon(
                  Icons.location_city_rounded,
                  color: context.primary,
                  size: 16,
                ),
                const SizedBox(width: 6),
                Text(
                  _resolveCity(),
                  style: TextStyle(
                    color: context.primary,
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
            context.primary,
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

  // Pill-style Employees / Fleet tab switcher.
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
                  color: context.primary,
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
            color: isSelected ? context.primary : Colors.transparent,
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

  // Bridges EmployeeFilterBar callbacks into setState.
  Widget _buildEmployeeFilterBar(BuildContext context) {
    return EmployeeFilterBar(
      availablePriorities: _availablePriorities,
      filterPriority: _filterPriority,
      sortByCost: _sortEmpByCost,
      isExpanded: _showEmpFilters,
      hasActiveFilters: _hasActiveEmpFilters,
      onToggleExpand: () => setState(() => _showEmpFilters = !_showEmpFilters),
      onPriorityChanged: (p) => setState(() => _filterPriority = p),
      onSortCostChanged: (s) => setState(() => _sortEmpByCost = s),
      onClear: _clearEmpFilters,
    );
  }

  // Bridges VehicleFilterBar callbacks into setState.
  Widget _buildVehicleFilterBar(BuildContext context) {
    return VehicleFilterBar(
      seatBreakpoints: _seatBreakpoints,
      filterMinSeats: _filterMinSeats,
      sortByCostPerKm: _sortVehByCostPerKm,
      sortBySpeed: _sortVehBySpeed,
      sortByTime: _sortVehByTime,
      isExpanded: _showVehFilters,
      hasActiveFilters: _hasActiveVehFilters,
      onToggleExpand: () => setState(() => _showVehFilters = !_showVehFilters),
      onMinSeatsChanged: (s) => setState(() => _filterMinSeats = s),
      onSortCostPerKmChanged: (s) => setState(() => _sortVehByCostPerKm = s),
      onSortSpeedChanged: (s) => setState(() => _sortVehBySpeed = s),
      onSortTimeChanged: (s) => setState(() => _sortVehByTime = s),
      onClear: _clearVehFilters,
    );
  }

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
                  itemBuilder: (ctx, i) => EmployeeCard(
                    emp: items[i],
                    baseline: _baseline,
                    highlightedId: _highlightedId,
                    cardKeys: _cardKeys,
                  ),
                ),
        ),
      ],
    );
  }

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
                  itemBuilder: (ctx, i) => VehicleCard(
                    veh: items[i],
                    highlightedId: _highlightedId,
                    cardKeys: _cardKeys,
                  ),
                ),
        ),
      ],
    );
  }

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

  // Delegates to InputActionBar and bridges mode changes back into state.
  Widget _buildActionBar(BuildContext context) {
    return InputActionBar(
      mode: _optimizationMode,
      isLoading: _isLoading,
      hasExistingSolution: _hasExistingSolution,
      onRunOptimization: () => _handleOptimizationAction(forceRun: true),
      onViewResults: () => _handleOptimizationAction(forceRun: false),
      onModeChanged: (m) => setState(() => _optimizationMode = m),
    );
  }
}

// Badge shown in the app bar when a solution already exists.
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
