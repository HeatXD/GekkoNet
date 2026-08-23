@echo off
set EXE=%~dp0x64\Debug\SpectatorSession.exe
if not exist "%EXE%" set EXE=%~dp0x64\Release\SpectatorSession.exe
if not exist "%EXE%" set EXE=%~dp0..\..\x64\Debug\SpectatorSession.exe
if not exist "%EXE%" set EXE=%~dp0..\..\x64\Release\SpectatorSession.exe
if not exist "%EXE%" (
    echo ERROR: SpectatorSession.exe not found. Build the project first.
    pause
    exit /b 1
)
set SECOND_JOIN_DELAY=10
set THIRD_JOIN_DELAY=20
echo Starting Host on port 7000 with three spectator slots...
start "Host" "%EXE%" -h 7000 7001 7002 %SECOND_JOIN_DELAY% 7003 %THIRD_JOIN_DELAY%

echo Starting Spectator 1 on port 7001 with the session...
start "Spectator 1 - Initial" "%EXE%" -s 7001 7000

timeout /t %SECOND_JOIN_DELAY% /nobreak >nul
echo Starting Spectator 2 on port 7002 after %SECOND_JOIN_DELAY% seconds...
start "Spectator 2 - Late" "%EXE%" -s 7002 7000

set /a FINAL_WAIT=%THIRD_JOIN_DELAY%-%SECOND_JOIN_DELAY%
timeout /t %FINAL_WAIT% /nobreak >nul
echo Starting Spectator 3 on port 7003 after %THIRD_JOIN_DELAY% seconds...
start "Spectator 3 - Late" "%EXE%" -s 7003 7000
