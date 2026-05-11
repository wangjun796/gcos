# GCOS VM 编译和测试指南

## 概述

本文档说明如何手动编译和测试GCOS VM项目，由于自动化构建脚本可能受到系统限制。

## 手动编译步骤

### 方法1: 使用命令行编译

```bash
# 进入项目目录
cd e:iews\gcos\prog\cos\gcos_vm

# 创建构建目录
mkdir build
cd build

# 生成Makefile（如果使用MinGW）
cmake .. -G "MinGW Makefiles" ..

# 编译Debug版本
cmake --build . --config Debug

# 编译Release版本
cmake --build . --config Release
```

### 方法2: 使用Visual Studio 2022编译

1. 打开Visual Studio 2022
2. 打开文件：`e:iews\gcos\prog\cos\gcos_vm\gcos_vm.sln`
3. 选择配置：
   - Debug x64
   - Release x64
4. 点击"生成"→"生成解决方案"
5. 等待编译完成

### 方法3: 使用MSBuild命令行

```cmd
cd e:iews\gcos\prog\cos\gcos_vm

# 清理旧的构建
if exist build rmdir /s /q build

# 生成Makefile
cmake .. -G "Visual Studio 17 2022" ..

# 编译Debug版本
msbuild gcos_vm.sln /p:Configuration=Debug /t:Rebuild /v:m

# 编译Release版本
msbuild gcos_vm.sln /p:Configuration=Release /t:Rebuild /v:m
```

## 编译输出

编译成功后，输出文件位于：
- `build\Debug\` - Debug版本
- `build\Release\` - Release版本

可执行文件：
- `build\Debug\main_test.exe` - Debug测试程序
- `build\Release\main_test.exe` - Release测试程序

## 运行测试

### Debug版本测试

```cmd
cd e:iews\gcos\prog\cos\gcos_vmuild\Debug
main_test.exe
```

### Release版本测试

```cmd
cd e:iews\gcos\prog\cos\gcos_vmuild\Release
main_test.exe
```

## 常见编译错误及解决

### 错误1: 找不到头文件

**错误信息**：
```
fatal error C1083: Cannot open include file: 'gcos_vm_full.h': No such file or directory
```

**解决方法**：
1. 确保在正确的目录下执行编译命令
2. 检查头文件是否存在于`include/`目录
3. 检查CMakeLists.txt中的路径配置

### 错误2: 链接错误

**错误信息**：
```
unresolved external symbol
```

**解决方法**：
1. 确保所有源文件都已添加到项目中
2. 清理旧的构建文件后重新编译
3. 检查是否所有依赖的头文件都存在

### 错误3: MSBuild找不到

**错误信息**：
```
'msbuild' is not recognized as an internal or external command
```

**解决方法**：
1. 确保已安装Visual Studio 2022或Build Tools
2. 使用Visual Studio Developer Command Prompt而不是普通CMD
3. 或使用完整路径调用MSBuild：
   ```
   "C:\Program Files\Microsoft Visual Studio2\Community\VC\Tools\Current\Bin\MSBuild.exe"
   ```

## 测试程序说明

测试程序`main_test.c`将执行以下测试：

1. **VM创建和初始化测试**
   - 创建虚拟机实例
   - 初始化虚拟机
   - 验证初始状态

2. **模块加载测试**
   - 加载模拟的SEF文件
   - 验证模块加载结果

3. **应用管理测试**
   - 安装模拟应用
   - 选择应用
   - 取消选择应用

4. **事务管理测试**
   - 开始事务
   - 提交事务
   - 测试嵌套事务
   - 回滚事务

5. **指令执行测试**
   - 执行NOP指令
   - 执行常量加载指令
   - 执行算术运算指令
   - 执行跳转指令

## 预期输出

测试程序成功运行后，应该看到以下输出：

```
========================================
GCOS VM Test Suite
Version: 1.0.0
========================================

========================================
Test: VM Creation and Initialization
========================================
[OK] VM Creation and Initialization PASSED

========================================
VM State: IDLE
PC: 0
SP: 0, BP: 0
Frame Top: 0
Module: (null)
App: (null)
Channel: 0
Instructions: 0
========================================

========================================
Test: Module Loading
========================================
[OK] Module Loading PASSED

========================================
... (其他测试结果)
```

## 调试建议

如果遇到问题，可以：

1. **启用详细日志**：在源代码中添加更多printf语句
2. **使用调试器**：在Visual Studio中设置断点调试
3. **检查内存**：使用内存检查工具检测内存泄漏
4. **验证头文件**：确保所有include路径正确

## 联系方式

如有问题或建议，请通过以下方式联系：

1. 查看项目文档：`README_FULL.md`
2. 查看实现计划：`IMPLEMENTATION_PLAN.md`
3. 查看架构设计：`ARCHITECTURE.md`

---

**注意**：本项目基于COS3规范实现，确保编译环境支持C99标准。
