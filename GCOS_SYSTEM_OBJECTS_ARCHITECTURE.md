# GCOS 基于 eflash 对象头表的系统管理数据架构

## 🎯 核心设计理念

### 问题重新分析

您指出的关键问题：
1. **eflash 前 12 页已预留**：LPN 0-7（对象头表）+ LPN 8-11（空闲链表）
2. **GCOS 应该使用对象管理系统**：通过固定对象 ID 锚定管理数据
3. **避免与 eflash 冲突**：不能使用逻辑地址 0x0000 作为固定锚点

### 正确的设计方案

**基于对象 ID 的锚定机制：**

```
eflash 对象头表（LPN 0-7，共 232 个对象槽位）
┌─────────────────────────────────────┐
│ Obj ID 0: Reserved (eflash internal)│
│ Obj ID 1: GCOS Module Registry      │ ← 模块管理表
│ Obj ID 2: GCOS App Instance Table   │ ← 应用管理表
│ Obj ID 3: GCOS GRT Table            │ ← 全局引用表
│ Obj ID 4: GCOS Free List            │ ← GCOS 空闲链表
│ Obj ID 5: GCOS System Config        │ ← 系统配置
│ Obj ID 6-231: Available for apps    │
└─────────────────────────────────────┘

每个对象的结构：
  Object Header (16 bytes, in OOT):
    - pkg_id: 标识符（如 0x4743 = "GC"）
    - class_id: 类型（如 0x0001 = Module Registry）
    - type: OBJ_TYPE_NORMAL
    - body_addr: 数据体的逻辑地址
    - body_size: 数据体大小
  
  Data Body (in Flash user data area):
    - 实际的管理数据结构
```

---

## 📐 完整架构设计

### 1. 对象 ID 分配方案

| 对象 ID | pkg_id | class_id | 用途 | 说明 |
|---------|--------|----------|------|------|
| 0 | - | - | eflash 内部保留 | 不使用 |
| **1** | 0x4743 | 0x0001 | **模块注册表** | Module Registry Table |
| **2** | 0x4743 | 0x0002 | **应用实例表** | Application Instance Table |
| **3** | 0x4743 | 0x0003 | **全局引用表** | Global Reference Table (GRT) |
| **4** | 0x4743 | 0x0004 | **GCOS 空闲链表** | GCOS Free List |
| **5** | 0x4743 | 0x0005 | **系统配置** | System Configuration |
| **6** | 0x4743 | 0x0006 | **符号解析表** | Symbol Resolution Table |
| **7-231** | - | - | 应用/模块动态分配 | 由 LOAD/INSTALL 命令分配 |

**pkg_id 含义：**
- `0x4743` = "GC" (GCOS)
- 所有 GCOS 系统对象使用相同的 pkg_id

**class_id 含义：**
- `0x0001-0x0006`: 系统管理类
- `0x0100+`: 应用类
- `0x0200+`: 模块类

### 2. 对象数据结构定义

#### 对象 1：模块注册表 (Module Registry Table)

```c
/* Object ID: 1 */
/* pkg_id: 0x4743, class_id: 0x0001 */

typedef struct {
    /* Header */
    u32 magic;                      /* 0x4D4F4452 = "MODR" */
    u16 version;                    /* Version 1.0 */
    u16 module_count;               /* Current module count */
    u16 max_modules;                /* Maximum modules (e.g., 16) */
    u16 checksum;                   /* CRC16 */
    
    /* Module entries (variable length) */
    GCOSModuleRegistry modules[1];  /* Array of module entries */
} GCOS_ModuleRegistryObject;

/* 每个模块条目（与之前设计相同）*/
typedef struct {
    u8 module_id;                   /* Module ID (0-15) */
    bool is_loaded;                 /* Is loaded flag */
    GCOSAID module_aid;             /* Module AID (up to 16 bytes) */
    u8 module_version[2];           /* Module version */
    
    u8 state;                       /* MODULE_NOT_LOADED / LOADED / ACTIVE */
    
    /* Code area */
    u32 code_base;                  /* Code logical address */
    u32 code_size;                  /* Code size */
    
    /* Function table */
    u32 function_table_addr;        /* Function table logical address */
    
    /* Global data area */
    u32 global_data_addr;           /* Global data logical address */
    u32 global_data_size;           /* Global data size */
    
    /* Instance tracking */
    u8 instance_count;              /* Number of app instances */
    u8 instance_ids[MAX_APPS_PER_MODULE]; /* Instance ID list */
} GCOSModuleRegistry;
```

#### 对象 2：应用实例表 (Application Instance Table)

```c
/* Object ID: 2 */
/* pkg_id: 0x4743, class_id: 0x0002 */

typedef struct {
    /* Header */
    u32 magic;                      /* 0x41505054 = "APPT" */
    u16 version;                    /* Version 1.0 */
    u16 app_count;                  /* Current app count */
    u16 max_apps;                   /* Maximum apps (e.g., 32) */
    u16 checksum;                   /* CRC16 */
    
    /* Application entries (variable length) */
    GCOSAppInstance apps[1];        /* Array of app instances */
} GCOS_AppInstanceObject;

/* 应用实例结构（与之前设计相同）*/
typedef struct {
    u8 app_id;                      /* Application ID */
    GCOSAID app_aid;                /* Application AID */
    
    u8 module_id;                   /* Owning module ID */
    u32 module_obj_id;              /* ⭐ Object ID of module registry entry */
    
    u8 lifecycle;                   /* APPLICATION_SELECTABLE, etc. */
    
    /* Global data for this instance */
    u32 global_data_addr;           /* Instance-specific global data address */
    u32 global_data_size;           /* Data size */
    
    /* Selection state */
    bool selected;                  /* Is currently selected */
} GCOSAppInstance;
```

#### 对象 3：全局引用表 (GRT)

```c
/* Object ID: 3 */
/* pkg_id: 0x4743, class_id: 0x0003 */

typedef struct {
    /* Header */
    u32 magic;                      /* 0x47525430 = "GRT0" */
    u16 version;                    /* Version 1.0 */
    u16 capacity;                   /* GRT capacity (e.g., 64) */
    u16 used_count;                 /* Currently used entries */
    u16 checksum;                   /* CRC16 */
    
    /* GRT entries (compact 32-bit format) */
    GCOSGlobalRefEntry entries[1];  /* Array of GRT entries */
} GCOS_GRTObject;

/* GRT 条目（紧凑格式）*/
typedef struct {
    u32 packed_data;                /* 8-bit module_id + 24-bit address */
} GCOSGlobalRefEntry;

/* 宏定义 */
#define GRT_GET_MODULE_ID(entry)    ((u8)(((entry).packed_data >> 24) & 0xFF))
#define GRT_GET_ADDRESS(entry)      ((u32)((entry).packed_data & 0x00FFFFFF))
#define GRT_SET_ENTRY(entry, addr, mod) \
    ((entry).packed_data = (((u32)(mod) << 24) | ((addr) & 0x00FFFFFF)))
#define GRT_IS_VALID(entry)         (GRT_GET_MODULE_ID(entry) != 0xFF)
#define GRT_INVALIDATE(entry)       ((entry).packed_data = 0xFF000000U)
```

#### 对象 4：GCOS 空闲链表 (Free List)

```c
/* Object ID: 4 */
/* pkg_id: 0x4743, class_id: 0x0004 */

typedef struct {
    /* Header */
    u32 magic;                      /* 0x4652454C = "FREL" */
    u16 version;                    /* Version 1.0 */
    u16 node_count;                 /* Number of free nodes */
    u32 total_free_bytes;           /* Total free bytes */
    u16 checksum;                   /* CRC16 */
    
    /* Free nodes (linked list) */
    GCOSFreeNode nodes[1];          /* Array of free nodes */
} GCOS_FreeListObject;

typedef struct {
    u32 start_addr;                 /* Start logical address */
    u32 size;                       /* Block size in bytes */
    u32 next_node_offset;           /* Offset to next node (0 = end) */
} GCOSFreeNode;
```

#### 对象 5：系统配置 (System Configuration)

```c
/* Object ID: 5 */
/* pkg_id: 0x4743, class_id: 0x0005 */

typedef struct {
    /* Header */
    u32 magic;                      /* 0x53595343 = "SYSC" */
    u16 version;                    /* Version 1.0 */
    u16 flags;                      /* System flags */
    
    /* System parameters */
    u32 max_modules;                /* Maximum modules */
    u32 max_apps;                   /* Maximum applications */
    u32 max_grt_entries;            /* Maximum GRT entries */
    u32 flash_total_size;           /* Total Flash size */
    
    /* Object ID references (⭐ KEY DESIGN) */
    u16 module_registry_obj_id;     /* Object ID of Module Registry (should be 1) */
    u16 app_instance_obj_id;        /* Object ID of App Instance Table (should be 2) */
    u16 grt_obj_id;                 /* Object ID of GRT (should be 3) */
    u16 free_list_obj_id;           /* Object ID of Free List (should be 4) */
    
    /* Runtime state */
    u32 module_count;               /* Current module count */
    u32 app_count;                  /* Current app count */
    u32 grt_used_count;             /* GRT used entries */
    
    /* Checksum */
    u32 checksum;                   /* CRC32 */
} GCOS_SystemConfigObject;

/* Flags */
#define GCOS_SYS_FLAG_INITIALIZED   0x0001
#define GCOS_SYS_FLAG_PERSISTENT    0x0002
#define GCOS_SYS_FLAG_RECOVERY_MODE 0x0004
```

### 3. 初始化流程

#### 首次上电初始化

```c
GCOSResult gcos_system_init(GCOSVM *vm) {
    printf("[GCOS_INIT] Initializing system...\n");
    
    /* Step 1: Initialize eflash FTL */
    eflash_ftl_init();
    
    /* Step 2: Check if system objects exist */
    obj_header_t sys_config_hdr;
    int ret = eflash_ftl_obj_get_header(GCOS_OBJ_ID_SYS_CONFIG, &sys_config_hdr);
    
    if (ret != 0 || sys_config_hdr.pkg_id != 0x4743) {
        /* First boot: create system objects */
        printf("[GCOS_INIT] First boot detected, creating system objects...\n");
        return gcos_system_objects_create(vm);
    }
    
    /* Step 3: Load system configuration */
    printf("[GCOS_INIT] Loading system configuration from object %u...\n", 
           GCOS_OBJ_ID_SYS_CONFIG);
    return gcos_system_objects_load(vm);
}

GCOSResult gcos_system_objects_create(GCOSVM *vm) {
    uint32_t logic_addr;
    obj_header_t hdr;
    
    /* Create Object 5: System Configuration */
    printf("[GCOS_INIT] Creating System Config object (ID=5)...\n");
    
    GCOS_SystemConfigObject sys_config;
    memset(&sys_config, 0, sizeof(sys_config));
    sys_config.magic = 0x53595343;
    sys_config.version = 0x0100;
    sys_config.flags = GCOS_SYS_FLAG_INITIALIZED;
    sys_config.max_modules = GCOS_DEFAULT_MAX_MODULES;
    sys_config.max_apps = GCOS_DEFAULT_MAX_APPS;
    sys_config.max_grt_entries = GCOS_DEFAULT_MAX_GRT;
    sys_config.flash_total_size = vm->flash_size;
    
    /* Set object ID references */
    sys_config.module_registry_obj_id = GCOS_OBJ_ID_MODULE_REGISTRY;
    sys_config.app_instance_obj_id = GCOS_OBJ_ID_APP_INSTANCE;
    sys_config.grt_obj_id = GCOS_OBJ_ID_GRT;
    sys_config.free_list_obj_id = GCOS_OBJ_ID_FREE_LIST;
    
    /* Calculate checksum */
    sys_config.checksum = gcos_calc_crc32(&sys_config, sizeof(sys_config) - 4);
    
    /* Allocate space and write to Flash */
    eflash_mgr_alloc(sizeof(sys_config), &logic_addr);
    eflash_ftl_write(logic_addr, (uint8_t *)&sys_config, sizeof(sys_config));
    
    /* Set object header */
    memset(&hdr, 0, sizeof(hdr));
    hdr.pkg_id = 0x4743;
    hdr.class_id = 0x0005;
    hdr.type = OBJ_TYPE_NORMAL;
    hdr.body_addr = logic_addr;
    hdr.body_size = sizeof(sys_config);
    eflash_ftl_obj_set_header(GCOS_OBJ_ID_SYS_CONFIG, &hdr);
    
    /* Create Object 1: Module Registry */
    printf("[GCOS_INIT] Creating Module Registry object (ID=1)...\n");
    gcos_create_module_registry_object(vm);
    
    /* Create Object 2: App Instance Table */
    printf("[GCOS_INIT] Creating App Instance Table object (ID=2)...\n");
    gcos_create_app_instance_object(vm);
    
    /* Create Object 3: GRT */
    printf("[GCOS_INIT] Creating GRT object (ID=3)...\n");
    gcos_create_grt_object(vm);
    
    /* Create Object 4: Free List */
    printf("[GCOS_INIT] Creating Free List object (ID=4)...\n");
    gcos_create_free_list_object(vm);
    
    printf("[GCOS_INIT] All system objects created successfully\n");
    return GCOS_SUCCESS;
}
```

#### 正常启动加载

```c
GCOSResult gcos_system_objects_load(GCOSVM *vm) {
    obj_header_t hdr;
    uint32_t logic_addr;
    
    /* Load System Configuration (Object 5) */
    printf("[GCOS_LOAD] Loading System Config from object %u...\n", 
           GCOS_OBJ_ID_SYS_CONFIG);
    
    if (eflash_ftl_obj_get_header(GCOS_OBJ_ID_SYS_CONFIG, &hdr) != 0) {
        printf("[GCOS_LOAD] ERROR: System Config object not found\n");
        return GCOS_ERROR_OBJECT_NOT_FOUND;
    }
    
    GCOS_SystemConfigObject *sys_config = malloc(sizeof(GCOS_SystemConfigObject));
    logic_addr = hdr.body_addr;
    eflash_ftl_read(logic_addr, (uint8_t *)sys_config, hdr.body_size);
    
    /* Validate */
    if (!gcos_validate_system_config(sys_config)) {
        printf("[GCOS_LOAD] ERROR: System Config validation failed\n");
        free(sys_config);
        return GCOS_ERROR_VALIDATION_FAILED;
    }
    
    vm->system_config = sys_config;
    
    /* Load Module Registry (Object 1) */
    printf("[GCOS_LOAD] Loading Module Registry from object %u...\n",
           sys_config->module_registry_obj_id);
    gcos_load_module_registry(vm, sys_config->module_registry_obj_id);
    
    /* Load App Instance Table (Object 2) */
    printf("[GCOS_LOAD] Loading App Instance Table from object %u...\n",
           sys_config->app_instance_obj_id);
    gcos_load_app_instance_table(vm, sys_config->app_instance_obj_id);
    
    /* Load GRT (Object 3) */
    printf("[GCOS_LOAD] Loading GRT from object %u...\n",
           sys_config->grt_obj_id);
    gcos_load_grt(vm, sys_config->grt_obj_id);
    
    printf("[GCOS_LOAD] All system objects loaded successfully\n");
    return GCOS_SUCCESS;
}
```

### 4. 对象管理与 API

#### 对象 ID 常量定义

```c
/* gcos_system_objects.h */

/* System Object IDs (reserved) */
#define GCOS_OBJ_ID_MODULE_REGISTRY     1   /* Module Registry Table */
#define GCOS_OBJ_ID_APP_INSTANCE        2   /* Application Instance Table */
#define GCOS_OBJ_ID_GRT                 3   /* Global Reference Table */
#define GCOS_OBJ_ID_FREE_LIST           4   /* GCOS Free List */
#define GCOS_OBJ_ID_SYS_CONFIG          5   /* System Configuration */
#define GCOS_OBJ_ID_SYMBOL_TABLE        6   /* Symbol Resolution Table */

/* System Object pkg_id and class_id */
#define GCOS_PKG_ID                     0x4743  /* "GC" */
#define GCOS_CLASS_MODULE_REGISTRY      0x0001
#define GCOS_CLASS_APP_INSTANCE         0x0002
#define GCOS_CLASS_GRT                  0x0003
#define GCOS_CLASS_FREE_LIST            0x0004
#define GCOS_CLASS_SYS_CONFIG           0x0005
#define GCOS_CLASS_SYMBOL_TABLE         0x0006

/* Magic numbers */
#define GCOS_MAGIC_MODR                 0x4D4F4452  /* "MODR" */
#define GCOS_MAGIC_APPT                 0x41505054  /* "APPT" */
#define GCOS_MAGIC_GRT0                 0x47525430  /* "GRT0" */
#define GCOS_MAGIC_FREL                 0x4652454C  /* "FREL" */
#define GCOS_MAGIC_SYSC                 0x53595343  /* "SYSC" */
```

#### 核心 API

```c
/* Create system objects on first boot */
GCOSResult gcos_system_objects_create(GCOSVM *vm);

/* Load system objects from Flash */
GCOSResult gcos_system_objects_load(GCOSVM *vm);

/* Save system objects to Flash */
GCOSResult gcos_system_objects_save(GCOSVM *vm);

/* Get object by ID */
void* gcos_object_get_data(u16 obj_id, u32 *out_size);

/* Update object data */
GCOSResult gcos_object_update_data(u16 obj_id, const void *data, u32 size);

/* Allocate new object for application/module */
GCOSResult gcos_object_allocate(u16 pkg_id, u16 class_id, u32 size, 
                                u16 *out_obj_id, uint32_t *out_logic_addr);

/* Free object */
GCOSResult gcos_object_free(u16 obj_id);
```

---

## 🔗 与 cref 的对比

| 特性 | cref | GCOS (新设计) | 说明 |
|------|------|---------------|------|
| 对象头表 | OBJ_HEADER (16字节) | obj_header_t (16字节) | ✅ 相同 |
| 根指针 | ROOT_BUFFER 固定偏移 | 对象 ID 1-6 | ✅ 更灵活 |
| 持久化对象 | PERSIST_OBJECT_HEAD | GCOS 系统对象 | ✅ 统一模型 |
| 空闲链表 | FREE_NODE | GCOS_FreeListObject | ✅ 独立管理 |
| 查找方式 | 直接索引 | eflash_ftl_obj_get_header() | ✅ 抽象层 |
| 扩展性 | 固定布局 | 对象链式扩展 | ✅ 更好 |

---

## 🎯 设计优势

### 1. 与 eflash 完美集成

- ✅ 不占用 eflash 预留的系统页（LPN 0-11）
- ✅ 使用标准的对象管理机制
- ✅ 支持对象头表的扩展（最多 232 + 116*N 个对象）
- ✅ 自动磨损均衡（通过 FTL 的 Radix Tree）

### 2. 灵活的锚定机制

```
传统方式（错误）：
  逻辑地址 0x0000 → System Root Header
  ❌ 可能与 eflash 的对象头表冲突

新方式（正确）：
  对象 ID 5 → System Configuration
  对象 ID 1 → Module Registry
  对象 ID 2 → App Instance Table
  ✅ 通过 eflash API 访问，无冲突
```

### 3. 统一的对象模型

- 所有数据都是对象（系统对象 + 应用对象 + 模块对象）
- 统一的创建/加载/保存/删除 API
- 统一的完整性校验（CRC）
- 统一的扩展机制

### 4. 清晰的层次结构

```
Level 1: System Configuration (Obj ID 5)
  ├─ Points to Module Registry (Obj ID 1)
  ├─ Points to App Instance Table (Obj ID 2)
  ├─ Points to GRT (Obj ID 3)
  └─ Points to Free List (Obj ID 4)

Level 2: Management Tables
  ├─ Module Registry → Array of modules
  ├─ App Instance Table → Array of apps
  └─ GRT → Array of cross-module references

Level 3: Application/Module Data
  ├─ Module Code Areas (dynamic objects)
  ├─ Module Function Tables (dynamic objects)
  └─ Application Global Data (dynamic objects)
```

---

## 📊 内存布局示意

```
Flash Physical Layout:
┌──────────────────────────────────┐
│ LPN 0-7:  Object Header Table    │ ← eflash 管理
│   - Obj 0: Reserved              │
│   - Obj 1: Module Registry       │ ← pkg_id=0x4743, class_id=0x0001
│   - Obj 2: App Instance Table    │ ← pkg_id=0x4743, class_id=0x0002
│   - Obj 3: GRT                   │ ← pkg_id=0x4743, class_id=0x0003
│   - Obj 4: Free List             │ ← pkg_id=0x4743, class_id=0x0004
│   - Obj 5: System Config         │ ← pkg_id=0x4743, class_id=0x0005
│   - Obj 6+: Dynamic allocation   │
├──────────────────────────────────┤
│ LPN 8-11: Free List              │ ← eflash 管理
├──────────────────────────────────┤
│ LPN 12+: User Data Area          │
│   - Obj 1 body: Module Registry data  │
│   - Obj 2 body: App Instance data     │
│   - Obj 3 body: GRT data              │
│   - Obj 4 body: Free List data        │
│   - Obj 5 body: System Config data    │
│   - Module code areas (dynamic)       │
│   - Application data (dynamic)        │
└──────────────────────────────────┘
```

---

## 🚀 实施计划

### 阶段 1：对象管理 API 封装

**文件：** `gcos_system_objects.h` / `gcos_system_objects.c`

**任务：**
1. 定义对象 ID 常量和数据结构
2. 封装 eflash API（get_header, set_header, read, write）
3. 实现对象创建/加载/保存/删除
4. 添加 CRC 校验和完整性验证

### 阶段 2：系统对象初始化

**任务：**
1. 实现首次上电初始化流程
2. 创建 6 个系统对象
3. 设置初始值和交叉引用
4. 写入 Flash 并验证

### 阶段 3：集成到 VM 生命周期

**任务：**
1. 在 `gcos_vm_create()` 中调用 `gcos_system_init()`
2. 在 `gcos_vm_destroy()` 中保存系统对象
3. 更新 GCOSVM 结构，添加对象 ID 引用
4. 测试重启后的数据恢复

### 阶段 4：模块加载与应用安装集成

**任务：**
1. LOAD 命令：分配新对象存储模块代码
2. INSTALL 命令：分配新对象存储应用数据
3. DELETE 命令：释放对象并回收空间
4. 更新模块注册表和应用实例表对象

---

## 💡 关键设计决策

### 为什么选择对象 ID 而不是固定逻辑地址？

1. **避免冲突**：eflash 已经使用了 LPN 0-11，我们不能假设逻辑地址 0x0000 可用
2. **灵活性**：对象可以动态分配到任何物理位置（FTL 负责映射）
3. **可扩展性**：对象头表支持扩展（最多数千个对象）
4. **统一管理**：所有数据都通过对象 API 访问，简化代码
5. **磨损均衡**：FTL 自动处理物理块的磨损均衡

### 为什么需要独立的系统配置对象（Obj ID 5）？

1. **自描述**：包含其他对象的 ID 引用，形成完整的元数据
2. **版本控制**：可以独立升级系统配置格式
3. **快速定位**：只需知道 Obj ID 5，就能找到所有其他对象
4. **完整性校验**：单独的 CRC32 保护系统配置

### 如何处理对象扩展？

eflash 支持对象头表扩展：
- 基础：8 页，232 个对象
- 扩展：每 4 页增加 116 个对象
- 最大：16 级扩展，共 2088 个对象

GCOS 可以使用这个机制动态扩展系统对象数量。

---

**文档版本：** 1.0.0  
**创建日期：** 2026-05-09  
**作者：** GCOS VM Development Team  
**参考：** eflash-master 架构、cref 对象管理、COS3 规范
