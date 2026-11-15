@echo off
setlocal EnableDelayedExpansion
title CLauncher Installer

echo ========================================================================
echo                       CLauncher Windows Installer                        
echo ========================================================================
echo.

where powershell >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Windows PowerShell is not found on your system.
    echo Please ensure PowerShell is installed to run the CLauncher installer.
    echo.
    pause
    exit /b 1
)

:: Check if local install.ps1 is available in the script's directory
if exist "%~dp0install.ps1" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "irm https://raw.githubusercontent.com/RanoGames/CLauncher/master/install.ps1 | iex"
)

set EXITCODE=%ERRORLEVEL%
if %EXITCODE% neq 0 (
    echo.
    echo [!] Installation encountered an error (exit code: %EXITCODE%).
    pause
    exit /b %EXITCODE%
)

endlocal
