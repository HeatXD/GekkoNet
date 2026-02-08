@echo off
set EXE=%~dp0..\..\x64\Debug\OnlineSession.exe
if not exist "%EXE%" set EXE=%~dp0..\..\x64\Release\OnlineSession.exe
if not exist "%EXE%" (
    echo ERROR: OnlineSession.exe not found. Build the project first.
    pause
    exit /b 1
)
echo Starting 2-player Online Session on ports 7000 and 7001...
start "" "%EXE%" 0 7000 7001
timeout /t 1 >nul
start "" "%EXE%" 1 7000 7001
