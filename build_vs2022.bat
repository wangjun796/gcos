@echo off
REM ========================================
REM GCOS VM Build Script for Visual Studio 2022
REM ========================================

echo.
echo ========================================
echo Building GCOS VM with Visual Studio 2022
echo ========================================
echo.

REM 设置环境变量
set "VCVARSALL=-DNDEBUG"
set "VCINSTALLDIR=%VS160COMNTOOLS%"

REM 检查MSBuild是否存在
if not exist "%VCINSTALLDIR%\MSBuild\Bin\MSBuild.exe" (
    echo ERROR: MSBuild not found at %VCINSTALLDIR%\MSBuild\Bin\MSBuild.exe%
    echo Please install Visual Studio Build Tools or run this script from Visual Studio Developer Command Prompt
    pause
    exit /b 1
)

REM 设置项目路径
set "PROJECT_DIR=%~dp0"
set "SOLUTION_FILE=%PROJECT_DIR%\gcos_vm.sln"
set "CONFIGURATION=Debug"

REM 清理旧的构建输出
if exist "%PROJECT_DIR%d\%CONFIGURATION%" (
    echo Cleaning old build output...
    rmdir /s /q "%PROJECT_DIR%d\%CONFIGURATION%"
)

REM 执行构建
echo.
echo Building %CONFIGURATION% configuration...
echo.
MSBuild "%SOLUTION_FILE%" /t:Build /p:Configuration=%CONFIGURATION% /v:m

REM 检查构建结果
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build completed successfully!
    echo ========================================
    echo.
    echo Output directory: %PROJECT_DIR%d\%CONFIGURATION%
    echo.
    echo To build the project, you can also:
    echo   1. Open gcos_vm.sln in Visual Studio 2022
    echo   2. Run this script from Visual Studio Developer Command Prompt
    echo.
    echo To run the test program:
    echo   cd %PROJECT_DIR%d\%CONFIGURATION%
    echo   main_test.exe
    echo.
) else (
    echo.
    echo ========================================
    echo Build FAILED with error code: %ERRORLEVEL%
    echo ========================================
    echo.
    echo Please check the error messages above for details.
    echo.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Press any key to exit...
pause >nul
