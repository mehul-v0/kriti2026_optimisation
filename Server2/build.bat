@echo off
echo ========================================
echo Custom VRP Solver - Windows Build
echo ========================================

if not exist json.hpp (
    echo Downloading nlohmann/json...
    curl -o json.hpp https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
    if errorlevel 1 (
        echo Error: Download failed
        pause
        exit /b 1
    )
    echo ✓ Downloaded json.hpp
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo.
    echo Compiling with g++...
    g++ -std=c++17 -O3 -Wall -Wextra -I. vrp_solver_custom.cpp -o vrp_solver_custom.exe
    if errorlevel 1 (
        echo ❌ Build failed
        pause
        exit /b 1
    )
    echo.
    echo ✅ Build successful: vrp_solver_custom.exe
    goto :end
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo.
    echo Compiling with Visual Studio...
    cl /EHsc /O2 /std:c++17 /I. vrp_solver_custom.cpp /Fe:vrp_solver_custom.exe
    if errorlevel 1 (
        echo ❌ Build failed
        pause
        exit /b 1
    )
    echo.
    echo ✅ Build successful: vrp_solver_custom.exe
    goto :end
)

echo ❌ No C++ compiler found!
echo Install MinGW or Visual Studio
pause
exit /b 1

:end
echo.
echo ========================================
echo Build complete!
echo ========================================
pause
