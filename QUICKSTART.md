# GCOS 虚拟机 - 快速开始指南

## 项目简介

本项目实现了符合《卡及身份识别安全设备片上操作系统第3部分：支持面向过程应用后下载的基础层技术要求》(GB/T 44901.3)标准的虚拟机系统。

## 系统要求

### Windows
- Windows 10/11
- Visual Studio 2022 (或更新版本)
- CMake 3.10+

### Linux
- GCC 7.0+ 或 Clang 6.0+
- CMake 3.10+
- Make

### macOS
- Xcode Command Line Tools
- CMake 3.10+

## 快速开始

### Windows用户

1. 双击运行 `build.bat`
2. 等待编译完成
3. 测试程序将自动运行

### Linux/macOS用户

```bash
chmod +x build.sh
./build.sh
```

## 手动编译

### 使用CMake

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译
make  # Linux/macOS
# 或者
cmake --build . --config Release  # Windows

# 运行测试
./test_vm  # Linux/macOS
# 或者
Release\test_vm.exe  # Windows
```

## 项目结构

```
gcos_vm/
├── include/              # 头文件
│   ├── vm_core.h        # 虚拟机核心API
│   ├── vm_types.h       # 类型定义
│   ├── vm_memory.h      # 内存管理
│   ├── vm_instructions.h # 指令集
│   ├── vm_executor.h    # 执行器
│   └── vm_loader.h      # 文件加载器
├── src/                 # 源代码
│   ├── vm_core.c
│   ├── vm_memory.c
│   ├── vm_instructions.c
│   ├── vm_executor.c
│   └── vm_loader.c
├── tests/               # 测试代码
│   └── test_vm.c
├── examples/            # 示例程序
│   └── hello_app.c
├── CMakeLists.txt       # CMake配置
├── build.bat            # Windows构建脚本
├── build.sh             # Linux/macOS构建脚本
└── README.md            # 项目说明
```

## 核心功能

### 1. 虚拟机核心
- 创建和销毁虚拟机实例
- 初始化和重置
- 状态管理

### 2. 指令集
- 200+条字节码指令
- 算术运算（ADD, SUB, MUL, DIV等）
- 逻辑运算（AND, OR, XOR, SHL等）
- 控制流（BR, BEQZ, CALL等）
- 数据访问（LDT, STT, LDM, STM等）
- 比较指令（EQ, NE, LT, GT等）

### 3. 内存管理
- 执行器栈（4字节单元）
- 间接访问变量栈（16字节单元）
- 全局数据区
- 堆（支持动态分配）
- 模块程序区

### 4. 应用管理
- 应用安装/卸载
- 应用选择/取消选择
- 多通道支持
- 生命周期管理

### 5. 事务管理
- 事务开始/提交/回滚
- 数据保护机制

### 6. 异常处理
- 完整的异常类型
- TRY/CATCH机制
- 异常处理器注册

## 使用示例

### 基本使用

```c
#include "vm_core.h"

int main() {
    // 创建虚拟机
    VMContext *vm = vm_create();
    
    // 初始化
    vm_init(vm);
    
    // 加载SEF文件
    vm_load_file(vm, "app.sef");
    
    // 选择应用
    AID app_aid = {...};
    vm_select_app(vm, &app_aid, 0);
    
    // 执行APDU命令
    u8 apdu[] = {...};
    u8 response[256];
    u32 response_len = sizeof(response);
    vm_execute_apdu(vm, apdu, sizeof(apdu), response, &response_len);
    
    // 清理
    vm_destroy(vm);
    return 0;
}
```

### 直接执行字节码

```c
// 准备字节码
u8 bytecode[] = {
    OP_LDC_U8, 10,
    OP_LDC_U8, 20,
    OP_ADD,
    OP_RET
};

// 加载到虚拟机
memcpy(vm->module_code, bytecode, sizeof(bytecode));

// 执行
vm_execute(vm);

// 获取结果
u32 result;
vm_stack_pop(vm, &result);
printf("Result: %u\n", result); // 输出: Result: 30
```

## 测试

运行测试套件验证虚拟机功能：

```bash
cd build
./test_vm  # 或 test_vm.exe
```

测试包括：
- 虚拟机创建/销毁
- 栈操作
- 算术指令
- 逻辑指令
- 比较指令
- 堆分配
- 事务管理

## 架构说明

### 栈式虚拟机

GCOS VM采用栈式架构，所有操作都通过操作数栈进行：

```
指令: ADD
执行前: [5] [3]
执行后: [8]
```

### 运行时数据区

```
┌─────────────────────┐
│  执行器栈 (4字节单元) │
├─────────────────────┤
│ 间接变量栈 (16字节)  │
├─────────────────────┤
│   全局数据区         │
├─────────────────────┤
│      堆             │
├─────────────────────┤
│   模块程序区         │
└─────────────────────┘
```

### 指令格式

每条指令包含：
- 操作码（1字节）
- 操作数（0-N字节，取决于指令）

例如：
```
OP_ADD       : 0x60          (无操作数)
OP_LDC_U8    : 0x4A 0x05     (8位操作数)
OP_BR_S16    : 0x11 0x00 0x10 (16位偏移)
```

## 扩展开发

### 添加新指令

1. 在 `vm_instructions.h` 中定义操作码
2. 在 `vm_instructions.c` 的指令表中添加信息
3. 在 `vm_execute_instruction()` 中实现指令逻辑
4. 更新解码器以支持新指令格式

### 添加系统调用(TRAP)

1. 定义TRAP ID和功能
2. 在 `vm_executor_call_trap()` 中添加处理逻辑
3. 实现相应的系统功能

## 性能优化建议

1. **指令缓存**: 预解码常用指令
2. **JIT编译**: 对热点代码进行即时编译
3. **内存池**: 使用对象池减少malloc调用
4. **内联函数**: 关键路径使用内联
5. **分支预测**: 优化条件跳转

## 调试技巧

### 启用追踪模式

```c
ExecutorConfig config = {0};
config.enable_trace = true;
vm_executor_init(vm, &config);
```

### 设置断点

```c
vm_executor_set_breakpoint(vm, 0x100);  // 在地址0x100处断点
```

### 查看统计信息

```c
u64 instr_count, exec_time;
u32 stack_peak;
vm_executor_get_stats(vm, &instr_count, &exec_time, &stack_peak);
```

## 常见问题

### Q: 编译时找不到头文件？
A: 确保CMake正确配置，检查include目录是否添加到编译路径。

### Q: 栈溢出错误？
A: 增加 `VM_EXECUTOR_STACK_SIZE` 或检查递归调用。

### Q: 如何加载实际的SEF文件？
A: 使用 `vm_load_file()` 函数，传入SEF文件路径。

### Q: 支持多线程吗？
A: 当前版本每个VM实例不是线程安全的，建议在单线程中使用或使用互斥锁保护。

## 参考资料

- GB/T 44901.3-XXXX 标准文档
- 《智能卡操作系统原理与实现》
- Java Card Virtual Machine Specification
- WebAssembly Specification

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议，请提交Issue或Pull Request。

---

**祝您使用愉快！**
