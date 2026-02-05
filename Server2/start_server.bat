@echo off
echo ========================================
echo VRP Solver - Server Startup
echo ========================================
echo.

REM Check if virtual environment exists
if not exist "venv" (
    echo Creating Python virtual environment...
    python -m venv venv
    echo.
)

REM Activate virtual environment
echo Activating virtual environment...
call venv\Scripts\activate.bat
echo.

REM Install Python dependencies
echo Installing/Updating Python dependencies...
pip install -r requirements.txt
echo.

REM Check if frontend build exists
if not exist "frontend\dist" (
    echo Frontend build not found. Building frontend...
    cd frontend
    call npm install
    call npm run build
    cd ..
    echo.
)

REM Build C++ solver if needed
if not exist "vrp_solver_custom.exe" (
    echo Building C++ solver...
    call build.bat
    echo.
)

REM Start Flask server
echo ========================================
echo Starting Flask server on http://localhost:5000
echo ========================================
echo.
python app.py
