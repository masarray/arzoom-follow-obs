@echo off
setlocal
cd /d "%~dp0"

where pwsh >nul 2>nul
if %ERRORLEVEL% EQU 0 (
  pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-existing-build.ps1"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package-existing-build.ps1"
)

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo Packaging failed. The compiled DLL is still safe in release\stage\RelWithDebInfo.
  pause
  exit /b %ERRORLEVEL%
)

echo.
echo Done. No rebuild was performed.
echo Package files are in the release folder.
pause
