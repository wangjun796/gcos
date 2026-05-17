# GCOS Flash 系统管理数据架构设计

## 🎯 核心问题

智能卡系统中，所有持久化数据都存储在 Flash 中，但面临以下挑战：

1. **动态数据的地址管理**：应用表、模块表、GRT 等数据结构在 Flash 中的逻辑地址是动态分配的
2. **固定锚点需求**：需要一些固定的逻辑地址作为"根指针"，指向这些动态结构
3. **首次上电初始化**：第一次上电时需要构建已知逻辑地址的数据结构
4. **模块引用机制**：模块间通过 AID 引用，模块内通过方法表和偏移引用

## 📐 整体架构设计

### Flash 逻辑地址空间布局

```
┌─────────────────────────────────────────────────┐
│         GCOS Flash Logical Address Space        │
├─────────────────────────────────────────────────┤
│                                                  │
│  0x0000 - 0x00FF: System Root Area (Fixed)      │
│    ├─ 0x0000: System Header (magic + version)   │
│    ├─ 0x0010: Module Registry Table Pointer     │
│    ├─ 0x0014: Application Table Pointer         │
│    ├─ 0x0018: Global Reference Table Pointer    │
│    ├─ 0x001C: Free List Head Pointer            │
│    └─ 0x0020-0x00FF: Reserved for future use    │
│                                                  │
│  0x0100 - 0x0FFF: System Management Tables      │
│    ├─ Module Registry Table (dynamic)           │
│    ├─ Application Instance Table (dynamic)      │
│    └─ Global Reference Table (dynamic)          │
│                                                  │
│  0x1000 - 0xFFFF: User Data Area                │
│    ├─ Module Code/Data (XIP or loaded)          │
│    ├─ Application Persistent Data               │
│    └─ Free Space (managed by free list)         │
│                                                  │
└─────────────────────────────────────────────────┘
```

### 关键设计原则

1. **System Root Area (固定)**：前 256 字节永远不变，包含所有管理表的指针
2. **间接寻址**：通过 Root Area 中的指针找到实际的管理表位置
3. **动态分配**：管理表可以在 User Data Area 中移动/扩展
4. **空闲链表**：跟踪 Flash 中的可用空间

---

## 🔧 数据结构设计

### 1. System Root Header (固定地址 0x0000)

```c
/**
 * @brief System Root Header - Fixed at logical address 0x0000
 * 
 * This is the anchor point for all system management data.
 * Never moves, always at the same logical address.
 */
typedef struct {
    u32 magic_number;           /* 0x47434F53 = "GCOS" */
    u16 version_major;          /* Major version */
    u16 version_minor;          /* Minor version */
    
    /* Pointers to management tables (logical addresses) */
    u32 module_registry_addr;   /* Logical address of module registry table */
    u32 app_table_addr;         /* Logical address of application instance table */
    u32 grt_addr;               /* Logical address of global reference table */
    u32 free_list_head_addr;    /* Logical address of free list head */
    
    /* System configuration */
    u16 max_modules;            /* Maximum number of modules */
    u16 max_apps;               /* Maximum number of applications */
    u16 max_grt_entries;        /* Maximum GRT entries */
    
    /* Checksum for integrity */
    u32 header_checksum;        /* CRC32 of header */
} GCOSSystemRootHeader;

/* Fixed logical address for root header */
#define GCOS_SYSTEM_ROOT_ADDR       0x0000U
#define GCOS_SYSTEM_ROOT_SIZE       256U  /* Reserve 256 bytes */
```

---

### 2. Module Registry Table (动态)

```c
/**
 * @brief Module Registry Entry in Flash
 * 
 * Stored in Flash, pointed to by System Root Header.
 * Each entry represents a loaded module.
 */
typedef struct {
    u8 module_id;               /* Module ID (0xFF = invalid) */
    u8 state;                   /* Module state (LOADED/VERIFIED/etc) */
    
    /* Module identity */
    GCOSAID module_aid;         /* Module AID (max 16 bytes) */
    u32 module_version;         /* Module version */
    
    /* Code location */
    u32 code_logical_addr;      /* Logical address of code in Flash */
    u32 code_size;              /* Code size in bytes */
    
    /* Function table (like cref's method table) */
    u32 function_table_addr;    /* Logical address of function table */
    u16 function_count;         /* Number of functions */
    
    /* Standard methods (fixed positions in function table) */
    u16 install_func_idx;       /* Index of install() in function table */
    u16 select_func_idx;        /* Index of select() in function table */
    u16 deselect_func_idx;      /* Index of deselect() in function table */
    u16 process_func_idx;       /* Index of process() in function table */
    
    /* Import/Export tables */
    u32 import_table_addr;      /* Logical address of import table */
    u8 import_count;            /* Number of imports */
    
    u32 export_table_addr;      /* Logical address of export table */
    u16 export_count;           /* Number of exports */
    
    /* Instance tracking */
    u8 instance_count;          /* Number of app instances using this module */
    u32 instance_list_addr;     /* Logical address of instance ID array */
    
    /* Global data template */
    u32 global_data_template_addr; /* Logical address of global data template */
    u32 global_data_size;       /* Template size */
    
    /* Metadata */
    u32 load_timestamp;         /* When module was loaded */
    u32 entry_checksum;         /* CRC32 of entry */
} GCOSModuleRegistryEntry;

/**
 * @brief Module Registry Table Header
 */
typedef struct {
    u16 entry_count;            /* Number of valid entries */
    u16 max_entries;            /* Maximum capacity */
    u32 next_entry_offset;      /* Offset for next allocation */
    u32 table_checksum;         /* CRC32 of table */
} GCOSModuleRegistryTableHeader;
```

---

### 3. Application Instance Table (动态)

```c
/**
 * @brief Application Instance Entry in Flash
 */
typedef struct {
    u8 app_id;                  /* Application ID (0xFF = invalid) */
    u8 lifecycle_state;         /* APPLICATION_INSTALLED/SELECTABLE/etc */
    
    /* Application identity */
    GCOSAID app_aid;            /* Application AID */
    
    /* Module reference */
    u8 module_id;               /* Module ID this app belongs to */
    u32 module_registry_addr;   /* Back-pointer to module registry entry */
    
    /* Method pointers (indices into module's function table) */
    u16 install_method_idx;     /* install() method index */
    u16 select_method_idx;      /* select() method index */
    u16 deselect_method_idx;    /* deselect() method index */
    u16 process_method_idx;     /* process() method index */
    
    /* Application data areas */
    u32 persistent_data_addr;   /* Logical address of persistent data */
    u32 persistent_data_size;   /* Persistent data size */
    
    u32 volatile_data_addr;     /* Logical address of volatile data (RAM shadow) */
    u32 volatile_data_size;     /* Volatile data size */
    
    /* Security context */
    u8 security_domain_id;      /* Owning security domain */
    u8 privilege_flags[3];      /* Privilege bytes */
    
    /* Runtime state */
    u8 is_selected;             /* Currently selected? */
    u8 selected_channel;        /* Channel that selected this app */
    
    /* Metadata */
    u32 install_timestamp;      /* When app was installed */
    u32 entry_checksum;         /* CRC32 of entry */
} GCOSAppInstanceEntry;

/**
 * @brief Application Instance Table Header
 */
typedef struct {
    u16 entry_count;            /* Number of valid entries */
    u16 max_entries;            /* Maximum capacity */
    u32 next_entry_offset;      /* Offset for next allocation */
    u32 table_checksum;         /* CRC32 of table */
} GCOSAppInstanceTableHeader;
```

---

### 4. Global Reference Table (动态)

```c
/**
 * @brief Global Reference Table Entry (4 bytes, packed)
 * 
 * Same as current implementation, but stored in Flash.
 */
typedef struct {
    u32 packed_data;  /* High 8 bits = module_id, Low 24 bits = logical_address */
} GCOSGlobalRefEntryFlash;

/**
 * @brief Global Reference Table Header
 */
typedef struct {
    u16 entry_count;            /* Number of valid entries */
    u16 max_entries;            /* Maximum capacity */
    u32 next_free_index;        /* Next free slot index */
    u32 table_checksum;         /* CRC32 of table */
} GCOSGlobalRefTableHeader;
```

---

### 5. Free List (动态)

```c
/**
 * @brief Free List Entry
 * 
 * Tracks available space in Flash User Data Area.
 * Implemented as a linked list.
 */
typedef struct {
    u32 start_logical_addr;     /* Start of free block */
    u32 size_bytes;             /* Size of free block */
    u32 next_free_addr;         /* Logical address of next free entry (0 = end) */
} GCOSFreeListEntry;

/**
 * @brief Free List Header
 */
typedef struct {
    u32 head_entry_addr;        /* Logical address of first free entry */
    u32 total_free_bytes;       /* Total free space */
    u32 largest_block_bytes;    /* Largest contiguous block */
    u32 list_checksum;          /* CRC32 of list */
} GCOSFreeListHeader;
```

---

## 🔗 模块引用机制设计

### 模块间引用：通过 AID

```
Module A wants to call Module B's function:

1. Module A's import table contains:
   - Imported module AID: B's AID
   - Imported function index: N

2. At link time (LOAD command):
   - Resolve Module B's AID → Module B's module_id
   - Create GRT entry: {module_id: B, address: func_N_addr}
   - Store GRT index in Module A's code

3. At runtime:
   - Execute: CALL.GLOBAL <grt_index>
   - VM looks up GRT[grt_index]
   - Gets {module_id: B, address: func_N_addr}
   - Jumps to Module B's function
```

---

### 模块内引用：方法表 + 偏移

#### 方法表设计（类似 cref）

```c
/**
 * @brief Module Function Table Entry
 * 
 * Each module has a function table with fixed positions for standard methods.
 */
typedef struct {
    u16 function_index;         /* Function index (0-based) */
    u32 code_offset;            /* Offset from module code base */
    u8 param_count;             /* Number of parameters */
    u8 return_type;             /* Return type code */
    u16 local_var_size;         /* Local variable space size */
} GCOSFunctionTableEntry;

/**
 * @brief Standard Method Indices (fixed positions)
 * 
 * First 4 entries in function table are reserved for standard methods.
 */
#define FUNC_IDX_INSTALL        0   /* install(byte[], byte, byte) */
#define FUNC_IDX_SELECT         1   /* select() */
#define FUNC_IDX_DESELECT       2   /* deselect() */
#define FUNC_IDX_PROCESS        3   /* process(APDU) */
#define FUNC_IDX_FIRST_CUSTOM   4   /* First custom function */
```

#### 成员变量引用：直接偏移

```c
/**
 * Module global variables are accessed via direct offset from global data base.
 * 
 * Example:
 *   Global data base address: 0x1000
 *   Variable 'counter' at offset: 0x00
 *   Variable 'buffer' at offset: 0x04
 * 
 * Access: LOAD.GLOBAL 0x00  → reads from addr 0x1000 + 0x00
 */
```

---

## 🚀 首次上电初始化流程

```c
/**
 * @brief Initialize system on first power-up
 * 
 * Called when magic_number != 0x47434F53
 */
GCOSResult gcos_system_first_init(void) {
    GCOSSystemRootHeader root;
    
    // 1. Initialize root header
    memset(&root, 0, sizeof(GCOSSystemRootHeader));
    root.magic_number = 0x47434F53;  // "GCOS"
    root.version_major = 1;
    root.version_minor = 0;
    
    // 2. Allocate management tables in User Data Area
    root.module_registry_addr = allocate_flash_block(sizeof(GCOSModuleRegistryTableHeader) + 
                                                      MAX_MODULES * sizeof(GCOSModuleRegistryEntry));
    root.app_table_addr = allocate_flash_block(sizeof(GCOSAppInstanceTableHeader) + 
                                                MAX_APPS * sizeof(GCOSAppInstanceEntry));
    root.grt_addr = allocate_flash_block(sizeof(GCOSGlobalRefTableHeader) + 
                                          MAX_GLOBAL_REFS * sizeof(GCOSGlobalRefEntryFlash));
    root.free_list_head_addr = allocate_flash_block(sizeof(GCOSFreeListHeader));
    
    // 3. Initialize module registry table
    init_module_registry_table(root.module_registry_addr);
    
    // 4. Initialize application table
    init_app_instance_table(root.app_table_addr);
    
    // 5. Initialize GRT
    init_global_ref_table(root.grt_addr);
    
    // 6. Initialize free list (remaining space)
    init_free_list(root.free_list_head_addr, 
                   FLASH_USER_DATA_START, 
                   FLASH_USER_DATA_SIZE);
    
    // 7. Calculate checksum and write to Flash
    root.header_checksum = calculate_crc32(&root, sizeof(root) - 4);
    flash_write(GCOS_SYSTEM_ROOT_ADDR, &root, sizeof(root));
    
    return GCOS_SUCCESS;
}
```

---

## 📝 运行时数据访问

### 读取模块注册表

```c
GCOSModuleRegistryEntry* get_module_entry(u8 module_id) {
    // 1. Read root header
    GCOSSystemRootHeader root;
    flash_read(GCOS_SYSTEM_ROOT_ADDR, &root, sizeof(root));
    
    // 2. Validate
    if (root.magic_number != 0x47434F53) {
        return NULL;  // System not initialized
    }
    
    // 3. Read module registry table header
    GCOSModuleRegistryTableHeader table_hdr;
    flash_read(root.module_registry_addr, &table_hdr, sizeof(table_hdr));
    
    // 4. Search for module_id
    u32 entry_addr = root.module_registry_addr + sizeof(table_hdr);
    for (u16 i = 0; i < table_hdr.entry_count; i++) {
        GCOSModuleRegistryEntry entry;
        flash_read(entry_addr + i * sizeof(entry), &entry, sizeof(entry));
        
        if (entry.module_id == module_id && entry.state != MODULE_INVALID) {
            return &entry;  // Found
        }
    }
    
    return NULL;  // Not found
}
```

---

## 🎯 优势总结

### 1. 固定锚点
- System Root Header 永远在 0x0000
- 所有其他表通过指针间接访问
- 首次上电只需写入一次 Root Header

### 2. 灵活性
- 管理表可以移动/扩展
- 只需更新 Root Header 中的指针
- 支持在线升级和重构

### 3. 完整性保护
- 每个表都有 CRC32 校验
- Root Header 也有校验
- 启动时验证所有数据结构

### 4. 空间管理
- 空闲链表跟踪可用空间
- 支持动态分配和回收
- 防止碎片化（可选合并策略）

### 5. 模块引用清晰
- 模块间：AID → module_id → GRT → 地址
- 模块内：方法表索引 / 全局数据偏移
- 标准方法固定位置（0-3）

---

## 🔨 下一步实施计划

1. **实现 System Root Header 读写接口**
2. **实现 Flash 空闲链表管理器**
3. **改造模块注册表为 Flash 存储**
4. **改造应用实例表为 Flash 存储**
5. **改造 GRT 为 Flash 存储**
6. **实现首次上电初始化流程**
7. **实现运行时数据访问 API**
8. **添加完整性检查和恢复机制**
