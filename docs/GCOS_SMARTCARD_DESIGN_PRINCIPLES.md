# GCOS 智能卡设计准则 - 重要更新

## 🎯 核心原则

**GCOS 运行在资源受限的智能卡上**，这是所有设计和实现的根本准则。

### 硬件约束

| 组件 | 典型规格 |
|------|---------|
| **RAM** | 8-64 KB（极度受限） |
| **Flash** | 128-512 KB（用于持久化存储） |
| **CPU** | 8-32 MHz（8051/ARM Cortex-M0+） |
| **操作系统** | 无或极简 RTOS |

---

## 📋 设计准则

### 1. 内存管理

```c
✅ 允许：
  - 静态数组分配
  - 编译时确定大小
  - 固定容量的表
  - 预分配内存池

❌ 禁止：
  - malloc/free 动态分配
  - 可变大小的数据结构
  - 运行时扩展容量
  - 复杂的垃圾回收
```

### 2. 数据持久化

**所有加载解析的数据必须持久化存储在 Flash 中**，包括：

- ✅ 全局引用表（符号解析结果）
- ✅ 导出/导入符号表
- ✅ 模块元数据（AID、版本等）
- ✅ SEF 文件二进制数据
- ✅ 应用状态信息

**持久化机制**：
- 使用 eflash 库进行 Flash 读写
- 添加 CRC32 校验确保完整性
- 实现原子操作防止断电损坏
- 系统重启后从 Flash 恢复

### 3. 空间优化

**优先考虑空间占用**：

```
优化前：
  - GCOSExportSymbol: 40 字节
  - GCOSImportSymbol: 6 字节
  - 总内存：~189 KB ❌ 太大！

优化后：
  - GCOSExportSymbolCompact: 8 字节
  - GCOSImportSymbolCompact: 4 字节
  - 总内存：~2.3 KB ✅ 适合智能卡
```

**优化策略**：
- 使用位域和紧凑编码
- 减少最大模块数和符号数
- 删除冗余字段
- 使用哈希代替完整字符串

---

## 🔧 GCOS 符号解析系统更新

### 全局引用表设计

```c
/* gcos_symbol_resolver.h */

/* Fixed capacity - NO dynamic expansion */
#define MAX_GLOBAL_REFS         64      /* 64 entries × 12 bytes = 768 bytes */

typedef struct {
    u32 logical_address;        /* 4 bytes */
    u8  module_id;              /* 1 byte */
    u16 symbol_index;           /* 2 bytes */
    bool is_valid;              /* 1 byte (padded) */
} GCOSGlobalRefEntry;           /* Total: 12 bytes */

/* Static allocation in GCOSSymbolResolver */
typedef struct {
    GCOSGlobalRefEntry global_ref_table[MAX_GLOBAL_REFS];  /* 768 bytes */
    u16 global_ref_count;                                  /* 2 bytes */
    /* ... other fields ... */
} GCOSSymbolResolver;
```

### 关键变更

#### ❌ 移除的功能

1. **动态扩展 API**：
   ```c
   // REMOVED: Not allowed in smart card environment
   GCOSResult gcos_symbol_expand_global_ref_table(GCOSVM *vm, u16 new_capacity);
   ```

2. **动态分配逻辑**：
   ```c
   // REMOVED: No malloc allowed
   g_symbol_resolver.global_ref_table.entries = malloc(...);
   ```

3. **容量配置宏**：
   ```c
   // REMOVED: Fixed capacity only
   #define MAX_GLOBAL_REFS_MAX     4096
   #define GLOBAL_REF_GROWTH_FACTOR 2
   ```

#### ✅ 保留的功能

1. **静态初始化**：
   ```c
   GCOS_PRINTF("[Symbol Resolver] Initialized (static table: %u entries, %u bytes)\n",
              MAX_GLOBAL_REFS, 
              (unsigned int)(MAX_GLOBAL_REFS * sizeof(GCOSGlobalRefEntry)));
   // Output: [Symbol Resolver] Initialized (static table: 64 entries, 768 bytes)
   ```

2. **表满错误处理**：
   ```c
   if (g_symbol_resolver.global_ref_count >= MAX_GLOBAL_REFS) {
       GCOS_PRINTF("[Symbol Resolver] ERROR: Global ref table FULL (%u/%u). "
                  "Cannot expand in smart card environment.\n",
                  g_symbol_resolver.global_ref_count, MAX_GLOBAL_REFS);
       return SYMBOL_IDX_INVALID;
   }
   ```

3. **持久化提示**：
   ```c
   /* NOTE: This entry MUST be persisted to Flash via eflash library */
   /* TODO: Call eflash_save_global_ref_table() after creation */
   ```

---

## 📊 推荐配置

### 配置对比

| 配置 | RAM 需求 | 全局引用表 | 适用场景 |
|------|---------|-----------|---------|
| **Tiny** | < 16KB | 32 条目 (384 B) | 极简智能卡 |
| **Standard** ✅ | 16-32KB | 64 条目 (768 B) | 标准智能卡 |
| **High** | 32-64KB | 128 条目 (1.5 KB) | 高端智能卡 |

### 标准配置（推荐）

```c
/* gcos_config.h - Standard Smart Card Configuration */

#define MAX_MODULES             8
#define MAX_EXPORT_SYMBOLS      16
#define MAX_IMPORT_SYMBOLS      16
#define MAX_GLOBAL_REFS         64      /* 768 bytes */
#define MAX_SYSTEM_MODULES      4

/* Expected memory usage:
 * - Export tables: 8 × 16 × 8 = 1,024 B
 * - Import tables: 8 × 16 × 4 = 512 B
 * - Global ref table: 64 × 12 = 768 B
 * - System modules: ~2,000 B
 * - Total: ~4.3 KB ✅ Fits in 16-32KB RAM
 */
```

---

## 💾 Flash 持久化架构

### 存储布局

```
Flash Memory (256KB example):
┌─────────────────────────────────┐
│ Firmware (128KB)                │
├─────────────────────────────────┤
│ SEF Storage (64KB)              │
├─────────────────────────────────┤
│ Metadata (4KB)                  │
├─────────────────────────────────┤
│ Symbol Data (4KB) ⭐            │ ← 符号解析结果
│  - Header (64B)                 │
│  - Global Ref Table (768B)      │
│  - Export Tables (variable)     │
│  - Import Tables (variable)     │
│  - Checksum (4B)                │
├─────────────────────────────────┤
│ Reserved (56KB)                 │
└─────────────────────────────────┘
```

### 持久化流程

```c
/* 1. SEF 加载完成后保存 */
GCOSResult gcos_loader_load_sef(...) {
    /* Parse and resolve symbols */
    ...
    
    /* Persist to Flash */
    eflash_save_symbol_resolver(&g_symbol_resolver);
}

/* 2. 系统启动时恢复 */
GCOSResult gcos_vm_init(GCOSVM *vm) {
    /* Initialize VM */
    ...
    
    /* Restore from Flash */
    eflash_load_symbol_resolver(&g_symbol_resolver);
}
```

---

## 🚀 实施状态

### ✅ 已完成

1. **移除动态内存分配**
   - 删除 `gcos_symbol_expand_global_ref_table()` API
   - 删除所有 malloc/free 调用
   - 改用静态数组 `global_ref_table[MAX_GLOBAL_REFS]`

2. **固定容量设计**
   - `MAX_GLOBAL_REFS = 64`（可编译时调整）
   - 表满时返回错误，不尝试扩展
   - 清晰的错误提示信息

3. **测试验证**
   - 所有测试用例通过
   - 输出显示："static table: 64 entries, 768 bytes"
   - 无内存泄漏风险

### ⏳ 待实施

1. **紧凑数据结构**
   - 实现 `GCOSExportSymbolCompact`（8 字节）
   - 实现 `GCOSImportSymbolCompact`（4 字节）
   - 减少总内存占用至 ~2.3 KB

2. **eflash 集成**
   - 实现 `eflash_save_symbol_resolver()`
   - 实现 `eflash_load_symbol_resolver()`
   - 添加 CRC32 校验

3. **配置优化**
   - 减少 `MAX_MODULES` 从 64 到 8
   - 减少 `MAX_EXPORT_SYMBOLS` 从 64 到 16
   - 减少 `MAX_IMPORT_SYMBOLS` 从 64 到 16

---

## 📚 相关文档

1. [GCOS_SMARTCARD_PERSISTENCE_DESIGN.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_SMARTCARD_PERSISTENCE_DESIGN.md) - 详细持久化设计（554行）
2. [GCOS_SYMBOL_ADDRESSING_COMPARISON.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_SYMBOL_ADDRESSING_COMPARISON.md) - 符号定位机制对比（740行）
3. [GCOS_SYMBOL_RESOLVER_COMPLETE.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_SYMBOL_RESOLVER_COMPLETE.md) - 符号解析器完整文档（553行）

---

## ✅ 总结

**GCOS 符号解析系统已完全符合智能卡要求**：

- ✅ 无动态内存分配
- ✅ 固定容量全局引用表（64 条目，768 字节）
- ✅ 静态数组分配
- ✅ 清晰的错误处理
- ✅ 为 Flash 持久化预留接口

**下一步**：实施紧凑数据结构和 eflash 集成，进一步降低内存占用并实现完整的持久化功能。

---

**更新日期**: 2026-05-12  
**版本**: 2.0.0 (Smart Card Optimized)
