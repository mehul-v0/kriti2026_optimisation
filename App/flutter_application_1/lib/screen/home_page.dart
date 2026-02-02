import 'dart:typed_data';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_application_1/screen/show_input_page.dart';
import 'package:flutter_application_1/services/auth_service.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/utils/excel_parser.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/theme/theme.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final AuthService _authService = AuthService();
  final DataService _dataService = DataService();

  bool _isLoading = true;
  List<Map<String, dynamic>> _testCases = [];

  @override
  void initState() {
    super.initState();
    _loadData();
  }

  Future<void> _loadData() async {
    try {
      final data = await _dataService.fetchTestCases();
      if (mounted) {
        setState(() {
          _testCases = data;
          _isLoading = false;
        });
      }
    } catch (e) {
      if (mounted) {
        AppSnackbar.show(context, message: e.toString(), isError: true);
        setState(() => _isLoading = false);
      }
    }
  }

  // --- Dialog Logic ---
  void _showAddTestCaseDialog() {
    showDialog(
      context: context,
      builder: (ctx) => _AddTestCaseDialog(
        onSuccess: () {
          _loadData(); // Refresh list after upload
          Navigator.pop(ctx);
        },
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    final isWide = size.width > 600;

    return Scaffold(
      appBar: AppBar(
        title: const Text("Optimization Dashboard"),
        actions: [
          IconButton(
            onPressed: () async => await _authService.signOut(),
            icon: const Icon(Icons.logout),
            tooltip: "Logout",
          ),
        ],
      ),
      // Floating Action Button with Primary Theme
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _showAddTestCaseDialog,
        backgroundColor: Theme.of(context).primaryColor,
        icon: const Icon(Icons.add, color: Colors.white),
        label: const Text(
          "New Test Case",
          style: TextStyle(color: Colors.white),
        ),
      ),
      body: LoadingOverlay(
        isLoading: _isLoading,
        child: Padding(
          padding: EdgeInsets.symmetric(
            horizontal: size.width * 0.05,
            vertical: 20,
          ),
          child: _testCases.isEmpty
              ? _buildEmptyState()
              : _buildGridOrList(isWide),
        ),
      ),
    );
  }

  Widget _buildEmptyState() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(Icons.folder_open, size: 60, color: Colors.grey[400]),
          const SizedBox(height: 16),
          Text(
            "No test cases found",
            style: Theme.of(
              context,
            ).textTheme.titleLarge?.copyWith(color: Colors.grey),
          ),
          const SizedBox(height: 8),
          const Text("Upload an Excel file to get started."),
        ],
      ),
    );
  }

  Widget _buildGridOrList(bool isWide) {
    return GridView.builder(
      gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
        crossAxisCount: isWide ? 3 : 1,
        childAspectRatio: isWide ? 2.5 : 3.5,
        crossAxisSpacing: 16,
        mainAxisSpacing: 16,
      ),
      itemCount: _testCases.length,
      itemBuilder: (context, index) {
        final testCase = _testCases[index];
        return Card(
          elevation: 2,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
          ),
          child: InkWell(
            borderRadius: BorderRadius.circular(12),
            onTap: () {
              Navigator.push(
                context,
                MaterialPageRoute(
                  builder: (context) => ShowInputPage(
                    testCaseName: testCase['case_name'],
                    data: testCase['input_data'],
                  ),
                ),
              );
            },
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Row(
                children: [
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: Theme.of(context).primaryColor.withOpacity(0.1),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: Icon(
                      Icons.description,
                      color: Theme.of(context).primaryColor,
                    ),
                  ),
                  const SizedBox(width: 16),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Text(
                          testCase['case_name'] ?? "Untitled",
                          style: const TextStyle(
                            fontWeight: FontWeight.bold,
                            fontSize: 16,
                          ),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                        const SizedBox(height: 4),
                        Text(
                          "Uploaded: ${testCase['created_at'].toString().split('T')[0]}",
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                  IconButton(
                    icon: const Icon(Icons.delete_outline, color: Colors.grey),
                    onPressed: () async {
                      await _dataService.deleteTestCase(testCase['id']);
                      _loadData(); // Refresh
                    },
                  ),
                ],
              ),
            ),
          ),
        );
      },
    );
  }
}

// --- Local Widget: The Dialog ---

class _AddTestCaseDialog extends StatefulWidget {
  final VoidCallback onSuccess;
  const _AddTestCaseDialog({required this.onSuccess});

  @override
  State<_AddTestCaseDialog> createState() => _AddTestCaseDialogState();
}

class _AddTestCaseDialogState extends State<_AddTestCaseDialog> {
  final _nameController = TextEditingController();
  final DataService _dataService = DataService();

  bool _isUploading = false;
  String? _selectedFileName;
  Uint8List? _selectedFileBytes;

  Future<void> _pickFile() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['xlsx', 'xls'],
      withData: true, // Important for web/bytes access
    );

    if (result != null) {
      setState(() {
        _selectedFileName = result.files.single.name;
        _selectedFileBytes = result.files.single.bytes;
      });
    }
  }

  Future<void> _handleUpload() async {
    if (_nameController.text.isEmpty || _selectedFileBytes == null) {
      AppSnackbar.show(
        context,
        message: "Name and File are mandatory",
        isError: true,
      );
      return;
    }

    setState(() => _isUploading = true);

    try {
      // 1. Parse Bytes to JSON
      final jsonData = ExcelParser.parseExcelBytes(_selectedFileBytes!);

      // 2. Upload to Supabase
      await _dataService.uploadTestCase(_nameController.text.trim(), jsonData);

      if (mounted) {
        AppSnackbar.show(context, message: "Test Case Uploaded!");
        widget.onSuccess();
      }
    } catch (e) {
      if (mounted) {
        AppSnackbar.show(context, message: "Upload failed: $e", isError: true);
      }
    } finally {
      if (mounted) setState(() => _isUploading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    // Basic media query for dialog width
    final width = MediaQuery.of(context).size.width;

    return AlertDialog(
      title: const Text("Add New Test Case"),
      content: SizedBox(
        width: width > 600 ? 400 : width * 0.8,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: _nameController,
              decoration: const InputDecoration(labelText: "Test Case Name"),
            ),
            const SizedBox(height: 20),

            // File Picker Area
            InkWell(
              onTap: _pickFile,
              child: Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  border: Border.all(color: AppColors.borderColor),
                  borderRadius: BorderRadius.circular(8),
                  color: AppColors.lightBackground,
                ),
                child: Row(
                  children: [
                    const Icon(
                      Icons.upload_file,
                      color: AppColors.primaryBrand,
                    ),
                    const SizedBox(width: 10),
                    Expanded(
                      child: Text(
                        _selectedFileName ?? "Click to upload Excel file",
                        style: TextStyle(
                          color: _selectedFileName == null
                              ? Colors.grey
                              : Colors.black87,
                          overflow: TextOverflow.ellipsis,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
      actions: [
        if (_isUploading)
          const Padding(
            padding: EdgeInsets.all(8.0),
            child: CircularProgressIndicator(),
          )
        else ...[
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text("Cancel"),
          ),
          ElevatedButton(onPressed: _handleUpload, child: const Text("Upload")),
        ],
      ],
    );
  }
}
