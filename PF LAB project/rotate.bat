@echo off
REM Run PowerShell script to rotate car.png -> car_rotated.png (no build required)
SETLOCAL
if not exist "%~dp0assets" mkdir "%~dp0assets"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0\rotate.ps1" "%~dp0assets\car.png" "%~dp0assets\car_rotated.png"
if %ERRORLEVEL% EQU 0 (
    echo Rotation completed: %~dp0assets\car_rotated.png
) else (
    echo Rotation failed. See PowerShell output above.
)
ENDLOCAL
pause
