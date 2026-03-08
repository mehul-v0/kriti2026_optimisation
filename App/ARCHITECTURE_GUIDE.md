# Flutter App Architecture Guide

> **See also:** [overview.md](overview.md) — complete feature documentation for every screen, widget, service, and theme.

---

## 🎯 Core Principle

The Flutter frontend is a **pass-through layer** for JSON.

```
Excel/Sheet  →  Backend /api/upload  →  JSON  →  Supabase
                                                     ↓
Supabase  →  JSON  →  Backend /api/optimize  →  Results  →  Supabase
```

- Backend owns the JSON schema. Frontend never defines, extends, or modifies it.
- Supabase stores only two JSON blobs per test case: `input_data` and `output_json`.
- No Excel bytes, no base64, no extra columns.

---

## 📊 Supabase Database Schema

**Table:** `test_cases`

```sql
create table public.test_cases (
  id          uuid        not null default gen_random_uuid(),
  created_at  timestamptz not null default now(),
  user_id     uuid        not null default auth.uid(),
  case_name   text        not null,
  input_data  jsonb       not null,   -- backend-generated JSON stored as-is
  output_json jsonb       null,       -- optimization results (null until first run)
  constraint test_cases_pkey primary key (id),
  constraint test_cases_user_id_fkey
    foreign key (user_id) references auth.users(id)
);
```

**❌ NO additional columns** (no file_base64, no file_name, no processed_at, etc.)

Row-Level Security: `user_id = auth.uid()` — users can only access their own rows. The `user_id` column uses `default auth.uid()` so the frontend never needs to pass a user ID explicitly.

---

## 🔄 Upload Flow

### Step-by-Step

1. **User picks an Excel file or provides a Google Sheets URL/ID.**
   - Excel: `FilePicker.platform.pickFiles(type: FileType.custom, allowedExtensions: ['xlsx','xls'])` → `Uint8List` bytes.
   - Google Sheets: `UploadService.exportGoogleSheetAsBytes(id)` downloads `.xlsx` via the Google export API — then the same Excel path is followed.

2. **Frontend sends the file to `/api/upload` as `multipart/form-data`.**
   ```dart
   var request = http.MultipartRequest('POST', uri);
   request.files.add(
     http.MultipartFile.fromBytes('file', fileBytes, filename: safeFileName),
   );
   ```
   > Filename must have `.xlsx` extension. Android FilePicker sometimes omits it, so the dialog enforces it: `safeFileName = rawName.endsWith('.xlsx') ? rawName : '$rawName.xlsx'`.

3. **Backend parses the Excel workbook and returns JSON.**
   ```json
   {
     "success": true,
     "message": "Parsed 42 employees and 8 vehicles",
     "employees": [...],
     "vehicles": [...],
     "digest": {...},
     "baseline_cost": 1234.56
   }
   ```
   Note: backend's `/api/upload` does **not** return `metadata` or `baseline` list — the frontend uses empty defaults.

4. **Frontend assembles and stores the input JSON in Supabase.**
   ```dart
   final inputJson = {
     "employees": backendResponse["employees"] ?? [],
     "vehicles": backendResponse["vehicles"] ?? [],
     "metadata": <String, dynamic>{},   // optimizer fills this from request params
     "baseline": <dynamic>[],           // empty; baseline list not returned by /api/upload
   };
   await _dataService.uploadTestCase(caseName, inputJson);
   ```

### Key Files

| File | Role |
|---|---|
| `lib/services/upload_service.dart` | `uploadExcelFile()`, `uploadGoogleSheet()` |
| `lib/widgets/add_test_case_dialog.dart` | Upload UI, calls services, stores to Supabase |
| `lib/services/data_service.dart` | `uploadTestCase()` — INSERT into Supabase |

---

## ⚡ Optimise Flow

### Step-by-Step

1. **User taps "OPTIMISE" on ShowInputPage.**

2. **Frontend fetches `input_data` from Supabase.**
   ```dart
   final inputData = await _dataService.fetchInputData(testCaseId);
   ```

3. **Frontend sends JSON + optimisation config to `/api/optimize`.**
   ```dart
   final requestBody = {
     ...inputData,                          // spread all input fields
     'mode': 'standard',
     if (solverDurationSeconds != null)
       'solverDurationSeconds': solverDurationSeconds,
     if (costWeight != null) 'costWeight': costWeight,
     if (timeWeight != null) 'timeWeight': timeWeight,
   };
   final response = await http.post(
     uri,
     headers: {"Content-Type": "application/json"},
     body: jsonEncode(requestBody),
   );
   ```
   Critical: `Content-Type: application/json` must be set.

4. **Backend runs C++ ALNS solver and returns optimised routes.**

5. **Frontend saves result to Supabase and navigates to OutputPage.**
   ```dart
   await _dataService.saveSolution(testCaseId, resultData);
   Navigator.push(context, MaterialPageRoute(builder: (_) => OutputPage(...)));
   ```

### Key Files

| File | Role |
|---|---|
| `lib/services/optimization_service.dart` | `runOptimization()` — builds request, calls `/api/optimize` |
| `lib/screen/show_input_page.dart` | Triggers optimisation, saves result |
| `lib/screen/output_page.dart` | Re-optimise button: fetches input JSON and re-runs |
| `lib/services/data_service.dart` | `fetchInputData()`, `saveSolution()` |

---

## 🚨 Common Mistakes That Cause 400 Errors

### ❌ Mistake 1 — Wrong Content-Type Header
```dart
// WRONG
headers: {"Content-Type": "text/plain"}

// CORRECT
headers: {"Content-Type": "application/json"}
```

### ❌ Mistake 2 — Wrapping JSON in an Extra Layer
```dart
// WRONG — backend doesn't understand a "data" envelope
final body = jsonEncode({"data": inputData});

// CORRECT — spread the input data directly
final body = jsonEncode({...inputData, 'mode': mode});
```

### ❌ Mistake 3 — Sending the Test Case ID Instead of JSON
```dart
// WRONG
body: jsonEncode({"test_case_id": testCaseId})

// CORRECT
body: jsonEncode(inputData)
```

### ❌ Mistake 4 — Re-uploading the File Before Optimising
```dart
// WRONG — the file should be uploaded once only, at test case creation
await uploadFile(fileBytes);
await optimize(inputData);

// CORRECT
await optimize(inputData);   // input_data already in Supabase
```

### ❌ Mistake 5 — Storing File Bytes in the Database
```dart
// WRONG
await supabase.from('test_cases').insert({'file_base64': base64Encode(bytes)});

// CORRECT
await supabase.from('test_cases').insert({'input_data': jsonData});
```

---

## 🗂️ Layer Responsibilities

```
┌─────────────────────────────────────────────────────────────────┐
│  UI Layer (screens + widgets)                                   │
│  auth_page, home_page, show_input_page, output_page            │
│  Widgets: card, dialog, drawer, map, panels, charts            │
├─────────────────────────────────────────────────────────────────┤
│  Service Layer                                                  │
│  auth_service   → Supabase Auth                                │
│  data_service   → Supabase CRUD (test_cases table)             │
│  upload_service → POST /api/upload (multipart)                 │
│  optimization_service → POST /api/optimize (JSON)             │
│  file_export_service  → JSON / Excel / PDF generation          │
├─────────────────────────────────────────────────────────────────┤
│  Config Layer                                                   │
│  env.dart  → backend URLs (live vs. local)                     │
│  info.dart → map tile URL and defaults                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🔐 Authentication Architecture

`main.dart` → `AuthGate` listens to `Supabase.instance.client.auth.onAuthStateChange` stream:

```
Session exists?  → YES → HomePage
                 → NO  → AuthPage
```

This is a **reactive** pattern: the app never needs to manually push/pop auth routes. When `signOut()` is called, the stream fires a `null` session event and the widget tree automatically rebuilds to show `AuthPage`.

The `AuthGate` widget is **cached** in `_MyAppState._cachedAuthGate` so it is not recreated on every theme change — only the `MaterialApp` shell rebuilds.

---

## 🎨 Theme Architecture

Theme state lives at the root — `_MyAppState` in `main.dart`:

```
_themeNotifier       (ValueNotifier<ThemeMode>) ─→ dark / light
_themeIndexNotifier  (ValueNotifier<int>)        ─→ 0=Petronas, 1=Orange, 2=Yellow
```

Both notifiers are passed down to `AuthGate → HomePage → HomeDrawer`. `HomeDrawer` writes to them; `MaterialApp` reads them via `ValueListenableBuilder` to swap the `ThemeData`.

`AppTheme.lightThemeAt(index)` and `AppTheme.darkThemeAt(index)` produce the full `ThemeData` for each combination.

---

## 📤 Export Architecture

`FileExportService.exportFile(context, type, fileName, data)` handles three formats:

| Type | Generator | Threading |
|---|---|---|
| `"json"` | `JsonEncoder.withIndent('  ')` | Main thread (fast) |
| `"excel"` | `excel` package, multi-sheet `.xlsx` | Main thread |
| `"pdf"` | `pdf` + `printing` (Google Fonts) | Background via `compute()` |

PDF runs in a `compute()` isolate because font embedding + zlib compression is CPU-intensive and would freeze the UI for several hundred milliseconds.

Files are saved to `/storage/emulated/0/Download/` on Android (with per-API-level permission handling) and the documents directory on iOS.

---

## ✅ Testing Checklist

- [ ] Upload Excel file → verify JSON stored in `input_data` (not file bytes)
- [ ] Upload Google Sheet → same verification
- [ ] Run optimisation → verify `output_json` stored in Supabase
- [ ] Check no 400 errors during optimisation
- [ ] Re-optimise from output page → verify updated result saved
- [ ] Export as JSON / Excel / PDF → verify files appear in Downloads
- [ ] Theme switch → verify map colour filter updates
- [ ] Sign out → verify redirect to AuthPage

---

## 📋 Key Rules Summary

| Rule | Reason |
|---|---|
| Store only JSON in `input_data` | Schema is owned by the backend |
| Never modify JSON returned by backend | Backend depends on exact field names |
| Always set `Content-Type: application/json` | Flask reads `request.get_json()` which needs this header |
| Send full input JSON to `/api/optimize` | Backend needs all fields, not just an ID |
| Never add columns to test_cases table | Breaks RLS policies and migration contracts |
| Run PDF generation in `compute()` | Prevents UI jank |

---

**Last Updated:** March 2026  
**See also:** [overview.md](overview.md) · [QUICK_REFERENCE.md](QUICK_REFERENCE.md)
