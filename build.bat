@echo off
REM GCOS VM 构建脚本 (Windows)

echo ========================================
echo GCOS VM Build Script
echo ========================================
echo.

REM 检查CMake是否安装
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake not found. Please install CMake first.
    pause
    exit /b 1
)

REM 创建构建目录
if not exist build mkdir build
cd build

REM 运行CMake配置
echo [1/3] Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake configuration failed
    cd ..
    pause
    exit /b 1
)
echo.

REM 编译项目
echo [2/3] Building project...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo Error: Build failed
    cd ..
    pause
    exit /b 1
)
echo.

REM 运行测试
echo [3/3] Running tests...
if exist Release\test_vm.exe (
    Release\test_vm.exe
) else if exist Debug\test_vm.exe (
    Debug\test_vm.exe
) else (
    echo Error: test_vm.exe not found
)
echo.

cd ..

echo ========================================
echo Build completed!
echo ========================================
pause
