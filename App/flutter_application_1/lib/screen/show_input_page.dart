import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/screen/output_page.dart';
import 'package:flutter_application_1/widgets/info_list_widget.dart';
import 'package:flutter_application_1/widgets/map_view.dart';
import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/services/optimization_service.dart';
import 'package:flutter_application_1/services/data_service.dart';

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

class _ShowInputPageState extends State<ShowInputPage> {
  final ScrollController _scrollController = ScrollController();
  final OptimizationService _optimizationService = OptimizationService();
  final DataService _dataService = DataService();

  bool _showBackToTop = false;
  bool _isLoading = false;
  bool _hasExistingSolution = false;

  @override
  void initState() {
    super.initState();
    _checkExistingSolution();
    _scrollController.addListener(() {
      if (_scrollController.offset > 300 && !_showBackToTop) {
        setState(() => _showBackToTop = true);
      } else if (_scrollController.offset <= 300 && _showBackToTop) {
        setState(() => _showBackToTop = false);
      }
    });
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  void _scrollToTop() {
    _scrollController.animateTo(
      0,
      duration: const Duration(milliseconds: 500),
      curve: Curves.easeInOut,
    );
  }

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
        resultData = await _optimizationService.runOptimization(
          widget.testCaseId,
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
            testCaseName: widget.testCaseName, // <--- PASSING NAME HERE
            resultData: resultData!,
          ),
        ),
      );
    } catch (e) {
      if (mounted)
        AppSnackbar.show(context, message: e.toString(), isError: true);
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    // Filter lists logic
    final rawEmployees = widget.data['employees'] as List? ?? [];
    final employees = rawEmployees.where((e) {
      final id = e['employee_id']?.toString() ?? '';
      return id.isNotEmpty && id.toLowerCase() != 'null';
    }).toList();

    final rawVehicles = widget.data['vehicles'] as List? ?? [];
    final vehicles = rawVehicles.where((v) {
      final id = v['vehicle_id']?.toString() ?? '';
      return id.isNotEmpty && id.toLowerCase() != 'null';
    }).toList();

    final size = MediaQuery.of(context).size;
    final isDesktop = size.width > 800;

    return Scaffold(
      appBar: AppBar(title: Text(widget.testCaseName), elevation: 0),
      floatingActionButton: _showBackToTop
          ? FloatingActionButton(
              onPressed: _scrollToTop,
              mini: true,
              backgroundColor: AppColors.primaryBrand,
              child: const Icon(Icons.arrow_upward, color: Colors.white),
            )
          : null,

      bottomNavigationBar: Container(
        decoration: BoxDecoration(
          color: Theme.of(context).scaffoldBackgroundColor,
          boxShadow: [
            BoxShadow(
              color: Colors.black.withOpacity(0.05),
              offset: const Offset(0, -4),
              blurRadius: 10,
            ),
          ],
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(16.0),
            child: Row(
              children: [
                Expanded(
                  child: OutlinedButton(
                    onPressed: (_isLoading || !_hasExistingSolution)
                        ? null
                        : () => _handleOptimizationAction(forceRun: false),
                    style: OutlinedButton.styleFrom(
                      minimumSize: const Size(double.infinity, 50),
                    ),
                    child: const Text("LOAD OUTPUT"),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: ElevatedButton(
                    onPressed: _isLoading
                        ? null
                        : () => _handleOptimizationAction(forceRun: true),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.success,
                      minimumSize: const Size(double.infinity, 50),
                    ),
                    child: _isLoading
                        ? const SizedBox(
                            height: 24,
                            width: 24,
                            child: CircularProgressIndicator(
                              color: Colors.white,
                              strokeWidth: 2,
                            ),
                          )
                        : const Text(
                            "RUN OPTIMIZATION",
                            style: TextStyle(
                              fontSize: 14,
                              fontWeight: FontWeight.bold,
                              color: Colors.white,
                            ),
                          ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),

      body: LoadingOverlay(
        isLoading: _isLoading,
        child: isDesktop
            ? _buildDesktopLayout(context, employees, vehicles)
            : _buildMobileLayout(context, size, employees, vehicles),
      ),
    );
  }

  Widget _buildMobileLayout(
    BuildContext context,
    Size size,
    List emps,
    List vehs,
  ) {
    return SingleChildScrollView(
      controller: _scrollController,
      child: Column(
        children: [
          SizedBox(
            height: size.height * 0.45,
            width: double.infinity,
            child: MapViewWidget(employees: emps, vehicles: vehs),
          ),
          Padding(
            padding: EdgeInsets.all(size.width * 0.04),
            child: InfoListWidget(data: widget.data),
          ),
          const SizedBox(height: 100),
        ],
      ),
    );
  }

  Widget _buildDesktopLayout(BuildContext context, List emps, List vehs) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Expanded(
          flex: 3,
          child: MapViewWidget(employees: emps, vehicles: vehs),
        ),
        Expanded(
          flex: 2,
          child: Container(
            decoration: const BoxDecoration(
              border: Border(left: BorderSide(color: AppColors.borderColor)),
            ),
            child: SingleChildScrollView(
              controller: _scrollController,
              padding: const EdgeInsets.all(24.0),
              child: Column(
                children: [
                  InfoListWidget(data: widget.data),
                  const SizedBox(height: 100),
                ],
              ),
            ),
          ),
        ),
      ],
    );
  }
}
