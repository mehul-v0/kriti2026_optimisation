import 'dart:io';
import 'package:flutter/material.dart';
import 'package:pdf/pdf.dart';
import 'package:pdf/widgets.dart' as pw;
import 'package:path_provider/path_provider.dart';
import 'package:open_file/open_file.dart';

class OutputPage extends StatelessWidget {
  final Map<String, dynamic> data;

  const OutputPage({Key? key, required this.data}) : super(key: key);

  Future<void> _generatePdf(BuildContext context) async {
    try {
      final pdf = pw.Document();
      final routes = data['routes'] as List;

      pdf.addPage(
        pw.MultiPage(
          build: (context) => [
            pw.Header(level: 0, child: pw.Text("Optimization Results")),
            pw.SizedBox(height: 20),
            pw.Text(
              "Total Cost: \$${data['total_fleet_cost']?.toStringAsFixed(2) ?? '0.00'}",
            ),
            pw.Text("Employees Served: ${data['total_employees_served']}"),
            pw.SizedBox(height: 20),
            ...routes.map((r) {
              return pw.Container(
                margin: const pw.EdgeInsets.only(bottom: 10),
                child: pw.Column(
                  crossAxisAlignment: pw.CrossAxisAlignment.start,
                  children: [
                    pw.Text(
                      "Vehicle: ${r['vehicle_id']} (${r['category']})",
                      style: pw.TextStyle(fontWeight: pw.FontWeight.bold),
                    ),
                    pw.Bullet(text: "Total Stops: ${r['stops'].length}"),
                    pw.Bullet(
                      text: "Route Cost: ${r['total_cost'].toStringAsFixed(2)}",
                    ),
                  ],
                ),
              );
            }).toList(),
          ],
        ),
      );

      final output = await getTemporaryDirectory();
      final file = File("${output.path}/velora_optimized.pdf");
      await file.writeAsBytes(await pdf.save());

      // Open PDF
      await OpenFile.open(file.path);
    } catch (e) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text("Error generating PDF: $e")));
    }
  }

  @override
  Widget build(BuildContext context) {
    // --- MEDIA QUERY ---
    final size = MediaQuery.of(context).size;
    final isDesktop = size.width > 800;

    final routes = data['routes'] as List? ?? [];
    final unassigned = data['unassigned'] as List? ?? [];

    return Scaffold(
      appBar: AppBar(
        title: Text("Optimization Results"),
        backgroundColor: Colors.blue[800],
        actions: [
          IconButton(
            icon: Icon(Icons.download),
            onPressed: () => _generatePdf(context),
            tooltip: "Download Report",
          ),
        ],
      ),
      body: routes.isEmpty && unassigned.isEmpty
          ? Center(child: Text("Empty Output: No valid routes found."))
          : Row(
              children: [
                // Main Content
                Expanded(
                  child: ListView(
                    padding: EdgeInsets.all(isDesktop ? 32 : 16),
                    children: [
                      // Stats Card
                      _buildStatsCard(data),
                      SizedBox(height: 20),
                      Text(
                        "Active Routes",
                        style: TextStyle(
                          fontSize: 20,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 10),

                      // Route List
                      ...routes
                          .map(
                            (r) => Card(
                              margin: EdgeInsets.only(bottom: 12),
                              elevation: 3,
                              child: ListTile(
                                leading: CircleAvatar(
                                  backgroundColor: Colors.green[100],
                                  child: Icon(
                                    Icons.local_shipping,
                                    color: Colors.green[800],
                                  ),
                                ),
                                title: Text(
                                  "${r['vehicle_id']} - ${r['category'].toString().toUpperCase()}",
                                ),
                                subtitle: Text(
                                  "Load: ${r['current_load']}/${r['capacity']} | Cost: \$${r['total_cost'].toStringAsFixed(2)}",
                                ),
                                trailing: Icon(
                                  Icons.arrow_forward_ios,
                                  size: 16,
                                ),
                              ),
                            ),
                          )
                          .toList(),

                      if (unassigned.isNotEmpty) ...[
                        SizedBox(height: 20),
                        Text(
                          "Unassigned Employees",
                          style: TextStyle(
                            fontSize: 20,
                            fontWeight: FontWeight.bold,
                            color: Colors.red,
                          ),
                        ),
                        Card(
                          color: Colors.red[50],
                          child: Padding(
                            padding: EdgeInsets.all(16),
                            child: Text(unassigned.join(", ")),
                          ),
                        ),
                      ],
                    ],
                  ),
                ),
              ],
            ),
    );
  }

  Widget _buildStatsCard(Map<String, dynamic> data) {
    return Card(
      color: Colors.blue[50],
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceAround,
          children: [
            _statItem(
              "Total Cost",
              "\$${data['total_fleet_cost']?.toStringAsFixed(2) ?? 0}",
            ),
            _statItem("Served", "${data['total_employees_served']}"),
            _statItem("Unassigned", "${(data['unassigned'] as List).length}"),
          ],
        ),
      ),
    );
  }

  Widget _statItem(String label, String value) {
    return Column(
      children: [
        Text(
          value,
          style: TextStyle(
            fontSize: 22,
            fontWeight: FontWeight.bold,
            color: Colors.blue[900],
          ),
        ),
        Text(label, style: TextStyle(color: Colors.blue[700])),
      ],
    );
  }
}
