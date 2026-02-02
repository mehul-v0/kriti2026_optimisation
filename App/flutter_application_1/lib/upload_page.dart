import 'dart:convert';
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:file_picker/file_picker.dart';
import 'package:http/http.dart' as http;
import 'package:http_parser/http_parser.dart'; // REQUIRED NOW
import 'output_page.dart';

class UploadPage extends StatefulWidget {
  @override
  _UploadPageState createState() => _UploadPageState();
}

class _UploadPageState extends State<UploadPage> {
  bool _isProcessing = false;
  String? _errorMsg;

  Future<void> _handleUpload() async {
    setState(() {
      _errorMsg = null;
    });

    try {
      FilePickerResult? result = await FilePicker.platform.pickFiles(
        type: FileType.custom,
        allowedExtensions: ['xlsx', 'xls'],
        withData: true, // CRITICAL FOR WEB
      );

      if (result == null) return;

      setState(() => _isProcessing = true);

      // 1. Define URL (Localhost for Web)
      String serverUrl = kIsWeb
          ? 'http://localhost:3000/process-vrp'
          : 'http://192.168.1.5:3000/process-vrp';

      var request = http.MultipartRequest('POST', Uri.parse(serverUrl));

      // 2. Add File with EXPLICIT Content-Type
      if (kIsWeb) {
        if (result.files.single.bytes != null) {
          request.files.add(
            http.MultipartFile.fromBytes(
              'excelFile',
              result.files.single.bytes!,
              filename: 'upload.xlsx', // Force a simple name
              contentType: MediaType(
                'application',
                'vnd.openxmlformats-officedocument.spreadsheetml.sheet',
              ),
            ),
          );
        }
      } else {
        if (result.files.single.path != null) {
          request.files.add(
            await http.MultipartFile.fromPath(
              'excelFile',
              result.files.single.path!,
            ),
          );
        }
      }

      print("Attempting connection to: $serverUrl");

      // 3. Send
      var streamedResponse = await request.send();
      var response = await http.Response.fromStream(streamedResponse);

      print("Response Code: ${response.statusCode}");
      print("Response Body: ${response.body}");

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (mounted) {
          Navigator.push(
            context,
            MaterialPageRoute(builder: (context) => OutputPage(data: data)),
          );
        }
      } else {
        setState(
          () => _errorMsg =
              "Server Error (${response.statusCode}):\n${response.body}",
        );
      }
    } catch (e) {
      setState(() => _errorMsg = "Connection Error: $e");
      print("Detailed Error: $e");
    } finally {
      if (mounted) setState(() => _isProcessing = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            if (_isProcessing) CircularProgressIndicator(),
            SizedBox(height: 20),
            ElevatedButton(
              onPressed: _isProcessing ? null : _handleUpload,
              child: Text("Upload Excel File"),
            ),
            if (_errorMsg != null)
              Padding(
                padding: EdgeInsets.all(20),
                child: Text(_errorMsg!, style: TextStyle(color: Colors.red)),
              ),
          ],
        ),
      ),
    );
  }
}
