@echo off
setlocal

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-release.ps1"
set "DME_BUILD_EXIT=%ERRORLEVEL%"

if not "%DME_BUILD_EXIT%"=="0" (
    echo.
    echo The release build failed. Read the error above.
    pause
    exit /b %DME_BUILD_EXIT%
)

echo.
echo The release package is ready in the release folder.
pause
