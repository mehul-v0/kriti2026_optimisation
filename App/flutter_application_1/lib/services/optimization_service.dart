import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:flutter_application_1/config/env.dart';

class OptimizationService {
  /// triggers the optimization process on the backend.
  /// Requires [testCaseId] to tell the server which data to process.
  Future<Map<String, dynamic>> runOptimization(
    String testCaseId, {
    String mode = 'standard',
  }) async {
    try {
      final uri = Uri.parse(Env.optimizeEndpoint);

      // We pass the ID and mode in the body so the backend knows what to fetch and how to run
      final response = await http.post(
        uri,
        headers: {
          "Content-Type": "application/json",
          "Access-Control-Allow-Origin": "*",
        },
        body: jsonEncode({"test_case_id": testCaseId, "mode": mode}),
      );

      if (response.statusCode == 200) {
        return jsonDecode(response.body) as Map<String, dynamic>;
      } else {
        try {
          final errorBody = jsonDecode(response.body);
          throw Exception(
            errorBody['error'] ?? "Server Error: ${response.statusCode}",
          );
        } catch (_) {
          throw Exception("Optimization Failed: ${response.statusCode}");
        }
      }
    } catch (e) {
      throw Exception("Connection Error: $e");
    }
  }
}
