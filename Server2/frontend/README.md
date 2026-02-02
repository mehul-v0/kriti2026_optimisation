# VRP Solver with React Frontend

A complete Vehicle Routing Problem solver with a modern React frontend.

## Features

- 📁 **Drag & Drop Excel Upload** - Easy file upload interface
- 📊 **Data Visualization** - Interactive route maps and dashboards
- 🚗 **Fleet Management** - View and analyze vehicle assignments
- 👥 **Employee View** - Detailed employee journey information
- 💰 **Cost Optimization** - Real-time optimization with cost savings analysis
- ⚡ **Fast Solver** - C++ based custom VRP solver

## Project Structure

```
Server2/
├── frontend/          # React + TypeScript frontend
│   ├── src/
│   │   ├── components/
│   │   ├── types/
│   │   ├── App.tsx
│   │   └── main.tsx
│   └── package.json
├── app.py            # Flask backend API
├── convert_excel_to_json.py
├── vrp_solver_custom.cpp
└── requirements.txt
```

## Installation

### 1. Backend Setup (Flask + Solver)

```bash
# Install Python dependencies
pip install -r requirements.txt

# Build the C++ solver (Windows)
build.bat

# Or build manually with g++
g++ -std=c++17 -O3 -o vrp_solver_custom.exe vrp_solver_custom.cpp
```

### 2. Frontend Setup (React)

```bash
cd frontend

# Install Node.js dependencies
npm install

# Start development server
npm run dev
```

### 3. Start the Backend

```bash
# In the Server2 directory
python app.py
```

The Flask backend will run on `http://localhost:5000`

### 4. Access the Application

Open your browser to `http://localhost:3000`

The React dev server proxies API requests to the Flask backend automatically.

## Usage

1. **Upload Excel File**
   - Drag and drop or click to browse for your Excel test case file
   - File must contain sheets: `employees`, `vehicles`, `metadata`, `baseline`

2. **Review Data Digest**
   - Check employee count, vehicle count, and fleet composition
   - View unoptimized employee pickup/drop locations on the map

3. **Run Optimization**
   - Click "Run Optimization" button
   - Watch progress in real-time
   - Solver will execute and return optimized routes

4. **View Results**
   - **KPI Dashboard**: Cost savings, distance, time, vehicles used
   - **Route Visualization**: Interactive map showing optimized routes
   - **Fleet View**: Vehicle-by-vehicle breakdown with assignments
   - **Employee View**: Individual employee journey details

## API Endpoints

### POST /api/upload
Upload Excel file and convert to JSON

**Request:** `multipart/form-data` with file

**Response:**
```json
{
  "success": true,
  "filename": "test.xlsx",
  "digest": { ... },
  "employees": [ ... ],
  "vehicles": [ ... ],
  "baseline_cost": 1500.0
}
```

### POST /api/optimize
Run the VRP solver on uploaded data

**Response:**
```json
{
  "success": true,
  "result": {
    "total_cost": 1200.0,
    "baseline_cost": 1500.0,
    "cost_savings": 300.0,
    "cost_savings_percent": 20.0,
    ...
  },
  "routes": [ ... ],
  "assignments": [ ... ]
}
```

### GET /api/download-solution
Download solution JSON file

## Excel File Format

Your Excel file should contain these sheets:

### employees
- employee_id
- pickup_lat, pickup_lng
- drop_lat, drop_lng
- priority (1=high, 2=medium, 3=low)
- earliest_pickup, latest_drop
- vehicle_preference
- sharing_preference

### vehicles
- vehicle_id
- current_lat, current_lng
- capacity
- cost_per_km
- avg_speed_kmph
- available_from
- category

### metadata
- Key-value pairs for weights and constraints

### baseline
- employee_id
- baseline_cost
- baseline_time

## Technology Stack

- **Frontend**: React 18, TypeScript, Tailwind CSS, Vite, Lucide Icons
- **Backend**: Flask, Flask-CORS, Python 3.x
- **Solver**: C++ with custom GLS algorithm
- **Data Processing**: Pandas, OpenPyXL

## Development

### Frontend Development
```bash
cd frontend
npm run dev     # Start dev server with hot reload
npm run build   # Build for production
npm run preview # Preview production build
```

### Backend Development
```bash
python app.py   # Runs with debug=True for auto-reload
```

## Production Deployment

1. Build the frontend:
```bash
cd frontend
npm run build
```

2. Serve static files from Flask (modify app.py to serve from `frontend/dist`)

3. Use production WSGI server:
```bash
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 app:app
```

## Troubleshooting

**Issue: Solver not found**
- Ensure `vrp_solver_custom.exe` (Windows) or `vrp_solver_custom` (Linux/Mac) exists
- Run `build.bat` or compile manually

**Issue: API requests failing**
- Check Flask server is running on port 5000
- Verify Vite proxy configuration in `vite.config.ts`

**Issue: File upload fails**
- Check file format matches expected Excel structure
- Verify file size is under 16MB
- Check console for detailed error messages

## License

MIT License
