# GCOS 符号定位机制与全局引用表管理策略

## 📋 执行摘要

本文档详细分析 **cref**、**iwasm** 和 **GCOS** 三种虚拟机在资源受限环境下的符号定位机制，并提供 GCOS 全局引用表的动态扩展策略。

---

## 🔍 三种虚拟机的符号定位机制对比

### 1. cref 的 Token 机制

#### Token 定义
cref 使用 **8位 token（令牌）** 来定位类的方法和成员：

```c
// cref 中的典型用法
u8 method_token = fetchByte();  // 从字节码读取方法 token
methodAddr = resolveVirtualMethod(objInfo, method_token);
```

#### Token 的特点

| 特性 | 说明 |
|------|------|
| **宽度** | 8位（0-255） |
| **作用域** | 类内相对索引 |
| **查找方式** | 两级查找：token → 方法表 → 地址 |
| **空间效率** | 极高（仅1字节） |

#### Token 解析流程

```
┌─────────────────────────────────────────────┐
│ 字节码中的 token (u8)                        │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ 在类的方法表中查找:                           │
│ - 公共方法表 (Public VMT)                    │
│ - 包私有方法表 (Package VMT)                 │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ 获取方法引用 (jref, 16位)                    │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ 检查 jref 最高位 (Bit 15):                   │
│                                             │
│  Bit 15 = 0:                                │
│    → 包内偏移，直接计算地址                  │
│    address = base + jref                    │
│                                             │
│  Bit 15 = 1:                                │
│    → 外部引用，查全局引用表                  │
│    address = global_ref_table[jref & 0x7FFF]│
└─────────────────────────────────────────────┘
```

#### cref 代码示例

```c
// reference.c - cref 的全局引用解析
memref resolveReferenceAddress(jref referenceID, u8 pkgID, u8 comp_type) {
    if ((referenceID & (jref)(0x8000)) != 0) {
        /* Reference is external - resolve using global reference table */
        return getReferenceAddress(referenceID);
    } else {
        /* Reference is internal - resolve using package table */
        return getPackageComponentLocation(pkgID, comp_type) + referenceID;
    }
}

// method.c - cref 的虚方法解析
memref resolveVirtualMethod(Object_Info_ptr objInfo, u8 method_token) {
    // 通过 token 在类的虚方法表中查找
    while (true) {
        u8 pub_mtb = CODE_read_u8(classAddr + CR_PublicMethods + MTH_MethodTableBase);
        u8 MethodTCount = CODE_read_u8(classAddr + CR_PublicMethods + MTH_NumMethods);
        
        if ((method_token < pub_mtb) || (method_token > (pub_mtb + MethodTCount - 1))) {
            // 未找到，查找父类
            classRef = get_super_class(classAddr);
            classAddr = resolveReferenceAddressAndPackage(classRef, classPkg, ...);
        } else {
            // 找到，获取方法引用
            methodRef = CODE_read_u16(classAddr + CR_MethodsTableStart + 
                                     (method_token - pub_mtb) * SIZE_REF);
            
            // 解析方法地址
            methodAddr = resolveReferenceAddressAndPackage(methodRef, classPkg, ...);
            return methodAddr;
        }
    }
}
```

---

### 2. iwasm 的函数索引机制

#### 函数索引定义
iwasm 使用 **32位函数索引（function index）** 直接定位函数实例：

```c
// iwasm 中的典型用法
uint32 func_idx = fetch_u32();  // 从字节码读取函数索引
WASMFunctionInstance *func = module_inst->e->functions + func_idx;
```

#### 函数索引的特点

| 特性 | 说明 |
|------|------|
| **宽度** | 32位（0-4,294,967,295） |
| **作用域** | 模块内绝对索引 |
| **查找方式** | 直接数组索引 |
| **空间效率** | 较低（4字节） |

#### iwasm 解析流程

```
┌─────────────────────────────────────────────┐
│ 字节码中的 func_idx (u32)                    │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ 判断是否为导入函数:                           │
│                                             │
│  if func_idx < import_function_count:       │
│    → 查导入函数表                            │
│    func = import_functions[func_idx]        │
│                                             │
│  else:                                      │
│    → 查本地函数表                            │
│    func = functions[func_idx - import_count]│
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ 获取函数实例指针                              │
│ → 直接调用                                   │
└─────────────────────────────────────────────┘
```

#### iwasm 代码示例

```c
// wasm_runtime.c - iwasm 的函数调用
static void call_indirect(WASMExecEnv *exec_env, uint32 elem_idx, 
                          uint32 argc, uint32 argv[]) {
    WASMModuleInstance *module_inst = exec_env->module_inst;
    
    // 直接通过索引获取函数实例
    WASMFunctionInstance *cur_func = module_inst->e->functions + elem_idx;
    
    // 调用函数
    wasm_interp_call_func_bytecode(module_inst, exec_env, cur_func, argc, argv);
}

// 创建函数对象
wasm_create_func_obj(WASMModuleInstance *module_inst, uint32 func_idx, ...) {
    // 检查索引范围
    if (func_idx >= module->import_function_count + module->function_count) {
        return NULL;  // 索引越界
    }
    
    // 根据索引获取函数类型
    if (func_idx < module->import_function_count) {
        func_type = module->import_functions[func_idx].u.function.func_type;
    } else {
        func_type = module->functions[func_idx - module->import_function_count].func_type;
    }
    
    // 创建函数对象
    ...
}
```

---

### 3. GCOS 的混合寻址机制

#### 设计理念
GCOS 结合了 **cref 的空间效率** 和 **iwasm 的简单性**，采用 **16位紧凑地址 + 全局引用表**：

```
┌─────────────────────────────────────────────┐
│ 16位紧凑地址格式                              │
├──────────┬──────────────────────────────────┤
│ Bit 15   │ Bits 14-0                        │
├──────────┼──────────────────────────────────┤
│ 0        │ 本地地址 (0-32767)                │
│          │ → 直接使用，类似 cref 包内偏移     │
├──────────┼──────────────────────────────────┤
│ 1        │ 全局引用索引 (0-32767)            │
│          │ → 查表得到32位地址                 │
│          │ → 类似 cref 全局引用表             │
└──────────┴──────────────────────────────────┘
```

#### GCOS 的特点

| 特性 | 说明 |
|------|------|
| **宽度** | 16位（节省50%空间 vs iwasm） |
| **作用域** | 混合：本地直接 + 全局间接 |
| **查找方式** | O(1)：检查Bit 15决定路径 |
| **空间效率** | 高（2字节，支持>64KB寻址） |
| **COS3兼容** | 完全符合导入段格式规范 |

#### GCOS 解析流程

```
┌─────────────────────────────────────────────┐
│ 字节码中的 compact_addr (u16)                │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ 检查 Bit 15:                                 │
│                                             │
│  Bit 15 = 0: 本地地址                       │
│    → logical_addr = (u32)compact_addr       │
│    → 直接使用（0x0000-0x7FFF）              │
│                                             │
│  Bit 15 = 1: 全局引用                       │
│    → index = compact_addr & 0x7FFF          │
│    → entry = global_ref_table.entries[index]│
│    → logical_addr = entry.logical_address   │
│    → 32位地址（无限制）                      │
└─────────────────────────────────────────────┘
```

#### GCOS 代码实现

```c
// gcos_symbol_resolver.c - GCOS 的地址解析
bool gcos_symbol_resolve_address(GCOSVM *vm, u16 compact_addr, u32 *out_logical_addr) {
    if (is_global_ref(compact_addr)) {
        /* Global reference - look up in global reference table */
        u16 index = get_index(compact_addr);  /* compact_addr & 0x7FFF */
        
        if (index >= g_symbol_resolver.global_ref_table.count) {
            return false;
        }
        
        GCOSGlobalRefEntry *entry = &g_symbol_resolver.global_ref_table.entries[index];
        
        if (!entry->is_valid) {
            return false;
        }
        
        *out_logical_addr = entry->logical_address;
        return true;
    }
    else {
        /* Local reference - direct address */
        *out_logical_addr = (u32)compact_addr;
        return true;
    }
}

// 创建全局引用
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index) {
    /* Check if table is full and needs expansion */
    if (g_symbol_resolver.global_ref_table.count >= 
        g_symbol_resolver.global_ref_table.capacity) {
        
        /* Try to expand table */
        GCOSResult ret = gcos_symbol_expand_global_ref_table(vm, 0);
        if (ret != GCOS_OK) {
            return SYMBOL_IDX_INVALID;
        }
    }
    
    u16 index = g_symbol_resolver.global_ref_table.count;
    GCOSGlobalRefEntry *entry = &g_symbol_resolver.global_ref_table.entries[index];
    
    entry->logical_address = logical_address;
    entry->module_id = module_id;
    entry->symbol_index = symbol_index;
    entry->is_valid = true;
    
    g_symbol_resolver.global_ref_table.count++;
    
    return make_global_addr(index);  /* index | 0x8000 */
}
```

---

## 📊 三种机制对比总结

| 维度 | cref | iwasm | GCOS |
|------|------|-------|------|
| **地址宽度** | 8位 token + 16位引用 | 32位索引 | 16位紧凑地址 |
| **字节码大小** | 极小（1-3字节） | 较大（4字节） | 中等（2字节） |
| **查找复杂度** | O(n) 类层次遍历 | O(1) 数组索引 | O(1) 位检查+查表 |
| **最大寻址** | 64KB（通过全局表扩展） | 4GB | >64KB（通过全局表） |
| **适用场景** | 智能卡（<8KB RAM） | 通用平台（>1MB RAM） | 智能卡/物联网（8-64KB RAM） |
| **规范兼容** | Java Card | WebAssembly | COS3 |

---

## 💾 GCOS 全局引用表管理策略

### 问题：资源受限环境下如何设计？

卡片环境的典型约束：
- **RAM**: 8KB - 64KB
- **Flash**: 128KB - 512KB
- **CPU**: 8-32 MHz
- **无操作系统** 或极简 RTOS

### 方案 1：静态分配（推荐用于极小内存 < 16KB）

#### 配置
```c
/* gcos_platform.h */
#define GCOS_TINY_MEMORY  /* Enable static allocation */

/* gcos_symbol_resolver.h */
#define MAX_GLOBAL_REFS     32    /* 32 entries × 12 bytes = 384 bytes */
```

#### 优点
- ✅ **零堆内存开销**：无需 malloc/free
- ✅ **确定性**：编译时确定内存使用
- ✅ **简单可靠**：无内存碎片风险
- ✅ **快速启动**：无需初始化堆

#### 缺点
- ❌ **固定上限**：无法扩展
- ❌ **可能浪费**：如果实际使用少于容量

#### 适用场景
- 智能卡（8-16KB RAM）
- 安全芯片
- 一次性加载少量模块

#### 实现
```c
// gcos_symbol_resolver.c
#ifdef GCOS_TINY_MEMORY
    /* Static allocation for tiny memory environments */
    g_symbol_resolver.global_ref_table.entries = g_symbol_resolver.static_global_refs;
    g_symbol_resolver.global_ref_table.capacity = MAX_GLOBAL_REFS;
    g_symbol_resolver.global_ref_table.is_dynamic = false;
#endif
```

---

### 方案 2：动态扩展（推荐用于中等内存 16-64KB）

#### 配置
```c
/* gcos_platform.h */
/* No GCOS_TINY_MEMORY defined - use dynamic allocation */

/* gcos_symbol_resolver.h */
#define MAX_GLOBAL_REFS         64      /* Initial capacity */
#define MAX_GLOBAL_REFS_MAX     4096    /* Absolute maximum */
#define GLOBAL_REF_GROWTH_FACTOR 2      /* Double when full */
```

#### 优点
- ✅ **灵活扩展**：按需增长
- ✅ **节省初始内存**：从小容量开始
- ✅ **适应性强**：适合不同规模的SEF文件

#### 缺点
- ❌ **需要堆内存**：依赖 malloc/free
- ❌ **可能失败**：内存不足时扩展失败
- ❌ **碎片风险**：多次扩展可能导致碎片

#### 适用场景
- 中高端智能卡（32-64KB RAM）
- 物联网设备
- 需要加载多个模块

#### 实现

**初始化**：
```c
// gcos_symbol_resolver.c
#ifndef GCOS_TINY_MEMORY
    /* Dynamic allocation for normal environments */
    g_symbol_resolver.global_ref_table.entries = (GCOSGlobalRefEntry *)malloc(
        sizeof(GCOSGlobalRefEntry) * MAX_GLOBAL_REFS);
    
    if (!g_symbol_resolver.global_ref_table.entries) {
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    g_symbol_resolver.global_ref_table.capacity = MAX_GLOBAL_REFS;
    g_symbol_resolver.global_ref_table.is_dynamic = true;
#endif
```

**自动扩展**：
```c
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index) {
    /* Check if table is full */
    if (g_symbol_resolver.global_ref_table.count >= 
        g_symbol_resolver.global_ref_table.capacity) {
        
        GCOS_PRINTF("[Symbol Resolver] WARNING: Table full, expanding...\n");
        
        /* Auto-expand */
        GCOSResult ret = gcos_symbol_expand_global_ref_table(vm, 0);
        if (ret != GCOS_OK) {
            return SYMBOL_IDX_INVALID;
        }
    }
    
    /* Create entry... */
}
```

**手动扩展 API**：
```c
GCOSResult gcos_symbol_expand_global_ref_table(GCOSVM *vm, u16 new_capacity) {
#ifdef GCOS_TINY_MEMORY
    /* Static allocation - cannot expand */
    return GCOS_ERR_OUT_OF_MEMORY;
#endif
    
    /* Calculate new capacity */
    if (new_capacity == 0) {
        new_capacity = g_symbol_resolver.global_ref_table.capacity * 
                       GLOBAL_REF_GROWTH_FACTOR;
    }
    
    /* Check maximum limit */
    if (new_capacity > MAX_GLOBAL_REFS_MAX) {
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Allocate new table */
    GCOSGlobalRefEntry *new_entries = (GCOSGlobalRefEntry *)malloc(
        sizeof(GCOSGlobalRefEntry) * new_capacity);
    
    if (!new_entries) {
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Copy existing entries */
    memcpy(new_entries, g_symbol_resolver.global_ref_table.entries,
           sizeof(GCOSGlobalRefEntry) * g_symbol_resolver.global_ref_table.count);
    
    /* Free old table */
    if (g_symbol_resolver.global_ref_table.is_dynamic) {
        free(g_symbol_resolver.global_ref_table.entries);
    }
    
    /* Update table */
    g_symbol_resolver.global_ref_table.entries = new_entries;
    g_symbol_resolver.global_ref_table.capacity = new_capacity;
    
    return GCOS_OK;
}
```

---

### 方案 3：混合策略（最佳实践）

结合静态和动态的优点：

```c
/* gcos_symbol_resolver.h */

/* Tier 1: Tiny memory (< 16KB RAM) */
#if defined(GCOS_TINY_MEMORY)
    #define MAX_GLOBAL_REFS     32
    #define USE_STATIC_ALLOC    1

/* Tier 2: Small memory (16-32KB RAM) */
#elif defined(GCOS_SMALL_MEMORY)
    #define MAX_GLOBAL_REFS     64
    #define MAX_GLOBAL_REFS_MAX 256
    #define USE_STATIC_ALLOC    0

/* Tier 3: Medium memory (32-64KB RAM) */
#elif defined(GCOS_MEDIUM_MEMORY)
    #define MAX_GLOBAL_REFS     128
    #define MAX_GLOBAL_REFS_MAX 1024
    #define USE_STATIC_ALLOC    0

/* Tier 4: Large memory (> 64KB RAM) */
#else
    #define MAX_GLOBAL_REFS     256
    #define MAX_GLOBAL_REFS_MAX 4096
    #define USE_STATIC_ALLOC    0
#endif
```

**使用建议**：

| RAM 大小 | 推荐配置 | 初始容量 | 最大容量 | 分配方式 |
|----------|---------|---------|---------|---------|
| < 8KB | GCOS_TINY_MEMORY | 16 | 16 | 静态 |
| 8-16KB | GCOS_TINY_MEMORY | 32 | 32 | 静态 |
| 16-32KB | GCOS_SMALL_MEMORY | 64 | 256 | 动态 |
| 32-64KB | GCOS_MEDIUM_MEMORY | 128 | 1024 | 动态 |
| > 64KB | Default | 256 | 4096 | 动态 |

---

## 🎯 实际部署建议

### 1. 智能卡部署（8-16KB RAM）

```c
/* gcos_config.h */
#define GCOS_TINY_MEMORY
#define MAX_MODULES             4
#define MAX_EXPORT_SYMBOLS      16
#define MAX_IMPORT_SYMBOLS      16
#define MAX_GLOBAL_REFS         32
#define MAX_SYSTEM_MODULES      2
```

**预期内存占用**：
- 全局引用表：32 × 12 = 384 字节（静态）
- 导出表：4 × 16 × 40 = 2,560 字节
- 导入表：4 × 16 × 6 = 384 字节
- 系统模块：2 × ~200 = 400 字节
- **总计**：~3.7 KB

### 2. 物联网设备部署（32-64KB RAM）

```c
/* gcos_config.h */
#define GCOS_MEDIUM_MEMORY
#define MAX_MODULES             16
#define MAX_EXPORT_SYMBOLS      64
#define MAX_IMPORT_SYMBOLS      64
#define MAX_GLOBAL_REFS         128
#define MAX_GLOBAL_REFS_MAX     1024
#define MAX_SYSTEM_MODULES      8
```

**预期内存占用**：
- 全局引用表：128 × 12 = 1,536 字节（初始），可扩展至 12KB
- 导出表：16 × 64 × 40 = 40,960 字节
- 导入表：16 × 64 × 6 = 6,144 字节
- 系统模块：8 × ~500 = 4,000 字节
- **总计**：~52 KB（初始），可扩展

### 3. 桌面/服务器部署（> 1MB RAM）

```c
/* gcos_config.h */
/* Use defaults */
#define MAX_GLOBAL_REFS         256
#define MAX_GLOBAL_REFS_MAX     4096
```

---

## 📈 性能分析

### 内存占用对比

| 配置 | 初始容量 | 最大容量 | 初始内存 | 最大内存 | 适用场景 |
|------|---------|---------|---------|---------|---------|
| Tiny | 16 | 16 | 192 B | 192 B | 8KB RAM 卡片 |
| Small | 32 | 32 | 384 B | 384 B | 16KB RAM 卡片 |
| Small+ | 64 | 256 | 768 B | 3 KB | 32KB RAM IoT |
| Medium | 128 | 1024 | 1.5 KB | 12 KB | 64KB RAM IoT |
| Large | 256 | 4096 | 3 KB | 48 KB | >1MB RAM |

### 扩展性能

假设每次扩展翻倍（growth factor = 2）：

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 创建引用 | O(1) 平均 | 摊销后，考虑扩展成本 |
| 解析地址 | O(1) | 直接数组访问 |
| 扩展表 | O(n) | 需要复制现有条目 |
| 最坏情况扩展 | O(n) | 当表满时触发 |

**扩展频率估算**：
- 初始容量 64，每次翻倍
- 扩展到 128：第 65 个引用
- 扩展到 256：第 129 个引用
- 扩展到 512：第 257 个引用
- 扩展到 1024：第 513 个引用

对于大多数应用，**64-128 的初始容量足够**，极少需要扩展。

---

## 🔧 使用示例

### 示例 1：静态分配（卡片环境）

```c
/* 编译时定义 */
#define GCOS_TINY_MEMORY

/* 初始化 */
GCOSVM *vm = gcos_vm_create();
gcos_vm_init(vm);
gcos_symbol_resolver_init(vm);

/* 输出：
 * [Symbol Resolver] Using static global ref table (32 entries)
 * [Symbol Resolver] Initialized
 */

/* 使用 - 最多32个全局引用 */
u16 ref1 = gcos_symbol_create_global_ref(vm, 0x1000, 0, 0);
u16 ref2 = gcos_symbol_create_global_ref(vm, 0x2000, 0, 1);
/* ... 最多32个 ... */

/* 尝试第33个 - 失败 */
u16 ref33 = gcos_symbol_create_global_ref(vm, 0x3000, 0, 2);
/* ref33 == SYMBOL_IDX_INVALID */
```

### 示例 2：动态扩展（IoT环境）

```c
/* 编译时不定义 GCOS_TINY_MEMORY */

/* 初始化 */
GCOSVM *vm = gcos_vm_create();
gcos_vm_init(vm);
gcos_symbol_resolver_init(vm);

/* 输出：
 * [Symbol Resolver] Using dynamic global ref table (64 entries)
 * [Symbol Resolver] Initialized
 */

/* 使用 - 自动扩展 */
for (int i = 0; i < 100; i++) {
    u16 ref = gcos_symbol_create_global_ref(vm, 0x1000 + i*0x100, 0, i);
    
    if (ref == SYMBOL_IDX_INVALID) {
        printf("Failed at reference %d\n", i);
        break;
    }
}

/* 输出：
 * [Symbol Resolver] WARNING: Global ref table full (64/64), attempting expansion...
 * [Symbol Resolver] Expanding global ref table: 64 -> 128 entries
 * [Symbol Resolver] Global ref table expanded successfully
 * [Symbol Resolver] WARNING: Global ref table full (128/128), attempting expansion...
 * [Symbol Resolver] Expanding global ref table: 128 -> 256 entries
 * [Symbol Resolver] Global ref table expanded successfully
 */

/* 统计 */
gcos_symbol_print_stats(vm);
/* 输出：
 * Global ref entries:   100 / 256 (dynamic)
 */
```

### 示例 3：手动控制扩展

```c
/* 预分配大容量 */
GCOSResult ret = gcos_symbol_expand_global_ref_table(vm, 512);

if (ret == GCOS_OK) {
    printf("Pre-allocated 512 global reference entries\n");
}

/* 后续创建不会触发自动扩展，直到超过512 */
```

---

## ✅ 总结与建议

### 关键要点

1. **cref 使用 8位 token**：
   - 极小的空间开销
   - 需要两级查找（token → 方法表 → 地址）
   - 适合极小内存环境

2. **iwasm 使用 32位索引**：
   - 简单直接的数组访问
   - 较大的空间开销
   - 适合资源丰富环境

3. **GCOS 使用 16位混合寻址**：
   - 平衡空间效率和性能
   - O(1) 时间复杂度
   - 通过全局引用表突破 64KB 限制
   - 完全兼容 COS3 规范

### 全局引用表管理建议

| 环境 | 推荐策略 | 初始容量 | 是否动态 |
|------|---------|---------|---------|
| **< 16KB RAM** | 静态分配 | 16-32 | ❌ 否 |
| **16-32KB RAM** | 动态分配 | 64 | ✅ 是 |
| **32-64KB RAM** | 动态分配 | 128 | ✅ 是 |
| **> 64KB RAM** | 动态分配 | 256 | ✅ 是 |

### 最佳实践

1. **优先静态分配**：如果内存允许，静态分配更可靠
2. **合理设置初始容量**：根据预期模块数量选择
3. **监控使用情况**：定期调用 `gcos_symbol_print_stats()`
4. **预留余量**：初始容量应为预期的 1.5-2 倍
5. **错误处理**：始终检查 `SYMBOL_IDX_INVALID` 返回值

---

## 📁 相关文件

- [include/gcos_symbol_resolver.h](file://e:/views/gcos/prog/cos/gcos_vm/include/gcos_symbol_resolver.h) - 接口定义
- [src/gcos_symbol_resolver.c](file://e:/views/gcos/prog/cos/gcos_vm/src/gcos_symbol_resolver.c) - 完整实现
- [tests/test_symbol_resolver.c](file://e:/views/gcos/prog/cos/gcos_vm/tests/test_symbol_resolver.c) - 测试套件
- [GCOS_SYMBOL_RESOLVER_COMPLETE.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_SYMBOL_RESOLVER_COMPLETE.md) - 完整文档

---

**版本**: 1.0.0  
**日期**: 2026-05-12  
**作者**: GCOS Development Team
