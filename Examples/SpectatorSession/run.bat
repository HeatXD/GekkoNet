@echo off
set EXE=%~dp0..\..\x64\Debug\SpectatorSession.exe
if not exist "%EXE%" set EXE=%~dp0..\..\x64\Release\SpectatorSession.exe
if not exist "%EXE%" (
    echo ERROR: SpectatorSession.exe not found. Build the project first.
    pause
    exit /b 1
)
echo Starting Host on port 7000, Spectator on port 7001...
start "" "%EXE%" -h 7000 7001
timeout /t 1 >nul
start "" "%EXE%" -s 7001 7000
