@echo off
setlocal
cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%"
if exist "build-final\fleetflow_app.exe" (
    .\build-final\fleetflow_app.exe
) else if exist "build-final-submission\fleetflow_app.exe" (
    .\build-final-submission\fleetflow_app.exe
) else (
    echo Error: fleetflow_app.exe not found.
)
pause
