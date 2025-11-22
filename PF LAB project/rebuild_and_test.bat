@echo off
REM Rebuild the Car Game with the latest high-score changes
REM Run this from the PF LAB project directory

echo ============================================
echo Building Car Game (GTK3 + Cairo)...
echo ============================================

REM Use MSYS2 MinGW bash to compile
bash.exe build\compile.sh

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================
    echo Build SUCCESSFUL!
    echo ============================================
    echo.
    echo Starting game from build directory...
    cd /d build
    car_game.exe
    cd ..
) else (
    echo.
    echo ============================================
    echo Build FAILED with error code %ERRORLEVEL%
    echo ============================================
    echo.
    echo Please ensure:
    echo   1. MSYS2 MinGW is installed
    echo   2. bash.exe is available in PATH or at: C:\msys64\usr\bin\bash.exe
    echo   3. GTK3 development files are installed in MSYS2
    echo.
    pause
    exit /b 1
)
