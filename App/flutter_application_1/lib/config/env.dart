import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter_application_1/config/secret.dart';

class Env {
  // Live deployment URL — loaded from secret.dart (gitignored).
  // To change: update AppSecrets.liveServerUrl in lib/config/secret.dart
  static String get _liveUrl => AppSecrets.liveServerUrl;

  // Local development URLs
  // NOTE: For Physical Android Device via USB run: adb reverse tcp:5000 tcp:5000
  static const String _androidLocal = 'http://127.0.0.1:5000';
  static const String _webLocal = 'http://127.0.0.1:5000';

  // Set to false for local development
  static const bool useLiveServer = true;

  static String get baseUrl {
    if (useLiveServer) return _liveUrl;

    if (kIsWeb) {
      return _webLocal;
    } else if (Platform.isAndroid) {
      return _androidLocal;
    } else {
      return _webLocal;
    }
  }

  static String get optimizeEndpoint => '$baseUrl/api/optimize';
}