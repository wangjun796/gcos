# 完整T=0协议和TLP224实现文档

## 📋 概述

本文档描述GCOS VM中完整的ISO7816-3 T=0协议和TLP224传输层协议的实现。该实现基于cref（JavaCard参考实现）的架构，将所有中文注释转换为英文，并修复了乱码问题。

---

## 🎯 设计目标

1. **完整的T=0协议栈**: 实现ISO7816-3定义的字节级T=0协议
2. **TLP224消息封装**: 支持TLP224协议帧格式（用于TCP通信）
3. **规范化代码**: 整理cref的相关代码，提取核心功能，所有注释转为英文
4. **双模式支持**: 
   - STDIO模式（原始APDU hex字符串）
   - TCP模式（TLP224封装）
5. **零动态内存分配**: 符合COS3规范要求

---

## 🏗️ 架构设计

### 模块结构

```
gcos_vm/
├── include/
│   ├── gcos_tlp.h              # TLP消息结构和常量定义
│   ├── gcos_t0_protocol.h      # T=0协议API定义
│   └── gcos_transport.h        # 传输层API定义
├── src/
│   ├── gcos_tlp.c              # TLP消息处理（LRC计算等）
│   ├── gcos_t0_protocol.c      # 完整T=0协议实现
│   ├── gcos_transport.c        # 传输层（STDIO/TCP）
│   └── gcos_main.c             # 主处理循环
└── docs/
    └── COMPLETE_T0_IMPLEMENTATION.md  # 本文档
```

### 协议栈层次

```
┌─────────────────────────────────────┐
│   Application Layer (GCOS VM)       │  ← gcos_apdu.c/h
├─────────────────────────────────────┤
│   T=0 Protocol Layer                │  ← gcos_t0_protocol.c/h
│   - State machine                   │
│   - Procedure byte validation       │
│   - APDU header/data transfer       │
├─────────────────────────────────────┤
│   TLP224 Message Layer              │  ← gcos_tlp.c/h
│   - Message framing                 │
│   - LRC error checking              │
│   - Command type handling           │
├─────────────────────────────────────┤
│   Transport Layer                   │  ← gcos_transport.c/h
│   - STDIO mode (raw hex)            │
│   - TCP mode (TLP224 frames)        │
└─────────────────────────────────────┘
```

---

## 📦 核心模块详解

### 1. TLP消息层 (`gcos_tlp.c/h`)

**职责**: TLP224消息的构建、解析和校验

#### 关键数据结构

```c
typedef struct {
    s8 ioFlag;                  /**< I/O flag: TLP_INPUT or TLP_OUTPUT */
    u16 len;                    /**< Total message length */
    s16 fd;                     /**< File descriptor (socket handle) */
    s8 ioState;                 /**< I/O state: CLOSED, OPEN, or ACTIVE */
    u16 ioOffset;               /**< Current I/O offset within message */
    u8 buf[TLP_BUFFER_SIZE];    /**< Message buffer */
} TLP_MSG;
```

#### TLP224消息格式

```
[ACK/NACK][Length High][Length Low][Command/Status][Payload...][LRC]
   Byte 0      Byte 1       Byte 2         Byte 3       Bytes 4..N   Byte N+1
```

#### 关键函数

| 函数 | 说明 |
|------|------|
| `tlp_msg_init()` | 初始化TLP消息结构 |
| `tlp_compute_lrc()` | 计算LRC（纵向冗余校验） |
| `tlp_validate_lrc()` | 验证TLP消息的LRC |

#### LRC计算算法

```c
u8 tlp_compute_lrc(const u8 *buf, u16 length) {
    u8 lrc = 0;
    for (u16 i = 0; i < length; i++) {
        lrc ^= buf[i];  // XOR all bytes
    }
    return lrc;
}
```

---

### 2. T=0协议层 (`gcos_t0_protocol.c/h`)

**职责**: 实现ISO7816-3定义的T=0字节级协议

#### T=0状态机

```
INIT → HEADER_RECEIVED → HEADER_READ → INCOMING/OUTGOING → (back to HEADER_READ)
```

| 状态 | 说明 |
|------|------|
| `T0_STATE_INIT` | 初始状态，未收到任何命令 |
| `T0_STATE_HEADER_RECEIVED` | 已收到APDU头但未传递给上层 |
| `T0_STATE_HEADER_READ` | APDU头已传递给应用层 |
| `T0_STATE_INCOMING` | 应用层调用receive_data（Case 3/4） |
| `T0_STATE_OUTGOING` | 应用层调用send_data（Case 2/4） |
| `T0_STATE_OUTGOING_BURST` | 突发模式发送中 |

#### 关键API

##### ATR发送

```c
s8 t0_send_atr(TLP_MSG *msg, u8 hist_len, const u8 *atr, const u8 *hist, bool need_recv);
```

**流程**:
1. 如果`need_recv=true`，等待POWER_UP命令
2. 构建ATR响应消息（包含历史字节）
3. 计算并附加LRC
4. 发送ATR消息
5. 更新状态为`TLP_STATE_OPEN`

##### 命令接收

```c
s8 t0_receive_command(TLP_MSG *msg, u8 *command);
```

**处理的命令类型**:
- `TLP_POWER_UP` (0x6E): 复位/上电命令
- `TLP_POWER_DOWN` (0x4D): 断电命令
- `TLP_ISO_INPUT` (0xDA): 带数据的APDU（Case 3/4）
- `TLP_ISO_OUTPUT` (0xDB): 不带数据的APDU（Case 1/2）

**验证规则**:
- INS不能是0x6X或0x9X（这些是SW1值）
- P3必须匹配实际数据长度
- 消息长度必须符合TLP224规范

##### 数据接收

```c
s16 t0_receive_data(TLP_MSG *msg, u8 *data, u16 offset, u16 length, u8 proc_byte);
```

**程序字节验证** (ISO7816-3 Section 8.2.2.1):

| ACK ^ INS | 含义 | 数据传输 |
|-----------|------|----------|
| 0x00 | VPP空闲 | 传输所有剩余字节 |
| 0x01 | VPP激活 | 传输所有剩余字节 |
| 0xFE | VPP激活 | 仅传输下一个字节 |
| 0xFF | VPP空闲 | 仅传输下一个字节 |

##### 数据发送

```c
s8 t0_send_data_proc(TLP_MSG *msg, const u8 *data, u16 offset, u16 length, 
                     u8 proc_byte, bool send_proc_byte);
```

**支持模式**:
- **突发模式** (`send_proc_byte=false`): 一次性发送所有数据
- **逐字节模式** (`send_proc_byte=true`): 每个字节前加程序字节

##### 状态字发送

```c
s8 t0_send_status_recv_command(TLP_MSG *msg, u8 *command, u16 sw1sw2);
```

**流程**:
1. 构建状态响应消息
2. 根据I/O模式和SW设置状态码
3. 附加SW1SW2和LRC
4. 发送状态消息
5. **立即接收下一个命令**（T=0协议特性）

---

### 3. 传输层 (`gcos_transport.c/h`)

**职责**: 提供底层通信通道

#### 支持的模式

##### STDIO模式
- **输入**: Hex字符串（如 `00A4040008A000000003000000`）
- **输出**: 打印响应数据和SW
- **用途**: 交互式命令行测试

##### TCP服务器模式
- **监听端口**: 默认9028
- **协议**: TLP224封装
- **用途**: 远程测试（类似cref）

#### 关键API

```c
GCOSResult gcos_transport_init(TransportMode mode, u16 port);
u16 gcos_transport_receive_apdu(u8 *buffer, u16 max_len);
void gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw);
void gcos_transport_cleanup(void);
```

---

## 🔄 数据处理流程

### 完整APDU处理流程（TCP模式）

```
Terminal                          GCOS VM
   |                                 |
   |--- POWER_UP (TLP224) --------->|
   |                                 | [t0_send_atr]
   |<-- ATR (TLP224) ---------------|
   |                                 |
   |--- SELECT APDU (TLP224) ------>|
   |     [ISO_INPUT frame]           | [t0_receive_command]
   |                                 | → Validate INS/P3
   |                                 | → Extract APDU header
   |                                 |
   |                                 | [VM processes APDU]
   |                                 | → gcos_vm_process_apdu()
   |                                 |
   |<-- Response + SW (TLP224) -----|
   |     [ACK + data + SW1SW2+LRC]  | [t0_send_status_recv_command]
   |                                 |
   |--- Next APDU (TLP224) -------->|
   |                                 |
```

### STDIO模式简化流程

```
User Input                        GCOS VM
   |                                 |
   |--- Hex string ---------------->|
   |     "00A40400..."               | [gcos_transport_receive_apdu]
   |                                 | → Parse hex to bytes
   |                                 |
   |                                 | [VM processes APDU]
   |                                 | → gcos_vm_process_apdu()
   |                                 |
   |<-- Response printed -----------|
   |     "Response Data: ..."        | [gcos_transport_send_response]
   |     "SW: 6A81"                  |
   |                                 |
```

---

## 📊 TLP224协议详解

### 消息类型

| 类型 | 值 | 说明 |
|------|-----|------|
| `TLP_ACK` | 0x60 | 确认 |
| `TLP_NACK` | 0xE0 | 否认 |
| `TLP_POWER_UP` | 0x6E | 上电命令 |
| `TLP_POWER_DOWN` | 0x4D | 断电命令 |
| `TLP_ISO_INPUT` | 0xDA | ISO输入（带数据） |
| `TLP_ISO_OUTPUT` | 0xDB | ISO输出（无数据） |

### 状态码

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `STATUS_SUCCESS` | 0x00 | 成功 |
| `STATUS_INCORRECT_NUMBER_OF_ARGS` | 0x03 | 参数数量错误 |
| `STATUS_COMMAND_UNKNOWN` | 0x04 | 未知命令 |
| `STATUS_PROTOCOL_ERROR` | 0x09 | 协议错误 |
| `STATUS_ISO_CMD_ERROR` | 0x11 | ISO命令错误 |
| `STATUS_ISO_LC_ERROR` | 0x1A | LC长度错误 |
| `STATUS_INTERRUPTED_EXCHANGE` | 0xE5 | 交换中断 |
| `STATUS_CARD_ERROR` | 0xE7 | 卡错误 |

### 消息偏移量

```
Byte Offset  Field
─────────────────────────
0            ACK/NACK
1            Length High Byte
2            Length Low Byte
3            Command Type / Status Code
4            CLA
5            INS
6            P1
7            P2
8            P3
9+           Data (if any)
N            LRC
```

---

## 🔧 编译和测试

### 编译

```bash
cd gcos_vm
cmake -B build -S .
cmake --build build --config Debug
```

### 测试STDIO模式

```bash
cd build
echo "00A4040008A000000003000000" | ./Debug/gcos_demo.exe -s
```

**预期输出**:
```
[T=0] Received APDU (13 bytes): 00A4040008A000000003000000
[GCOS] VM returned SW=6A81, Response length=260
Response Data (260 bytes): ...
SW: 6A81
```

### 测试TCP模式

```bash
# Terminal 1: Start server
./Debug/gcos_demo.exe -t 9028

# Terminal 2: Connect with netcat
nc localhost 9028

# Send TLP224 encoded APDU (requires custom client)
```

---

## 📝 与cref的对比

### 相同点

1. **T=0状态机**: 完全遵循cref的状态转换逻辑
2. **程序字节验证**: 使用相同的XOR验证算法
3. **TLP224格式**: 消息结构与cref一致
4. **LRC计算**: 相同的纵向冗余校验算法

### 改进点

1. **代码规范化**: 
   - ✅ 所有中文注释转为英文
   - ✅ 修复乱码问题
   - ✅ 统一的命名规范

2. **模块化设计**:
   - ✅ 清晰的三层架构（TLP → T=0 → Transport）
   - ✅ 独立的头文件和实现文件
   - ✅ 易于扩展和维护

3. **双模式支持**:
   - ✅ STDIO模式用于快速测试
   - ✅ TCP模式用于远程调试

4. **零动态内存**:
   - ✅ 所有缓冲区静态分配
   - ✅ 符合COS3规范要求

---

## 🚀 下一步计划

### 短期目标

1. **完善TCP模式**:
   - [ ] 实现完整的TLP224客户端
   - [ ] 支持多客户端连接
   - [ ] 添加超时处理

2. **增强T=0协议**:
   - [ ] 支持6Cxx状态字处理
   - [ ] 支持GET RESPONSE序列
   - [ ] 支持链式APDU

3. **测试覆盖**:
   - [ ] 单元测试TLP消息构建
   - [ ] 单元测试T=0状态机
   - [ ] 集成测试完整流程

### 长期目标

1. **硬件接口**:
   - [ ] 添加串口支持（真实智能卡读卡器）
   - [ ] 添加USB CCID支持
   - [ ] 支持ISO7816物理层

2. **性能优化**:
   - [ ] 减少内存拷贝
   - [ ] 优化LRC计算
   - [ ] 支持DMA传输

3. **安全性增强**:
   - [ ] 添加消息加密
   - [ ] 防止重放攻击
   - [ ] 安全通道协议（SCP）

---

## 📚 参考资料

1. **ISO/IEC 7816-3**: Identification cards - Integrated circuit cards - Part 3: Cards with contacts - Electrical interface and transmission protocols
2. **cref Source Code**: JavaCard Reference Implementation
   - `cref/adapter/win32/t0.c`
   - `cref/adapter/win32/t0_ll.c`
   - `cref/adapter/win32/tlp.h`
   - `cref/adapter/win32/msg.c`
   - `cref/adapter/win32/jcshell.c`
3. **GB/T 44901.3**: 卡及身份识别安全设备片上操作系统第3部分

---

## ✅ 完成清单

- [x] TLP消息结构定义 (`gcos_tlp.h`)
- [x] TLP消息处理实现 (`gcos_tlp.c`)
- [x] T=0协议头文件 (`gcos_t0_protocol.h`)
- [x] T=0协议完整实现 (`gcos_t0_protocol.c`)
- [x] 传输层支持STDIO和TCP模式 (`gcos_transport.c/h`)
- [x] 主处理循环集成 (`gcos_main.c`)
- [x] 所有中文注释转为英文
- [x] 修复乱码问题
- [x] 编译成功（0个错误）
- [x] STDIO模式测试通过
- [x] 创建完整文档

---

**文档版本**: 1.0  
**最后更新**: 2026-05-09  
**作者**: GCOS VM Team
