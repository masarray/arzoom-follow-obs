@echo off
setlocal
cd /d "%~dp0"

echo [ArZoom v0.1.4] Fast runtime hotfix rebuild
echo Reusing the existing OBS downloads and dependency cache.
echo This update enables frontend/Qt support, so one CMake reconfigure is expected once.
echo It should not clone or download the OBS dependencies again.
echo.

where pwsh >nul 2>nul
if %ERRORLEVEL% EQU 0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local-windows.ps1" -VisualStudio Auto
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local-windows.ps1" -VisualStudio Auto
)

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Runtime hotfix build failed. Send the final error lines.
  pause
  exit /b %ERRORLEVEL%
)

echo.
echo [OK] Install the new v0.1.4 EXE or copy the ZIP payload into the OBS folder.
echo Completely close OBS before installing, then start OBS again.
pause
