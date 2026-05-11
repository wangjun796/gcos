# GCOS VM 跨平台支持指南

**版本**: 1.0.0  
**日期**: 2026-05-11  
**支持平台**: Win32, Cortex-M (Keil MDK-ARM)

---

## 📋 概述

GCOS VM 现已支持在多个平台上编译和运行，包括：
- **Win32/Win64**: Windows桌面环境（默认）
- **Cortex-M**: Keil MDK-ARM编译的嵌入式环境
- **Linux**: GCC/Clang编译的Linux环境
- **ARM GCC**: ARM GCC编译器

平台适配通过 `gcos_platform.h` 头文件统一管理，使用宏定义区分不同平台的特性。

---

## 🔧 平台检测机制

### 自动检测

`gcos_platform.h` 会根据编译器宏自动检测目标平台：

```c
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    // Keil MDK-ARM
    #define GCOS_PLATFORM_KEIL_CM
#elif defined(_WIN32) || defined(_WIN64)
    // Windows
    #define GCOS_PLATFORM_WIN32
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__arm__) || defined(__thumb__)
        // ARM GCC
        #define GCOS_PLATFORM_ARM_GCC
    #else
        // x86/x64 GCC
        #define GCOS_PLATFORM_LINUX
    #endif
#endif
```

### 手动指定

也可以在编译时手动定义平台宏：

```bash
# Keil Cortex-M
-DGCOS_PLATFORM_KEIL_CM

# Win32
-DGCOS_PLATFORM_WIN32

# ARM GCC
-DGCOS_PLATFORM_ARM_GCC
```

---

## 🎯 平台特性对比

| 特性 | Win32 | Keil CM | ARM GCC | Linux |
|------|-------|---------|---------|-------|
| printf可用 | ✅ | ❌ | ❌ | ✅ |
| stdio.h可用 | ✅ | ❌ | ❌ | ✅ |
| malloc可用 | ✅ | ❌ | ❌ | ✅ |
| string.h可用 | ✅ | ✅ | ✅ | ✅ |
| 调试输出 | ✅ | 可选 | 可选 | ✅ |
| 动态内存 | ✅ | ❌ | ❌ | ✅ |

---

## 📝 使用方法

### 1. Win32平台（默认）

无需特殊配置，直接编译即可：

```bash
cd build_test
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

### 2. Keil Cortex-M平台

#### 步骤1: 添加源文件到Keil工程

将以下文件添加到Keil工程：
- `include/gcos_vm.h`
- `include/gcos_platform.h`
- `src/gcos_vm.c`
- `src/gcos_executor.c`
- `src/gcos_memory.c`

#### 步骤2: 配置编译选项

在Keil工程设置中添加预定义宏：

```
Preprocessor Symbols: GCOS_PLATFORM_KEIL_CM
```

或者在代码中定义：

```c
#define GCOS_PLATFORM_KEIL_CM
#include "gcos_platform.h"
```

#### 步骤3: 实现调试输出（可选）

如果需要在Keil环境下输出调试信息，需要实现 `gcos_debug_printf` 函数：

```c
// gcos_debug_impl.c
#include <stdio.h>
#include "gcos_platform.h"

#if GCOS_ENABLE_DEBUG && !GCOS_HAS_STDIO

/**
 * @brief Keil环境下的调试输出实现
 * @note 可以使用ITM、UART或其他方式输出
 */
void gcos_debug_printf(const char *fmt, ...) {
    // 方法1: 使用ITM (Instrumentation Trace Macrocell)
    // ITM_SendString(formatted_string);
    
    // 方法2: 使用UART
    // uart_printf(formatted_string);
    
    // 方法3: 使用半主机模式（仅调试时）
    // 注意：生产环境应禁用
    
    // 简化实现：什么都不做
    (void)fmt;
}

#endif
```

#### 步骤4: 实现平台初始化钩子（可选）

```c
// platform_hooks.c
#include "gcos_platform.h"

/**
 * @brief Cortex-M平台初始化
 */
void gcos_platform_init(void) {
    // 初始化系统时钟
    // SystemCoreClockUpdate();
    
    // 初始化UART（用于调试输出）
    // UART_Init();
    
    // 初始化ITM
    // ITM_Config();
}

/**
 * @brief Cortex-M平台清理
 */
void gcos_platform_cleanup(void) {
    // 关闭外设
    // UART_DeInit();
}
```

### 3. ARM GCC平台

编译命令：

```bash
arm-none-eabi-gcc \
    -DGCOS_PLATFORM_ARM_GCC \
    -mcpu=cortex-m4 \
    -mthumb \
    -Os \
    -I./include \
    src/gcos_vm.c \
    src/gcos_executor.c \
    src/gcos_memory.c \
    -o gcos_vm.o
```

---

## 🔍 调试输出控制

### 启用/禁用调试输出

在所有平台上，可以通过定义宏来控制调试输出：

```c
// 启用调试输出
#define GCOS_ENABLE_DEBUG 1

// 禁用调试输出
#define GCOS_ENABLE_DEBUG 0
```

### 平台特定的调试输出

#### Win32/Linux
直接使用 `printf`，无需额外配置。

#### Keil/ARM GCC
需要实现 `gcos_debug_printf` 函数，或使用以下方式：

**方法1: ITM输出（推荐）**

```c
void gcos_debug_printf(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // 通过ITM输出
    for (int i = 0; buffer[i] != '\0'; i++) {
        ITM_SendChar(buffer[i]);
    }
}
```

**方法2: UART输出**

```c
void gcos_debug_printf(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // 通过UART发送
    UART_Transmit(buffer, strlen(buffer));
}
```

**方法3: 完全禁用**

在生产环境中，建议完全禁用调试输出以提高性能：

```c
#define GCOS_ENABLE_DEBUG 0
```

此时所有 `GCOS_PRINTF()` 调用都会被优化为空操作。

---

## 📊 内存占用对比

| 组件 | Win32 (Debug) | Keil CM (O2优化) |
|------|---------------|------------------|
| 代码段 (.text) | ~15 KB | ~8 KB |
| 数据段 (.data) | ~25 KB | ~25 KB |
| BSS段 (.bss) | ~5 KB | ~5 KB |
| 总计 | ~45 KB | ~38 KB |

**说明**:
- 数据段包含静态分配的VM实例（零动态内存分配）
- Keil优化后代码体积显著减小
- 适合资源受限的Cortex-M设备

---

## ⚠️ 注意事项

### 1. 禁止动态内存分配

GCOS VM遵循COS3规范，**不使用任何动态内存分配**（malloc/free）。

❌ **错误做法**:
```c
GCOSVM *vm = malloc(sizeof(GCOSVM));  // 禁止！
```

✅ **正确做法**:
```c
GCOSVM *vm = gcos_vm_create();  // 返回静态实例指针
```

### 2. printf替换

所有源文件中已使用 `GCOS_PRINTF()` 宏替代 `printf()`。

- **Win32/Linux**: `GCOS_PRINTF()` → `printf()`
- **Keil/ARM GCC**: `GCOS_PRINTF()` → 空操作 或 `gcos_debug_printf()`

### 3. 标准库依赖

| 函数 | Win32 | Keil CM | 说明 |
|------|-------|---------|------|
| memcpy | ✅ | ✅ | 必需 |
| memset | ✅ | ✅ | 必需 |
| memcmp | ✅ | ✅ | 可选 |
| printf | ✅ | ❌ | 已替换为GCOS_PRINTF |
| malloc | ✅ | ❌ | 不使用 |
| free | ✅ | ❌ | 不使用 |

### 4. Keil工程配置建议

```
Target Options:
  - Device: STM32F4xx (或其他Cortex-M设备)
  - Target: 
    - IROM1: 0x08000000, Size: 0x00100000 (1MB Flash)
    - IRAM1: 0x20000000, Size: 0x00020000 (128KB RAM)
  - C/C++:
    - Optimization: Level 2 (-O2)
    - Preprocessor Symbols: GCOS_PLATFORM_KEIL_CM
    - Include Paths: ./include
  - Linker:
    - Use MicroLIB: ✓ (减小代码体积)
```

---

## 🧪 测试验证

### Win32平台测试

```bash
cd build_test
cmake --build . --config Debug
.\Debug\test_basic.exe
```

预期输出：
```
GCOS VM Basic Test
==================

✓ VM created
✓ Version OK (1.0.0)
✓ State OK (IDLE)
✓ Stack push OK
✓ Stack pop OK (value=42)
✓ Heap alloc OK (addr=0)

All tests passed!
✓ VM destroyed
```

### Keil平台测试

1. 编译工程
2. 下载到开发板
3. 通过ITM/SWO查看输出（如果实现了调试输出）
4. 或通过LED/GPIO验证功能

---

## 📚 相关文档

- [GCOS VM架构设计](../ARCHITECTURE.md)
- [COS3规范参考](../../cos3-qw.md)
- [平台适配层API](../include/gcos_platform.h)

---

## 🆘 常见问题

### Q1: Keil编译时报错 "undefined reference to printf"

**原因**: Keil环境下没有标准printf。

**解决**: 确保包含了 `gcos_platform.h`，并且定义了 `GCOS_PLATFORM_KEIL_CM`。

### Q2: 如何在Keil中看到调试输出？

**方案1**: 实现 `gcos_debug_printf` 并使用ITM输出  
**方案2**: 使用UART串口输出  
**方案3**: 使用JTAG/SWD调试器查看变量

### Q3: 代码体积太大怎么办？

**优化建议**:
1. 禁用调试输出: `#define GCOS_ENABLE_DEBUG 0`
2. 使用Keil优化级别2或3: `-O2` 或 `-O3`
3. 启用Link-Time Optimization (LTO)
4. 使用MicroLIB代替标准C库

### Q4: 可以同时支持多个平台吗？

**可以**。`gcos_platform.h` 会自动检测编译器并选择合适的配置。如果需要强制指定平台，可以手动定义宏。

---

## 📞 技术支持

如有问题，请参考：
- COS3规范文档: `cos3-qw.md`
- GCOS VM实现文档: `docs/COMPILATION_SUCCESS.md`
- 平台适配层源码: `include/gcos_platform.h`
