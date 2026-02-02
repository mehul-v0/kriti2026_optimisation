import 'package:supabase_flutter/supabase_flutter.dart';

class DataService {
  final SupabaseClient _supabase = Supabase.instance.client;

  // Fetch all test cases for the current user
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
    });
  }

  // Delete a test case
  Future<void> deleteTestCase(String id) async {
    await _supabase.from('test_cases').delete().eq('id', id);
  }
}
