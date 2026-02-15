import 'dart:convert';
import 'dart:io';
import 'package:excel/excel.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:path_provider/path_provider.dart';
import 'package:pdf/pdf.dart';
import 'package:pdf/widgets.dart' as pw;
import 'package:permission_handler/permission_handler.dart';
import 'package:device_info_plus/device_info_plus.dart'; // Add this package

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
        // If permission denied, show a specific error or open settings
        AppSnackbar.show(
          context,
          message:
              "Storage permission is required to save files. Please enable it in Settings.",
          isError: true,
        );
        await openAppSettings(); // Guide user to settings
        return;
      }

      List<int> bytes = [];
      String extension = "";

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
          bytes = await _generatePdf(data);
          extension = "pdf";
          break;
      }

      final String filePath = await _saveToDownloads(
        bytes,
        fileName,
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

    Sheet sheet1 = excel['Summary'];
    Map stats = data['result'] ?? {};

    // REMOVED 'const'
    sheet1.appendRow([TextCellValue('Metric'), TextCellValue('Value')]);

    sheet1.appendRow([
      TextCellValue('Total Cost'),
      DoubleCellValue((stats['total_cost'] ?? 0).toDouble()),
    ]);
    sheet1.appendRow([
      TextCellValue('Total Time'),
      DoubleCellValue((stats['total_time'] ?? 0).toDouble()),
    ]);
    sheet1.appendRow([
      TextCellValue('Total Distance'),
      DoubleCellValue((stats['total_distance'] ?? 0).toDouble()),
    ]);
    sheet1.appendRow([
      TextCellValue('Vehicles Used'),
      IntCellValue((stats['vehicles_used'] ?? 0)),
    ]);

    Sheet sheet2 = excel['Routes'];

    // REMOVED 'const'
    sheet2.appendRow([
      TextCellValue('Vehicle ID'),
      TextCellValue('Trip #'),
      TextCellValue('Type'),
      TextCellValue('ID'),
      TextCellValue('Time'),
    ]);

    List routes = data['routes'] ?? [];
    for (var route in routes) {
      String vId = route['vehicle_id'];
      List points = route['route_points'] ?? [];
      for (var p in points) {
        sheet2.appendRow([
          TextCellValue(vId),
          IntCellValue(p['trip_number'] ?? 1),
          TextCellValue(p['type']),
          TextCellValue(p['employee_id'] ?? 'Office'),
          TextCellValue(p['pickup_time'] ?? ''),
        ]);
      }
    }

    return excel.encode() ?? [];
  }

  Future<List<int>> _generatePdf(Map<String, dynamic> data) async {
    final pdf = pw.Document();
    final Map result = data['result'] ?? {};
    final List routes = data['routes'] ?? [];

    pdf.addPage(
      pw.MultiPage(
        pageFormat: PdfPageFormat.a4,
        build: (pw.Context context) {
          return [
            pw.Header(
              level: 0,
              child: pw.Text(
                "Optimization Report",
                style: pw.TextStyle(
                  fontSize: 24,
                  fontWeight: pw.FontWeight.bold,
                ),
              ),
            ),
            pw.SizedBox(height: 20),

            pw.Text(
              "Summary Statistics",
              style: pw.TextStyle(fontSize: 18, fontWeight: pw.FontWeight.bold),
            ),
            pw.SizedBox(height: 10),
            pw.Table.fromTextArray(
              context: context,
              data: <List<String>>[
                <String>['Metric', 'Value'],
                <String>['Total Cost', result['total_cost'].toString()],
                <String>[
                  'Savings %',
                  result['cost_savings_percent'].toString(),
                ],
                <String>['Vehicles Used', result['vehicles_used'].toString()],
                <String>['Total Distance', result['total_distance'].toString()],
              ],
            ),

            pw.SizedBox(height: 30),

            pw.Text(
              "Optimized Routes",
              style: pw.TextStyle(fontSize: 18, fontWeight: pw.FontWeight.bold),
            ),
            pw.SizedBox(height: 10),
            ...routes.map((route) {
              return pw.Container(
                margin: const pw.EdgeInsets.only(bottom: 10),
                child: pw.Column(
                  crossAxisAlignment: pw.CrossAxisAlignment.start,
                  children: [
                    pw.Text(
                      "Vehicle: ${route['vehicle_id']} (Cost: ${route['total_cost']})",
                      style: pw.TextStyle(fontWeight: pw.FontWeight.bold),
                    ),
                    pw.Padding(
                      padding: const pw.EdgeInsets.only(left: 10),
                      child: pw.Column(
                        crossAxisAlignment: pw.CrossAxisAlignment.start,
                        children: (route['route_points'] as List).map((p) {
                          return pw.Text(
                            "- ${p['type'].toUpperCase()}: ${p['employee_id'] ?? 'Office'}",
                          );
                        }).toList(),
                      ),
                    ),
                  ],
                ),
              );
            }).toList(),
          ];
        },
      ),
    );

    return pdf.save();
  }
}
