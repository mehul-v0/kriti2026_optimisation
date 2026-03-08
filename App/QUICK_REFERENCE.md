# Quick Reference — VRP Optimisation App

> For full docs see [overview.md](overview.md) | For architecture rules see [ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md)

---

## 🚀 Common Tasks

### Run the App

```bash
cd App/flutter_application_1
flutter pub get
flutter run                      # debug build on connected device/emulator
flutter run --release            # release build
flutter build apk --release      # produce APK
```

### Local Server (USB + ADB)

```cmd
cd C:\Users\<You>\AppData\Local\Android\sdk\platform-tools
.\adb reverse tcp:5000 tcp:5000
```

Then in `lib/config/env.dart`:
```dart
static const bool useLiveServer = false;   // use localhost
```

### Switch to Live Server

```dart
// lib/config/env.dart
static const bool useLiveServer = true;    // points to _liveUrl
```

---

## 📤 Upload Flow (Dart code summary)

```dart
// 1. Pick file bytes
final result = await FilePicker.platform.pickFiles(
  type: FileType.custom, allowedExtensions: ['xlsx','xls'], withData: true);
final bytes = result!.files.single.bytes!;

// 2. Upload to backend → get JSON
final backendResponse = await UploadService().uploadExcelFile(bytes, 'file.xlsx');
// { "success": true, "employees": [...], "vehicles": [...] }

// 3. Store JSON in Supabase
final inputJson = {
  "employees": backendResponse["employees"] ?? [],
  "vehicles": backendResponse["vehicles"] ?? [],
  "metadata": <String, dynamic>{},   // backend fills at optimise time
  "baseline": <dynamic>[],
};
await DataService().uploadTestCase(caseName, inputJson);
```

---

## ⚡ Optimise Flow (Dart code summary)

```dart
// 1. Fetch JSON from Supabase
final inputData = await DataService().fetchInputData(testCaseId);

// 2. Run solver
final result = await OptimizationService().runOptimization(
  inputData!,
  mode: 'standard',       // 'quick' | 'standard' | 'advanced'
  costWeight: 0.6,
  timeWeight: 0.4,
);
// result = { routes: [...], result: {...}, violation_details: {...} }

// 3. Save result
await DataService().saveSolution(testCaseId, result);
```

---

## 📊 Supabase Table Quick View

| Column | Type | Content |
|---|---|---|
| `id` | uuid | Auto-generated primary key |
| `created_at` | timestamptz | Auto now() |
| `user_id` | uuid | `auth.uid()` — RLS scoped |
| `case_name` | text | User-provided name |
| `input_data` | jsonb | employees, vehicles, metadata, baseline |
| `output_json` | jsonb | routes, result stats, violations |

**DataService methods:**

```dart
fetchTestCases()               // SELECT all for current user, newest first
uploadTestCase(name, json)     // INSERT input_data
fetchInputData(id)             // SELECT input_data WHERE id = ?
fetchSolution(id)              // SELECT output_json WHERE id = ?
saveSolution(id, solution)     // UPDATE output_json WHERE id = ?
deleteTestCase(id)             // DELETE WHERE id = ?
renameTestCase(id, name)       // UPDATE case_name WHERE id = ?
```

---

## 🔗 Backend Endpoints

| Endpoint | Method | Content-Type | Purpose |
|---|---|---|---|
| `/api/upload` | POST | `multipart/form-data` | Excel → JSON conversion |
| `/api/optimize` | POST | `application/json` | Run VRP solver |

**Optimise request body:**
```json
{
  "employees": [...],
  "vehicles": [...],
  "metadata": {...},
  "baseline": [...],
  "mode": "standard",
  "solverDurationSeconds": 30,
  "costWeight": 0.6,
  "timeWeight": 0.4,
  "distanceMethod": "haversine"
}
```

---

## 🗺️ Screen Navigation

```
AuthGate
  ├─ (no session) → AuthPage
  │     └─ sign in / register → AuthGate re-routes to HomePage
  └─ (session)   → HomePage
        └─ tap card → ShowInputPage
              └─ OPTIMISE → OutputPage
                    └─ RE-OPTIMISE → re-fetches input, re-runs solver
```

---

## 🎨 Theme / Appearance

Changing theme from the side drawer:
- **Colour Theme:** 0 = Petronas (teal `#00D2BE`), 1 = Orange, 2 = Yellow
- **Dark / Light mode:** toggle in drawer

Programmatically from anywhere with access to the notifiers:
```dart
themeIndexNotifier.value = 1;           // switch to Orange
themeNotifier.value = ThemeMode.light;  // switch to light mode
```

---

## 📦 Export Output

```dart
// From OutputPage, user taps download FAB
FileExportService().exportFile(
  context,
  'json',    // or 'excel' or 'pdf'
  'my_result',
  resultData,
);
// Saved to: /storage/emulated/0/Download/optimization_output_YYYYMMDD_HHmmss.{ext}
```

---

## 🔍 Key File Locations

| What | File |
|---|---|
| Backend URL config | `lib/config/env.dart` |
| Map tile URL | `lib/config/info.dart` |
| Supabase init + auth | `lib/services/auth_service.dart` |
| Supabase CRUD | `lib/services/data_service.dart` |
| Excel upload | `lib/services/upload_service.dart` |
| Optimisation call | `lib/services/optimization_service.dart` |
| Export (JSON/Excel/PDF) | `lib/services/file_export_service.dart` |
| Auth form | `lib/screen/auth_page.dart` |
| Home (test case list) | `lib/screen/home_page.dart` |
| Input viewer + optimise | `lib/screen/show_input_page.dart` |
| Results + map | `lib/screen/output_page.dart` |
| Colour + dark/light theme | `lib/theme/theme.dart` |

---

## ⚠️ Never Do

| ❌ Don't | Reason |
|---|---|
| Store file bytes in DB | Schema only allows `input_data` jsonb |
| Add columns to `test_cases` | Breaks RLS and migration contracts |
| Wrap JSON in extra envelope before posting | Backend reads top-level keys directly |
| Omit `Content-Type: application/json` | Flask's `request.get_json()` returns `None` |
| Call `/api/optimize` with a test case ID | Backend expects the actual JSON, not an ID |
| Modify backend JSON structure | Backend owns the schema |

---

## ✅ Input Excel Sheet Names (Required)

| Sheet Name | Purpose |
|---|---|
| `employees` | Employee pickup/drop coordinates, priorities, preferences |
| `vehicles` | Vehicle specs, depot locations, costs |
| `metadata` | Key-value config (city, office coords, shift start) |
| `baseline` | Per-employee baseline cost and time for comparison |

---

## 🧑‍💻 Quick Debugging

**App cannot connect to backend:**
- Check `useLiveServer` in `env.dart`
- For USB local: run `adb reverse tcp:5000 tcp:5000`
- Verify Flask is running on port 5000

**400 error from `/api/optimize`:**
- Confirm `Content-Type: application/json` header is set
- Confirm body contains actual JSON (not `{"test_case_id": ...}`)
- Confirm `input_data` in Supabase is not null

**Upload fails with "file type not allowed":**
- Ensure filename ends in `.xlsx` (enforced in `add_test_case_dialog.dart`)

**Google Sheets upload fails:**
- Sheet must be **publicly accessible** (Anyone with link → Viewer)
- URL must contain `/spreadsheets/d/{ID}/`

**Auth error: "Wrong password or email not found":**
- User's email may not be confirmed — check Supabase Auth dashboard

---

*Last Updated: March 2026*
