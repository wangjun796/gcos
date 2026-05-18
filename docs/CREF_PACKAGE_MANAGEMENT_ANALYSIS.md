# Cref 包/模块管理架构深度分析与 GCOS 应用方案

## 📊 核心概念对比

### Cref vs GCOS 术语映射

| Cref 概念 | GCOS 对应概念 | 说明 |
|-----------|--------------|------|
| **Package** | **Module** | 代码和资源的容器单元 |
| Package ID | Module ID/Index | 包的唯一标识符 |
| Package AID | Module AID | 包的 AID 标识 |
| PackageEntry | GCOSModule | 包在 VM 中的运行时表示 |
| Applet | Application Instance | 可执行的应用实例 |
| CAP File | SEF File | 可加载文件格式 |
| LOAD Command | LOAD Command (0xE4) | 加载包到卡中 |
| INSTALL Command | INSTALL Command (0xE6) | 从包创建应用实例 |

---

## 1️⃣ Cref PackageEntry 结构详细分析

### 1.1 PackageEntry 字段定义

根据 `cref/common/objAccess.h` 和 `cref/common/objAccess.c`:

```c
// PackageEntry 对象字段偏移量定义
#define PKG_ENTRY_pkgAID                (PackageEntry_pkgAID<<1)           // jref -> AID 对象引用
#define PKG_ENTRY_importedPackages      (PackageEntry_importedPackages<<1) // jref -> 导入包数组
#define PKG_ENTRY_applets               (PackageEntry_applets<<1)          // jref -> 应用列表
#define PKG_ENTRY_pkgName               (PackageEntry_pkgName<<1)          // jref -> 包名称字符串
#define PKG_ENTRY_pkgMinor              (PackageEntry_pkgMinor<<1)         // u8 -> 次版本号
#define PKG_ENTRY_pkgMajor              (PackageEntry_pkgMajor<<1)         // u8 -> 主版本号
#define PKG_ENTRY_pkgStaticReferenceCount (PackageEntry_pkgStaticReferenceCount<<1) // u16 -> 静态引用计数
#define PKG_ENTRY_importCount           (PackageEntry_importCount<<1)      // u8 -> 导入包数量
#define PKG_ENTRY_appletCount           (PackageEntry_appletCount<<1)      // u8 -> 应用数量
#define PKG_ENTRY_pkgNamelength         (PackageEntry_pkgNamelength<<1)    // u8 -> 包名长度
#define PKG_ENTRY_sdID                  (PackageEntry_sdID<<1)             // u8 -> 安全域 ID
#define PKG_ENTRY_status                (PackageEntry_status<<1)           // u8 -> 包状态

// GP CGM Quota 扩展字段 (可选)
#define PKG_ENTRY_CGM_dwCodeSpaceC6     // 代码空间配额
#define PKG_ENTRY_CGM_dwDataSpaceC8     // 数据空间配额
#define PKG_ENTRY_CGM_dwRamSpaceC7      // RAM 空间配额
#define PKG_ENTRY_CGM_dwUsedFlashSpace  // 已用 Flash 空间
#define PKG_ENTRY_CGM_dwUsedRamSpace    // 已用 RAM 空间
```

### 1.2 PackageEntry 初始化流程

在 `cref/common/objAccess.c` 的 `newPackageEntry()` 函数中:

```c
jref newPackageEntry(jref aid, u8 minor, u8 major, u8 sdID) {
    // 1. 分配 PackageEntry 对象
    obj = prepareNewObj(COM_SUN_JAVACARD_IMPL_PKG_ID, OBJECT_PackageEntry_OFFSET);
    
    // 2. 初始化所有字段为 0
    os_memset(objectEntry, 0, sizeof(objectEntry));
    
    // 3. 设置关键字段
    objectEntry[PKG_ENTRY_pkgAID>>1] = htoj16(aid);              // AID 引用
    objectEntry[PKG_ENTRY_pkgMinor>>1] = htoj16(B2S(minor));     // 次版本
    objectEntry[PKG_ENTRY_pkgMajor>>1] = htoj16(B2S(major));     // 主版本
    objectEntry[PKG_ENTRY_sdID>>1] = htoj16(B2S(sdID));          // 安全域 ID
    objectEntry[PKG_ENTRY_status>>1] = htoj16(B2S(1));           // 状态 = LOADED
    
    // 4. 存储到 EEPROM
    storeArray(get_array_data(locateObjectInMemory(obj)), 
               (u8_ptr)objectEntry, PKG_ENTRY_CGM_DATA);
    
    return obj;
}
```

---

## 2️⃣ Cref 应用安装两阶段流程

### 2.1 Phase 1: LOAD 命令 (加载包)

**目的**: 将 CAP 文件解析并加载到卡中,创建 PackageEntry

**LOAD 状态机** (`cref/native/native_install.c`):

```c
// 加载器状态定义
#define INSTALLER_STATE_READY     0x00   // 就绪状态
#define INSTALLER_STATE_LOADING   0x02   // 加载中
#define INSTALLER_STATE_CREATING  0x04   // 创建应用中

// CAP 组件类型 (按顺序加载)
#define ORDER_HEADER        1   // Header 组件 (魔数、版本、标志)
#define ORDER_DIRECTORY     2   // Directory 组件 (组件索引表)
#define ORDER_IMPORT        3   // Import 组件 (依赖包列表)
#define ORDER_APPLET        4   // Applet 组件 (应用 AID 列表)
#define ORDER_CLASS         5   // Class 组件 (类定义)
#define ORDER_METHOD        6   // Method 组件 (方法字节码)
#define ORDER_STATICFIELD   7   // StaticField 组件 (静态变量)
#define ORDER_EXPORT        8   // Export 组件 (导出符号)
#define ORDER_CONSTANTPOOL  9   // ConstantPool 组件 (常量池)
#define ORDER_REFERENCELOCATION 10 // ReferenceLocation 组件 (重定位信息)
#define ORDER_DESCRIPTOR    11  // Descriptor 组件 (描述符)
#define ORDER_DEBUG         12  // Debug 组件 (调试信息,可选)
```

**LOAD 处理流程**:

```
1. INSTALL FOR LOAD (P1=0x00)
   ├─ 验证 Package AID 是否重复
   ├─ 分配新的 Package ID
   ├─ 保存 SD ID (安全域)
   └─ 进入 LOADING 状态

2. LOAD BLOCKS (多次 APDU)
   ├─ 解析 Header 组件
   │  ├─ 验证魔数 (0xDECAFED)
   │  ├─ 检查版本兼容性
   │  └─ 读取标志位 (是否有 Applet、整数支持等)
   ├─ 解析 Directory 组件
   │  └─ 建立组件索引表
   ├─ 解析 Import 组件
   │  ├─ 验证依赖包是否存在
   │  └─ 记录导入包数量和版本
   ├─ 解析 Applet 组件
   │  └─ 记录应用 AID 列表和数量
   ├─ 解析 Class/Method/StaticField
   │  ├─ 分配内存空间
   │  ├─ 写入字节码和数据
   │  └─ 处理重定位
   └─ 解析 Export/ConstantPool
      └─ 建立导出符号表

3. FINALIZE LOAD
   ├─ 创建 PackageEntry 对象
   ├─ 设置状态为 LOADED
   ├─ 提交事务 (Transaction Commit)
   └─ 返回成功 (0x9000)
```

**关键数据结构**:

```c
// LOAD 阶段全局变量
uint8_t g_newPackageIdentifier;     // 新分配的 Package ID
uint8_t g_AID[CAP_MAX_AID_LENGTH];  // Package AID
uint8_t g_capFlags;                 // CAP 文件标志
uint8_t g_capMinor, g_capMajor;     // CAP 版本
uint8_t g_pkgMinor, g_pkgMajor;     // Package 版本
uint8_t f_pkgCount;                 // 导入包数量
uint8_t f_aidCount;                 // 应用 AID 数量
jref g_pkgEntry;                    // PackageEntry 对象引用
memref f_objectArrayData;           // 对象数组数据指针
```

---

### 2.2 Phase 2: INSTALL 命令 (创建应用实例)

**目的**: 从已加载的 Package 创建 Applet 实例

**INSTALL 状态机**:

```c
// INSTALL 子命令
#define INSTALL_FOR_LOAD    0x02   // 为 LOAD 准备 (Phase 1 前置)
#define INSTALL_FOR_INSTALL 0x04   // 创建应用实例 (Phase 2)

// 应用安装状态
#define APPLET_STATE_COUNT      0x00   // 等待应用数量
#define APPLET_STATE_AID        0x01   // 等待应用 AID
#define APPLET_STATE_PRIVILEGE  0x02   // 等待权限字节
#define APPLET_STATE_PARAMETERS 0x03   // 等待安装参数
```

**INSTALL 处理流程**:

```
1. INSTALL FOR INSTALL (P1=0x02)
   ├─ 验证 Package 是否已加载
   ├─ 读取应用数量
   └─ 进入 APPLET_STATE_COUNT 状态

2. 对每个应用:
   ├─ 读取应用 AID
   │  ├─ 验证 AID 长度 (5-16 字节)
   │  └─ 检查 AID 是否重复
   ├─ 读取权限字节 (privByte1/2/3)
   ├─ 读取安装参数 (install parameters)
   └─ 调用 Applet.install() 方法
      ├─ 传递 AID、偏移量、长度
      ├─ 执行 Java 层初始化
      └─ 返回状态字

3. 创建 AppTableEntry
   ├─ 分配应用 ID
   ├─ 设置 Applet 对象引用
   ├─ 设置 AID 引用
   ├─ 设置权限字节
   ├─ 设置安全域 ID
   ├─ 设置生命周期状态 (INSTALLED)
   └─ 添加到应用表

4. 提交事务
   └─ 返回成功 (0x9000)
```

**关键代码片段** (`cref/native/native_install.c`):

```c
// 添加应用到包
void addAppletInfo(Object_Info_ptr pkgEntry, jref aid, u8 installOffset) {
    // 1. 获取包的应用列表
    jref appletsArray = loadReferenceMember(pkgEntry, PKG_ENTRY_applets);
    
    // 2. 增加应用计数
    u8 appCount = loadByteMember(pkgEntry, PKG_ENTRY_appletCount);
    storeByteMember(pkgEntry, PKG_ENTRY_appletCount, appCount + 1);
    
    // 3. 在应用列表中添加 AID 引用
    storeReferenceMember(appletsArray, appCount * 2, aid);
    
    // 4. 创建 AppTableEntry
    jref appEntry = newAppTableEntry(aid, installOffset, sdID);
    
    // 5. 设置权限和状态
    storeByteMember(appEntry, APP_ENTRY_privByte1, privilege1);
    storeByteMember(appEntry, APP_ENTRY_privByte2, privilege2);
    storeByteMember(appEntry, APP_ENTRY_privByte3, privilege3);
    storeByteMember(appEntry, APP_ENTRY_status, APPLICATION_INSTALLED);
}
```

---

## 3️⃣ Cref 包依赖管理机制

### 3.1 Import 组件结构

CAP 文件的 Import 组件包含:

```c
typedef struct {
    u8 package_count;               // 导入包数量
    struct {
        u8 aid_length;              // 包 AID 长度
        u8 aid[16];                 // 包 AID
        u8 minor_version;           // 需要的次版本
        u8 major_version;           // 需要的主版本
    } imported_packages[];
} ImportComponent;
```

### 3.2 依赖验证流程

```c
// 验证导入包是否存在且版本兼容
boolean verifyImport(u8* aid, u8 minor, u8 major) {
    // 1. 遍历已加载的包列表
    for (i = 0; i < f_pkgTableSize; i++) {
        Object_Info_ptr pkgEntry = getPackageEntry(i);
        
        // 2. 比较 AID
        if (compareAid(pkgEntry, aid)) {
            // 3. 检查版本兼容性
            u8 pkgMinor = loadByteMember(pkgEntry, PKG_ENTRY_pkgMinor);
            u8 pkgMajor = loadByteMember(pkgEntry, PKG_ENTRY_pkgMajor);
            
            if (pkgMajor == major && pkgMinor >= minor) {
                return TRUE;  // 版本兼容
            }
        }
    }
    return FALSE;  // 未找到或版本不兼容
}
```

---

## 4️⃣ Cref 资源配额管理 (CGM - Card Global Memory)

### 4.1 Package 级别配额

当启用 `GP_CGM_QUOTA` 时,PackageEntry 包含:

```c
#define PKG_ENTRY_CGM_dwCodeSpaceC6     // 代码空间配额 (Flash)
#define PKG_ENTRY_CGM_dwDataSpaceC8     // 数据空间配额 (EEPROM)
#define PKG_ENTRY_CGM_dwRamSpaceC7      // RAM 空间配额
#define PKG_ENTRY_CGM_dwUsedFlashSpace  // 已用 Flash 空间
#define PKG_ENTRY_CGM_dwUsedRamSpace    // 已用 RAM 空间
```

### 4.2 应用级别配额

AppTableEntry 包含:

```c
#define APP_ENTRY_dwFlashQuotaSpaceC8   // Flash 配额
#define APP_ENTRY_dwRamQuotaSpaceC7     // RAM 配额
#define APP_ENTRY_usedFlashQuotaSpace   // 已用 Flash
#define APP_ENTRY_usedRamQuotaSpace     // 已用 RAM
```

---

## 5️⃣ GCOS 当前实现与 Cref 对比

### 5.1 GCOSModule 结构 (当前)

```c
struct GCOSModule {
    GCOSAID module_aid;             /* 模块AID */
    GCOSModuleType type;            /* 模块类型 (应用/库) */
    u32 version;                    /* 版本号 */
    
    /* 数据区 */
    u8 *global_data;                /* 模块全局数据 (易失性) */
    u32 global_data_size;
    const u8 *readonly_data;        /* 只读数据 (非易失性) */
    u32 readonly_data_size;
    u8 *domain_data;                /* 域数据 (非易失性) */
    u32 domain_data_size;
    
    /* 代码区 */
    const u8 *code;                 /* 程序代码 */
    u32 code_size;
    
    /* 函数表 */
    GCOSFunctionHeader *functions;
    u16 function_count;
    
    /* 导入/导出表 */
    void *import_table;
    u16 import_count;
    void *export_table;
    u16 export_count;
    
    /* 应用实例列表 */
    GCOSAppInstance *app_instances[MAX_APPS_PER_MODULE];
    u8 app_instance_count;
    
    bool loaded;
    bool initialized;
};
```

### 5.2 缺失的关键字段

| Cref 字段 | GCOS 当前状态 | 建议操作 |
|-----------|--------------|----------|
| **package_id** (内部 ID) | ❌ 缺失 | ✅ 添加 `module_id` (u8) |
| **minor_version / major_version** | ⚠️ 合并为 u32 version | ✅ 拆分为 u8 minor/major |
| **imported_packages** (依赖列表) | ⚠️ 有 import_table 但无详细信息 | ✅ 添加 `GCOSImportInfo[]` |
| **applet_count** | ✅ 有 app_instance_count | 保持 |
| **sdID** (安全域 ID) | ❌ 缺失 | ✅ 添加到 GCOSModule |
| **status** (包状态) | ⚠️ 有 loaded bool | ✅ 改为枚举状态 |
| **static_reference_count** | ❌ 缺失 | ⚠️ 可选 (用于 GC) |
| **quota fields** (资源配额) | ❌ 缺失 | ⚠️ Phase 2 添加 |
| **package_name** | ❌ 缺失 | ⚠️ 可选 (调试用) |

---

## 6️⃣ GCOS 改进方案

### 6.1 增强 GCOSModule 结构

```c
/**
 * @brief Module lifecycle states (similar to cref PackageEntry status)
 */
typedef enum {
    MODULE_NOT_LOADED = 0x00,       /* 未加载 */
    MODULE_LOADED = 0x01,           /* 已加载但未验证 */
    MODULE_VERIFIED = 0x02,         /* 已验证,可创建应用 */
    MODULE_ERROR = 0xFF             /* 加载错误 */
} GCOSModuleState;

/**
 * @brief Import dependency information (similar to cref Import component)
 */
typedef struct {
    GCOSAID package_aid;            /* 依赖包的 AID */
    u8 minor_version;               /* 需要的次版本 */
    u8 major_version;               /* 需要的主版本 */
    bool resolved;                  /* 是否已解析 */
} GCOSImportInfo;

/**
 * @brief Enhanced module structure (aligned with cref PackageEntry)
 */
struct GCOSModule {
    /* === 基本信息 === */
    u8 module_id;                   /* ⭐ 新增: 模块内部 ID (类似 cref package_id) */
    GCOSAID module_aid;             /* 模块 AID */
    GCOSModuleType type;            /* 模块类型 */
    
    /* ⭐ 新增: 分离的版本号 (类似 cref pkgMinor/pkgMajor) */
    u8 version_major;               /* 主版本号 */
    u8 version_minor;               /* 次版本号 */
    
    /* ⭐ 新增: 模块状态 (替代简单的 loaded bool) */
    GCOSModuleState state;          /* 模块状态 */
    
    /* ⭐ 新增: 安全域 ID (类似 cref sdID) */
    u8 security_domain_id;          /* 所属安全域 ID (0xFF = ISD) */
    
    /* === 依赖管理 (类似 cref importedPackages) === */
    GCOSImportInfo imports[MAX_IMPORTS];  /* ⭐ 新增: 导入依赖列表 */
    u8 import_count;                      /* 导入包数量 */
    
    /* === 数据区 === */
    u8 *global_data;
    u32 global_data_size;
    const u8 *readonly_data;
    u32 readonly_data_size;
    u8 *domain_data;
    u32 domain_data_size;
    
    /* === 代码区 === */
    const u8 *code;
    u32 code_size;
    
    /* === 函数表 === */
    GCOSFunctionHeader *functions;
    u16 function_count;
    
    /* === 导出表 === */
    void *export_table;
    u16 export_count;
    
    /* === 应用实例列表 === */
    GCOSAppInstance *app_instances[MAX_APPS_PER_MODULE];
    u8 app_instance_count;
    
    /* === 资源配额 (Phase 2 可选) === */
    // u32 code_quota;              /* 代码空间配额 */
    // u32 data_quota;              /* 数据空间配额 */
    // u32 ram_quota;               /* RAM 配额 */
    // u32 used_code;               /* 已用代码空间 */
    // u32 used_data;               /* 已用数据空间 */
    // u32 used_ram;                /* 已用 RAM */
};
```

### 6.2 实现 LOAD 命令状态机

```c
/**
 * @brief LOAD command state machine (similar to cref installer)
 */
typedef enum {
    LOAD_STATE_IDLE = 0x00,           /* 空闲 */
    LOAD_STATE_INITIALIZATION = 0x01, /* 初始化阶段 (INSTALL FOR LOAD) */
    LOAD_STATE_LOADING_BLOCKS = 0x02, /* 加载数据块 (LOAD BLOCKS) */
    LOAD_STATE_FINALIZATION = 0x03,   /* 完成阶段 (FINALIZE) */
    LOAD_STATE_ERROR = 0xFF           /* 错误状态 */
} GCOSLoadState;

/**
 * @brief LOAD context (maintains state across multiple APDUs)
 */
typedef struct {
    GCOSLoadState state;              /* 当前状态 */
    u8 target_module_id;              /* 目标模块 ID */
    GCOSAID package_aid;              /* 包 AID */
    u8 version_major;                 /* 主版本 */
    u8 version_minor;                 /* 次版本 */
    u8 sd_id;                         /* 安全域 ID */
    
    u32 total_size;                   /* 总大小 */
    u32 loaded_size;                  /* 已加载大小 */
    
    u8 *buffer;                       /* 临时缓冲区 */
    u32 buffer_size;                  /* 缓冲区大小 */
    
    u8 import_count;                  /* 导入包数量 */
    GCOSImportInfo imports[MAX_IMPORTS]; /* 导入列表 */
    
    u8 app_count;                     /* 应用数量 */
    GCOSAID app_aids[MAX_APPS];       /* 应用 AID 列表 */
} GCOSLoadContext;
```

### 6.3 LOAD 命令处理流程

```c
/**
 * @brief Handle LOAD command (INS=0xE4)
 * 
 * Implements three-phase loading similar to cref:
 * 1. INSTALL FOR LOAD (P1=0x00) - Initialize loading
 * 2. LOAD BLOCKS (P1=0x01) - Load data blocks
 * 3. FINALIZE (P1=0x02) - Finalize and create module
 */
static u16 isd_handler_load(GCOSAppInstance *app,
                            const u8 *apdu,
                            u16 apdu_len,
                            u8 *response,
                            u16 *resp_len) {
    u8 p1 = apdu[2];  // P1: Sub-command
    u8 p2 = apdu[3];  // P2: Parameters
    
    switch (p1) {
        case 0x00:  // INSTALL FOR LOAD
            return handle_install_for_load(apdu, apdu_len, response, resp_len);
        
        case 0x01:  // LOAD BLOCKS
            return handle_load_blocks(apdu, apdu_len, response, resp_len);
        
        case 0x02:  // FINALIZE
            return handle_finalize_load(apdu, apdu_len, response, resp_len);
        
        default:
            return 0x6A86;  // SW_INCORRECT_P1P2
    }
}

/**
 * @brief Phase 1: INSTALL FOR LOAD
 */
static u16 handle_install_for_load(const u8 *apdu, u16 apdu_len,
                                   u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    // Parse TLV data from APDU
    // Expected format:
    // Tag 0x4F: Package AID
    // Tag 0xC4: Load parameters (version, SD ID, etc.)
    
    // 1. Extract Package AID
    GCOSAID pkg_aid;
    if (!parse_tlv_aid(apdu, apdu_len, 0x4F, &pkg_aid)) {
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    // 2. Check for duplicate package
    if (module_find_by_aid(vm, &pkg_aid) != NULL) {
        return 0x6A89;  // SW_FILE_ALREADY_EXISTS
    }
    
    // 3. Allocate new module ID
    u8 module_id = allocate_module_id(vm);
    if (module_id == 0xFF) {
        return 0x6A84;  // SW_MEMORY_FAILURE
    }
    
    // 4. Initialize load context
    g_load_context.state = LOAD_STATE_INITIALIZATION;
    g_load_context.target_module_id = module_id;
    g_load_context.package_aid = pkg_aid;
    g_load_context.loaded_size = 0;
    
    printf("[LOAD] INSTALL FOR LOAD: Module ID=%u, AID=", module_id);
    print_aid(&pkg_aid);
    printf("\n");
    
    return 0x9000;
}

/**
 * @brief Phase 2: LOAD BLOCKS
 */
static u16 handle_load_blocks(const u8 *apdu, u16 apdu_len,
                              u8 *response, u16 *resp_len) {
    // Append data to buffer
    // Parse SEF sections as they arrive
    // Update loaded_size
    
    if (g_load_context.state != LOAD_STATE_LOADING_BLOCKS) {
        return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
    }
    
    // Copy block data
    memcpy(g_load_context.buffer + g_load_context.loaded_size,
           &apdu[5], apdu_len - 5);
    g_load_context.loaded_size += (apdu_len - 5);
    
    printf("[LOAD] Block loaded: %u/%u bytes\n",
           g_load_context.loaded_size, g_load_context.total_size);
    
    return 0x9000;
}

/**
 * @brief Phase 3: FINALIZE LOAD
 */
static u16 handle_finalize_load(const u8 *apdu, u16 apdu_len,
                                u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    if (g_load_context.state != LOAD_STATE_LOADING_BLOCKS) {
        return 0x6985;
    }
    
    // 1. Parse complete SEF file
    GCOSSefFile sef;
    if (!parse_sef_file(g_load_context.buffer, g_load_context.loaded_size, &sef)) {
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    // 2. Verify imports (check dependencies)
    for (u8 i = 0; i < sef.import_count; i++) {
        if (!verify_import(vm, &sef.imports[i])) {
            printf("[LOAD] ERROR: Import verification failed\n");
            return 0x6A88;  // SW_REFERENCED_DATA_NOT_FOUND
        }
    }
    
    // 3. Create module instance
    GCOSModule *module = &vm->modules[g_load_context.target_module_id];
    module->module_id = g_load_context.target_module_id;
    module->module_aid = g_load_context.package_aid;
    module->version_major = sef.version_major;
    module->version_minor = sef.version_minor;
    module->security_domain_id = g_load_context.sd_id;
    module->state = MODULE_LOADED;
    
    // 4. Copy code and data
    module->code = allocate_code_memory(sef.code_size);
    memcpy((u8*)module->code, sef.code_section, sef.code_size);
    module->code_size = sef.code_size;
    
    // 5. Setup function table
    module->functions = sef.functions;
    module->function_count = sef.function_count;
    
    // 6. Setup import/export tables
    module->imports = g_load_context.imports;
    module->import_count = g_load_context.import_count;
    module->export_table = sef.export_table;
    module->export_count = sef.export_count;
    
    // 7. Mark as loaded
    module->loaded = true;
    vm->module_count++;
    
    printf("[LOAD] Module loaded successfully: ID=%u, Functions=%u\n",
           module->module_id, module->function_count);
    
    // Reset load context
    g_load_context.state = LOAD_STATE_IDLE;
    
    return 0x9000;
}
```

---

## 7️⃣ 实施优先级建议

### 🔴 Phase 1 (立即实施)

1. ✅ **添加 module_id 字段** - 内部标识符,类似 cref package_id
2. ✅ **拆分 version 为 major/minor** - 对齐 cref 版本管理
3. ✅ **添加 GCOSModuleState 枚举** - 替代简单的 loaded bool
4. ✅ **添加 security_domain_id** - 支持多安全域
5. ✅ **实现 LOAD 状态机** - 三阶段加载流程

### 🟡 Phase 2 (后续优化)

6. ⚠️ **完善 import 依赖管理** - 详细的依赖验证
7. ⚠️ **添加资源配额字段** - CGM 支持
8. ⚠️ **实现包删除功能** - DELETE 命令
9. ⚠️ **添加包状态查询** - GET STATUS 命令

### 🟢 Phase 3 (高级功能)

10. 💡 **实现 DAP 验证** - 数字签名验证
11. 💡 **支持增量加载** - 断点续传
12. 💡 **包版本升级** - 热更新机制

---

## 8️⃣ 总结

### Cref 设计精髓

1. **两阶段安装**: LOAD (加载包) + INSTALL (创建实例)
2. **状态机驱动**: 明确的状态转换,支持流式处理
3. **依赖管理**: Import 组件确保包兼容性
4. **对象引用模型**: PackageEntry/AppTableEntry 作为运行时表示
5. **事务保护**: 所有修改在事务中完成,失败可回滚

### GCOS 适配策略

1. **保留简化设计**: 直接使用 C 结构,无需 Java 对象模型
2. **借鉴状态机**: 采用 cref 的三阶段 LOAD 流程
3. **增强元数据**: 添加 module_id、version、sdID 等字段
4. **模块化扩展**: 通过 import 表管理依赖关系
5. **渐进式实施**: Phase 1 核心功能 → Phase 2 优化 → Phase 3 高级特性

---

**参考文档**:
- Java Card 2.2.1 Runtime Environment Specification
- GlobalPlatform Card Specification v2.2.1
- cref source code: `native/native_install.c`, `common/objAccess.c`
