import 'dart:io';
import 'package:flutter/foundation.dart';

class Env {
  // CHANGE THIS when deploying to Render/Supabase
  static const String _liveUrl =
      "https://i80owo0o8wo0swkkcw888ws4.65.21.154.72.sslip.io/";

  // Localhost setup
  // NOTE: For Physical Device via USB, you MUST run: adb reverse tcp:5000 tcp:5000
  static const String _androidLocal = "http://127.0.0.1:5000";
  static const String _webLocal = "http://127.0.0.1:5000";

  // Set this to true to force using the live URL
  static const bool useLiveServer = true;

  static String get baseUrl {
    if (useLiveServer) return _liveUrl;

    if (kIsWeb) {
      return _webLocal;
    } else if (Platform.isAndroid) {
      // On physical device with 'adb reverse', 127.0.0.1 works.
      // On emulator, 10.0.2.2 is standard, but 127.0.0.1 also works usually if mapped.
      // For safety with physical USB: use 127.0.0.1
      return _androidLocal;
    } else {
      return _webLocal; // iOS or Desktop
    }
  }

  static String get optimizeEndpoint => "$baseUrl/api/optimize";
}



/***********************
cd C:\Users\Aditya\AppData\Local\Android\sdk\platform-tools
.\adb devices
.\adb reverse tcp:5000 tcp:5000
 ***********************/