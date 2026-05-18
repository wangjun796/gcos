# GCOS 系统管理数据 Flash 持久化实施方案

## 📋 实施概览

基于 cref 设计和 COS3 规范，实现完整的 Flash 系统管理数据架构。

## 🎯 核心设计原则

### 1. 固定锚点（System Root Header）
- **位置**：逻辑地址 0x0000（永远不变）
- **大小**：256 字节
- **内容**：指向所有动态管理表的指针
- **首次上电**：写入一次，之后只更新指针

### 2. 动态管理表
- **模块注册表**：存储模块元数据和函数表指针
- **应用实例表**：存储应用实例状态和数据区指针
- **全局引用表（GRT）**：存储跨模块引用
- **空闲链表**：跟踪 Flash 可用空间

### 3. 模块引用机制
- **模块间**：AID → module_id → GRT → 目标地址
- **模块内方法**：函数表索引（前4个为标准方法）
- **模块内成员**：全局数据基址 + 偏移

---

## 🔧 实施步骤

### 阶段 1：System Root Header 实现 ✅

**文件**：`gcos_system_root.h/c`

**功能**：
- [x] 定义 System Root Header 结构
- [x] 实现首次上电初始化
- [x] 实现读写接口
- [x] 实现完整性校验（CRC32）

**关键代码**：
```c
typedef struct {
    u32 magic_number;           // 0x47434F53
    u32 module_registry_addr;   // Pointer to module table
    u32 app_table_addr;         // Pointer to app table
    u32 grt_addr;               // Pointer to GRT
    u32 free_list_head_addr;    // Pointer to free list
    // ... more fields
} GCOSSystemRootHeader;
```

---

### 阶段 2：Flash 空闲链表管理器

**文件**：`gcos_flash_allocator.h/c`

**功能**：
- 管理 Flash User Data Area 的空闲空间
- 支持动态分配和回收
- 维护空闲块链表

**数据结构**：
```c
typedef struct {
    u32 start_addr;     // Start of free block
    u32 size_bytes;     // Size of block
    u32 next_addr;      // Next free block (0 = end)
} GCOSFreeBlock;
```

**API**：
```c
u32 flash_alloc(u32 size_bytes);      // Allocate block
void flash_free(u32 addr);             // Free block
u32 flash_get_free_space(void);        // Get total free space
```

---

### 阶段 3：模块注册表 Flash 化

**改造**：将 `GCOSModuleRegistry` 从 RAM 迁移到 Flash

**新结构**：
```c
typedef struct {
    u8 module_id;
    GCOSAID module_aid;
    u32 code_logical_addr;     // Flash address of code
    u32 function_table_addr;   // Flash address of function table
    
    // Standard method indices (fixed positions)
    u16 install_func_idx;      // Index 0
    u16 select_func_idx;       // Index 1
    u16 deselect_func_idx;     // Index 2
    u16 process_func_idx;      // Index 3
    
    u8 instance_count;
    // ... more fields
} GCOSModuleRegistryEntryFlash;
```

**函数表设计**（参考 cref）：
```c
typedef struct {
    u16 func_index;            // Function index
    u32 code_offset;           // Offset from code base
    u8 param_count;
    u8 return_type;
} GCOSFunctionTableEntry;

// Standard method positions (first 4 entries)
#define FUNC_IDX_INSTALL    0
#define FUNC_IDX_SELECT     1
#define FUNC_IDX_DESELECT   2
#define FUNC_IDX_PROCESS    3
```

---

### 阶段 4：应用实例表 Flash 化

**改造**：将 `GCOSAppInstance` 从 RAM 迁移到 Flash

**新结构**：
```c
typedef struct {
    u8 app_id;
    GCOSAID app_aid;
    u8 module_id;              // Reference to module
    
    // Method indices (into module's function table)
    u16 install_method_idx;
    u16 select_method_idx;
    u16 deselect_method_idx;
    u16 process_method_idx;
    
    u32 persistent_data_addr;  // Flash address
    u32 persistent_data_size;
    
    u8 lifecycle_state;
    // ... more fields
} GCOSAppInstanceEntryFlash;
```

---

### 阶段 5：GRT Flash 持久化

**现有**：GRT 已在 Flash 中（通过 eflash）

**改进**：
- 使用 System Root Header 中的 `grt_addr` 指针
- 添加表头结构（entry_count, checksum）
- 优化写入策略（批量更新）

---

### 阶段 6：运行时访问 API

**功能**：提供统一的运行时数据访问接口

**示例**：
```c
// Get module entry
GCOSModuleRegistryEntryFlash* get_module_by_id(u8 module_id) {
    GCOSSystemRootHeader root;
    gcos_system_root_read(&root);
    
    // Read module registry table
    // Search for module_id
    // Return pointer to entry
}

// Call module's process() method
u16 call_app_process(u8 app_id, const u8 *apdu, ...) {
    GCOSAppInstanceEntryFlash *app = get_app_by_id(app_id);
    GCOSModuleRegistryEntryFlash *module = get_module_by_id(app->module_id);
    
    // Get process() function from module's function table
    u16 func_idx = module->process_func_idx;
    GCOSFunctionTableEntry func_entry;
    read_function_table(module->function_table_addr, func_idx, &func_entry);
    
    // Execute function
    return execute_function(module->code_logical_addr + func_entry.code_offset, ...);
}
```

---

### 阶段 7：首次上电初始化流程

**流程**：
```c
GCOSResult gcos_vm_init(GCOSVM *vm) {
    // 1. Check if system is initialized
    if (!gcos_system_is_initialized()) {
        // First power-up: initialize everything
        gcos_system_root_init();
    }
    
    // 2. Read system root header
    GCOSSystemRootHeader root;
    gcos_system_root_read(&root);
    
    // 3. Load management tables into RAM cache
    load_module_registry_to_ram(root.module_registry_addr);
    load_app_table_to_ram(root.app_table_addr);
    load_grt_to_ram(root.grt_addr);
    
    // 4. Initialize VM structures
    init_vm_from_flash_data(vm);
    
    return GCOS_SUCCESS;
}
```

---

### 阶段 8：完整性检查和恢复

**功能**：
- 启动时验证所有表的 CRC32
- 检测到损坏时尝试恢复
- 日志记录错误信息

**实现**：
```c
bool validate_system_integrity(void) {
    GCOSSystemRootHeader root;
    gcos_system_root_read(&root);
    
    if (!gcos_system_root_validate(&root)) {
        log_error("Root header corrupted!");
        return false;
    }
    
    if (!validate_module_registry(root.module_registry_addr)) {
        log_error("Module registry corrupted!");
        return false;
    }
    
    // ... validate other tables
    
    return true;
}
```

---

## 📊 地址映射示例

### 场景：调用应用的 process() 方法

```
1. SELECT app with AID A0.00.00.00.01.02.03.01
   ↓
2. Find app in App Instance Table (Flash @ 0x0100)
   - app_id = 1
   - module_id = 0
   - process_method_idx = 3
   ↓
3. Find module in Module Registry Table (Flash @ 0x0200)
   - module_id = 0
   - function_table_addr = 0x1000
   - code_logical_addr = 0x2000
   ↓
4. Read function table entry @ index 3
   - code_offset = 0x0100
   ↓
5. Calculate target address
   - target = code_logical_addr + code_offset
   - target = 0x2000 + 0x0100 = 0x2100
   ↓
6. Execute bytecode @ 0x2100
```

---

## 🎯 优势总结

### 1. 清晰的层次结构
```
System Root Header (Fixed @ 0x0000)
  ├─→ Module Registry Table (@ 0x0100)
  ├─→ App Instance Table (@ 0x0200)
  ├─→ Global Ref Table (@ 0x0300)
  └─→ Free List (@ 0x0400)
```

### 2. 灵活的动态分配
- 管理表可以移动/扩展
- 只需更新 Root Header 指针
- 支持在线升级

### 3. 高效的模块引用
- 模块间：AID 解析（安装时完成）
- 模块内：直接索引/偏移（运行时快速）
- 标准方法固定位置（0-3）

### 4. 完整性保障
- 每个表都有 CRC32
- 启动时全面验证
- 支持错误恢复

---

## 🚀 下一步行动

由于这是一个庞大的工程，建议按以下优先级实施：

**高优先级**（核心功能）：
1. ✅ System Root Header 实现
2. ⏳ Flash 空闲链表管理器
3. ⏳ 模块注册表 Flash 化
4. ⏳ 应用实例表 Flash 化

**中优先级**（性能优化）：
5. ⏳ GRT 优化
6. ⏳ 运行时访问 API
7. ⏳ 缓存机制（RAM shadow）

**低优先级**（高级特性）：
8. ⏳ 完整性检查和恢复
9. ⏳ 在线升级支持
10. ⏳ 碎片整理

---

## 📝 注意事项

1. **Flash 写入次数限制**：使用写缓冲和批量更新策略
2. **掉电保护**：关键操作使用事务机制
3. **空间碎片**：定期整理空闲链表
4. **向后兼容**：版本号管理，支持旧格式升级

---

**这个设计完全参考了 cref 的架构，并针对 GCOS 的智能卡环境进行了优化！**
