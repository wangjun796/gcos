# GCOS 模块与应用架构优化 - 阶段1完成报告

## ✅ 实施状态

**阶段 1: 数据结构重构** - **已完成！**

---

## 🎯 核心改进

### 1. 新增 GCOSModuleRegistry 结构

**文件**: `include/gcos_vm.h` (lines 405-458)

```c
/**
 * @brief Module registry entry (code layer, shared by multiple apps)
 * 
 * Similar to cref's Package Entry.
 * Stores module code and metadata, loaded once from Flash.
 * Multiple app instances can share the same module code.
 */
typedef struct GCOSModuleRegistry {
    u8 module_id;                   /* Module ID (0xFF = invalid) */
    bool is_loaded;                 /* Whether module is loaded */
    
    /* Module identity */
    GCOSAID module_aid;             /* Module AID */
    u32 module_version;             /* Module version */
    
    /* Code location (XIP or loaded to RAM) */
    const u8 *code_base;            /* Code section base address */
    u32 code_size;                  /* Code size in bytes */
    
    /* Function table */
    u16 function_count;             /* Number of functions */
    const void *functions;          /* Function headers */
    
    /* Export table */
    u16 export_count;               /* Number of exported symbols */
    void *exports;                  /* Export symbol table */
    
    /* Import dependencies */
    u8 import_count;                /* Number of imports */
    GCOSImportInfo imports[MAX_IMPORTS]; /* Import dependencies */
    
    /* ⭐ Instance tracking */
    u8 instance_count;              /* Number of app instances using this module */
    u8 instance_ids[MAX_APPS_PER_MODULE]; /* Array of app IDs using this module */
    
    /* Global data template (copied to each instance) */
    const u8 *global_data_template; /* Read-only template */
    u32 global_data_size;           /* Template size */
    
    /* Module state */
    GCOSModuleState state;          /* LOADED/VERIFIED/ERROR */
} GCOSModuleRegistry;
```

**关键特性：**
- ✅ **代码共享**：多个应用实例可以共享同一份模块代码
- ✅ **实例跟踪**：记录哪些应用实例在使用该模块
- ✅ **全局数据模板**：每个实例有独立的全局数据副本
- ✅ **Cref 兼容**：设计对齐 cref 的 Package Entry

---

### 2. 修改 GCOSAppInstance 结构

**文件**: `include/gcos_vm.h` (lines 809-817)

**修改前：**
```c
struct GCOSAppInstance {
    GCOSAID app_aid;
    u8 app_id;
    u16 module_index;               // ❌ 直接索引，不支持共享
    GCOSAppLifecycleState lifecycle;
    // ...
};
```

**修改后：**
```c
struct GCOSAppInstance {
    GCOSAID app_aid;
    u8 app_id;
    
    /* ⭐ 改进：指向模块注册表，支持多实例共享代码 */
    u8 module_id;                       // Module ID (references GCOSModuleRegistry)
    struct GCOSModuleRegistry *module;  // Pointer to module registry entry
    
    GCOSAppLifecycleState lifecycle;
    // ...
};
```

**优势：**
- ✅ **解耦**：应用实例不再直接绑定到模块索引
- ✅ **灵活**：可以通过指针访问模块注册表
- ✅ **共享**：多个实例可以指向同一个模块

---

### 3. VM 结构添加模块注册表数组

**文件**: `include/gcos_vm.h` (lines 1016-1023)

```c
struct GCOSVM {
    // ... existing fields ...
    
    /* 模块管理 */
    GCOSModule modules[MAX_MODULES];        /* 模块数组 (legacy, kept for compatibility) */
    u8 module_count;                        /* 已加载模块数 */
    u8 current_module_index;                /* 当前模块索引 */
    
    /* ⭐ NEW: Module registry (code layer, shared by multiple apps) */
    GCOSModuleRegistry module_registry[MAX_MODULES]; /* Module registry entries */
    u8 registry_count;                      /* Number of registered modules */
    
    // ... rest of fields ...
};
```

**设计考虑：**
- 保留旧的 `modules[]` 数组以保持向后兼容
- 新增 `module_registry[]` 数组用于新的共享机制
- 逐步迁移，不破坏现有功能

---

### 4. 调整类型定义顺序

**文件**: `include/gcos_vm.h` (lines 405-458)

**问题**: `GCOSModuleRegistry` 使用了 `GCOSImportInfo`，但后者定义在后

**解决**: 将 `GCOSImportInfo` 移到 `GCOSModuleRegistry` 之前

```c
// 1. GCOSModuleState enum
// 2. GCOSImportInfo struct ← 移到这里
// 3. GCOSModuleRegistry struct ← 现在可以使用 GCOSImportInfo
```

---

### 5. 修复应用注册函数

**文件**: `src/gcos_app_manager.c` (lines 327-335)

**修改前：**
```c
app->module_index = module_index;
```

**修改后：**
```c
/* ⭐ 改进：使用 module_id 和 module 指针 */
app->module_id = (u8)module_index;  // Cast u16 to u8 for module_id
app->module = NULL;  // Will be set when module is loaded
```

---

## 📊 编译结果

```bash
$ cmake --build build

Build completed successfully! ✅
No errors, no warnings.
```

---

## 🔍 架构对比

### 优化前（一对一）

```
┌──────────┐     ┌──────────┐
│ App #1   │────▶│ Module 0 │ (代码 + 数据)
└──────────┘     └──────────┘

┌──────────┐     ┌──────────┐
│ App #2   │────▶│ Module 1 │ (代码重复!)
└──────────┘     └──────────┘

❌ 问题：
- 代码重复存储
- 无法共享
- 内存浪费
```

### 优化后（多对一）

```
                  ┌──────────────────┐
                  │ Module Registry  │
                  │ (Code Layer)     │
                  │ - code_base      │
                  │ - functions      │
                  │ - exports        │
                  │ - instance_count │
                  └──────────────────┘
                         ▲
                         │ 共享
            ┌────────────┼────────────┐
            │            │            │
     ┌──────┴──────┐ ┌───┴──────┐ ┌──┴────────┐
     │ App Instance│ │ App Inst │ │ App Inst  │
     │ #1          │ │ #2       │ │ #3        │
     │ - global    │ │ - global │ │ - global  │
     │   data copy │ │   data   │ │   data    │
     └─────────────┘ └──────────┘ └───────────┘

✅ 优势：
- 代码只存储一次
- 多个实例共享
- 节省内存
- 易于管理
```

---

## 🚀 下一步计划

### 阶段 2: 模块加载逻辑改造（预计 2 天）

1. **创建模块注册表管理模块**
   - 新文件：`src/gcos_module_registry.c`
   - 新头文件：`include/gcos_module_registry.h`
   
2. **实现核心函数**
   ```c
   GCOSResult module_registry_init(GCOSVM *vm);
   GCOSResult module_registry_register(GCOSVM *vm, const u8 *sef_data, u32 sef_size, u8 *module_id);
   GCOSModuleRegistry* module_registry_find_by_aid(GCOSVM *vm, const GCOSAID *aid);
   GCOSResult module_registry_add_instance(GCOSVM *vm, u8 module_id, u8 app_id);
   GCOSResult module_registry_remove_instance(GCOSVM *vm, u8 module_id, u8 app_id);
   GCOSResult module_registry_unload(GCOSVM *vm, u8 module_id);
   ```

3. **修改 LOAD 命令处理**
   - 文件：`src/gcos_loader.c`
   - 变更：创建 Module Registry Entry 而非直接关联到 App

### 阶段 3: 应用安装逻辑改造（预计 1 天）

1. **修改 INSTALL 命令处理**
   - 文件：`src/gcos_app_manager.c`
   - 从模块注册表查找模块
   - 设置 `app->module` 指针
   - 复制全局数据模板

2. **实现应用实例初始化**
   ```c
   // 分配全局数据副本
   app->global_data_copy = malloc(module->global_data_size);
   memcpy(app->global_data_copy, module->global_data_template, module->global_data_size);
   ```

### 阶段 4: GRT 回收集成（预计 1 天）

1. **修改 app_delete()**
   - 回收该应用的全局引用
   - 从模块注册表移除实例引用
   - 释放应用资源

2. **实现 module_unload()**
   - 检查实例计数
   - 回收 GRT
   - 清理模块注册表

### 阶段 5: 测试验证（预计 1 天）

1. **单元测试**
   - 模块加载/卸载
   - 多实例创建
   - 实例删除
   - GRT 回收

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
| **示例：3个实例** | 3 × 16KB | **16KB** | **32KB** |

### 功能增强

- ✅ **多实例支持**：一个模块代码可创建多个应用实例
- ✅ **自动 GRT 回收**：删除应用时自动清理全局引用
- ✅ **模块卸载**：无实例时可卸载模块释放空间
- ✅ **Cref 兼容性**：架构与 cref 保持一致

---

## 🎓 技术要点

### 1. 为什么需要 module_id 和 module 指针？

**module_id (u8)**:
- 快速索引到 `vm->module_registry[module_id]`
- 存储在 GRT entry 中（高 8 bits）
- 持久化到 Flash

**module 指针**:
- 运行时快速访问模块信息
- 避免每次都查表
- 提高性能

### 2. 全局数据隔离机制

```c
// 模块提供全局数据模板（只读）
const u8 *template = module->global_data_template;

// 每个实例创建独立副本
app->channel_data[channel].global_data_copy = malloc(module->global_data_size);
memcpy(app->channel_data[channel].global_data_copy, template, module->global_data_size);

// 执行时，BP 指向实例的副本
vm->runtime.base_pointer = app->channel_data[channel].global_data_copy;
```

### 3. 实例计数管理

```c
// 创建实例时添加
module->instance_ids[module->instance_count++] = app_id;

// 删除实例时移除
for (u8 i = 0; i < module->instance_count; i++) {
    if (module->instance_ids[i] == app_id) {
        // Shift remaining entries
        for (u8 j = i; j < module->instance_count - 1; j++) {
            module->instance_ids[j] = module->instance_ids[j + 1];
        }
        module->instance_count--;
        break;
    }
}

// 卸载模块前检查
if (module->instance_count > 0) {
    return GCOS_ERROR_MODULE_IN_USE;
}
```

---

## ✅ 总结

本次优化完成了 **阶段 1: 数据结构重构**，实现了：

1. **✅ 新增 GCOSModuleRegistry 结构** - 支持代码共享
2. **✅ 修改 GCOSAppInstance 结构** - 解耦应用与模块
3. **✅ VM 添加模块注册表数组** - 管理所有模块
4. **✅ 修复编译错误** - 所有代码编译通过

**下一步**：开始实施阶段 2（模块加载逻辑改造）。

---

**实施日期**: 2026-05-09  
**参考设计**: cref Package/Applet 模型  
**优化目标**: 代码共享、GRT 回收、多实例支持  
**当前进度**: 阶段 1/5 完成 ✅
