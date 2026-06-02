# Kriti Optimization 2026

A full-stack **Vehicle Routing Problem (VRP)** optimization platform for corporate employee transportation. Solves a **Multi-Trip Capacitated VRP with Time Windows (MT-CVRPTW)** — minimizing costs and travel time while respecting vehicle capacities, employee time windows, preferences, and priority-based scheduling.

**Live Demo:** [https://fkw404kkcccwg8k4gkg8wgwk.65.21.154.72.sslip.io/](https://fkw404kkcccwg8k4gkg8wgwk.65.21.154.72.sslip.io/)

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| **C++ Solver** | C++17, ALNS metaheuristic, 9 construction heuristics |
| **Backend** | Python 3.11, Flask, Gunicorn |
| **Web Frontend** | React 18, TypeScript, Vite, Tailwind CSS, Leaflet, Recharts |
| **Mobile App** | Flutter 3.10+, Supabase (Auth + PostgreSQL) |
| **Deployment** | Docker, Nginx, Coolify / Railway / Render |

---

## Features

- **Adaptive Large Neighborhood Search (ALNS)** with 9 construction heuristics for high-quality solutions
- **Real road distances** via OpenRouteService API (with haversine fallback)
- **Multi-trip support** — vehicles can make multiple round trips
- **Priority-aware scheduling** — higher-priority employees get stricter time guarantees
- **Preference matching** — vehicle type (premium/normal) and sharing preferences (single/double/triple)
- **Interactive map visualization** — Leaflet-based route explorer with actual road geometry
- **Comprehensive exports** — JSON, Excel, and PDF reports with cost projections
- **Real-time progress tracking** — live optimization stage updates
- **Mobile app** — Flutter app with Supabase auth and cloud-synced test cases

---

## Project Structure

```
├── server/                         # Backend + C++ Solver
│   ├── app.py                      # Flask REST API
│   ├── convert_excel_to_json.py    # Excel → JSON converter
│   ├── solver_config.json          # ALNS tunable parameters
│   ├── Dockerfile
│   ├── Makefile
│   ├── src/                        # C++ solver source
│   │   ├── vrp_solver_custom.cpp   # Main solver entry
│   │   ├── vrp_alns.h              # ALNS metaheuristic
│   │   ├── vrp_construction.h      # 9 construction heuristics
│   │   ├── vrp_local_search.h      # Local search operators
│   │   ├── vrp_constraints.h       # Constraint engine
│   │   ├── vrp_types.h             # Data structures
│   │   ├── vrp_parser.h            # JSON input parser
│   │   ├── vrp_output.h            # JSON output formatter
│   │   ├── vrp_validators.h        # Solution validation
│   │   └── ...
│   └── output/                     # Solver output files
│
├── frontend/                       # React Web Dashboard
│   ├── src/
│   │   ├── pages/                  # 11 feature pages
│   │   ├── components/             # Layout, Sidebar, Charts
│   │   ├── services/api.ts         # Backend API client
│   │   ├── context/                # Global state (AppContext, SidebarContext)
│   │   └── utils/                  # Data mappers & helpers
│   ├── Dockerfile
│   └── nginx.conf
│
├── App/flutter_application_1/      # Flutter Mobile App
│   ├── lib/
│   │   ├── screen/                 # Auth, Home, Input, Output pages
│   │   ├── services/               # Supabase, Upload, Optimize, Export
│   │   ├── widgets/                # UI components (maps, charts, cards)
│   │   ├── config/                 # Environment & map config
│   │   └── main.dart
│   └── assets/                     # Lottie animations
│
└── DEPLOYMENT.md                   # Deployment guide
```

---

## Running Locally

### Prerequisites

| Requirement | Version | What it's for |
|-------------|---------|---------------|
| **Python** | 3.11+ | Backend Flask server |
| **g++ (MinGW)** | C++17 support | Compiling the VRP solver |
| **Node.js** | 18+ | Frontend React app |
| **npm** | 9+ | Frontend package manager |
| **Flutter SDK** | 3.10+ | Mobile app (optional) |
| **ORS API Key** | Free | Real road distances ([sign up here](https://openrouteservice.org/dev/#/signup)) |

> **Windows users**: Install [MinGW-w64](https://www.mingw-w64.org/) or use MSYS2 to get `g++`. Make sure `g++` is in your PATH.

---

### 1. Backend (Python + C++ Solver)

Open a terminal in the project root:

```bash
cd server
```

#### Step 1 — Install Python dependencies

```bash
pip install -r requirements.txt
```

This installs: flask, flask-cors, pandas, openpyxl, requests, python-dotenv, gunicorn.

#### Step 2 — Compile the C++ solver

**On Windows:**
```bash
build.bat
```
This auto-downloads `json.hpp` if missing and compiles using g++ or MSVC.

**On Linux/macOS:**
```bash
make clean && make
```

Both produce `vrp_solver_custom.exe` (Windows) or `vrp_solver_custom` (Linux/macOS).

#### Step 3 — Set up the ORS API key

Create a `.env` file in the `server/` folder:

```
ORS_API_KEY=your_openrouteservice_api_key_here
```

Without this key, the solver still works but uses straight-line (haversine) distances instead of actual road distances.

#### Step 4 — Run the server

```bash
python app.py
```

The backend starts on **http://localhost:5000**. You should see:

```
============================================================
  VRP Solver Backend — http://localhost:5000
============================================================
  Solver status : ✓ Ready
============================================================
```

---

### 2. Frontend (React + Vite)

Open a **new terminal** in the project root:

```bash
cd frontend
```

#### Step 1 — Install dependencies

```bash
npm install
```

#### Step 2 — Run the dev server

```bash
npm run dev
```

The frontend starts on **http://localhost:5173**. API calls to `/api/*` are automatically proxied to the backend at `http://127.0.0.1:5000` (configured in `vite.config.ts`).

> **Important**: The backend must be running first. Start the backend (Step 1) before opening the frontend.

#### Production build (optional)

```bash
npm run build      # outputs to dist/
npm run preview    # preview the production build locally
```

---

### 3. Mobile App (Flutter) — Optional

Open a **new terminal** in the project root:

```bash
cd App/flutter_application_1
```

#### Step 1 — Install dependencies

```bash
flutter pub get
```

#### Step 2 — Configure the backend URL

Edit `lib/config/env.dart` and set the backend URL to your local server:

```dart
static const String backendUrl = 'http://localhost:5000';
```

> On Android emulator, use `http://10.0.2.2:5000` instead of `localhost`.

#### Step 3 — Run on a device or emulator

```bash
flutter run
```

#### Build release APK (optional)

```bash
flutter build apk --release
```

The APK is output to `build/app/outputs/flutter-apk/app-release.apk`.

---

### Quick Start Summary

Run these in **three separate terminals** (backend must start first):

```bash
# Terminal 1 — Backend
cd server
pip install -r requirements.txt
build.bat                        # Windows (or: make clean && make)
python app.py

# Terminal 2 — Frontend
cd frontend
npm install
npm run dev

# Terminal 3 — Mobile (optional)
cd App/flutter_application_1
flutter pub get
flutter run
```

Then open **http://localhost:5173** in your browser.

---

## Data Flow

```
Excel (.xlsx)
    │
    ▼
POST /api/upload  ──►  convert_excel_to_json.py
    │
    ▼
Frontend receives JSON (employees, vehicles, metadata)
    │
    ▼
POST /api/optimize  ──►  C++ Solver (vrp_solver_custom.exe)
    │                         │
    ▼                         ▼
Flask formats results    Background geometry fetch (ORS)
    │
    ▼
JSON response → Dashboard visualization
```

### Input Format (Excel)

The Excel file contains 4 sheets:

| Sheet | Contents |
|-------|----------|
| **Employees** | Pickup/drop coordinates, priority (1–5), time windows, vehicle/sharing preferences |
| **Vehicles** | Capacity, cost per km, average speed, availability, category (premium/normal) |
| **Baseline** | Individual trip cost and time benchmarks |
| **Metadata** | City, delay tolerances per priority, objective weights |

---

## Web Dashboard Pages

| Page | Description |
|------|-------------|
| **Landing Dashboard** | Lifetime metrics, session history, quick actions |
| **Data Upload** | Drag-and-drop Excel upload with validation |
| **Data Insights** | Time window charts, priority distributions, solver config |
| **Optimization Processing** | Real-time progress with stage updates |
| **Results Overview** | KPI summary, savings metrics |
| **Constraint Validation** | Hard vs soft constraint compliance |
| **Route Explorer** | Interactive Leaflet map with route geometry |
| **Vehicle Fleet** | Fleet utilization, fuel type, capacity charts |
| **Employee Assignments** | Searchable table with priority filtering |
| **Cost Breakdown** | Baseline vs optimized, daily/monthly/annual projections |
| **Export & Reports** | JSON, Excel, PDF export |

---

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/api/upload` | Upload Excel file, returns parsed JSON |
| `POST` | `/api/optimize` | Run VRP solver with configuration |
| `GET` | `/api/status` | Check optimization progress |
| `GET` | `/api/geometry` | Poll route geometry (background fetch) |

---

## C++ Solver

The solver (`vrp_solver_custom.exe`) implements an **Adaptive Large Neighborhood Search (ALNS)** algorithm:

1. **Construction phase** — runs 9 heuristics (including Clarke-Wright savings) to generate diverse initial solutions
2. **ALNS optimization** — iteratively destroys and repairs solutions using adaptive neighborhood selection
3. **Local search** — applies 2-opt, relocate, and other operators for refinement

```bash
# Direct usage
./vrp_solver_custom.exe <input.json> [output.json] [time_limit_seconds]
```

Parameters are tunable via `solver_config.json`.

---

## Deployment

Both backend and frontend are containerized with Docker.

```bash
# Backend
cd server
docker build -t kriti-backend .
docker run -e ORS_API_KEY=your_key -p 5000:5000 kriti-backend

# Frontend
cd frontend
docker build --build-arg VITE_API_URL=https://your-backend-url -t kriti-frontend .
docker run -p 80:80 kriti-frontend
```

### Environment Variables

| Variable | Service | Description |
|----------|---------|-------------|
| `ORS_API_KEY` | Backend (runtime) | OpenRouteService API key for real road distances |
| `VITE_API_URL` | Frontend (build-time) | Backend API URL (e.g. `https://api.example.com`) |

See [DEPLOYMENT.md](DEPLOYMENT.md) for detailed guides on Railway, Render, AWS, and DigitalOcean.

---

## License

This project was built for **Kriti 2026** — IIT Guwahati's annual technical event.
