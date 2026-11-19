@echo off
cd /d "%~dp0"
if exist "bin\terminal.exe" (
    "bin\terminal.exe"
) else (
    "build-host\terminal.exe"
)
if errorlevel 1 pause
