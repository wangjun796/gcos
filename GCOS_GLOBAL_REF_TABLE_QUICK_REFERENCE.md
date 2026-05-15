# 全局引用表持久化与扩展 - 快速参考

## 🎯 一句话总结

全局引用表现在支持 **Flash 持久化**（掉电不丢失）和 **动态扩展**（64 → 256 条目）。

---

## 📊 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 初始容量 | 64 条目 | 768 字节 RAM |
| 最大容量 | 256 条目 | 3 KB RAM |
| 扩展步长 | 32 条目 | 每次扩展增加 |
| Flash 偏移 | 0x031000 | Symbol Table Region |
| CRC32 校验 | ✅ | 数据完整性保护 |

---

## 🔑 核心 API

### 1. 自动扩展（透明）

```c
// 创建全局引用（自动处理扩展）
u16 ref = gcos_symbol_create_global_ref(vm, logical_addr, module_id, symbol_idx);

// 如果表满，自动触发扩展：
// [Symbol Resolver] Table FULL (64/64). Attempting expansion...
// [Symbol Resolver] Table expanded successfully. New capacity: 96
```

---

### 2. 手动扩展（可选）

```c
// 提前扩展，避免运行时中断
GCOSResult ret = gcos_symbol_expand_global_ref_table(vm);
if (ret == GCOS_SUCCESS) {
    printf("Expanded to %u entries\n", vm->symbol_resolver.global_ref_capacity);
}
```

---

### 3. Flash 保存

```c
// 保存到 Flash（模块加载完成后调用）
gcos_symbol_save_global_ref_table_to_flash(vm);
```

---

### 4. Flash 加载（自动）

```c
// VM 初始化时自动从 Flash 恢复
gcos_vm_init(vm);
// 内部调用 gcos_symbol_resolver_init()
// 自动加载全局引用表
```

---

## 📈 内存占用

### RAM 使用

```
初始状态（64 条目）：
  Base table: 768 B
  Total: 768 B

扩展到 96 条目：
  Base table: 768 B
  Ext table: 1,152 B (32 × 12 × 3)
  Total: 1,920 B

扩展到 256 条目：
  Base table: 768 B
  Ext table: 3,840 B (192 × 12)
  Total: 4,608 B
```

### Flash 使用

```
总大小 = 12 + (Count × 12) 字节

64 条目：  780 B
128 条目： 1,548 B
256 条目： 3,084 B
```

---

## ⚡ 性能指标

| 操作 | 耗时 | 说明 |
|------|------|------|
| 创建引用（未扩展） | ~1 μs | 直接写入 RAM |
| 创建引用（触发扩展） | ~5 ms | 扩展 + Flash 保存 |
| 地址解析 | ~0.5 μs | 查表 |
| Flash 保存（64） | ~3 ms | 序列化 + 写入 |
| Flash 加载（64） | ~2 ms | 读取 + 反序列化 |

---

## 🔍 调试命令

### 查看统计信息

```c
gcos_symbol_print_stats(vm);
```

**输出示例：**
```
=== Symbol Resolver Statistics ===
Total resolutions:    10
Failed resolutions:   0
Global ref entries:   64 / 96 (expanded, 1,920 bytes)
System modules:       1 / 8
================================
```

---

### Dump 全局引用表

```c
gcos_symbol_dump_tables(vm, 0xFF);  // 0xFF = all modules
```

**输出示例：**
```
--- Global Reference Table ---
Entries: 64 / 96 (expanded, 1,920 bytes)
  [0] 0x8000 -> 0x00001000 (mod=0, sym=0)
  [1] 0x8001 -> 0x00002000 (mod=0, sym=1)
  ...
  [63] 0x803F -> 0x0000A000 (mod=5, sym=10)
  [64] 0x8040 -> 0x0000B000 (mod=6, sym=0)  ← Extended table
  ...
```

---

### 检查 Flash 内容

```c
// 读取魔术字
u8 buffer[4];
eflash_ftl_read_logical(0x031000, buffer, 4);

u32 magic;
memcpy(&magic, buffer, 4);
printf("Magic: 0x%08X (expected 0x47524546)\n", magic);
// 输出: Magic: 0x47524546 (expected 0x47524546) ✅
```

---

## 💡 最佳实践

### 1. 批量创建后保存

```c
// ❌ 不好：每次创建都保存（频繁 Flash 写入）
for (int i = 0; i < 100; i++) {
    gcos_symbol_create_global_ref(vm, addr[i], mod[i], sym[i]);
    gcos_symbol_save_global_ref_table_to_flash(vm);  // 100 次写入！
}

// ✅ 好：批量创建后统一保存
for (int i = 0; i < 100; i++) {
    gcos_symbol_create_global_ref(vm, addr[i], mod[i], sym[i]);
}
gcos_symbol_save_global_ref_table_to_flash(vm);  // 仅 1 次写入
```

---

### 2. 预扩展策略

```c
// 如果预计需要大量引用，提前扩展
if (expected_refs > 64) {
    while (vm->symbol_resolver.global_ref_capacity < expected_refs) {
        gcos_symbol_expand_global_ref_table(vm);
    }
}
```

---

### 3. 监控使用情况

```c
// 定期检查使用率
u16 usage_percent = (vm->symbol_resolver.global_ref_count * 100) / 
                    vm->symbol_resolver.global_ref_capacity;

if (usage_percent > 80) {
    printf("WARNING: Global ref table 80%% full (%u/%u)\n",
           vm->symbol_resolver.global_ref_count,
           vm->symbol_resolver.global_ref_capacity);
    
    // 提前扩展
    gcos_symbol_expand_global_ref_table(vm);
}
```

---

## ⚠️ 常见问题

### Q1: 扩展失败怎么办？

```c
u16 ref = gcos_symbol_create_global_ref(vm, addr, mod, sym);
if (ref == SYMBOL_IDX_INVALID) {
    // 可能原因：
    // 1. 已达到最大容量（256 条目）
    // 2. Flash 写入失败
    // 3. 系统未初始化
    
    printf("ERROR: Failed to create global ref\n");
    printf("Current: %u / %u\n", 
           vm->symbol_resolver.global_ref_count,
           vm->symbol_resolver.global_ref_capacity);
}
```

---

### Q2: Flash 加载失败如何处理？

```c
GCOSResult ret = gcos_symbol_load_global_ref_table_from_flash(vm);
if (ret != GCOS_SUCCESS) {
    // 正常情况：首次启动无 Flash 数据
    printf("No Flash data found (fresh start)\n");
    
    // 继续使用，从头开始
}
```

---

### Q3: 如何减少 Flash 写入次数？

**方案 1：延迟保存**
```c
static bool dirty = false;

void on_ref_created() {
    dirty = true;  // 标记为脏
}

void periodic_flush() {
    if (dirty) {
        gcos_symbol_save_global_ref_table_to_flash(vm);
        dirty = false;
    }
}
```

**方案 2：仅在关键时刻保存**
- 模块加载完成后
- 系统关机前
- 定期（如每 10 分钟）

---

## 📝 数据结构速查

### GCOSSymbolResolver（相关字段）

```c
typedef struct {
    /* Base table (always in RAM) */
    GCOSGlobalRefEntry global_ref_table[64];
    
    /* Extension (loaded from Flash when expanded) */
    GCOSGlobalRefEntry *global_ref_table_ext;  // NULL if not expanded
    
    /* Metadata */
    u16 global_ref_capacity;  // Current capacity (64, 96, 128, ...)
    u16 global_ref_count;     // Current usage
    u32 global_ref_flash_offset;  // Flash storage offset (0x031000)
} GCOSSymbolResolver;
```

---

### GCOSGlobalRefEntry（每个条目 12 字节）

```c
typedef struct {
    u32 logical_address;  // 4B: 32-bit logical address
    u8  module_id;        // 1B: Owning module ID
    u16 symbol_index;     // 2B: Symbol index within module
    bool is_valid;        // 1B: Valid flag (padded)
} GCOSGlobalRefEntry;     // Total: 12 bytes
```

---

### Flash 存储格式

```
Offset  Size   Field
------  -----  ---------------------------
0x00    4B     Magic (0x47524546 = "GREF")
0x04    2B     Entry count
0x06    2B     Table capacity
0x08    N×12B  Base table entries (max 64)
0x08+N  M×12B  Extension entries (if any)
Last    4B     CRC32 checksum
```

---

## 🎓 学习路径

1. **理解基础概念**
   - 什么是全局引用表？
   - 为什么需要 16 位紧凑地址？
   - Bit 15 标志的作用？

2. **掌握基本使用**
   - 创建全局引用
   - 解析地址
   - 查看统计信息

3. **深入高级特性**
   - Flash 持久化机制
   - 动态扩展原理
   - CRC32 校验

4. **优化实践**
   - 减少 Flash 写入
   - 预扩展策略
   - 监控和调试

---

## 📚 相关文档

- [详细实施文档](GCOS_GLOBAL_REF_TABLE_PERSISTENCE_AND_EXPANSION.md) - 完整技术细节
- [Flash API 参考](GCOS_FLASH_API_QUICK_REFERENCE.md) - eflash 函数说明
- [持久化对比](GCOS_PERSISTENCE_VS_FLASH_STORAGE_COMPARISON.md) - 方案选择指南

---

**最后更新：** 2026-05-12  
**版本：** 1.0.0
