# GCOS Flash 存储 API 快速参考

## 🚀 核心 API

### 1. 加载 SEF 到 Flash

```c
GCOSResult gcos_loader_load_sef_to_flash(GCOSVM *vm, const u8 *sef_data, u32 sef_size);
```

**用途：** 将 SEF 文件持久化到 Flash，并更新 VM 运行时上下文

**示例：**
```c
u8 sef_buffer[4096];
u32 sef_size = read_sef_from_storage(sef_buffer);

GCOSResult ret = gcos_loader_load_sef_to_flash(vm, sef_buffer, sef_size);
if (ret == GCOS_SUCCESS) {
    // SEF 已保存到 Flash，可以复用 buffer
}
```

---

### 2. 分配 Flash 空间

```c
u32 gcos_flash_alloc_sef_space(u32 sef_size);
```

**返回值：**
- 成功：Flash 偏移量（`>= SEF_STORAGE_BASE`）
- 失败：`FLASH_OFFSET_INVALID` (0xFFFFFFFF)

**示例：**
```c
u32 flash_offset = gcos_flash_alloc_sef_space(sef_size);
if (flash_offset == FLASH_OFFSET_INVALID) {
    // 处理错误：Flash 空间不足
}
```

---

### 3. 保存模块元数据

```c
GCOSResult gcos_persistence_save_module_metadata(GCOSVM *vm, u8 module_index);
```

**用途：** 将模块的元数据（AID、版本、Flash 偏移量等）保存到 Flash

**示例：**
```c
// 通常在加载 SEF 后调用
gcos_loader_load_sef_to_flash(vm, sef_data, sef_size);
gcos_persistence_save_module_metadata(vm, vm->module_count - 1);
```

---

### 4. 释放 Flash 空间

```c
GCOSResult gcos_flash_free_sef_space(u32 flash_offset);
```

**用途：** 标记 Flash 空间为可用（当前实现为占位符）

---

## 📖 Flash 执行宏 (XIP)

这些宏定义在 `gcos_flash_exec.h` 中，用于从 Flash 读取指令和数据。

### 基本宏

```c
// 读取单字节
u8 byte = FLASH_FETCH_BYTE(flash_offset);

// 读取双字节（小端序）
u16 word = FLASH_FETCH_U16(flash_offset);

// 读取四字节（小端序）
u32 dword = FLASH_FETCH_U32(flash_offset);
```

### 使用示例

```c
// 在指令执行器中
u32 code_base = vm->runtime.code_flash_offset;
u32 pc = vm->runtime.program_counter;

u8 opcode = FLASH_FETCH_BYTE(code_base + pc);
pc++;

u16 operand = FLASH_FETCH_U16(code_base + pc);
pc += 2;
```

---

## 🗂️ 数据结构

### GCOSRuntimeContext（修改后）

```c
struct GCOSRuntimeContext {
    // ❌ 已移除
    // u8 module_code[GCOS_MODULE_CODE_SIZE];
    
    // ✅ 新增字段
    u32 code_flash_offset;      // 代码段在 Flash 中的偏移量
    u32 code_size;              // 代码段大小
    u32 sef_flash_offset;       // SEF 文件在 Flash 中的偏移量
    u32 sef_size;               // SEF 文件总大小
    
    // ... 其他字段保持不变
};
```

### GCOSModuleMetadataFlash

```c
typedef struct {
    u32 magic;                  // 魔术字 (0x4D4F444C = "MODL")
    u8  aid[16];                // 模块 AID
    u8  aid_length;             // AID 长度
    u32 version;                // 模块版本
    u32 sef_flash_offset;       // SEF 文件 Flash 偏移量
    u32 sef_size;               // SEF 文件大小
    u32 code_flash_offset;      // 代码段 Flash 偏移量
    u32 code_size;              // 代码段大小
    u32 data_flash_offset;      // 数据段 Flash 偏移量
    u32 data_size;              // 数据段大小
    u16 export_count;           // 导出符号数量
    u16 import_count;           // 导入符号数量
    u8  load_status;            // 加载状态 (0=未加载, 1=已加载, 2=活跃)
    u8  reserved[3];            // 对齐填充
    u32 checksum;               // CRC32 校验和
} GCOSModuleMetadataFlash;     // 总大小: 64 字节
```

---

## 🔧 eflash FTL API 集成

GCOS 使用以下 eflash FTL API 进行 Flash 操作：

### 写入数据

```c
int eflash_ftl_write_logical(uint32_t logical_addr, const uint8_t *data, int16_t size);
```

**参数：**
- `logical_addr`: 逻辑地址（Flash 偏移量）
- `data`: 要写入的数据缓冲区
- `size`: 数据大小（字节）

**返回值：**
- `0`: 成功
- 非零: 错误码

### 读取数据

```c
int eflash_ftl_read_logical(uint32_t logical_addr, uint8_t *data, int16_t size);
```

**参数：**
- `logical_addr`: 逻辑地址（Flash 偏移量）
- `data`: 接收数据的缓冲区
- `size`: 要读取的字节数

**返回值：**
- `0`: 成功
- 非零: 错误码

---

## 📍 Flash 布局

```
Flash Memory (示例: 256 KB)
┌──────────────────────────────┐ 0x000000
│ Firmware                     │ 128 KB
├──────────────────────────────┤ 0x020000
│ SEF Storage Region           │ 64 KB
│  - Module 1 SEF (0x020000)   │
│  - Module 2 SEF (0x021000)   │
│  - Module 3 SEF (0x022000)   │
│  ...                         │
├──────────────────────────────┤ 0x030000
│ Metadata Region              │ 4 KB
│  - Module 0 Meta (64 bytes)  │
│  - Module 1 Meta (64 bytes)  │
│  - Module 2 Meta (64 bytes)  │
│  ... (最多 64 个模块)         │
├──────────────────────────────┤ 0x031000
│ Symbol Table Region          │ 4 KB
├──────────────────────────────┤ 0x032000
│ Runtime State Region         │ 4 KB
├──────────────────────────────┤ 0x033000
│ Reserved                     │ 52 KB
└──────────────────────────────┘ 0x040000
```

---

## ⚠️ 注意事项

### 1. Flash 写入限制

- **擦除次数有限**：Flash 有擦写寿命（通常 10K-100K 次）
- **写入前需擦除**：eflash FTL 层自动处理
- **原子性保证**：使用事务机制确保数据一致性

### 2. 性能考虑

- **Flash 读取较慢**：比 RAM 慢 10-100 倍
- **批量读取优于多次读取**：尽量一次性读取所需数据
- **缓存热点数据**：频繁访问的数据可缓存到 RAM

### 3. 错误处理

```c
GCOSResult ret = gcos_loader_load_sef_to_flash(vm, sef_data, sef_size);
switch (ret) {
    case GCOS_SUCCESS:
        // 成功
        break;
    case GCOS_ERR_OUT_OF_MEMORY:
        // Flash 空间不足
        handle_flash_full();
        break;
    case GCOS_ERR_FILE_FORMAT:
        // Flash 写入错误或格式错误
        handle_io_error();
        break;
    case GCOS_ERR_INVALID_PARAM:
        // 参数错误
        handle_invalid_param();
        break;
    default:
        // 未知错误
        handle_unknown_error();
        break;
}
```

---

## 🔍 调试技巧

### 1. 启用 Flash 操作日志

```c
// 在 gcos_platform.h 中启用
#define GCOS_DEBUG_FLASH 1
```

### 2. 验证 Flash 内容

```c
// 读取并验证
u8 verify_buffer[256];
eflash_ftl_read_logical(flash_offset, verify_buffer, sizeof(verify_buffer));
if (memcmp(verify_buffer, expected_data, sizeof(verify_buffer)) != 0) {
    GCOS_PRINTF("Flash verification failed!\n");
}
```

### 3. 检查 Flash 使用情况

```c
// 打印 Flash 布局信息
GCOS_PRINTF("SEF Flash offset: 0x%08X\n", vm->runtime.sef_flash_offset);
GCOS_PRINTF("Code Flash offset: 0x%08X\n", vm->runtime.code_flash_offset);
GCOS_PRINTF("SEF size: %u bytes\n", vm->runtime.sef_size);
GCOS_PRINTF("Code size: %u bytes\n", vm->runtime.code_size);
```

---

## 📚 相关文档

- [GCOS_LOADER_FLASH_STORAGE_IMPLEMENTATION.md](GCOS_LOADER_FLASH_STORAGE_IMPLEMENTATION.md) - 详细实施文档
- [GCOS_SMARTCARD_PERSISTENCE_DESIGN.md](GCOS_SMARTCARD_PERSISTENCE_DESIGN.md) - 持久化设计
- [GCOS_SMARTCARD_DESIGN_PRINCIPLES.md](GCOS_SMARTCARD_DESIGN_PRINCIPLES.md) - 设计准则
- [GCOS_SMARTCARD_ANALYSIS_SUMMARY.md](GCOS_SMARTCARD_ANALYSIS_SUMMARY.md) - 架构分析

---

**最后更新：** 2026-05-12  
**版本：** 1.0.0
