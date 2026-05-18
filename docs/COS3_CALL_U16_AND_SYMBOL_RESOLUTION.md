# COS3 call.u16 指令与符号解析机制深度分析

## 🎯 核心问题

**用户疑问**: 
> call 指令参数是 16-bit 的，符号解析也只能替换为 16-bit，所以用最高 bit 标识是模块内偏移还是 GRT 引用？

**答案**: **是的！** 这正是 GCOS 采用的混合寻址模式的核心设计。

---

## 📋 COS3 规范的 call 指令

### 三种调用指令

COS3 规范定义了三种函数调用指令（line 1561-1592）：

| 指令 | 操作码 | 参数 | 用途 |
|------|--------|------|------|
| `callex.u8` | 0x14 | u8 funcidx | **外部**函数调用（跨模块） |
| `callin.u8` | 0x15 | u8 funcidx | **内部**函数调用（本模块） |
| `call.u16` | 0x16 | u16 funcidx | **通用**函数调用（自动判断） |

---

### call.u16 的关键描述

**规范原文** (line 1591):

> 执行函数调用；funcidx为u16类型数据，表示函数索引;
> - **若funcidx在内部函数索引范围之内**，从函数段获取内部函数地址
> - **若funcidx在外部函数索引范围之内**，依据导入段信息获取外部函数地址

**关键理解：**

```
call.u16 funcidx (16-bit)
  ↓
判断: funcidx < internal_function_count ?
  ├─ YES → 内部调用：直接从函数段查找
  └─ NO  → 外部调用：从导入段查找
           external_idx = funcidx - internal_function_count
           导入表[external_idx] → compact_address (16-bit)
           解码 compact_address → module_idx + func_idx
           查找目标模块 → 导出表 → 内部函数索引
           跳转到目标函数
```

---

## 🔍 GCOS 的实现策略

### 问题分析

**约束条件：**
1. ✅ call.u16 的参数是 **16-bit**
2. ✅ 符号解析后需要填入 **16-bit** 的值
3. ❌ 16-bit 无法直接存储 32-bit 物理地址

**解决方案：混合寻址模式**

GCOS 采用了类似 cref 的设计，使用 **bit 15** 作为标志位：

```
┌─────────────────────────────────────────┐
│     16-bit Resolved Address Format      │
├──────────────┬──────────────────────────┤
│  Bit 15      │      Bit 14-0            │
│  Global Flag │  Index/Offset            │
│  (1 bit)     │  (15 bits)               │
└──────────────┴──────────────────────────┘
     ↑                    ↑
  0 = 本地偏移        本地函数偏移
  1 = GRT 引用        GRT 表索引
```

---

### GCOS 的地址格式定义

**文件**: [gcos_symbol_resolver.h](file://e:/views/gcos/prog/cos/gcos_vm/include/gcos_symbol_resolver.h#L65-L70)

```c
/* Address format flags */
#define ADDR_FLAG_GLOBAL        0x8000  /* Bit 15: global reference flag */
#define ADDR_MASK_INDEX         0x7FFF  /* Bits 14-0: index mask */

/* Invalid symbol index */
#define SYMBOL_IDX_INVALID      0xFFFF
```

**两种地址类型：**

#### 1. 本地函数调用（bit 15 = 0）

```
16-bit address: 0x0000 - 0x7FFF (0 - 32767)
含义: 模块内部的函数偏移量
示例: 0x0123 → 直接跳转到 code_base + 0x0123
```

#### 2. 全局引用调用（bit 15 = 1）

```
16-bit address: 0x8000 - 0xFFFF
含义: GRT (Global Reference Table) 索引
示例: 0x8005 → GRT[5] → 32-bit 逻辑地址 → 实际物理地址
```

**辅助宏：**

```c
/* 检查是否为全局引用 */
#define IS_GLOBAL_REF(addr)     (((addr) & ADDR_FLAG_GLOBAL) != 0)

/* 提取 GRT 索引 */
#define GET_GRT_INDEX(addr)     ((addr) & ADDR_MASK_INDEX)

/* 创建全局引用地址 */
#define MAKE_GLOBAL_ADDR(index) ((u16)(ADDR_FLAG_GLOBAL | ((index) & ADDR_MASK_INDEX)))
```

---

## 🔄 完整的符号解析流程

### 场景 1: 内部函数调用

```c
// 源代码
call.u16 5  // 调用本模块的第 5 个函数

// 链接时解析
if (funcidx < module->internal_function_count) {
    // 内部调用，直接使用偏移量
    resolved_address = function_offset[5];  // 例如: 0x0234
}

// 运行时执行
vm->pc = module->code_base + resolved_address;
// vm->pc = code_base + 0x0234
```

### 场景 2: 外部函数调用（通过 GRT）

```c
// 源代码
call.u16 100  // 假设内部函数只有 50 个，所以这是外部调用

// 链接时解析
if (funcidx >= module->internal_function_count) {
    // 外部调用
    external_idx = funcidx - module->internal_function_count;  // 100 - 50 = 50
    
    // 从导入表查找
    GCOSImportSymbol *import = &module->imports[external_idx];
    
    // 解码紧凑地址 (5位模块 + 11位函数)
    u8 target_module_idx = DECODE_MODULE_ID(import->compact_address);
    u16 target_func_idx = DECODE_FUNC_IDX(import->compact_address);
    
    // 查找目标模块
    GCOSModule *target_mod = find_module_by_index(vm, target_module_idx);
    
    // 在目标模块的导出表中查找
    u16 internal_idx = target_mod->export_table[target_func_idx];
    u32 logical_addr = target_mod->functions[internal_idx].offset;
    
    // ⭐ 创建 GRT 条目
    u16 grt_index = gcos_symbol_create_global_ref(
        vm, 
        logical_addr,      // 32-bit 逻辑地址
        target_module_idx, // 模块 ID
        target_func_idx    // 函数索引
    );
    
    // ⭐ 返回 16-bit GRT 引用地址
    import->resolved_address = MAKE_GLOBAL_ADDR(grt_index);
    // 例如: grt_index = 3 → resolved_address = 0x8003
}

// 运行时执行
u16 resolved_addr = 0x8003;

if (IS_GLOBAL_REF(resolved_addr)) {
    // 全局引用，查 GRT 表
    u16 grt_idx = GET_GRT_INDEX(resolved_addr);  // 3
    u32 logical_addr;
    gcos_symbol_resolve_global_ref(vm, grt_idx, &logical_addr);
    
    // 转换为物理地址并跳转
    vm->pc = translate_logical_to_physical(logical_addr);
} else {
    // 本地调用
    vm->pc = module->code_base + resolved_addr;
}
```

---

## 📊 地址空间划分

### 16-bit 地址空间布局

```
┌─────────────────────────────────────────┐
│       16-bit Address Space              │
├─────────────────────────────────────────┤
│ 0x0000 - 0x7FFF  (0 - 32767)           │
│   Local Function Offsets                │
│   直接跳转到 code_base + offset         │
├─────────────────────────────────────────┤
│ 0x8000 - 0xFFFF  (32768 - 65535)       │
│   GRT References                        │
│   索引 = addr & 0x7FFF                  │
│   查 GRT[index] → 32-bit 逻辑地址       │
└─────────────────────────────────────────┘
```

### GRT 表结构

**文件**: [gcos_symbol_resolver.h](file://e:/views/gcos/prog/cos/gcos_vm/include/gcos_symbol_resolver.h#L95-L135)

```c
typedef struct {
    u32 packed_data;  /* High 8 bits = module_id, Low 24 bits = logical_address */
} GCOSGlobalRefEntry;

// 示例：GRT[3] = { module_id: 5, logical_address: 0x123456 }
// packed_data = 0x05123456
```

**内存布局：**

```
GRT Entry (4 bytes):
┌──────────────┬──────────────────────────┐
│  Bit 31-24   │      Bit 23-0            │
│  Module ID   │  Logical Address         │
│  (8 bits)    │  (24 bits, 支持 16 MB)   │
└──────────────┴──────────────────────────┘
```

---

## 🎯 为什么需要这种设计？

### 问题 1: 16-bit 限制

**挑战：**
- call.u16 只能传递 16-bit 参数
- 但智能卡可能有 > 64KB 的代码空间
- 需要支持跨模块调用

**解决：**
- 使用 bit 15 作为标志位
- 本地调用：直接用 15-bit 偏移（支持 32KB）
- 全局调用：用 15-bit 索引查 GRT（支持无限扩展）

### 问题 2: 跨模块调用

**挑战：**
- 目标函数在其他模块
- 其他模块可能还未加载
- 需要动态解析

**解决：**
- 链接时创建 GRT 条目
- GRT 存储 32-bit 逻辑地址
- 运行时通过 GRT 间接跳转

### 问题 3: 模块卸载

**挑战：**
- 模块卸载后，引用该模块的代码怎么办？

**解决：**
- GRT 条目包含 module_id
- 模块卸载时回收 GRT 条目
- 标记为无效（module_id = 0xFF）
- 槽位可重用

---

## 🔧 GCOS 实现细节

### 1. 创建全局引用

**文件**: [gcos_symbol_resolver.c](file://e:/views/gcos/prog/cos/gcos_vm/src/gcos_symbol_resolver.c#L407-L459)

```c
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index) {
    // 1. 查找空闲槽位（module_id == 0xFF）
    u16 index = gcos_symbol_find_free_slot(vm);
    
    if (index == SYMBOL_IDX_INVALID) {
        // 2. 没有空闲槽位，扩展表
        if (g_symbol_resolver.global_ref_count >= g_symbol_resolver.global_ref_capacity) {
            gcos_symbol_expand_global_ref_table(vm);
        }
        index = g_symbol_resolver.global_ref_count++;
    }
    
    // 3. 写入 GRT 条目（packed 格式）
    GCOSGlobalRefEntry *entry = get_entry_by_index(vm, index);
    GRT_SET_ENTRY(*entry, logical_address, module_id);
    
    // 4. 返回 16-bit GRT 引用地址
    return MAKE_GLOBAL_ADDR(index);  // 0x8000 | index
}
```

### 2. 解析全局引用

**文件**: [gcos_symbol_resolver.c](file://e:/views/gcos/prog/cos/gcos_vm/src/gcos_symbol_resolver.c#L370-L405)

```c
bool gcos_symbol_resolve_global_ref(GCOSVM *vm, u16 global_addr, u32 *out_logical_addr) {
    // 1. 检查是否为全局引用
    if (!IS_GLOBAL_REF(global_addr)) {
        return false;
    }
    
    // 2. 提取 GRT 索引
    u16 index = GET_GRT_INDEX(global_addr);
    
    // 3. 查找 GRT 条目
    GCOSGlobalRefEntry *entry = get_entry_by_index(vm, index);
    
    // 4. 检查有效性
    if (!GRT_IS_VALID(*entry)) {
        return false;  // 条目已被回收
    }
    
    // 5. 提取 24-bit 逻辑地址
    *out_logical_addr = GRT_GET_ADDRESS(*entry);
    return true;
}
```

### 3. 执行 call.u16

**伪代码：**

```c
void execute_call_u16(GCOSVM *vm, u16 funcidx) {
    GCOSModule *current_mod = vm->runtime.current_module;
    u16 resolved_addr;
    
    // 1. 判断是内部还是外部调用
    if (funcidx < current_mod->internal_function_count) {
        // 内部调用：直接使用偏移量
        resolved_addr = current_mod->function_offsets[funcidx];
    } else {
        // 外部调用：从导入表查找
        u16 external_idx = funcidx - current_mod->internal_function_count;
        GCOSImportSymbol *import = &current_mod->imports[external_idx];
        
        if (!import->is_resolved) {
            // 延迟解析
            resolve_import_symbol(vm, import);
        }
        
        resolved_addr = import->resolved_address;
    }
    
    // 2. 判断是本地还是全局引用
    if (IS_GLOBAL_REF(resolved_addr)) {
        // 全局引用：查 GRT
        u32 logical_addr;
        gcos_symbol_resolve_global_ref(vm, resolved_addr, &logical_addr);
        
        // 转换为物理地址
        vm->runtime.program_counter = translate_logical(logical_addr);
    } else {
        // 本地调用：直接跳转
        vm->runtime.program_counter = current_mod->code_base + resolved_addr;
    }
    
    // 3. 创建新的栈帧
    push_stack_frame(vm);
}
```

---

## 📈 性能分析

### 内存开销

| 项目 | 大小 | 说明 |
|------|------|------|
| **GRT 条目** | 4 字节 | module_id (8 bits) + logical_address (24 bits) |
| **64 条目 GRT** | 256 字节 | 静态分配 |
| **256 条目 GRT** | 1,024 字节 | 动态扩展上限 |
| **每个导入符号** | 6 字节 | compact_address (2) + resolved_address (2) + is_resolved (1) + padding (1) |

### 时间开销

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| **本地调用** | O(1) | 直接跳转 |
| **全局调用（已解析）** | O(1) | 查 GRT + 跳转 |
| **全局调用（首次）** | O(n) | 线性搜索空闲槽位 |
| **GRT 扩展** | O(m) | m = 新容量，需初始化 |

### 优势

✅ **节省空间**：16-bit 地址足够大部分场景  
✅ **灵活扩展**：GRT 支持动态扩展  
✅ **快速访问**：O(1) 查表  
✅ **易于回收**：软删除 + 槽位重用  

---

## 🆚 与 Cref 对比

| 特性 | COS3/GCOS | Cref (Java Card) |
|------|-----------|------------------|
| **地址格式** | 16-bit (bit 15 标志) | 16-bit (bit 15 标志) |
| **本地偏移** | 15 bits (0-32767) | 15 bits (0-32767) |
| **GRT 索引** | 15 bits (0-32767) | 15 bits (0-32767) |
| **GRT 条目** | 4 字节 (8+24) | 5 字节 (32+8) |
| **模块 ID** | 8 bits (0-255) | 8 bits (0-255) |
| **地址空间** | 24 bits (16 MB) | 32 bits (4 GB) |
| **链接时机** | 加载时静态链接 | 运行时动态链接 |

**相似之处：**
- ✅ 都使用 bit 15 作为全局引用标志
- ✅ 都使用 GRT 进行间接寻址
- ✅ 都支持槽位重用

**不同之处：**
- ❌ GCOS 的 GRT 更紧凑（4 字节 vs 5 字节）
- ❌ GCOS 在加载时解析，cref 在运行时解析
- ❌ GCOS 支持 16 MB 地址空间，cref 支持 4 GB

---

## ✅ 总结

### 回答用户问题

**Q**: call 指令参数是 16-bit 的，符号解析也就是只能替换为 16-bit，所以用最高 bit 标识是模块内偏移还是 GRT 的引用？

**A**: **完全正确！** 

1. ✅ **call.u16 参数是 16-bit**
2. ✅ **符号解析结果也是 16-bit**
3. ✅ **使用 bit 15 作为标志位**：
   - `0` = 本地函数偏移（直接跳转）
   - `1` = GRT 索引（间接跳转）

### 设计优势

- 🎯 **兼容 COS3 规范**：符合 call.u16 的 16-bit 限制
- 🎯 **支持大地址空间**：通过 GRT 间接寻址支持 16 MB
- 🎯 **高效内存利用**：GRT 条目仅 4 字节
- 🎯 **灵活的模块管理**：支持模块卸载和 GRT 回收

### 实施状态

- ✅ **数据结构**：已定义 GCOSGlobalRefEntry (4 字节 packed)
- ✅ **地址格式**：已定义 ADDR_FLAG_GLOBAL (0x8000)
- ✅ **创建函数**：已实现 gcos_symbol_create_global_ref()
- ✅ **解析函数**：已实现 gcos_symbol_resolve_global_ref()
- ✅ **回收机制**：已实现 gcos_symbol_delete_module_global_refs()
- ⏸️ **集成到 call.u16**：待实施

---

**相关文档：**
- [COS3_CROSS_MODULE_ACCESS_ANALYSIS.md](file://e:/views/gcos/prog/cos/gcos_vm/COS3_CROSS_MODULE_ACCESS_ANALYSIS.md) - COS3 跨模块访问分析
- [GCOS_GRT_OPTIMIZATION_COMPLETE.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_GRT_OPTIMIZATION_COMPLETE.md) - GRT 优化完成报告
- [cos3-qw.md](file://e:/views/gcos/prog/cos/cos3-qw.md#L1581-L1592) - COS3 规范 call.u16 指令定义
