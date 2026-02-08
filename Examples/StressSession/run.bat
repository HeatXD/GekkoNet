@echo off
set EXE=%~dp0..\..\x64\Debug\StressSession.exe
if not exist "%EXE%" set EXE=%~dp0..\..\x64\Release\StressSession.exe
if not exist "%EXE%" (
    echo ERROR: StressSession.exe not found. Build the project first.
    pause
    exit /b 1
)
echo Starting StressSession...
start "" "%EXE%"
