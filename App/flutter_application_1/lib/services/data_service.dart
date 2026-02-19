import 'package:supabase_flutter/supabase_flutter.dart';

class DataService {
  final SupabaseClient _supabase = Supabase.instance.client;

  // Fetch all test cases
  Future<List<Map<String, dynamic>>> fetchTestCases() async {
    try {
      final response = await _supabase
          .from('test_cases')
          .select('id, case_name, created_at, input_data')
          .order('created_at', ascending: false);

      return List<Map<String, dynamic>>.from(response);
    } catch (e) {
      throw Exception('Error fetching cases: $e');
    }
  }

  // Upload a new test case
  Future<void> uploadTestCase(
    String name,
    Map<String, dynamic> jsonData,
  ) async {
    final user = _supabase.auth.currentUser;
    if (user == null) throw Exception("User not logged in");

    await _supabase.from('test_cases').insert({
      'user_id': user.id,
      'case_name': name,
      'input_data': jsonData,
      // output_json is null by default in DB, so we don't send it here
    });
  }

  // Delete a test case
  Future<void> deleteTestCase(String id) async {
    await _supabase.from('test_cases').delete().eq('id', id);
  }

  // Rename a test case
  Future<void> renameTestCase(String id, String newName) async {
    await _supabase
        .from('test_cases')
        .update({'case_name': newName})
        .eq('id', id);
  }

  // --- NEW: Solution Handling ---

  // Fetch existing solution (if any)
  Future<Map<String, dynamic>?> fetchSolution(String id) async {
    try {
      final response = await _supabase
          .from('test_cases')
          .select('output_json')
          .eq('id', id)
          .single();

      if (response['output_json'] == null) return null;
      return response['output_json'] as Map<String, dynamic>;
    } catch (e) {
      return null;
    }
  }

  // Save (Update) solution
  Future<void> saveSolution(String id, Map<String, dynamic> solution) async {
    await _supabase
        .from('test_cases')
        .update({'output_json': solution})
        .eq('id', id);
  }
}
