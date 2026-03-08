import 'package:flutter/material.dart';
import 'package:flutter_application_1/screen/home_page.dart';
import 'package:supabase_flutter/supabase_flutter.dart';
import 'auth_page.dart';

class AuthGate extends StatelessWidget {
  final ValueNotifier<ThemeMode>? themeNotifier;
  final ValueNotifier<int>? themeIndexNotifier;

  const AuthGate({super.key, this.themeNotifier, this.themeIndexNotifier});

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<AuthState>(
      stream: Supabase.instance.client.auth.onAuthStateChange,
      builder: (context, snapshot) {
        final session = snapshot.data?.session;
        if (session != null) {
          return HomePage(
            themeNotifier: themeNotifier,
            themeIndexNotifier: themeIndexNotifier,
          );
        } else {
          return const AuthPage();
        }
      },
    );
  }
}
