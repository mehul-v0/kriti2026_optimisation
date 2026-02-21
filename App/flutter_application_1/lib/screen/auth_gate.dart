import 'package:flutter/material.dart';
import 'package:flutter_application_1/screen/home_page.dart';
import 'package:supabase_flutter/supabase_flutter.dart';
import 'auth_page.dart';

class AuthGate extends StatelessWidget {
  final ValueNotifier<ThemeMode>? themeNotifier;

  const AuthGate({super.key, this.themeNotifier});

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<AuthState>(
      // Listens to Supabase auth changes
      stream: Supabase.instance.client.auth.onAuthStateChange,
      builder: (context, snapshot) {
        final session = snapshot.data?.session;

        // If we have a session, go to Home. Otherwise, stay on Auth.
        if (session != null) {
          return HomePage(themeNotifier: themeNotifier);
        } else {
          return const AuthPage();
        }
      },
    );
  }
}
