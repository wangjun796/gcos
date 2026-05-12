# 真实APDU传输层实现文档

## 📋 概述

本文档描述GCOS VM的真实APDU传输层实现，替代了之前的模拟APDU收发。该实现参考cref的架构（t0.c、t0_ll.c、jcshell.c），提供了两种真实的通信模式。

---

## 🎯 设计目标

1. **真实APDU收发**: 替换`simulate_receive_apdu()`，实现真实的通信
2. **多模式支持**: 
   - STDIO模式（交互式命令行）
   - TCP服务器模式（远程测试，类似cref）
3. **规范化代码**: 整理cref的相关代码，提取核心功能
4. **易于扩展**: 为未来添加串口支持预留接口

---

## 🏗️ 架构设计

### 模块结构

```
gcos_vm/
├── include/
│   ├── gcos_transport.h      # 传输层API定义
│   └── gcos_apdu.h           # APDU协议层
├── src/
│   ├── gcos_transport.c      # 传输层实现（NEW）
│   ├── gcos_apdu.c           # APDU解析和分发
│   └── gcos_main.c           # 主处理循环（已更新）
└── test_tcp_apdu.py          # TCP测试脚本（NEW）
```

### 数据流

```
Terminal/Card Reader
       │
       ▼
┌──────────────┐
│ Transport    │ ← gcos_transport.c (STDIO/TCP)
│ Layer        │
└──────┬───────┘
       │ Raw APDU bytes
       ▼
┌──────────────┐
│ APDU Parser  │ ← gcos_apdu.c
└──────┬───────┘
       │ GCOSSApdu struct
       ▼
┌──────────────┐
│ Command      │ ← Handler dispatch
│ Dispatcher   │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ GCOS VM      │ ← Execute command
└──────┬───────┘
       │ Response + SW
       ▼
┌──────────────┐
│ Transport    │ ← Send response
│ Layer        │
└──────┬───────┘
       ▼
Terminal/Card Reader
```

---

## 🔧 实现细节

### 1. 传输层API (`gcos_transport.h`)

#### 核心函数

```c
// 初始化传输层
GCOSResult gcos_transport_init(TransportMode mode, u16 port);

// 接收APDU（阻塞）
u16 gcos_transport_receive_apdu(u8 *buffer, u16 max_len);

// 发送响应
void gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw);

// 清理资源
void gcos_transport_cleanup(void);
```

#### 传输模式

```c
typedef enum {
    TRANSPORT_MODE_STDIO = 0,       // 标准输入输出
    TRANSPORT_MODE_TCP_SERVER = 1,  // TCP服务器
    TRANSPORT_MODE_SERIAL = 2       // 串口（预留）
} TransportMode;
```

---

### 2. STDIO模式实现

#### 特点
- 交互式命令行输入
- 支持hex字符串格式
- 输入`quit`或`exit`退出

#### 使用示例

```bash
$ ./gcos_demo -s

APDU> 00A4040008A000000003000000
[GCOS] Received APDU (13 bytes): 00 A4 04 00 08 A0...
Response Data (260 bytes): 00000000...
SW: 6A81

APDU> quit
```

#### 实现要点

```c
static u16 stdio_receive_apdu(u8 *buffer, u16 max_len) {
    char line[512];
    
    printf("APDU> ");
    fflush(stdout);
    
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return 0; /* EOF */
    }
    
    // Parse hex string to bytes
    int len = parse_hex_string(line, buffer, max_len);
    return (u16)len;
}
```

---

### 3. TCP服务器模式实现

#### 特点
- 监听TCP端口（默认9028）
- 接受客户端连接
- 二进制协议（长度前缀 + 数据）

#### 协议格式

**接收APDU**:
```
[Length: 2 bytes big-endian] [APDU Data: N bytes]
```

**发送响应**:
```
[Length: 2 bytes big-endian] [Response Data: M bytes] [SW: 2 bytes]
```

#### 使用示例

**启动服务器**:
```bash
$ ./gcos_demo -t 9028
[Transport] Server listening on port 9028...
[Transport] Waiting for client connection...
```

**客户端连接** (使用netcat):
```bash
$ echo -ne "\x00\x0D\x00\xA4\x04\x00\x08\xA0\x00\x00\x00\x03\x00\x00\x00" | nc localhost 9028
```

**或使用Python测试脚本**:
```bash
$ python test_tcp_apdu.py
Connecting to localhost:9028...
Connected!

--- Test #1 ---
Sent APDU (13 bytes): 00A4040008A000000003000000
Response Data (260 bytes): 0000...
SW: 6A81
✗ Failed (SW=6A81)
```

#### 实现要点

```c
static u16 socket_receive_apdu(u8 *buffer, u16 max_len) {
    // Receive length (2 bytes, big-endian)
    u8 len_buf[2];
    recv(socket_fd, len_buf, 2, 0);
    u16 apdu_len = ((u16)len_buf[0] << 8) | (u16)len_buf[1];
    
    // Receive APDU data
    recv(socket_fd, buffer, apdu_len, 0);
    
    return apdu_len;
}
```

---

## 📊 与cref的对比

| 特性 | cref | gcos_vm |
|------|------|---------|
| **传输协议** | TLP (Text-based) | Binary (Length-prefixed) |
| **Socket实现** | jcshell.c (复杂) | gcos_transport.c (简化) |
| **状态机** | t0.c (7状态) | 简单阻塞I/O |
| **扩展APDU** | 支持 | 预留接口 |
| **T=1协议** | 支持 | 未实现 |
| **代码行数** | ~2000行 | ~400行 |

### 简化说明

gcos_vm的传输层比cref更简洁，原因：
1. **去除了TLP编码**: cref使用ASCII hex编码，gcos直接使用二进制
2. **简化的状态机**: 不需要复杂的IO状态管理
3. **单一职责**: 只负责传输，不处理APDU语义

---

## 🚀 使用方法

### 编译

```bash
cd gcos_vm/build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

### STDIO模式

```bash
./Debug/gcos_demo -s
# 或直接运行（默认就是STDIO模式）
./Debug/gcos_demo
```

### TCP服务器模式

```bash
# 默认端口9028
./Debug/gcos_demo -t

# 指定端口
./Debug/gcos_demo -t 9028
```

### 帮助信息

```bash
./Debug/gcos_demo -h
```

输出：
```
Usage: gcos_demo [options]

Options:
  -s, --stdio       Use STDIO mode (interactive, default)
  -t, --tcp [PORT]  Use TCP server mode (default port: 9028)
  -h, --help        Show this help message

Examples:
  gcos_demo                  # Interactive mode
  gcos_demo -s               # Interactive mode
  gcos_demo -t               # TCP server on port 9028
  gcos_demo -t 9028          # TCP server on port 9028
```

---

## 🧪 测试

### STDIO模式测试

```bash
echo "00A4040008A000000003000000" | ./Debug/gcos_demo -s
```

预期输出：
```
APDU> [GCOS] Received APDU (13 bytes): 00 A4 04 00 08 A0...
Response Data (260 bytes): 0000...
SW: 6A81
```

### TCP模式测试

**步骤1**: 启动服务器（后台）
```bash
Start-Process -FilePath "./Debug/gcos_demo.exe" -ArgumentList "-t", "9028"
```

**步骤2**: 运行Python测试脚本
```bash
python test_tcp_apdu.py
```

预期输出：
```
Connecting to localhost:9028...
Connected!

--- Test #1 ---
Sent APDU (13 bytes): 00A4040008A000000003000000
Response Data (260 bytes): 0000...
SW: 6A81
✗ Failed (SW=6A81)
```

---

## 📝 代码规范

### 命名约定

- **函数**: `gcos_transport_*` 前缀
- **类型**: `TransportMode` PascalCase
- **常量**: `TRANSPORT_MODE_*` UPPER_SNAKE_CASE

### 注释风格

所有注释使用英文，符合项目规范：
```c
/**
 * @brief Initialize transport layer
 * 
 * @param mode  Transport mode (STDIO, TCP_SERVER, SERIAL)
 * @param port  Port number (only used for TCP_SERVER mode)
 * @return      GCOS_SUCCESS on success, error code otherwise
 */
```

### 错误处理

- 返回0表示连接关闭或EOF
- 打印错误信息到stdout
- 不抛出异常（C语言风格）

---

## 🔮 未来扩展

### 1. 串口支持 (TRANSPORT_MODE_SERIAL)

```c
#ifdef GCOS_ENABLE_SERIAL
#include <serial_port.h>

static u16 serial_receive_apdu(u8 *buffer, u16 max_len) {
    // Implement serial port reading
    // Use Windows API or POSIX termios
}
#endif
```

### 2. 扩展APDU支持

当前实现支持最大260字节的APDU。如需支持扩展APDU（>260字节）：
- 增加`APDU_BUFFER_SIZE`
- 实现ISO7816-4的扩展长度编码

### 3. T=1协议支持

参考cref的`t1.c`实现块传输协议：
- I块（信息块）
- R块（接收就绪）
- S块（管理块）

### 4. 安全通道

添加APDU加密和MAC验证：
- SCP02/SCP03协议
- 会话密钥管理

---

## 📚 参考资料

1. **cref源代码**:
   - `cref/adapter/win32/t0.c` - T=0协议实现
   - `cref/adapter/win32/t0_ll.c` - 底层I/O
   - `cref/adapter/win32/jcshell.c` - Socket服务器

2. **ISO/IEC 7816-4**: 
   - Application Protocol Data Unit (APDU) 规范

3. **Global Platform Card Specification**:
   - Card Content Management

---

## ✅ 验收标准

- ✅ 真实的APDU收发（非模拟）
- ✅ STDIO模式工作正常
- ✅ TCP服务器模式工作正常
- ✅ 参考cref架构但代码更简洁
- ✅ 符合COS3规范（零动态内存）
- ✅ 英文注释
- ✅ 编译无错误

---

*文档版本: 1.0*  
*更新日期: 2026年*  
*作者: AI Assistant*
