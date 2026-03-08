import 'package:flutter/foundation.dart';
import 'package:supabase_flutter/supabase_flutter.dart';
import 'package:flutter_application_1/config/secret.dart';

class SupabaseConfig {
  static Future<void> initialize() async {
    await Supabase.initialize(
      url: AppSecrets.supabaseUrl,
      anonKey: AppSecrets.supabaseAnonKey,
    );
  }
}

class AuthService {
  final SupabaseClient _supabase = Supabase.instance.client;

  // Sign Up Logic
  Future<void> signUp(String email, String password) async {
    try {
      final response = await _supabase.auth.signUp(
        email: email,
        password: password,
      );
      if (response.user == null) {
        throw Exception("Sign up failed.");
      }
    } on AuthException catch (e) {
      // Common Supabase error: "User already registered"
      throw Exception(e.message);
    } catch (e) {
      throw Exception("Connection error. Please try again.");
    }
  }

  // Sign In Logic with specific Error Parsing
  Future<void> signIn(String email, String password) async {
    try {
      final response = await _supabase.auth.signInWithPassword(
        email: email,
        password: password,
      );

      if (response.session == null) {
        throw Exception("Login failed: No session created.");
      }
    } on AuthException catch (e) {
      // Print the full error to the console for your debugging
      debugPrint("Supabase Auth Error: ${e.message} (Status: ${e.statusCode})");

      if (e.message.toLowerCase().contains("invalid login credentials")) {
        throw Exception("Wrong password or email not found.");
      } else if (e.statusCode == '400') {
        throw Exception("Invalid request. Check your email format.");
      } else {
        throw Exception(e.message);
      }
    } catch (e) {
      throw Exception("Check your internet connection.");
    }
  }

  Future<void> signOut() async {
    await _supabase.auth.signOut();
  }

  User? get currentUser => _supabase.auth.currentUser;

  // Useful for the AuthGate mentioned previously
  Stream<AuthState> get authStateChanges => _supabase.auth.onAuthStateChange;
}
