# GCOS Flash 系统管理数据架构 - 完整设计与实现

## 📋 目录

1. [核心问题分析](#核心问题分析)
2. [整体架构设计](#整体架构设计)
3. [固定锚点：System Root Header](#固定锚点system-root-header)
4. [动态管理表](#动态管理表)
5. [模块引用机制](#模块引用机制)
6. [实施状态](#实施状态)
7. [下一步计划](#下一步计划)

---

## 🎯 核心问题分析

您提出的问题非常关键，涉及到智能卡系统的核心设计挑战：

### 问题 1：动态数据的地址管理

**问题描述：**
- 应用管理表、模块管理表、GRT 等需要持久化存储在 Flash 中
- 但这些数据结构的大小是动态的（随应用/模块数量变化）
- 如何在 Flash 中为它们分配逻辑地址？
- 如何确保重启后能找到这些动态分配的结构？

**解决方案：固定锚点模式**
```
┌─────────────────────────────────────┐
│  System Root Header (固定地址 0x0000) │ ← 永远在同一个位置
│  - module_registry_addr: 0x0100      │ ← 指向动态表
│  - app_instance_table_addr: 0x0200   │ ← 指向动态表
│  - grt_table_addr: 0x0300            │ ← 指向动态表
│  - free_list_addr: 0x0400            │ ← 指向动态表
└─────────────────────────────────────┘
         ↓              ↓           ↓
    ┌────────┐   ┌────────┐   ┌────────┐
    │模块注册表│   │应用实例表│   │  GRT表  │  ← 动态分配
    └────────┘   └────────┘   └────────┘
```

### 问题 2：模块间引用机制

**问题描述：**
- 模块间通过 AID 引用（最大 16 字节，长度适中）
- 但模块内方法/成员如何引用？
- install/select/deselect/process 等标准方法的位置？

**解决方案：函数表 + 偏移量**
```
模块 A 调用 模块 B 的方法：
  1. 通过 AID 查找模块 B → 得到 module_id
  2. 通过 module_id 查找函数表 → 得到 function_table_addr
  3. 通过方法索引访问函数表 → 得到 method_logical_address
  4. VM 执行器跳转到该地址执行

模块内成员访问：
  global_data_base + member_offset
```

### 问题 3：首次上电初始化

**问题描述：**
- 第一次上电时 Flash 是空的
- 如何构建初始的数据结构？
- 哪些逻辑地址是已知的？

**解决方案：两级初始化**
```
阶段 1：固定锚点初始化（逻辑地址 0x0000）
  - 写入 System Root Header
  - 设置 magic number、version、默认配置
  - 所有动态表指针初始化为 0（未分配）

阶段 2：动态表按需分配
  - 加载第一个模块时，分配模块注册表空间
  - 安装第一个应用时，分配应用实例表空间
  - 更新 System Root Header 中的指针
```

---

## 🏗️ 整体架构设计

### Flash 逻辑地址空间布局

```
┌──────────────────────────────────────────────────┐
│        GCOS Flash Logical Address Space          │
├──────────────────────────────────────────────────┤
│ 0x0000 - 0x00FF: System Root Header (256 bytes)  │ ← 固定
│                Magic, Version, Table Pointers     │
├──────────────────────────────────────────────────┤
│ 0x0100 - 0x01FF: Module Registry Table           │ ← 动态
│                Array of GCOSModuleRegistry        │
├──────────────────────────────────────────────────┤
│ 0x0200 - 0x02FF: Application Instance Table      │ ← 动态
│                Array of GCOSAppInstance           │
├──────────────────────────────────────────────────┤
│ 0x0300 - 0x03FF: Global Reference Table (GRT)    │ ← 动态
│                Compact 32-bit entries             │
├──────────────────────────────────────────────────┤
│ 0x0400 - 0x04FF: Free List                       │ ← 动态
│                Linked list of free blocks         │
├──────────────────────────────────────────────────┤
│ 0x0500 - ...   : Module Code Areas               │ ← 动态
│                Loaded module bytecode             │
├──────────────────────────────────────────────────┤
│ ...            : Module Function Tables           │ ← 动态
│                Method dispatch tables             │
├──────────────────────────────────────────────────┤
│ ...            : Module Global Data Areas         │ ← 动态
│                Per-module static/global variables │
├──────────────────────────────────────────────────┤
│ ...            : Application Data Areas           │ ← 动态
│                Per-instance global data           │
└──────────────────────────────────────────────────┘
```

### 数据结构关系图

```
System Root Header (0x0000)
    ├─ module_registry_addr ──→ Module Registry Table
    │                               ├─ Module 0
    │                               │   ├─ module_id: 0
    │                               │   ├─ is_loaded: true
    │                               │   ├─ module_aid: {0xA0,...}
    │                               │   ├─ code_base: 0x0500
    │                               │   ├─ function_table_addr: 0x0600
    │                               │   ├─ global_data_addr: 0x0700
    │                               │   └─ instance_count: 2
    │                               └─ Module 1
    │                                   └─ ...
    │
    ├─ app_instance_table_addr ──→ Application Instance Table
    │                                  ├─ App 0 (ISD)
    │                                  │   ├─ app_id: 0
    │                                  │   ├─ module_id: 0xFF (built-in)
    │                                  │   └─ lifecycle: SELECTABLE
    │                                  ├─ App 1
    │                                  │   ├─ app_id: 1
    │                                  │   ├─ module_id: 0
    │                                  │   ├─ module pointer → Module 0
    │                                  │   └─ global_data_addr: 0x0800
    │                                  └─ App 2
    │                                      ├─ app_id: 2
    │                                      ├─ module_id: 0
    │                                      ├─ module pointer → Module 0 (共享代码!)
    │                                      └─ global_data_addr: 0x0900
    │
    ├─ grt_table_addr ──→ Global Reference Table
    │                        ├─ Entry 0: module_id=0, addr=0x1234
    │                        ├─ Entry 1: module_id=0, addr=0x5678
    │                        └─ ...
    │
    └─ free_list_addr ──→ Free List
                             ├─ Block 1: start=0x0A00, size=512
                             ├─ Block 2: start=0x0C00, size=1024
                             └─ ...
```

---

## 🔒 固定锚点：System Root Header

### 结构设计

**文件：** `gcos_system_root.h` / `gcos_system_root.c`

```c
typedef struct {
    /* Magic number for validation */
    u32 magic;                          /* 0x47434F53 = "GCOS" */
    
    /* Version information */
    u16 version_major;                  /* 1 */
    u16 version_minor;                  /* 0 */
    
    /* System flags */
    u16 flags;                          /* GCOS_FLAG_INITIALIZED | PERSISTENT */
    u16 reserved1;
    
    /* Pointers to dynamic management tables (logical addresses) */
    u32 module_registry_addr;           /* e.g., 0x0100 */
    u32 app_instance_table_addr;        /* e.g., 0x0200 */
    u32 grt_table_addr;                 /* e.g., 0x0300 */
    u32 free_list_addr;                 /* e.g., 0x0400 */
    
    /* Table sizes and counts */
    u32 module_count;                   /* Current loaded modules */
    u32 app_count;                      /* Current app instances */
    u32 grt_capacity;                   /* GRT table capacity */
    u32 flash_total_size;               /* Total Flash size */
    
    /* Configuration parameters */
    u32 max_modules;                    /* 16 (default) */
    u32 max_apps;                       /* 32 (default) */
    u32 max_grt_entries;                /* 64 (default) */
    
    /* Integrity check */
    u32 checksum;                       /* CRC32 */
    
    /* Reserved for future expansion */
    u8 reserved[184];
} GCOS_SystemRootHeader;
```

### 关键特性

1. **固定位置**：永远在逻辑地址 0x0000
2. **完整性校验**：CRC32 checksum 防止数据损坏
3. **版本控制**：支持未来格式升级
4. **自包含**：包含所有必要的配置参数

### API 函数

```c
/* 首次上电初始化 */
GCOSResult gcos_system_root_init(GCOSVM *vm);

/* 从 Flash 加载 */
GCOSResult gcos_system_root_load(GCOSVM *vm);

/* 保存到 Flash */
GCOSResult gcos_system_root_save(GCOSVM *vm);

/* 验证完整性 */
bool gcos_system_root_validate(const GCOS_SystemRootHeader *header);

/* 更新表指针 */
GCOSResult gcos_system_root_update_table_ptr(GCOSVM *vm, u8 table_type, u32 new_addr);
```

---

## 📊 动态管理表

### 1. 模块注册表 (Module Registry Table)

**作用：** 跟踪所有已加载的模块

**结构：** `GCOSModuleRegistry` (已在之前实现)

```c
typedef struct {
    u8 module_id;                     /* 模块 ID (0-15) */
    bool is_loaded;                   /* 是否已加载 */
    GCOSAID module_aid;               /* 模块 AID */
    u8 module_version[2];             /* 模块版本 */
    
    u8 state;                         /* MODULE_NOT_LOADED / LOADED / ACTIVE */
    
    /* Code area */
    u32 code_base;                    /* 代码起始逻辑地址 */
    u32 code_size;                    /* 代码大小 */
    
    /* ⭐ NEW: Function table */
    u32 function_table_addr;          /* 函数表逻辑地址 */
    
    /* Global data area */
    u32 global_data_addr;             /* 全局数据起始地址 */
    u32 global_data_size;             /* 全局数据大小 */
    
    /* Instance tracking */
    u8 instance_count;                /* 使用该模块的应用实例数 */
    u8 instance_ids[MAX_APPS_PER_MODULE]; /* 实例 ID 列表 */
} GCOSModuleRegistry;
```

**关键点：**
- 一次加载，多个应用实例共享代码
- 通过 `instance_count` 防止卸载正在使用的模块
- 函数表指针指向方法调度表

### 2. 应用实例表 (Application Instance Table)

**作用：** 跟踪所有安装的应用实例

**结构：** `GCOSAppInstance` (已存在)

```c
typedef struct {
    u8 app_id;                        /* 应用 ID */
    GCOSAID app_aid;                  /* 应用 AID */
    
    u8 module_id;                     /* 所属模块 ID */
    GCOSModuleRegistry *module;       /* ⭐ 指向模块注册表（代码共享）*/
    
    u8 lifecycle;                     /* APPLICATION_SELECTABLE 等 */
    
    /* Global data for this instance */
    u32 global_data_addr;             /* 实例专属全局数据地址 */
    u32 global_data_size;             /* 数据大小 */
    
    /* Selection state */
    bool selected;                    /* 是否被选中 */
} GCOSAppInstance;
```

**关键点：**
- 每个实例有独立的全局数据区
- 多个实例可以共享同一个模块的代码
- 通过 `module` 指针链接到模块注册表

### 3. 全局引用表 (GRT)

**作用：** 跨模块引用的紧凑索引表

**结构：** 32位紧凑格式 (已在之前实现)

```c
typedef struct {
    u32 packed_data;                  /* 8-bit module_id + 24-bit address */
} GCOSGlobalRefEntry;

/* 宏定义 */
#define GRT_GET_MODULE_ID(entry)    ((u8)(((entry).packed_data >> 24) & 0xFF))
#define GRT_GET_ADDRESS(entry)      ((u32)((entry).packed_data & 0x00FFFFFF))
#define GRT_SET_ENTRY(entry, addr, mod) \
    ((entry).packed_data = (((u32)(mod) << 24) | ((addr) & 0x00FFFFFF)))
#define GRT_IS_VALID(entry)         (GRT_GET_MODULE_ID(entry) != 0xFF)
#define GRT_INVALIDATE(entry)       ((entry).packed_data = 0xFF000000U)
```

**关键点：**
- 节省空间：每个条目仅 4 字节
- 软删除：设置 module_id = 0xFF 标记无效
- 快速查找：通过索引直接访问

### 4. 空闲链表 (Free List)

**作用：** 跟踪 Flash 中的可用空间

**结构：** （待实现）

```c
typedef struct GCOSFreeBlock {
    u32 start_addr;                   /* 起始逻辑地址 */
    u32 size;                         /* 块大小 */
    u32 next_block_addr;              /* 下一个空闲块的逻辑地址 (0 = end) */
} GCOSFreeBlock;
```

**关键点：**
- 支持可变大小的内存分配
- 合并相邻的空闲块以减少碎片
- 类似 cref 的对象头表管理

---

## 🔗 模块引用机制

### 模块间引用：AID → module_id → GRT

**流程：**

```
模块 A 想要调用 模块 B 的导出函数：

1. 解析导入信息（来自 SEF 文件的 IMPORT 段）
   - 目标模块 AID: {0xB0, 0xB1, ..., 0xB7}
   - 导出函数索引: 5

2. 通过 AID 查找模块注册表
   module_registry_find_by_aid(vm, target_aid)
   → 返回 module_id = 2

3. 通过 module_id 和函数索引查找 GRT
   grt_index = calculate_grt_index(module_id=2, export_idx=5)
   grt_entry = vm->grt_table[grt_index]
   
4. 从 GRT 条目提取目标地址
   target_module_id = GRT_GET_MODULE_ID(grt_entry)  // = 2
   target_address = GRT_GET_ADDRESS(grt_entry)       // = 0x1234

5. VM 执行器跳转到目标地址执行
   vm_jump_to(target_address)
```

### 模块内引用：函数表 + 偏移量

#### 方法调用：函数表索引

**文件：** `gcos_module_function_table.h` / `.c`

**标准方法固定位置：**

```c
#define FUNC_IDX_INSTALL        0   /* 模块安装方法 */
#define FUNC_IDX_SELECT         1   /* 应用选择方法 */
#define FUNC_IDX_DESELECT       2   /* 应用取消选择方法 */
#define FUNC_IDX_PROCESS        3   /* APDU 处理方法 */
#define FUNC_IDX_UNINSTALL      4   /* 模块卸载方法（可选）*/
#define FUNC_IDX_FIRST_CUSTOM   5   /* 第一个自定义方法 */
```

**函数表结构：**

```c
typedef struct {
    /* Function table header */
    u8 module_aid[16];              /* 模块 AID（用于验证）*/
    u16 function_count;             /* 函数数量 */
    u16 checksum;                   /* 校验和 */
    
    /* Function entries array */
    GCOSFunctionEntry functions[MAX_FUNCTIONS_PER_MODULE];
} GCOSModuleFunctionTable;

typedef struct {
    u16 logical_address;            /* 函数代码的逻辑地址 */
    u8 signature_type;              /* 函数签名类型 */
    u8 parameter_count;             /* 参数数量 */
    u16 reserved;
} GCOSFunctionEntry;
```

**调用示例：**

```c
/* INSTALL 命令处理器调用模块的 install 方法 */
u16 handle_install_command(...) {
    // 1. 找到模块
    GCOSModuleRegistry *reg = module_registry_find_by_aid(vm, &module_aid);
    
    // 2. 获取函数表
    GCOSModuleFunctionTable *func_table = 
        get_function_table_from_flash(reg->function_table_addr);
    
    // 3. 获取 install 方法的地址（固定索引 0）
    u16 install_method_addr = func_table->functions[FUNC_IDX_INSTALL].logical_address;
    
    // 4. 通过 VM 执行器调用
    u16 status = vm_call_method(vm, install_method_addr, install_params, params_len);
    
    return status;
}
```

#### 成员访问：基址 + 偏移量

**原理：** 类似 C 语言的结构体成员访问

```c
/* 编译器生成的代码 */
// C 代码: module_global_var = 42;
// 编译为: STORE_GLOBAL offset=0x10, value=42

/* VM 执行 STORE_GLOBAL 指令 */
void execute_store_global(GCOSVM *vm, u16 offset, u32 value) {
    // 1. 获取当前模块的全局数据基址
    GCOSModuleRegistry *current_module = vm->runtime.current_module;
    u32 base_addr = current_module->global_data_addr;
    
    // 2. 计算绝对地址
    u32 absolute_addr = base_addr + offset;
    
    // 3. 写入 Flash/RAM
    flash_write_u32(absolute_addr, value);
}
```

**成员偏移量表（可选优化）：**

```c
typedef struct {
    u16 member_count;               /* 成员数量 */
    u16 total_data_size;            /* 总数据大小 */
    u16 member_offsets[1];          /* 可变长度数组：每个成员的偏移量 */
} GCOSModuleMemberTable;
```

**用途：**
- 调试：将偏移量映射回变量名
- 反射：运行时查询模块的成员布局
- 序列化：保存/恢复模块状态

---

## ✅ 实施状态

### 已完成

| 组件 | 文件 | 状态 | 说明 |
|------|------|------|------|
| **System Root Header** | `gcos_system_root.h`<br>`gcos_system_root.c` | ✅ 完成 | 固定锚点，256字节，CRC32校验 |
| **Module Function Table** | `gcos_module_function_table.h`<br>`gcos_module_function_table.c` | ✅ 完成 | 标准方法固定位置，函数表管理 |
| **GCOSVM 扩展** | `gcos_vm.h` | ✅ 完成 | 添加 system_root 指针和 flash_size |
| **CMakeLists.txt** | `CMakeLists.txt` | ✅ 完成 | 添加新源文件 |

### 待完成

| 组件 | 优先级 | 说明 |
|------|--------|------|
| **Flash 存储后端集成** | 🔴 高 | 将模拟的 Flash 操作替换为真实的 eFlash API |
| **空闲链表管理** | 🟡 中 | 实现动态内存分配和回收 |
| **SEF 解析增强** | 🟡 中 | 从 SEF 文件提取函数表和成员表信息 |
| **模块加载完善** | 🟡 中 | 集成函数表初始化到 LOAD 命令 |
| **测试用例** | 🟢 低 | 验证 Flash 持久化和引用机制 |

---

## 🚀 下一步计划

### 阶段 6：Flash 存储后端集成

**目标：** 将模拟的 Flash 操作替换为真实的 eFlash API

**任务：**
1. 集成 eFlash-master 库的 API
2. 实现 `flash_read()` / `flash_write()` / `flash_erase()` 包装函数
3. 实现事务性写入（原子性保证）
4. 添加磨损均衡支持

**关键代码：**

```c
/* 从 eFlash 读取数据 */
GCOSResult flash_read(u32 logical_addr, u8 *buffer, u32 length) {
    /* Convert logical address to physical address */
    u32 physical_addr = logical_to_physical(logical_addr);
    
    /* Read from Flash using eFlash API */
    eflash_status_t status = eflash_read(physical_addr, buffer, length);
    
    return (status == EFLASH_OK) ? GCOS_SUCCESS : GCOS_ERROR_FLASH_READ;
}

/* 向 eFlash 写入数据（带事务保护）*/
GCOSResult flash_write_atomic(u32 logical_addr, const u8 *data, u32 length) {
    /* Start transaction */
    transaction_begin();
    
    /* Write to Flash */
    u32 physical_addr = logical_to_physical(logical_addr);
    eflash_status_t status = eflash_write(physical_addr, data, length);
    
    if (status == EFLASH_OK) {
        /* Commit transaction */
        transaction_commit();
        return GCOS_SUCCESS;
    } else {
        /* Abort transaction */
        transaction_abort();
        return GCOS_ERROR_FLASH_WRITE;
    }
}
```

### 阶段 7：空闲链表管理

**目标：** 实现动态内存分配和回收

**任务：**
1. 实现 `GCOSFreeBlock` 结构
2. 实现分配算法（首次适配/最佳适配）
3. 实现空闲块合并
4. 集成到模块加载和应用安装流程

**关键代码：**

```c
/* 分配 Flash 空间 */
u32 flash_allocate(u32 size) {
    GCOSFreeBlock *block = find_best_fit_block(size);
    
    if (block == NULL) {
        return 0;  /* Out of memory */
    }
    
    /* Split block if necessary */
    if (block->size > size + sizeof(GCOSFreeBlock)) {
        create_new_free_block(block->start_addr + size, 
                              block->size - size - sizeof(GCOSFreeBlock));
    }
    
    /* Update free list */
    remove_block_from_free_list(block);
    
    return block->start_addr;
}

/* 释放 Flash 空间 */
void flash_free(u32 addr) {
    GCOSFreeBlock *block = get_block_at_address(addr);
    
    /* Add to free list */
    add_block_to_free_list(block);
    
    /* Merge with adjacent free blocks */
    merge_adjacent_blocks(block);
}
```

### 阶段 8：SEF 解析增强

**目标：** 从 SEF 文件提取完整的函数表和成员表信息

**任务：**
1. 解析 FUNCTION 段提取函数信息
2. 解析 GLOBAL 段提取成员偏移量
3. 生成函数表和成员表
4. 写入 Flash 并更新模块注册表

**SEF 文件格式（参考 COS3 规范）：**

```
SEF File Structure:
  ├── Section 1: FIRST (模块元数据)
  ├── Section 2: IMPORT (导入符号)
  ├── Section 3: FUNCTION (函数定义)
  │     └─ Function 0: install (signature, address)
  │     └─ Function 1: select (signature, address)
  │     └─ Function 2: deselect (signature, address)
  │     └─ Function 3: process (signature, address)
  │     └─ Function 4+: custom functions...
  ├── Section 5: GLOBAL (全局变量)
  │     └─ Variable 0: offset=0x00, size=4
  │     └─ Variable 1: offset=0x04, size=16
  │     └─ ...
  ├── Section 9: CODE (字节码)
  └─ ...
```

---

## 📚 参考资料

1. **cref 实现**
   - `cref/common/memory.h`: OBJ_HEADER 结构定义
   - `cref/common/rootprocess.h`: ROOT 配置和 Flash 布局
   - `cref/driver/tq/drivers/include/flash.h`: Flash API

2. **COS3 规范**
   - 第 7 章：二进制文件和模块
   - 第 8 章：功能要求（安装器、执行器）
   - 附录 F：可加载文件示例

3. **相关标准**
   - GB/T 44901.3-XXXX：卡及身份识别安全设备 片上操作系统
   - ISO/IEC 7816-4：APDU 协议
   - GlobalPlatform Card Specification v2.3.1

---

## 🎓 设计原则总结

1. **固定锚点模式**：System Root Header 永远在 0x0000，作为所有动态结构的根
2. **分层引用**：AID → module_id → function_table → method_address
3. **代码共享**：一个模块加载一次，多个应用实例共享代码
4. **数据隔离**：每个应用实例有独立的全局数据区
5. **紧凑编码**：GRT 使用 32 位紧凑格式节省空间
6. **完整性校验**：CRC32 校验和防止数据损坏
7. **事务保护**：Flash 写入使用事务保证原子性
8. **向后兼容**：版本号支持未来格式升级

---

**文档版本：** 1.0.0  
**创建日期：** 2026-05-09  
**作者：** GCOS VM Development Team
