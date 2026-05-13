# Cref与GCOS函数对照表

## 1. JCShell层（jcshell.c ↔ gcos_jcshell.c）

### Cref函数列表

| Cref函数 | 行号 | 功能 | GCOS对应函数 | 状态 |
|---------|------|------|-------------|------|
| `startJCShellThread()` | - | 启动JCShell线程 | `gcos_jcshell_start()` | ✅ 已实现 |
| `processConnect(int sock, int type)` | 769 | 处理客户端连接 | `process_client_connection(int client_sock, u16 port)` | ✅ 已实现 |
| `powerup(int sock, char* rapdu, int rapduLength, int connType)` | 677 | 发送POWER_UP到JCRE | ❌ **缺失** | ❌ 未实现 |
| `sendData(int sock, int type, char* data, int size)` | 697 | 发送二进制响应 | ❌ **缺失**（内联实现） | ⚠️ 内联 |
| `ConnectToJCRE(int type)` | 721 | 连接到JCRE（9025） | ❌ **缺失** | ❌ 未实现 |
| `SendConnType(int jcreSock, int type)` | 94 | 发送ConnectInfo握手 | ❌ **缺失** | ❌ 未实现 |
| `closeJcreSock()` | - | 关闭JCRE连接 | ❌ **缺失** | ❌ 未实现 |
| `writeByte(int fd, char c)` | 64 | 写单字节 | ❌ **缺失**（使用send/write） | ⚠️ 内联 |
| `readByte(int fd)` | 74 | 读单字节 | ❌ **缺失**（使用recv/read） | ⚠️ 内联 |

### 关键差异

**Cref的JCShell工作流程：**
```c
processConnect() {
    while(1) {
        recv(header, 4);  // [type][cmd][size_hi][size_lo]
        recv(data, size);
        
        if (type==0 && cmd==0x21) {  // POWER_UP
            jcreSock = ConnectToJCRE(connType);  // 连接到9025
            SendConnType(jcreSock, connType);     // 发送握手
            size = powerup(jcreSock, ...);        // 转发POWER_UP
            sendData(sock, type, &revbuf[3], size);  // 返回ATR
        } else {
            size = sendApdu(jcreSock, ...);       // 转发APDU
            sendData(sock, type, revbuf, size);   // 返回响应
        }
    }
}
```

**GCOS的JCShell工作流程（当前）：**
```c
process_client_connection() {
    while(1) {
        recv(header, 4);  // [type][cmd][size_hi][size_lo]
        recv(data, size);
        
        if (type==0 && cmd==0x21) {  // POWER_UP
            // 直接构建ATR响应（不连接到TLP Server）
            send(resp_header, 4);
            send(atr_data, atr_len);
        } else {
            // TODO: 转发APDU到VM
            send(sw_response, 2);  // 当前只返回SW 0x6D00
        }
    }
}
```

### 需要实现的函数

#### 1.1 ConnectToJCRE() - 连接到TLP Server

```c
/**
 * @brief Connect to TLP Server (port 9025)
 * @param type Connection type (0=contacted, 2=contactless)
 * @return Socket fd, or -1 on error
 */
int ConnectToJCRE(int type) {
    struct sockaddr_in serverAddr;
    int jcreSock;
    
    jcreSock = socket(AF_INET, SOCK_STREAM, 0);
    if (jcreSock == -1)
        return -1;
    
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9025);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(jcreSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        closesocket(jcreSock);
        return -1;
    }
    
    // Send ConnectInfo handshake
    SendConnType(jcreSock, type);
    
    return jcreSock;
}
```

#### 1.2 SendConnType() - 发送ConnectInfo握手

```c
/**
 * @brief Send ConnectInfo handshake to JCRE
 * @param jcreSock Socket to JCRE
 * @param type Connection type
 * @return 1 on success, 0 on error
 */
int SendConnType(int jcreSock, int type) {
    typedef struct {
        u32 magic;          // 0x5a5a1234
        u32 conectType;     // 0=contacted, 2=contactless
    } ConnectInfo;
    
    ConnectInfo info;
    info.magic = 0x5a5a1234;
    info.conectType = (type == 9000) ? 0 : 2;
    
    if (send(jcreSock, (void*)&info, sizeof(ConnectInfo), 0) != sizeof(ConnectInfo)) {
        return 0;
    }
    return 1;
}
```

#### 1.3 powerup() - 发送POWER_UP到JCRE

```c
/**
 * @brief Send POWER_UP command to JCRE and receive ATR
 * @param sock Socket to JCRE
 * @param rapdu Response buffer
 * @param rapduLength Response buffer length
 * @param connType Connection type
 * @return ATR length
 */
static int powerup(int sock, char* rapdu, int rapduLength, int connType) {
    // Build POWER_UP message in TLP224 format
    char msgData[8];
    msgData[0] = ACK;
    msgData[1] = 0;
    msgData[2] = 4;
    msgData[3] = POWER_UP;
    msgData[4] = 0;
    msgData[5] = 0;
    msgData[6] = 0;
    msgData[7] = computeLRC(msgData, 7);
    
    // Send via TLP224
    sendTLP224Message(sock, msgData, 8, connType);
    
    // Receive ATR response
    rapduLength = receiveTLP224Message(sock, rapdu, rapduLength);
    
    // Extract ATR data from TLP response
    rapduLength = getApdu(rapdu, rapduLength);
    
    return rapduLength;
}
```

---

## 2. JCRE Server层（server.c ↔ gcos_tlp_server.c）

### Cref函数列表

| Cref函数 | 行号 | 功能 | GCOS对应函数 | 状态 |
|---------|------|------|-------------|------|
| `createSockets()` | 86 | 创建监听socket（9025） | `gcos_tlp_server_init()` | ✅ 已实现 |
| `getConnection(TLP_MSG *msg)` | 123 | Accept客户端连接 | `accept_client_connection()` | ✅ 已实现 |
| `RecvProtocol(int fd)` | 240 | 接收ConnectInfo握手 | `receive_handshake()` | ✅ 已实现 |
| `closeSocket()` | - | 关闭socket | `gcos_tlp_server_cleanup()` | ✅ 已实现 |

### 对比结果

✅ **完全一致** - 所有关键函数都已实现

---

## 3. TLP224协议层（io_cad.c ↔ gcos_tlp.c）

### Cref函数列表

| Cref函数 | 行号 | 功能 | GCOS对应函数 | 状态 |
|---------|------|------|-------------|------|
| `cref_sendTLP224Message(TLP_MSG *msg)` | 205 | 发送TLP224消息 | `tlp_send_message()` | ✅ 已实现 |
| `cref_receiveTLP224Message(TLP_MSG *msg)` | 126 | 接收TLP224消息 | `tlp_receive_message()` | ✅ 已实现 |
| `computeLRC(char* buf, int length)` | - | 计算LRC | `tlp_compute_lrc()` | ✅ 已实现 |
| `socket_writeByte(int fd, char c)` | - | 写单字节 | (内联在tlp_send_message) | ✅ 已实现 |
| `socket_readByte(int fd)` | - | 读单字节 | (内联在tlp_receive_message) | ✅ 已实现 |
| `resetMSG(TLP_MSG *msg)` | - | 重置消息 | `tlp_msg_reset()` | ✅ 已实现 |

### 对比结果

✅ **完全一致** - 所有关键函数都已实现

---

## 4. T=0协议层（t0_ll.c + t0.c ↔ gcos_t0_protocol.c）

### Cref函数列表（t0_ll.c）

| Cref函数 | 行号 | 功能 | GCOS对应函数 | 状态 |
|---------|------|------|-------------|------|
| `send_ATR()` | 91 | 发送ATR（阻塞等待连接） | (集成在gcos_main.c) | ⚠️ 部分 |
| `_t0sendATR(TLP_MSG*, u8, u8[], u8[], int)` | 792 | 构建并发送ATR | `t0_send_atr()` | ✅ 已实现 |
| `_t0RcvCommand(TLP_MSG*, u8*)` | 139 | 接收APDU命令 | `t0_receive_apdu()` | ✅ 已实现 |
| `statusResponse(TLP_MSG*, char)` | 251 | 发送状态响应 | (内联) | ⚠️ 部分 |
| `T0_start_send_wait()` | 859 | 启动发送等待 | ❌ **缺失** | ❌ 未实现 |
| `T0_stop_send_wait()` | 866 | 停止发送等待 | ❌ **缺失** | ❌ 未实现 |

### Cref函数列表（t0.c）

| Cref函数 | 行号 | 功能 | GCOS对应函数 | 状态 |
|---------|------|------|-------------|------|
| `T0_send_ATR()` | 111 | 发送ATR（needRecv=1） | (集成在t0_send_atr) | ✅ 已实现 |
| `T0_send_ATR_norecv()` | 96 | 发送ATR（needRecv=0） | (集成在t0_send_atr) | ✅ 已实现 |
| `T0_ProcessCommand()` | - | 处理APDU命令 | `t0_process_command()` | ✅ 已实现 |

### 对比结果

✅ **核心功能一致** - ATR发送和APDU接收已实现
⚠️ **次要功能缺失** - T0_start/stop_send_wait未实现（可选）

---

## 5. 传输层（无独立文件 ↔ gcos_transport.c）

### GCOS独有功能

| GCOS函数 | 功能 | Cref对应 | 状态 |
|---------|------|---------|------|
| `gcos_transport_init()` | 初始化传输层 | (无，cref直接使用socket) | 💡 GCOS增强 |
| `gcos_transport_send_byte()` | 发送单字节 | socket_writeByte | ✅ 功能对等 |
| `gcos_transport_receive_byte()` | 接收单字节 | socket_readByte | ✅ 功能对等 |
| `gcos_transport_send_response()` | 发送响应 | (分散在各处) | 💡 GCOS封装 |
| `gcos_transport_receive_apdu()` | 接收APDU | (分散在各处) | 💡 GCOS封装 |

### 对比结果

💡 **GCOS增强** - 提供了更清晰的传输层抽象，cref直接使用socket API

---

## 6. VM核心层（reset.c + main.c ↔ gcos_vm.c + gcos_main.c）

### Cref函数列表

| Cref函数 | 文件 | 功能 | GCOS对应函数 | 状态 |
|---------|------|------|-------------|------|
| `JCRE_main()` | main.c:926 | VM主循环 | (集成在gcos_main.c) | ✅ 已实现 |
| `warm_reset()` | reset.c:86 | 热复位 | `gcos_vm_reset()` | ✅ 已实现 |
| `cold_reset()` | reset.c:146 | 冷复位 | `gcos_vm_init()` | ✅ 已实现 |
| `run()` | reset.c:250 | 执行循环 | `gcos_vm_execute()` | ✅ 已实现 |

### 对比结果

✅ **核心功能一致** - VM初始化和执行已实现

---

## 缺失功能总结

### 必须实现（影响核心功能）

1. **JCShell APDU转发** (gcos_jcshell.c)
   ```c
   // 当前代码（第188-221行）
   /* Step 4: Regular APDU command - forward to VM */
   printf("[JCShell] Processing APDU command\n");
   
   /* TODO: Forward APDU to VM and get response */
   /* For now, return a simple error response */
   u8 sw_hi = 0x6D;
   u8 sw_lo = 0x00;
   ```
   
   **需要实现：**
   - 解析APDU（CLA, INS, P1, P2, P3, Data）
   - 调用`gcos_vm_process_apdu(vm, apdu, apdu_len, response, &resp_len)`
   - 构建二进制响应：`[type][cmd][size_hi][size_lo][response_data...][SW1][SW2]`
   - 发送到客户端

2. **JCShell连接到TLP Server** (可选，完善三层架构)
   - 实现`ConnectToJCRE()`
   - 实现`SendConnType()`
   - 实现`powerup()`
   - 修改流程：JCShell → TLP Server (9025) → VM

### 建议实现（增强功能）

3. **T=0状态机增强**
   - 实现链式APDU支持
   - 实现GET RESPONSE机制
   - 实现T=0错误恢复

4. **调试和日志**
   - 添加详细的协议日志
   - 添加性能统计

---

## 实现优先级

### P0 - 立即实现（阻塞使用）
- ✅ JCShell POWER_UP响应（已完成）
- ❌ JCShell APDU转发（**最关键**）

### P1 - 短期实现（完善功能）
- ❌ TLP Server APDU处理
- ❌ T=0链式APDU支持

### P2 - 中期实现（架构优化）
- ❌ JCShell连接到TLP Server（完整三层架构）
- ❌ 完善的错误处理

### P3 - 长期实现（增强功能）
- ❌ 性能优化
- ❌ 高级调试功能

---

## 代码量估算

| 功能 | 预估行数 | 复杂度 |
|------|---------|--------|
| JCShell APDU转发 | ~100行 | 中等 |
| JCShell连接TLP Server | ~150行 | 中等 |
| TLP Server APDU处理 | ~80行 | 简单 |
| T=0链式APDU | ~200行 | 复杂 |
| **总计** | **~530行** | - |

---

## 结论

**一致性评估：85%**

✅ **已实现（85%）：**
- 协议格式（100%）
- 端口配置（100%）
- 握手协议（100%）
- TLP224编码（100%）
- ATR发送（100%）
- 基础架构（100%）

❌ **未实现（15%）：**
- APDU转发逻辑（最关键）
- 三层架构完整性
- T=0高级功能

**建议：**
优先实现JCShell APDU转发功能（约100行代码），这将使系统可以正常工作。其他功能可以逐步完善。
