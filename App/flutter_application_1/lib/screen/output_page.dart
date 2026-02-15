import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/services/optimization_service.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/services/file_export_service.dart';
import 'package:flutter_application_1/widgets/download_dialog.dart';
import 'package:flutter_application_1/theme/theme.dart';

class OutputPage extends StatefulWidget {
  final String testCaseId;
  final String testCaseName; // <--- ADDED
  final Map<String, dynamic> resultData;

  const OutputPage({
    super.key,
    required this.testCaseId,
    required this.testCaseName, // <--- ADDED
    required this.resultData,
  });

  @override
  State<OutputPage> createState() => _OutputPageState();
}

class _OutputPageState extends State<OutputPage> {
  late Map<String, dynamic> _currentData;
  bool _isLoading = false;

  final OptimizationService _optimizationService = OptimizationService();
  final DataService _dataService = DataService();
  final FileExportService _fileExportService = FileExportService();

  @override
  void initState() {
    super.initState();
    _currentData = widget.resultData;
  }

  Future<void> _handleRetry() async {
    setState(() => _isLoading = true);
    try {
      final newData = await _optimizationService.runOptimization(
        widget.testCaseId,
      );
      await _dataService.saveSolution(widget.testCaseId, newData);

      if (mounted) {
        setState(() => _currentData = newData);
        AppSnackbar.show(context, message: "Result Updated Successfully!");
      }
    } catch (e) {
      if (mounted)
        AppSnackbar.show(context, message: e.toString(), isError: true);
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

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
    setState(() => _isLoading = true);
    await Future.delayed(const Duration(milliseconds: 300)); // UI smooth

    await _fileExportService.exportFile(
      context,
      type,
      widget.testCaseName, // <--- USING THE NAME HERE
      _currentData,
    );

    if (mounted) setState(() => _isLoading = false);
  }

  @override
  Widget build(BuildContext context) {
    final String prettyJson = const JsonEncoder.withIndent(
      '  ',
    ).convert(_currentData);

    return Scaffold(
      appBar: AppBar(
        title: const Text("Optimization Result"),
        actions: [
          IconButton(
            onPressed: _openDownloadDialog,
            icon: const Icon(Icons.download),
            tooltip: "Download",
          ),
        ],
      ),
      body: LoadingOverlay(
        isLoading: _isLoading,
        child: Column(
          children: [
            Expanded(
              child: SingleChildScrollView(
                padding: const EdgeInsets.all(16.0),
                child: Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: Theme.of(context).cardColor,
                    border: Border.all(color: AppColors.borderColor),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: SelectableText(
                    prettyJson,
                    style: TextStyle(
                      fontFamily: 'Courier',
                      fontSize: 14,
                      color: Theme.of(context).textTheme.bodyMedium?.color,
                    ),
                  ),
                ),
              ),
            ),

            // Button Area
            Container(
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
                  child: SizedBox(
                    width: double.infinity,
                    child: ElevatedButton.icon(
                      onPressed: _handleRetry,
                      style: ElevatedButton.styleFrom(
                        backgroundColor: AppColors.primaryBrand,
                        padding: const EdgeInsets.symmetric(vertical: 16),
                      ),
                      icon: const Icon(Icons.refresh, color: Colors.white),
                      label: const Text(
                        "RETRY / RE-RUN OPTIMIZATION",
                        style: TextStyle(
                          color: Colors.white,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
