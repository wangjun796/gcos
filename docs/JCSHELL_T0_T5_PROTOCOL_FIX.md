# JCShell T0/T5协议支持问题分析与修复方案

## ⚠️ 严重问题发现

**当前GCOS实现无法正确区分T0和T5协议！**

### 问题描述

虽然GCOS监听了两个端口（9000和9900），但`process_client_connection()`函数**完全没有使用port参数来区分协议类型**。所有APDU都直接调用`gcos_vm_process_apdu()`，VM无法知道这是T0还是T5连接。

### Cref的正确实现

Cref通过以下机制确保JCRE知道连接类型：

```c
// cref/jcshell.c line 94-110
int SendConnType(int jcreSock, int type)
{
    ConnectInfo info;
    info.magic = 0x5a5a1234;
    
    if(type == CONTACTED_PORT)      // 9000
        info.conectType = 0;         // T=0协议
    else                             // 9900
        info.conectType = 2;         // T=CL协议
    
    send(jcreSock, (void*)&info, sizeof(ConnectInfo), 0);
    return 1;
}
```

**关键点：**
1. 每次发送APDU前，先发送ConnectInfo握手包
2. `conectType = 0` 表示T=0（contacted）
3. `conectType = 2` 表示T=CL（contactless）
4. JCRE根据连接类型选择不同的处理逻辑

---

## 📊 架构对比

### Cref的完整流程

```
卡外工具 → JCShell (9000/9900) → [SendConnType] → JCRE (9025) → VM
                                    ↓
                              告知连接类型(T0/T5)
```

**关键步骤：**
1. 卡外工具连接9000或9900
2. JCShell连接到JCRE（9025）
3. **JCShell发送ConnectInfo握手包**（magic: 0x5a5a1234, conectType: 0或2）
4. JCRE保存连接类型
5. 后续APDU处理时，JCRE根据连接类型选择T0或T5协议栈

### GCOS当前的错误流程

```
卡外工具 → JCShell (9000/9900) → 直接调用 → VM
                                    ❌
                            没有告知连接类型！
```

**问题：**
- ❌ 没有ConnectInfo握手
- ❌ VM不知道是T0还是T5连接
- ❌ 无法正确处理不同协议的APDU

---

## 🔧 修复方案

### 方案1：保持简化架构（推荐）

**思路：** 在直接调用VM时传递连接类型参数

**需要修改：**

#### 1. 扩展VM APDU处理接口

```c
// gcos_vm.h
typedef enum {
    GCOS_CONN_TYPE_T0 = 0,   /**< T=0 protocol (contacted, port 9000) */
    GCOS_CONN_TYPE_T5 = 2    /**< T=CL protocol (contactless, port 9900) */
} GCOSConnType;

/**
 * @brief Process APDU with connection type
 * 
 * @param vm            VM instance
 * @param apdu          APDU data
 * @param apdu_len      APDU length
 * @param response      Response buffer
 * @param response_len  Response length (in/out)
 * @param conn_type     Connection type (T0 or T5)
 * @return              Status word (SW)
 */
u16 gcos_vm_process_apdu_with_conn_type(GCOSVM* vm, const u8* apdu, u16 apdu_len,
                                        u8* response, u16* response_len,
                                        GCOSConnType conn_type);
```

#### 2. 修改JCShell传递连接类型

```c
// gcos_jcshell.c - process_client_connection()

static int process_client_connection(int client_sock, u16 port) {
    
    /* Determine connection type based on port */
    GCOSConnType conn_type;
    if (port == 9000) {
        conn_type = GCOS_CONN_TYPE_T0;
        printf("[JCShell] Connection type: T=0 (contacted)\n");
    } else if (port == 9900) {
        conn_type = GCOS_CONN_TYPE_T5;
        printf("[JCShell] Connection type: T=CL (contactless)\n");
    } else {
        printf("[JCShell] ERROR: Unknown port %u\n", port);
        return -1;
    }
    
    while (1) {
        // ... receive header and data ...
        
        /* Step 4: Regular APDU command - forward to VM with connection type */
        printf("[JCShell] Processing APDU command (%u bytes, conn_type=%d)\n", 
               data_size, conn_type);
        
        extern GCOSVM* gcos_vm_get_instance(void);
        GCOSVM* vm = gcos_vm_get_instance();
        
        if (vm == NULL) {
            // ... error handling ...
            continue;
        }
        
        /* Process APDU through VM with connection type */
        u8 response_buffer[RESPONSE_BUFFER_SIZE];
        memset(response_buffer, 0, RESPONSE_BUFFER_SIZE);
        u16 response_length = RESPONSE_BUFFER_SIZE;
        
        // ✅ 关键修改：传递连接类型
        u16 sw = gcos_vm_process_apdu_with_conn_type(
            vm, apdu_buffer, data_size,
            response_buffer, &response_length,
            conn_type
        );
        
        // ... send response ...
    }
}
```

#### 3. VM内部实现连接类型处理

```c
// gcos_vm_core.c

u16 gcos_vm_process_apdu_with_conn_type(GCOSVM* vm, const u8* apdu, u16 apdu_len,
                                        u8* response, u16* response_len,
                                        GCOSConnType conn_type) {
    if (vm == NULL || apdu == NULL || response == NULL || response_len == NULL) {
        return 0x6F00;  // No precise diagnosis
    }
    
    /* Save connection type in VM context */
    vm->current_conn_type = conn_type;
    
    printf("[VM] Processing APDU with connection type: %s\n",
           conn_type == GCOS_CONN_TYPE_T0 ? "T=0" : "T=CL");
    
    /* Route to appropriate protocol handler */
    if (conn_type == GCOS_CONN_TYPE_T0) {
        /* Use T=0 protocol stack */
        return t0_process_apdu(vm, apdu, apdu_len, response, response_len);
    } else if (conn_type == GCOS_CONN_TYPE_T5) {
        /* Use T=CL protocol stack */
        return tcl_process_apdu(vm, apdu, apdu_len, response, response_len);
    } else {
        return 0x6F00;
    }
}
```

---

### 方案2：完整实现三层架构（不推荐）

**思路：** 完全按照cref实现，JCShell通过网络连接到TLP Server

**优点：**
- ✅ 与cref 100%一致
- ✅ 可以独立部署JCRE和JCShell

**缺点：**
- ❌ 增加网络开销
- ❌ 代码复杂度大幅增加
- ❌ 不适合嵌入式环境
- ❌ 需要实现完整的TLP224客户端

**不推荐理由：** 当前简化架构已经工作正常，只需添加连接类型传递即可。

---

## 📋 实施计划

### Phase 1: 扩展VM接口（1小时）

1. 在`gcos_vm.h`中添加`GCOSConnType`枚举
2. 在`GCOSVM`结构体中添加`current_conn_type`字段
3. 实现`gcos_vm_process_apdu_with_conn_type()`函数

### Phase 2: 修改JCShell（30分钟）

1. 在`process_client_connection()`中根据port确定conn_type
2. 调用新的VM接口传递连接类型
3. 添加日志输出便于调试

### Phase 3: VM协议路由（2小时）

1. 实现T=0协议处理函数`t0_process_apdu()`
2. 实现T=CL协议处理函数`tcl_process_apdu()`
3. 根据conn_type路由到不同的协议栈

### Phase 4: 测试验证（1小时）

1. 测试9000端口（T=0）连接
2. 测试9900端口（T=CL）连接
3. 同时连接两个端口验证并发处理
4. 验证VM正确识别连接类型

---

## 🎯 预期效果

修复后，GCOS将能够：

✅ **正确区分T0和T5协议**
- 9000端口 → T=0协议处理
- 9900端口 → T=CL协议处理

✅ **同时支持两个连接**
- 可以同时有T0和T5客户端连接
- VM根据连接类型选择正确的协议栈

✅ **与cref功能等价**
- 虽然架构简化（无网络层）
- 但功能完全一致
- 协议处理逻辑相同

---

## 💡 总结

**核心问题：** GCOS当前实现缺少连接类型传递机制

**解决方案：** 在直接调用VM时传递连接类型参数（方案1）

**工作量：** 约4.5小时

**风险：** 低（仅扩展接口，不影响现有功能）

**建议：** 立即实施方案1，保持简化架构的优势，同时获得正确的协议区分能力。
