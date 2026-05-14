# GCOS VM 应用管理方案

**基于 COS3 规范和 cref 参考实现的深度分析**

**日期**: 2026-05-11  
**版本**: 1.0

---

## 🎯 核心设计理念

### 关键洞察

你的理解完全正确：

1. **GCOS 自身相当于安全域功能** - GCOS VM 本身就是 ISD（Initial Security Domain）
2. **每个应用有自己的 APDU 命令表** - 应用级别的命令分发
3. **APDU 命令表应该与应用实例关联** - 面向对象的设计
4. **gcos_vm_process_apdu 的第一个参数应该是 Applet 实例** - 而不是 VM

---

## 📋 问题分析

### 当前设计的问题

```c
// 当前的设计（有问题）
u16 gcos_vm_process_apdu(GCOSVM *vm, const u8 *apdu, u16 apdu_len,
                         u8 *response, u16 *resp_len);

// 问题：
// 1. 只有一个全局的 apdu_command_table
// 2. 所有应用共享同一个命令表
// 3. 无法实现应用级别的命令隔离
// 4. SELECT 后不知道应该调用哪个应用的 handler
```

### cref 的设计（正确）

cref 中每个应用实例（Applet）都有：
- 独立的 AID
- 独立的生命周期状态
- **独立的 APDU 处理能力**（通过 install() 方法注册）
- 应用表（theAppTable）管理所有应用

```c
// cref 的应用表结构
typedef struct {
    w_memref theApplet;         // Applet 对象引用
    w_memref theAID;            // AID
    u8 status;                  // 生命周期状态
    u8 type;                    // 应用类型 (ISD/SSD/Applet)
    u8 privByte1/2/3;          // 特权字节
    // ... 其他字段
} AppEntry;

// 应用选择流程
selectApp(appID) {
    appEntry = getAppTableEntry(appID);
    theApplet = loadReferenceMember(appEntry, APP_ENTRY_theApplet);
    
    // 调用 Applet 的 process() 方法处理 APDU
    call_virtual_method(theApplet, METHOD_process, apdu);
}
```

---

## ✅ 正确的应用管理方案

### 1. 架构设计

```
┌─────────────────────────────────────────────┐
│           GCOS VM (相当于 ISD)               │
│                                             │
│  ┌───────────────────────────────────────┐  │
│  │     Application Manager               │  │
│  │                                       │  │
│  │  App Table:                           │  │
│  │  ┌─────────────────────────────┐     │  │
│  │  │ App[0] = ISD (预装)         │     │  │
│  │  │   - AID: A000000151000000   │     │  │
│  │  │   - State: SELECTABLE       │     │  │
│  │  │   - Handler: isd_handlers[] │     │  │
│  │  └─────────────────────────────┘     │  │
│  │  ┌─────────────────────────────┐     │  │
│  │  │ App[1] = Payment App        │     │  │
│  │  │   - AID: A000000001000001   │     │  │
│  │  │   - State: SELECTABLE       │     │  │
│  │  │   - Handler: pay_handlers[] │     │  │
│  │  └─────────────────────────────┘     │  │
│  │  ┌─────────────────────────────┐     │  │
│  │  │ App[2] = ID Card App        │     │  │
│  │  │   - AID: A000000002000001   │     │  │
│  │  │   - State: INSTALLED        │     │  │
│  │  │   - Handler: id_handlers[]  │     │  │
│  │  └─────────────────────────────┘     │  │
│  └───────────────────────────────────────┘  │
│                                             │
│  Selected App: App[1] (Payment)             │
│  Current Channel: 0                         │
└──────────────────┬──────────────────────────┘
                   │ APDU
                   ▼
┌─────────────────────────────────────────────┐
│     APDU Dispatcher                         │
│                                             │
│  if (selected_app == NULL)                  │
│      → 返回 6E00 (No application selected)  │
│                                             │
│  else                                       │
│      → 调用 selected_app->handlers          │
│      → 查找匹配的 INS                       │
│      → 执行对应的 handler                   │
└─────────────────────────────────────────────┘
```

---

### 2. 数据结构设计

#### 2.1 APDU Handler 定义

```c
// include/gcos_apdu.h

/**
 * @brief APDU Handler 函数指针类型
 * 
 * 每个应用可以定义自己的 APDU 命令处理器
 */
typedef u16 (*ApduHandler)(GCOSAppInstance *app, 
                           const GCOSSApdu *apdu,
                           u8 *response, 
                           u16 *resp_len);

/**
 * @brief APDU 命令表项
 */
typedef struct {
    u8 ins;                 /* 指令代码 */
    ApduHandler handler;    /* 处理函数 */
    const char *name;       /* 命令名称（调试用）*/
} ApduCommandEntry;

/**
 * @brief 应用的 APDU 命令表
 * 
 * 每个应用实例都有自己的命令表
 */
typedef struct {
    const ApduCommandEntry *commands;  /* 命令表数组 */
    u16 command_count;                  /* 命令数量 */
} ApduCommandTable;
```

---

#### 2.2 应用实例结构（增强版）

```c
// include/gcos_vm.h

struct GCOSAppInstance {
    /* === 基本信息 === */
    GCOSAID app_aid;                /* 应用 AID */
    u8 app_id;                      /* 应用 ID (0 = ISD) */
    u16 module_index;               /* 所属模块索引 */
    GCOSAppLifecycleState lifecycle;/* 生命周期状态 */
    
    /* === APDU 处理 === */
    ApduCommandTable cmd_table;     /* ⭐ 应用的 APDU 命令表 */
    
    /* === 应用数据 (堆上，非易失性) === */
    u8 *app_domain_data;            /* 应用域数据 */
    u32 app_domain_data_size;       /* 应用域数据大小 */
    
    u8 *ref_domain_data;            /* 引用域数据 */
    u32 ref_domain_data_size;       /* 引用域数据大小 */
    
    u8 *persistent_data;            /* 持久性数据 */
    u32 persistent_data_size;       /* 持久性数据大小 */
    
    /* === 运行时数据 (每个通道独立，易失性) === */
    struct {
        u8 *temp_dynamic_data;      /* 临时动态数据 */
        u32 temp_dynamic_data_size; /* 临时动态数据大小 */
        
        u8 *global_data_copy;       /* 模块全局数据副本 */
        u32 global_data_size;       /* 全局数据大小 */
    } channel_data[MAX_CHANNELS];
    
    /* === 状态标志 === */
    bool is_selected;               /* 是否被选中 */
    u8 selected_channel;            /* 选中的通道 */
};
```

---

#### 2.3 VM 结构（简化版）

```c
// include/gcos_vm.h

struct GCOSVM {
    /* === 版本和状态 === */
    GCOSVersion version;
    GCOSState state;
    
    /* === 应用管理 === */
    GCOSAppInstance apps[MAX_APPS];     /* 应用实例数组 */
    u8 app_count;                        /* 已安装应用数 */
    
    /* ⭐ 当前选中的应用（不再是索引，而是指针）*/
    GCOSAppInstance *selected_app;      
    
    /* === 通道管理 === */
    struct {
        GCOSAppInstance *selected_app;  /* 该通道选择的应用 */
        bool active;                     /* 通道是否激活 */
    } channels[MAX_CHANNELS];
    
    u8 current_channel;                  /* 当前通道 */
    
    /* === 其他组件 === */
    GCOSMemory memory;                   /* 内存管理 */
    GCOSSecurity security;              /* 安全管理 */
    GCOTransaction transaction;         /* 事务管理 */
    
    /* === 统计信息 === */
    GCOSStats stats;
    
    /* === 配置 === */
    GCOSConfig config;
};
```

---

### 3. API 设计

#### 3.1 修改后的 APDU 处理函数

```c
// include/gcos_apdu.h

/**
 * @brief 处理 APDU 命令（新版本）
 * 
 * @param app 选中的应用实例（⭐ 关键变化）
 * @param apdu APDU 数据
 * @param apdu_len APDU 长度
 * @param response 响应缓冲区
 * @param resp_len 响应长度（输出）
 * @return 状态字 SW
 * 
 * 工作流程：
 * 1. 检查 app 是否为 NULL
 * 2. 从 app->cmd_table 查找匹配的 INS
 * 3. 调用对应的 handler
 * 4. 返回状态字
 */
u16 gcos_process_apdu(GCOSAppInstance *app, 
                      const u8 *apdu, 
                      u16 apdu_len,
                      u8 *response, 
                      u16 *resp_len);

/**
 * @brief 兼容旧版本的 API（可选）
 * 
 * @param vm VM 实例
 * @param apdu APDU 数据
 * @param apdu_len APDU 长度
 * @param response 响应缓冲区
 * @param resp_len 响应长度（输出）
 * @return 状态字 SW
 * 
 * 内部实现：
 * 1. 从 vm->selected_app 获取当前选中的应用
 * 2. 调用 gcos_process_apdu(vm->selected_app, ...)
 */
u16 gcos_vm_process_apdu(GCOSVM *vm, 
                         const u8 *apdu, 
                         u16 apdu_len,
                         u8 *response, 
                         u16 *resp_len);
```

---

#### 3.2 应用管理 API

```c
// include/gcos_app_manager.h

/**
 * @brief 初始化应用管理器（创建 ISD）
 * 
 * 在 VM 启动时调用，创建默认的 ISD 应用
 */
GCOSResult app_manager_init(GCOSVM *vm);

/**
 * @brief 注册应用到应用表
 * 
 * @param vm VM 实例
 * @param app_aid 应用 AID
 * @param cmd_table 应用的 APDU 命令表
 * @param module_index 所属模块索引
 * @param[out] app_id 输出的应用 ID
 * @return GCOS_SUCCESS 成功
 */
GCOSResult app_register(GCOSVM *vm, 
                        const GCOSAID *app_aid,
                        const ApduCommandTable *cmd_table,
                        u16 module_index,
                        u8 *app_id);

/**
 * @brief 根据 AID 查找应用
 * 
 * @param vm VM 实例
 * @param aid AID 数据
 * @param aid_len AID 长度
 * @return 应用实例指针，NULL 表示未找到
 */
GCOSAppInstance* app_find_by_aid(GCOSVM *vm, 
                                  const u8 *aid, 
                                  u8 aid_len);

/**
 * @brief 根据应用 ID 查找应用
 * 
 * @param vm VM 实例
 * @param app_id 应用 ID
 * @return 应用实例指针，NULL 表示未找到
 */
GCOSAppInstance* app_find_by_id(GCOSVM *vm, u8 app_id);

/**
 * @brief 选择应用
 * 
 * @param vm VM 实例
 * @param app_id 应用 ID
 * @param channel 通道号
 * @return GCOS_SUCCESS 成功
 */
GCOSResult app_select(GCOSVM *vm, u8 app_id, u8 channel);

/**
 * @brief 取消选择应用
 * 
 * @param vm VM 实例
 * @param channel 通道号
 * @return GCOS_SUCCESS 成功
 */
GCOSResult app_deselect(GCOSVM *vm, u8 channel);

/**
 * @brief 删除应用
 * 
 * @param vm VM 实例
 * @param app_id 应用 ID
 * @return GCOS_SUCCESS 成功
 */
GCOSResult app_delete(GCOSVM *vm, u8 app_id);
```

---

### 4. 应用选择和命令分发流程

#### 4.1 SELECT 命令处理

```c
// src/gcos_apdu.c

/**
 * @brief SELECT 命令处理器（ISD 级别）
 */
static u16 apdu_handler_select(GCOSAppInstance *app, 
                               const GCOSSApdu *apdu,
                               u8 *response, 
                               u16 *resp_len) {
    GCOSVM *vm = get_vm_from_app(app);  // 从应用获取 VM 指针
    
    u8 p1 = apdu->p1;
    u8 p2 = apdu->p2;
    
    // 情况 1: 隐式选择（没有提供 AID）
    if (apdu->lc == 0) {
        // 选择默认应用或 ISD
        GCOSAppInstance *default_app = app_find_by_id(vm, APP_FIRST);  // ISD
        
        if (default_app == NULL) {
            return SW_FILE_NOT_FOUND;
        }
        
        // 更新选中状态
        vm->selected_app = default_app;
        default_app->is_selected = true;
        default_app->selected_channel = vm->current_channel;
        
        // 生成 FCP
        *resp_len = generate_fcp(default_app, response);
        return SW_SUCCESS;
    }
    
    // 情况 2: 显式选择（提供 AID）
    if (apdu->data == NULL || apdu->lc < 5 || apdu->lc > 16) {
        return SW_WRONG_DATA;
    }
    
    // 查找匹配的应用
    GCOSAppInstance *target_app = app_find_by_aid(vm, apdu->data, apdu->lc);
    
    if (target_app == NULL) {
        return SW_FILE_NOT_FOUND;  // 0x6A82
    }
    
    // 验证应用状态
    if (target_app->lifecycle != APPLICATION_SELECTABLE &&
        target_app->lifecycle != APPLICATION_PERSONALIZED) {
        return SW_CONTRADICTION;  // 0x6985
    }
    
    // 选择应用
    vm->selected_app = target_app;
    target_app->is_selected = true;
    target_app->selected_channel = vm->current_channel;
    
    printf("[SELECT] Application selected. AID: ");
    for (int i = 0; i < target_app->app_aid.length; i++) {
        printf("%02X", target_app->app_aid.data[i]);
    }
    printf("\n");
    
    // 生成 FCP
    *resp_len = generate_fcp(target_app, response);
    return SW_SUCCESS;
}
```

---

#### 4.2 APDU 命令分发

```c
// src/gcos_apdu.c

/**
 * @brief 处理 APDU 命令（核心函数）
 * 
 * ⭐ 关键变化：第一个参数是应用实例，而不是 VM
 */
u16 gcos_process_apdu(GCOSAppInstance *app, 
                      const u8 *apdu, 
                      u16 apdu_len,
                      u8 *response, 
                      u16 *resp_len) {
    // 步骤 1: 检查应用是否有效
    if (app == NULL) {
        printf("[APDU] ERROR: No application selected\n");
        return SW_NO_PRECISE_DIAGNOSIS;  // 0x6F00
    }
    
    // 步骤 2: 解析 APDU
    GCOSSApdu parsed_apdu;
    u16 result = parse_apdu(apdu, apdu_len, &parsed_apdu);
    if (result != SW_SUCCESS) {
        return result;
    }
    
    printf("[APDU] Processing: CLA=%02X INS=%02X P1=%02X P2=%02X\n",
           parsed_apdu.cla, parsed_apdu.ins, parsed_apdu.p1, parsed_apdu.p2);
    
    // 步骤 3: ⭐ 从应用的命令表中查找 handler
    ApduHandler handler = find_handler_in_app(app, parsed_apdu.ins);
    
    if (handler == NULL) {
        printf("[APDU] INS 0x%02X not supported by this application\n", 
               parsed_apdu.ins);
        return SW_INS_NOT_SUPPORTED;  // 0x6D00
    }
    
    // 步骤 4: 调用应用的 handler
    printf("[APDU] Calling handler for INS 0x%02X\n", parsed_apdu.ins);
    u16 sw = handler(app, &parsed_apdu, response, resp_len);
    
    printf("[APDU] Handler returned SW=0x%04X\n", sw);
    return sw;
}

/**
 * @brief 在应用的命令表中查找 handler
 */
static ApduHandler find_handler_in_app(GCOSAppInstance *app, u8 ins) {
    if (app->cmd_table.commands == NULL) {
        return NULL;
    }
    
    const ApduCommandEntry *entry = app->cmd_table.commands;
    
    for (u16 i = 0; i < app->cmd_table.command_count; i++) {
        if (entry[i].ins == ins) {
            printf("[APDU] Found handler: %s\n", entry[i].name);
            return entry[i].handler;
        }
    }
    
    return NULL;
}

/**
 * @brief 兼容旧版本的 API
 */
u16 gcos_vm_process_apdu(GCOSVM *vm, 
                         const u8 *apdu, 
                         u16 apdu_len,
                         u8 *response, 
                         u16 *resp_len) {
    // 获取当前选中的应用
    GCOSAppInstance *app = vm->selected_app;
    
    // 调用新的 API
    return gcos_process_apdu(app, apdu, apdu_len, response, resp_len);
}
```

---

### 5. ISD 和应用示例

#### 5.1 ISD 的命令表

```c
// src/gcos_isd.c

/**
 * @brief ISD 的 APDU 命令表
 * 
 * ISD 处理 GlobalPlatform 管理命令
 */
static u16 isd_handler_load(GCOSAppInstance *app, 
                            const GCOSSApdu *apdu,
                            u8 *response, 
                            u16 *resp_len);

static u16 isd_handler_install(GCOSAppInstance *app, 
                               const GCOSSApdu *apdu,
                               u8 *response, 
                               u16 *resp_len);

static u16 isd_handler_delete(GCOSAppInstance *app, 
                              const GCOSSApdu *apdu,
                              u8 *response, 
                              u16 *resp_len);

static u16 isd_handler_get_status(GCOSAppInstance *app, 
                                  const GCOSSApdu *apdu,
                                  u8 *response, 
                                  u16 *resp_len);

static const ApduCommandEntry isd_commands[] = {
    { 0xE4, isd_handler_load,        "LOAD" },
    { 0xE6, isd_handler_install,     "INSTALL" },
    { 0xE2, isd_handler_delete,      "DELETE" },
    { 0xF2, isd_handler_get_status,  "GET STATUS" },
    { 0x50, isd_handler_init_update, "INITIALIZE UPDATE" },
    { 0x82, isd_handler_ext_auth,    "EXTERNAL AUTHENTICATE" },
    { 0xCA, isd_handler_get_data,    "GET DATA" },
    { 0xDA, isd_handler_put_data,    "PUT DATA" },
};

static const ApduCommandTable isd_cmd_table = {
    .commands = isd_commands,
    .command_count = sizeof(isd_commands) / sizeof(ApduCommandEntry)
};

/**
 * @brief 创建 ISD 应用
 */
GCOSResult create_isd_application(GCOSVM *vm) {
    GCOSAppInstance *isd = &vm->apps[APP_FIRST];
    
    // 设置 ISD AID
    u8 isd_aid[] = {0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x00};
    memcpy(isd->app_aid.data, isd_aid, 8);
    isd->app_aid.length = 8;
    
    // 设置应用 ID
    isd->app_id = APP_FIRST;
    
    // 设置生命周期状态
    isd->lifecycle = APPLICATION_SELECTABLE;
    
    // ⭐ 设置 ISD 的命令表
    isd->cmd_table = isd_cmd_table;
    
    // 标记为已安装
    isd->is_selected = false;
    
    vm->app_count = 1;
    
    printf("[ISD] Created with AID: ");
    for (int i = 0; i < isd->app_aid.length; i++) {
        printf("%02X", isd->app_aid.data[i]);
    }
    printf("\n");
    
    return GCOS_SUCCESS;
}
```

---

#### 5.2 用户应用的命令表

```c
// examples/payment_app.c

/**
 * @brief 支付应用的 APDU 命令表
 */
static u16 pay_handler_purchase(GCOSAppInstance *app, 
                                const GCOSSApdu *apdu,
                                u8 *response, 
                                u16 *resp_len);

static u16 pay_handler_get_balance(GCOSAppInstance *app, 
                                   const GCOSSApdu *apdu,
                                   u8 *response, 
                                   u16 *resp_len);

static const ApduCommandEntry payment_commands[] = {
    { 0xA0, pay_handler_purchase,    "PURCHASE" },
    { 0xB0, pay_handler_get_balance, "GET BALANCE" },
};

static const ApduCommandTable payment_cmd_table = {
    .commands = payment_commands,
    .command_count = sizeof(payment_commands) / sizeof(ApduCommandEntry)
};

/**
 * @brief INSTALL 命令中注册支付应用
 */
static u16 isd_handler_install(GCOSAppInstance *app, 
                               const GCOSSApdu *apdu,
                               u8 *response, 
                               u16 *resp_len) {
    GCOSVM *vm = get_vm_from_app(app);
    
    // 解析安装参数...
    
    // 注册新应用
    u8 new_app_id;
    GCOSResult result = app_register(vm, 
                                     &install_params.instance_aid,
                                     &payment_cmd_table,  // ⭐ 传入应用的命令表
                                     module_index,
                                     &new_app_id);
    
    if (result != GCOS_SUCCESS) {
        return SW_EXECUTION_ERROR;
    }
    
    printf("[INSTALL] Payment app registered. App ID: %u\n", new_app_id);
    
    *resp_len = 0;
    return SW_SUCCESS;
}
```

---

### 6. 完整的 APDU 处理流程

```
卡外工具发送 APDU
    ↓
┌─────────────────────────────────┐
│  JCShell Server (gcos_jcshell)  │
│  - 接收二进制协议消息            │
│  - 解析 APDU                     │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  gcos_main.c (主循环)           │
│                                 │
│  while (1) {                    │
│      apdu_len = receive_apdu(); │
│      sw = gcos_vm_process_apdu( │
│          vm, apdu, apdu_len,    │
│          response, &resp_len    │
│      );                         │
│      send_response(response,    │
│                    resp_len, sw │
│      );                         │
│  }                              │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  gcos_vm_process_apdu()         │
│                                 │
│  app = vm->selected_app;        │
│  return gcos_process_apdu(      │
│      app, apdu, ...             │
│  );                             │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  gcos_process_apdu() ⭐         │
│                                 │
│  1. 检查 app != NULL            │
│  2. 解析 APDU                   │
│  3. handler = find_handler_     │
│       in_app(app, ins)          │
│  4. return handler(app, apdu)   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  应用的 Handler                  │
│                                 │
│  例如：pay_handler_purchase()   │
│  - 执行业务逻辑                  │
│  - 填充 response                 │
│  - 返回 SW                      │
└─────────────────────────────────┘
```

---

## 📊 方案总结

### 核心改进

| 方面 | 旧设计 | 新设计 |
|------|--------|--------|
| **APDU 处理入口** | `gcos_vm_process_apdu(vm, ...)` | `gcos_process_apdu(app, ...)` |
| **命令表位置** | 全局单一表 | 每个应用独立表 |
| **命令分发** | 基于 INS 查找全局表 | 基于 INS 查找应用表 |
| **应用选择** | 只记录索引 | 保存应用指针 |
| **扩展性** | 难以添加新应用 | 轻松注册新应用 |
| **隔离性** | 所有应用共享命令空间 | 应用级命令隔离 |

---

### 实施步骤

#### Phase 1: 数据结构改造
1. 修改 `GCOSAppInstance` 结构，添加 `cmd_table` 字段
2. 修改 `GCOSVM` 结构，将 `selected_app_index` 改为 `selected_app` 指针
3. 定义 `ApduCommandTable` 和 `ApduHandler` 类型

#### Phase 2: API 重构
1. 实现 `gcos_process_apdu(app, ...)` 新 API
2. 保留 `gcos_vm_process_apdu(vm, ...)` 作为兼容层
3. 实现应用管理 API（register/find/select/deselect）

#### Phase 3: ISD 实现
1. 定义 ISD 的命令表（LOAD/INSTALL/DELETE/GET STATUS）
2. 实现 `create_isd_application()` 函数
3. 在 `app_manager_init()` 中创建 ISD

#### Phase 4: 应用注册机制
1. 实现 `app_register()` 函数
2. 在 INSTALL 命令中调用 `app_register()`
3. 为新应用分配命令表

#### Phase 5: 测试验证
1. 测试 ISD 创建和选择
2. 测试 LOAD + INSTALL 流程
3. 测试应用选择和命令分发
4. 测试多应用隔离

---

## 🎯 关键决策点

### 1. 为什么每个应用要有独立的命令表？

**优点**：
- ✅ 应用级命令隔离（不同应用可以有相同的 INS）
- ✅ 模块化设计（应用自包含）
- ✅ 易于扩展（添加新应用不影响其他应用）
- ✅ 符合 JavaCard 规范（每个 Applet 有自己的 process() 方法）

**缺点**：
- ⚠️ 需要更多的内存存储命令表指针
- ⚠️ 命令查找稍微复杂一些

**结论**：优点远大于缺点，必须采用！

---

### 2. 为什么 gcos_process_apdu 的第一个参数是 app 而不是 vm？

**原因**：
1. **语义清晰** - APDU 是发给应用的，不是发给 VM 的
2. **解耦设计** - APDU 处理不依赖 VM 的其他部分
3. **便于测试** - 可以单独测试应用的命令处理
4. **符合规范** - JavaCard 的 `Applet.process()` 也是应用级别的

---

### 3. 是否需要保留 gcos_vm_process_apdu？

**建议**：保留作为兼容层

**理由**：
- 现有的调用代码不需要大规模修改
- 内部调用 `gcos_process_apdu(vm->selected_app, ...)`
- 逐步迁移到新 API

---

## 📝 待确认的问题

### 问题 1: 命令表的存储方式

**选项 A**: 静态定义（推荐）
```c
static const ApduCommandEntry isd_commands[] = {...};
static const ApduCommandTable isd_cmd_table = {...};
```
- ✅ 零运行时开销
- ✅ 存储在 ROM/Flash
- ❌ 不支持动态添加命令

**选项 B**: 动态注册
```c
app_register_command(app, INS_PURCHASE, pay_handler_purchase);
```
- ✅ 灵活，支持动态添加
- ❌ 需要额外的内存管理
- ❌ 增加复杂度

**建议**：采用**选项 A**（静态定义），符合嵌入式系统设计原则。

---

### 问题 2: 未注册的 INS 如何处理？

**选项 A**: 返回 `6D00` (INS not supported)
- ✅ 符合智能卡规范
- ✅ 明确告知客户端命令不支持

**选项 B**: 使用 echo handler（当前实现）
- ✅ 便于调试
- ❌ 不符合生产环境要求

**建议**：**Phase 1 使用选项 B**（便于测试），**Phase 2 切换到选项 A**。

---

### 问题 3: 应用命令表和 ISD 命令表的关系？

**场景**：某些命令应该由 ISD 处理，即使有其他应用被选中

**解决方案**：
```c
u16 gcos_process_apdu(GCOSAppInstance *app, ...) {
    // 特殊情况：GP 管理命令始终由 ISD 处理
    if (is_gp_management_command(parsed_apdu.ins)) {
        GCOSAppInstance *isd = app_find_by_id(vm, APP_FIRST);
        return gcos_process_apdu(isd, apdu, ...);
    }
    
    // 正常情况：由选中的应用处理
    return dispatch_to_app(app, parsed_apdu);
}
```

**GP 管理命令包括**：
- INITIALIZE UPDATE (0x50)
- EXTERNAL AUTHENTICATE (0x82)
- GET DATA (0xCA) - 特定标签
- PUT DATA (0xDA) - 特定标签
- DELETE (0xE2)
- GET STATUS (0xF2)
- INSTALL (0xE6)
- LOAD (0xE4)

---

## ✅ 下一步行动

请确认以下设计方案：

1. ✅ **每个应用有独立的 APDU 命令表**
2. ✅ **gcos_process_apdu 的第一个参数是 Applet 实例**
3. ✅ **保留 gcos_vm_process_apdu 作为兼容层**
4. ✅ **采用静态命令表定义**
5. ✅ **Phase 1 使用 echo handler，Phase 2 返回 6D00**
6. ✅ **GP 管理命令始终由 ISD 处理**

**确认后我将开始实施代码实现！**
