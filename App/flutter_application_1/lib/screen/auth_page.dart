import 'package:flutter/material.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/elements/spinner.dart';
import 'package:flutter_application_1/services/auth_service.dart';

class AuthPage extends StatefulWidget {
  const AuthPage({super.key});

  @override
  State<AuthPage> createState() => _AuthPageState();
}

class _AuthPageState extends State<AuthPage> {
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();
  final _confirmPasswordController = TextEditingController();
  final _formKey = GlobalKey<FormState>();

  bool _isLoading = false;
  bool _isLogin = true;
  final AuthService _authService = AuthService();

  void _toggleAuthMode() {
    setState(() {
      _isLogin = !_isLogin;
      _formKey.currentState?.reset();
      _emailController.clear();
      _passwordController.clear();
      _confirmPasswordController.clear();
    });
  }

  Future<void> _handleSubmit() async {
    if (!_formKey.currentState!.validate()) return;

    // Remove focus to hide keyboard and prevent layout jumps
    FocusScope.of(context).unfocus();

    setState(() => _isLoading = true);

    try {
      if (_isLogin) {
        await _authService.signIn(
          _emailController.text.trim(),
          _passwordController.text.trim(),
        );
        if (mounted) AppSnackbar.show(context, message: "Welcome back!");
      } else {
        await _authService.signUp(
          _emailController.text.trim(),
          _passwordController.text.trim(),
        );
        if (mounted)
          AppSnackbar.show(context, message: "Account created successfully!");
      }

      // Navigate to your Home page here if NOT using an AuthGate
    } on Exception catch (e) {
      if (mounted) {
        AppSnackbar.show(context, message: e.toString(), isError: true);
      }
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  @override
  void dispose() {
    _emailController.dispose();
    _passwordController.dispose();
    _confirmPasswordController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;
    // Constraints for Tablet/PC (Max width 450)
    final double cardWidth = size.width > 600 ? 450 : size.width * 0.85;

    return Scaffold(
      body: LoadingOverlay(
        isLoading: _isLoading,
        child: Center(
          child: SingleChildScrollView(
            padding: EdgeInsets.symmetric(horizontal: size.width * 0.05),
            child: Container(
              width: cardWidth,
              padding: const EdgeInsets.all(24),
              decoration: BoxDecoration(
                color: Theme.of(context).colorScheme.surface,
                borderRadius: BorderRadius.circular(16),
                boxShadow: [
                  if (size.width > 600)
                    BoxShadow(
                      color: Colors.black.withOpacity(0.05),
                      blurRadius: 20,
                      spreadRadius: 5,
                    ),
                ],
              ),
              child: Form(
                key: _formKey,
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(
                      _isLogin ? "Sign In" : "Register",
                      style: Theme.of(context).textTheme.headlineLarge
                          ?.copyWith(fontSize: size.width > 600 ? 32 : 26),
                    ),
                    SizedBox(height: size.height * 0.04),

                    TextFormField(
                      controller: _emailController,
                      decoration: const InputDecoration(
                        labelText: "Email Address",
                        prefixIcon: Icon(Icons.alternate_email),
                      ),
                      keyboardType: TextInputType.emailAddress,
                      validator: (val) => (val != null && val.contains('@'))
                          ? null
                          : "Enter a valid email",
                    ),
                    SizedBox(height: size.height * 0.02),

                    TextFormField(
                      controller: _passwordController,
                      decoration: const InputDecoration(
                        labelText: "Password",
                        prefixIcon: Icon(Icons.lock_outline),
                      ),
                      obscureText: true,
                      validator: (val) => (val != null && val.length >= 6)
                          ? null
                          : "Password must be at least 6 characters",
                    ),

                    if (!_isLogin) ...[
                      SizedBox(height: size.height * 0.02),
                      TextFormField(
                        controller: _confirmPasswordController,
                        decoration: const InputDecoration(
                          labelText: "Confirm Password",
                          prefixIcon: Icon(Icons.check_circle_outline),
                        ),
                        obscureText: true,
                        validator: (val) => val == _passwordController.text
                            ? null
                            : "Passwords do not match",
                      ),
                    ],

                    SizedBox(height: size.height * 0.04),

                    SizedBox(
                      width: double.infinity,
                      height: 50,
                      child: ElevatedButton(
                        onPressed: _handleSubmit,
                        child: Text(_isLogin ? "LOGIN" : "CREATE ACCOUNT"),
                      ),
                    ),

                    SizedBox(height: size.height * 0.02),

                    TextButton(
                      onPressed: _toggleAuthMode,
                      child: Text(
                        _isLogin
                            ? "New here? Create an account"
                            : "Already have an account? Sign in",
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
