# GCOS 模块与应用架构优化设计 - 参考 Cref Package/Applet 模型

## 📋 问题分析

### 当前 GCOS 架构的问题

1. **❌ 模块与应用一对一绑定**
   - 当前每个 `GCOSAppInstance` 直接关联一个 `module_index`
   - 无法实现一个模块代码被多个应用实例共享

2. **❌ GRT 回收未集成到删除流程**
   - `app_delete()` 仅清空应用槽位
   - 没有调用 `gcos_symbol_delete_module_global_refs()`
   - 导致全局引用表内存泄漏

3. **❌ 缺少 Package 概念**
   - cref 有清晰的 Package（代码）和 Applet（实例）分离
   - GCOS 目前只有 Module，没有区分"代码"和"实例"

---

## 🎯 Cref Package/Applet 模型分析

### Cref 的核心设计理念

```
┌─────────────────────────────────────────┐
│         Package (代码层)                 │
│  - 存储在 ROM/Flash                     │
│  - 包含字节码、常量池、静态字段           │
│  - 只加载一次，多个 Applet 共享          │
│  - Package ID: 8 bits                   │
└─────────────────────────────────────────┘
              ↓ 实例化多次
┌──────────┬──────────┬──────────┐
│ Applet 1 │ Applet 2 │ Applet 3 │
│ (实例)   │ (实例)   │ (实例)   │
│ - 独立状态            │
│ - 独立堆对象          │
│ - 独立生命周期        │
└──────────┴──────────┴──────────┘
```

### Cref 的关键数据结构

#### 1. Package Table (`f_pkgTable`)

```c
// cref/common/data.h
extern w_memref f_pkgTable;        // Package 表
extern u8 f_pkgTableSize;          // Package 数量
```

**Package Entry 结构：**
- Package AID
- Package ID (8 bits)
- Major/Minor version
- Component locations (Code, Static Fields, etc.)
- Applet count (该 Package 有多少个 Applet 实例)
- Applet array (指向 AppletInfo 数组)

#### 2. Applet Table (`theAppTable`)

```c
// cref/common/data.h
extern w_memref theAppTable;       // Applet 表
extern u8 theAppTableSize;         // Applet 数量
```

**Applet Entry 结构：**
- Applet AID
- Package context (指向所属 Package)
- Applet object reference (实例对象)
- Lifecycle state (INSTALLED/SELECTABLE/LOCKED/etc.)
- Privileges

#### 3. AppletInfo

```c
// cref/common/objAccess.c
jref newAppletInfo(jref aid, u16 methodAddr) {
    // 创建 AppletInfo 对象
    // - theClassAID: Applet 类的 AID
    // - installMethodAddr: install() 方法地址
}
```

### Cref 的工作流程

#### 安装 Package（代码加载）

```
INSTALL FOR LOAD
  ↓
解析 CAP 文件
  ↓
创建 Package Entry (f_pkgTable)
  ↓
存储字节码到 ROM
  ↓
初始化静态字段
  ↓
Package 状态: LOADED
```

#### 创建 Applet 实例

```
INSTALL FOR INSTALL
  ↓
查找 Package (by AID)
  ↓
从 Package 获取 install() 方法地址
  ↓
创建 Applet Entry (theAppTable)
  ↓
分配实例对象 (heap)
  ↓
调用 install(byte[], byte, byte)
  ↓
Applet 状态: INSTALLED → SELECTABLE
```

#### 选择 Applet

```
SELECT (AID)
  ↓
查找 theAppTable (by AID)
  ↓
检查状态是否为 SELECTABLE
  ↓
设置 selected_app = applet_entry
  ↓
调用 select() 回调
  ↓
返回 0x9000
```

#### 删除 Applet

```
DELETE (AID)
  ↓
查找 theAppTable (by AID)
  ↓
检查是否有其他对象引用
  ↓
标记为 TERMINATED
  ↓
清空 theAppTable entry
  ↓
触发 GC（如果无引用）
  ↓
⚠️ 注意：Package 不会被删除！
```

#### 删除 Package

```
DELETE (Package AID)
  ↓
检查是否有 Applet 实例存在
  ↓
如果有，拒绝删除 ❌
  ↓
如果没有：
  - 删除 Package Entry
  - 释放 ROM 空间（如果支持）
  - 清理 GRT 引用
```

---

## 🔧 GCOS 优化方案

### 方案设计原则

1. **保持向后兼容**：不破坏现有 API
2. **最小改动**：在现有结构上扩展
3. **清晰分离**：Module（代码）vs App（实例）
4. **GRT 自动回收**：模块删除时自动清理

### 核心改进点

#### 改进 1: 引入 Module Registry（模块注册表）

**目标**：将模块代码与实例解耦

```c
/**
 * @brief Module registry entry (code layer, shared by multiple apps)
 * 
 * Similar to cref's Package Entry.
 * Stores module code and metadata, loaded once from Flash.
 */
typedef struct {
    u8 module_id;                 /* Module ID (0xFF = invalid) */
    bool is_loaded;               /* Whether module is loaded */
    
    /* Module identity */
    GCOSAID module_aid;           /* Module AID */
    u32 module_version;           /* Module version */
    
    /* Code location (XIP or loaded to RAM) */
    const u8 *code_base;          /* Code section base address */
    u32 code_size;                /* Code size in bytes */
    
    /* Function table */
    u16 function_count;           /* Number of functions */
    const GCOSFunctionHeader *functions; /* Function headers */
    
    /* Export table */
    u16 export_count;             /* Number of exported symbols */
    GCOSExportSymbol *exports;    /* Export symbol table */
    
    /* Import dependencies */
    u8 import_count;              /* Number of imports */
    GCOSImportInfo imports[MAX_IMPORTS]; /* Import dependencies */
    
    /* Instance tracking */
    u8 instance_count;            /* Number of app instances using this module */
    u8 instance_ids[MAX_APPS_PER_MODULE]; /* Array of app IDs using this module */
    
    /* Global data template (copied to each instance) */
    const u8 *global_data_template; /* Read-only template */
    u32 global_data_size;         /* Template size */
    
    /* Module state */
    GCOSModuleState state;        /* LOADED/VERIFIED/ERROR */
} GCOSModuleRegistry;
```

**优势：**
- ✅ 模块代码只加载一次
- ✅ 多个应用实例共享同一份代码
- ✅ 跟踪哪些实例在使用该模块
- ✅ 删除模块前检查是否有活跃实例

#### 改进 2: 修改 GCOSAppInstance 结构

**目标**：应用实例指向模块注册表，而非直接包含代码

```c
struct GCOSAppInstance {
    /* === 基本信息 === */
    GCOSAID app_aid;                /* 应用AID */
    u8 app_id;                      /* 应用 ID (0 = ISD) */
    
    /* ⭐ 改进：指向模块注册表，而非 module_index */
    u8 module_id;                   /* Module ID (references GCOSModuleRegistry) */
    GCOSModuleRegistry *module;     /* Pointer to module registry entry */
    
    GCOSAppLifecycleState lifecycle;/* 生命周期状态 */
    
    /* === 类型、权限和安全域 === */
    GCOSAppType app_type;           /* 应用类型 */
    u8 security_domain_id;          /* 所属安全域 ID */
    u8 privilege_byte1;             /* 权限字节 1 */
    u8 privilege_byte2;             /* 权限字节 2 */
    u8 privilege_byte3;             /* 权限字节 3 */
    u8 install_param;               /* 安装参数 */
    
    /* === APDU 处理方法 === */
    GCOSResult (*on_install)(struct GCOSAppInstance *app, ...);
    u16 (*process)(struct GCOSAppInstance *app, ...);
    GCOSResult (*on_select)(struct GCOSAppInstance *app);
    void (*on_deselect)(struct GCOSAppInstance *app);
    
    /* === 应用数据 (每个实例独立) === */
    u8 *app_domain_data;            /* 应用域数据 */
    u32 app_domain_data_size;
    
    u8 *ref_domain_data;            /* 引用域数据 */
    u32 ref_domain_data_size;
    
    u8 *persistent_data;            /* 持久性数据 */
    u32 persistent_data_size;
    
    /* === 运行时数据 (每个通道独立) === */
    struct {
        u8 *temp_dynamic_data;
        u32 temp_dynamic_data_size;
        
        /* ⭐ 改进：每个实例有独立的全局数据副本 */
        u8 *global_data_copy;       /* Copy from module's global_data_template */
        u32 global_data_copy_size;
        
        bool active;
        bool selected;
    } channel_data[MAX_CHANNELS];
    
    bool installed;                 /* 是否已安装 */
};
```

**关键变化：**
- ❌ 移除：`u16 module_index`
- ✅ 新增：`u8 module_id` + `GCOSModuleRegistry *module`
- ✅ 新增：每个实例独立的 `global_data_copy`

#### 改进 3: VM 结构添加模块注册表

```c
struct GCOSVM {
    /* === 现有字段 === */
    GCOSState state;
    GCOSExecutor executor;
    GCOSMemoryManager memory;
    
    /* === 应用管理 === */
    GCOSAppInstance apps[MAX_APPS];
    u8 app_count;
    GCOSAppInstance *selected_app;
    
    /* ⭐ 新增：模块注册表 */
    GCOSModuleRegistry modules[MAX_MODULES];
    u8 module_count;
    
    /* === 其他现有字段 === */
    GCOSChannel channels[MAX_CHANNELS];
    u8 current_channel;
    GCOSLoadContext load_context;
    // ...
};
```

#### 改进 4: GRT 回收集成到模块删除流程

**方案 A：应用删除时回收（推荐）**

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
    
    // Deselect if currently selected
    if (vm->selected_app == app) {
        app_deselect(vm, vm->current_channel);
    }
    
    /* ⭐ 步骤 1: 回收该应用的全局引用 */
    if (app->module != NULL && app->module->is_loaded) {
        gcos_symbol_delete_module_global_refs(vm, app->module_id);
    }
    
    /* ⭐ 步骤 2: 从模块注册表中移除实例引用 */
    if (app->module != NULL) {
        GCOSModuleRegistry *mod = app->module;
        
        // Remove app_id from instance_ids array
        for (u8 i = 0; i < mod->instance_count; i++) {
            if (mod->instance_ids[i] == app_id) {
                // Shift remaining entries
                for (u8 j = i; j < mod->instance_count - 1; j++) {
                    mod->instance_ids[j] = mod->instance_ids[j + 1];
                }
                mod->instance_count--;
                break;
            }
        }
        
        // If no more instances, mark module as unloadable
        if (mod->instance_count == 0) {
            GCOS_PRINTF("[Module] Module %u has no more instances\n", app->module_id);
            // Optional: Unload module code here if needed
        }
    }
    
    /* ⭐ 步骤 3: 释放应用实例资源 */
    // Free app_domain_data, ref_domain_data, persistent_data, etc.
    free_app_resources(app);
    
    /* ⭐ 步骤 4: 清空应用槽位 */
    memset(app, 0, sizeof(GCOSAppInstance));
    
    vm->app_count--;
    
    printf("[APP_DELETE] Application %u deleted successfully\n", app_id);
    
    return GCOS_SUCCESS;
}
```

**方案 B：模块卸载时回收（额外保护）**

```c
GCOSResult module_unload(GCOSVM *vm, u8 module_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSModuleRegistry *mod = &vm->modules[module_id];
    
    if (!mod->is_loaded) {
        return GCOS_ERROR_MODULE_NOT_FOUND;
    }
    
    // Check if any instances are still using this module
    if (mod->instance_count > 0) {
        GCOS_PRINTF("[Module] ERROR: Cannot unload module %u, %u instances still active\n",
                   module_id, mod->instance_count);
        return GCOS_ERROR_MODULE_IN_USE;
    }
    
    /* ⭐ 回收该模块的所有全局引用 */
    gcos_symbol_delete_module_global_refs(vm, module_id);
    
    /* Clear module registry entry */
    mod->is_loaded = false;
    mod->state = MODULE_LOADED;
    mod->code_base = NULL;
    mod->function_count = 0;
    mod->export_count = 0;
    mod->import_count = 0;
    
    vm->module_count--;
    
    printf("[Module] Module %u unloaded successfully\n", module_id);
    
    return GCOS_SUCCESS;
}
```

---

## 📊 工作流程对比

### 场景 1：安装模块并创建两个应用实例

#### Cref 方式

```
1. INSTALL FOR LOAD (Package AID=A000000001)
   → 创建 Package Entry (pkg_id=5)
   → 加载字节码到 ROM
   → Package.applet_count = 0

2. INSTALL FOR INSTALL (Applet AID=A000000002, Package AID=A000000001)
   → 查找 Package (pkg_id=5)
   → 创建 Applet Entry #1 (theAppTable[1])
   → Applet.package_context = 5
   → 调用 install()
   → Package.applet_count = 1

3. INSTALL FOR INSTALL (Applet AID=A000000003, Package AID=A000000001)
   → 查找 Package (pkg_id=5) ← 复用同一个 Package!
   → 创建 Applet Entry #2 (theAppTable[2])
   → Applet.package_context = 5
   → 调用 install()
   → Package.applet_count = 2

结果：
- 1 个 Package（代码）
- 2 个 Applet 实例（独立状态）
- 共享同一份字节码
```

#### GCOS 优化后

```
1. LOAD BLOCKS (Module AID=A000000001)
   → 创建 Module Registry Entry (module_id=5)
   → 加载字节码到 Flash/RAM
   → Module.instance_count = 0

2. INSTALL (App AID=A000000002, Module AID=A000000001)
   → 查找 Module (module_id=5)
   → 创建 App Instance #1 (apps[1])
   → App.module_id = 5
   → App.module = &vm->modules[5]
   → 分配 global_data_copy
   → 调用 on_install()
   → Module.instance_count = 1
   → Module.instance_ids[0] = 1

3. INSTALL (App AID=A000000003, Module AID=A000000001)
   → 查找 Module (module_id=5) ← 复用同一个 Module!
   → 创建 App Instance #2 (apps[2])
   → App.module_id = 5
   → App.module = &vm->modules[5]
   → 分配 global_data_copy
   → 调用 on_install()
   → Module.instance_count = 2
   → Module.instance_ids[1] = 2

结果：
- 1 个 Module（代码）
- 2 个 App 实例（独立状态）
- 共享同一份字节码
```

### 场景 2：删除一个应用实例

#### Cref 方式

```
DELETE (Applet AID=A000000002)
  → 查找 theAppTable[1]
  → 标记为 TERMINATED
  → 清空 theAppTable[1]
  → Package.applet_count = 1 (仍然有 Applet #2 在使用)
  → Package 不会被删除 ✅
```

#### GCOS 优化后

```
app_delete(vm, app_id=1)
  → 回收 App #1 的全局引用 ✅
  → 从 Module.instance_ids 移除 app_id=1
  → Module.instance_count = 1 (仍然有 App #2 在使用)
  → Module 不会被卸载 ✅
  → 清空 apps[1]
```

### 场景 3：删除所有实例后卸载模块

#### Cref 方式

```
DELETE (Applet AID=A000000003)
  → 清空 theAppTable[2]
  → Package.applet_count = 0

DELETE (Package AID=A000000001)
  → 检查 Package.applet_count == 0 ✅
  → 删除 Package Entry
  → 释放 ROM 空间
  → 清理 GRT 引用
```

#### GCOS 优化后

```
app_delete(vm, app_id=2)
  → 回收 App #2 的全局引用 ✅
  → Module.instance_count = 0

module_unload(vm, module_id=5)
  → 检查 Module.instance_count == 0 ✅
  → 回收 Module 的全局引用 ✅
  → 清除 Module Registry Entry
  → 释放代码空间（如果需要）
```

---

## 🎯 实施计划

### 阶段 1：数据结构重构（2天）

1. **定义 GCOSModuleRegistry 结构**
   - 文件：`include/gcos_vm.h`
   - 内容：模块注册表结构定义

2. **修改 GCOSAppInstance 结构**
   - 文件：`include/gcos_vm.h`
   - 变更：`module_index` → `module_id` + `module` 指针

3. **修改 GCOSVM 结构**
   - 文件：`include/gcos_vm.h`
   - 新增：`modules[MAX_MODULES]` 数组

### 阶段 2：模块加载逻辑改造（2天）

1. **修改 LOAD 命令处理**
   - 文件：`src/gcos_loader.c`
   - 变更：创建 Module Registry Entry 而非直接关联到 App

2. **实现模块注册表管理函数**
   - 文件：`src/gcos_module_registry.c`（新文件）
   - 函数：
     - `module_registry_init()`
     - `module_registry_register()`
     - `module_registry_find_by_aid()`
     - `module_registry_add_instance()`
     - `module_registry_remove_instance()`

### 阶段 3：应用安装逻辑改造（1天）

1. **修改 INSTALL 命令处理**
   - 文件：`src/gcos_app_manager.c`
   - 变更：从模块注册表查找模块，创建应用实例

2. **实现应用实例初始化**
   - 复制模块的全局数据模板
   - 设置 module 指针

### 阶段 4：GRT 回收集成（1天）

1. **修改 app_delete()**
   - 文件：`src/gcos_app_manager.c`
   - 集成 GRT 回收调用
   - 从模块注册表移除实例引用

2. **实现 module_unload()**
   - 文件：`src/gcos_module_registry.c`
   - 检查实例计数
   - 回收 GRT
   - 清理模块注册表

### 阶段 5：测试验证（1天）

1. **单元测试**
   - 测试模块加载
   - 测试多实例创建
   - 测试实例删除
   - 测试 GRT 回收

2. **集成测试**
   - 完整安装/卸载流程
   - 并发实例测试
   - 内存泄漏检测

---

## 📈 预期收益

### 内存效率

| 项目 | 优化前 | 优化后 | 节省 |
|------|--------|--------|------|
| **模块代码重复** | N 份 | **1 份** | **(N-1) × code_size** |
| **全局引用表** | 永久占用 | **可回收** | **~100%** |
| **64 条目 GRT** | 768 B | **256 B** | **512 B** |

### 功能增强

- ✅ **多实例支持**：一个模块代码可创建多个应用实例
- ✅ **自动 GRT 回收**：删除应用时自动清理全局引用
- ✅ **模块卸载**：无实例时可卸载模块释放空间
- ✅ **Cref 兼容性**：架构与 cref 保持一致

### 性能提升

- ✅ **加载速度**：模块代码只加载一次
- ✅ **内存占用**：减少代码重复存储
- ✅ **Flash 寿命**：减少不必要的写入

---

## 🔍 关键技术细节

### 1. 模块代码共享机制

**问题**：多个实例如何共享同一份代码？

**解决方案**：
```c
// 模块代码存储在 XIP Flash 或固定 RAM 区域
GCOSModuleRegistry *mod = &vm->modules[module_id];
const u8 *code = mod->code_base;  // 所有实例共享

// 执行时，PC 指向共享代码
vm->executor.pc = code + function_offset;
```

### 2. 全局数据隔离机制

**问题**：多个实例如何拥有独立的全局变量？

**解决方案**：
```c
// 模块提供全局数据模板（只读）
const u8 *template = mod->global_data_template;

// 每个实例创建独立副本
app->global_data_copy = malloc(mod->global_data_size);
memcpy(app->global_data_copy, template, mod->global_data_size);

// 执行时，BP 指向实例的副本
vm->executor.bp = app->channel_data[channel].global_data_copy;
```

### 3. GRT 地址解析

**问题**：GRT 中的 module_id 如何映射到正确的代码？

**解决方案**：
```c
// GRT entry 存储 module_id（8 bits）
u8 module_id = GRT_GET_MODULE_ID(entry);

// 通过 module_id 查找模块注册表
GCOSModuleRegistry *mod = &vm->modules[module_id];

// 获取代码基址
const u8 *code_base = mod->code_base;

// 计算实际地址
u32 logical_address = GRT_GET_ADDRESS(entry);
const u8 *actual_addr = code_base + logical_address;
```

### 4. 实例计数管理

**问题**：如何跟踪模块有多少个活跃实例？

**解决方案**：
```c
// 模块注册表维护实例列表
typedef struct {
    u8 instance_count;
    u8 instance_ids[MAX_APPS_PER_MODULE];
} GCOSModuleRegistry;

// 创建实例时添加
mod->instance_ids[mod->instance_count++] = app_id;

// 删除实例时移除
for (u8 i = 0; i < mod->instance_count; i++) {
    if (mod->instance_ids[i] == app_id) {
        // Shift and decrement
        mod->instance_count--;
        break;
    }
}

// 卸载模块前检查
if (mod->instance_count > 0) {
    return GCOS_ERROR_MODULE_IN_USE;
}
```

---

## ✅ 总结

本次优化将 GCOS 的模块/应用架构对齐到 cref 的 Package/Applet 模型，实现了：

1. **✅ 代码共享**：一个模块代码可被多个应用实例共享
2. **✅ GRT 回收**：删除应用时自动回收全局引用
3. **✅ 模块卸载**：无实例时可卸载模块释放资源
4. **✅ 架构清晰**：明确区分"代码层"和"实例层"

**下一步行动**：开始实施阶段 1（数据结构重构）。
