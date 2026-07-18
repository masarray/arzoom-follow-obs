@echo off
setlocal
cd /d "%~dp0"

where pwsh >nul 2>nul
if %ERRORLEVEL% EQU 0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local-windows.ps1" -VisualStudio Auto
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-local-windows.ps1" -VisualStudio Auto
)

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Build failed. Check the message above.
  echo Requirements: Git, CMake, Visual Studio 2026 or 2022 with Desktop development with C++.
  pause
  exit /b %ERRORLEVEL%
)

echo.
echo Done. Package files are in the release folder.
pause
