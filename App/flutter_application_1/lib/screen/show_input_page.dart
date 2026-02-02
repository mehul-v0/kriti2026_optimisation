import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/widgets/info_list_widget.dart';
import 'package:flutter_application_1/widgets/map_view.dart';
import 'package:flutter_application_1/theme/theme.dart';

class ShowInputPage extends StatefulWidget {
  final String testCaseName;
  final Map<String, dynamic> data;

  const ShowInputPage({
    super.key,
    required this.testCaseName,
    required this.data,
  });

  @override
  State<ShowInputPage> createState() => _ShowInputPageState();
}

class _ShowInputPageState extends State<ShowInputPage> {
  final ScrollController _scrollController = ScrollController();
  bool _showBackToTop = false;

  @override
  void initState() {
    super.initState();
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

  @override
  Widget build(BuildContext context) {
    // 1. Filter lists here too so Map doesn't show ghost markers
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

      // FIXED: Added SafeArea to prevent button from being hidden by Android System Nav
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
            child: ElevatedButton(
              onPressed: () {
                // Future optimization logic
              },
              style: ElevatedButton.styleFrom(
                backgroundColor: AppColors.success,
                minimumSize: const Size(double.infinity, 50),
              ),
              child: const Text(
                "RUN OPTIMIZATION",
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.bold,
                  color: Colors.white,
                ),
              ),
            ),
          ),
        ),
      ),

      body: LoadingOverlay(
        isLoading: false,
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
          // Map Container
          SizedBox(
            height: size.height * 0.45,
            width: double.infinity,
            child: MapViewWidget(employees: emps, vehicles: vehs),
          ),

          // Info Section
          Padding(
            padding: EdgeInsets.all(size.width * 0.04),
            child: InfoListWidget(data: widget.data),
          ),

          // Extra space for FAB and Bottom Bar
          const SizedBox(height: 80),
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
                  const SizedBox(height: 80),
                ],
              ),
            ),
          ),
        ),
      ],
    );
  }
}
