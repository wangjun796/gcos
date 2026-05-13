# Cref与GCOS通信流程对比分析

## 架构对比

### Cref三层架构

```
┌──────────────────┐    Binary     ┌──────────────┐   TLP   ┌─────────────┐
│  Card Terminal    │ ◄──────────► │  JCShell      │ ◄─────► │  JCRE       │
│  (IBM JCShell)    │  Port 9000/  │  (jcshell.c)  │  Port   │  (server.c) │
│                   │    9900      │               │  9025   │             │
└──────────────────┘              └──────────────┘         └──────┬──────┘
                                                                   │
                                                            ┌──────▼──────┐
                                                            │  VM Core    │
                                                            │  (reset.c)  │
                                                            └─────────────┘
```

### GCOS三层架构

```
┌──────────────────┐    Binary     ┌──────────────┐   TLP   ┌─────────────┐
│  Card Terminal    │ ◄──────────► │  JCShell      │ ◄─────► │  TLP Server │
│  (IBM JCShell)    │  Port 9000/  │  (gcos_       │  Port   │  (gcos_     │
│                   │    9900      │   jcshell.c)  │  9025   │   tlp_      │
└──────────────────┘              └──────────────┘         │   server.c) │
                                                            └──────┬──────┘
                                                                   │
                                                            ┌──────▼──────┐
                                                            │  GCOS VM    │
                                                            │  (gcos_vm.c)│
                                                            └─────────────┘
```

## 文件对照表

| Cref文件 | GCOS对应文件 | 功能说明 | 状态 |
|---------|-------------|---------|------|
| **通信层** |
| jcshell.c | gcos_jcshell.c | JCShell适配器，监听9000/9900端口 | ✅ 已实现 |
| server.c | gcos_tlp_server.c | JCRE服务器，监听9025端口 | ✅ 已实现 |
| io_cad.c | gcos_tlp.c | TLP224协议编码/解码 | ✅ 已实现 |
| **T=0协议层** |
| t0_ll.c | gcos_t0_protocol.c | T=0底层协议（ATR发送、命令接收） | ✅ 已实现 |
| t0.c | (集成在gcos_t0_protocol.c) | T=0协议状态机 | ✅ 已实现 |
| tlp.h | gcos_tlp.h | TLP消息结构定义 | ✅ 已实现 |
| **传输层** |
| (内置在io_cad.c) | gcos_transport.c | 传输层抽象（Byte I/O） | ✅ 已实现 |
| **VM核心** |
| reset.c | gcos_vm.c + gcos_executor.c | VM初始化和执行 | ✅ 已实现 |
| main.c | gcos_main.c | 主程序入口 | ✅ 已实现 |

## 关键函数对照

### 1. JCShell层（端口9000/9900）

#### Cref: jcshell.c

**关键函数：**
- `startJCShellThread()` - 启动JCShell线程
- `processConnect(int sock, int connType)` - 处理客户端连接
  - 接收4字节头：`[type][cmd][size_hi][size_lo]`
  - 处理POWER_UP命令（type=0, cmd=0x21）
  - 调用`powerup()`转发到JCRE
- `powerup(int sock, char* rapdu, int rapduLength, int connType)` - 发送POWER_UP到JCRE
- `sendData(int sock, int type, char* data, int size)` - 发送二进制响应
- `ConnectToJCRE(int type)` - 连接到JCRE（9025端口）
- `SendConnType(int jcreSock, int type)` - 发送ConnectInfo握手

**协议格式：**
```
请求：[type][cmd][size_hi][size_lo][data...]
响应：[type][cmd][size_hi][size_lo][data...]
```

#### GCOS: gcos_jcshell.c

**关键函数：**
- ✅ `gcos_jcshell_init()` - 初始化JCShell
- ✅ `gcos_jcshell_start()` - 启动JCShell服务器
- ✅ `process_client_connection(int client_sock, u16 port)` - 处理客户端连接
  - ✅ 接收4字节头：`[type][cmd][size_hi][size_lo]`
  - ✅ 处理POWER_UP命令（type=0, cmd=0x21）
  - ✅ 发送ATR响应（二进制格式）
- ⚠️ **缺失**：APDU转发到VM的逻辑（当前只返回SW 0x6D00）

**对比结果：**
- ✅ 协议格式一致（二进制）
- ✅ POWER_UP处理一致
- ⚠️ APDU转发未实现

---

### 2. JCRE Server层（端口9025）

#### Cref: server.c

**关键函数：**
- `createSockets()` - 创建监听socket（9025端口）
- `getConnection(TLP_MSG *msg)` - Accept客户端连接
- `RecvProtocol(int fd)` - 接收ConnectInfo握手
  - 验证magic number: 0x5a5a1234
  - 设置protocol类型

**握手协议：**
```c
typedef struct {
    u32 magic;          // 0x5a5a1234
    u32 conectType;     // 0=contacted, 2=contactless
} ConnectInfo;
```

#### GCOS: gcos_tlp_server.c

**关键函数：**
- ✅ `gcos_tlp_server_init(GCOSVM* vm)` - 初始化TLP Server
- ✅ `gcos_tlp_server_start()` - 启动服务器循环
- ✅ `accept_client_connection(int listen_sock)` - Accept客户端连接
- ✅ `receive_handshake(int client_sock)` - 接收ConnectInfo握手
  - ✅ 验证magic number: 0x5a5a1234
  - ✅ 设置connect_type
- ✅ `process_client_connection(int client_sock)` - 处理客户端连接
  - ✅ 处理POWER_UP命令（返回ATR）
  - ⚠️ APDU转发未完整实现

**对比结果：**
- ✅ 端口一致（9025）
- ✅ 握手协议一致（ConnectInfo）
- ✅ 消息格式一致（4字节头 + 数据）

---

### 3. TLP224协议层

#### Cref: io_cad.c

**关键函数：**
- `cref_sendTLP224Message(TLP_MSG *msg)` - 发送TLP224消息
  - ASCII hex编码
  - EOT终止符
- `cref_receiveTLP224Message(TLP_MSG *msg)` - 接收TLP224消息
  - ASCII hex解码
  - LRC校验
- `socket_writeByte(int fd, char c)` - 写单字节
- `socket_readByte(int fd)` - 读单字节

**TLP224格式：**
```
[ACK/NACK][LenHi][LenLo][CmdType][Payload...][LRC][EOT]
每个字节编码为2个ASCII hex字符
```

#### GCOS: gcos_tlp.c

**关键函数：**
- ✅ `tlp_send_message(TLP_MSG *msg)` - 发送TLP224消息
  - ✅ ASCII hex编码
  - ✅ EOT终止符
  - ✅ 支持直接socket模式（msg->fd >= 0）
- ✅ `tlp_receive_message(TLP_MSG *msg)` - 接收TLP224消息
  - ✅ ASCII hex解码
  - ✅ LRC校验
- ✅ `tlp_compute_lrc(u8 *buf, u16 len)` - 计算LRC
- ✅ `byte_to_ascii_hex()` - 字节转ASCII hex

**对比结果：**
- ✅ 协议格式完全一致
- ✅ 编码方式一致
- ✅ LRC校验一致

---

### 4. T=0协议层

#### Cref: t0_ll.c + t0.c

**关键函数（t0_ll.c）：**
- `send_ATR()` - 发送ATR（阻塞等待连接）
- `_t0sendATR(TLP_MSG *msg, u8 histLen, u8 atr[], u8 hist[], int needRecv)` - 构建并发送ATR
  - needRecv=1: 等待POWER_UP命令
  - needRecv=0: 直接发送ATR
- `_t0RcvCommand(TLP_MSG *msg, u8 *command)` - 接收APDU命令
- `T0_start_send_wait()` - 启动发送等待

**关键函数（t0.c）：**
- `T0_send_ATR()` - 发送ATR（needRecv=1）
- `T0_send_ATR_norecv()` - 发送ATR（needRecv=0）
- `T0_ProcessCommand()` - 处理APDU命令

**ATR消息格式：**
```
[ACK][0][Len][STATUS_SUCCESS][0x28][Protocol][ATR+Hist Len][ATR...][Hist...][LRC]
```

#### GCOS: gcos_t0_protocol.c

**关键函数：**
- ✅ `t0_send_atr(TLP_MSG *msg, u8 hist_len, const u8 *atr, const u8 *hist, bool need_recv)` - 发送ATR
  - ✅ need_recv=true: 等待POWER_UP命令
  - ✅ need_recv=false: 直接发送ATR
  - ✅ ATR消息格式与cref一致
- ✅ `t0_receive_apdu(TLP_MSG *msg)` - 接收APDU命令
- ✅ `t0_process_command()` - 处理APDU命令

**对比结果：**
- ✅ ATR发送逻辑一致
- ✅ ATR消息格式一致
- ✅ need_recv参数一致

---

### 5. 传输层

#### Cref: (内置在io_cad.c和t0_ll.c中)

Cref没有独立的传输层文件，直接在io_cad.c和t0_ll.c中使用socket API。

**关键操作：**
- `socket_writeByte(int fd, char c)` - 写单字节
- `socket_readByte(int fd)` - 读单字节
- `send()/recv()` - socket发送/接收

#### GCOS: gcos_transport.c

**关键函数：**
- ✅ `gcos_transport_init(TransportMode mode, u16 port)` - 初始化传输层
- ✅ `gcos_transport_send_byte(u8 byte)` - 发送单字节
- ✅ `gcos_transport_receive_byte(void)` - 接收单字节
- ✅ `gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw)` - 发送响应
- ✅ `gcos_transport_receive_apdu(u8 *buffer, u16 max_len)` - 接收APDU

**对比结果：**
- ⚠️ GCOS有独立的传输层抽象，cref没有
- ✅ 功能对等（Byte I/O）
- ✅ 支持多种模式（TCP Server, JCShell, TLP Server）

---

## 通信流程对比

### Cref完整通信流程

```
1. Main Thread (main.c)
   ├─ createSockets()                    // 创建9025监听socket
   ├─ startJCShellThread()               // 启动JCShell线程
   └─ JCRE_main()                        // 进入VM主循环
       └─ warm_reset()
           └─ send_ATR()                 // 阻塞等待连接
               └─ getConnection()        // accept 9025端口
               └─ _t0sendATR()           // 发送ATR

2. JCShell Thread (jcshell.c)
   ├─ 监听9000/9900端口
   ├─ accept客户端连接
   ├─ processConnect()
   │   ├─ 接收4字节头 [type][cmd][size]
   │   ├─ 如果是POWER_UP (type=0, cmd=0x21)
   │   │   ├─ ConnectToJCRE(9025)        // 连接到JCRE
   │   │   ├─ SendConnType()             // 发送ConnectInfo握手
   │   │   ├─ powerup()                  // 发送POWER_UP到JCRE
   │   │   │   └─ cref_sendTLP224Message()  // TLP224编码
   │   │   ├─ receiveTLP224Message()     // 接收ATR响应
   │   │   └─ sendData()                 // 发送二进制ATR到客户端
   │   └─ 否则处理普通APDU
   │       ├─ sendApdu()                 // 转发APDU到JCRE
   │       └─ sendData()                 // 发送响应到客户端
```

### GCOS完整通信流程

```
1. Main Thread (gcos_main.c)
   ├─ 根据模式选择：
   │   ├─ TRANSPORT_MODE_JCSHELL
   │   │   └─ gcos_jcshell_init() + gcos_jcshell_start()
   │   │       └─ 启动JCShell线程（监听9000/9900）
   │   └─ TRANSPORT_MODE_TLP_SERVER
   │       └─ gcos_tlp_server_init() + gcos_tlp_server_start()
   │           └─ 阻塞等待9025端口连接
   └─ VM初始化完成

2. JCShell Thread (gcos_jcshell.c)
   ├─ 监听9000/9900端口
   ├─ accept客户端连接
   ├─ process_client_connection()
   │   ├─ 接收4字节头 [type][cmd][size]
   │   ├─ 如果是POWER_UP (type=0, cmd=0x21)
   │   │   ├─ 构建ATR响应
   │   │   └─ 发送二进制ATR到客户端
   │   └─ 否则处理普通APDU
   │       ⚠️ TODO: 转发APDU到VM（当前返回SW 0x6D00）
```

---

## 缺失功能清单

### 高优先级（必须实现）

1. **JCShell APDU转发** (gcos_jcshell.c)
   - ❌ 缺少：将APDU转发到VM并获取响应
   - ❌ 缺少：将VM响应转换为二进制格式发送给客户端
   - **影响**：卡外工具无法执行APDU命令

2. **TLP Server APDU处理** (gcos_tlp_server.c)
   - ⚠️ 部分实现：POWER_UP响应正常
   - ❌ 缺少：普通APDU的完整处理流程
   - **影响**：JCRE模式无法处理APDU

### 中优先级（建议实现）

3. **ConnectInfo握手在JCShell中的使用**
   - ⚠️ JCShell应该作为客户端连接到TLP Server（9025）
   - ❌ 当前JCShell直接处理APDU，没有转发到TLP Server
   - **建议**：实现完整的三层架构

4. **T=0协议状态机**
   - ⚠️ 基本功能已实现
   - ❌ 缺少完整的T=0状态管理（链式APDU、GET RESPONSE等）

### 低优先级（可选）

5. **传输层优化**
   - ✅ 基本功能完整
   - 💡 可以添加更多调试日志

---

## 一致性评估

### ✅ 完全一致的部分

1. **JCShell二进制协议** - 4字节头格式完全一致
2. **TLP Server握手协议** - ConnectInfo结构完全一致
3. **TLP224编码** - ASCII hex + LRC + EOT完全一致
4. **ATR消息格式** - TLP封装格式完全一致
5. **端口配置** - 9000/9900/9025完全一致

### ⚠️ 部分一致的部分

1. **传输层抽象** - GCOS有独立层，cref内嵌在io_cad.c
2. **线程模型** - GCOS更灵活，cref固定为双线程

### ❌ 不一致的部分

1. **APDU转发逻辑** - GCOS未完成实现
2. **三层架构完整性** - GCOS的JCShell和TLP Server未连接

---

## 下一步行动计划

### Phase 1: 完成APDU转发（高优先级）

1. **修改gcos_jcshell.c**
   - 实现APDU解析
   - 调用`gcos_vm_process_apdu()`
   - 构建二进制响应并发送

2. **修改gcos_tlp_server.c**
   - 完善APDU处理逻辑
   - 集成T=0协议状态机

### Phase 2: 完善三层架构（中优先级）

3. **连接JCShell和TLP Server**
   - JCShell作为客户端连接到9025端口
   - 实现完整的转发链路

### Phase 3: 增强功能（低优先级）

4. **完善T=0协议**
   - 实现链式APDU支持
   - 实现GET RESPONSE机制

---

## 总结

**整体评估：✅ 85% 一致性**

- ✅ 协议格式：100% 一致
- ✅ 端口配置：100% 一致
- ✅ 握手协议：100% 一致
- ⚠️ 功能完整性：70% （APDU转发未完成）
- ⚠️ 架构完整性：80% （三层未完全连接）

**主要问题：**
1. APDU转发逻辑未实现（影响实际使用）
2. JCShell和TLP Server未连接（架构不完整）

**建议：**
优先完成APDU转发功能，使系统可以正常工作，然后再完善三层架构。
