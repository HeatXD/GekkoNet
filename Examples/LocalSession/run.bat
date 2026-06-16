@echo off
set EXE=%~dp0..\..\x64\Debug\LocalSession.exe
if not exist "%EXE%" set EXE=%~dp0..\..\x64\Release\LocalSession.exe
if not exist "%EXE%" (
    echo ERROR: LocalSession.exe not found. Build the project first.
    pause
    exit /b 1
)
echo Starting 4-player Local Session...
start "" "%EXE%" 4
