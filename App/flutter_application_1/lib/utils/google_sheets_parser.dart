import 'dart:convert';
import 'package:http/http.dart' as http;

/// Parses a public/shared Google Sheet into the same JSON format as ExcelParser.
/// The sheet must be shared as "Anyone with the link can view".
///
/// Supports two URL formats:
/// - Full URL: https://docs.google.com/spreadsheets/d/SPREADSHEET_ID/...
/// - Just the spreadsheet ID
///
/// The Google Sheet must have named sheets: employees, vehicles, metadata, baseline
class GoogleSheetsParser {
  /// Extracts the spreadsheet ID from a Google Sheets URL
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

  /// Fetches and parses all sheets from a Google Spreadsheet
  static Future<Map<String, dynamic>> parseFromUrl(String urlOrId) async {
    final spreadsheetId = extractSpreadsheetId(urlOrId);
    if (spreadsheetId == null) {
      throw Exception(
        'Invalid Google Sheets URL or ID. '
        'Please provide a valid link like:\n'
        'https://docs.google.com/spreadsheets/d/YOUR_ID/edit',
      );
    }

    // Fetch each named sheet as CSV
    final employees = await _fetchSheetAsCsv(spreadsheetId, 'employees');
    final vehicles = await _fetchSheetAsCsv(spreadsheetId, 'vehicles');
    final metadata = await _fetchSheetAsCsv(spreadsheetId, 'metadata');
    final baseline = await _fetchSheetAsCsv(spreadsheetId, 'baseline');

    return {
      "employees": _parseEmployees(employees),
      "vehicles": _parseVehicles(vehicles),
      "metadata": _parseMetadata(metadata),
      "baseline": _parseBaseline(baseline),
    };
  }

  /// Fetches a specific sheet/tab from a Google Spreadsheet as CSV
  static Future<List<List<String>>> _fetchSheetAsCsv(
    String spreadsheetId,
    String sheetName,
  ) async {
    // Google Sheets export URL for a specific sheet as CSV
    final url = Uri.parse(
      'https://docs.google.com/spreadsheets/d/$spreadsheetId'
      '/gviz/tq?tqx=out:csv&sheet=$sheetName',
    );

    try {
      final response = await http.get(url).timeout(const Duration(seconds: 15));

      if (response.statusCode == 200) {
        return _parseCsv(response.body);
      } else if (response.statusCode == 400) {
        // Sheet might not exist, return empty
        return [];
      } else {
        throw Exception(
          'Failed to fetch sheet "$sheetName" (HTTP ${response.statusCode}). '
          'Make sure the spreadsheet is shared publicly.',
        );
      }
    } catch (e) {
      if (e is Exception && e.toString().contains('Failed to fetch')) rethrow;
      // Sheet doesn't exist or network error - return empty for optional sheets
      return [];
    }
  }

  /// Simple CSV parser that handles quoted fields
  static List<List<String>> _parseCsv(String csvText) {
    final List<List<String>> rows = [];
    final lines = const LineSplitter().convert(csvText);

    for (final line in lines) {
      if (line.trim().isEmpty) continue;
      rows.add(_parseCsvLine(line));
    }
    return rows;
  }

  /// Parse a single CSV line handling quoted fields with commas
  static List<String> _parseCsvLine(String line) {
    final List<String> fields = [];
    final buf = StringBuffer();
    bool inQuotes = false;

    for (int i = 0; i < line.length; i++) {
      final ch = line[i];
      if (inQuotes) {
        if (ch == '"') {
          if (i + 1 < line.length && line[i + 1] == '"') {
            buf.write('"');
            i++; // Skip escaped quote
          } else {
            inQuotes = false;
          }
        } else {
          buf.write(ch);
        }
      } else {
        if (ch == '"') {
          inQuotes = true;
        } else if (ch == ',') {
          fields.add(buf.toString().trim());
          buf.clear();
        } else {
          buf.write(ch);
        }
      }
    }
    fields.add(buf.toString().trim());
    return fields;
  }

  // --- Sheet-specific parsers (skip header row, same structure as ExcelParser) ---

  static List<Map<String, dynamic>> _parseEmployees(List<List<String>> rows) {
    if (rows.length <= 1) return []; // Only header or empty
    final List<Map<String, dynamic>> result = [];
    for (int i = 1; i < rows.length; i++) {
      final r = rows[i];
      if (r.isEmpty || (r.length == 1 && r[0].isEmpty)) continue;
      result.add({
        "employee_id": _str(r, 0),
        "priority": _int(r, 1),
        "pickup_lat": _double(r, 2),
        "pickup_lng": _double(r, 3),
        "drop_lat": _double(r, 4),
        "drop_lng": _double(r, 5),
        "earliest_pickup": _str(r, 6),
        "latest_drop": _str(r, 7),
        "vehicle_preference": _str(r, 8),
        "sharing_preference": _str(r, 9),
      });
    }
    return result;
  }

  static List<Map<String, dynamic>> _parseVehicles(List<List<String>> rows) {
    if (rows.length <= 1) return [];
    final List<Map<String, dynamic>> result = [];
    for (int i = 1; i < rows.length; i++) {
      final r = rows[i];
      if (r.isEmpty || (r.length == 1 && r[0].isEmpty)) continue;
      result.add({
        "vehicle_id": _str(r, 0),
        "fuel_type": _str(r, 1),
        "vehicle_type": _str(r, 2),
        "capacity": _int(r, 3),
        "cost_per_km": _double(r, 4),
        "avg_speed_kmph": _double(r, 5),
        "current_lat": _double(r, 6),
        "current_lng": _double(r, 7),
        "available_from": _str(r, 8),
        "category": _str(r, 9),
      });
    }
    return result;
  }

  static Map<String, dynamic> _parseMetadata(List<List<String>> rows) {
    if (rows.length <= 1) return {};
    final Map<String, dynamic> result = {};
    for (int i = 1; i < rows.length; i++) {
      final r = rows[i];
      if (r.isEmpty || (r.length == 1 && r[0].isEmpty)) continue;
      final key = _str(r, 0);
      if (key.isEmpty) continue;
      result[key] = _autoVal(_str(r, 1));
    }
    return result;
  }

  static List<Map<String, dynamic>> _parseBaseline(List<List<String>> rows) {
    if (rows.length <= 1) return [];
    final List<Map<String, dynamic>> result = [];
    for (int i = 1; i < rows.length; i++) {
      final r = rows[i];
      if (r.isEmpty || (r.length == 1 && r[0].isEmpty)) continue;
      result.add({
        "employee_id": _str(r, 0),
        "baseline_cost": _double(r, 1),
        "baseline_time_min": _double(r, 2),
      });
    }
    return result;
  }

  // --- Helpers ---

  static String _str(List<String> row, int index) {
    if (index >= row.length) return "";
    return row[index];
  }

  static double _double(List<String> row, int index) {
    if (index >= row.length) return 0.0;
    return double.tryParse(row[index]) ?? 0.0;
  }

  static int _int(List<String> row, int index) {
    if (index >= row.length) return 0;
    return int.tryParse(row[index]) ?? 0;
  }

  static dynamic _autoVal(String val) {
    if (val.toLowerCase() == 'true') return true;
    if (val.toLowerCase() == 'false') return false;
    if (int.tryParse(val) != null) return int.parse(val);
    if (double.tryParse(val) != null) return double.parse(val);
    return val;
  }
}
