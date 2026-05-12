@echo off
REM ============================================================================
REM JCShell Server and Client Test Script
REM ============================================================================
REM This script:
REM 1. Starts the GCOS VM with JCShell server (ports 9000/9900)
REM 2. Waits for server to start
REM 3. Runs the test client
REM ============================================================================

echo ========================================
echo   JCShell Server + Client Test
echo ========================================
echo.

REM Check if executables exist
if not exist "build\Debug\gcos_demo.exe" (
    echo ERROR: gcos_demo.exe not found!
    echo Please build the project first: cmake --build build --config Debug
    exit /b 1
)

if not exist "build\Debug\test_jcshell_client.exe" (
    echo ERROR: test_jcshell_client.exe not found!
    echo Please build the project first: cmake --build build --config Debug
    exit /b 1
)

echo [Test] Starting JCShell server...
echo.

REM Start server in background
start "GCOS VM - JCShell Server" build\Debug\gcos_demo.exe tcp

REM Wait for server to initialize
echo [Test] Waiting 3 seconds for server to start...
timeout /t 3 /nobreak >nul

echo.
echo [Test] Running test client...
echo.

REM Run test client
build\Debug\test_jcshell_client.exe

echo.
echo [Test] Test completed!
echo.
echo NOTE: The server is still running in a separate window.
echo Press Ctrl+C in that window to stop the server.
echo.

pause
