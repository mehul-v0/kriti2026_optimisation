import 'dart:convert';
import 'dart:typed_data';
import 'package:http/http.dart' as http;
import 'package:flutter_application_1/config/env.dart';

/// Service to upload Excel files to the backend for JSON conversion.
/// The backend (/api/upload) converts Excel → JSON using server-side conversion.
class UploadService {
  /// Upload an Excel file to the backend and receive the converted JSON.
  ///
  /// [fileBytes] - Raw bytes of the Excel file (from FilePicker)
  /// [fileName] - Original filename (e.g., "test_case_1.xlsx")
  ///
  /// Returns the parsed JSON data from backend:
  /// {
  ///   "success": true,
  ///   "message": "Parsed X employees and Y vehicles",
  ///   "filename": "1234567890_test_case_1.xlsx",
  ///   "digest": {...},
  ///   "employees": [...],
  ///   "vehicles": [...],
  ///   "baseline_cost": 1234.56
  /// }
  Future<Map<String, dynamic>> uploadExcelFile(
    Uint8List fileBytes,
    String fileName,
  ) async {
    try {
      final uri = Uri.parse('${Env.baseUrl}/api/upload');

      // Create multipart request
      var request = http.MultipartRequest('POST', uri);

      // Add file as multipart form-data
      request.files.add(
        http.MultipartFile.fromBytes(
          'file', // Backend expects 'file' field
          fileBytes,
          filename: fileName,
        ),
      );

      // Send request
      final streamedResponse = await request.send();
      final response = await http.Response.fromStream(streamedResponse);

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body) as Map<String, dynamic>;

        if (data['success'] == true) {
          return data;
        } else {
          throw Exception(data['error'] ?? 'Backend conversion failed');
        }
      } else {
        // Try to parse error message from response
        try {
          final errorBody = jsonDecode(response.body);
          throw Exception(
            errorBody['error'] ?? 'Server Error: ${response.statusCode}',
          );
        } catch (_) {
          throw Exception('Upload failed: ${response.statusCode}');
        }
      }
    } catch (e) {
      if (e is Exception) rethrow;
      throw Exception('Connection error: $e');
    }
  }

  /// Convert Google Sheets to Excel format and upload.
  ///
  /// For Google Sheets, we need to:
  /// 1. Export the sheet as .xlsx using Google Sheets export API
  /// 2. Send the exported Excel file to backend /api/upload
  ///
  /// [spreadsheetId] - The Google Sheets ID
  ///
  /// Returns the same format as uploadExcelFile
  Future<Map<String, dynamic>> uploadGoogleSheet(String spreadsheetId) async {
    try {
      final excelBytes = await exportGoogleSheetAsBytes(spreadsheetId);

      // Now upload the Excel bytes to our backend
      return await uploadExcelFile(
        excelBytes,
        'google_sheet_$spreadsheetId.xlsx',
      );
    } catch (e) {
      if (e is Exception) rethrow;
      throw Exception('Google Sheets export error: $e');
    }
  }

  /// Export Google Sheet as Excel bytes (for storage and re-use)
  Future<Uint8List> exportGoogleSheetAsBytes(String spreadsheetId) async {
    try {
      // Google Sheets export URL for .xlsx format
      final exportUrl = Uri.parse(
        'https://docs.google.com/spreadsheets/d/$spreadsheetId/export?format=xlsx',
      );

      // Download as Excel
      final exportResponse = await http
          .get(exportUrl)
          .timeout(const Duration(seconds: 30));

      if (exportResponse.statusCode != 200) {
        throw Exception(
          'Failed to export Google Sheet. Make sure the sheet is publicly accessible.',
        );
      }

      return exportResponse.bodyBytes;
    } catch (e) {
      if (e is Exception) rethrow;
      throw Exception('Google Sheets download error: $e');
    }
  }

  /// Helper to extract spreadsheet ID from URL
  static String? extractSpreadsheetId(String input) {
    input = input.trim();

    // If it looks like a raw ID (no slashes, no spaces)
    if (!input.contains('/') && !input.contains(' ') && input.length > 10) {
      return input;
    }

    // Extract from URL pattern: /spreadsheets/d/ID/
    final regex = RegExp(r'/spreadsheets/d/([a-zA-Z0-9_-]+)');
    final match = regex.firstMatch(input);
    return match?.group(1);
  }
}
