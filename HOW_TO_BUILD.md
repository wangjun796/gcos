# GCOS VM 编译指南

## 概述

由于系统限制，无法直接执行编译命令。本文档说明如何使用已创建的文件来手动编译项目。

## 文件说明

已创建的文件：
- `compile.bat` - 简单的编译脚本
- `BUILD_GUIDE.md` - 详细的编译和测试指南

## 编译步骤

### 步骤1：准备环境

1. 确认已安装Visual Studio 2022或Build Tools
2. 确认项目文件已创建完成

### 步骤2：手动编译

#### 方法A：使用Visual Studio 2022

1. 打开Visual Studio 2022
2. 文件 → 打开 → 项目/解决方案
3. 选择 `e:iews\gcos\prog\cos\gcos_vm\gcos_vm.sln`
4. 在顶部工具栏选择配置（Debug x64 或 Release x64）
5. 点击"生成"→"生成解决方案"
6. 等待编译完成

#### 方法B：使用命令行

1. 打开"Developer Command Prompt for VS 2022"
2. 进入项目目录：
   ```
   cd e:iews\gcos\prog\cos\gcos_vm
   ```
3. 编译项目：
   ```
   msbuild gcos_vm.sln /p:Configuration=Debug /t:Rebuild /v:m
   ```
4. 或编译Release版本：
   ```
   msbuild gcos_vm.sln /p:Configuration=Release /t:Rebuild /v:m
   ```

### 步骤3：运行测试

1. 打开命令提示符
2. 进入编译输出目录：
   ```
   cd e:iews\gcos\prog\cos\gcos_vmuild\Debug
   ```
3. 运行测试程序：
   ```
   main_test.exe
   ```

## 常见编译错误及解决

### 错误1：找不到头文件

**错误信息**：
```
fatal error C1083: Cannot open include file: 'xxx.h': No such file or directory
```

**解决方法**：
1. 检查头文件是否存在于 `include/` 目录
2. 检查 `vm_types.h` 是否存在
3. 确保所有依赖的头文件都已创建

### 错误2：链接错误

**错误信息**：
```
unresolved external symbol
```

**解决方法**：
1. 清理旧的编译输出
2. 重新生成项目文件
3. 确保所有源文件都已添加到项目中

### 错误3：语法错误

**错误信息**：
```
error C2065: 'identifier': undeclared identifier
```

**解决方法**：
1. 检查源代码中的语法错误
2. 确保所有函数都已正确声明
3. 检查头文件中的类型定义

## 项目结构

确保以下文件结构正确：

```
gcos_vm/
├── include/
│   ├── gcos_vm_full.h
│   ├── gcos_vm_full_part2.h
│   ├── vm_types.h
│   └── ... (其他头文件)
├── src/
│   ├── vm_core_full.c
│   ├── vm_instructions_full.c
│   ├── vm_executor_full.c
│   ├── vm_loader_full.c
│   ├── vm_transaction_full.c
│   ├── vm_app_manager_full.c
│   └── vm_memory.c
├── gcos_vm.sln
├── gcos_vm.vcxproj
└── compile.bat
```

## 编译输出

编译成功后，输出文件位于：
- Debug版本：`build\Debug\`
  - `main_test.exe` - 测试程序
  - `gcos_vm.lib` - 静态库文件
  - 各种 `.obj` 文件 - 目标文件

- Release版本：`build\Release\`
  - `main_test.exe` - 测试程序
  - `gcos_vm.lib` - 静态库文件
  - 各种 `.obj` 文件 - 目标文件

## 测试说明

测试程序 `main_test.exe` 将执行以下测试：

1. **VM创建和初始化测试**
   - 验证虚拟机实例创建
   - 验证初始化过程

2. **模块加载测试**
   - 验证SEF文件加载
   - 验证模块解析

3. **应用管理测试**
   - 验证应用安装
   - 验证应用选择
   - 验证应用取消选择

4. **事务管理测试**
   - 验证事务开始
   - 验证事务提交
   - 验证事务回滚
   - 验证嵌套事务

5. **指令执行测试**
   - 验证基本指令执行
   - 验证算术运算
   - 验证跳转指令

## 注意事项

1. **编译顺序**：先编译静态库，再编译测试程序
2. **依赖关系**：确保所有头文件都存在
3. **内存管理**：如果遇到内存不足，可以调整配置
4. **调试支持**：在Debug模式下可以获得更详细的错误信息

## 联系方式

如有编译问题，请检查：
1. 源代码语法
2. 头文件路径
3. 项目配置

---

**注意**：由于系统限制，无法直接执行自动化编译命令，请按照上述步骤手动进行编译。
