# GCOS VM - 基于COS3规范的国产智能卡虚拟机

## 📖 项目概述

**GCOS VM** (GuoChao Operating System Virtual Machine) 是一个基于中华人民共和国国家标准 **GB/T 44901.3《卡及身份识别安全设备片上操作系统第3部分：支持面向过程应用后下载的基础层技术要求》** 实现的虚拟机系统。

### 🎯 设计目标

- ✅ **完全符合COS3规范** - 严格遵循国家标准要求
- ✅ **栈式字节码执行器** - 弹栈-执行-压栈计算模型
- ✅ **支持应用后下载** - SEF文件格式加载和安装
- ✅ **多通道应用隔离** - 最多8个逻辑通道独立运行
- ✅ **事务管理机制** - 原子性操作保证数据一致性
- ✅ **运行时安全管理** - 应用隔离、接口授权、异常处理
- ✅ **零动态内存分配** - 全局静态实例,适合嵌入式环境
- ✅ **参考成熟架构** - 借鉴 wasm3/iwasm 的设计模式

---

## 📚 文档导航

### 核心文档

- **[实现计划](docs/IMPLEMENTATION_PLAN.md)** ⭐ - 详细的模块设计、接口规范和开发路线图（743行）
- **[COS3 vs WASM对比](docs/COS3_VS_WASM_COMPARISON.md)** ⭐⭐ - COS3规范与WebAssembly的核心差异分析（870行）
- **[对比分析总结](docs/COMPARISON_SUMMARY.md)** - 对比分析工作的完成总结和关键洞察（379行）
- **[开发指南](docs/DEVELOPER_GUIDE.md)** - 编码规范、测试指南和贡献流程（363行）
- **[任务跟踪](docs/TASK_TRACKER.md)** - 详细的开发进度跟踪和里程碑（418行）
- **[架构设计](ARCHITECTURE.md)** - 系统架构图和数据流图（399行）
- **[项目概述](README_COS3_VM.md)** - 完整的项目介绍和快速开始（411行）

### 快速链接

| 我想... | 查看文档 |
|---------|----------|
| 了解项目整体情况 | [README_COS3_VM.md](README_COS3_VM.md) |
| 查看详细实现计划 | [IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md) |
| **理解COS3与WASM差异** | **[COS3_VS_WASM_COMPARISON.md](docs/COS3_VS_WASM_COMPARISON.md)** |
| 开始编写代码 | [DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) |
| 跟踪开发进度 | [TASK_TRACKER.md](docs/TASK_TRACKER.md) |
| 理解系统架构 | [ARCHITECTURE.md](ARCHITECTURE.md) |

---

## 架构特点

- **栈式虚拟机架构**：基于操作数栈执行字节码指令
- **支持C语言子集编译**：将C代码编译为自定义字节码
- **模块化设计**：支持应用模块和库模块的动态加载
- **安全管理**：内置访问控制和事务管理机制
- **持久化存储**：支持易失性和非易失性数据存储

## 目录结构

```
gcos_vm/
├── include/                 # 头文件
│   ├── vm_core.h           # 虚拟机核心定义
│   ├── vm_executor.h       # 执行器
│   ├── vm_memory.h         # 内存管理
│   ├── vm_instructions.h   # 指令集定义
│   ├── vm_loader.h         # 文件加载器
│   └── vm_types.h          # 类型定义
├── src/                     # 源代码
│   ├── vm_core.c
│   ├── vm_executor.c
│   ├── vm_memory.c
│   ├── vm_instructions.c
│   └── vm_loader.c
├── tests/                   # 测试代码
│   └── test_vm.c
├── examples/                # 示例程序
│   └── hello_app.c
└── CMakeLists.txt          # 构建配置
```

## 核心组件

### 1. 执行器 (Executor)
- 字节码解释执行引擎
- 栈帧管理
- 异常处理

### 2. 运行时数据区
- **执行器栈**：4字节栈单元，管理函数调用
- **间接访问变量栈**：16字节栈单元，存储组合数据类型
- **全局数据区**：存储模块全局数据和临时数据
- **堆**：非易失性存储，支持持久化数据
- **程序计数器**：跟踪当前执行指令

### 3. 指令集
支持200+条指令，包括：
- 算术运算：ADD, SUB, MUL, DIV等
- 逻辑运算：AND, OR, XOR, SHL等
- 控制流：BR, BEQZ, BNEZ, CALL等
- 数据访问：LDT, STT, LDM, STM等
- 类型转换：CVT系列指令

### 4. 内存管理
- 模块数据管理（全局、局部、只读、域数据）
- 应用数据管理（临时静态、临时动态、跨域、持久性等）
- 自动垃圾回收

## 编译和运行

```bash
mkdir build && cd build
cmake ..
make
./tests/test_vm
```

## 使用示例

```c
#include "vm_core.h"

int main() {
    VMContext *vm = vm_create();
    
    // 加载可执行文件
    vm_load_file(vm, "app.sef");
    
    // 选择应用
    vm_select_app(vm, app_aid);
    
    // 执行命令
    vm_execute_command(vm, apdu_data);
    
    // 清理
    vm_destroy(vm);
    return 0;
}
```

## 技术特性

- ✅ 完整的指令集实现
- ✅ 栈式执行模型
- ✅ 模块化应用支持
- ✅ 事务管理机制
- ✅ 异常处理机制
- ✅ 多通道应用选择
- ✅ 持久化数据存储
- ✅ 安全的内存隔离

## 标准符合性

完全符合 GB/T 44901.3 标准要求：
- 二进制文件格式（SEF/LINK/ASM）
- 模块化和应用部署流程
- 运行时数据管理规范
- 安全管理和访问控制
