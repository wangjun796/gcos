# COS3 规范跨模块访问机制分析

## 📋 核心问题

**用户疑问**: COS3 规范如何定义其它模块函数或变量的访问？是否使用"模块ID + 成员ID"的格式？

---

## ✅ 答案总结

**是的！** COS3 规范采用了 **紧凑编码（Compact Encoding）** 的方式，将模块索引和函数/变量索引打包到一个 16-bit 值中。

### 核心设计

```
┌─────────────────────────────────────────┐
│     16-bit Compact Address Format       │
├──────────────┬──────────────────────────┤
│  Bit 15-11   │      Bit 10-0            │
│  Module ID   │  Member Index            │
│  (5 bits)    │  (11 bits)               │
└──────────────┴──────────────────────────┘
     ↑                    ↑
  最多 32 个模块      最多 2048 个成员
```

---

## 🔍 详细分析

### 1. 导入函数的编码格式

**规范位置**: cos3-qw.md 表 24 (line 619-624)

#### IMPORT_MODULEIDX_FUNCIDX 数据结构

| 数据项 | 数据类型 | 说明 |
|--------|----------|------|
| `import_moduleidx` | u16数据高5位（bit15-bit11） | 导入函数的模块在 `import_module_items` 的索引 |
| `import_funcidx` | u16数据低11位（bit10-bit0） | 导入函数在导入模块的内部函数索引 |

**C 语言实现：**

```c
// 编码：module_idx (5 bits) + func_idx (11 bits)
u16 encode_import_ref(u8 module_idx, u16 func_idx) {
    return ((u16)(module_idx & 0x1F) << 11) | (func_idx & 0x7FF);
}

// 解码：提取 module_idx
u8 get_module_idx(u16 compact_addr) {
    return (compact_addr >> 11) & 0x1F;
}

// 解码：提取 func_idx
u16 get_func_idx(u16 compact_addr) {
    return compact_addr & 0x7FF;
}
```

**示例：**

```c
// 调用模块 #5 的函数 #128
u16 import_ref = encode_import_ref(5, 128);
// 结果: 0x2880 (二进制: 00101 000000010000000)
//       高5位=00101 (5), 低11位=000000010000000 (128)

// 解析
u8 mod_idx = get_module_idx(0x2880);  // 返回 5
u16 func_idx = get_func_idx(0x2880);  // 返回 128
```

---

### 2. 函数调用指令

**规范位置**: cos3-qw.md 表 70-72 (line 1561-1592)

COS3 提供了三种函数调用指令：

#### 2.1 callex.u8 - 外部函数调用

```
操作码: 0x14
格式:   callex.u8 funcidx
栈:     …, arg1, …, argn -> …
描述:   执行外部函数调用；funcidx为u8类型数据，表示外部函数索引
        按照funcidx指示依据导入段信息获取外部函数地址
```

**工作流程：**

```
1. 从导入段查找 funcidx 对应的 IMPORT_FUNCTION_ITEMS
2. 读取 import_moduleidx_funcidx (16-bit compact address)
3. 解码得到 module_idx 和 func_idx
4. 查找 module_idx 对应的模块
5. 在该模块的导出表中查找 func_idx
6. 跳转到目标函数地址执行
```

#### 2.2 callin.u8 - 内部函数调用

```
操作码: 0x15
格式:   callin.u8 funcidx
栈:     …, arg1, …, argn -> …
描述:   执行内部函数调用；funcidx为u8类型数据，表示内部函数索引
        从函数段获取内部函数地址
```

**工作流程：**

```
1. 在当前模块的函数段中查找 func_idx
2. 直接跳转到目标函数地址执行
```

#### 2.3 call.u16 - 通用函数调用

```
操作码: 0x16
格式:   call.u16 funcidx
栈:     …, arg1, …, argn -> …
描述:   执行函数调用；funcidx为u16类型数据，表示函数索引
        若funcidx在内部函数索引范围之内，从函数段获取内部函数地址
        若funcidx在外部函数索引范围之内，依据导入段信息获取外部函数地址
```

**工作流程：**

```
if (funcidx < internal_function_count) {
    // 内部调用
    从函数段获取内部函数地址
} else {
    // 外部调用
    external_idx = funcidx - internal_function_count
    从导入段查找 external_idx
    解码 compact address
    跳转到目标模块的目标函数
}
```

---

### 3. 模块数据结构

#### 3.1 SEF 文件中的导入段

**规范位置**: cos3-qw.md 表 21-23 (line 598-618)

```
导入段 (SECTION_IMPORT):
├─ import_module_count (u8)          // 导入模块个数
├─ import_function_count (u16)       // 导入函数个数
├─ import_module_items[]             // 导入模块信息数组
│  └─ [每个模块]:
│     ├─ import_module_version (u32)
│     ├─ import_module_aid_size (u8)
│     └─ import_module_aid[] (u8[])
└─ import_function_items[]           // 导入函数信息数组
   └─ [每个函数]:
      └─ import_moduleidx_funcidx (u16)  ← 紧凑编码！
```

#### 3.2 导出段

**规范位置**: cos3-qw.md 表 29 (line 672-679)

```
导出段 (SECTION_EXPORT):
├─ section_id (u8) = 0x06
├─ size (u32)
└─ function_idxs[] (u16[])  // 导出模块内部函数索引
```

**示例：**

```
模块 A 导出 3 个函数：
导出段: [0, 5, 12]
  → 导出的是模块 A 内部的函数 #0, #5, #12

模块 B 导入模块 A 的函数 #5：
导入段:
  import_module_items[0] = { aid: "Module_A", ... }
  import_function_items[0] = encode_import_ref(0, 1)
    → module_idx=0 (指向 import_module_items[0])
    → func_idx=1 (指向 Module A 导出段的第 1 个索引)
    → Module A 导出段[1] = 5
    → 最终调用 Module A 的内部函数 #5
```

---

### 4. 全局数据访问

**规范位置**: cos3-qw.md 表 28 (line 655-669)

#### 全局段 (SECTION_GLOBAL)

| 数据项 | 数据类型 | 说明 |
|--------|----------|------|
| `rodata_base` | u16 | 模块只读数据起始地址 |
| `rwdata_base` | u16 | 模块全局数据起始地址 |
| `refdata_base` | u16 | 引用域数据起始地址 |
| `moddata_base` | u16 | 模块域数据起始地址 |
| `appdata_base` | u16 | 应用域数据起始地址 |
| `data_end` | u16 | 应用域数据结束地址 |

**内存布局：**

```
┌─────────────────────────────────────────┐
│         Module Data Layout              │
├─────────────────────────────────────────┤
│ rodata_base                             │
│   ├── 只读数据 (ROM, 不可修改)           │
│   └── ...                               │
├─────────────────────────────────────────┤
│ rwdata_base                             │
│   ├── 全局数据 (RAM, 易失性)             │
│   └── ...                               │
├─────────────────────────────────────────┤
│ refdata_base                            │
│   ├── 引用域数据 (NVM, 非易失性)         │
│   └── ...                               │
├─────────────────────────────────────────┤
│ moddata_base                            │
│   ├── 模块域数据 (NVM, 非易失性)         │
│   └── ...                               │
├─────────────────────────────────────────┤
│ appdata_base                            │
│   ├── 应用域数据 (NVM, 非易失性)         │
│   └── ...                               │
├─────────────────────────────────────────┤
│ data_end                                │
└─────────────────────────────────────────┘
```

**访问方式：**

COS3 **没有** 提供类似 `lg.module_idx.offset` 的指令来直接访问其他模块的全局数据。

**跨模块数据访问规则：**

1. **库模块的数据可以被所有应用实例访问**
   - 规范 line 473: "库模块的只读数据可被所有应用实例访问"
   - 规范 line 475: "库模块的模块域数据可被所有应用实例访问"

2. **应用模块的数据仅限于本模块范围**
   - 规范 line 473: "应用模块的只读数据限于应用模块范围"
   - 规范 line 475: "应用模块的模块域数据限于应用模块范围"

3. **通过函数调用来间接访问**
   - 其他模块不能直接访问你的全局变量
   - 必须通过导出函数来提供访问接口

**示例：**

```c
// 模块 A (库模块) - 提供 getter/setter
// global_data.c
static int secret_value = 42;

// 导出函数
int get_secret_value() {
    return secret_value;
}

void set_secret_value(int val) {
    secret_value = val;
}

// 模块 B - 调用模块 A 的函数
// main.c
extern int get_secret_value();  // 导入声明

void my_function() {
    int val = get_secret_value();  // callex.u8 调用
    // 不能直接访问模块 A 的 secret_value!
}
```

---

### 5. 内存访问指令

**规范位置**: cos3-qw.md 表 149-156 (line 2588-2691)

COS3 提供了通用的内存读写指令：

#### 5.1 读取指令 (Load from Memory)

| 指令 | 操作码 | 描述 |
|------|--------|------|
| `ldms8 disp` | 0x63 | 读取 1 字节有符号数 |
| `ldmu8 disp` | 0x64 | 读取 1 字节无符号数 |
| `ldms16 disp` | 0x65 | 读取 2 字节有符号数 |
| `ldmu16 disp` | 0x66 | 读取 2 字节无符号数 |
| `ldm32 disp` | 0x67 | 读取 4 字节数 |

**格式：**

```
栈: …, addr -> …, value
描述: 以 addr + disp 为起始地址，读取数据压入栈顶
```

#### 5.2 写入指令 (Store to Memory)

| 指令 | 操作码 | 描述 |
|------|--------|------|
| `stm8 disp` | 0x68 | 写入 1 字节 |
| `stm16 disp` | 0x69 | 写入 2 字节 |
| `stm32 disp` | 0x6a | 写入 4 字节 |

**格式：**

```
栈: …, addr, value -> …
描述: 将 value 写入到 addr + disp 地址
```

**使用示例：**

```c
// 访问当前模块的全局数据
// 假设 rwdata_base = 0x1000
// 要访问偏移 0x20 处的 u32 变量

// 方法 1: 直接计算绝对地址
push 0x1020        // addr = rwdata_base + 0x20
ldm32 0            // disp = 0, 读取 4 字节

// 方法 2: 使用基址 + 偏移
push 0x1000        // addr = rwdata_base
ldm32 0x20         // disp = 0x20, 读取 4 字节
```

---

## 🎯 GCOS 实现建议

基于 COS3 规范，GCOS 应该实现以下机制：

### 1. 紧凑地址格式支持

```c
// gcos_symbol_resolver.h

/* Compact address format (5 bits module + 11 bits function) */
#define COMPACT_MODULE_ID_MASK    0xF800U  /* Bits 15-11 */
#define COMPACT_FUNC_IDX_MASK     0x07FFU  /* Bits 10-0 */

/* Encode module_id and func_idx into compact address */
#define ENCODE_IMPORT_REF(mod_idx, func_idx) \
    (((u16)((mod_idx) & 0x1F) << 11) | ((func_idx) & 0x7FF))

/* Decode module_id from compact address */
#define DECODE_MODULE_ID(compact_addr) \
    (((compact_addr) >> 11) & 0x1F)

/* Decode func_idx from compact address */
#define DECODE_FUNC_IDX(compact_addr) \
    ((compact_addr) & 0x7FF)

/* Check if address is an import reference */
#define IS_IMPORT_REF(compact_addr) \
    (DECODE_MODULE_ID(compact_addr) != 0)
```

### 2. 导入段解析

```c
// gcos_loader.c

typedef struct {
    u32 module_version;
    GCOSAID module_aid;
    bool resolved;
    u8 resolved_module_id;  /* 0xFF if not resolved */
} GCOSImportModuleInfo;

typedef struct {
    u16 compact_address;  /* module_idx (5 bits) + func_idx (11 bits) */
    u16 resolved_address; /* Resolved absolute address after linking */
    bool is_resolved;
} GCOSImportFunctionInfo;

typedef struct {
    u8 import_module_count;
    u16 import_function_count;
    GCOSImportModuleInfo modules[MAX_IMPORT_MODULES];
    GCOSImportFunctionInfo functions[MAX_IMPORTS];
} GCOSImportSection;
```

### 3. 链接时解析

```c
// gcos_linker.c

GCOSResult resolve_imports(GCOSVM *vm, GCOSModule *target_module) {
    GCOSImportSection *imports = &target_module->imports;
    
    for (u16 i = 0; i < imports->import_function_count; i++) {
        GCOSImportFunctionInfo *func = &imports->functions[i];
        
        /* Decode compact address */
        u8 module_idx = DECODE_MODULE_ID(func->compact_address);
        u16 func_idx = DECODE_FUNC_IDX(func->compact_address);
        
        /* Find the imported module */
        GCOSImportModuleInfo *mod_info = &imports->modules[module_idx];
        if (!mod_info->resolved) {
            return GCOS_ERROR_MODULE_NOT_FOUND;
        }
        
        /* Find the target module by AID */
        GCOSModule *imported_mod = find_module_by_aid(vm, &mod_info->module_aid);
        if (imported_mod == NULL) {
            return GCOS_ERROR_MODULE_NOT_FOUND;
        }
        
        /* Get the exported function index */
        if (func_idx >= imported_mod->export_count) {
            return GCOS_ERROR_INVALID_PARAM;
        }
        u16 internal_func_idx = imported_mod->export_table[func_idx];
        
        /* Resolve to absolute address */
        func->resolved_address = imported_mod->code + 
                                  imported_mod->functions[internal_func_idx].offset;
        func->is_resolved = true;
    }
    
    return GCOS_SUCCESS;
}
```

### 4. 运行时调用

```c
// gcos_executor.c

GCOSResult execute_callex(GCOSVM *vm, u8 funcidx) {
    GCOSModule *current_mod = vm->runtime.current_module;
    GCOSImportSection *imports = &current_mod->imports;
    
    /* Check bounds */
    if (funcidx >= imports->import_function_count) {
        throw_exception(EXCEPTION_SECURITY);
        return GCOS_ERROR_ACCESS_DENIED;
    }
    
    GCOSImportFunctionInfo *func = &imports->functions[funcidx];
    
    /* Check if resolved */
    if (!func->is_resolved) {
        throw_exception(EXCEPTION_SECURITY);
        return GCOS_ERROR_ACCESS_DENIED;
    }
    
    /* Jump to resolved address */
    vm->runtime.program_counter = func->resolved_address;
    
    return GCOS_SUCCESS;
}
```

---

## 📊 与 Cref 对比

| 特性 | COS3 规范 | Cref (Java Card) |
|------|-----------|------------------|
| **函数引用格式** | 16-bit 紧凑编码 (5+11) | 16-bit (bit 15 = global flag) |
| **模块标识** | 5-bit 索引 (最多 32 个) | 8-bit package ID (最多 256 个) |
| **函数索引** | 11-bit (最多 2048 个) | 15-bit (最多 32768 个) |
| **全局引用表** | ❌ 未明确定义 | ✅ GRT (Global Reference Table) |
| **跨模块数据** | ❌ 禁止直接访问 | ❌ 禁止直接访问 |
| **访问方式** | 通过导出函数 | 通过导出方法 |
| **链接时机** | 加载时静态链接 | 运行时动态链接 |

---

## ✅ 总结

### COS3 规范的跨模块访问机制

1. **✅ 使用紧凑编码**
   - 16-bit 值：高 5 位模块索引 + 低 11 位函数索引
   - 存储在导入段的 `import_moduleidx_funcidx` 字段

2. **✅ 通过导入/导出表**
   - 导出段：列出模块对外提供的函数索引
   - 导入段：列出需要的外部函数（使用紧凑编码）

3. **✅ 静态链接**
   - 加载时解析所有导入引用
   - 运行时直接使用解析后的地址

4. **❌ 不支持直接数据访问**
   - 其他模块的全局变量不能直接访问
   - 必须通过导出函数提供访问接口

5. **✅ 库模块特殊处理**
   - 库模块的数据可以被所有应用实例访问
   - 应用模块的数据仅限于本模块

### GCOS 实施建议

1. **实现紧凑地址编解码宏**
2. **完善导入段解析逻辑**
3. **实现链接时的符号解析**
4. **添加运行时安全检查**
5. **考虑引入 GRT 以支持动态扩展**（可选）

---

**参考文档**:
- [cos3-qw.md](file://e:/views/gcos/prog/cos/cos3-qw.md) - COS3 规范全文
- [GCOS_MODULE_APP_ARCHITECTURE_OPTIMIZATION.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_MODULE_APP_ARCHITECTURE_OPTIMIZATION.md) - GCOS 模块架构优化设计
