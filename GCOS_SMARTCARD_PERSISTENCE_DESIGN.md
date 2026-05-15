# GCOS 符号解析系统 - 智能卡持久化设计

## 📋 执行摘要

**重要设计准则**：GCOS 运行在**资源受限的智能卡**上（RAM 8-64KB），所有符号解析数据必须**持久化存储在 Flash** 中。

### 关键约束

1. **无动态内存分配**：禁止使用 malloc/free，全部采用静态分配
2. **固定容量**：全局引用表大小固定为 64 条目（768 字节）
3. **Flash 持久化**：所有符号数据必须保存到 Flash，重启后可恢复
4. **空间优化**：数据结构紧凑，最小化 RAM 和 Flash 占用

---

## 🎯 智能卡环境要求

### 典型硬件规格

| 组件 | 规格 | 说明 |
|------|------|------|
| **CPU** | 8-32 MHz | 8051/ARM Cortex-M0+ |
| **RAM** | 8-64 KB | 极度受限 |
| **Flash** | 128-512 KB | 用于代码和数据存储 |
| **EEPROM** | 4-64 KB | 可选，用于频繁写入 |
| **操作系统** | 无或极简 RTOS | 通常裸机运行 |

### 设计影响

```
❌ 不允许：
  - malloc/free 动态内存分配
  - 可变大小的数据结构
  - 运行时扩展表容量
  - 复杂的垃圾回收

✅ 必须做到：
  - 编译时确定所有数据结构大小
  - 静态数组分配
  - 固定容量的表
  - 所有数据持久化到 Flash
```

---

## 🏗️ GCOS 符号解析系统设计

### 1. 全局引用表（静态分配）

#### 数据结构

```c
/* gcos_symbol_resolver.h */

/* Fixed capacity - NO dynamic expansion */
#define MAX_GLOBAL_REFS         64

/**
 * Global reference table entry (12 bytes per entry)
 * 
 * Memory layout:
 * - logical_address: 4 bytes (u32)
 * - module_id:       1 byte  (u8)
 * - symbol_index:    2 bytes (u16)
 * - is_valid:        1 byte  (bool, padded to 4 bytes alignment)
 * 
 * Total: 12 bytes × 64 entries = 768 bytes
 */
typedef struct {
    u32 logical_address;    /* 32-bit logical address */
    u8  module_id;          /* Module that owns this symbol */
    u16 symbol_index;       /* Symbol index within module */
    bool is_valid;          /* Whether entry is valid */
} GCOSGlobalRefEntry;

/* Static allocation in GCOSSymbolResolver */
typedef struct {
    /* ... other fields ... */
    
    /* Global reference table (STATIC - persisted to Flash) */
    GCOSGlobalRefEntry global_ref_table[MAX_GLOBAL_REFS];
    u16 global_ref_count;   /* Current usage count */
    
    /* ... other fields ... */
} GCOSSymbolResolver;
```

#### 内存占用分析

```
全局引用表：
  - 单条目大小：12 字节
  - 总条目数：64
  - 总内存：768 字节（0.75 KB）
  
对比不同配置：
  - 32 条目：384 字节  （适合 < 16KB RAM）
  - 64 条目：768 字节  （适合 16-32KB RAM）✅ 推荐
  - 128 条目：1.5 KB   （适合 32-64KB RAM）
  - 256 条目：3 KB     （适合 > 64KB RAM）
```

#### 初始化

```c
GCOSResult gcos_symbol_resolver_init(GCOSVM *vm) {
    /* Static allocation - no malloc needed */
    memset(&g_symbol_resolver, 0, sizeof(GCOSSymbolResolver));
    
    g_symbol_resolver.global_ref_count = 0;
    
    for (int i = 0; i < MAX_GLOBAL_REFS; i++) {
        g_symbol_resolver.global_ref_table[i].is_valid = false;
    }
    
    GCOS_PRINTF("[Symbol Resolver] Initialized (static table: %u entries, %u bytes)\n",
               MAX_GLOBAL_REFS, 
               (unsigned int)(MAX_GLOBAL_REFS * sizeof(GCOSGlobalRefEntry)));
    
    return GCOS_SUCCESS;
}
```

**输出示例**：
```
[Symbol Resolver] Initialized (static table: 64 entries, 768 bytes)
```

---

### 2. 创建全局引用（无扩展）

```c
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index) {
    if (!g_resolver_initialized) {
        return SYMBOL_IDX_INVALID;
    }
    
    /* Check if static table is full - NO expansion allowed */
    if (g_symbol_resolver.global_ref_count >= MAX_GLOBAL_REFS) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Global ref table FULL (%u/%u). "
                   "Cannot expand in smart card environment.\n",
                   g_symbol_resolver.global_ref_count, MAX_GLOBAL_REFS);
        GCOS_PRINTF("[Symbol Resolver] HINT: Reduce cross-module references "
                   "or increase MAX_GLOBAL_REFS at compile time.\n");
        return SYMBOL_IDX_INVALID;
    }
    
    u16 index = g_symbol_resolver.global_ref_count;
    GCOSGlobalRefEntry *entry = &g_symbol_resolver.global_ref_table[index];
    
    entry->logical_address = logical_address;
    entry->module_id = module_id;
    entry->symbol_index = symbol_index;
    entry->is_valid = true;
    
    g_symbol_resolver.global_ref_count++;
    
    /* IMPORTANT: Persist to Flash after creation */
    /* TODO: eflash_save_global_ref_table() */
    
    return make_global_addr(index);  /* index | 0x8000 */
}
```

**关键点**：
- ✅ 检查表满，返回错误
- ❌ **不尝试扩展**（无 malloc）
- 💾 **必须持久化到 Flash**

---

### 3. Flash 持久化设计

#### 持久化策略

```
┌─────────────────────────────────────────────┐
│ GCOS 符号解析数据持久化架构                  │
├─────────────────────────────────────────────┤
│                                             │
│  RAM (运行时)                                │
│  ┌───────────────────────────┐              │
│  │ GCOSSymbolResolver        │              │
│  │  - global_ref_table[64]   │◄──┐         │
│  │  - export_tables[]        │   │         │
│  │  - import_tables[]        │   │         │
│  │  - system_modules[]       │   │         │
│  └───────────────────────────┘   │         │
│                                  │         │
│  保存时机:                        │         │
│  1. SEF 模块加载完成后           │         │
│  2. 符号解析完成后               │         │
│  3. 应用安装完成后               │         │
│                                  │         │
│  Flash (持久化存储)               │         │
│  ┌───────────────────────────┐   │         │
│  │ eflash 存储区域            │   │         │
│  │  - 元数据区                │   │         │
│  │  - SEF 二进制数据          │   │         │
│  │  - 符号解析结果            │───┘         │
│  │    · 全局引用表            │             │
│  │    · 导出符号表            │             │
│  │    · 导入符号表            │             │
│  └───────────────────────────┘              │
│                                             │
│  恢复时机:                                   │
│  1. 系统启动时                               │
│  2. VM 初始化后                              │
│  3. 从 Flash 加载到 RAM                      │
└─────────────────────────────────────────────┘
```

#### eflash 集成接口

```c
/* gcos_persistence.h - 待实现 */

/**
 * Save symbol resolver state to Flash
 * Called after SEF loading and symbol resolution
 */
GCOSResult eflash_save_symbol_resolver(const GCOSSymbolResolver *resolver);

/**
 * Load symbol resolver state from Flash
 * Called during VM initialization
 */
GCOSResult eflash_load_symbol_resolver(GCOSSymbolResolver *resolver);

/**
 * Save single global reference entry (atomic operation)
 * For incremental updates
 */
GCOSResult eflash_save_global_ref_entry(u16 index, const GCOSGlobalRefEntry *entry);

/**
 * Clear all symbol data in Flash
 * For factory reset or complete reinstall
 */
GCOSResult eflash_clear_symbol_data(void);
```

#### 持久化流程

```c
/* gcos_loader.c - SEF 加载完成后 */

GCOSResult gcos_loader_load_sef(GCOSVM *vm, const u8 *sef_data, u32 sef_size) {
    /* 1. Parse SEF file */
    /* ... existing code ... */
    
    /* 2. Resolve symbols */
    for (u8 i = 0; i < vm->module_count; i++) {
        gcos_symbol_resolve_imports(vm, i);
    }
    
    /* 3. Persist symbol data to Flash */
    GCOSResult ret = eflash_save_symbol_resolver(&g_symbol_resolver);
    if (ret != GCOS_OK) {
        GCOS_PRINTF("[Loader] WARNING: Failed to persist symbol data\n");
        /* Continue anyway - data is in RAM */
    }
    
    return GCOS_SUCCESS;
}
```

#### 恢复流程

```c
/* gcos_vm.c - VM 初始化时 */

GCOSResult gcos_vm_init(GCOSVM *vm) {
    /* 1. Initialize VM core */
    /* ... existing code ... */
    
    /* 2. Initialize symbol resolver */
    gcos_symbol_resolver_init(vm);
    
    /* 3. Try to load symbol data from Flash */
    GCOSResult ret = eflash_load_symbol_resolver(&g_symbol_resolver);
    if (ret == GCOS_OK) {
        GCOS_PRINTF("[VM] Symbol data restored from Flash\n");
    } else {
        GCOS_PRINTF("[VM] No saved symbol data - starting fresh\n");
    }
    
    return GCOS_SUCCESS;
}
```

---

### 4. Flash 存储布局

#### 建议的 Flash 分区

```
Flash Memory Layout (example: 256KB Flash)
┌─────────────────────────────────────────┐
│ 0x00000 - 0x1FFFF  | Firmware (128KB)   │  ← GCOS 固件代码
├─────────────────────────────────────────┤
│ 0x20000 - 0x2FFFF  | SEF Storage (64KB) │  ← SEF 文件二进制数据
├─────────────────────────────────────────┤
│ 0x30000 - 0x30FFF  | Metadata (4KB)     │  ← 应用元数据、AID映射
├─────────────────────────────────────────┤
│ 0x31000 - 0x31FFF  | Symbol Data (4KB)  │  ← 符号解析结果 ⭐
│                     │                    │
│  Structure:         │                    │
│  - Header (64B)     │                    │
│  - Global Ref Table │                    │
│    (64×12=768B)     │                    │
│  - Export Tables    │                    │
│  - Import Tables    │                    │
│  - Checksum (4B)    │                    │
├─────────────────────────────────────────┤
│ 0x32000 - 0x3FFFF  | Reserved (56KB)    │  ← 预留扩展
└─────────────────────────────────────────┘
```

#### 符号数据存储格式

```c
/* Flash storage format for symbol resolver */

typedef struct {
    /* Header */
    u32 magic;                  /* 0x53594D42 = "SYMB" */
    u32 version;                /* Format version */
    u16 global_ref_count;       /* Number of valid entries */
    u16 reserved;               /* Alignment */
    
    /* Global Reference Table */
    GCOSGlobalRefEntry global_refs[MAX_GLOBAL_REFS];
    
    /* Export tables (compact format) */
    u8 export_counts[MAX_MODULES];
    /* Followed by variable-length export data */
    
    /* Import tables (compact format) */
    u8 import_counts[MAX_MODULES];
    /* Followed by variable-length import data */
    
    /* Footer */
    u32 checksum;               /* CRC32 of entire structure */
} GCOSSymbolDataFlash;

/* Size calculation:
 * Header: 12 bytes
 * Global refs: 64 × 12 = 768 bytes
 * Export counts: 64 bytes
 * Import counts: 64 bytes
 * Checksum: 4 bytes
 * Total: ~912 bytes (< 1KB)
 */
```

---

## 📊 空间占用分析

### RAM 占用（完整符号解析器）

```
GCOSSymbolResolver 结构体:
┌──────────────────────────────────────┬──────────┐
│ 字段                                  │ 大小      │
├──────────────────────────────────────┼──────────┤
│ export_tables[64][64]                │ 160 KB   │ ← 需要优化！
│ export_counts[64]                    │ 64 B     │
│ import_tables[64][64]                │ 24 KB    │ ← 需要优化！
│ import_counts[64]                    │ 64 B     │
│ global_ref_table[64]                 │ 768 B    │ ✅
│ global_ref_count                     │ 2 B      │
│ system_modules[8]                    │ ~4 KB    │
│ statistics                           │ 12 B     │
├──────────────────────────────────────┼──────────┤
│ 总计                                  │ ~189 KB  │ ❌ 太大！
└──────────────────────────────────────┴──────────┘

问题：export/import 表过大，不适合智能卡！
```

### ⚠️ 需要优化的数据结构

当前设计对于智能卡来说**太大**了！需要重新设计：

#### 优化方案 1：减少最大模块数和符号数

```c
/* gcos_symbol_resolver.h - 智能卡优化配置 */

/* For smart cards with limited RAM */
#define MAX_MODULES             8       /* Reduced from 64 */
#define MAX_EXPORT_SYMBOLS      16      /* Reduced from 64 */
#define MAX_IMPORT_SYMBOLS      16      /* Reduced from 64 */
#define MAX_GLOBAL_REFS         64      /* Keep as is */
#define MAX_SYSTEM_MODULES      4       /* Reduced from 8 */

/* New memory usage:
 * export_tables[8][16]: 8 × 16 × 40 = 5,120 B (5 KB)
 * import_tables[8][16]: 8 × 16 × 6 = 768 B
 * global_ref_table[64]: 768 B
 * system_modules[4]: ~2 KB
 * Total: ~8.6 KB ✅ Fits in 16-32KB RAM
 */
```

#### 优化方案 2：使用紧凑编码

```c
/* Compact export symbol (reduce from 40 bytes to 16 bytes) */
typedef struct {
    u16 function_index;         /* 2 bytes */
    u32 logical_address;        /* 4 bytes */
    u8 name_hash;               /* 1 byte (hash of name, not full string) */
    u8 reserved;                /* 1 byte (alignment) */
} GCOSExportSymbolCompact;      /* 8 bytes total */

/* Compact import symbol (reduce from 6 bytes to 4 bytes) */
typedef struct {
    u16 module_idx_func_idx;    /* 2 bytes (COS3 format) */
    u16 resolved_address;       /* 2 bytes */
    /* Remove is_resolved flag - infer from resolved_address != 0xFFFF */
} GCOSImportSymbolCompact;      /* 4 bytes total */

/* New memory usage with compact format:
 * export_tables[8][16]: 8 × 16 × 8 = 1,024 B (1 KB)
 * import_tables[8][16]: 8 × 16 × 4 = 512 B
 * global_ref_table[64]: 768 B
 * Total: ~2.3 KB ✅ Excellent for smart cards!
 */
```

---

## 🔧 实施建议

### 阶段 1：立即实施（当前）

1. ✅ **已完成**：移除动态内存分配
2. ✅ **已完成**：固定全局引用表容量（64 条目）
3. ⏳ **待实施**：减少 MAX_MODULES 和符号表大小
4. ⏳ **待实施**：实现 eflash 持久化接口

### 阶段 2：短期优化（1-2周）

1. 实现紧凑数据结构（ExportSymbolCompact, ImportSymbolCompact）
2. 集成 eflash 库，实现 save/load 功能
3. 添加 Flash 数据校验（CRC32）
4. 实现原子写入（防止断电损坏）

### 阶段 3：长期优化（1-2月）

1. 实现增量更新（只保存变化的条目）
2. 添加磨损均衡（延长 Flash 寿命）
3. 实现压缩存储（进一步节省空间）
4. 支持多版本兼容（向后兼容）

---

## 📝 配置示例

### 配置 1：极小内存智能卡（8-16KB RAM）

```c
/* gcos_config_tiny.h */

#define MAX_MODULES             4
#define MAX_EXPORT_SYMBOLS      8
#define MAX_IMPORT_SYMBOLS      8
#define MAX_GLOBAL_REFS         32
#define MAX_SYSTEM_MODULES      2

/* Memory usage:
 * export_tables: 4 × 8 × 8 = 256 B
 * import_tables: 4 × 8 × 4 = 128 B
 * global_ref_table: 32 × 12 = 384 B
 * Total: ~1 KB
 */
```

### 配置 2：标准智能卡（16-32KB RAM）✅ 推荐

```c
/* gcos_config_standard.h */

#define MAX_MODULES             8
#define MAX_EXPORT_SYMBOLS      16
#define MAX_IMPORT_SYMBOLS      16
#define MAX_GLOBAL_REFS         64
#define MAX_SYSTEM_MODULES      4

/* Memory usage:
 * export_tables: 8 × 16 × 8 = 1,024 B
 * import_tables: 8 × 16 × 4 = 512 B
 * global_ref_table: 64 × 12 = 768 B
 * Total: ~2.3 KB
 */
```

### 配置 3：高端智能卡（32-64KB RAM）

```c
/* gcos_config_high.h */

#define MAX_MODULES             16
#define MAX_EXPORT_SYMBOLS      32
#define MAX_IMPORT_SYMBOLS      32
#define MAX_GLOBAL_REFS         128
#define MAX_SYSTEM_MODULES      8

/* Memory usage:
 * export_tables: 16 × 32 × 8 = 4,096 B
 * import_tables: 16 × 32 × 4 = 2,048 B
 * global_ref_table: 128 × 12 = 1,536 B
 * Total: ~7.7 KB
 */
```

---

## ✅ 总结

### 关键设计原则

1. **静态分配**：所有数据结构在编译时确定大小
2. **固定容量**：不支持运行时扩展
3. **Flash 持久化**：所有符号数据必须保存到 Flash
4. **空间优化**：使用紧凑编码，最小化内存占用
5. **无动态内存**：禁止 malloc/free

### 当前状态

- ✅ 全局引用表：静态分配，64 条目，768 字节
- ✅ 无动态扩展：表满时返回错误
- ⏳ 待优化：export/import 表仍然过大
- ⏳ 待实施：eflash 持久化集成

### 下一步行动

1. **立即**：减少 MAX_MODULES 和符号表大小
2. **短期**：实现紧凑数据结构
3. **中期**：集成 eflash 持久化
4. **长期**：实现增量更新和磨损均衡

---

**版本**: 1.0.0  
**日期**: 2026-05-12  
**适用平台**: 资源受限智能卡（8-64KB RAM）
