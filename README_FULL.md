# GCOS VM 完整实现文档

## 项目概述

**GCOS VM** (GuoChao Operating System Virtual Machine) 是一个基于中华人民共和国国家标准 **GB/T 44901.3《卡及身份识别安全设备片上操作系统第3部分：支持面向过程应用后下载的基础层技术要求》** 实现的完整虚拟机系统。

## 核心特性

- ✅ **完全符合COS3规范** - 严格遵循国家标准要求
- ✅ **栈式字节码执行器** - 弹栈-执行-压栈计算模型
- ✅ **支持应用后下载** - SEF文件格式加载和安装
- ✅ **多通道应用隔离** - 最多8个逻辑通道独立运行
- ✅ **事务管理机制** - 原子性操作保证数据一致性
- ✅ **运行时安全管理** - 应用隔离、接口授权、异常处理
- ✅ **零动态内存分配** - 全局静态实例,适合嵌入式环境
- ✅ **参考成熟架构** - 借鉴 wasm3/iwasm 的设计模式

## 项目结构

```
gcos_vm/
├── include/                      # 头文件
│   ├── gcos_vm_full.h          # 完整VM头文件
│   ├── gcos_vm_full_part2.h    # VM API第二部分
│   ├── vm_types.h               # 类型定义
│   ├── vm_core.h                # VM核心定义
│   ├── vm_instructions.h        # 指令集定义
│   ├── vm_executor.h             # 执行器定义
│   ├── vm_memory.h               # 内存管理定义
│   └── vm_loader.h               # 加载器定义
│
├── src/                         # 源代码文件
│   ├── vm_core_full.c            # VM核心实现
│   ├── vm_instructions_full.c     # 指令集实现
│   ├── vm_executor_full.c         # 执行器实现
│   ├── vm_executor_part2.c       # 执行器部分实现
│   ├── vm_loader_full.c          # 加载器实现
│   ├── vm_transaction_full.c     # 事务管理器实现
│   ├── vm_app_manager_full.c     # 应用管理器实现
│   └── vm_memory.c              # 内存管理实现
│
├── docs/                        # 文档目录
│   ├── ARCHITECTURE.md          # 架构设计文档
│   ├── IMPLEMENTATION_PLAN.md    # 实现计划
│   ├── COS3_VS_WASM_COMPARISON.md  # COS3与WASM对比
│   └── DEVELOPER_GUIDE.md       # 开发指南
│
├── examples/                     # 示例程序
│   └── hello_app.c             # Hello World示例
│
├── CMakeLists.txt              # 构建配置
└── README_FULL.md              # 本文档
```

## 核心模块说明

### 1. 虚拟机核心 (vm_core_full.c)

**主要功能：**
- 虚拟机生命周期管理（创建、初始化、重置、销毁）
- 模块加载和管理
- 应用选择和取消选择
- APDU命令执行
- 异常处理
- 统计信息收集

**关键API：**
```c
GCOSVM* gcos_vm_create(void);
void gcos_vm_destroy(GCOSVM *vm);
GCOSResult gcos_vm_init(GCOSVM *vm);
GCOSResult gcos_vm_reset(GCOSVM *vm);
GCOSResult gcos_vm_load_module(GCOSVM *vm, const u8 *sef_data, 
                             u32 sef_size, u8 *module_index);
GCOSResult gcos_vm_select_app(GCOSVM *vm, u8 channel, const GCOSAID *app_aid);
GCOSResult gcos_vm_deselect_app(GCOSVM *vm, u8 channel);
GCOSResult gcos_vm_execute_apdu(GCOSVM *vm, u8 channel, const u8 *apdu, u32 apdu_len,
                          u8 *response, u32 *response_len);
```

### 2. 指令集 (vm_instructions_full.c)

**主要功能：**
- 完整的COS3指令集实现（200+条指令）
- 指令解码
- 指令信息查询
- 指令打印（调试用）

**指令分类：**
- **控制流指令** (0x00-0x1F): 跳转、调用、返回
- **数值指令** (0x18-0x42): 常量、算术、位运算、比较
- **变量指令** (0x43-0x61): 局部变量读写
- **内存指令** (0x63-0x6C): 内存读写
- **异常处理指令** (0x6D-0x6E): TRY-CATCH
- **复合指令** (0x6F-0x7D): 栈操作、复合运算

**关键API：**
```c
const GCOSInstructionInfo* gcos_instruction_get_info(u8 opcode);
int gcos_decode_instruction(const u8 *code, u32 code_size, u32 offset,
                           u8 *opcode, u32 *operands, u8 *operand_count);
GCOSResult gcos_execute_instruction(GCOSVM *vm, u8 opcode,
                            const u32 *operands, u8 operand_count);
void gcos_print_instruction(u8 opcode, const u32 *operands, u8 operand_count);
```

### 3. 执行器 (vm_executor_full.c & vm_executor_part2.c)

**主要功能：**
- 字节码解释执行引擎
- 指令分发和调度
- 栈帧管理
- 函数调用和返回
- 异常传播和处理
- 性能统计

**关键API：**
```c
GCOSResult gcos_vm_executor_start(GCOSVM *vm);
void gcos_vm_executor_stop(GCOSVM *vm);
void gcos_vm_executor_pause(GCOSVM *vm);
void gcos_vm_executor_resume(GCOSVM *vm);
void gcos_vm_executor_set_trace(GCOSVM *vm, bool enable);
void gcos_vm_executor_set_max_instructions(GCOSVM *vm, u32 max);
GCOSResult gcos_vm_executor_run(GCOSVM *vm);
GCOSResult gcos_vm_executor_step(GCOSVM *vm);
```

### 4. 加载器 (vm_loader_full.c)

**主要功能：**
- SEF文件格式解析
- 段提取和验证
- 符号链接
- 模块实例化
- 应用信息解析

**SEF文件结构：**
```
SEF文件
├── 文件头
│   ├── 文件类型 ('sef')
│   ├── 版本号
│   └── 段数量
└── 段数据
    ├── 首段 (0x01) - 模块信息、导入信息、段信息
    ├── 导入段 (0x02) - 导入函数信息
    ├── 函数段 (0x03) - 内部函数空间信息
    ├── 应用段 (0x04) - 可执行模块安装信息
    ├── 全局段 (0x05) - 模块数据、应用数据空间信息
    ├── 导出段 (0x06) - 导出函数信息
    ├── 元素段 (0x07) - 被引用的函数索引信息
    ├── 数据段 (0x08) - 模块数据、应用数据初始值信息
    ├── 代码段 (0x09) - 模块程序代码
    └── 自定义段 (0x0F) - 自定义信息
```

### 5. 事务管理器 (vm_transaction_full.c)

**主要功能：**
- 事务开始/提交/回滚
- 数据备份和恢复
- 嵌套事务支持
- 原子性保证

**事务特性：**
- 支持最多16层嵌套事务
- 自动备份应用域数据、引用域数据、持久性数据、模块域数据
- 提交时释放备份
- 回滚时恢复数据

**关键API：**
```c
GCOSResult gcos_vm_transaction_init(GCOSVM *vm);
GCOSResult gcos_vm_transaction_begin(GCOSVM *vm);
GCOSResult gcos_vm_transaction_commit(GCOSVM *vm);
GCOSResult gcos_vm_transaction_rollback(GCOSVM *vm);
bool gcos_vm_transaction_is_active(const GCOSVM *vm);
u8 gcos_vm_transaction_get_nesting_level(const GCOSVM *vm);
void gcos_vm_transaction_cleanup(GCOSVM *vm);
```

### 6. 应用管理器 (vm_app_manager_full.c)

**主要功能：**
- 应用安装/卸载
- 应用选择/取消选择
- 多通道管理
- 应用生命周期管理

**应用生命周期状态：**
- 已安装 (INSTALLED)
- 可选择 (SELECTABLE)
- 已选择 (SELECTED)
- 已个性化 (PERSONALIZED)
- 已锁定 (LOCKED)
- 已终止 (TERMINATED)

**关键API：**
```c
GCOSResult gcos_vm_app_manager_init(GCOSVM *vm);
GCOSResult gcos_vm_install_app(GCOSVM *vm, u8 module_index, 
                             const GCOSAID *app_aid);
GCOSResult gcos_vm_uninstall_app(GCOSVM *vm, const GCOSAID *app_aid);
GCOSResult gcos_vm_select_app(GCOSVM *vm, u8 channel, const GCOSAID *app_aid);
GCOSResult gcos_vm_deselect_app(GCOSVM *vm, u8 channel);
```

### 7. 内存管理 (vm_memory.c)

**主要功能：**
- 执行器栈管理（4字节单元）
- 间接访问变量栈管理（16字节单元）
- 全局数据区管理
- 堆管理（非易失性存储）
- 内存分配和释放

**内存布局：**
```
运行时数据区
├── 执行器栈 (256×4B = 1KB)
├── 间接访问变量栈 (64×16B = 1KB)
├── 全局数据区 (4KB)
└── 堆 (8KB, 非易失性)
```

## 编译和使用

### 编译

```bash
mkdir build && cd build
cmake ..
make
```

### 使用示例

```c
#include "gcos_vm_full.h"

int main() {
    // 创建虚拟机实例
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("Failed to create VM\n");
        return -1;
    }

    // 初始化虚拟机
    if (gcos_vm_init(vm) != GCOS_OK) {
        printf("Failed to initialize VM\n");
        gcos_vm_destroy(vm);
        return -1;
    }

    // 加载SEF文件
    GCOSResult ret = gcos_vm_load_module(vm, sef_data, sef_size, &module_index);
    if (ret != GCOS_OK) {
        printf("Failed to load module: %d\n", ret);
        gcos_vm_destroy(vm);
        return -1;
    }

    // 选择应用
    GCOSAID app_aid = {.length = 5, .aid = {0xA0, 0x00, 0x00, 0x01, 0x00}};
    ret = gcos_vm_select_app(vm, 0, &app_aid);
    if (ret != GCOS_OK) {
        printf("Failed to select app: %d\n", ret);
        gcos_vm_destroy(vm);
        return -1;
    }

    // 执行APDU命令
    u8 apdu[] = {0x00, 0xA4, 0x04, 0x00, 0x00, 0x00};
    u8 response[256];
    u32 response_len = sizeof(response);

    ret = gcos_vm_execute_apdu(vm, 0, apdu, sizeof(apdu), 
                               response, &response_len);
    if (ret != GCOS_OK) {
        printf("Failed to execute APDU: %d\n", ret);
    gcos_vm_destroy(vm);
        return -1;
    }

    printf("Response: ");
    for (u32 i = 0; i < response_len; i++) {
        printf("%02X ", response[i]);
    }
    printf("\n");

    // 清理
    gcos_vm_destroy(vm);
    return 0;
}
```

## 技术特性

### 内存管理

- **执行器栈**: 256个4字节单元，用于函数调用和表达式求值
- **间接访问变量栈**: 64个16字节单元，用于存储组合数据类型
- **全局数据区**: 4KB易失性存储，用于模块全局数据和临时数据
- **堆**: 8KB非易失性存储，用于应用域数据、引用域数据、持久性数据

### 指令执行

- **基于栈的计算模型**: 弹栈-执行-压栈
- **完整的COS3指令集**: 200+条指令
- **指令分类**: 控制流、数值、变量、内存、异常处理、复合指令
- **高效的指令分发**: 使用跳转表实现O(1)指令分发

### 安全特性

- **应用隔离**: 每个应用实例独立运行
- **多通道支持**: 最多8个逻辑通道，支持并发执行
- **事务保护**: 原子性操作，支持嵌套事务
- **访问控制**: 基于授权的接口访问控制
- **异常处理**: 完整的异常检测和处理机制

## 性能优化

- **零动态内存分配**: 所有数据结构静态分配
- **高效的指令执行**: 优化的指令分发和执行
- **紧凑的数据布局**: 最小化内存占用
- **缓存友好的设计**: 适合嵌入式环境

## 标准符合性

完全符合 GB/T 44901.3 标准要求：

- ✅ 二进制文件格式（SEF/LINK/ASM）
- ✅ 模块化和应用部署流程
- ✅ 运行时数据管理规范
- ✅ 安全管理和访问控制
- ✅ 事务管理机制
- ✅ 异常处理机制
- ✅ 多通道应用隔离
- ✅ 应用生命周期管理

## 开发指南

1. **编码规范**: 遵循C99标准，使用4空格缩进
2. **命名约定**: 使用gcos_前缀，清晰描述功能
3. **错误处理**: 所有函数返回GCOSResult，检查返回值
4. **内存管理**: 使用提供的内存管理API，避免直接malloc/free
5. **文档注释**: 为所有公共API提供详细注释

## 调试支持

- **指令追踪**: 可启用指令执行追踪
- **状态打印**: 可打印虚拟机完整状态
- **栈状态打印**: 可查看执行器栈内容
- **模块信息打印**: 可查看已加载模块信息

## 版本历史

- **v1.0.0** (2026-05-08): 初始版本，实现完整的COS3虚拟机

## 许可证

本项目遵循中华人民共和国国家标准 GB/T 44901.3 实现。

## 联系方式

如有问题或建议，请联系开发团队。

---

**注意**: 本实现基于COS3规范草案，可能随着标准的更新而调整。
