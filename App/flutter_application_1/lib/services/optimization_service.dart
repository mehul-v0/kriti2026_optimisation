import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:flutter_application_1/config/env.dart';

class OptimizationService {
  /// Triggers the optimization process on the backend.
  ///
  /// Sends the input JSON data directly to /api/optimize endpoint.
  /// The backend processes the JSON and returns optimized routes.
  ///
  /// [inputData] - The exact JSON from Supabase input_data column
  /// [mode] - Optimization mode (standard, quick, thorough, etc.)
  /// [solverDurationSeconds] - Solver time limit in seconds
  /// [costWeight] - Weight for cost in objective function (0.0 to 1.0)
  /// [timeWeight] - Weight for time in objective function (0.0 to 1.0)
  Future<Map<String, dynamic>> runOptimization(
    Map<String, dynamic> inputData, {
    String mode = 'standard',
    int? solverDurationSeconds,
    double? costWeight,
    double? timeWeight,
    Map<int, int>? priorityDelays,
    String? distanceMethod,
  }) async {
    try {
      final uri = Uri.parse(Env.optimizeEndpoint);

      // Build request body with input data and optimization parameters
      final requestBody = {
        ...inputData, // Spread the input data
        // Add optimization config parameters
        if (solverDurationSeconds != null)
          'solverDurationSeconds': solverDurationSeconds,
        if (costWeight != null) 'costWeight': costWeight,
        if (timeWeight != null) 'timeWeight': timeWeight,
        if (priorityDelays != null) 'priorityDelays': priorityDelays,
        if (distanceMethod != null) 'distanceMethod': distanceMethod,
        'mode': mode,
      };

      final response = await http.post(
        uri,
        headers: {
          "Content-Type": "application/json",
          "Access-Control-Allow-Origin": "*",
        },
        body: jsonEncode(requestBody),
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
      if (e is Exception) rethrow;
      throw Exception("Connection Error: $e");
    }
  }
}
