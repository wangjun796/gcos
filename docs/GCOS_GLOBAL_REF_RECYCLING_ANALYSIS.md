# 全局引用表回收机制分析与设计方案

## 📋 问题描述

用户提出了一个关键问题：**当应用删除后，如何回收全局引用表（Global Reference Table）中的条目？**

当前实现存在以下问题：
1. ❌ **没有回收机制**：创建的全局引用条目永远不会被释放
2. ❌ **无法追踪归属**：不知道哪个条目属于哪个模块/应用
3. ❌ **内存泄漏风险**：频繁安装/卸载应用会导致全局引用表耗尽
4. ❌ **is_valid 字段未充分利用**：仅标记有效性，未记录引用计数或所有者

---

## 🔍 当前实现分析

### 1. 全局引用表结构

```c
typedef struct {
    u32 logical_address;        /* 32-bit logical address */
    u8 module_id;               /* Module that owns this symbol */
    u16 symbol_index;           /* Symbol index within module */
    bool is_valid;              /* Whether entry is valid */
} GCOSGlobalRefEntry;
```

**已有字段：**
- ✅ `module_id` - 记录了所属模块ID
- ✅ `is_valid` - 标记条目是否有效

**缺失字段：**
- ❌ `reference_count` - 引用计数（多少地方在使用这个条目）
- ❌ `owner_app_id` - 所属应用ID（用于批量回收）
- ❌ `creation_timestamp` - 创建时间（用于LRU淘汰）

---

### 2. 创建流程

```c
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index) {
    // 1. 检查容量，必要时扩展
    if (global_ref_count >= global_ref_capacity) {
        expand_global_ref_table(vm);
    }
    
    // 2. 分配新条目
    u16 index = global_ref_count;
    entry->logical_address = logical_address;
    entry->module_id = module_id;
    entry->symbol_index = symbol_index;
    entry->is_valid = true;
    
    // 3. 增加计数
    global_ref_count++;
    
    // 4. 返回紧凑地址
    return make_global_addr(index);
}
```

**问题：**
- ❌ 只增不减：`global_ref_count` 只会增加，不会减少
- ❌ 线性分配：总是追加到末尾，不重用已释放的槽位
- ❌ 无引用追踪：不知道有多少导入符号在使用这个全局引用

---

### 3. 删除应用流程

```c
GCOSResult app_delete(GCOSVM *vm, u8 app_id) {
    // 1. 取消选择
    if (vm->selected_app == app) {
        app_deselect(vm, vm->current_channel);
    }
    
    // 2. 清空应用槽位
    memset(app, 0, sizeof(GCOSAppInstance));
    
    // 3. 减少应用计数
    vm->app_count--;
    
    // ❌ 没有清理全局引用表！
    // ❌ 没有清理模块的导出/导入符号表！
    
    return GCOS_SUCCESS;
}
```

**问题：**
- ❌ 仅清空应用数据结构
- ❌ 未清理该应用创建的模块
- ❌ 未回收全局引用表条目
- ❌ 未清理持久化存储（Flash）

---

## 💡 解决方案设计

### 方案 A：基于引用计数的回收（推荐）

#### 核心思想

每个全局引用条目维护一个**引用计数**，当计数降为0时自动回收。

#### 数据结构改进

```c
typedef struct {
    u32 logical_address;        /* 32-bit logical address */
    u8 module_id;               /* Module that owns this symbol */
    u16 symbol_index;           /* Symbol index within module */
    u8 reference_count;         /* NEW: Number of import symbols using this entry */
    u8 owner_app_id;            /* NEW: Application ID that created this entry */
    bool is_valid;              /* Whether entry is valid */
} GCOSGlobalRefEntry;
```

**新增字段说明：**
- `reference_count`: 
  - 创建时 = 1（创建者持有）
  - 每次有导入符号解析到此条目时 +1
  - 每次导入符号解除解析时 -1
  - 降为 0 时可回收
  
- `owner_app_id`:
  - 记录创建此条目的应用ID
  - 应用删除时，可批量回收该应用的所有条目
  - 0xFF 表示系统模块（不自动回收）

---

#### API 扩展

```c
/**
 * @brief Create global reference with owner tracking
 */
u16 gcos_symbol_create_global_ref_ex(GCOSVM *vm, u32 logical_address, 
                                      u8 module_id, u16 symbol_index,
                                      u8 owner_app_id);

/**
 * @brief Increment reference count (when import symbol resolves to this entry)
 */
void gcos_symbol_increment_ref_count(GCOSVM *vm, u16 global_ref_index);

/**
 * @brief Decrement reference count (when import symbol is unlinked)
 * Returns true if entry can be recycled
 */
bool gcos_symbol_decrement_ref_count(GCOSVM *vm, u16 global_ref_index);

/**
 * @brief Recycle a global reference entry (set is_valid = false)
 */
void gcos_symbol_recycle_global_ref(GCOSVM *vm, u16 global_ref_index);

/**
 * @brief Delete all global references owned by an application
 * Called when application is uninstalled
 */
void gcos_symbol_delete_app_global_refs(GCOSVM *vm, u8 app_id);

/**
 * @brief Find reusable slot in global reference table
 * Returns index of invalid entry, or -1 if none available
 */
int gcos_symbol_find_reusable_slot(GCOSVM *vm);
```

---

#### 工作流程

##### 1. 创建全局引用

```c
u16 gcos_symbol_create_global_ref_ex(GCOSVM *vm, u32 logical_address, 
                                      u8 module_id, u16 symbol_index,
                                      u8 owner_app_id) {
    // 1. 尝试重用已释放的槽位
    int index = gcos_symbol_find_reusable_slot(vm);
    
    if (index < 0) {
        // 没有可用槽位，分配新的
        if (global_ref_count >= global_ref_capacity) {
            expand_global_ref_table(vm);
        }
        index = global_ref_count++;
    }
    
    // 2. 初始化条目
    GCOSGlobalRefEntry *entry = get_entry_by_index(index);
    entry->logical_address = logical_address;
    entry->module_id = module_id;
    entry->symbol_index = symbol_index;
    entry->reference_count = 1;      // 初始引用计数 = 1
    entry->owner_app_id = owner_app_id;  // 记录所有者
    entry->is_valid = true;
    
    return make_global_addr(index);
}
```

---

##### 2. 解析导入符号时增加引用计数

```c
GCOSResult gcos_symbol_resolve_imports(GCOSVM *vm, u8 module_id) {
    for (int i = 0; i < import_count; i++) {
        // ... 查找导出符号 ...
        
        // 找到目标符号
        u16 target_compact_addr = find_export_symbol(...);
        
        if (is_global_ref(target_compact_addr)) {
            u16 global_ref_index = get_index(target_compact_addr);
            
            // ✅ 增加引用计数
            gcos_symbol_increment_ref_count(vm, global_ref_index);
        }
        
        // 更新导入符号表
        import_table[i].resolved_address = target_compact_addr;
        import_table[i].is_resolved = true;
    }
    
    return GCOS_SUCCESS;
}

void gcos_symbol_increment_ref_count(GCOSVM *vm, u16 global_ref_index) {
    GCOSGlobalRefEntry *entry = get_entry_by_index(global_ref_index);
    if (entry && entry->is_valid) {
        entry->reference_count++;
        GCOS_PRINTF("[Symbol Resolver] Global ref %u ref_count++ (%u)\n",
                   global_ref_index, entry->reference_count);
    }
}
```

---

##### 3. 卸载模块时减少引用计数

```c
GCOSResult gcos_module_unload(GCOSVM *vm, u8 module_id) {
    // 1. 遍历该模块的所有导入符号
    for (int i = 0; i < import_count; i++) {
        if (import_table[i].is_resolved) {
            u16 resolved_addr = import_table[i].resolved_address;
            
            if (is_global_ref(resolved_addr)) {
                u16 global_ref_index = get_index(resolved_addr);
                
                // ✅ 减少引用计数
                bool can_recycle = gcos_symbol_decrement_ref_count(vm, global_ref_index);
                
                if (can_recycle) {
                    GCOS_PRINTF("[Symbol Resolver] Global ref %u can be recycled\n",
                               global_ref_index);
                }
            }
            
            // 清除解析状态
            import_table[i].is_resolved = false;
            import_table[i].resolved_address = 0;
        }
    }
    
    // 2. 清除导出符号
    export_count = 0;
    
    return GCOS_SUCCESS;
}

bool gcos_symbol_decrement_ref_count(GCOSVM *vm, u16 global_ref_index) {
    GCOSGlobalRefEntry *entry = get_entry_by_index(global_ref_index);
    if (!entry || !entry->is_valid) {
        return false;
    }
    
    if (entry->reference_count > 0) {
        entry->reference_count--;
    }
    
    GCOS_PRINTF("[Symbol Resolver] Global ref %u ref_count-- (%u)\n",
               global_ref_index, entry->reference_count);
    
    // 引用计数降为0，可以回收
    return (entry->reference_count == 0);
}
```

---

##### 4. 回收条目

```c
void gcos_symbol_recycle_global_ref(GCOSVM *vm, u16 global_ref_index) {
    GCOSGlobalRefEntry *entry = get_entry_by_index(global_ref_index);
    if (!entry || !entry->is_valid) {
        return;
    }
    
    GCOS_PRINTF("[Symbol Resolver] Recycling global ref %u (mod=%u, sym=%u)\n",
               global_ref_index, entry->module_id, entry->symbol_index);
    
    // 标记为无效
    entry->is_valid = false;
    entry->reference_count = 0;
    
    // 可选：清零数据（安全考虑）
    entry->logical_address = 0;
    entry->module_id = 0;
    entry->symbol_index = 0;
}
```

---

##### 5. 应用删除时批量回收

```c
GCOSResult app_delete(GCOSVM *vm, u8 app_id) {
    if (vm == NULL || app_id >= MAX_APPS) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = &vm->apps[app_id];
    
    if (!app->installed) {
        return GCOS_ERROR_APP_NOT_FOUND;
    }
    
    // Cannot delete ISD
    if (app_id == APP_FIRST) {
        return GCOS_ERROR_CANNOT_DELETE_ISD;
    }
    
    // ✅ 1. 回收该应用拥有的所有全局引用
    gcos_symbol_delete_app_global_refs(vm, app_id);
    
    // 2. 卸载该应用的所有模块
    for (int i = 0; i < app->module_count; i++) {
        gcos_module_unload(vm, app->modules[i]);
    }
    
    // 3. 从 Flash 删除持久化数据
    for (int i = 0; i < app->module_count; i++) {
        gcos_persist_delete_module(vm, app->modules[i]);
    }
    
    // 4. Deselect if currently selected
    if (vm->selected_app == app) {
        app_deselect(vm, vm->current_channel);
    }
    
    // 5. Clear application slot
    memset(app, 0, sizeof(GCOSAppInstance));
    
    vm->app_count--;
    
    printf("[APP_DELETE] Application %u deleted (global refs recycled)\n", app_id);
    
    return GCOS_SUCCESS;
}

void gcos_symbol_delete_app_global_refs(GCOSVM *vm, u8 app_id) {
    int recycled_count = 0;
    
    for (u16 i = 0; i < global_ref_capacity; i++) {
        GCOSGlobalRefEntry *entry = get_entry_by_index(i);
        
        if (entry->is_valid && entry->owner_app_id == app_id) {
            // 强制回收（不管引用计数）
            gcos_symbol_recycle_global_ref(vm, i);
            recycled_count++;
        }
    }
    
    if (recycled_count > 0) {
        GCOS_PRINTF("[Symbol Resolver] Recycled %d global refs for app %u\n",
                   recycled_count, app_id);
        
        // 保存到 Flash
        gcos_symbol_save_global_ref_table_to_flash(vm);
    }
}
```

---

##### 6. 查找可重用槽位

```c
int gcos_symbol_find_reusable_slot(GCOSVM *vm) {
    // 优先查找无效的槽位
    for (u16 i = 0; i < global_ref_capacity; i++) {
        GCOSGlobalRefEntry *entry = get_entry_by_index(i);
        if (!entry->is_valid) {
            return i;  // 找到可重用的槽位
        }
    }
    
    return -1;  // 没有可用槽位
}
```

---

### 方案 B：基于所有者的批量回收（简化版）

如果不想引入引用计数，可以采用更简单的方案：**仅记录所有者，应用删除时批量回收**。

#### 数据结构改进

```c
typedef struct {
    u32 logical_address;        /* 32-bit logical address */
    u8 module_id;               /* Module that owns this symbol */
    u16 symbol_index;           /* Symbol index within module */
    u8 owner_app_id;            /* NEW: Application ID (0xFF = system) */
    bool is_valid;              /* Whether entry is valid */
} GCOSGlobalRefEntry;
```

#### 优缺点对比

| 特性 | 方案A（引用计数） | 方案B（批量回收） |
|------|------------------|------------------|
| 实现复杂度 | ⭐⭐⭐ 中等 | ⭐ 简单 |
| 内存开销 | +2 字节/条目 | +1 字节/条目 |
| 回收精度 | 精确（按需回收） | 粗糙（批量回收） |
| 适用场景 | 频繁安装/卸载 | 偶尔安装/卸载 |
| 安全性 | 高（防止悬空引用） | 中（可能误删） |

**推荐：** 对于智能卡环境，**方案A（引用计数）** 更安全，但**方案B（批量回收）** 更容易实现。可以先实施方案B，后续再升级到方案A。

---

## 🎯 推荐实施计划

### 阶段 1：基础回收机制（方案B）

1. **扩展数据结构**
   - 添加 `owner_app_id` 字段
   - 修改 `gcos_symbol_create_global_ref()` 接受 `owner_app_id` 参数

2. **实现批量回收**
   - 实现 `gcos_symbol_delete_app_global_refs()`
   - 在 `app_delete()` 中调用

3. **实现槽位重用**
   - 实现 `gcos_symbol_find_reusable_slot()`
   - 修改创建逻辑优先重用槽位

4. **测试验证**
   - 安装应用 → 创建全局引用
   - 卸载应用 → 回收全局引用
   - 重新安装 → 重用槽位

---

### 阶段 2：引用计数优化（方案A）

1. **添加引用计数**
   - 添加 `reference_count` 字段
   - 修改创建逻辑初始化计数为1

2. **实现增量/减量**
   - 实现 `gcos_symbol_increment_ref_count()`
   - 实现 `gcos_symbol_decrement_ref_count()`

3. **集成到解析流程**
   - 在 `gcos_symbol_resolve_imports()` 中增加计数
   - 在模块卸载时减少计数

4. **自动回收**
   - 当计数降为0时自动标记为可回收
   - 定期清理无效条目

---

### 阶段 3：持久化支持

1. **Flash 格式更新**
   - 更新序列化/反序列化逻辑
   - 包含新字段（owner_app_id, reference_count）

2. **迁移兼容**
   - 检测旧格式数据
   - 自动迁移到新格式

3. **崩溃恢复**
   - 确保回收操作的原子性
   - 使用事务保护

---

## 📊 性能分析

### 内存开销

| 方案 | 每条目大小 | 64条目总大小 | 256条目总大小 |
|------|-----------|-------------|--------------|
| 当前 | 12 字节 | 768 B | 3,072 B |
| 方案B | 13 字节 | 832 B (+64 B) | 3,328 B (+256 B) |
| 方案A | 14 字节 | 896 B (+128 B) | 3,584 B (+512 B) |

**评估：** 内存开销增加可接受（< 20%）

---

### 时间开销

| 操作 | 当前 | 方案B | 方案A |
|------|------|-------|-------|
| 创建引用 | ~1 μs | ~2 μs（查找槽位） | ~2 μs |
| 解析导入 | ~5 μs | ~5 μs | ~6 μs（+计数） |
| 卸载模块 | ~10 μs | ~10 μs | ~50 μs（-计数×N） |
| 删除应用 | ~1 ms | ~2 ms（批量回收） | ~2 ms |

**评估：** 时间开销增加可接受（< 10%）

---

## ⚠️ 注意事项

### 1. 跨应用共享符号

**问题：** 如果多个应用导入同一个系统模块的符号，它们会共享同一个全局引用条目。

**解决：**
- 系统模块的全局引用设置 `owner_app_id = 0xFF`（不回收）
- 或者使用引用计数，只有计数降为0时才回收

---

### 2. 悬空引用

**问题：** 如果错误地回收了仍在使用的条目，会导致悬空引用。

**解决：**
- 方案A：引用计数确保只有无人使用时才回收
- 方案B：仅在应用删除时回收，此时所有导入符号已清除
- 调试模式：启用引用检查，检测非法访问

---

### 3. 碎片化

**问题：** 频繁创建/回收可能导致全局引用表碎片化。

**解决：**
- 优先重用无效槽位（`find_reusable_slot()`）
- 定期压缩（可选，移动有效条目到前面）
- 监控碎片率，超过阈值时触发压缩

---

### 4. 持久化一致性

**问题：** 回收操作需要同时更新 RAM 和 Flash。

**解决：**
- 先更新 RAM
- 再写入 Flash（事务保护）
- 失败时回滚 RAM 状态

---

## 🧪 测试用例

### 测试 1：基本回收

```c
// 1. 安装应用 A
app_install(vm, app_a_aid, app_a_sef);
// → 创建全局引用 0, 1, 2（owner = app_a）

// 2. 卸载应用 A
app_delete(vm, app_a_id);
// → 回收全局引用 0, 1, 2（is_valid = false）

// 3. 验证
assert(global_ref_table[0].is_valid == false);
assert(global_ref_table[1].is_valid == false);
assert(global_ref_table[2].is_valid == false);
```

---

### 测试 2：槽位重用

```c
// 1. 安装应用 A
app_install(vm, app_a_aid, app_a_sef);
// → 使用槽位 0, 1, 2

// 2. 卸载应用 A
app_delete(vm, app_a_id);
// → 槽位 0, 1, 2 标记为无效

// 3. 安装应用 B
app_install(vm, app_b_aid, app_b_sef);
// → 重用槽位 0, 1, 2（而不是分配 3, 4, 5）

// 4. 验证
assert(global_ref_table[0].owner_app_id == app_b_id);
assert(global_ref_table[1].owner_app_id == app_b_id);
assert(global_ref_table[2].owner_app_id == app_b_id);
```

---

### 测试 3：引用计数（方案A）

```c
// 1. 应用 A 导入系统模块函数
app_install(vm, app_a_aid, app_a_sef);
// → 创建全局引用 0（ref_count = 1, owner = sys）

// 2. 应用 B 也导入同一个函数
app_install(vm, app_b_aid, app_b_sef);
// → 复用全局引用 0（ref_count = 2）

// 3. 卸载应用 A
app_delete(vm, app_a_id);
// → ref_count-- （ref_count = 1，不回收）

// 4. 验证全局引用 0 仍然有效
assert(global_ref_table[0].is_valid == true);
assert(global_ref_table[0].reference_count == 1);

// 5. 卸载应用 B
app_delete(vm, app_b_id);
// → ref_count-- （ref_count = 0，可回收）

// 6. 验证全局引用 0 已回收
assert(global_ref_table[0].is_valid == false);
```

---

## 📝 总结

### 当前问题

❌ **没有回收机制**：全局引用条目只增不减  
❌ **内存泄漏风险**：频繁安装/卸载应用会耗尽全局引用表  
❌ **无法追踪归属**：不知道哪个条目属于哪个应用  

---

### 推荐方案

✅ **方案 A（引用计数）** - 更安全、更精确  
✅ **方案 B（批量回收）** - 更简单、更快实现  

**建议：** 先实施方案B快速解决问题，后续再升级到方案A提供更精细的控制。

---

### 实施优先级

1. **高优先级**：添加 `owner_app_id` 字段，实现批量回收
2. **中优先级**：实现槽位重用，避免浪费
3. **低优先级**：添加引用计数，实现精确回收

---

**文档版本：** 1.0.0  
**最后更新：** 2026-05-12  
**作者：** GCOS VM Development Team
