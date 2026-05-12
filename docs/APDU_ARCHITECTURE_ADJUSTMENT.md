# GCOS VM 架构调整方案 - 基于cref (JavaCard) 参考实现

## 📋 文档信息

- **版本**: v1.0
- **日期**: 2026-05-11
- **状态**: 架构分析与调整规划
- **参考标准**: GB/T 44901.3 (COS3规范)
- **参考实现**: cref (JavaCard虚拟机)
- **目标**: 将gcos_vm从WASM风格调整为APDU驱动的JavaCard风格

---

## 1. 核心架构差异分析

### 1.1 WASM虚拟机 vs JavaCard/COS3虚拟机

| 维度 | WASM (iwasm/wasm3) | JavaCard/cref | COS3/gcos_vm (目标) |
|------|-------------------|---------------|-------------------|
| **交互接口** | C API函数调用 | APDU命令流 | **APDU命令流** ✅ |
| **加载方式** | 一次性加载完整文件 | **流式加载**（多APDU分段） | **流式加载** ✅ |
| **安装流程** | load + instantiate | **状态机管理** | **状态机管理** ✅ |
| **应用选择** | 直接调用export | SELECT/DESELECT | SELECT/DESELECT ✅ |
| **事务支持** | 无内置事务 | **原子性事务** | **原子性事务** ✅ |
| **内存模型** | 线性内存 | **分区内存** | **分区内存** ✅ |
| **通道管理** | 无 | **8逻辑通道** | **8逻辑通道** ✅ |
| **生命周期** | 简单创建/销毁 | **6种状态** | **6种状态** ✅ |

---

## 2. cref关键设计模式分析

### 2.1 APDU驱动架构

```c
// cref的核心执行循环 (t0.c)
static u8 state = INIT;  // IO线状态机

// 状态流转:
// INIT → HEADER_RECEIVED → HEADER_READ → INCOMING/OUTGOING

// APDU处理流程:
1. T0_receive_header()     // 接收APDU头 (CLA, INS, P1, P2, P3)
2. T0_dispatch_command()   // 根据INS分发到对应处理器
3. T0_process_data()       // 处理数据段 (可能分片)
4. T0_send_response()      // 返回SW1SW2状态码
```

### 2.2 流式加载状态机 (native_install.c)

```c
// LOAD指令状态机 (GP_LOAD_INS = 0xE8)
typedef enum {
    LOAD_STATE_IDLE,           // 空闲
    LOAD_STATE_INIT,           // 初始化加载上下文
    LOAD_STATE_RECEIVING,      // 接收代码块
    LOAD_STATE_LINKING,        // 链接阶段
    LOAD_STATE_INSTALLING,     // 安装阶段
    LOAD_STATE_COMPLETE        // 完成
} LoadState;

// 流式加载流程:
APDU 1: LOAD [INIT]     → 创建加载上下文，分配资源
APDU 2: LOAD [DATA]     → 接收第一段代码
APDU 3: LOAD [DATA]     → 接收第二段代码
...
APDU N: LOAD [DATA]     → 接收最后一段代码
APDU N+1: LOAD [LINK]   → 解析导入/导出表
APDU N+2: INSTALL       → 实例化应用
```

### 2.3 应用生命周期管理

```c
// cref的应用状态 (lifecycle.h)
typedef enum {
    APP_STATE_LOADED,         // 已加载 (代码在ROM)
    APP_STATE_INSTALLED,      // 已安装 (对象已创建)
    APP_STATE_SELECTABLE,     // 可选择 (可被SELECT)
    APP_STATE_SELECTED,       // 已选择 (当前活动)
    APP_STATE_PERSONALIZED,   // 已个性化
    APP_STATE_LOCKED,         // 已锁定
    APP_STATE_DELETED         // 已删除
} AppState;

// SELECT命令处理:
if (INS == SELECT_INS) {
    app = find_app_by_aid(AID);
    if (app->state == SELECTABLE) {
        deselect_current_app();
        select_app(app);
        app->state = SELECTED;
        return SW_SUCCESS;
    }
}
```

### 2.4 事务管理机制

```c
// cref的事务备份 (TransactionMgr.c)
void transaction_begin() {
    // 备份堆和全局数据
    backup_heap();
    backup_global_data();
    transaction_depth++;
}

void transaction_commit() {
    // 提交更改，清除备份
    clear_backup();
    transaction_depth--;
}

void transaction_abort() {
    // 恢复备份
    restore_heap();
    restore_global_data();
    transaction_depth = 0;
}
```

---

## 3. gcos_vm当前问题诊断

### 3.1 架构不匹配问题

#### ❌ 问题1: 缺少APDU接口层

**现状**: gcos_vm使用C API直接调用
```c
// 当前方式 (WASM风格)
GCOSResult gcos_vm_load_sef(GCOSVM *vm, const u8 *sef_data, u32 size);
GCOSResult gcos_vm_install_app(GCOSVM *vm, const u8 *aid, ...);
```

**需要改为**: APDU命令处理
```c
// 目标方式 (JavaCard风格)
void gcos_apdu_handler(const u8 *apdu, u8 length, u8 *response, u16 *resp_len);

// APDU格式:
// CLA INS P1 P2 Lc [DATA] Le
// 例如: 0x80 0xE8 0x00 0x00 0x10 [SEF_DATA...] 0x00
```

#### ❌ 问题2: 加载器不支持流式传输

**现状**: `gcos_loader_load_sef()` 期望完整SEF文件
```c
GCOSResult gcos_loader_load_sef(GCOSVM *vm, const u8 *sef_data, u32 sef_size) {
    // 一次性解析整个文件
}
```

**需要改为**: 状态机驱动的流式加载
```c
typedef struct {
    LoadState state;              // 加载状态
    u8 load_context_id;           // 加载上下文ID
    u32 total_size;               // 总大小
    u32 received_size;            // 已接收大小
    u8 buffer[LOAD_BUFFER_SIZE];  // 临时缓冲区
    u8 current_block;             // 当前块索引
} LoadContext;

GCOSResult gcos_loader_process_apdu(GCOSVM *vm, const u8 *apdu, u8 length) {
    switch (load_ctx.state) {
        case LOAD_STATE_INIT:
            return init_load_context(apdu);
        case LOAD_STATE_RECEIVING:
            return receive_data_block(apdu);
        case LOAD_STATE_LINKING:
            return perform_linking();
        case LOAD_STATE_INSTALLING:
            return install_application();
    }
}
```

#### ❌ 问题3: 缺少APDU命令分发器

**现状**: 没有APDU路由机制

**需要添加**: APDU命令表
```c
typedef struct {
    u8 ins;                       // 指令码
    u8 cla_mask;                  // CLA掩码
    ApduHandler handler;          // 处理函数
    u8 min_length;                // 最小长度
    u8 max_length;                // 最大长度
} ApduCommandTable;

// COS3标准APDU指令:
#define INS_LOAD        0xE8    // 加载模块
#define INS_INSTALL     0xE6    // 安装应用
#define INS_DELETE      0xE4    // 删除应用
#define INS_SELECT      0xA4    // 选择应用
#define INS_DESELECT    0xAA    // 取消选择
#define INS_GET_STATUS  0xF2    // 获取状态
#define INS_SET_CHANNEL 0x70    // 设置通道
```

#### ❌ 问题4: 通道管理与APDU解耦

**现状**: 通道管理是独立的API调用

**需要改为**: 通道信息与APDU绑定
```c
// APDU的CLA字节包含通道信息
u8 logical_channel = apdu[APDU_OFFSET_CLA] & 0x03;  // bits 0-1

// 每个通道维护独立的选择状态
typedef struct {
    u8 selected_app_index;      // 该通道选择的应用
    bool channel_open;          // 通道是否打开
    u8 security_level;          // 安全级别
} ChannelContext;

ChannelContext channels[MAX_CHANNELS];
```

---

## 4. 架构调整方案

### 4.1 新增模块清单

#### ✅ 模块1: APDU接口层 (`gcos_apdu.c/h`)

**职责**:
- APDU命令接收和解析
- 命令分发到对应处理器
- 响应组装和发送
- 状态码管理 (SW1SW2)

**关键结构**:
```c
typedef struct {
    u8 cla;                     // Class byte
    u8 ins;                     // Instruction byte
    u8 p1;                      // Parameter 1
    u8 p2;                      // Parameter 2
    u8 lc;                      // Length of data
    const u8 *data;             // Data pointer
    u8 le;                      // Expected response length
} GCOSSApdu;

typedef struct {
    u8 sw1;                     // Status word 1
    u8 sw2;                     // Status word 2
} GCOSSwStatus;

// APDU处理函数签名
typedef GCOSSwStatus (*ApduHandler)(GCOSVM *vm, const GCOSSApdu *apdu, 
                                     u8 *response, u16 *resp_len);
```

**状态码定义**:
```c
#define SW_SUCCESS              0x9000  // 成功
#define SW_WARNING              0x6200  // 警告
#define SW_EXECUTION_ERROR      0x6400  // 执行错误
#define SW_SECURITY_ERROR       0x6982  // 安全状态不满足
#define SW_WRONG_LENGTH         0x6700  // 长度错误
#define SW_WRONG_DATA           0x6A80  // 数据错误
#define SW_APP_NOT_FOUND        0x6A82  // 应用未找到
#define SW_CONDITION_NOT_SATISFIED 0x6985 // 条件不满足
```

#### ✅ 模块2: 流式加载管理器 (`gcos_stream_loader.c/h`)

**职责**:
- 管理LOAD命令的状态机
- 缓冲和重组SEF数据块
- 验证数据完整性
- 触发链接和安装

**状态机设计**:
```c
typedef enum {
    STREAM_LOADER_IDLE,         // 空闲
    STREAM_LOADER_INIT,         // 初始化加载上下文
    STREAM_LOADER_RECEIVING,    // 接收数据块
    STREAM_LOADER_VERIFYING,    // 验证完整性
    STREAM_LOADER_LINKING,      // 链接阶段
    STREAM_LOADER_INSTALLING,   // 安装阶段
    STREAM_LOADER_COMPLETE,     // 完成
    STREAM_LOADER_ERROR         // 错误状态
} StreamLoaderState;

typedef struct {
    StreamLoaderState state;
    u8 context_id;              // 加载上下文ID (支持并发加载)
    u32 total_expected_size;    // 期望的总大小
    u32 received_size;          // 已接收大小
    u8 *buffer;                 // 数据缓冲区
    u32 buffer_capacity;        // 缓冲区容量
    u16 checksum;               // 校验和
    u8 block_sequence;          // 块序列号
    GCOSSefHeader header;       // SEF文件头 (延迟解析)
} StreamLoadContext;
```

**APDU处理流程**:
```
APDU: LOAD [INIT, total_size=1024]
  → 创建StreamLoadContext
  → 分配缓冲区
  → 返回 SW_SUCCESS

APDU: LOAD [DATA, seq=1, data[256]]
  → 验证序列号
  → 复制到缓冲区
  → 更新checksum
  → 返回 SW_SUCCESS

APDU: LOAD [DATA, seq=2, data[256]]
  → ...

APDU: LOAD [DATA, seq=4, data[256], FINAL]
  → 验证总大小
  → 验证checksum
  → 状态切换到 VERIFYING
  → 解析SEF头
  → 返回 SW_SUCCESS

APDU: LOAD [LINK]
  → 解析导入/导出表
  → 状态切换到 LINKING
  → 执行符号链接
  → 返回 SW_SUCCESS

APDU: INSTALL [AID, params]
  → 状态切换到 INSTALLING
  → 创建应用实例
  → 初始化应用数据
  → 状态切换到 COMPLETE
  → 返回 SW_SUCCESS + app_id
```

#### ✅ 模块3: 应用选择管理器 (`gcos_selector.c/h`)

**职责**:
- 处理SELECT/DESELECT命令
- 管理通道的选择状态
- 应用生命周期转换
- AID查找和验证

**关键功能**:
```c
GCOSSwStatus handle_select_apdu(GCOSVM *vm, const GCOSSApdu *apdu, 
                                 u8 *response, u16 *resp_len) {
    // 1. 解析AID
    const u8 *aid = apdu->data;
    u8 aid_length = apdu->lc;
    
    // 2. 查找应用
    s8 app_index = find_app_by_aid(vm, aid, aid_length);
    if (app_index < 0) {
        return make_sw_status(SW_APP_NOT_FOUND);
    }
    
    // 3. 检查生命周期状态
    GCOSAppInstance *app = vm->apps[app_index];
    if (!is_selectable(app->lifecycle)) {
        return make_sw_status(SW_CONDITION_NOT_SATISFIED);
    }
    
    // 4. 获取当前通道
    u8 channel = get_logical_channel(apdu->cla);
    
    // 5. 取消当前选择的应用
    deselect_current_app(vm, channel);
    
    // 6. 选择新应用
    select_app_on_channel(vm, channel, app_index);
    
    // 7. 返回FCI (File Control Information)
    build_fci_response(app, response, resp_len);
    
    return make_sw_status(SW_SUCCESS);
}
```

#### ✅ 模块4: 通道管理器增强 (`gcos_channel.c/h`)

**职责**:
- 管理8个逻辑通道
- 通道打开/关闭
- 通道间数据隔离
- 基本通道 (channel 0) 特殊处理

**增强功能**:
```c
typedef struct {
    u8 channel_id;              // 通道ID (0-7)
    bool is_open;               // 是否打开
    s8 selected_app_index;      // 选择的应用索引 (-1=未选择)
    u8 security_level;          // 当前安全级别
    u8 session_key[16];         // 会话密钥 (加密通道)
    u16 transaction_counter;    // 事务计数器
} LogicalChannel;

// 通道管理API
GCOSSwStatus open_logical_channel(GCOSVM *vm, u8 channel);
GCOSSwStatus close_logical_channel(GCOSVM *vm, u8 channel);
s8 get_selected_app(GCOSVM *vm, u8 channel);
void set_channel_security(GCOSVM *vm, u8 channel, u8 level);
```

---

## 5. 实施计划

### 阶段1: APDU基础设施 (优先级: 🔴 高)

**任务**:
1. ✅ 创建 `gcos_apdu.h/c` - APDU解析和分发框架
2. ✅ 定义APDU命令表结构
3. ✅ 实现状态码管理
4. ✅ 创建测试APDU序列

**预计工作量**: 2-3天

### 阶段2: 流式加载器 (优先级: 🔴 高)

**任务**:
1. ✅ 重构 `gcos_loader.c` 为状态机模式
2. ✅ 创建 `gcos_stream_loader.c` - 流式加载上下文管理
3. ✅ 实现LOAD APDU处理器
4. ✅ 实现INSTALL APDU处理器
5. ✅ 添加数据完整性验证

**预计工作量**: 3-4天

### 阶段3: 应用选择机制 (优先级: 🟡 中)

**任务**:
1. ✅ 创建 `gcos_selector.c` - SELECT/DESELECT处理
2. ✅ 增强通道管理与APDU集成
3. ✅ 实现FCI响应构建
4. ✅ 添加AID查找优化

**预计工作量**: 2-3天

### 阶段4: 事务与APDU集成 (优先级: 🟡 中)

**任务**:
1. ✅ 修改 `gcos_transaction.c` 支持APDU触发
2. ✅ 实现BEGIN TRANSACTION APDU
3. ✅ 实现COMMIT/ABORT APDU
4. ✅ 添加自动回滚机制

**预计工作量**: 2天

### 阶段5: 测试与验证 (优先级: 🟢 低)

**任务**:
1. ✅ 创建APDU测试套件
2. ✅ 模拟智能卡读卡器
3. ✅ 端到端加载/安装/执行测试
4. ✅ 性能基准测试

**预计工作量**: 3-4天

---

## 6. 代码示例

### 6.1 APDU主处理循环

```c
/**
 * @brief Main APDU processing entry point
 * @param vm VM instance
 * @param apdu_buffer Input APDU data
 * @param apdu_length APDU length
 * @param response_buffer Output response buffer
 * @param response_length Output response length
 * @return Status word (SW1SW2)
 */
u16 gcos_vm_process_apdu(GCOSVM *vm, const u8 *apdu_buffer, u8 apdu_length,
                         u8 *response_buffer, u16 *response_length) {
    
    // 1. Validate APDU length
    if (apdu_length < APDU_HEADER_MIN_LENGTH) {
        return SW_WRONG_LENGTH;
    }
    
    // 2. Parse APDU
    GCOSSApdu apdu;
    parse_apdu(apdu_buffer, apdu_length, &apdu);
    
    // 3. Extract logical channel from CLA
    u8 channel = apdu.cla & 0x03;
    vm->current_channel = channel;
    
    // 4. Check channel validity
    if (!vm->channels[channel].is_open && channel != 0) {
        return SW_CONDITION_NOT_SATISFIED;  // Channel not open
    }
    
    // 5. Dispatch to handler based on INS
    ApduHandler handler = find_apdu_handler(apdu.ins);
    if (handler == NULL) {
        return SW_INS_NOT_SUPPORTED;
    }
    
    // 6. Execute handler
    GCOSSwStatus status = handler(vm, &apdu, response_buffer, response_length);
    
    // 7. Return status word
    return (status.sw1 << 8) | status.sw2;
}
```

### 6.2 LOAD APDU处理器

```c
GCOSSwStatus handle_load_apdu(GCOSVM *vm, const GCOSSApdu *apdu,
                               u8 *response, u16 *resp_len) {
    
    StreamLoadContext *ctx = get_current_load_context(vm);
    
    switch (apdu->p1) {
        case LOAD_P1_INIT:
            return handle_load_init(vm, apdu, ctx);
        
        case LOAD_P1_DATA:
            return handle_load_data(vm, apdu, ctx);
        
        case LOAD_P1_LINK:
            return handle_load_link(vm, apdu, ctx);
        
        case LOAD_P1_ABORT:
            return handle_load_abort(vm, ctx);
        
        default:
            return make_sw_status(SW_WRONG_DATA);
    }
}

static GCOSSwStatus handle_load_data(GCOSVM *vm, const GCOSSApdu *apdu,
                                      StreamLoadContext *ctx) {
    
    // Verify sequence number
    if (apdu->p2 != ctx->block_sequence) {
        return make_sw_status(SW_WRONG_DATA);  // Wrong sequence
    }
    
    // Check buffer capacity
    if (ctx->received_size + apdu->lc > ctx->buffer_capacity) {
        return make_sw_status(SW_MEMORY_FAILURE);
    }
    
    // Copy data to buffer
    memcpy(ctx->buffer + ctx->received_size, apdu->data, apdu->lc);
    ctx->received_size += apdu->lc;
    ctx->block_sequence++;
    
    // Update checksum
    ctx->checksum += calculate_checksum(apdu->data, apdu->lc);
    
    // Check if this is the final block
    if (apdu->p1 & LOAD_P1_FINAL_FLAG) {
        ctx->state = STREAM_LOADER_VERIFYING;
    }
    
    return make_sw_status(SW_SUCCESS);
}
```

---

## 7. 与cref的关键对齐点

### 7.1 状态机设计对齐

| cref组件 | gcos_vm对应组件 | 对齐状态 |
|---------|----------------|---------|
| `t0.c` IO状态机 | `gcos_apdu.c` APDU状态机 | 🔄 待实现 |
| `native_install.c` LOAD状态机 | `gcos_stream_loader.c` | 🔄 待实现 |
| `lifecycle.c` 应用状态 | `gcos_selector.c` | 🔄 待实现 |
| `chn_manager.c` 通道管理 | `gcos_channel.c` | ✅ 已有基础 |
| `TransactionMgr.c` 事务 | `gcos_transaction.c` | ✅ 已有基础 |

### 7.2 APDU指令集对齐

| 指令 | cref实现 | COS3规范 | gcos_vm实现 |
|------|---------|---------|------------|
| LOAD (0xE8) | `native_GP.c` | 表45 | 🔄 待实现 |
| INSTALL (0xE6) | `native_install.c` | 表46 | 🔄 待实现 |
| DELETE (0xE4) | `native_GP.c` | 表47 | 🔄 待实现 |
| SELECT (0xA4) | `t0.c` | ISO7816-4 | 🔄 待实现 |
| GET STATUS (0xF2) | `native_GP.c` | 表48 | 🔄 待实现 |

---

## 8. 风险评估与缓解

### 8.1 技术风险

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| 流式加载缓冲区溢出 | 高 | 中 | 动态边界检查 + 事务回滚 |
| APDU状态机死锁 | 高 | 低 | 超时机制 + 看门狗 |
| 通道间数据泄露 | 高 | 低 | 严格的通道隔离验证 |
| 事务嵌套过深 | 中 | 中 | 限制最大嵌套深度 (4层) |

### 8.2 兼容性风险

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 与现有gcos_vm API不兼容 | 中 | 保留旧API作为包装层 |
| SEF文件格式变更 | 低 | 版本检测 + 向后兼容 |
| 指令集扩展困难 | 低 | 模块化指令表设计 |

---

## 9. 下一步行动

### 立即执行 (本周)

1. ✅ **创建APDU框架骨架**
   ```bash
   touch src/gcos_apdu.c include/gcos_apdu.h
   touch src/gcos_stream_loader.c include/gcos_stream_loader.h
   touch src/gcos_selector.c include/gcos_selector.h
   ```

2. ✅ **定义APDU命令表**
   - 列出所有必需的APDU指令
   - 定义处理函数签名
   - 创建命令注册机制

3. ✅ **实现LOAD状态机原型**
   - 基本的INIT/DATA/LINK状态
   - 简单的缓冲区管理
   - 最小化的错误处理

### 短期目标 (2周内)

1. ✅ 完成流式加载器完整实现
2. ✅ 集成APDU处理到主VM循环
3. ✅ 编写APDU测试用例
4. ✅ 验证与cref的行为一致性

### 中期目标 (1个月内)

1. ✅ 实现完整的SELECT/DESELECT机制
2. ✅ 增强通道管理与安全级别
3. ✅ 优化性能和内存使用
4. ✅ 编写完整的开发者文档

---

## 10. 参考资料

1. **GB/T 44901.3-XXXX** - COS3规范文档 (`cos3-qw.md`)
2. **cref源代码** - JavaCard参考实现 (`cref/`目录)
   - `adapter/win32/t0.c` - APDU传输层
   - `native/native_install.c` - 安装流程
   - `native/native_GP.c` - GP命令处理
   - `common/interpreter.c` - 字节码解释器
3. **ISO/IEC 7816-4** - APDU命令格式标准
4. **Global Platform Card Specification** - 卡片管理规范

---

## 附录A: APDU命令速查表

### A.1 管理指令

| INS | 名称 | 描述 | P1/P2 |
|-----|------|------|-------|
| 0xE8 | LOAD | 加载模块 | P1=模式, P2=序列号 |
| 0xE6 | INSTALL | 安装应用 | P1=标志, P2=00 |
| 0xE4 | DELETE | 删除应用 | P1=标志, P2=00 |
| 0xF2 | GET STATUS | 获取状态 | P1=对象类型, P2=标志 |

### A.2 应用指令

| INS | 名称 | 描述 | P1/P2 |
|-----|------|------|-------|
| 0xA4 | SELECT | 选择应用 | P1=选择方式, P2=出现控制 |
| 0xAA | DESELECT | 取消选择 | P1=00, P2=00 |
| 0xXX | CUSTOM | 自定义命令 | 由应用定义 |

### A.3 通道指令

| INS | 名称 | 描述 | P1/P2 |
|-----|------|------|-------|
| 0x70 | MANAGE CHANNEL | 管理通道 | P1=操作, P2=通道号 |

---

**文档结束**
