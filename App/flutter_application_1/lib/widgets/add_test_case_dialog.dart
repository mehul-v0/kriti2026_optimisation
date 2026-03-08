import 'dart:typed_data';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/services/upload_service.dart';
import 'package:flutter_application_1/theme/theme.dart';

enum InputMode { excel, googleSheets }

class AddTestCaseDialog extends StatefulWidget {
  final VoidCallback onSuccess;

  const AddTestCaseDialog({super.key, required this.onSuccess});

  @override
  State<AddTestCaseDialog> createState() => _AddTestCaseDialogState();
}

class _AddTestCaseDialogState extends State<AddTestCaseDialog> {
  final _nameController = TextEditingController();
  final _sheetsUrlController = TextEditingController();
  final DataService _dataService = DataService();
  final UploadService _uploadService = UploadService();

  bool _isUploading = false;
  InputMode _inputMode = InputMode.excel;
  String? _errorMessage;

  // Excel state
  String? _selectedFileName;
  Uint8List? _selectedFileBytes;

  Future<void> _pickFile() async {
    try {
      FilePickerResult? result = await FilePicker.platform.pickFiles(
        type: FileType.custom,
        allowedExtensions: ['xlsx', 'xls'],
        withData: true,
      );

      if (result != null && result.files.isNotEmpty) {
        setState(() {
          _selectedFileName = result.files.single.name;
          _selectedFileBytes = result.files.single.bytes;
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() => _errorMessage = 'Error picking file: $e');
      }
    }
  }

  Future<void> _handleUpload() async {
    // Clear any previous error
    setState(() => _errorMessage = null);

    if (_nameController.text.trim().isEmpty) {
      setState(() => _errorMessage = 'Please enter a case name');
      return;
    }

    if (_inputMode == InputMode.excel && _selectedFileBytes == null) {
      setState(() => _errorMessage = 'Please select an Excel file');
      return;
    }

    if (_inputMode == InputMode.googleSheets &&
        _sheetsUrlController.text.trim().isEmpty) {
      setState(() => _errorMessage = 'Please enter a Google Sheets URL');
      return;
    }

    setState(() => _isUploading = true);

    try {
      Map<String, dynamic> backendResponse;

      // Send file to backend for conversion
      if (_inputMode == InputMode.excel) {
        // Ensure filename always has .xlsx extension (Android FilePicker
        // can return display name without extension, causing backend 400)
        final rawName = _selectedFileName ?? 'upload.xlsx';
        final safeFileName =
            (rawName.toLowerCase().endsWith('.xlsx') ||
                rawName.toLowerCase().endsWith('.xls'))
            ? rawName
            : '$rawName.xlsx';
        backendResponse = await _uploadService.uploadExcelFile(
          _selectedFileBytes!,
          safeFileName,
        );
      } else {
        // Extract Google Sheets ID and export as Excel, then upload
        final spreadsheetId = UploadService.extractSpreadsheetId(
          _sheetsUrlController.text.trim(),
        );

        if (spreadsheetId == null) {
          throw Exception(
            'Invalid Google Sheets URL. Please provide a valid link.',
          );
        }

        backendResponse = await _uploadService.uploadGoogleSheet(spreadsheetId);
      }

      // Backend /api/upload returns: employees, vehicles, digest, baseline_cost,
      // and metadata (parsed from the Excel Metadata sheet).
      // baseline list is NOT returned; only baseline_cost (a scalar) is provided.
      final inputJson = {
        "employees": backendResponse["employees"] ?? [],
        "vehicles": backendResponse["vehicles"] ?? [],
        // Preserve metadata returned by backend (cost/time weights, priority delays, etc.)
        "metadata":
            (backendResponse["metadata"] as Map<String, dynamic>?) ??
            <String, dynamic>{},
        // baseline list is not returned by backend; use empty list
        "baseline": <dynamic>[],
      };

      // Store the backend-generated JSON in Supabase exactly as-is
      // NO file storage, NO modifications
      await _dataService.uploadTestCase(_nameController.text.trim(), inputJson);

      if (mounted) {
        widget.onSuccess();
      }
    } catch (e) {
      if (mounted) {
        // Clean up the error message for display
        String msg = e.toString();
        if (msg.startsWith('Exception: ')) msg = msg.substring(11);
        setState(() => _errorMessage = msg);
      }
    } finally {
      if (mounted) setState(() => _isUploading = false);
    }
  }

  @override
  void dispose() {
    _nameController.dispose();
    _sheetsUrlController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final width = MediaQuery.sizeOf(context).width;
    final isDesktop = width > 600;
    final theme = Theme.of(context);
    final isDark = theme.brightness == Brightness.dark;
    final surfaceColor = isDark
        ? AppColors.darkSurface
        : AppColors.lightSurface;
    final borderColor = isDark
        ? AppColors.darkBorderColor
        : AppColors.borderColor;

    return Dialog(
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(20)),
      backgroundColor: surfaceColor,
      insetPadding: EdgeInsets.symmetric(
        horizontal: isDesktop ? (width - 420) / 2 : 24,
        vertical: 40,
      ),
      child: SingleChildScrollView(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // Header
              _buildHeader(theme, isDark),
              const SizedBox(height: 24),

              // Case Name Input
              _buildNameField(isDark, borderColor),
              const SizedBox(height: 20),

              // Input Mode Toggle
              _buildInputModeToggle(isDark, borderColor),
              const SizedBox(height: 16),

              // Input Area (Excel or Google Sheets)
              if (_inputMode == InputMode.excel)
                _buildFilePickerArea(theme, isDark, borderColor)
              else
                _buildGoogleSheetsInput(isDark, borderColor),

              const SizedBox(height: 20),

              // Inline error banner � always in front, no z-order issues
              if (_errorMessage != null)
                Container(
                  margin: const EdgeInsets.only(bottom: 12),
                  padding: const EdgeInsets.symmetric(
                    horizontal: 14,
                    vertical: 10,
                  ),
                  decoration: BoxDecoration(
                    color: const Color(0x1FE53935),
                    border: Border.all(color: const Color(0x66E53935)),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const Icon(
                        Icons.error_outline,
                        color: Color(0xFFE53935),
                        size: 18,
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          _errorMessage!,
                          style: const TextStyle(
                            color: Color(0xFFE53935),
                            fontSize: 13,
                            height: 1.4,
                          ),
                        ),
                      ),
                      GestureDetector(
                        onTap: () => setState(() => _errorMessage = null),
                        child: const Icon(
                          Icons.close,
                          color: Color(0xFFE53935),
                          size: 16,
                        ),
                      ),
                    ],
                  ),
                ),

              // Action Buttons
              _buildActionButtons(isDark, borderColor),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildHeader(ThemeData theme, bool isDark) {
    return Row(
      children: [
        Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: context.primary.withOpacity(0.12),
            borderRadius: BorderRadius.circular(12),
          ),
          child: Icon(
            Icons.add_chart_rounded,
            color: context.primary,
            size: 24,
          ),
        ),
        const SizedBox(width: 14),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                "New Test Case",
                style: theme.textTheme.titleLarge?.copyWith(
                  fontWeight: FontWeight.bold,
                ),
              ),
              const SizedBox(height: 2),
              Text(
                "Upload Excel or import from Google Sheets",
                style: theme.textTheme.bodySmall?.copyWith(color: Colors.grey),
              ),
            ],
          ),
        ),
        IconButton(
          onPressed: () => Navigator.pop(context),
          icon: Icon(Icons.close, color: Colors.grey[500]),
          style: IconButton.styleFrom(
            backgroundColor: isDark
                ? const Color(0x0DFFFFFF)
                : const Color(0x1A9E9E9E),
          ),
        ),
      ],
    );
  }

  Widget _buildNameField(bool isDark, Color borderColor) {
    return TextField(
      controller: _nameController,
      style: TextStyle(color: isDark ? Colors.white : Colors.black87),
      decoration: InputDecoration(
        labelText: "Case Name",
        labelStyle: TextStyle(
          color: isDark ? Colors.grey[400] : Colors.grey[600],
        ),
        hintText: "Enter a name for this case",
        hintStyle: TextStyle(
          color: isDark ? Colors.grey[600] : Colors.grey[400],
        ),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: borderColor),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: borderColor),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
          borderSide: BorderSide(color: context.primary, width: 2),
        ),
        prefixIcon: Icon(
          Icons.edit_outlined,
          color: isDark ? Colors.grey[400] : Colors.grey[600],
        ),
        filled: true,
        fillColor: isDark ? const Color(0x0DFFFFFF) : const Color(0x0A9E9E9E),
      ),
    );
  }

  Widget _buildInputModeToggle(bool isDark, Color borderColor) {
    return Container(
      height: 44,
      decoration: BoxDecoration(
        color: isDark ? const Color(0x1AFFFFFF) : Colors.grey.shade200,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: borderColor),
      ),
      child: Row(
        children: [
          Expanded(
            child: _buildModeTab(
              label: "Excel File",
              icon: Icons.description_outlined,
              mode: InputMode.excel,
              isDark: isDark,
            ),
          ),
          Expanded(
            child: _buildModeTab(
              label: "Google Sheets",
              icon: Icons.table_chart_outlined,
              mode: InputMode.googleSheets,
              isDark: isDark,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildModeTab({
    required String label,
    required IconData icon,
    required InputMode mode,
    required bool isDark,
  }) {
    final isActive = _inputMode == mode;
    return GestureDetector(
      onTap: () => setState(() => _inputMode = mode),
      child: Container(
        margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
        decoration: BoxDecoration(
          color: isActive ? context.primary : Colors.transparent,
          borderRadius: BorderRadius.circular(7), // 10 (outer) - 3 (margin) = 7
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              icon,
              color: isActive
                  ? Colors.white
                  : (isDark ? Colors.grey[400] : Colors.grey[700]),
              size: 18,
            ),
            const SizedBox(width: 6),
            Text(
              label,
              style: TextStyle(
                color: isActive
                    ? Colors.white
                    : (isDark ? Colors.grey[400] : Colors.grey[700]),
                fontWeight: isActive ? FontWeight.bold : FontWeight.normal,
                fontSize: 13,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildFilePickerArea(ThemeData theme, bool isDark, Color borderColor) {
    final hasFile = _selectedFileName != null;
    return InkWell(
      onTap: _pickFile,
      borderRadius: BorderRadius.circular(12),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 200),
        padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 16),
        decoration: BoxDecoration(
          border: Border.all(
            color: hasFile ? context.primary.withOpacity(0.5) : borderColor,
            width: hasFile ? 1.5 : 1,
          ),
          borderRadius: BorderRadius.circular(12),
          color: hasFile
              ? context.primary.withOpacity(isDark ? 0.08 : 0.04)
              : (isDark ? const Color(0x08FFFFFF) : const Color(0x089E9E9E)),
        ),
        child: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: context.primary.withOpacity(hasFile ? 0.15 : 0.1),
                borderRadius: BorderRadius.circular(10),
              ),
              child: Icon(
                hasFile
                    ? Icons.check_circle_outline
                    : Icons.upload_file_outlined,
                color: context.primary,
                size: 22,
              ),
            ),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    _selectedFileName ?? "Select Excel File",
                    style: TextStyle(
                      fontWeight: hasFile ? FontWeight.w600 : FontWeight.normal,
                      fontSize: 14,
                      color: hasFile
                          ? (isDark ? Colors.white : Colors.black87)
                          : Colors.grey,
                    ),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                  const SizedBox(height: 3),
                  Text(
                    hasFile ? "Tap to change file" : "Supports .xlsx, .xls",
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: Colors.grey[500],
                      fontSize: 12,
                    ),
                  ),
                ],
              ),
            ),
            Icon(Icons.chevron_right, color: Colors.grey[400], size: 20),
          ],
        ),
      ),
    );
  }

  Widget _buildGoogleSheetsInput(bool isDark, Color borderColor) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        TextField(
          controller: _sheetsUrlController,
          style: TextStyle(color: isDark ? Colors.white : Colors.black87),
          maxLines: 1,
          decoration: InputDecoration(
            labelText: "Google Sheets URL",
            labelStyle: TextStyle(
              color: isDark ? Colors.grey[400] : Colors.grey[600],
            ),
            hintText: "https://docs.google.com/spreadsheets/d/...",
            hintStyle: TextStyle(
              color: isDark ? Colors.grey[600] : Colors.grey[400],
              fontSize: 13,
            ),
            border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
              borderSide: BorderSide(color: borderColor),
            ),
            enabledBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
              borderSide: BorderSide(color: borderColor),
            ),
            focusedBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(12),
              borderSide: BorderSide(color: context.primary, width: 2),
            ),
            prefixIcon: Icon(
              Icons.link,
              color: isDark ? Colors.grey[400] : Colors.grey[600],
            ),
            filled: true,
            fillColor: isDark
                ? const Color(0x0DFFFFFF)
                : const Color(0x0A9E9E9E),
          ),
        ),
        const SizedBox(height: 10),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 4),
          child: Text(
            "The sheet must be shared publicly (\"Anyone with the link\").\n"
            "Required tabs: employees, vehicles, metadata, baseline",
            style: TextStyle(
              color: Colors.grey[500],
              fontSize: 11,
              height: 1.4,
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildActionButtons(bool isDark, Color borderColor) {
    if (_isUploading) {
      return Center(
        child: Padding(
          padding: EdgeInsets.symmetric(vertical: 8),
          child: SizedBox(
            width: 24,
            height: 24,
            child: CircularProgressIndicator(
              strokeWidth: 2.5,
              color: context.primary,
            ),
          ),
        ),
      );
    }

    return Row(
      children: [
        Expanded(
          child: OutlinedButton(
            onPressed: () => Navigator.pop(context),
            style: OutlinedButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 14),
              side: BorderSide(color: borderColor),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
              ),
              foregroundColor: isDark ? Colors.grey[300] : Colors.grey[700],
            ),
            child: const Text("Cancel"),
          ),
        ),
        SizedBox(width: 12),
        Expanded(
          flex: 2,
          child: ElevatedButton(
            onPressed: _handleUpload,
            style: ElevatedButton.styleFrom(
              backgroundColor: context.primary,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(vertical: 14),
              elevation: 0,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(12),
              ),
            ),
            child: const Text(
              "Upload",
              style: TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
        ),
      ],
    );
  }
}
