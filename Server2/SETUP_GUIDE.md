# VRP Solver - Server Setup Guide

## Quick Start

### Option 1: Production Mode (Recommended)
Run both backend and frontend from a single server on port 5000:

```bash
start_server.bat
```

Then open your browser to: **http://localhost:5000**

### Option 2: Development Mode
Run backend and frontend separately for development:

```bash
start_dev.bat
```

- Backend API: **http://localhost:5000**
- Frontend Dev: **http://localhost:3000**

---

## Prerequisites

### Required Software
1. **Python 3.8+** - [Download here](https://www.python.org/downloads/)
2. **Node.js 18+** - [Download here](https://nodejs.org/)
3. **C++ Compiler** (MinGW-w64 or Visual Studio)

### Verify Installation
```bash
python --version
node --version
npm --version
g++ --version
```

---

## Setup Instructions

### 1. Install Python Dependencies

Create and activate virtual environment:
```bash
python -m venv venv
venv\Scripts\activate.bat
```

Install requirements:
```bash
pip install -r requirements.txt
```

### 2. Install Frontend Dependencies

Navigate to frontend folder:
```bash
cd frontend
npm install
cd ..
```

### 3. Build C++ Solver

```bash
build.bat
```

This creates `vrp_solver_custom.exe`

### 4. Build Frontend for Production

```bash
build_frontend.bat
```

This creates `frontend\dist` folder with optimized production files.

---

## Running the Server

### Production Mode (Single Server)

```bash
start_server.bat
```

This will:
- Activate Python virtual environment
- Install/update dependencies if needed
- Build frontend if not already built
- Start Flask server on port 5000
- Serve both API and frontend

**Access at: http://localhost:5000**

### Development Mode (Separate Servers)

```bash
start_dev.bat
```

This starts:
- Backend API server on port 5000
- Frontend dev server on port 3000 (with hot reload)

**Access at: http://localhost:3000** (automatically proxies API calls to port 5000)

---

## Manual Steps

### Build Frontend Only
```bash
cd frontend
npm run build
```

### Run Backend Only
```bash
venv\Scripts\activate.bat
python app.py
```

### Run Frontend Dev Server Only
```bash
cd frontend
npm run dev
```

---

## Project Structure

```
Server2/
├── app.py                    # Flask backend (serves API + frontend)
├── requirements.txt          # Python dependencies
├── build.bat                 # C++ solver build script
├── start_server.bat          # Production startup script
├── start_dev.bat            # Development startup script
├── build_frontend.bat       # Frontend build script
├── vrp_solver_custom.cpp    # C++ solver source
├── frontend/
│   ├── src/                 # React source code
│   ├── dist/                # Production build (created by npm run build)
│   ├── package.json
│   └── vite.config.ts
├── uploads/                 # User uploaded files
└── output/                  # Solver output files
```

---

## API Endpoints

All API endpoints are prefixed with `/api`:

- `GET /api/health` - Health check
- `POST /api/upload` - Upload Excel file
- `POST /api/optimize` - Run optimization
- `GET /api/download-solution` - Download solution JSON

---

## Troubleshooting

### Port Already in Use
If port 5000 is in use, edit `app.py` and change:
```python
app.run(debug=True, host='0.0.0.0', port=5000)
```
to a different port (e.g., 8000).

Also update `frontend/vite.config.ts` proxy target.

### Frontend Build Not Found
Run:
```bash
build_frontend.bat
```

### Solver Executable Not Found
Run:
```bash
build.bat
```

### Python Dependencies Missing
```bash
venv\Scripts\activate.bat
pip install -r requirements.txt
```

### Node Modules Missing
```bash
cd frontend
npm install
```

---

## Environment Variables (Optional)

Create `.env` file in root directory:

```env
FLASK_ENV=production
FLASK_DEBUG=False
PORT=5000
MAX_UPLOAD_SIZE=16777216
```

---

## Deployment Notes

For production deployment:

1. Set `FLASK_DEBUG=False` in app.py or via environment variable
2. Use a production WSGI server (gunicorn, waitress):
   ```bash
   pip install waitress
   waitress-serve --port=5000 app:app
   ```
3. Consider using a reverse proxy (nginx, Apache)
4. Enable HTTPS for secure connections
5. Set appropriate CORS origins in app.py

---

## Support

For issues or questions:
1. Check the troubleshooting section
2. Review the main README.md for solver details
3. Check the browser console for frontend errors
4. Check Flask logs for backend errors
