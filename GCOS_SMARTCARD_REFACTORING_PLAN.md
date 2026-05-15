# GCOS 智能卡架构重构方案

## 🚨 当前实现的严重问题

### 问题 1: SEF 代码存储在 RAM（❌ 致命问题）

**现状**：
```c
// gcos_vm.h
#define GCOS_MODULE_CODE_SIZE   16384   // 16 KB RAM!

struct GCOSRuntimeContext {
    u8 module_code[GCOS_MODULE_CODE_SIZE];  // ❌ 16KB RAM 用于存储代码
    u32 code_size;
};

// gcos_loader.c - 加载时将 SEF 代码复制到 RAM
memcpy(vm->runtime.module_code, &data[offset], bytecode_size);
vm->runtime.code_size = bytecode_size;
```

**问题分析**：
- ❌ **浪费 RAM**：16KB 对于 8-64KB RAM 的智能卡来说占比太大（25%-200%）
- ❌ **掉电丢失**：RAM 中的数据在断电后完全丢失
- ❌ **重复存储**：SEF 文件既在 Flash 又在 RAM，浪费空间
- ❌ **启动慢**：每次启动都需要从 Flash 加载到 RAM

**正确做法**：
- ✅ SEF 代码应直接存储在 Flash
- ✅ 执行时直接从 Flash 读取（XIP - Execute In Place）
- ✅ RAM 仅保存必要的运行时状态（PC、栈指针等）

---

### 问题 2: 符号解析数据未持久化（❌ 严重问题）

**现状**：
```c
// gcos_symbol_resolver.c
static GCOSSymbolResolver g_symbol_resolver;  // 静态变量在 RAM

// 符号解析结果保存在 RAM，重启后丢失
g_symbol_resolver.global_ref_table[index].logical_address = ...;
```

**问题分析**：
- ❌ **重启丢失**：所有符号解析结果在重启后需要重新计算
- ❌ **加载慢**：每次启动都要重新解析所有模块的符号
- ❌ **不一致风险**：如果解析失败，系统无法恢复

**正确做法**：
- ✅ 符号解析结果持久化到 Flash
- ✅ 启动时从 Flash 恢复，无需重新解析
- ✅ 仅在模块更新时才重新解析

---

### 问题 3: 缺少 Flash 布局规划（❌ 架构缺陷）

**现状**：
- eflash 库已集成，但未定义具体的存储布局
- SEF 文件、元数据、符号表混在一起
- 没有考虑磨损均衡和原子性操作

**正确做法**：
- ✅ 设计清晰的 Flash 分区
- ✅ 分离代码区、数据区、元数据区
- ✅ 实现版本管理和回滚机制

---

## 🏗️ 智能卡优化的 Flash 布局设计

### 推荐的 Flash 分区方案（256KB Flash 示例）

```
Flash Memory Layout (256KB Total)
┌──────────────────────────────────────────────┐
│ 0x000000 - 0x01FFFF  | Firmware (128KB)      │ ← GCOS 固件代码
│                      |                       │
│                      | - VM Core             │
│                      | - Instruction Set     │
│                      | - APDU Handler        │
│                      | - T=0/TLP Protocol    │
├──────────────────────────────────────────────┤
│ 0x020000 - 0x02FFFF  | SEF Storage (64KB)    │ ← SEF 文件二进制
│                      |                       │
│                      | Structure:            │
│                      | - SEF Header (8B)     │
│                      | - Sections (variable) │
│                      |   · First Section     │
│                      |   · Import Section    │
│                      |   · Function Section  │
│                      |   · Code Section ⭐   │ ← 直接在 Flash 执行
│                      |   · Data Section      │
│                      | - Multiple SEFs       │
├──────────────────────────────────────────────┤
│ 0x030000 - 0x030FFF  | Module Metadata (4KB) │ ← 模块元数据
│                      |                       │
│                      | Per-module:           │
│                      | - AID (5-16B)         │
│                      | - Version (4B)        │
│                      | - SEF Offset (4B)     │ ← 指向 SEF Storage
│                      | - SEF Size (4B)       │
│                      | - Load Status (1B)    │
│                      | - Checksum (4B)       │
├──────────────────────────────────────────────┤
│ 0x031000 - 0x031FFF  | Symbol Tables (4KB)   │ ← 符号解析结果 ⭐
│                      |                       │
│                      | - Global Ref Table    │
│                      |   (64×12=768B)        │
│                      | - Export Tables       │
│                      |   (compact format)    │
│                      | - Import Tables       │
│                      |   (compact format)    │
│                      | - System Modules      │
│                      | - Checksum (4B)       │
├──────────────────────────────────────────────┤
│ 0x032000 - 0x032FFF  | Runtime State (4KB)   │ ← 运行时状态
│                      |                       │
│                      | - Active App AID      │
│                      | - PC (Program Counter)│
│                      | - Stack Pointer       │
│                      | - Transaction State   │
│                      | - Security Context    │
├──────────────────────────────────────────────┤
│ 0x033000 - 0x03FFFF  | Reserved (52KB)       │ ← 预留扩展
│                      |                       │
│                      | Future use:           │
│                      | - Wear leveling       │
│                      | - Backup copies       │
│                      | - Logging             │
└──────────────────────────────────────────────┘
```

### 关键设计要点

#### 1. SEF 代码直接在 Flash 执行（XIP）

```c
/* 新的数据结构设计 */

typedef struct {
    /* Flash location - NOT copied to RAM */
    u32 sef_flash_offset;       /* SEF file offset in Flash */
    u32 sef_size;               /* SEF file size */
    
    /* Code location in Flash */
    u32 code_flash_offset;      /* Code section offset in Flash */
    u32 code_size;              /* Code section size */
    
    /* Runtime state (in RAM) */
    u32 program_counter;        /* PC - relative to code_flash_offset */
    u32 stack_pointer;          /* Stack pointer */
} GCOSModuleFlashInfo;

/* Execution from Flash */
u8 fetch_bytecode(GCOSModuleFlashInfo *module, u32 pc_offset) {
    /* Read directly from Flash - NO RAM copy */
    return eflash_read_byte(module->code_flash_offset + pc_offset);
}
```

**优势**：
- ✅ **零 RAM 占用**：代码不占用 RAM
- ✅ **掉电保持**：Flash 中的数据永久保存
- ✅ **快速启动**：无需加载代码到 RAM
- ✅ **节省空间**：只需存储一份副本

---

#### 2. 符号表持久化格式

```c
/* Flash storage format for symbol tables */

#define SYMBOL_TABLE_MAGIC      0x53594D42  /* "SYMB" */
#define SYMBOL_TABLE_VERSION    0x00010000  /* v1.0.0.0 */

typedef struct {
    /* Header */
    u32 magic;                  /* Magic number */
    u32 version;                /* Format version */
    u16 global_ref_count;       /* Number of valid entries */
    u16 module_count;           /* Number of loaded modules */
    u32 timestamp;              /* Last update timestamp */
    
    /* Global Reference Table (fixed size) */
    struct {
        u32 logical_address;    /* 32-bit address (Flash offset) */
        u8  module_id;          /* Module ID */
        u16 symbol_index;       /* Symbol index */
        u8  is_valid;           /* Valid flag */
    } global_refs[64];          /* 64 × 12 = 768 bytes */
    
    /* Module export/import tables (compact) */
    struct {
        u8  module_id;
        u8  export_count;
        u8  import_count;
        /* Followed by variable-length data */
    } module_info[8];           /* 8 modules max */
    
    /* Footer */
    u32 checksum;               /* CRC32 of entire structure */
} GCOSSymbolTableFlash;

/* Size: ~1KB total */
```

---

#### 3. 模块元数据持久化

```c
/* Module metadata in Flash */

typedef struct {
    u32 magic;                  /* 0x4D4F444C = "MODL" */
    u8  aid[16];                /* Application ID */
    u8  aid_length;             /* AID length */
    u32 version;                /* Module version */
    
    /* SEF location */
    u32 sef_flash_offset;       /* SEF file offset */
    u32 sef_size;               /* SEF file size */
    
    /* Code location */
    u32 code_flash_offset;      /* Code section offset */
    u32 code_size;              /* Code section size */
    
    /* Data location */
    u32 data_flash_offset;      /* Data section offset */
    u32 data_size;              /* Data section size */
    
    /* Symbol info */
    u16 export_count;           /* Number of exports */
    u16 import_count;           /* Number of imports */
    
    /* Status */
    u8  load_status;            /* 0=unloaded, 1=loaded, 2=active */
    u8  reserved[3];            /* Alignment */
    
    /* Integrity */
    u32 checksum;               /* CRC32 */
} GCOSModuleMetadata;

/* Size: 64 bytes per module */
/* For 8 modules: 512 bytes */
```

---

## 🔧 重构实施计划

### 阶段 1: 修改数据结构（立即）

#### 1.1 移除 RAM 中的代码存储

```c
/* gcos_vm.h - BEFORE (❌) */
struct GCOSRuntimeContext {
    u8 module_code[GCOS_MODULE_CODE_SIZE];  // 16KB RAM - REMOVE!
    u32 code_size;
    u32 program_counter;
};

/* gcos_vm.h - AFTER (✅) */
struct GCOSRuntimeContext {
    /* Flash references - NO code in RAM */
    u32 current_module_id;          /* Current module ID */
    u32 code_flash_offset;          /* Code offset in Flash */
    u32 code_size;                  /* Code size */
    
    /* Runtime state (minimal RAM usage) */
    u32 program_counter;            /* PC - relative offset */
    u32 stack_pointer;              /* Stack pointer */
    u8  operand_stack[256];         /* Operand stack (small) */
    u32 stack_top;                  /* Stack top index */
};

/* Reduce RAM usage from 16KB to ~1KB */
```

#### 1.2 添加 Flash 访问接口

```c
/* gcos_flash_exec.h - NEW FILE */

#ifndef GCOS_FLASH_EXEC_H
#define GCOS_FLASH_EXEC_H

#include "gcos_vm.h"

/**
 * Fetch bytecode from Flash (XIP - Execute In Place)
 * @param flash_offset Flash offset
 * @return Bytecode byte
 */
static inline u8 flash_fetch_byte(u32 flash_offset) {
    return eflash_read_byte(flash_offset);
}

/**
 * Fetch 16-bit value from Flash (little-endian)
 */
static inline u16 flash_fetch_u16(u32 flash_offset) {
    u8 b0 = eflash_read_byte(flash_offset);
    u8 b1 = eflash_read_byte(flash_offset + 1);
    return (u16)b0 | ((u16)b1 << 8);
}

/**
 * Fetch 32-bit value from Flash (little-endian)
 */
static inline u32 flash_fetch_u32(u32 flash_offset) {
    u8 b0 = eflash_read_byte(flash_offset);
    u8 b1 = eflash_read_byte(flash_offset + 1);
    u8 b2 = eflash_read_byte(flash_offset + 2);
    u8 b3 = eflash_read_byte(flash_offset + 3);
    return (u32)b0 | ((u32)b1 << 8) | 
           ((u32)b2 << 16) | ((u32)b3 << 24);
}

/**
 * Execute instruction from Flash
 * @param vm VM instance
 * @param pc_offset Program counter offset (relative to code_flash_offset)
 * @return GCOSResult
 */
GCOSResult flash_execute_instruction(GCOSVM *vm, u32 pc_offset);

#endif /* GCOS_FLASH_EXEC_H */
```

---

### 阶段 2: 修改加载器（短期）

#### 2.1 流式解析 SEF（不加载到 RAM）

```c
/* gcos_loader.c - BEFORE (❌) */
GCOSResult gcos_loader_load_sef(GCOSVM *vm, const u8 *sef_data, u32 sef_size) {
    /* Parse entire SEF in RAM */
    /* Copy code to vm->runtime.module_code */
    memcpy(vm->runtime.module_code, code_data, code_size);
}

/* gcos_loader.c - AFTER (✅) */
GCOSResult gcos_loader_load_sef_to_flash(GCOSVM *vm, const u8 *sef_data, u32 sef_size) {
    /* Step 1: Write SEF to Flash */
    u32 flash_offset = eflash_allocate_sef_storage(sef_size);
    eflash_write_block(flash_offset, sef_data, sef_size);
    
    /* Step 2: Parse SEF header and sections (streaming) */
    GCOSModuleMetadata metadata;
    parse_sef_metadata(sef_data, sef_size, &metadata);
    
    /* Step 3: Update metadata with Flash locations */
    metadata.sef_flash_offset = flash_offset;
    metadata.code_flash_offset = flash_offset + metadata.code_section_offset;
    
    /* Step 4: Save metadata to Flash */
    eflash_save_module_metadata(vm->module_count, &metadata);
    
    /* Step 5: Register module (minimal RAM) */
    vm->modules[vm->module_count].flash_info.sef_flash_offset = flash_offset;
    vm->modules[vm->module_count].flash_info.code_flash_offset = metadata.code_flash_offset;
    vm->modules[vm->module_count].flash_info.code_size = metadata.code_size;
    
    vm->module_count++;
    
    return GCOS_SUCCESS;
}
```

---

### 阶段 3: 修改执行器（中期）

#### 3.1 从 Flash 取指执行

```c
/* gcos_executor.c - BEFORE (❌) */
void execute_instruction(GCOSVM *vm) {
    u8 opcode = vm->runtime.module_code[vm->runtime.program_counter];
    vm->runtime.program_counter++;
    /* Execute... */
}

/* gcos_executor.c - AFTER (✅) */
void execute_instruction(GCOSVM *vm) {
    GCOSModuleFlashInfo *flash_info = &vm->current_module->flash_info;
    
    /* Fetch from Flash - XIP */
    u8 opcode = flash_fetch_byte(
        flash_info->code_flash_offset + vm->runtime.program_counter
    );
    
    vm->runtime.program_counter++;
    
    /* Execute... */
}
```

---

### 阶段 4: 持久化符号表（中期）

#### 4.1 保存/加载符号表

```c
/* gcos_persistence.c - NEW FUNCTIONS */

GCOSResult eflash_save_symbol_table(const GCOSSymbolResolver *resolver) {
    GCOSSymbolTableFlash flash_data;
    
    /* Fill header */
    flash_data.magic = SYMBOL_TABLE_MAGIC;
    flash_data.version = SYMBOL_TABLE_VERSION;
    flash_data.global_ref_count = resolver->global_ref_count;
    
    /* Copy global reference table */
    for (u16 i = 0; i < resolver->global_ref_count; i++) {
        flash_data.global_refs[i].logical_address = 
            resolver->global_ref_table[i].logical_address;
        flash_data.global_refs[i].module_id = 
            resolver->global_ref_table[i].module_id;
        flash_data.global_refs[i].symbol_index = 
            resolver->global_ref_table[i].symbol_index;
        flash_data.global_refs[i].is_valid = 
            resolver->global_ref_table[i].is_valid;
    }
    
    /* Calculate checksum */
    flash_data.checksum = crc32(&flash_data, sizeof(flash_data) - 4);
    
    /* Write to Flash */
    return eflash_write_block(SYMBOL_TABLE_FLASH_OFFSET, 
                             (u8 *)&flash_data, 
                             sizeof(flash_data));
}

GCOSResult eflash_load_symbol_table(GCOSSymbolResolver *resolver) {
    GCOSSymbolTableFlash flash_data;
    
    /* Read from Flash */
    eflash_read_block(SYMBOL_TABLE_FLASH_OFFSET, 
                     (u8 *)&flash_data, 
                     sizeof(flash_data));
    
    /* Verify magic and checksum */
    if (flash_data.magic != SYMBOL_TABLE_MAGIC) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (flash_data.checksum != crc32(&flash_data, sizeof(flash_data) - 4)) {
        return GCOS_ERR_CHECKSUM_MISMATCH;
    }
    
    /* Restore to RAM */
    resolver->global_ref_count = flash_data.global_ref_count;
    
    for (u16 i = 0; i < flash_data.global_ref_count; i++) {
        resolver->global_ref_table[i].logical_address = 
            flash_data.global_refs[i].logical_address;
        resolver->global_ref_table[i].module_id = 
            flash_data.global_refs[i].module_id;
        resolver->global_ref_table[i].symbol_index = 
            flash_data.global_refs[i].symbol_index;
        resolver->global_ref_table[i].is_valid = 
            flash_data.global_refs[i].is_valid;
    }
    
    return GCOS_SUCCESS;
}
```

---

## 📊 内存占用对比

### 重构前（❌）

```
RAM Usage:
┌─────────────────────────────────┬──────────┐
│ Component                       │ Size     │
├─────────────────────────────────┼──────────┤
│ module_code (code storage)      │ 16 KB    │ ❌
│ global_data                     │ 4 KB     │
│ heap                            │ 4 KB     │
│ operand_stack                   │ 1 KB     │
│ symbol_resolver                 │ ~10 KB   │ ❌
│ Other runtime state             │ ~2 KB    │
├─────────────────────────────────┼──────────┤
│ Total                           │ ~37 KB   │ ❌ Too large!
└─────────────────────────────────┴──────────┘

Flash Usage:
┌─────────────────────────────────┬──────────┐
│ Firmware                        │ 128 KB   │
│ SEF files (raw)                 │ 64 KB    │
│ Unorganized data                │ ?        │
└─────────────────────────────────┴──────────┘
```

### 重构后（✅）

```
RAM Usage:
┌─────────────────────────────────┬──────────┐
│ Component                       │ Size     │
├─────────────────────────────────┼──────────┤
│ Flash references (offsets)      │ 64 B     │ ✅
│ global_data                     │ 4 KB     │
│ heap                            │ 4 KB     │
│ operand_stack                   │ 256 B    │ ✅
│ symbol_resolver (cached)        │ ~2 KB    │ ✅
│ Runtime state (PC, SP)          │ 32 B     │ ✅
│ Other state                     │ ~1 KB    │
├─────────────────────────────────┼──────────┤
│ Total                           │ ~11.3 KB │ ✅ Fits in 16KB!
└─────────────────────────────────┴──────────┘

Flash Usage:
┌─────────────────────────────────┬──────────┐
│ Firmware                        │ 128 KB   │
│ SEF files (executable)          │ 64 KB    │ ✅
│ Module metadata                 │ 4 KB     │ ✅
│ Symbol tables                   │ 4 KB     │ ✅
│ Runtime state (backup)          │ 4 KB     │ ✅
│ Reserved                        │ 52 KB    │
└─────────────────────────────────┴──────────┘
```

**RAM 节省**: 37 KB → 11.3 KB (**减少 69%**）✅

---

## ✅ 实施检查清单

### 立即实施（本周）

- [ ] 创建 `gcos_flash_exec.h` 头文件
- [ ] 修改 `GCOSRuntimeContext` 移除 `module_code[]`
- [ ] 添加 Flash 访问宏/函数
- [ ] 更新测试用例

### 短期实施（1-2周）

- [ ] 修改 `gcos_loader.c` 支持 Flash 存储
- [ ] 实现 SEF 流式解析
- [ ] 添加模块元数据结构
- [ ] 实现元数据持久化

### 中期实施（2-4周）

- [ ] 修改 `gcos_executor.c` 支持 XIP
- [ ] 实现符号表持久化
- [ ] 添加 CRC32 校验
- [ ] 实现启动时恢复逻辑

### 长期优化（1-2月）

- [ ] 实现磨损均衡
- [ ] 添加版本管理和回滚
- [ ] 实现增量更新
- [ ] 性能优化和基准测试

---

## 🎯 总结

### 核心改进

1. **代码存储在 Flash**：从 RAM 中移除 16KB 代码存储
2. **XIP 执行**：直接从 Flash 取指执行
3. **符号表持久化**：重启后无需重新解析
4. **清晰的 Flash 布局**：分区明确，易于维护
5. **最小化 RAM 占用**：从 37KB 降至 11.3KB

### 关键收益

- ✅ **适合智能卡**：RAM 占用减少 69%
- ✅ **掉电安全**：所有数据持久化
- ✅ **快速启动**：无需加载代码
- ✅ **节省空间**：单一副本存储
- ✅ **可扩展**：清晰的架构便于维护

---

**版本**: 1.0.0  
**日期**: 2026-05-12  
**状态**: 待实施
