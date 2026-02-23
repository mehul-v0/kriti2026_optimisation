import 'dart:convert';
import 'dart:io';
import 'package:excel/excel.dart';
import 'package:flutter/foundation.dart'; // for compute()
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';
import 'package:pdf/pdf.dart';
import 'package:pdf/widgets.dart' as pw;
import 'package:permission_handler/permission_handler.dart';
import 'package:printing/printing.dart'; // PdfGoogleFonts — embeds real TTF
import 'package:device_info_plus/device_info_plus.dart';

class FileExportService {
  Future<void> exportFile(
    dynamic context,
    String type,
    String fileName,
    Map<String, dynamic> data,
  ) async {
    try {
      // 1. Robust Permission Check
      if (!await _checkStoragePermission()) {
        AppSnackbar.show(
          context,
          message:
              "Storage permission is required to save files. Please enable it in Settings.",
          isError: true,
        );
        await openAppSettings();
        return;
      }

      List<int> bytes = [];
      String extension = "";
      final timestamp = DateFormat('yyyyMMdd_HHmmss').format(DateTime.now());
      final exportName = 'optimization_output_$timestamp';

      switch (type) {
        case 'json':
          const encoder = JsonEncoder.withIndent('  ');
          bytes = utf8.encode(encoder.convert(data));
          extension = "json";
          break;
        case 'excel':
          bytes = await _generateExcel(data);
          extension = "xlsx";
          break;
        case 'pdf':
          // Run in background isolate — pdf.save() is CPU-intensive
          // (font embedding + zlib compression) and would freeze the UI
          // if executed on the main thread.
          bytes = await compute(FileExportService._generatePdf, data);
          extension = "pdf";
          break;
      }

      final String filePath = await _saveToDownloads(
        bytes,
        exportName,
        extension,
      );
      AppSnackbar.show(context, message: "Saved successfully to: $filePath");
    } catch (e) {
      AppSnackbar.show(context, message: "Export failed: $e", isError: true);
    }
  }

  Future<bool> _checkStoragePermission() async {
    if (!Platform.isAndroid) return true;

    final deviceInfo = await DeviceInfoPlugin().androidInfo;
    final sdkInt = deviceInfo.version.sdkInt;

    if (sdkInt >= 33) {
      // Android 13+ doesn't use READ_EXTERNAL_STORAGE for files usually
      // It relies on scoped storage. We often don't need explicit permission
      // to write to 'Downloads', but checking 'photos' or 'videos' might be needed if accessing media.
      // For purely writing documents to Downloads, we can often skip this or check:
      return true; // Usually allowed to write to own app cache or public downloads via MediaStore logic
    } else if (sdkInt >= 30) {
      // Android 11 & 12
      var status = await Permission.storage.status;
      if (status.isGranted) return true;

      // If Manage External Storage is needed (rare for simple file saving, but robust)
      if (await Permission.manageExternalStorage.request().isGranted) {
        return true;
      }
      return await Permission.storage.request().isGranted;
    } else {
      // Android 10 and below
      var status = await Permission.storage.status;
      if (!status.isGranted) {
        status = await Permission.storage.request();
      }
      return status.isGranted;
    }
  }

  Future<String> _saveToDownloads(
    List<int> bytes,
    String name,
    String ext,
  ) async {
    Directory? directory;
    try {
      if (Platform.isAndroid) {
        // Try standard downloads path first
        directory = Directory('/storage/emulated/0/Download');
        if (!await directory.exists()) {
          // Fallback to app-specific external storage which is always allowed
          directory = await getExternalStorageDirectory();
        }
      } else {
        directory = await getApplicationDocumentsDirectory();
      }
    } catch (e) {
      directory = await getApplicationDocumentsDirectory();
    }

    // Sanitize filename
    final safeName = name.replaceAll(RegExp(r'[^\w\s]+'), '');
    final String path =
        '${directory!.path}/${safeName}_${DateTime.now().millisecondsSinceEpoch}.$ext';

    final File file = File(path);
    await file.writeAsBytes(bytes);
    return path;
  }

  Future<List<int>> _generateExcel(Map<String, dynamic> data) async {
    var excel = Excel.createExcel();

    // ── Sheet 1: Summary ──
    Sheet summary = excel['Summary'];
    final Map result = data['result'] ?? data['stats'] ?? {};

    summary.appendRow([TextCellValue('Metric'), TextCellValue('Value')]);
    summary.appendRow([
      TextCellValue('Total Cost'),
      DoubleCellValue((result['total_cost'] ?? data['cost'] ?? 0).toDouble()),
    ]);
    summary.appendRow([
      TextCellValue('Total Time'),
      DoubleCellValue(
        (result['total_time'] ?? data['total_time'] ?? 0).toDouble(),
      ),
    ]);
    summary.appendRow([
      TextCellValue('Total Distance'),
      DoubleCellValue((result['total_distance'] ?? 0).toDouble()),
    ]);
    summary.appendRow([
      TextCellValue('Vehicles Used'),
      IntCellValue(result['vehicles_used'] ?? 0),
    ]);
    summary.appendRow([
      TextCellValue('Vehicles Available'),
      IntCellValue(result['vehicles_available'] ?? 0),
    ]);
    summary.appendRow([
      TextCellValue('Hard Violations'),
      IntCellValue(
        result['hard_violations'] ?? data['stats']?['hard_violations'] ?? 0,
      ),
    ]);
    summary.appendRow([
      TextCellValue('Soft Violations'),
      IntCellValue(
        result['soft_violations'] ?? data['stats']?['soft_violations'] ?? 0,
      ),
    ]);
    if (result['baseline_cost'] != null) {
      summary.appendRow([
        TextCellValue('Baseline Cost'),
        DoubleCellValue((result['baseline_cost'] as num).toDouble()),
      ]);
    }
    if (result['cost_savings'] != null) {
      summary.appendRow([
        TextCellValue('Cost Savings'),
        DoubleCellValue((result['cost_savings'] as num).toDouble()),
      ]);
    }
    if (result['cost_savings_percent'] != null) {
      summary.appendRow([
        TextCellValue('Cost Savings %'),
        DoubleCellValue((result['cost_savings_percent'] as num).toDouble()),
      ]);
    }
    summary.appendRow([
      TextCellValue('Generated'),
      TextCellValue(DateFormat('yyyy-MM-dd HH:mm:ss').format(DateTime.now())),
    ]);

    // ── Sheet 2: Vehicle Routes ──
    Sheet routeSheet = excel['Vehicle Routes'];

    // Detect format
    final bool isFormatB = data.containsKey('routes') && data['routes'] is List;

    if (isFormatB) {
      routeSheet.appendRow([
        TextCellValue('Vehicle ID'),
        TextCellValue('Trip #'),
        TextCellValue('Type'),
        TextCellValue('Employee ID'),
        TextCellValue('Arrival'),
        TextCellValue('Departure'),
        TextCellValue('Distance (km)'),
      ]);

      List routes = data['routes'] ?? [];
      for (var route in routes) {
        String vId = route['vehicle_id']?.toString() ?? '';
        List points = route['route_points'] ?? [];
        for (var p in points) {
          routeSheet.appendRow([
            TextCellValue(vId),
            IntCellValue(p['trip_number'] ?? 1),
            TextCellValue(p['type']?.toString() ?? ''),
            TextCellValue(p['employee_id']?.toString() ?? 'Office'),
            TextCellValue(p['arrival_time']?.toString() ?? ''),
            TextCellValue(p['departure_time']?.toString() ?? ''),
            DoubleCellValue((p['distance_from_prev'] as num?)?.toDouble() ?? 0),
          ]);
        }
      }
    } else {
      // Format A
      routeSheet.appendRow([
        TextCellValue('Vehicle ID'),
        TextCellValue('Trip #'),
        TextCellValue('Location'),
        TextCellValue('Arrival'),
        TextCellValue('Departure'),
        TextCellValue('Distance (km)'),
        TextCellValue('Wait (min)'),
      ]);

      List vehicles = data['vehicles'] ?? [];
      for (var veh in vehicles) {
        String vId = veh['vehicle_id']?.toString() ?? '';
        for (var trip in (veh['trips'] as List? ?? [])) {
          for (var stop in (trip['stops'] as List? ?? [])) {
            routeSheet.appendRow([
              TextCellValue(vId),
              IntCellValue(trip['trip_number'] ?? 1),
              TextCellValue(stop['location']?.toString() ?? ''),
              TextCellValue(stop['arrival_time']?.toString() ?? ''),
              TextCellValue(stop['departure_time']?.toString() ?? ''),
              DoubleCellValue(
                (stop['distance_from_prev'] as num?)?.toDouble() ?? 0,
              ),
              IntCellValue(stop['wait_time'] ?? 0),
            ]);
          }
        }
      }
    }

    // ── Sheet 3: Constraint Violations ──
    Sheet violSheet = excel['Violations'];
    final Map vd = data['violation_details'] ?? {};

    violSheet.appendRow([
      TextCellValue('Type'),
      TextCellValue('Severity'),
      TextCellValue('Vehicle'),
      TextCellValue('Employee'),
      TextCellValue('Trip'),
      TextCellValue('Details'),
    ]);

    for (var v in (vd['capacity_violations'] as List? ?? [])) {
      violSheet.appendRow([
        TextCellValue('Capacity'),
        TextCellValue('HARD'),
        TextCellValue(v['vehicle']?.toString() ?? ''),
        TextCellValue(v['employees']?.toString() ?? ''),
        IntCellValue(v['trip'] ?? 0),
        TextCellValue('${v['passengers']} pax / ${v['capacity']} capacity'),
      ]);
    }
    for (var v in (vd['time_window_violations'] as List? ?? [])) {
      violSheet.appendRow([
        TextCellValue('Time Window'),
        TextCellValue('HARD'),
        TextCellValue(v['vehicle']?.toString() ?? ''),
        TextCellValue(v['employee']?.toString() ?? ''),
        IntCellValue(v['trip'] ?? 0),
        TextCellValue(
          'Arrived ${v['office_arrival']}, deadline ${v['deadline']}, ${v['delay_min']} min late',
        ),
      ]);
    }
    for (var v in (vd['unassigned_employees'] as List? ?? [])) {
      violSheet.appendRow([
        TextCellValue('Unassigned'),
        TextCellValue('HARD'),
        TextCellValue(''),
        TextCellValue(v['employee']?.toString() ?? ''),
        IntCellValue(0),
        TextCellValue('Not assigned to any vehicle'),
      ]);
    }
    for (var v in (vd['vehicle_pref_violations'] as List? ?? [])) {
      violSheet.appendRow([
        TextCellValue('Vehicle Pref'),
        TextCellValue('SOFT'),
        TextCellValue(v['vehicle']?.toString() ?? ''),
        TextCellValue(v['employee']?.toString() ?? ''),
        IntCellValue(0),
        TextCellValue('Preferred ${v['preferred']}, got ${v['assigned']}'),
      ]);
    }
    for (var v in (vd['sharing_pref_violations'] as List? ?? [])) {
      violSheet.appendRow([
        TextCellValue('Sharing Pref'),
        TextCellValue('SOFT'),
        TextCellValue(v['vehicle']?.toString() ?? ''),
        TextCellValue(v['employee']?.toString() ?? ''),
        IntCellValue(v['trip'] ?? 0),
        TextCellValue(
          'Preferred ${v['preferred']}, trip has ${v['actual_riders']} riders',
        ),
      ]);
    }

    // ── Sheet 4: Metrics ──
    Sheet metricsSheet = excel['Metrics'];
    metricsSheet.appendRow([
      TextCellValue('Vehicle ID'),
      TextCellValue('Trips'),
      TextCellValue('Passengers'),
      TextCellValue('Distance (km)'),
      TextCellValue('Cost'),
      TextCellValue('Utilization %'),
    ]);

    if (isFormatB) {
      for (var route in (data['routes'] as List? ?? [])) {
        metricsSheet.appendRow([
          TextCellValue(route['vehicle_id']?.toString() ?? ''),
          IntCellValue(route['trips_count'] ?? 1),
          IntCellValue(route['passengers_count'] ?? 0),
          DoubleCellValue((route['total_distance'] as num?)?.toDouble() ?? 0),
          DoubleCellValue((route['total_cost'] as num?)?.toDouble() ?? 0),
          DoubleCellValue(
            (route['capacity_utilization'] as num?)?.toDouble() ?? 0,
          ),
        ]);
      }
    } else {
      for (var veh in (data['vehicles'] as List? ?? [])) {
        metricsSheet.appendRow([
          TextCellValue(veh['vehicle_id']?.toString() ?? ''),
          IntCellValue((veh['trips'] as List?)?.length ?? 0),
          IntCellValue(0), // Not easily derivable without parsing stops
          DoubleCellValue((veh['total_distance'] as num?)?.toDouble() ?? 0),
          DoubleCellValue((veh['total_cost'] as num?)?.toDouble() ?? 0),
          DoubleCellValue(0),
        ]);
      }
    }

    // Remove default Sheet1 if it exists
    if (excel.sheets.containsKey('Sheet1')) {
      excel.delete('Sheet1');
    }

    return excel.encode() ?? [];
  }

  // static so it can be passed to compute() and run in a background isolate
  static Future<List<int>> _generatePdf(Map<String, dynamic> data) async {
    // Load a real TrueType font so the document uses physically-embedded glyphs.
    // The default PDF built-in fonts (Helvetica/Times) are not embedded — many
    // Android PDF readers can't find them and render boxes instead.
    // PdfGoogleFonts downloads from Google CDN once, then caches to disk.
    final font = await PdfGoogleFonts.notoSansRegular();
    final fontBold = await PdfGoogleFonts.notoSansBold();

    final pdf = pw.Document(
      theme: pw.ThemeData.withFont(base: font, bold: fontBold),
    );

    // Helper: format a number to 2 decimal places
    String fmt(dynamic v) {
      if (v == null) return 'N/A';
      final n = num.tryParse(v.toString());
      return n != null ? n.toStringAsFixed(2) : v.toString();
    }

    final Map result = data['result'] ?? data['stats'] ?? {};
    final List routes = data['routes'] ?? data['vehicles'] ?? [];
    final Map vd = data['violation_details'] ?? {};
    final now = DateFormat('yyyy-MM-dd HH:mm:ss').format(DateTime.now());
    final isFormatB = data.containsKey('routes');

    // ── Helper: section title ──
    pw.Widget sectionTitle(String text) => pw.Padding(
      padding: const pw.EdgeInsets.only(top: 20, bottom: 8),
      child: pw.Text(
        text,
        style: pw.TextStyle(fontSize: 14, fontWeight: pw.FontWeight.bold),
      ),
    );

    pdf.addPage(
      pw.MultiPage(
        pageFormat: PdfPageFormat.a4,
        margin: const pw.EdgeInsets.all(32),
        header: (ctx) => pw.Row(
          mainAxisAlignment: pw.MainAxisAlignment.spaceBetween,
          children: [
            pw.Text(
              'Optimization Report',
              style: pw.TextStyle(
                fontSize: 12,
                fontWeight: pw.FontWeight.bold,
                color: PdfColors.grey700,
              ),
            ),
            pw.Text(
              'Generated: $now',
              style: const pw.TextStyle(fontSize: 9, color: PdfColors.grey500),
            ),
          ],
        ),
        footer: (ctx) => pw.Align(
          alignment: pw.Alignment.centerRight,
          child: pw.Text(
            'Page ${ctx.pageNumber} of ${ctx.pagesCount}',
            style: const pw.TextStyle(fontSize: 9, color: PdfColors.grey500),
          ),
        ),
        build: (pw.Context context) {
          return [
            // ── Title ──
            pw.Center(
              child: pw.Text(
                "Vehicle Route Optimization Report",
                style: pw.TextStyle(
                  fontSize: 24,
                  fontWeight: pw.FontWeight.bold,
                ),
              ),
            ),
            pw.SizedBox(height: 8),
            pw.Center(
              child: pw.Text(
                now,
                style: const pw.TextStyle(
                  fontSize: 12,
                  color: PdfColors.grey600,
                ),
              ),
            ),
            pw.SizedBox(height: 24),

            // ── Summary Statistics ──
            sectionTitle("Summary Statistics"),
            pw.Table.fromTextArray(
              context: context,
              headerStyle: pw.TextStyle(
                fontWeight: pw.FontWeight.bold,
                fontSize: 11,
              ),
              cellStyle: const pw.TextStyle(fontSize: 10),
              cellAlignment: pw.Alignment.centerLeft,
              data: <List<String>>[
                ['Metric', 'Value'],
                ['Total Cost', fmt(result['total_cost'] ?? data['cost'])],
                ['Total Distance', '${fmt(result['total_distance'])} km'],
                [
                  'Total Time',
                  '${fmt(result['total_time'] ?? data['total_time'])} min',
                ],
                [
                  'Vehicles Used',
                  '${result['vehicles_used'] ?? routes.length}',
                ],
                ['Hard Violations', '${result['hard_violations'] ?? 0}'],
                ['Soft Violations', '${result['soft_violations'] ?? 0}'],
                if (result['baseline_cost'] != null)
                  ['Baseline Cost', fmt(result['baseline_cost'])],
                if (result['cost_savings'] != null)
                  ['Cost Savings', fmt(result['cost_savings'])],
                if (result['cost_savings_percent'] != null)
                  ['Cost Savings %', '${fmt(result['cost_savings_percent'])}%'],
              ],
            ),

            // ── Optimized Routes ──
            sectionTitle("Optimized Routes"),
            ...routes.map((route) {
              final vid = route['vehicle_id']?.toString() ?? '';

              // Build stops list
              List<String> stopsList = [];
              if (isFormatB) {
                for (var p in (route['route_points'] as List? ?? [])) {
                  final type = p['type']?.toString().toUpperCase() ?? '';
                  final emp = p['employee_id']?.toString() ?? 'Office';
                  final arr = p['arrival_time']?.toString() ?? '';
                  stopsList.add('$type: $emp @ $arr');
                }
              } else {
                for (var trip in (route['trips'] as List? ?? [])) {
                  for (var stop in (trip['stops'] as List? ?? [])) {
                    stopsList.add(
                      '${stop['location']} (${stop['arrival_time'] ?? ''})',
                    );
                  }
                }
              }

              return pw.Container(
                margin: const pw.EdgeInsets.only(bottom: 12),
                padding: const pw.EdgeInsets.all(10),
                decoration: pw.BoxDecoration(
                  border: pw.Border.all(color: PdfColors.grey300),
                  borderRadius: pw.BorderRadius.circular(4),
                ),
                child: pw.Column(
                  crossAxisAlignment: pw.CrossAxisAlignment.start,
                  children: [
                    pw.Text(
                      'Vehicle: $vid  |  Cost: ${fmt(route['total_cost'])}  |  Dist: ${fmt(route['total_distance'])} km',
                      style: pw.TextStyle(
                        fontWeight: pw.FontWeight.bold,
                        fontSize: 10,
                      ),
                    ),
                    pw.SizedBox(height: 4),
                    ...stopsList.map(
                      (s) => pw.Padding(
                        padding: const pw.EdgeInsets.only(left: 10, top: 2),
                        child: pw.Text(
                          "- $s",
                          style: const pw.TextStyle(fontSize: 9),
                        ),
                      ),
                    ),
                  ],
                ),
              );
            }),

            // ── Violations ──
            if (_hasViolations(vd)) ...[
              sectionTitle("Constraint Violations"),
              _pdfViolationSection(
                'Capacity Violations',
                vd['capacity_violations'],
                (v) =>
                    'Vehicle ${v['vehicle']}, Trip ${v['trip']}: ${v['passengers']} pax / ${v['capacity']} capacity',
              ),
              _pdfViolationSection(
                'Time Window Violations',
                vd['time_window_violations'],
                (v) =>
                    'Employee ${v['employee']} on ${v['vehicle']}: arrived ${v['office_arrival']}, deadline ${v['deadline']} (${v['delay_min']} min late)',
              ),
              _pdfViolationSection(
                'Unassigned Employees',
                vd['unassigned_employees'],
                (v) => 'Employee ${v['employee']} not assigned',
              ),
              _pdfViolationSection(
                'Vehicle Preference',
                vd['vehicle_pref_violations'],
                (v) =>
                    '${v['employee']}: preferred ${v['preferred']}, assigned ${v['assigned']}',
              ),
              _pdfViolationSection(
                'Sharing Preference',
                vd['sharing_pref_violations'],
                (v) =>
                    '${v['employee']} on ${v['vehicle']}: preferred ${v['preferred']}, ${v['actual_riders']} riders',
              ),
            ] else ...[
              sectionTitle("Constraint Violations"),
              pw.Text(
                "No violations detected.",
                style: pw.TextStyle(fontSize: 11, color: PdfColors.green700),
              ),
            ],
          ];
        },
      ),
    );

    return pdf.save();
  }

  static bool _hasViolations(Map vd) {
    return (vd['capacity_violations'] as List?)?.isNotEmpty == true ||
        (vd['time_window_violations'] as List?)?.isNotEmpty == true ||
        (vd['unassigned_employees'] as List?)?.isNotEmpty == true ||
        (vd['vehicle_pref_violations'] as List?)?.isNotEmpty == true ||
        (vd['sharing_pref_violations'] as List?)?.isNotEmpty == true;
  }

  static pw.Widget _pdfViolationSection(
    String title,
    List? items,
    String Function(Map) formatter,
  ) {
    if (items == null || items.isEmpty) return pw.SizedBox.shrink();
    return pw.Column(
      crossAxisAlignment: pw.CrossAxisAlignment.start,
      children: [
        pw.Padding(
          padding: const pw.EdgeInsets.only(top: 8, bottom: 4),
          child: pw.Text(
            title,
            style: pw.TextStyle(fontSize: 12, fontWeight: pw.FontWeight.bold),
          ),
        ),
        ...items.map(
          (v) => pw.Padding(
            padding: const pw.EdgeInsets.only(left: 10, top: 2),
            child: pw.Text(
              "- ${formatter(v is Map ? v : {})}",
              style: const pw.TextStyle(fontSize: 9),
            ),
          ),
        ),
      ],
    );
  }
}
