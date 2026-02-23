# Quick Reference: Upload & Optimize Flows

## 📤 Upload Flow (Excel/Google Sheets → Supabase)

```dart
// 1. Get file bytes (Excel or exported Google Sheet)
final bytes = await FilePicker.platform.pickFiles(...);

// 2. Send to backend via multipart/form-data
final response = await UploadService().uploadExcelFile(bytes, fileName);
// Backend returns: { employees: [...], vehicles: [...], metadata: {...}, baseline: [...] }

// 3. Store JSON in Supabase (no file bytes!)
final inputJson = {
  "employees": response["employees"] ?? [],
  "vehicles": response["vehicles"] ?? [],
  "metadata": response["metadata"] ?? {},
  "baseline": response["baseline"] ?? [],
};
await DataService().uploadTestCase(caseName, inputJson);
```

**Key Points:**
- ✅ Send file to `/api/upload` with `multipart/form-data`
- ✅ Backend converts Excel → JSON
- ✅ Store only JSON in `input_data` column
- ❌ Do NOT store file bytes
- ❌ Do NOT modify returned JSON

---

## ⚡ Optimize Flow (Supabase → Backend → Supabase)

```dart
// 1. Fetch input JSON from Supabase
final inputData = await DataService().fetchInputData(testCaseId);
// Returns: { employees: [...], vehicles: [...], metadata: {...}, baseline: [...] }

// 2. Send JSON to backend optimization endpoint
final result = await OptimizationService().runOptimization(inputData);
// Backend returns: { routes: [...], result: {...}, assignments: [...] }

// 3. Store results in Supabase
await DataService().saveSolution(testCaseId, result);
```

**Key Points:**
- ✅ Fetch JSON from `input_data` column
- ✅ Send exact JSON to `/api/optimize`
- ✅ Use `Content-Type: application/json` header
- ❌ Do NOT re-upload Excel file
- ❌ Do NOT modify input JSON
- ❌ Do NOT send test_case_id instead of JSON

---

## 🔧 Troubleshooting: 400 Bad Request Errors

### Error: Backend returns 400 during optimization

**Possible Causes:**

1. **Missing Content-Type header**
   ```dart
   // Wrong
   headers: {}
   
   // Correct
   headers: {"Content-Type": "application/json"}
   ```

2. **Sending test_case_id instead of JSON**
   ```dart
   // Wrong
   body: jsonEncode({"test_case_id": id})
   
   // Correct
   body: jsonEncode(inputData)
   ```

3. **Modified JSON structure**
   ```dart
   // Wrong - wrapping in extra layer
   final json = {"data": backendResponse};
   
   // Correct - exact backend format
   final json = {
     "employees": backendResponse["employees"],
     "vehicles": backendResponse["vehicles"],
     // ...
   };
   ```

4. **Re-uploading file before optimization**
   ```dart
   // Wrong workflow
   await uploadFile(bytes);
   await optimize();
   
   // Correct workflow
   await optimize(inputData);
   ```

---

## 📊 Data Flow Summary

```
Excel/Sheet → Backend /api/upload → JSON → Supabase input_data
                                              ↓
Supabase input_data → JSON → Backend /api/optimize → Results → Supabase output_json
```

**Critical:** JSON never leaves Supabase except to go to backend `/api/optimize`

---

## 🎯 File Responsibilities

| File | Responsibility |
|------|----------------|
| `upload_service.dart` | Upload files to backend `/api/upload` |
| `optimization_service.dart` | Send JSON to backend `/api/optimize` |
| `data_service.dart` | Store/fetch JSON from Supabase |
| `add_test_case_dialog.dart` | Handle upload UI and flow |
| `show_input_page.dart` | Trigger optimization |
| `output_page.dart` | Display results, retry optimization |

---

## ⚠️ What NOT to Do

1. ❌ Do NOT add `file_base64` or `file_name` columns
2. ❌ Do NOT store file bytes in database
3. ❌ Do NOT modify backend code
4. ❌ Do NOT change Supabase schema
5. ❌ Do NOT re-upload files during optimization
6. ❌ Do NOT modify JSON from backend
7. ❌ Do NOT send test_case_id to `/api/optimize`

---

## ✅ What TO Do

1. ✅ Use existing `input_data` and `output_json` columns
2. ✅ Send files to `/api/upload` during upload only
3. ✅ Send JSON to `/api/optimize` during optimization
4. ✅ Store backend JSON exactly as-is
5. ✅ Use correct headers (`Content-Type: application/json`)
6. ✅ Let backend be the source of truth for schema

---

**Remember:** Frontend is a **pass-through layer**. Backend owns the schema.
