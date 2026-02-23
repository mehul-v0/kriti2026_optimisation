# Flutter Frontend Architecture - Corrected Implementation

## 🎯 Overview

This document explains the **correct** frontend architecture that works with the existing backend without any modifications.

**Key Principle**: Frontend acts as a **pass-through layer** for JSON. Backend is the single source of truth for schema.

---

## 📊 Supabase Database Schema

**Table:** `test_cases`

```sql
create table public.test_cases (
  id uuid not null default gen_random_uuid(),
  created_at timestamp with time zone not null default now(),
  user_id uuid not null default auth.uid(),
  case_name text not null,
  input_data jsonb not null,       -- Backend-generated JSON stored as-is
  output_json jsonb null,           -- Optimization results
  constraint test_cases_pkey primary key (id),
  constraint test_cases_user_id_fkey foreign key (user_id) references auth.users (id)
);
```

**❌ NO additional columns** (no file_base64, no file_name, etc.)

---

## 🔄 Upload Flow

### Step-by-Step Process:

1. **User uploads Excel or provides Google Sheet link**
   - Excel: Use FilePicker to get file bytes
   - Google Sheets: Export to .xlsx format first

2. **Frontend sends file to backend `/api/upload`**
   ```dart
   // Using multipart/form-data
   var request = http.MultipartRequest('POST', uri);
   request.files.add(
     http.MultipartFile.fromBytes('file', fileBytes, filename: fileName),
   );
   ```

3. **Backend converts Excel → JSON**
   - Backend reads Excel sheets (employees, vehicles, metadata, baseline)
   - Converts to standardized JSON schema
   - Returns JSON in response

4. **Frontend receives backend response**
   ```json
   {
     "success": true,
     "message": "Parsed X employees and Y vehicles",
     "employees": [...],
     "vehicles": [...],
     "metadata": {...},
     "baseline": [...],
     "baseline_cost": 1234.56
   }
   ```

5. **Frontend stores JSON exactly as-is**
   ```dart
   final inputJson = {
     "employees": backendResponse["employees"] ?? [],
     "vehicles": backendResponse["vehicles"] ?? [],
     "metadata": backendResponse["metadata"] ?? {},
     "baseline": backendResponse["baseline"] ?? [],
   };
   
   await _dataService.uploadTestCase(caseName, inputJson);
   ```

   **Critical:** Store only the JSON. Do NOT store file bytes.

### Implementation Files:

**[upload_service.dart](lib/services/upload_service.dart)**
- `uploadExcelFile()` - Sends Excel to backend
- `uploadGoogleSheet()` - Exports Sheet → Excel → sends to backend
- `exportGoogleSheetAsBytes()` - Downloads Google Sheet as Excel

**[add_test_case_dialog.dart](lib/widgets/add_test_case_dialog.dart)**
- Handles UI for Excel/Google Sheets upload
- Calls upload service
- Stores returned JSON in Supabase

**[data_service.dart](lib/services/data_service.dart)**
- `uploadTestCase()` - Stores JSON in `input_data` column

---

## ⚡ Optimize Flow

### Step-by-Step Process:

1. **User clicks "Optimize" button**

2. **Frontend fetches input JSON from Supabase**
   ```dart
   final inputData = await _dataService.fetchInputData(testCaseId);
   ```

3. **Frontend sends JSON directly to `/api/optimize`**
   ```dart
   final response = await http.post(
     uri,
     headers: {
       "Content-Type": "application/json",
     },
     body: jsonEncode(inputData),
   );
   ```

   **Critical Headers:**
   - `Content-Type: application/json` ← Must be set correctly
   - Send the exact JSON from Supabase without modification

4. **Backend processes optimization**
   - Reads JSON from request body
   - Runs C++ solver
   - Returns optimized routes

5. **Frontend stores results**
   ```dart
   await _dataService.saveSolution(testCaseId, resultData);
   ```

### Implementation Files:

**[optimization_service.dart](lib/services/optimization_service.dart)**
- `runOptimization()` - Sends JSON to `/api/optimize`
- Parameters: inputData (JSON), mode, solver config

**[show_input_page.dart](lib/screen/show_input_page.dart)**
- Fetches `input_data` from Supabase
- Calls optimization service
- Saves results

**[output_page.dart](lib/screen/output_page.dart)**
- Retry optimization: fetches input JSON and re-optimizes

---

## 🚨 Common Mistakes That Cause 400 Errors

### ❌ Mistake #1: Wrong Content-Type Header
**Problem:**
```dart
// Missing or wrong header
headers: {
  "Content-Type": "text/plain", // WRONG
}
```

**Solution:**
```dart
headers: {
  "Content-Type": "application/json", // CORRECT
}
```

### ❌ Mistake #2: Modifying Backend JSON
**Problem:**
```dart
// Wrapping or modifying the JSON
final inputJson = {
  "data": backendResponse, // WRONG - adding wrapper
};
```

**Solution:**
```dart
// Store exactly as backend returns it
final inputJson = {
  "employees": backendResponse["employees"] ?? [],
  "vehicles": backendResponse["vehicles"] ?? [],
  "metadata": backendResponse["metadata"] ?? {},
  "baseline": backendResponse["baseline"] ?? [],
};
```

### ❌ Mistake #3: Sending Test Case ID Instead of JSON
**Problem:**
```dart
// Backend expects JSON, not ID
body: jsonEncode({"test_case_id": testCaseId}), // WRONG
```

**Solution:**
```dart
// Send the actual input JSON
body: jsonEncode(inputData), // CORRECT
```

### ❌ Mistake #4: Re-uploading Files Before Optimization
**Problem:**
```dart
// Unnecessary - causes delays and errors
await uploadFile(fileBytes);
await optimize(); // WRONG workflow
```

**Solution:**
```dart
// Direct optimization with JSON
await optimize(inputData); // CORRECT workflow
```

### ❌ Mistake #5: Storing File Bytes in Database
**Problem:**
```dart
// Violates architecture constraints
await _supabase.from('test_cases').insert({
  'file_base64': base64Encode(fileBytes), // WRONG
});
```

**Solution:**
```dart
// Store only JSON
await _supabase.from('test_cases').insert({
  'input_data': jsonData, // CORRECT
});
```

---

## 📝 Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      UPLOAD FLOW                            │
└─────────────────────────────────────────────────────────────┘

User Uploads
    │
    ├─ Excel File ──────────────┐
    │                           │
    └─ Google Sheet ─┬─ Export to .xlsx
                     │
                     ▼
              [Frontend] upload_service.dart
                     │
                     │ multipart/form-data
                     ▼
              [Backend] /api/upload
                     │
                     │ Excel → JSON conversion
                     ▼
              Returns JSON response
                     │
                     │ { employees, vehicles, metadata, baseline }
                     ▼
              [Frontend] Receives JSON
                     │
                     │ Store as-is (NO modifications)
                     ▼
              [Supabase] input_data (jsonb)


┌─────────────────────────────────────────────────────────────┐
│                    OPTIMIZE FLOW                            │
└─────────────────────────────────────────────────────────────┘

User Clicks Optimize
         │
         ▼
    [Frontend] data_service.fetchInputData()
         │
         │ SELECT input_data FROM test_cases
         ▼
    [Supabase] Returns JSON
         │
         │ Exact JSON (no modifications)
         ▼
    [Frontend] optimization_service.runOptimization()
         │
         │ POST /api/optimize
         │ Content-Type: application/json
         │ Body: { employees, vehicles, metadata, baseline, ... }
         ▼
    [Backend] Processes JSON
         │
         │ Runs C++ solver
         ▼
    Returns optimization results
         │
         │ { routes, result, assignments, ... }
         ▼
    [Frontend] Receives results
         │
         │ Store in output_json
         ▼
    [Supabase] output_json (jsonb)
```

---

## 🔧 Files Modified

| File | Changes |
|------|---------|
| `data_service.dart` | Added `fetchInputData()`, removed file storage methods |
| `upload_service.dart` | No changes needed (already correct) |
| `add_test_case_dialog.dart` | Store JSON only, removed file bytes storage |
| `optimization_service.dart` | Send JSON directly, removed file upload step |
| `show_input_page.dart` | Fetch JSON instead of file bytes |
| `output_page.dart` | Fetch JSON instead of file bytes |

---

## ✅ Testing Checklist

- [ ] Upload Excel file and verify JSON stored in `input_data`
- [ ] Upload Google Sheet and verify JSON stored in `input_data`
- [ ] Run optimization and verify results stored in `output_json`
- [ ] Check no 400 errors during optimization
- [ ] Verify snackbar appears on top after upload
- [ ] Test retry optimization from output page

---

## 🎨 UI Improvements

### Snackbar Z-Index Fix

**Issue:** Dialog covered snackbar after upload

**Fix:** Close dialog before showing snackbar
```dart
// BEFORE
AppSnackbar.show(context, message: "Success");
Navigator.pop(context);

// AFTER
Navigator.pop(context);
AppSnackbar.show(context, message: "Success");
```

---

## 📋 Key Takeaways

1. ✅ **Frontend = Pass-through layer** for JSON
2. ✅ **Backend = Single source of truth** for schema
3. ✅ **No file storage** in database
4. ✅ **No schema changes** required
5. ✅ **Direct JSON optimization** flow
6. ✅ **Proper headers** on all requests

---

**Last Updated:** February 23, 2026  
**Architecture Version:** 2.0 (Corrected)  
**Backend Version:** Unchanged (no modifications)
