@echo off
REM ============================================================================
REM Modus - Stealth MeshAgent Service Cleanup Script
REM ============================================================================
REM Author: Joel Aaron Guff
REM Version: 1.0
REM This script safely removes the stealth MeshAgent service and components
REM 
REM This will:
REM - Stop and remove the svchost_net service
REM - Unload and delete the Cronos rootkit driver
REM - Remove svchost.dll from System32
REM - Clean up registry entries
REM ============================================================================

echo Modus - Stealth MeshAgent Service Cleanup
echo ==========================================

REM Check for administrative privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

echo [INFO] Administrative privileges confirmed

REM Warning about protected service
echo.
echo WARNING: This service may be protected by the Cronos rootkit
echo If the cleanup fails, the system may need to be rebooted
echo.
set /p continue="Continue with cleanup? (y/N): "
if /i not "%continue%"=="y" exit /b 0

REM Stop the service
echo [INFO] Stopping Network Host Service...
sc stop svchost_net
if %errorLevel% equ 0 (
    echo [SUCCESS] Service stopped
    timeout /t 3 /nobreak >nul
) else (
    echo [WARNING] Service may already be stopped or protected
)

REM Remove service from netsvcs group
echo [INFO] Removing service from netsvcs group...
for /f "tokens=3*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost" /v netsvcs ^| findstr "REG_MULTI_SZ"') do (
    set "netsvcs_list=%%a %%b"
)
set "netsvcs_list=%netsvcs_list:svchost_net=%"
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost" /v netsvcs /t REG_MULTI_SZ /d "%netsvcs_list%" /f >nul 2>&1

REM Delete the service
echo [INFO] Deleting Network Host Service...
sc delete svchost_net
if %errorLevel% equ 0 (
    echo [SUCCESS] Service deleted
) else (
    echo [WARNING] Failed to delete service - may be protected
)

REM Stop and remove Cronos driver
echo [INFO] Stopping Cronos rootkit driver...
sc stop cronos >nul 2>&1
sc delete cronos >nul 2>&1
if %errorLevel% equ 0 (
    echo [SUCCESS] Cronos driver removed
) else (
    echo [INFO] Cronos driver was not installed or already removed
)

REM Remove driver file
echo [INFO] Removing Cronos driver file...
del /f /q "%SystemRoot%\System32\drivers\Cronos.sys" >nul 2>&1
del /f /q "%SystemRoot%\System32\drivers\CronosRootkit.sys" >nul 2>&1

REM Remove DLL and configuration
echo [INFO] Removing svchost.dll and configuration...
del /f /q "%SystemRoot%\system32\svchost.dll" >nul 2>&1
del /f /q "%SystemRoot%\system32\taskmaster.msh" >nul 2>&1
if %errorLevel% equ 0 (
    echo [SUCCESS] DLL and configuration removed
) else (
    echo [WARNING] Failed to remove DLL or configuration - file may be in use
    echo The files will be removed on next reboot
    move "%SystemRoot%\system32\svchost.dll" "%SystemRoot%\system32\svchost.dll.deleted" >nul 2>&1
    move "%SystemRoot%\system32\taskmaster.msh" "%SystemRoot%\system32\taskmaster.msh.deleted" >nul 2>&1
)

REM Clean up registry entries
echo [INFO] Cleaning up registry entries...
reg delete "HKLM\SYSTEM\CurrentControlSet\Services\svchost_net" /f >nul 2>&1

REM Remove firewall rules
echo [INFO] Removing firewall rules...
netsh advfirewall firewall delete rule name="Network Host Service peer-to-peer (UDP)" >nul 2>&1
netsh advfirewall firewall delete rule name="Network Host Service management (UDP)" >nul 2>&1  
netsh advfirewall firewall delete rule name="Network Host Service peer-to-peer (TCP)" >nul 2>&1
netsh advfirewall firewall delete rule name="Network Host Service management (TCP)" >nul 2>&1
netsh advfirewall firewall delete rule name="Network Host Service background service" >nul 2>&1

REM Verify cleanup
echo.
echo [INFO] Verifying cleanup...
sc query svchost_net >nul 2>&1
if %errorLevel% neq 0 (
    echo [SUCCESS] Service successfully removed
) else (
    echo [WARNING] Service may still exist - reboot may be required
)

if not exist "%SystemRoot%\system32\svchost.dll" (
    echo [SUCCESS] DLL successfully removed  
) else (
    echo [WARNING] DLL still exists - reboot may be required
)

if not exist "%SystemRoot%\system32\taskmaster.msh" (
    echo [SUCCESS] Configuration successfully removed
) else (
    echo [WARNING] Configuration still exists - reboot may be required
)

echo.
echo ============================================================================
echo Cleanup Complete!
echo ============================================================================
echo.
echo The following components have been removed:
echo   - Network Host Service (svchost_net)
echo   - Cronos rootkit driver  
echo   - svchost.dll
echo   - taskmaster.msh configuration
echo   - Registry entries
echo   - Firewall rules
echo.
echo If any warnings appeared above, a system reboot may be required
echo to complete the removal process.
echo.
echo IMPORTANT: If the service was protected and cleanup failed,
echo you may need to boot from a rescue disk to remove remaining files.
echo.
pause
