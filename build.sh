#!/bin/bash
# GCOS VM 构建脚本 (Linux/macOS)

echo "========================================"
echo "GCOS VM Build Script"
echo "========================================"
echo ""

# 检查CMake是否安装
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Please install CMake first."
    exit 1
fi

# 创建构建目录
mkdir -p build
cd build

# 运行CMake配置
echo "[1/3] Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release
if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed"
    cd ..
    exit 1
fi
echo ""

# 编译项目
echo "[2/3] Building project..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
if [ $? -ne 0 ]; then
    echo "Error: Build failed"
    cd ..
    exit 1
fi
echo ""

# 运行测试
echo "[3/3] Running tests..."
if [ -f test_vm ]; then
    ./test_vm
else
    echo "Error: test_vm not found"
fi
echo ""

cd ..

echo "========================================"
echo "Build completed!"
echo "========================================"
