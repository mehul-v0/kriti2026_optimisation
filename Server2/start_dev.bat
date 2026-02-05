@echo off
echo ========================================
echo VRP Solver - Development Mode
echo ========================================
echo Starting backend and frontend in development mode...
echo.

REM Start backend in a new window
start "VRP Backend" cmd /k "call venv\Scripts\activate.bat && python app.py"

REM Wait a bit for backend to start
timeout /t 3 /nobreak > nul

REM Start frontend in a new window
start "VRP Frontend" cmd /k "cd frontend && npm run dev"

echo.
echo ========================================
echo Services started:
echo - Backend: http://localhost:5000
echo - Frontend: http://localhost:3000
echo ========================================
