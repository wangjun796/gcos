# Quick Test Script for JCShell Server
# This script starts the server and runs the client test

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  GCOS VM JCShell Quick Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$serverPath = "build\Debug\gcos_demo.exe"
$clientPath = "build\Debug\test_jcshell_client.exe"

# Check if executables exist
if (-not (Test-Path $serverPath)) {
    Write-Host "ERROR: gcos_demo.exe not found!" -ForegroundColor Red
    Write-Host "Please build first: cmake --build build --config Debug" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $clientPath)) {
    Write-Host "ERROR: test_jcshell_client.exe not found!" -ForegroundColor Red
    Write-Host "Please build first: cmake --build build --config Debug" -ForegroundColor Yellow
    exit 1
}

Write-Host "[1/3] Starting JCShell server..." -ForegroundColor Green
Write-Host ""

# Start server in background
$serverProcess = Start-Process -FilePath $serverPath -ArgumentList "tcp" -PassThru -WindowStyle Normal
Write-Host "      Server PID: $($serverProcess.Id)" -ForegroundColor Gray

Write-Host "[2/3] Waiting for server to initialize (3 seconds)..." -ForegroundColor Green
Start-Sleep -Seconds 3

Write-Host ""
Write-Host "[3/3] Running test client..." -ForegroundColor Green
Write-Host ""

# Run test client
& $clientPath

$exitCode = $LASTEXITCODE

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan

if ($exitCode -eq 0) {
    Write-Host "  ✓ All tests PASSED!" -ForegroundColor Green
} else {
    Write-Host "  ✗ Tests FAILED (exit code: $exitCode)" -ForegroundColor Red
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "NOTE: Server is still running (PID: $($serverProcess.Id))" -ForegroundColor Yellow
Write-Host "To stop the server, close its window or run:" -ForegroundColor Yellow
Write-Host "  Stop-Process -Id $($serverProcess.Id)" -ForegroundColor Gray
Write-Host ""
