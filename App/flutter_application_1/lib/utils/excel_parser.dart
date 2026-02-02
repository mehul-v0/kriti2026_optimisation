import 'dart:typed_data';
import 'package:excel/excel.dart';

class ExcelParser {
  /// Parses the Excel file bytes into the required JSON map.
  static Map<String, dynamic> parseExcelBytes(Uint8List bytes) {
    var excel = Excel.decodeBytes(bytes);

    // 1. Parse Employees
    List<Map<String, dynamic>> employees = [];
    if (excel.tables.keys.contains('employees')) {
      var sheet = excel.tables['employees']!;
      // Skip header row (index 0), start from 1
      for (int i = 1; i < sheet.maxRows; i++) {
        var row = sheet.rows[i];
        if (row.isEmpty) continue;

        employees.add({
          "employee_id": _getVal(row[0]),
          "priority": _getInt(row[1]),
          "pickup_lat": _getDouble(row[2]),
          "pickup_lng": _getDouble(row[3]),
          "drop_lat": _getDouble(row[4]),
          "drop_lng": _getDouble(row[5]),
          "earliest_pickup": _getVal(row[6]),
          "latest_drop": _getVal(row[7]),
          "vehicle_preference": _getVal(row[8]),
          "sharing_preference": _getVal(row[9]),
        });
      }
    }

    // 2. Parse Vehicles
    List<Map<String, dynamic>> vehicles = [];
    if (excel.tables.keys.contains('vehicles')) {
      var sheet = excel.tables['vehicles']!;
      for (int i = 1; i < sheet.maxRows; i++) {
        var row = sheet.rows[i];
        if (row.isEmpty) continue;

        vehicles.add({
          "vehicle_id": _getVal(row[0]),
          "fuel_type": _getVal(row[1]),
          "vehicle_type": _getVal(row[2]),
          "capacity": _getInt(row[3]),
          "cost_per_km": _getDouble(row[4]),
          "avg_speed_kmph": _getDouble(row[5]),
          "current_lat": _getDouble(row[6]),
          "current_lng": _getDouble(row[7]),
          "available_from": _getVal(row[8]),
          "category": _getVal(row[9]),
        });
      }
    }

    // 3. Parse Metadata (Key-Value pairs)
    Map<String, dynamic> metadata = {};
    if (excel.tables.keys.contains('metadata')) {
      var sheet = excel.tables['metadata']!;
      for (int i = 1; i < sheet.maxRows; i++) {
        var row = sheet.rows[i];
        if (row.isEmpty) continue;
        // Col 0 is Key, Col 1 is Value
        String key = _getVal(row[0]).toString();
        var value = _getAutoVal(row[1]);
        metadata[key] = value;
      }
    }

    // 4. Parse Baseline
    List<Map<String, dynamic>> baseline = [];
    if (excel.tables.keys.contains('baseline')) {
      var sheet = excel.tables['baseline']!;
      for (int i = 1; i < sheet.maxRows; i++) {
        var row = sheet.rows[i];
        if (row.isEmpty) continue;

        baseline.add({
          "employee_id": _getVal(row[0]),
          "baseline_cost": _getDouble(row[1]),
          "baseline_time_min": _getDouble(
            row[2],
          ), // Adjusted key name to match csv
        });
      }
    }

    return {
      "employees": employees,
      "vehicles": vehicles,
      "metadata": metadata,
      "baseline": baseline,
    };
  }

  // --- Helper methods to safely handle Excel cells ---

  static dynamic _getVal(Data? cell) {
    return cell?.value?.toString() ?? "";
  }

  static double _getDouble(Data? cell) {
    if (cell?.value == null) return 0.0;
    return double.tryParse(cell!.value.toString()) ?? 0.0;
  }

  static int _getInt(Data? cell) {
    if (cell?.value == null) return 0;
    return int.tryParse(cell!.value.toString()) ?? 0;
  }

  // Auto-detect type for metadata values (bool, int, double, string)
  static dynamic _getAutoVal(Data? cell) {
    if (cell?.value == null) return "";
    String val = cell!.value.toString();

    if (val.toLowerCase() == 'true') return true;
    if (val.toLowerCase() == 'false') return false;
    if (int.tryParse(val) != null) return int.parse(val);
    if (double.tryParse(val) != null) return double.parse(val);
    return val;
  }
}
