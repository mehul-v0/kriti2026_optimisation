# App Overview — VRP Optimisation Flutter Application

> **Version:** 0.1.0 · **Framework:** Flutter (Dart SDK ^3.10.4) · **Last Updated:** March 2026

---

## Table of Contents

1. [What the App Does](#1-what-the-app-does)
2. [Tech Stack](#2-tech-stack)
3. [Project Structure](#3-project-structure)
4. [End-to-End Data Flow](#4-end-to-end-data-flow)
5. [Authentication & Supabase](#5-authentication--supabase)
6. [Database Schema](#6-database-schema)
7. [Input Data Format](#7-input-data-format)
8. [Server Connection](#8-server-connection)
9. [Screen-by-Screen Guide](#9-screen-by-screen-guide)
   - [Auth Page](#91-auth-page)
   - [Home Page](#92-home-page)
   - [Show Input Page](#93-show-input-page)
   - [Output Page](#94-output-page)
10. [Services Reference](#10-services-reference)
11. [Widgets Reference](#11-widgets-reference)
12. [Theming System](#12-theming-system)
13. [Output Data Format](#13-output-data-format)
14. [File Export System](#14-file-export-system)
15. [Small Features & Details](#15-small-features--details)
16. [Dependencies](#16-dependencies)

---

## 1. What the App Does

This is a **Vehicle Routing Problem (VRP) optimisation** mobile/tablet app. It solves the employee cab-pooling / last-mile logistics problem: given a set of employees who need to be picked up from various locations and dropped at a central office, and a fleet of vehicles, find the most cost- and time-efficient allocation of employees to vehicles with optimal routes.

**Core workflow:**
1. A planner uploads an **Excel spreadsheet** (or provides a **Google Sheets link**) containing employee pickup locations, vehicle details, time windows, and baseline costs.
2. The app stores this data in **Supabase** (cloud database) as a *Test Case*.
3. The planner clicks **Optimise** — the app sends the JSON to a **C++ backend solver** running as a Python-Flask REST API.
4. The backend returns optimised routes with total cost, distances, timing, and constraint violations.
5. The app **visualises** the result on an interactive map, shows tabular route details, renders comparative charts, and lets the planner **export** the solution as JSON, Excel, or PDF.

---

## 2. Tech Stack

| Layer | Technology |
|---|---|
| Mobile/Tablet App | Flutter (Dart) |
| Authentication | Supabase Auth (email + password) |
| Cloud Database | Supabase (PostgreSQL + JSONB) |
| Optimisation Backend | Python Flask + C++ ALNS Solver |
| Maps | flutter_map + OpenStreetMap tiles |
| Charts | fl_chart |
| Excel Parsing | excel package |
| PDF Generation | pdf + printing packages |
| File Picking | file_picker |
| HTTP | http package |

---

## 3. Project Structure

```
lib/
├── main.dart                    # App entry point, theme wiring
├── config/
│   ├── env.dart                 # Backend URLs (live vs. local)
│   └── info.dart                # Map config (tile URL, defaults)
├── screen/
│   ├── auth_gate.dart           # Supabase session listener → route to Home or Auth
│   ├── auth_page.dart           # Sign In / Register form
│   ├── home_page.dart           # Test case list, search, sort, selection
│   ├── show_input_page.dart     # Input data viewer + Optimise trigger
│   └── output_page.dart         # Result map, summary, charts, violations
├── services/
│   ├── auth_service.dart        # Supabase auth + SupabaseConfig
│   ├── data_service.dart        # Supabase CRUD (test_cases table)
│   ├── upload_service.dart      # Excel / Google Sheets → Backend /api/upload
│   ├── optimization_service.dart# JSON → Backend /api/optimize
│   └── file_export_service.dart # Export results as JSON / Excel / PDF
├── widgets/
│   ├── add_test_case_dialog.dart# Upload dialog (Excel or Google Sheets)
│   ├── test_case_card.dart      # Card on home list
│   ├── home_drawer.dart         # Side drawer: theme picker, logout
│   ├── filter_bottom_sheet.dart # Sort options bottom sheet
│   ├── map_view.dart            # Input data map (employee + vehicle markers)
│   ├── output_summary_panel.dart# KPI cards, savings, fleet snapshot
│   ├── output_violations_panel.dart # Constraint violation breakdown
│   ├── output_charts_panel.dart # Cost/time comparison bar charts
│   └── download_dialog.dart     # Export format picker bottom sheet
├── elements/
│   ├── snackbar.dart            # Themed snack bar helper
│   ├── spinner.dart             # Loading overlay
│   └── sliver_loading.dart      # Sliver-compatible skeleton loader
├── utils/
│   ├── excel_parser.dart        # Client-side Excel parser (unused in production)
│   └── google_sheets_parser.dart# Google Sheets helper utilities
└── theme/
    ├── theme.dart               # AppColors, AppThemeData, AppTheme
    └── demo_theme.dart          # Alternate / demo theme
```

---

## 4. End-to-End Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                           UPLOAD FLOW                               │
│                                                                     │
│  User selects Excel file                                            │
│         │                                                           │
│         ▼                                                           │
│  FilePicker → raw bytes (Uint8List)                                 │
│         │                                                           │
│         ▼                                                           │
│  POST /api/upload  (multipart/form-data)                            │
│  ┌─────────────────────────────────┐                                │
│  │  Python Flask Backend           │                                │
│  │  Reads Excel sheets:            │                                │
│  │  - employees sheet              │                                │
│  │  - vehicles sheet               │                                │
│  │  - metadata sheet (key-value)   │                                │
│  │  - baseline sheet               │                                │
│  │  Returns JSON response          │                                │
│  └─────────────────────────────────┘                                │
│         │                                                           │
│         ▼                                                           │
│  Frontend receives JSON:                                            │
│  { employees:[...], vehicles:[...], metadata:{}, baseline:[] }      │
│         │                                                           │
│         ▼                                                           │
│  Supabase INSERT: test_cases.input_data = JSON                      │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                         OPTIMISE FLOW                               │
│                                                                     │
│  User taps "OPTIMISE" on Show Input Page                            │
│         │                                                           │
│         ▼                                                           │
│  Supabase SELECT: test_cases.input_data WHERE id = ?                │
│         │                                                           │
│         ▼                                                           │
│  POST /api/optimize  (Content-Type: application/json)               │
│  Body = input_data JSON + optimisation params:                      │
│  { ...inputData, mode, solverDurationSeconds,                       │
│    costWeight, timeWeight, distanceMethod }                         │
│  ┌─────────────────────────────────┐                                │
│  │  C++ ALNS Solver (via Flask)    │                                │
│  │  Computes optimal routes        │                                │
│  │  Returns routes, result stats,  │                                │
│  │  assignments, violations        │                                │
│  └─────────────────────────────────┘                                │
│         │                                                           │
│         ▼                                                           │
│  Supabase UPDATE: test_cases.output_json = result                   │
│         │                                                           │
│         ▼                                                           │
│  Navigate to OutputPage with result data                            │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                         EXPORT FLOW                                 │
│                                                                     │
│  User taps Download button on Output Page                           │
│         │                                                           │
│         ▼                                                           │
│  DownloadDialog → select JSON / Excel / PDF                         │
│         │                                                           │
│         ▼                                                           │
│  FileExportService: generate bytes in background isolate            │
│         │                                                           │
│         ▼                                                           │
│  Save to device Downloads folder                                    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Authentication & Supabase

### Initialisation

Supabase is initialised at app startup inside `main()` before `runApp()`:

```dart
await SupabaseConfig.initialize();
// URL: https://edfijyopbfawwqmqukwf.supabase.co
// anon key embedded in auth_service.dart
```

### Auth Gate Pattern

`AuthGate` (inside `main.dart` widget tree) listens to a **stream** of Supabase auth state changes:

```
onAuthStateChange stream → AuthState
  session != null  →  HomePage (user is logged in)
  session == null  →  AuthPage (login form)
```

This means the app **automatically navigates** when a user signs in or signs out — no manual navigation code needed.

### Sign In (Login)

- Form with **Email** and **Password** fields.
- Calls `_supabase.auth.signInWithPassword(email, password)`.
- Specific error handling:
  - "Invalid login credentials" → "Wrong password or email not found."
  - HTTP 400 → "Invalid request. Check your email format."
  - Network error → "Check your internet connection."
- On success: `onAuthStateChange` stream fires → AuthGate routes to HomePage.

### Sign Up (Register)

- Same form reveals a **Confirm Password** field.
- Calls `_supabase.auth.signUp(email, password)`.
- "User already registered" error is translated to a friendly message.
- On success: Supabase auto-signs in the user → AuthGate routes to HomePage.

### Sign Out

- Triggered from the side drawer (HomeDrawer).
- Calls `_supabase.auth.signOut()`.
- `onAuthStateChange` fires → AuthGate routes back to AuthPage.

### Row Level Security

All test cases in Supabase are **user-scoped**:
- The `test_cases` table has `user_id` which references `auth.users(id)`.
- Supabase RLS policies ensure users can only read/write their own rows.
- `user_id` is set automatically via `auth.uid()` on insert.

---

## 6. Database Schema

**Table: `test_cases`** (Supabase PostgreSQL)

```sql
create table public.test_cases (
  id          uuid        not null default gen_random_uuid(),
  created_at  timestamptz not null default now(),
  user_id     uuid        not null default auth.uid(),
  case_name   text        not null,
  input_data  jsonb       not null,   -- backend-generated JSON (employees, vehicles, metadata, baseline)
  output_json jsonb       null,       -- optimisation result (routes, result stats, assignments)
  constraint test_cases_pkey primary key (id),
  constraint test_cases_user_id_fkey
    foreign key (user_id) references auth.users(id)
);
```

### JSONB Column Contents

**`input_data`** — stored after `/api/upload`:
```json
{
  "employees": [
    {
      "employee_id": "E01",
      "priority": 1,
      "pickup_lat": 12.9352,
      "pickup_lng": 77.6152,
      "drop_lat": 12.9716,
      "drop_lng": 77.5946,
      "earliest_pickup": "07:30",
      "latest_drop": "09:00",
      "vehicle_preference": "sedan",
      "sharing_preference": "shared"
    }
  ],
  "vehicles": [
    {
      "vehicle_id": "V01",
      "fuel_type": "CNG",
      "vehicle_type": "sedan",
      "capacity": 4,
      "cost_per_km": 12.5,
      "avg_speed_kmph": 35,
      "current_lat": 12.9716,
      "current_lng": 77.5946,
      "available_from": "06:00",
      "category": "premium"
    }
  ],
  "metadata": {
    "city": "Bengaluru",
    "office_lat": 12.9716,
    "office_lng": 77.5946,
    "shift_start": "09:00"
  },
  "baseline": [
    {
      "employee_id": "E01",
      "baseline_cost": 150.0,
      "baseline_time_min": 35
    }
  ]
}
```

**`output_json`** — stored after `/api/optimize`:
```json
{
  "optimization_id": "opt_17718...",
  "result": {
    "total_cost": 1240.50,
    "total_time": 480,
    "total_distance": 128.4,
    "vehicles_used": 5,
    "vehicles_available": 8,
    "hard_violations": 0,
    "soft_violations": 2,
    "baseline_cost": 1850.0,
    "baseline_time": 700,
    "cost_savings": 609.5,
    "cost_savings_percent": 32.9
  },
  "routes": [...],
  "assignments": [...],
  "violation_details": {
    "capacity_violations": [],
    "unassigned_employees": [],
    "time_window_violations": [],
    "sharing_pref_violations": [],
    "vehicle_pref_violations": []
  }
}
```

### DataService API

| Method | Description |
|---|---|
| `fetchTestCases()` | Get all cases for current user, ordered newest first |
| `uploadTestCase(name, json)` | Insert new test case with `input_data` |
| `fetchInputData(id)` | Fetch only `input_data` for a case (for optimisation) |
| `fetchSolution(id)` | Fetch only `output_json` for a case |
| `saveSolution(id, solution)` | Update `output_json` with optimisation result |
| `deleteTestCase(id)` | Hard delete a test case |
| `renameTestCase(id, newName)` | Update `case_name` |

---

## 7. Input Data Format

### Excel Spreadsheet Structure

The Excel file uploaded by users must contain the following **named sheets**:

#### Sheet: `employees`

| Column | Field | Type | Description |
|---|---|---|---|
| A | employee_id | string | Unique ID (e.g., "E01") |
| B | priority | int | 1=CRITICAL, 2=HIGH, 3=MEDIUM, 4=LOW, 5=FLEX |
| C | pickup_lat | float | Pickup latitude |
| D | pickup_lng | float | Pickup longitude |
| E | drop_lat | float | Office/drop latitude |
| F | drop_lng | float | Office/drop longitude |
| G | earliest_pickup | string | Earliest time vehicle can arrive ("HH:MM") |
| H | latest_drop | string | Latest acceptable drop-off time ("HH:MM") |
| I | vehicle_preference | string | Preferred vehicle type (e.g., "sedan", "any") |
| J | sharing_preference | string | "shared" or "private" |

#### Sheet: `vehicles`

| Column | Field | Type | Description |
|---|---|---|---|
| A | vehicle_id | string | Unique ID (e.g., "V01") |
| B | fuel_type | string | "CNG", "Petrol", "Diesel", "Electric" |
| C | vehicle_type | string | "sedan", "suv", "bus" |
| D | capacity | int | Max passenger capacity |
| E | cost_per_km | float | Operating cost per kilometre |
| F | avg_speed_kmph | float | Average speed |
| G | current_lat | float | Starting latitude (depot position) |
| H | current_lng | float | Starting longitude |
| I | available_from | string | Earliest available time ("HH:MM") |
| J | category | string | "standard", "premium" |

#### Sheet: `metadata` (key-value pairs)

| Key | Example Value | Description |
|---|---|---|
| city | "Bengaluru" | City name for display |
| office_lat | 12.9716 | Office/destination latitude |
| office_lng | 77.5946 | Office/destination longitude |
| shift_start | "09:00" | Required arrival time |

#### Sheet: `baseline` (optional)

| Column | Field | Description |
|---|---|---|
| A | employee_id | Must match employees sheet |
| B | baseline_cost | Current per-employee transportation cost |
| C | baseline_time_min | Current journey time in minutes |

### Google Sheets Support

Instead of a local Excel file, users can paste a **Google Sheets URL** or spreadsheet ID. The app:
1. Constructs the Google Sheets export URL: `https://docs.google.com/spreadsheets/d/{ID}/export?format=xlsx`
2. Downloads the `.xlsx` bytes.
3. Sends those bytes to `/api/upload` — identical to the local Excel flow.

> **Requirement:** The Google Sheet must be publicly accessible (view permission for anyone with link).

---

## 8. Server Connection

### Backend URLs (`lib/config/env.dart`)

```dart
// Live server (Render/VPS — always used when useLiveServer = true)
static const String _liveUrl =
    "https://i80owo0o8wo0swkkcw888ws4.65.21.154.72.sslip.io/";

// Local development (Android physical device via USB)
static const String _androidLocal = "http://127.0.0.1:5000";

// Local development (Web browser)
static const String _webLocal = "http://127.0.0.1:5000";

static const bool useLiveServer = true; // toggle here
```

### Endpoints Used

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/upload` | POST multipart/form-data | Excel → JSON conversion |
| `/api/optimize` | POST application/json | Run VRP solver |

### Local Development ADB Setup

When testing on a **physical Android device** connected via USB with a local server:

```cmd
cd C:\Users\<You>\AppData\Local\Android\sdk\platform-tools
.\adb devices
.\adb reverse tcp:5000 tcp:5000
```

This forwards port 5000 from the device to the local machine so `127.0.0.1:5000` works on the device.

### Optimisation Parameters Sent to Backend

The `OptimizationService.runOptimization()` method merges `input_data` JSON with additional solver config:

```json
{
  ...inputData,
  "mode": "standard",
  "solverDurationSeconds": 30,
  "costWeight": 0.6,
  "timeWeight": 0.4,
  "distanceMethod": "haversine",
  "priorityDelays": { "1": 0, "2": 5, "3": 10 }
}
```

| Parameter | Options | Default | Description |
|---|---|---|---|
| mode | "quick", "standard", "advanced" | "standard" | Solver effort level |
| solverDurationSeconds | integer | backend default | Time limit for solver |
| costWeight | 0.0–1.0 | 0.6 | Weight of cost in objective |
| timeWeight | 0.0–1.0 | 0.4 | Weight of time in objective |
| distanceMethod | "haversine", "euclidean" | "haversine" | Distance calculation method |

---

## 9. Screen-by-Screen Guide

### 9.1 Auth Page

**File:** `lib/screen/auth_page.dart`

The entry screen shown to unauthenticated users.

#### Layout
- Centered card (max width 450px on tablets/web, 85% width on phones).
- Title: "Sign In" or "Register" (toggles dynamically).
- On screens wider than 600px, a subtle drop shadow appears on the card.

#### Fields
- **Email Address** — `TextInputType.emailAddress`, client-side format validation.
- **Password** — obscured, minimum 6 characters.
- **Confirm Password** — only visible in Register mode, must match password.

#### Validation
All fields validated via Flutter's `Form` + `GlobalKey<FormState>` before any network call.

#### Actions
- **Submit Button** — shows a `LoadingOverlay` spinner while the request is in flight.
- **Toggle Link** — "Don't have an account? Register" / "Already have an account? Sign In" — switches modes and clears all fields.

#### Error Handling
- Network errors, wrong credentials, and already-registered emails all surface as themed snack bars at the bottom of the screen.

---

### 9.2 Home Page

**File:** `lib/screen/home_page.dart`

The main dashboard showing all test cases owned by the logged-in user.

#### App Bar
- **Title:** "Test Cases" (normal mode) | "N selected" (selection mode).
- **Search icon** — tapping it expands an inline search bar with debounced filtering (300ms delay) that searches by `case_name`.
- **Sort icon** — opens `FilterBottomSheet` with 4 sort options.
- **Select All icon** — visible in selection mode; toggles selecting/deselecting all cases.
- **Cancel icon** — exits selection mode.
- **Delete icon** — visible in selection mode; shows confirmation dialog then parallel-deletes selected cases.

#### FAB (Floating Action Button)
Large `+` button opens `AddTestCaseDialog` to upload a new test case.

#### Test Case List
- Loaded from Supabase via `DataService.fetchTestCases()`.
- Displayed as a **responsive grid**: 2 columns on wide screens (width > 600px), 1 column on narrow screens.
- Each item is a `TestCaseCard` showing:
  - Folder icon (or checkmark when selected).
  - Case name and creation date (formatted as "Jan 12, 2026").
  - Pin icon, Delete icon, Rename icon (accessible via trailing action row or long-press context menu).

#### Sort Options
Via `FilterBottomSheet`:
- Newest First (default)
- Oldest First
- Name A → Z
- Name Z → A

Pinned items always float to the top regardless of sort order.

#### Search
- Real-time case-insensitive search filters `case_name`.
- 300ms debounce prevents excessive rebuilds while typing.
- Processed list is **cached** between rebuilds and only recomputed when `_needsReprocessing` is true.

#### Selection Mode
- Activated by long-pressing any card.
- Multi-select by tapping additional cards.
- Batch delete with confirmation dialog.
- Parallel delete: `Future.wait(ids.map((id) => deleteTestCase(id)))` — deletes all selected cases concurrently.

#### Pinning
- Pin toggle is in-memory (not persisted to Supabase).
- Pinned cases always appear at the top of the list.

#### Rename
- Inline dialog with a pre-filled text field.
- Calls `DataService.renameTestCase(id, newName)`.
- List reloads after rename.

#### Scroll-to-Top Button
- Appears as an animated FAB after scrolling 200px.
- Debounced scroll listener (100ms) prevents excessive state updates.

#### Drawer
- Hamburger menu icon opens `HomeDrawer`.

#### Card Tapping
- Tapping a card while NOT in selection mode opens `ShowInputPage` for that test case.
- Tapping a card while in selection mode toggles its selection.

---

### 9.3 Show Input Page

**File:** `lib/screen/show_input_page.dart`

Displays the raw input data for a test case and is the launch point for optimisation.

#### Tab Structure
- **Outer Tabs** (Data | Map) — switches the main content pane.
- **Inner Tabs inside Data** (Employees | Vehicles) — switches the list.

#### Data Tab — Employees Sub-tab

- Lists all employees from `input_data.employees`.
- Each card shows:
  - Employee ID (e.g., "E01")
  - Priority badge: CRITICAL / HIGH / MEDIUM / LOW / FLEX (colour-coded)
  - Pickup coordinates
  - Latest drop-off time
  - Vehicle preference and sharing preference
- Filter bar (shown via toggle button):
  - Filter by priority (chips for each priority level present in the data).
  - Sort by cost (ascending/descending), computed from baseline data.
- Tapping a card in the Map tab triggers scrolling in the Data tab to the corresponding employee.

#### Data Tab — Vehicles Sub-tab

- Lists all vehicles from `input_data.vehicles`.
- Each card shows:
  - Vehicle ID
  - Vehicle type and fuel type
  - Seating capacity
  - Cost per km
  - Average speed
  - Starting location coordinates
  - Available-from time
  - Category (standard / premium)
- Filter bar:
  - Filter by minimum seat count (dynamic breakpoints computed from actual data).
  - Sort by cost per km, time, or speed.

#### Data Tab — Metadata Summary

At the top of the data panel, a compact metadata strip shows:
- City name (resolved from metadata → employee fields → lat/lng inference → "Bengaluru" fallback)
- Number of employees and vehicles
- Shift start time
- Office coordinates

#### Map Tab

- Powered by `flutter_map` with OpenStreetMap tiles.
- **Employee markers** (silver/grey pins) at pickup locations.
- **Vehicle markers** (brand-colour pins) at their starting positions.
- Auto-centers and zooms to fit all markers.
- Tapping an employee marker: switches to Data tab, scrolls to and highlights that employee card for 3 seconds.
- Tapping a vehicle marker: highlights the corresponding vehicle card.
- Dark/light map colour filter matrices applied depending on current theme.
- Floating controls: Reset Rotation, Re-center, Legend toggle.
- Legend overlay shows marker types.

#### Existing Solution Banner

When a test case already has a saved `output_json`, a banner appears:
> "Solution exists — you can view it or re-optimise."

Tapping "View" navigates directly to `OutputPage` without re-running the solver.

#### Optimise Action

A persistent bottom action bar (outside the scroll area) contains:
- **Mode selector** — chips for "quick", "standard", "advanced".
- **OPTIMISE button** — large, primary-coloured button.

On tap:
1. Shows `LoadingOverlay` spinner with a Lottie animation.
2. Calls `DataService.fetchInputData(id)` to retrieve the JSON from Supabase.
3. Calls `OptimizationService.runOptimization(inputData, mode: _optimizationMode)`.
4. If successful: saves result to Supabase via `DataService.saveSolution(id, result)`.
5. Navigates to `OutputPage` with the result data.
6. If error: shows snack bar with the error message.

#### Scroll-to-Top FAB
Appears after scrolling 300px, scrolls back to top with smooth animation.

---

### 9.4 Output Page

**File:** `lib/screen/output_page.dart`

The results visualisation screen with three top-level tabs.

#### Top-Level Tabs (3 tabs)

| Tab | Content |
|---|---|
| Map | Animated route map |
| Overview | Summary / Routes / Violations sub-tabs |
| Charts | Cost & time comparison charts |

---

#### Tab 1: Map

The interactive route visualisation:

- All vehicle routes drawn as **polylines** on the OpenStreetMap map.
- Each vehicle gets a unique colour from an 8-colour palette (green, blue, amber, red, violet, cyan, pink, orange).
- **Stop markers** rendered at each pickup location and the office.
- Employee ID labels on markers derived from stop location strings (e.g., "E01 Pickup").

**Route Playback Animation:**
- Play/Pause button starts an `AnimationController` (15-second duration by default).
- A progress slider shows playback position (0.0–1.0).
- **Playback speed selector**: 0.5×, 1×, 2×, 4×.
- During playback, polylines are progressively drawn based on `_playbackProgress`.
- Focused vehicle filter: tap a vehicle card to isolate its route on the map.

**Fleet Filter Panel:**
- Toggle button shows/hides a horizontal filter bar.
- Filter by vehicle ID, or search by vehicle ID text.
- Sort routes by total cost (asc/desc) or total distance (asc/desc).
- Filter state shown with a badge count on the filter button.

**Map Legend Overlay:**
- Small toggleable overlay in the top-right corner.
- Shows colour swatches for each vehicle route.

**North/Rotation Reset Button:**
- Returns map to north-up orientation.

**Re-center Button:**
- Animates map back to the centroid of all route stops.

---

#### Tab 2: Overview — Summary Sub-tab

**File:** `lib/widgets/output_summary_panel.dart`

Comprehensive KPI dashboard:

- **Solution Quality Header:** "OPTIMAL SOLUTION ✓" (green) or "N hard violations, M soft violations" (red/amber).
- **Optimisation ID** — displayed for traceability.
- **Cost Savings Hero Card** (shown only when savings > 0):
  - Optimised cost vs baseline cost.
  - Savings amount and percentage in a large highlighted card.
- **2×2 KPI Grid:**
  - Total Cost (₹)
  - Total Distance (km)
  - Time Saved vs baseline (minutes and %)
  - Total Time (minutes)
- **Fleet Snapshot:**
  - Vehicles used / available
  - Fleet utilisation %
  - Employees served
  - Total trips
  - Average capacity utilisation %
- **Efficiency Ratios:**
  - Cost per km
  - Cost per employee
  - Distance per employee
  - Cost per trip
- **Service Window:**
  - Earliest pickup time
  - Latest office arrival time
  - Total window span

---

#### Tab 2: Overview — Routes Sub-tab

- Expandable cards for each vehicle.
- Each vehicle card shows:
  - Vehicle ID
  - Total cost, distance, time
  - Colour swatch matching the map polyline
  - Number of trips and passengers
- Expanding a card reveals:
  - Per-trip breakdown with all stops.
  - Each stop: location name, arrival time, departure time, wait time, distance from previous stop.
  - **Phantom trip filtering**: zero-distance trips that only contain office/depot stops are hidden automatically.
  - Trips are re-numbered sequentially after phantom filtering.
- **Search bar** — filter vehicles by ID.
- **Vehicle sort**: by cost or distance.

---

#### Tab 2: Overview — Violations Sub-tab

**File:** `lib/widgets/output_violations_panel.dart`

Constraint violation analysis:

- **Feasibility Status Header:** shows hard and soft violation counts with colour-coded badges.
- **Constraint Categories:**
  - ✅/❌ Capacity violations — lists each vehicle that exceeded capacity with employee count vs limit.
  - ✅/❌ Unassigned employees — lists employees that could not be assigned to any vehicle.
  - ✅/❌ Time window violations — employees not served within their earliest_pickup–latest_drop window.
  - ✅/❌ Sharing preference violations — employees who requested "private" but were assigned shared rides.
  - ✅/❌ Vehicle preference violations — employees assigned to a vehicle type other than their preference.
- **Employee Dwell Time Table:**
  - Lists all employees with non-zero wait times at pickup.
  - Sorted by dwell time descending (longest waits first).
  - Shows vehicle ID, arrival time, departure time, and wait duration.

---

#### Tab 3: Charts

**File:** `lib/widgets/output_charts_panel.dart`

Data visualisation using `fl_chart`:

- **Cost Comparison** (shown if baseline > 0):
  - Horizontal bar chart comparing baseline cost vs optimised cost side by side.
  - Savings amount labelled on the chart.
- **Time Comparison** (shown if baseline time > 0):
  - Horizontal bar chart of baseline minutes vs optimised minutes.
- **Fleet Composition:**
  - Per-vehicle bar chart: cost, distance, capacity utilisation %.
  - Legend matching the route colour palette.
- **Passenger Distribution:**
  - Bar chart of passengers per vehicle.
- **Trips Distribution:**
  - Bar chart of trip count per vehicle.

---

#### Download / Export Button

Floating action button in the bottom-right of the Output Page.

Tapping shows `DownloadDialog` bottom sheet with three options:
1. **JSON** — exports full `output_json` as a formatted `.json` file.
2. **Excel (.xlsx)** — generates a multi-sheet Excel workbook:
   - Sheet 1: Summary (KPI metrics)
   - Sheet 2: Vehicle Routes (all stops with times)
3. **PDF Report** — generates a PDF with formatted summary tables and stats (processed in a background `compute()` isolate to avoid UI freezes).

Files are saved to the device's **Downloads** folder (`/storage/emulated/0/Download/` on Android).

#### Re-optimise

An "RE-OPTIMISE" button in the Output Page app bar:
1. Fetches `input_data` from Supabase again.
2. Re-runs the solver with the same or updated parameters.
3. Saves new result and refreshes the page.

---

## 10. Services Reference

### `AuthService` (`lib/services/auth_service.dart`)

| Method | Returns | Description |
|---|---|---|
| `signIn(email, password)` | `Future<void>` | Sign in with email + password, throws `Exception` with user-friendly message on error |
| `signUp(email, password)` | `Future<void>` | Register new account |
| `signOut()` | `Future<void>` | Sign out current user |
| `currentUser` | `User?` | Currently authenticated user object |
| `authStateChanges` | `Stream<AuthState>` | Stream used by AuthGate |

`SupabaseConfig.initialize()` is a static method that calls `Supabase.initialize()` with the project URL and anon key.

---

### `DataService` (`lib/services/data_service.dart`)

All methods use the currently authenticated user's session automatically.

| Method | Returns | Description |
|---|---|---|
| `fetchTestCases()` | `Future<List<Map>>` | Fetch all test cases for current user |
| `uploadTestCase(name, json)` | `Future<void>` | Insert new test case |
| `fetchInputData(id)` | `Future<Map?>` | Fetch `input_data` for optimisation |
| `fetchSolution(id)` | `Future<Map?>` | Fetch `output_json` |
| `saveSolution(id, solution)` | `Future<void>` | Update `output_json` |
| `deleteTestCase(id)` | `Future<void>` | Delete a test case |
| `renameTestCase(id, name)` | `Future<void>` | Rename a test case |

---

### `UploadService` (`lib/services/upload_service.dart`)

| Method | Returns | Description |
|---|---|---|
| `uploadExcelFile(bytes, fileName)` | `Future<Map>` | POST Excel bytes to `/api/upload` via multipart |
| `uploadGoogleSheet(spreadsheetId)` | `Future<Map>` | Export sheet as Excel → upload |
| `exportGoogleSheetAsBytes(id)` | `Future<Uint8List>` | Download Google Sheet as `.xlsx` bytes |
| `extractSpreadsheetId(input)` | `String?` | Static helper: extract ID from URL or raw ID |

---

### `OptimizationService` (`lib/services/optimization_service.dart`)

| Method | Returns | Description |
|---|---|---|
| `runOptimization(inputData, {mode, solverDurationSeconds, costWeight, timeWeight, priorityDelays, distanceMethod})` | `Future<Map>` | POST JSON + config to `/api/optimize`, returns result map |

The method merges `inputData` with non-null config parameters using Dart's spread operator before encoding.

---

### `FileExportService` (`lib/services/file_export_service.dart`)

| Method | Returns | Description |
|---|---|---|
| `exportFile(context, type, fileName, data)` | `Future<void>` | Export data as "json", "excel", or "pdf" |

- Checks Android storage permission (handles API levels 29, 30–32, 33+).
- PDF generation runs in a separate `compute()` isolate (CPU-intensive).
- Embeds Google Fonts via `printing` package for PDF.
- Saves to `/storage/emulated/0/Download/` on Android, documents directory on iOS.

---

## 11. Widgets Reference

### `AddTestCaseDialog` (`lib/widgets/add_test_case_dialog.dart`)

A `Dialog` widget for creating a new test case.

**Input Modes (toggle):**
- **Excel** — shows a file picker area. Tapping opens `FilePicker` filtered to `.xlsx`/`.xls` files.
- **Google Sheets** — shows a URL/ID text field.

**Upload Flow:**
1. Validates case name is not empty.
2. Ensures file is selected (Excel) or URL is provided (Sheets).
3. Ensures filename has `.xlsx` extension (Android FilePicker may omit it).
4. Calls `UploadService.uploadExcelFile()` or `uploadGoogleSheet()`.
5. Assembles `inputJson = { employees, vehicles, metadata, baseline }`.
6. Calls `DataService.uploadTestCase()`.
7. On success: calls `onSuccess` callback (triggers home page reload).
8. Errors shown as an inline red banner inside the dialog.

---

### `TestCaseCard` (`lib/widgets/test_case_card.dart`)

Displays a single test case row in the home list.

**States:**
- Normal: folder icon, case name, date.
- Selection mode: circle checkbox icon (filled when selected).
- Selected: blue border ring.
- Pinned: pin icon shown in the trailing section.

**Actions (trailing row):**
- Pin toggle, Rename, Delete — each calls the corresponding callback from `HomePage`.

---

### `HomeDrawer` (`lib/widgets/home_drawer.dart`)

Left-side navigation drawer.

**Contents:**
- `UserAccountsDrawerHeader` with gradient background and user email.
- **Colour Theme Picker** — 3 animated chips:
  - **Petronas** (`#00D2BE` teal) — default
  - **Orange** (`#FF8000`)
  - **Yellow** (`#F5B800`)
  - Selection updates `themeIndexNotifier` which propagates to the root `MaterialApp`.
- **Dark / Light Mode Toggle** — toggles `themeNotifier` between `ThemeMode.dark` and `ThemeMode.light`.
- **Logout** — calls `AuthService.signOut()`.

---

### `FilterBottomSheet` (`lib/widgets/filter_bottom_sheet.dart`)

A modal bottom sheet for the home page sort control.

4 option chips in a 2×2 grid:
- Newest First / Oldest First
- Name A→Z / Name Z→A

Tapping any chip calls `onSortChanged` and dismisses the sheet.

---

### `MapViewWidget` (`lib/widgets/map_view.dart`)

Standalone map widget used inside `ShowInputPage`.

- Auto-computes center from mean of all pickup and vehicle coordinates.
- Falls back to Bengaluru (`12.9716, 77.5946`) if no coordinates are present.
- Tapping employee markers fires `onEmployeeTap`.
- Tapping vehicle markers fires `onVehicleTap`.
- Map tile colour adjusted via `ColorFilter.matrix`:
  - Dark mode (Petronas theme): custom dark teal matrix.
  - Dark mode (other themes): neutral charcoal matrix.
  - Light mode: desaturated light matrix.

---

### `OutputSummaryPanel` (`lib/widgets/output_summary_panel.dart`)

Key metrics for the optimisation result. See [Section 9.4](#tab-2-overview--summary-sub-tab) for full details.

---

### `OutputViolationsPanel` (`lib/widgets/output_violations_panel.dart`)

Constraint violation breakdown. See [Section 9.4](#tab-2-overview--violations-sub-tab) for full details.

---

### `OutputChartsPanel` (`lib/widgets/output_charts_panel.dart`)

Data charts using `fl_chart`. See [Section 9.4](#tab-3-charts) for full details.

---

### `DownloadDialog` (`lib/widgets/download_dialog.dart`)

Bottom sheet for export format selection. Three chips: JSON, Excel, PDF.

---

### UI Elements (`lib/elements/`)

| Element | Description |
|---|---|
| `AppSnackbar.show(context, message, isError)` | Shows a themed snack bar (green success / red error) |
| `LoadingOverlay` | Full-screen blur overlay with a circular indicator |
| `SliverLoading` | Skeleton shimmer placeholder for the `CustomScrollView` loading state |
| `AppSpinner` | Simple inline circular progress indicator |

---

## 12. Theming System

The app supports **3 colour themes × 2 light/dark modes = 6 appearance variants**.

### Colour Themes

| Index | Name | Primary Colour | Use |
|---|---|---|---|
| 0 | Petronas | `#00D2BE` (teal) | Default |
| 1 | Orange | `#FF8000` | Alternative |
| 2 | Yellow | `#F5B800` | Alternative |

Theme index is stored in `themeIndexNotifier` (a `ValueNotifier<int>` created in `_MyAppState` and passed down through the widget tree).

### Light / Dark Mode

`themeNotifier` is a `ValueNotifier<ThemeMode>`, toggled from `HomeDrawer`.

### AppTheme Class

`AppTheme.lightThemeAt(index)` and `AppTheme.darkThemeAt(index)` return `ThemeData` objects with:
- Primary colour seed
- Surface / background colours from `AppColors`
- Input decoration theme
- Card theme
- Text theme

### AppColors Constants

```dart
AppColors.darkBackground    // #090909
AppColors.darkSurface       // #1A1A1A
AppColors.lightBackground   // #EAEAEA
AppColors.lightSurface      // #F5F5F5
AppColors.error             // #EF4444
AppColors.warning           // #F59E0B
AppColors.silver            // #C0C0C0 (employee markers)
```

### Performance Optimisation

The `AuthGate` widget is **cached** in `_MyAppState._cachedAuthGate` to prevent subtree recreation on every theme change. Only the `MaterialApp` shell rebuilds.

---

## 13. Output Data Format

The backend returns JSON with two format variants that `OutputPage` handles transparently.

### Format A — Raw Solver Output

```json
{
  "vehicles": [
    {
      "vehicle_id": "V01",
      "total_distance": 42.3,
      "total_time": 95,
      "total_cost": 528.75,
      "trips": [
        {
          "trip_number": 1,
          "total_cost": 528.75,
          "total_distance": 42.3,
          "total_time": 95,
          "stops": [
            {
              "location": "E03 Pickup",
              "arrival_time": "07:45",
              "departure_time": "07:47",
              "distance_from_prev": 5.2,
              "wait_time": 2
            },
            {
              "location": "Office Drop",
              "arrival_time": "08:55",
              "departure_time": "08:55",
              "distance_from_prev": 12.8,
              "wait_time": 0
            }
          ]
        }
      ]
    }
  ]
}
```

### Format B — Backend API Response

```json
{
  "optimization_id": "opt_17718...",
  "result": { "total_cost": 1240.50, "..." },
  "routes": [
    {
      "vehicle_id": "V01",
      "total_cost": 528.75,
      "total_distance": 42.3,
      "trips_count": 1,
      "passengers_count": 5,
      "capacity_utilization": 83.3,
      "route_points": [
        {
          "type": "pickup",
          "employee_id": "E03",
          "arrival_time": "07:45",
          "departure_time": "07:47",
          "distance_from_prev": 5.2,
          "lat": 12.9352,
          "lng": 77.6152
        },
        {
          "type": "office",
          "arrival_time": "08:55",
          "lat": 12.9716,
          "lng": 77.5946
        }
      ]
    }
  ],
  "assignments": [...],
  "violation_details": {
    "capacity_violations": [],
    "unassigned_employees": [],
    "time_window_violations": [],
    "sharing_pref_violations": [],
    "vehicle_pref_violations": []
  }
}
```

`OutputPage._parseVehicleRoutes()` detects which format is present by checking for `vehicles` vs `routes` key and delegates to the appropriate parser.

**Phantom Trip Filtering:** In Format A, trips where all stops are office/depot AND total distance is 0 are silently dropped from the display. Remaining trips are re-numbered sequentially (1, 2, 3...).

---

## 14. File Export System

### JSON Export

```dart
const encoder = JsonEncoder.withIndent('  ');
bytes = utf8.encode(encoder.convert(data));
// Saved as: optimization_output_YYYYMMDD_HHmmss.json
```

### Excel Export (multi-sheet)

Uses the `excel` package to generate an `.xlsx` workbook with:

- **Summary sheet** — total cost, time, distance, vehicles used/available, hard violations, soft violations, baseline cost, cost savings, generation timestamp.
- **Vehicle Routes sheet** — one row per stop across all vehicles/trips: vehicle ID, trip number, stop location, arrival time, departure time, distance from previous stop.

### PDF Export

Uses the `pdf` package with `PdfGoogleFonts` from `printing` for font embedding.

- Runs in a background isolate via `compute(FileExportService._generatePdf, data)` to avoid freezing the UI during PDF generation (font embedding + zlib compression takes several hundred milliseconds).
- Contains formatted tables similar to the Excel export.

### Android Permission Handling

| Android API Level | Strategy |
|---|---|
| API 33+ (Android 13+) | No permission needed for Downloads folder |
| API 30–32 (Android 11–12) | First tries `MANAGE_EXTERNAL_STORAGE`, falls back to `READ_EXTERNAL_STORAGE` |
| API ≤29 (Android 10 and below) | Requests `READ_EXTERNAL_STORAGE` / `WRITE_EXTERNAL_STORAGE` |

---

## 15. Small Features & Details

### Debounced Interactions

| Interaction | Delay | Why |
|---|---|---|
| Search text changes | 300ms | Avoid rebuilding on every keystroke |
| Scroll listener | 100ms | Reduce state updates while scrolling |

### Processed List Caching

`_processedTestCases` is a computed getter with a cache. It only re-filters and re-sorts when `_needsReprocessing` is `true`. Simple operations (pin, sort change, search change) set the flag; reads return the cached list. This avoids O(N) list processing on every `build()`.

### Responsive Grid Layout

Home page grid delegate switches:
- Width > 600px: 2-column grid (tablets, web)
- Width ≤ 600px: single column (phones)

### Parallel Batch Delete

When deleting multiple selected test cases:
```dart
await Future.wait(ids.map((id) => _dataService.deleteTestCase(id)));
```
All delete requests are fired concurrently rather than sequentially.

### City Inference from Coordinates

`ShowInputPage._resolveCity()` resolves city name using this priority chain:
1. `metadata.city` field
2. Employee `city` / `drop_city` / `location_city` field
3. Bounding box check on `drop_lat`/`drop_lng` for 6 Indian cities (Bengaluru, Delhi, Mumbai, Hyderabad, Chennai, Kolkata)
4. Fallback: "Bengaluru"

### Map Dark/Light Matrix

Three colour filter matrices are defined in `AppThemeData`:
- `mapDarkMatrix` — teal-tinted dark matrix for Petronas theme
- `mapDarkNeutralMatrix` — neutral charcoal for Orange/Yellow themes
- `mapLightMatrix` — desaturated light matrix for light mode

### Employee ID Extraction from Stop Strings

In Format A (raw solver), stop locations are strings like "E03 Pickup". The output parser uses a regex `r'^(E\d+)\s'` to extract the employee ID for marker labelling.

### Filename Safety on Android

File picker on Android can return display names without extensions. The upload dialog enforces `.xlsx` extension:
```dart
final safeFileName = rawName.toLowerCase().endsWith('.xlsx') || rawName.toLowerCase().endsWith('.xls')
    ? rawName
    : '$rawName.xlsx';
```

### Lottie Loading Animation

Two Lottie JSON assets (`loading_animation.json`, `Downloading.json`) are used on the optimisation loading overlay and download spinner, providing smooth animated feedback during long operations.

### Priority Label Mapping

Employee priorities are stored as integers and displayed as human-readable labels:

| Number | Label |
|---|---|
| 1 | CRITICAL |
| 2 | HIGH |
| 3 | MEDIUM |
| 4 | LOW |
| 5 | FLEX |

### Optimisation Config Merging

The request body is built using Dart's spread operator:
```dart
final requestBody = {
  ...inputData,               // All employee/vehicle/metadata fields
  'mode': mode,               // Solver effort
  if (costWeight != null) 'costWeight': costWeight,  // Conditional merging
  if (timeWeight != null) 'timeWeight': timeWeight,
};
```
Only non-null parameters are sent.

### Scroll-to-Card from Map Marker

When a user taps an employee marker on the input map:
1. Outer tab controller animates to Data tab (300ms).
2. After 420ms delay (enough for tab + list rebuild), `ScrollPosition.ensureVisible()` is called directly on the scroll position with `alignmentPolicy: explicit`.
3. The targeted card is highlighted for 3 seconds, then the highlight is cleared.

Using `ScrollPosition.ensureVisible` directly avoids accidentally scrolling parent scrollables (e.g., PageView).

---

## 16. Dependencies

```yaml
# Core Flutter
flutter: sdk: flutter

# Data & Networking
supabase_flutter: ^2.12.0    # Auth + database
http: ^1.6.0                 # HTTP requests
http_parser: ^4.1.2          # Multipart request helpers

# File Handling
file_picker: ^10.3.8         # Pick Excel files from device
path_provider: ^2.1.5        # Get Downloads/Documents directory
permission_handler: ^12.0.1  # Request Android storage permission
open_file: ^3.5.11           # Open exported files

# Data Parsing & Export
excel: ^4.0.0                # Read/write .xlsx files
pdf: ^3.11.3                 # Generate PDF reports
printing: ^5.14.2            # Embed Google Fonts in PDF
intl: ^0.20.2                # Date formatting

# Maps
flutter_map: ^7.0.0          # OpenStreetMap integration
latlong2: ^0.9.1             # LatLng type

# Charts
fl_chart: ^0.70.2            # Bar charts, line charts

# Animations & UI
lottie: ^3.1.0               # Lottie JSON animations (loading spinner)
device_info_plus: ^12.3.0    # Android API level detection for permissions
```

---

## Quick Start Summary

```
1. Clone repository
2. cd App/flutter_application_1
3. flutter pub get
4. Ensure Supabase project is set up (see auth_service.dart for URL/key)
5. Set useLiveServer = true in env.dart (or false + run adb reverse for local)
6. flutter run
7. Register account → Upload Excel test case → View input data → Optimise → View results → Export
```

---

*This document covers the complete Flutter mobile application layer of the VRP Optimisation system. For the C++ solver and Python Flask backend, see `Server/README.md`. For the web frontend, see `Frontend/README.md`.*
