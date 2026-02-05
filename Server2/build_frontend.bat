@echo off
echo ========================================
echo Building Frontend for Production
echo ========================================
echo.

cd frontend

REM Install dependencies if needed
if not exist "node_modules" (
    echo Installing dependencies...
    call npm install
    echo.
)

REM Build frontend
echo Building frontend...
call npm run build

echo.
echo ========================================
echo Frontend built successfully!
echo Build output: frontend\dist
echo ========================================

cd ..
