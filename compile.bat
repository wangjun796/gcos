@echo off
REM ========================================
REM GCOS VM Compile Script
REM ========================================

echo.
echo ========================================
echo Compiling GCOS VM Project
echo ========================================
echo.

REM 设置项目路径
set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%PROJECT_DIR%\build"

REM 创建构建目录
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
    echo Created build directory: %BUILD_DIR%
)

REM 编译测试程序
echo Compiling test program...
cl /nologo /c99 /I"%PROJECT_DIR%\include" /Fe"%BUILD_DIR%\main_test.exe" "%PROJECT_DIR%\main_test.c"
if errorlevel 1 (
    echo ERROR: Failed to compile test program
    pause
    exit /b 1
)

echo Test program compiled successfully.

REM 编译说明
echo.
echo ========================================
echo Compilation Complete
echo ========================================
echo.
echo Build directory: %BUILD_DIR%
echo Test executable: %BUILD_DIR%\main_test.exe
echo.
echo To run the test:
echo   cd %BUILD_DIR%
echo   main_test.exe
echo.
echo Press any key to exit...
pause >nul
