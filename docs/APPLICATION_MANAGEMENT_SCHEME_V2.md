# GCOS VM 应用管理方案（基于 cref 架构分析）

**深度分析 cref 的应用命令分发机制**

**日期**: 2026-05-11  
**版本**: 2.0（修正版）

---

## 🔍 cref 架构核心发现

### 关键洞察

通过分析 cref 源码，我发现了一个**非常重要的架构模式**：

#### cref 的命令分发流程

```
┌─────────────────────────────────────────────┐
│         JCShell 发送 APDU                    │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│    T=0/T=CL 协议层接收 APDU                  │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│    JCRE 主循环 (run())                       │
│    - 字节码解释器持续运行                     │
│    - 执行 JavaCard 字节码                    │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│    JCRE 字节码处理 SELECT 命令               │
│    - 查找匹配的应用                          │
│    - 调用 selectOnly()                       │
│    - 调用应用的 install/select 方法          │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│    JCRE 字节码处理其他 APDU                  │
│    - 检查是否有选中的应用                    │
│    - 调用应用的 process() 方法               │
│      run_app(process_method_offset)          │
└─────────────────────────────────────────────┘
```

---

### cref 的核心设计模式

#### 1. **单一 process() 方法处理所有命令**

在 cref 中，每个 Applet 只有一个 `process()` 方法：

```java
// JavaCard Applet 代码示例
public class PaymentApplet extends Applet {
    public void process(APDU apdu) {
        byte[] buffer = apdu.getBuffer();
        byte ins = buffer[ISO7816.OFFSET_INS];
        
        switch (ins) {
            case INS_PURCHASE:
                handlePurchase(apdu);
                break;
            case INS_GET_BALANCE:
                handleGetBalance(apdu);
                break;
            default:
                ISOException.throwIt(ISO7816.SW_INS_NOT_SUPPORTED);
        }
    }
}
```

**关键点**：
- ✅ 每个应用只有**一个** `process()` 方法
- ✅ `process()` 内部通过 `switch(ins)` 分发到不同的 handler
- ✅ 不需要为每个 INS 注册单独的 handler
- ✅ 简单、灵活、符合 JavaCard 规范

---

#### 2. **固定方法接口**

cref 中应用只有几个固定的方法：

| 方法 | 用途 | 调用时机 |
|------|------|----------|
| `install()` | 安装应用 | INSTALL 命令 |
| `select()` | 选择应用 | SELECT 命令（首次） |
| `deselect()` | 取消选择 | 取消选择时 |
| `process()` | 处理 APDU | 所有其他 APDU |

**这些方法的签名是固定的**，由 JavaCard 规范定义。

---

#### 3. **GCOS 与 ISD 的职责分离**

```
┌─────────────────────────────────────────────┐
│         GCOS VM (相当于 JCRE + ISD)          │
│                                             │
│  职责：                                      │
│  1. 处理 GlobalPlatform 管理命令             │
│     - LOAD (0xE4)                           │
│     - INSTALL (0xE6)                        │
│     - DELETE (0xE2)                         │
│     - GET STATUS (0xF2)                     │
│     - INITIALIZE UPDATE (0x50)              │
│     - EXTERNAL AUTHENTICATE (0x82)          │
│                                             │
│  2. 应用生命周期管理                         │
│     - 创建应用实例                           │
│     - 选择/取消选择应用                      │
│     - 删除应用                               │
│                                             │
│  3. APDU 分发                               │
│     - 如果是 GP 命令 → GCOS 处理            │
│     - 如果有选中的应用 → 调用 process()     │
│     - 否则 → 返回 6E00                      │
└─────────────────────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│         用户应用 (Applet)                    │
│                                             │
│  职责：                                      │
│  1. 实现 process() 方法                     │
│     - 解析 INS                              │
│     - 执行业务逻辑                           │
│     - 返回 SW                               │
│                                             │
│  2. 可选实现 select()/deselect()            │
│     - 初始化/清理资源                        │
└─────────────────────────────────────────────┘
```

---

## ✅ 修正后的 GCOS 应用管理方案

### 1. 数据结构设计

#### 1.1 应用实例结构

```c
// include/gcos_vm.h

/**
 * @brief 应用实例结构
 * 
 * 参考 cref 的设计，每个应用只有一个 process() 方法指针
 */
struct GCOSAppInstance {
    /* === 基本信息 === */
    GCOSAID app_aid;                /* 应用 AID */
    u8 app_id;                      /* 应用 ID (0 = ISD) */
    u16 module_index;               /* 所属模块索引 */
    GCOSAppLifecycleState lifecycle;/* 生命周期状态 */
    
    /* === APDU 处理方法 ⭐ === */
    /**
     * @brief 应用的 process() 方法指针
     * 
     * 类似于 cref 中的 Applet.process(APDU)
     * 这个方法负责处理所有非 GP 管理的 APDU 命令
     * 
     * @param app 应用实例指针
     * @param apdu APDU 数据
     * @param apdu_len APDU 长度
     * @param response 响应缓冲区
     * @param resp_len 响应长度（输出）
     * @return 状态字 SW
     */
    u16 (*process)(struct GCOSAppInstance *app,
                   const u8 *apdu,
                   u16 apdu_len,
                   u8 *response,
                   u16 *resp_len);
    
    /**
     * @brief 应用的 select() 方法（可选）
     * 
     * 在 SELECT 命令成功后调用，用于初始化
     * 
     * @param app 应用实例指针
     * @return GCOS_SUCCESS 成功，其他表示失败
     */
    GCOSResult (*on_select)(struct GCOSAppInstance *app);
    
    /**
     * @brief 应用的 deselect() 方法（可选）
     * 
     * 在取消选择时调用，用于清理资源
     * 
     * @param app 应用实例指针
     */
    void (*on_deselect)(struct GCOSAppInstance *app);
    
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

#### 1.2 VM 结构

```c
// include/gcos_vm.h

struct GCOSVM {
    /* === 版本和状态 === */
    GCOSVersion version;
    GCOSState state;
    
    /* === 应用管理 === */
    GCOSAppInstance apps[MAX_APPS];     /* 应用实例数组 */
    u8 app_count;                        /* 已安装应用数 */
    
    /* ⭐ 当前选中的应用（指针）*/
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

### 2. API 设计

#### 2.1 APDU 处理函数

```c
// include/gcos_apdu.h

/**
 * @brief 处理 APDU 命令
 * 
 * ⭐ 核心函数：根据 cref 架构设计
 * 
 * 工作流程：
 * 1. 解析 APDU
 * 2. 判断是否为 GP 管理命令
 *    - 是 → 调用 ISD 的 process() 处理
 *    - 否 → 继续步骤 3
 * 3. 检查是否有选中的应用
 *    - 无 → 返回 6E00
 *    - 有 → 调用 selected_app->process()
 * 
 * @param vm VM 实例
 * @param apdu APDU 数据
 * @param apdu_len APDU 长度
 * @param response 响应缓冲区
 * @param resp_len 响应长度（输出）
 * @return 状态字 SW
 */
u16 gcos_process_apdu(GCOSVM *vm, 
                      const u8 *apdu, 
                      u16 apdu_len,
                      u8 *response, 
                      u16 *resp_len);
```

---

#### 2.2 应用管理 API

```c
// include/gcos_app_manager.h

/**
 * @brief 初始化应用管理器（创建 ISD）
 */
GCOSResult app_manager_init(GCOSVM *vm);

/**
 * @brief 注册应用到应用表
 * 
 * 在 INSTALL 命令中调用
 * 
 * @param vm VM 实例
 * @param app_aid 应用 AID
 * @param process_func 应用的 process() 方法指针 ⭐
 * @param on_select 应用的 select() 方法（可选）
 * @param on_deselect 应用的 deselect() 方法（可选）
 * @param module_index 所属模块索引
 * @param[out] app_id 输出的应用 ID
 * @return GCOS_SUCCESS 成功
 */
GCOSResult app_register(GCOSVM *vm, 
                        const GCOSAID *app_aid,
                        u16 (*process_func)(GCOSAppInstance *, const u8 *, u16, u8 *, u16 *),
                        GCOSResult (*on_select)(GCOSAppInstance *),
                        void (*on_deselect)(GCOSAppInstance *),
                        u16 module_index,
                        u8 *app_id);

/**
 * @brief 根据 AID 查找应用
 */
GCOSAppInstance* app_find_by_aid(GCOSVM *vm, 
                                  const u8 *aid, 
                                  u8 aid_len);

/**
 * @brief 选择应用
 * 
 * 1. 查找应用
 * 2. 验证状态
 * 3. 调用 on_select()（如果存在）
 * 4. 更新 vm->selected_app
 */
GCOSResult app_select(GCOSVM *vm, u8 app_id, u8 channel);

/**
 * @brief 取消选择应用
 * 
 * 1. 调用 on_deselect()（如果存在）
 * 2. 清除 vm->selected_app
 */
GCOSResult app_deselect(GCOSVM *vm, u8 channel);
```

---

### 3. 核心实现

#### 3.1 APDU 处理主函数

```c
// src/gcos_apdu.c

/**
 * @brief 判断是否为 GP 管理命令
 */
static bool is_gp_management_command(u8 cla, u8 ins) {
    // GP 管理命令列表
    switch (ins) {
        case 0x50:  // INITIALIZE UPDATE
        case 0x82:  // EXTERNAL AUTHENTICATE
        case 0xCA:  // GET DATA (特定标签)
        case 0xDA:  // PUT DATA (特定标签)
        case 0xE2:  // DELETE
        case 0xE4:  // LOAD
        case 0xE6:  // INSTALL
        case 0xF2:  // GET STATUS
            return true;
        default:
            return false;
    }
}

/**
 * @brief 处理 APDU 命令（核心函数）
 * 
 * ⭐ 基于 cref 架构设计
 */
u16 gcos_process_apdu(GCOSVM *vm, 
                      const u8 *apdu, 
                      u16 apdu_len,
                      u8 *response, 
                      u16 *resp_len) {
    // 步骤 1: 基本验证
    if (apdu == NULL || apdu_len < 4) {
        return SW_WRONG_LENGTH;
    }
    
    u8 cla = apdu[0];
    u8 ins = apdu[1];
    
    printf("[APDU] CLA=%02X INS=%02X\n", cla, ins);
    
    // 步骤 2: 判断是否为 GP 管理命令
    if (is_gp_management_command(cla, ins)) {
        printf("[APDU] GP management command detected\n");
        
        // GP 命令始终由 ISD 处理
        GCOSAppInstance *isd = app_find_by_id(vm, APP_FIRST);
        
        if (isd == NULL) {
            printf("[APDU] ERROR: ISD not found!\n");
            return SW_NO_PRECISE_DIAGNOSIS;
        }
        
        // 调用 ISD 的 process() 方法
        if (isd->process == NULL) {
            printf("[APDU] ERROR: ISD has no process handler!\n");
            return SW_NO_PRECISE_DIAGNOSIS;
        }
        
        return isd->process(isd, apdu, apdu_len, response, resp_len);
    }
    
    // 步骤 3: 非 GP 命令，需要选中的应用
    if (vm->selected_app == NULL) {
        printf("[APDU] ERROR: No application selected\n");
        return SW_NO_PRECISE_DIAGNOSIS;  // 0x6F00
    }
    
    GCOSAppInstance *app = vm->selected_app;
    
    printf("[APDU] Dispatching to application. AID: ");
    for (int i = 0; i < app->app_aid.length; i++) {
        printf("%02X", app->app_aid.data[i]);
    }
    printf("\n");
    
    // 步骤 4: 检查应用是否有 process 方法
    if (app->process == NULL) {
        printf("[APDU] ERROR: Application has no process handler!\n");
        return SW_NO_PRECISE_DIAGNOSIS;
    }
    
    // 步骤 5: ⭐ 调用应用的 process() 方法
    // 类似于 cref 中的 run_app(process_method_offset)
    printf("[APDU] Calling app->process()\n");
    u16 sw = app->process(app, apdu, apdu_len, response, resp_len);
    
    printf("[APDU] process() returned SW=0x%04X\n", sw);
    return sw;
}
```

---

#### 3.2 ISD 的 process() 实现

```c
// src/gcos_isd.c

/**
 * @brief ISD 的 process() 方法
 * 
 * 处理所有 GP 管理命令
 */
static u16 isd_process(GCOSAppInstance *app,
                       const u8 *apdu,
                       u16 apdu_len,
                       u8 *response,
                       u16 *resp_len) {
    u8 ins = apdu[1];
    
    printf("[ISD] Processing command INS=0x%02X\n", ins);
    
    switch (ins) {
        case 0xE4:  // LOAD
            return isd_handler_load(app, apdu, apdu_len, response, resp_len);
        
        case 0xE6:  // INSTALL
            return isd_handler_install(app, apdu, apdu_len, response, resp_len);
        
        case 0xE2:  // DELETE
            return isd_handler_delete(app, apdu, apdu_len, response, resp_len);
        
        case 0xF2:  // GET STATUS
            return isd_handler_get_status(app, apdu, apdu_len, response, resp_len);
        
        case 0x50:  // INITIALIZE UPDATE
            return isd_handler_init_update(app, apdu, apdu_len, response, resp_len);
        
        case 0x82:  // EXTERNAL AUTHENTICATE
            return isd_handler_ext_auth(app, apdu, apdu_len, response, resp_len);
        
        case 0xCA:  // GET DATA
            return isd_handler_get_data(app, apdu, apdu_len, response, resp_len);
        
        case 0xDA:  // PUT DATA
            return isd_handler_put_data(app, apdu, apdu_len, response, resp_len);
        
        default:
            printf("[ISD] Unsupported GP command: INS=0x%02X\n", ins);
            return SW_INS_NOT_SUPPORTED;  // 0x6D00
    }
}

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
    
    // ⭐ 设置 ISD 的 process() 方法
    isd->process = isd_process;
    isd->on_select = NULL;   // ISD 不需要 select/deselect
    isd->on_deselect = NULL;
    
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

#### 3.3 用户应用的 process() 实现

```c
// examples/payment_app.c

/**
 * @brief 支付应用的 process() 方法
 * 
 * ⭐ 类似于 cref 中的 Applet.process(APDU)
 * 这个方法处理所有支付相关的 APDU 命令
 */
static u16 payment_app_process(GCOSAppInstance *app,
                               const u8 *apdu,
                               u16 apdu_len,
                               u8 *response,
                               u16 *resp_len) {
    u8 ins = apdu[1];
    
    printf("[PaymentApp] Processing command INS=0x%02X\n", ins);
    
    // ⭐ 内部通过 switch 分发到不同的 handler
    switch (ins) {
        case 0xA0:  // PURCHASE
            return pay_handler_purchase(app, apdu, apdu_len, response, resp_len);
        
        case 0xB0:  // GET BALANCE
            return pay_handler_get_balance(app, apdu, apdu_len, response, resp_len);
        
        case 0xC0:  // CREDIT
            return pay_handler_credit(app, apdu, apdu_len, response, resp_len);
        
        default:
            printf("[PaymentApp] Unsupported command: INS=0x%02X\n", ins);
            return SW_INS_NOT_SUPPORTED;  // 0x6D00
    }
}

/**
 * @brief 支付应用的 select() 回调
 */
static GCOSResult payment_app_on_select(GCOSAppInstance *app) {
    printf("[PaymentApp] Selected\n");
    // 可以在此初始化应用状态
    return GCOS_SUCCESS;
}

/**
 * @brief 支付应用的 deselect() 回调
 */
static void payment_app_on_deselect(GCOSAppInstance *app) {
    printf("[PaymentApp] Deselected\n");
    // 可以在此清理临时数据
}

/**
 * @brief INSTALL 命令中注册支付应用
 */
static u16 isd_handler_install(GCOSAppInstance *app, 
                               const u8 *apdu,
                               u16 apdu_len,
                               u8 *response, 
                               u16 *resp_len) {
    GCOSVM *vm = get_vm_from_app(app);
    
    // 解析安装参数...
    // ...
    
    // ⭐ 注册新应用，传入 process() 方法指针
    u8 new_app_id;
    GCOSResult result = app_register(vm, 
                                     &install_params.instance_aid,
                                     payment_app_process,        // ⭐ process 方法
                                     payment_app_on_select,      // ⭐ select 回调
                                     payment_app_on_deselect,    // ⭐ deselect 回调
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

#### 3.4 应用注册函数

```c
// src/gcos_app_manager.c

/**
 * @brief 注册应用到应用表
 */
GCOSResult app_register(GCOSVM *vm, 
                        const GCOSAID *app_aid,
                        u16 (*process_func)(GCOSAppInstance *, const u8 *, u16, u8 *, u16 *),
                        GCOSResult (*on_select)(GCOSAppInstance *),
                        void (*on_deselect)(GCOSAppInstance *),
                        u16 module_index,
                        u8 *app_id) {
    // 查找空闲的应用槽位
    u8 new_app_id = find_free_app_slot(vm);
    
    if (new_app_id == 0xFF) {
        return GCOS_ERROR_APP_TABLE_FULL;
    }
    
    GCOSAppInstance *app = &vm->apps[new_app_id];
    
    // 设置应用信息
    memcpy(&app->app_aid, app_aid, sizeof(GCOSAID));
    app->app_id = new_app_id;
    app->module_index = module_index;
    app->lifecycle = APPLICATION_INSTALLED;
    
    // ⭐ 设置方法指针
    app->process = process_func;
    app->on_select = on_select;
    app->on_deselect = on_deselect;
    
    // 初始化状态
    app->is_selected = false;
    app->selected_channel = 0xFF;
    
    vm->app_count++;
    
    if (app_id != NULL) {
        *app_id = new_app_id;
    }
    
    printf("[APP] Registered. ID=%u AID=", new_app_id);
    for (int i = 0; i < app_aid->length; i++) {
        printf("%02X", app_aid->data[i]);
    }
    printf("\n");
    
    return GCOS_SUCCESS;
}
```

---

#### 3.5 应用选择函数

```c
// src/gcos_app_manager.c

/**
 * @brief 选择应用
 */
GCOSResult app_select(GCOSVM *vm, u8 app_id, u8 channel) {
    // 步骤 1: 查找应用
    GCOSAppInstance *app = app_find_by_id(vm, app_id);
    
    if (app == NULL) {
        return GCOS_ERROR_APP_NOT_FOUND;
    }
    
    // 步骤 2: 验证应用状态
    if (app->lifecycle != APPLICATION_SELECTABLE &&
        app->lifecycle != APPLICATION_PERSONALIZED) {
        return GCOS_ERROR_APP_NOT_SELECTABLE;
    }
    
    // 步骤 3: 取消当前选择的应用（如果有）
    if (vm->selected_app != NULL && vm->selected_app != app) {
        app_deselect(vm, vm->current_channel);
    }
    
    // 步骤 4: 调用应用的 on_select() 回调（如果存在）
    if (app->on_select != NULL) {
        GCOSResult result = app->on_select(app);
        if (result != GCOS_SUCCESS) {
            printf("[SELECT] on_select() failed: %d\n", result);
            return result;
        }
    }
    
    // 步骤 5: 更新选择状态
    vm->selected_app = app;
    app->is_selected = true;
    app->selected_channel = channel;
    
    vm->channels[channel].selected_app = app;
    vm->channels[channel].active = true;
    vm->current_channel = channel;
    
    printf("[SELECT] Application selected. AID: ");
    for (int i = 0; i < app->app_aid.length; i++) {
        printf("%02X", app->app_aid.data[i]);
    }
    printf("\n");
    
    return GCOS_SUCCESS;
}

/**
 * @brief 取消选择应用
 */
GCOSResult app_deselect(GCOSVM *vm, u8 channel) {
    GCOSAppInstance *app = vm->channels[channel].selected_app;
    
    if (app == NULL) {
        return GCOS_SUCCESS;  // 没有应用被选择
    }
    
    // 步骤 1: 调用应用的 on_deselect() 回调（如果存在）
    if (app->on_deselect != NULL) {
        app->on_deselect(app);
    }
    
    // 步骤 2: 清除选择状态
    app->is_selected = false;
    app->selected_channel = 0xFF;
    
    vm->channels[channel].selected_app = NULL;
    vm->channels[channel].active = false;
    
    if (vm->selected_app == app) {
        vm->selected_app = NULL;
    }
    
    printf("[DESELECT] Application deselected\n");
    
    return GCOS_SUCCESS;
}
```

---

### 4. 完整的 APDU 处理流程

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
│      sw = gcos_process_apdu(    │
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
│  gcos_process_apdu() ⭐         │
│                                 │
│  1. 解析 APDU (CLA, INS)        │
│  2. if (GP 管理命令) {          │
│       isd = find_isd();         │
│       return isd->process();    │
│     }                           │
│  3. if (selected_app == NULL) { │
│       return 6E00;              │
│     }                           │
│  4. return selected_app->       │
│       process();                │
└──────────────┬──────────────────┘
               │
        ┌──────┴──────┐
        │             │
        ▼             ▼
   GP 命令      应用命令
        │             │
        ▼             ▼
┌──────────────┐ ┌──────────────────┐
│ ISD 处理     │ │ 应用 process()   │
│              │ │                  │
│ switch(ins){ │ │ switch(ins){     │
│  LOAD: ...   │ │  PURCHASE: ...  │
│  INSTALL:... │ │  BALANCE: ...   │
│  DELETE: ... │ │  ...            │
│ }            │ │ }                │
└──────────────┘ └──────────────────┘
```

---

## 📊 方案总结

### 核心改进（对比之前的方案）

| 方面 | 旧方案（错误） | 新方案（正确） |
|------|---------------|---------------|
| **命令表** | 每个应用有独立的命令表 | 每个应用只有一个 process() 方法 |
| **命令分发** | 在 VM 层查找命令表 | 在应用内部 switch(ins) 分发 |
| **方法数量** | 多个 handler 指针 | 三个方法指针：process/on_select/on_deselect |
| **扩展性** | 需要修改命令表结构 | 只需修改应用内部的 switch |
| **符合规范** | 不符合 JavaCard 规范 | 完全符合 JavaCard 规范 |
| **实现复杂度** | 复杂（需要维护命令表） | 简单（只需方法指针） |

---

### 为什么这个方案更好？

#### 1. **符合 JavaCard 规范**

JavaCard 规范要求每个 Applet 实现：
```java
public abstract void process(APDU apdu);
```

我们的设计与之一致：
```c
u16 (*process)(GCOSAppInstance *app, const u8 *apdu, ...);
```

---

#### 2. **简化了 VM 的设计**

- ❌ 旧方案：VM 需要维护每个应用的命令表
- ✅ 新方案：VM 只需要调用 process() 方法

---

#### 3. **应用开发者更灵活**

应用开发者可以自由决定如何分发命令：

```c
// 方式 1: switch-case
u16 my_app_process(...) {
    switch (ins) {
        case 0xA0: handle_a0(); break;
        case 0xB0: handle_b0(); break;
    }
}

// 方式 2: 查表法
static HandlerEntry handlers[] = {...};
u16 my_app_process(...) {
    HandlerEntry *h = find_handler(handlers, ins);
    return h->func(app, apdu, ...);
}

// 方式 3: 条件判断
u16 my_app_process(...) {
    if (ins == 0xA0) return handle_a0();
    if (ins == 0xB0) return handle_b0();
}
```

---

#### 4. **与 cref 架构一致**

cref 中：
- JCRE 字节码调用 `Applet.process()`
- 应用的字节码内部通过 switch 分发

我们的设计：
- VM 调用 `app->process()`
- 应用内部通过 switch 分发

**完全一致！**

---

## 🎯 实施步骤

### Phase 1: 数据结构改造
1. 修改 `GCOSAppInstance` 结构，添加三个方法指针：
   - `process`
   - `on_select`
   - `on_deselect`
2. 移除之前设计的 `cmd_table` 字段

### Phase 2: API 实现
1. 实现 `gcos_process_apdu()` 主函数
2. 实现 `app_register()` 函数
3. 实现 `app_select()` / `app_deselect()` 函数

### Phase 3: ISD 实现
1. 实现 `isd_process()` 函数
2. 实现各个 GP 命令的 handler
3. 实现 `create_isd_application()` 函数

### Phase 4: 示例应用
1. 创建支付应用示例
2. 实现 `payment_app_process()` 函数
3. 测试 INSTALL + SELECT + APDU 流程

### Phase 5: 测试验证
1. 测试 ISD 创建和 GP 命令
2. 测试应用注册和选择
3. 测试应用命令分发
4. 测试多应用隔离

---

## ✅ 最终确认的设计

1. ✅ **每个应用只有一个 process() 方法**（不是命令表）
2. ✅ **gcos_process_apdu 的第一个参数是 VM**（不是 app）
3. ✅ **不需要 gcos_vm_process_apdu 兼容层**
4. ✅ **应用内部通过 switch(ins) 分发命令**
5. ✅ **Phase 1 使用 echo handler，Phase 2 返回 6D00**
6. ✅ **GP 管理命令始终由 ISD 处理**

---

**请确认后我将开始实施代码实现！** 🚀
