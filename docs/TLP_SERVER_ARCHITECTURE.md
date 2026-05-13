# GCOS TLP Server Implementation (JCRE Mode)

## 架构概述

### Cref的三层通信架构

```
┌─────────────────┐     TLP224      ┌──────────────┐    TLP Protocol    ┌─────────────┐
│  Card Terminal   │ ◄────────────► │   JCShell    │ ◄────────────────► │    JCRE     │
│  (GlobalPlatform)│   Port 9000/   │  (Adapter)   │    Port 9025       │   (VM)      │
│                  │     9900       │              │                    │             │
└─────────────────┘                └──────────────┘                    └─────────────┘
     卡外工具层                        中间适配层                         虚拟机层
```

### GCOS对应实现

```
┌─────────────────┐     TLP224      ┌──────────────┐    TLP Protocol    ┌─────────────┐
│  Card Terminal   │ ◄────────────► │   JCShell    │ ◄────────────────► │    GCOS     │
│  (GlobalPlatform)│   Port 9000/   │  (gcos_      │    Port 9025       │   (VM)      │
│                  │     9900       │   jcshell.c) │                    │             │
└─────────────────┘                └──────────────┘                    └─────────────┘
                                         ↓                                    ↑
                                   已实现(监听                              新实现
                                   9000/9900)                          (gcos_tlp_server.c)
                                                                           监听9025
```

## 关键文件

### 1. gcos_tlp_server.c (NEW)
- **功能**: 实现JCRE服务器，监听端口9025
- **参考**: cref/adapter/win32/server.c, io_cad.c
- **协议**: 
  - 接收ConnectInfo握手（8字节：magic + connect_type）
  - 接收4字节消息头：[type][cmd][size_hi][size_lo]
  - 接收APDU数据
  - 发送响应：[type][cmd][size_hi][size_lo][data...][SW]

### 2. gcos_jcshell.c (EXISTING)
- **功能**: 实现JCShell服务器，监听端口9000/9900
- **参考**: cref/adapter/win32/jcshell.c
- **协议**: TLP224（ASCII hex编码）
- **职责**: 
  - 接收卡外工具的TLP224消息
  - 解析为APDU
  - 连接到GCOS TLP Server（9025）
  - 转发APDU并返回响应

### 3. gcos_main.c (UPDATED)
- **新增模式**: `TRANSPORT_MODE_TLP_SERVER`
- **命令行**: `-T` 或 `--tlp`
- **默认模式**: TLP Server（JCRE mode）

## 通信流程

### 完整APDU交互流程

1. **卡外工具** → **JCShell** (Port 9000/9900)
   - 发送TLP224格式消息（ASCII hex）
   - 例如: `06210000...EOT` (POWER_UP命令)

2. **JCShell** 解析TLP224
   - 解码ASCII hex为二进制
   - 提取APDU命令

3. **JCShell** → **GCOS TLP Server** (Port 9025)
   - 建立TCP连接
   - 发送ConnectInfo握手：`[0x34 0x12 0x5A 0x5A 0x00 0x00 0x00 0x00]`
   - 发送4字节头：`[type][cmd][size_hi][size_lo]`
   - 发送APDU数据

4. **GCOS TLP Server** 处理
   - 验证握手magic number
   - 接收APDU
   - 调用`gcos_vm_process_apdu()`
   - 构建响应

5. **GCOS TLP Server** → **JCShell**
   - 发送4字节响应头
   - 发送响应数据 + SW

6. **JCShell** → **卡外工具**
   - 将响应编码为TLP224格式
   - 发送回卡外工具

## 测试方法

### 方法1: 直接测试TLP Server（当前实现）

```bash
# 启动GCOS TLP Server
./build/Debug/gcos_demo.exe -T

# 运行测试脚本
python test_tlp_server.py
```

测试脚本模拟JCShell行为：
1. 连接到9025端口
2. 发送ConnectInfo握手
3. 发送POWER_UP命令
4. 发送SELECT APDU
5. 接收响应

### 方法2: 完整链路测试（需要实现JCShell到TLP Server的连接）

```bash
# 终端1: 启动GCOS TLP Server
./build/Debug/gcos_demo.exe -T

# 终端2: 启动JCShell（如果实现）
# ./gcos_jcshell_client localhost 9000

# 终端3: 使用GlobalPlatform Pro
gp -list
```

## 当前状态

### ✅ 已完成
- [x] gcos_tlp_server.c实现
- [x] ConnectInfo握手协议
- [x] 4字节消息头解析
- [x] POWER_UP命令处理（返回ATR）
- [x] APDU转发到VM
- [x] 响应构建和发送
- [x] 多线程支持（Windows/Linux）
- [x] CMakeLists.txt更新
- [x] gcos_main.c集成

### ⚠️ 待调试
- [ ] 线程执行问题（POWER_UP响应超时）
- [ ] VM APDU handler实现（SELECT命令返回SW_INS_NOT_SUPPORTED）
- [ ] 完整的错误处理

### 🔧 下一步
1. 调试线程执行问题
2. 实现缺失的APDU handlers（SELECT, INSTALL等）
3. 测试与真实JCShell的集成
4. 添加日志和调试输出

## 关键代码位置

### ConnectInfo握手
```c
// gcos_tlp_server.c:57-78
typedef struct {
    u32 magic;          // 0x5a5a1234
    u32 connect_type;   // 0=contacted, 2=contactless
} ConnectInfo;
```

### 消息格式
```
Request:  [type][cmd][size_hi][size_lo][data...]
Response: [type][cmd][size_hi][size_lo][data...][SW_hi][SW_lo]
```

### POWER_UP命令
```c
if (type == 0 && cmd == 0x21) {
    // Return ATR
    u8 atr[] = { 0x3B, 0xF4, 0x11, 0x00, 0xFF, 0x00 };
}
```

## 参考文档

- cref/adapter/win32/server.c - RecvProtocol(), getConnection()
- cref/adapter/win32/io_cad.c - cref_receiveTLP224Message()
- cref/adapter/win32/jcshell.c - processConnect(), ConnectToJCRE()
- cref/adapter/win32/t0_ll.c - send_ATR(), getConnection()

## 端口分配

| 组件 | 端口 | 协议 | 方向 |
|------|------|------|------|
| JCShell | 9000 | TLP224 | 卡外工具 → JCShell |
| JCShell | 9900 | TLP224 | 卡外工具 → JCShell (contactless) |
| GCOS TLP Server | 9025 | Binary TLP | JCShell → GCOS |

## 注意事项

1. **不要混淆端口**: 
   - 9000/9900是JCShell监听的（卡外工具连接）
   - 9025是GCOS TLP Server监听的（JCShell连接）

2. **握手是必须的**: 
   - JCShell连接后必须先发送ConnectInfo
   - Magic number必须为0x5a5a1234

3. **消息格式**: 
   - 使用4字节头，不是2字节
   - 长度字段是大端序

4. **线程安全**: 
   - 每个客户端连接在独立线程中处理
   - Windows使用CreateThread，Linux使用pthread
