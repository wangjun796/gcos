# GCOS 系统对象架构修正 - 移除 Free List 对象

## 🎯 修正原因

用户指出：**Obj 4 (Free List) 不需要**，因为 eflash 已经通过 `eflash_mgr` 在 LPN 8-11 管理了空闲链表。

## ✅ 修正内容

### 1. 对象 ID 重新分配

**修正前：**
```c
#define GCOS_OBJ_ID_MODULE_REGISTRY     1
#define GCOS_OBJ_ID_APP_INSTANCE        2
#define GCOS_OBJ_ID_GRT                 3
#define GCOS_OBJ_ID_FREE_LIST           4   /* ❌ 删除 */
#define GCOS_OBJ_ID_SYS_CONFIG          5
#define GCOS_OBJ_ID_SYMBOL_TABLE        6
```

**修正后：**
```c
#define GCOS_OBJ_ID_MODULE_REGISTRY     1
#define GCOS_OBJ_ID_APP_INSTANCE        2
#define GCOS_OBJ_ID_GRT                 3
/* Obj ID 4 is NOT used - eflash already manages free list at LPN 8-11 */
#define GCOS_OBJ_ID_SYS_CONFIG          5
#define GCOS_OBJ_ID_SYMBOL_TABLE        6
```

### 2. 删除的结构定义

- ❌ `GCOS_FreeListObject` - 不再需要
- ❌ `GCOSFreeNode` - 不再需要
- ❌ `GCOS_MAGIC_FREL` - 不再使用
- ❌ `GCOS_CLASS_FREE_LIST` - 不再使用

### 3. 删除的函数

- ❌ `gcos_create_free_list_object()` - 不再实现
- ❌ `g_free_list_obj` 静态变量 - 不再需要

### 4. API 调用修正

**修正前（错误）：**
```c
eflash_ftl_write(logic_addr, data, size);  // ❌ 错误的 API
```

**修正后（正确）：**
```c
eflash_ftl_write_logical(logic_addr, data, (int16_t)size);  // ✅ 正确的 API
eflash_ftl_read_logical(logic_addr, buffer, (int16_t)size); // ✅ 正确的 API
```

### 5. 错误码统一

所有错误返回统一使用 `GCOS_ERROR_INVALID_PARAM`，因为以下错误码未定义：
- ❌ `GCOS_ERROR_OBJECT_NOT_FOUND`
- ❌ `GCOS_ERROR_BUFFER_TOO_SMALL`
- ❌ `GCOS_ERROR_FLASH_READ`
- ❌ `GCOS_ERROR_FLASH_WRITE`

---

## 📊 最终对象 ID 分配

```
eflash Object Header Table (LPN 0-7):
┌─────────────────────────────────────┐
│ Obj 0: Reserved (eflash internal)   │
│ Obj 1: Module Registry              │ ← GCOS
│ Obj 2: App Instance Table           │ ← GCOS
│ Obj 3: GRT                          │ ← GCOS
│ Obj 4: [RESERVED - NOT USED]        │ ← eflash manages free list
│ Obj 5: System Config ⭐             │ ← GCOS 根锚点
│ Obj 6: Symbol Table                 │ ← GCOS
│ Obj 7+: Dynamic allocation          │ ← Apps/Modules
└─────────────────────────────────────┘

eflash Free List (LPN 8-11):
┌─────────────────────────────────────┐
│ Managed by eflash_mgr               │
│ GCOS uses:                          │
│   - eflash_mgr_alloc()              │
│   - eflash_mgr_free()               │
└─────────────────────────────────────┘
```

---

## 🔧 空间管理方式

### GCOS 如何使用 Flash 空间

**之前（错误的设计）：**
```c
// GCOS 维护自己的 Free List 对象
GCOS_FreeListObject *free_list = ...;
// 从 free_list 中分配空间
```

**现在（正确的方式）：**
```c
// 直接使用 eflash_mgr API
uint32_t logic_addr;
eflash_mgr_alloc(size, &logic_addr);  // 分配空间

// 使用后释放
eflash_mgr_free(logic_addr, size);    // 回收空间
```

**优势：**
1. ✅ 无需重复实现空闲链表管理
2. ✅ 利用 eflash 的成熟算法（首次适配、块合并等）
3. ✅ 自动磨损均衡（FTL Radix Tree）
4. ✅ 减少 GCOS 代码复杂度

---

## 📝 修改的文件

| 文件 | 修改内容 |
|------|----------|
| [gcos_system_objects.h](file://e:/views/gcos/prog/cos/gcos_vm/include/gcos_system_objects.h) | 删除 Free List 相关定义，添加注释说明 Obj 4 保留但不使用 |
| [gcos_system_objects.c](file://e:/views/gcos/prog/cos/gcos_vm/src/gcos_system_objects.c) | 删除 Free List 创建函数，修正 eflash API 调用 |

---

## 💡 关键教训

### 教训 1：不要重复造轮子

**错误：** 为 GCOS 创建独立的 Free List 对象  
**正确：** 直接使用 eflash 已有的空闲管理机制

### 教训 2：理解底层库的 API

**错误：** 使用 `eflash_ftl_write(logic_addr, data, size)`  
**正确：** 使用 `eflash_ftl_write_logical(logic_addr, data, size)`

**区别：**
- `eflash_ftl_write(sector_id, data)` - 按页写入（sector_id 是页号）
- `eflash_ftl_write_logical(logical_addr, data, size)` - 按逻辑地址写入（支持跨页）

### 教训 3：统一的错误处理

使用统一的错误码 `GCOS_ERROR_INVALID_PARAM` 简化错误处理，避免定义过多的细分错误码。

---

## 🚀 下一步

继续实施阶段 1-4（如之前计划）：
1. 完善对象管理 API
2. 集成到 VM 生命周期
3. 模块加载与应用安装集成
4. 测试与验证

---

**修正日期：** 2026-05-09  
**状态：** ✅ 已完成修正
