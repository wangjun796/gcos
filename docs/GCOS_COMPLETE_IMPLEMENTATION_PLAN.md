# GCOS VM 完整实现计划

**基于 cref 参考实现和 COS3 规范**

**日期**: 2026-05-11  
**版本**: 1.0  
**状态**: 规划阶段

---

## 📋 执行摘要

本文档分析了 cref（完整的 JavaCard 虚拟机参考实现）和 GB/T 44901.3（COS3 规范），制定了完善 GCOS VM 的完整实施计划。

### 核心发现

1. **cref 是一个成熟的 JavaCard VM 实现**，包含：
   - 完整的字节码解释器
   - 应用安装和管理系统
   - T=0/T=1/T=CL 协议栈
   - JCShell 通信服务器
   - 事务管理机制
   - 安全域隔离

2. **COS3 规范要求支持面向过程应用后下载**，关键特性：
   - C 语言子集作为应用编程语言
   - WebAssembly 中间文件格式（.wasm）
   - SEF 可加载文件格式
   - 栈式虚拟机架构
   - 零动态内存分配（嵌入式环境）

3. **GCOS 当前状态**：
   - ✅ VM 核心框架已搭建
   - ✅ 传输层和通信协议完成
   - ⚠️ APDU handlers 使用 echo 占位符
   - ❌ SELECT/LOAD/INSTALL 等核心命令未实现
   - ❌ SEF 文件解析和链接未完成
   - ❌ 完整的字节码执行器待实现

---

## 🎯 实施目标

### 最终目标
实现一个完全符合 COS3 规范的智能卡虚拟机，支持：
- 应用后下载（SEF 文件加载和安装）
- 多通道应用隔离
- 事务管理机制
- 运行时安全管理
- 与 cref 兼容的通信协议

### 阶段性目标

#### Phase 1: 核心 APDU 命令实现（当前优先级）
- [ ] SELECT 命令 - 应用选择
- [ ] LOAD 命令 - 流式加载
- [ ] INSTALL 命令 - 应用安装

#### Phase 2: 文件加载和链接
- [ ] SEF 文件解析
- [ ] LINK 文件处理
- [ ] 模块链接和重定位

#### Phase 3: 字节码执行器
- [ ] 指令解码和执行
- [ ] 栈帧管理
- [ ] 控制流指令

#### Phase 4: 高级功能
- [ ] 事务管理完善
- [ ] 安全管理完善
- [ ] 性能优化

---

## 📊 cref 架构分析

### 1. 整体架构

```
┌─────────────────────────────────────────────┐
│         Card Terminal (JCShell)              │
└──────────────┬──────────────────────────────┘
               │ Binary Protocol (Port 9000/9900)
┌──────────────▼──────────────────────────────┐
│         Server Layer (server.c)              │
│  - Socket management                         │
│  - Connection handling                       │
│  - Multi-threading support                   │
└──────────────┬──────────────────────────────┘
               │ TLP224 Protocol
┌──────────────▼──────────────────────────────┐
│      Protocol Layer (t0.c, t1.c, t5.c)      │
│  - T=0 byte-oriented protocol                │
│  - T=1 block-oriented protocol               │
│  - T=CL contactless protocol                 │
└──────────────┬──────────────────────────────┘
               │ APDU Commands
┌──────────────▼──────────────────────────────┐
│       APDU Processing (jcshell.c)           │
│  - Command dispatch                          │
│  - Response formatting                       │
└──────────────┬──────────────────────────────┘
               │ Native Calls
┌──────────────▼──────────────────────────────┐
│      VM Core (interpreter.c)                │
│  - Bytecode interpreter                      │
│  - Stack frame management                    │
│  - Exception handling                        │
└──────────────┬──────────────────────────────┘
               │
┌──────────────▼──────────────────────────────┐
│     Runtime Services                        │
│  - Memory management (memory.c)             │
│  - Object management (object.c)             │
│  - Method dispatch (method.c)               │
│  - Transaction (TransactionMgr.c)           │
│  - Security (firewall.c)                    │
└─────────────────────────────────────────────┘
```

### 2. 关键模块分析

#### 2.1 Server 层 (server.c)
**位置**: `cref/adapter/win32/server.c`

**功能**:
- TCP socket 服务器实现
- 监听端口 9000（接触式）和 9900（非接触式）
- 多线程客户端连接处理
- TLP 消息封装和解封装

**关键函数**:
```c
boolean createSockets();          // 创建监听 socket
void processClientConnection();   // 处理客户端连接
int readTLPMessage();             // 读取 TLP 消息
void sendTLPResponse();           // 发送 TLP 响应
```

**对 GCOS 的启示**:
- ✅ GCOS 已有 gcos_jcshell.c 实现类似功能
- ⚠️ 需要检查多线程实现的完整性
- ⚠️ 需要验证 TLP 协议的完全兼容性

---

#### 2.2 协议层 (t0.c, t1.c, t5.c)
**位置**: `cref/adapter/win32/t0.c`, `t1.c`, `t5.c`

**T=0 协议特点**:
- 面向字节的传输协议
- 5 字节 APDU 头：CLA INS P1 P2 P3
- 过程字节交换（Procedure Bytes）
- 支持链式传输

**关键实现**:
```c
// T=0 接收 APDU
u16 t0_receive_apdu(u8 *apdu_buf, u16 max_len);

// T=0 发送响应
void t0_send_response(const u8 *data, u16 len, u16 sw);
```

**对 GCOS 的启示**:
- ✅ GCOS 已有 gcos_t0_protocol.c
- ⚠️ 需要对照 cref 验证协议细节
- ⚠️ 特别关注 ATR 响应格式

---

#### 2.3 VM 解释器 (interpreter.c)
**位置**: `cref/common/interpreter.c`

**核心循环**:
```c
void executCode() {
    bytecode_flowCtl_init(0x5AA5);
    
    while (1) {
        checkBreakPoint(PC);
        
        u8 bc = fetchByte();  // 取指令
        
        switch (bc) {
            case OP_ADD:    exec_add(); break;
            case OP_SUB:    exec_sub(); break;
            case OP_CALL:   exec_call(); break;
            // ... 更多指令
        }
    }
}
```

**关键组件**:
1. **程序计数器 (PC)**: 跟踪当前指令地址
2. **操作数栈**: 存储指令操作数和中间结果
3. **栈帧管理**: 函数调用时的局部变量和参数
4. **异常处理**: try-catch-finally 机制

**对 GCOS 的启示**:
- ⚠️ GCOS 的 gcos_executor.c 需要完善指令执行逻辑
- ⚠️ 需要实现完整的指令集（200+ 条指令）
- ⚠️ 需要实现栈帧管理和异常处理

---

#### 2.4 内存管理 (memory.c)
**位置**: `cref/common/memory.c`

**内存区域**:
1. **RAM**: 易失性存储（栈、全局数据）
2. **EEPROM/Flash**: 非易失性存储（堆、持久化数据）
3. **ROM**: 只读存储（代码、常量）

**关键特性**:
- 静态内存分配（无 malloc/free）
- 内存池管理
- 引用计数垃圾回收

**对 GCOS 的启示**:
- ✅ GCOS 采用零动态内存分配设计，符合要求
- ⚠️ 需要完善 gcos_memory.c 的内存池管理
- ⚠️ 需要实现引用计数或标记清除 GC

---

#### 2.5 事务管理 (TransactionMgr.c)
**位置**: `cref/Transaction/TransactionMgr.c`

**事务 API**:
```c
void JCSystem_beginTrans();    // 开始事务
void JCSystem_commitTrans();   // 提交事务
void JCSystem_abortTrans();    // 中止事务
```

**实现机制**:
- 事务开始前备份数据
- 提交时写入永久存储
- 中止时恢复备份数据
- 支持嵌套事务（最多 4 层）

**对 GCOS 的启示**:
- ✅ GCOS 已有 gcos_transaction.c 框架
- ⚠️ 需要完善备份/恢复机制
- ⚠️ 需要实现原子性保证

---

#### 2.6 安全管理 (firewall.c)
**位置**: `cref/common/firewall.c`

**安全机制**:
1. **应用隔离**: 每个应用有独立的运行域
2. **接口授权**: 基于授权表的访问控制
3. **类型检查**: 运行时类型安全
4. **数组边界检查**: 防止缓冲区溢出

**对 GCOS 的启示**:
- ✅ GCOS 已有 gcos_security.c 框架
- ⚠️ 需要实现完整的防火墙机制
- ⚠️ 需要实现接口授权表

---

## 📖 COS3 规范关键要求

### 1. 应用编程语言

**C 语言子集**（符合 GB/T 15272）:

**支持的关键字**:
- 数据类型: `char`, `int`, `short`, `signed`, `unsigned`, `void`, `struct`, `union`, `enum`
- 存储类: `extern`, `static`, `const`
- 控制流: `if`, `else`, `switch`, `case`, `default`, `while`, `do`, `for`, `break`, `continue`, `return`, `goto`
- 其他: `sizeof`, `typedef`

**不支持的特性**:
- ❌ 指针算术
- ❌ 动态内存分配（malloc/free）
- ❌ 递归函数
- ❌ 浮点运算
- ❌ 变长数组

---

### 2. 二进制文件格式

#### 2.1 中间文件 (.wasm)
- 符合 W3C WebAssembly Core Specification
- 文件类型标识符: `'0061736D'` ("asm")
- 跨平台中间表示

#### 2.2 链接文件 (.link)
- 存放模块导出链接信息
- 文件类型标识符: `'6C696E6B'` ("link")
- 包含：模块名称、模块 ID、导出函数索引

#### 2.3 可加载文件 (.sef)
- 由应用转换器生成
- 文件类型标识符: `'00736566'` ("sef")
- 包含：代码、数据、链接信息
- **小端顺序**存储多字节数据

**文件结构**:
```
┌──────────────┐
│  File Header  │  - File Type (u32)
│              │  - File Version (u32)
├──────────────┤
│  File Body   │  - Code Section
│              │  - Data Section
│              │  - Link Section
└──────────────┘
```

---

### 3. 运行时数据区

| 区域 | 类型 | 大小 | 用途 |
|------|------|------|------|
| 执行器栈 | 易失性 | 4 字节/单元 | 栈帧、参数、局部变量、中间结果 |
| 间接访问变量栈 | 易失性 | 16 字节/单元 | 组合数据类型元素 |
| 全局数据区 | 易失性 | 可变 | 模块全局数据、临时数据 |
| 堆 | 非易失性 | 可变 | 持久化数据、跨域数据 |
| 程序计数器 | 易失性 | 4 字节 | 当前指令地址 |
| 模块程序区 | 非易失性 | 可变 | 可执行代码 |

---

### 4. 指令集

**附录 A 定义了 200+ 条指令**，包括：

#### 4.1 算术运算
- `ADD`, `SUB`, `MUL`, `DIV`, `MOD`
- `NEG`, `INC`, `DEC`

#### 4.2 逻辑运算
- `AND`, `OR`, `XOR`, `NOT`
- `SHL`, `SHR`, `SAR`

#### 4.3 控制流
- `BR` (无条件跳转)
- `BEQZ`, `BNEZ` (条件跳转)
- `CALL`, `RET` (函数调用)
- `SWITCH` (多分支)

#### 4.4 数据访问
- `LDT`, `STT` (加载/存储临时变量)
- `LDM`, `STM` (加载/存储模块数据)
- `LDG`, `STG` (加载/存储全局数据)

#### 4.5 类型转换
- `CVT_I2S`, `CVT_S2I` (int ↔ short)
- `CVT_I2U`, `CVT_U2I` (signed ↔ unsigned)

---

### 5. APDU 命令

#### 5.1 SELECT (INS=0xA4)
**功能**: 选择应用实例

**参数**:
- P1: 选择模式（0x00=按 AID，0x02=按次级 AID）
- P2: 选择选项
- Data: AID（5-16 字节）

**响应**:
- SW=0x9000: 成功
- Data: FCP（文件控制参数）

#### 5.2 LOAD (INS=0xE4)
**功能**: 流式加载可执行模块

**参数**:
- P1: 加载阶段（0x00=初始化，0x01=数据块，0x02=完成）
- P2: 保留
- Data: 加载数据

**状态机**:
```
INIT → RECEIVE_BLOCKS → FINALIZE
```

#### 5.3 INSTALL (INS=0xE6)
**功能**: 安装应用实例

**参数**:
- P1: 安装模式
- P2: 保留
- Data: 安装参数（AID、实例 AID、权限等）

**响应**:
- SW=0x9000: 成功
- SW=0x6A80: 参数错误

#### 5.4 DELETE (INS=0xE2)
**功能**: 删除应用或模块

#### 5.5 GET STATUS (INS=0xF2)
**功能**: 查询应用/模块状态

#### 5.6 MANAGE CHANNEL (INS=0x70)
**功能**: 管理逻辑通道

---

## 🔍 GCOS 当前实现分析

### 已完成部分 ✅

#### 1. VM 核心框架
- ✅ `gcos_vm.c` - VM 初始化和配置
- ✅ `gcos_executor.c` - 执行引擎框架
- ✅ `gcos_memory.c` - 内存管理框架（零动态分配）
- ✅ `gcos_instructions.c` - 指令集定义

#### 2. 通信协议
- ✅ `gcos_transport.c` - 统一传输层
- ✅ `gcos_hal_win32.c` - HAL 实现
- ✅ `gcos_tlp.c` - TLP224 协议
- ✅ `gcos_t0_protocol.c` - T=0 协议
- ✅ `gcos_jcshell.c` - JCShell Server

#### 3. 应用管理框架
- ✅ `gcos_app_manager.c` - 应用生命周期管理框架
- ✅ `gcos_loader.c` - SEF 加载器框架
- ✅ `gcos_security.c` - 安全管理框架
- ✅ `gcos_transaction.c` - 事务管理框架

#### 4. APDU 处理
- ✅ `gcos_apdu.c` - APDU 处理框架
- ✅ Echo handler 实现（占位符）

---

### 待完善部分 ⚠️

#### 1. APDU 命令实现（高优先级）

**SELECT 命令**:
```c
// 当前状态: 使用 echo handler
// 需要实现:
static u16 apdu_handler_select(GCOSVM *vm, const GCOSSApdu *apdu, 
                               u8 *response, u16 *resp_len) {
    // 1. 解析 AID
    // 2. 查找匹配的应用实例
    // 3. 更新选中应用状态
    // 4. 返回 FCP
}
```

**LOAD 命令**:
```c
// 需要实现流式加载状态机
typedef enum {
    LOAD_STATE_IDLE,
    LOAD_STATE_INIT,
    LOAD_STATE_RECEIVING,
    LOAD_STATE_FINALIZING
} LoadState;

static LoadState g_load_state = LOAD_STATE_IDLE;
static u8 g_load_buffer[SEF_MAX_SIZE];
static u32 g_load_offset = 0;
```

**INSTALL 命令**:
```c
// 需要实现应用安装逻辑
static u16 apdu_handler_install(GCOSVM *vm, const GCOSSApdu *apdu,
                                u8 *response, u16 *resp_len) {
    // 1. 解析安装参数
    // 2. 创建应用实例
    // 3. 初始化应用数据
    // 4. 注册应用到应用表
}
```

---

#### 2. SEF 文件解析（中优先级）

**需要实现**:
```c
// gcos_loader.c
GCOSResult sef_parse_header(const u8 *data, u32 size, SEFHeader *header);
GCOSResult sef_parse_code_section(const u8 *data, u32 size, CodeSection *code);
GCOSResult sef_parse_data_section(const u8 *data, u32 size, DataSection *data_sec);
GCOSResult sef_parse_link_section(const u8 *data, u32 size, LinkSection *links);
```

**SEF 文件结构**:
```c
typedef struct {
    u32 file_type;      // '00736566'
    u32 file_version;   // 版本号
    u32 code_offset;    // 代码段偏移
    u32 code_size;      // 代码段大小
    u32 data_offset;    // 数据段偏移
    u32 data_size;      // 数据段大小
    u32 link_offset;    // 链接段偏移
    u32 link_size;      // 链接段大小
} SEFHeader;
```

---

#### 3. 字节码执行器（中优先级）

**需要完善 gcos_executor.c**:
```c
// 当前状态: 框架已搭建，指令执行是 stub
// 需要实现:

u16 executor_fetch_instruction(Executor *exec) {
    // 从模块程序区读取指令
    u8 opcode = exec->module_code[exec->pc++];
    return opcode;
}

void executor_execute_instruction(Executor *exec, u8 opcode) {
    switch (opcode) {
        case OP_ADD:
            exec_add(exec);
            break;
        case OP_SUB:
            exec_sub(exec);
            break;
        // ... 实现所有指令
    }
}
```

**指令实现示例**:
```c
static void exec_add(Executor *exec) {
    // 弹出两个操作数
    i32 op2 = stack_pop_i32(&exec->operand_stack);
    i32 op1 = stack_pop_i32(&exec->operand_stack);
    
    // 执行加法
    i32 result = op1 + op2;
    
    // 压入结果
    stack_push_i32(&exec->operand_stack, result);
}
```

---

#### 4. 栈帧管理（中优先级）

**需要实现**:
```c
typedef struct {
    u32 local_vars[MAX_LOCALS];     // 局部变量
    u32 operand_stack[STACK_SIZE];  // 操作数栈
    u16 sp;                         // 栈指针
    u16 pc;                         // 程序计数器
    u16 method_id;                  // 方法 ID
} StackFrame;

StackFrame* stack_frame_create(Method *method);
void stack_frame_destroy(StackFrame *frame);
void stack_frame_push(StackFrame *frame);
StackFrame* stack_frame_pop(void);
```

---

#### 5. 异常处理（低优先级）

**需要实现**:
```c
typedef struct {
    u16 exception_type;
    u16 handler_pc;
    u16 start_pc;
    u16 end_pc;
} ExceptionHandler;

void exception_throw(u16 exception_type);
ExceptionHandler* exception_find_handler(u16 exception_type);
void exception_handle(ExceptionHandler *handler);
```

---

## 📅 实施路线图

### Phase 1: 核心 APDU 命令（2-3 周）

**Week 1: SELECT 命令**
- [ ] Day 1-2: 实现 AID 匹配算法
- [ ] Day 3-4: 实现应用选择状态机
- [ ] Day 5: 实现 FCP 生成和返回
- [ ] 测试：使用 JCShell 发送 SELECT 命令

**Week 2: LOAD 命令**
- [ ] Day 1-2: 实现加载状态机
- [ ] Day 3-4: 实现数据块接收和缓冲
- [ ] Day 5: 实现加载完成处理
- [ ] 测试：流式加载 SEF 文件

**Week 3: INSTALL 命令**
- [ ] Day 1-2: 实现安装参数解析
- [ ] Day 3-4: 实现应用实例创建
- [ ] Day 5: 实现应用注册
- [ ] 测试：安装应用并验证

---

### Phase 2: SEF 文件解析（2 周）

**Week 4: 文件解析**
- [ ] Day 1-2: 实现 SEF 头部解析
- [ ] Day 3-4: 实现代码段解析
- [ ] Day 5: 实现数据段解析

**Week 5: 链接处理**
- [ ] Day 1-2: 实现链接段解析
- [ ] Day 3-4: 实现符号重定位
- [ ] Day 5: 实现模块链接
- [ ] 测试：加载和链接完整 SEF 文件

---

### Phase 3: 字节码执行器（3-4 周）

**Week 6-7: 基础指令**
- [ ] 实现算术运算指令（ADD, SUB, MUL, DIV）
- [ ] 实现逻辑运算指令（AND, OR, XOR）
- [ ] 实现数据访问指令（LDT, STT, LDM, STM）
- [ ] 测试：编写简单测试程序

**Week 8-9: 控制流指令**
- [ ] 实现跳转指令（BR, BEQZ, BNEZ）
- [ ] 实现函数调用指令（CALL, RET）
- [ ] 实现栈帧管理
- [ ] 测试：函数调用和返回

---

### Phase 4: 高级功能（2-3 周）

**Week 10: 事务管理**
- [ ] 完善事务备份/恢复机制
- [ ] 实现嵌套事务支持
- [ ] 测试：事务原子性

**Week 11: 安全管理**
- [ ] 实现应用隔离
- [ ] 实现接口授权表
- [ ] 实现类型检查
- [ ] 测试：安全边界

**Week 12: 优化和测试**
- [ ] 性能优化
- [ ] 完整测试套件
- [ ] 文档完善

---

## 🔧 开发工具和资源

### 参考实现
- **cref**: `e:\views\gcos\prog\cos\cref`
  - server.c - TCP 服务器
  - t0.c - T=0 协议
  - interpreter.c - 字节码解释器
  - memory.c - 内存管理
  - TransactionMgr.c - 事务管理

### 规范文档
- **cos3-qw.md**: GB/T 44901.3 完整规范
- **ARCHITECTURE_COMPARISON_GCOS_VS_CREF.md**: 架构对比

### 开发工具
- **编译器**: MSVC / GCC
- **构建系统**: CMake
- **调试工具**: GDB / Visual Studio Debugger
- **测试工具**: IBM JCShell

---

## 📝 关键设计决策

### 1. 零动态内存分配
**决策**: 坚持零动态内存分配设计原则

**理由**:
- 符合嵌入式环境要求
- 提高确定性和安全性
- 避免内存碎片

**实现**:
- 所有内存静态预分配
- 使用内存池管理
- 固定大小的数据结构

---

### 2. 渐进式开发
**决策**: 先实现 APDU 命令，再完善底层

**理由**:
- 快速验证通信链路
- 逐步增加功能复杂度
- 便于测试和调试

**实施**:
- Phase 1: APDU 命令（高层）
- Phase 2: 文件解析（中层）
- Phase 3: 字节码执行（底层）

---

### 3. 与 cref 保持兼容
**决策**: 通信协议和 APDU 格式与 cref 保持一致

**理由**:
- 可以使用现有工具（JCShell）
- 便于测试和验证
- 降低学习成本

**实施**:
- 使用相同的端口（9000/9900）
- 使用相同的 TLP224 协议
- 使用相同的 APDU 格式

---

## ⚠️ 风险和挑战

### 1. 技术风险

**风险**: SEF 文件格式复杂，解析困难

**缓解**:
- 详细研究 cref 的实现
- 分步骤实现（头部→代码→数据→链接）
- 充分的单元测试

---

**风险**: 指令集庞大（200+ 条指令）

**缓解**:
- 优先实现常用指令
- 使用代码生成工具
- 参考 cref 的实现

---

### 2. 时间风险

**风险**: 实施周期可能超出预期

**缓解**:
- 制定详细的周计划
- 每周进行进度评估
- 必要时调整范围

---

### 3. 质量风险

**风险**: 安全和稳定性问题

**缓解**:
- 严格的代码审查
- 完整的测试覆盖
- 参考 cref 的安全机制

---

## 📊 成功标准

### 功能标准
- [ ] 能够加载和安装 SEF 文件
- [ ] 能够选择和执行应用
- [ ] 支持多通道并发
- [ ] 事务机制正常工作
- [ ] 安全隔离有效

### 性能标准
- [ ] APDU 响应时间 < 100ms
- [ ] 内存使用 < 64KB
- [ ] 代码大小 < 256KB

### 兼容性标准
- [ ] 与 IBM JCShell 完全兼容
- [ ] 支持 T=0 和 T=CL 协议
- [ ] 符合 COS3 规范要求

---

## 🎯 下一步行动

### 立即行动（本周）
1. **实现 SELECT 命令**
   - 研究 cref 的 SELECT 实现
   - 实现 AID 匹配算法
   - 测试与 JCShell 的兼容性

2. **完善项目文档**
   - 更新 PROJECT_STATUS_REPORT.md
   - 记录设计决策
   - 制定详细的技术规范

### 短期行动（本月）
1. **完成 Phase 1**
   - SELECT/LOAD/INSTALL 命令
   - 基本的应用管理能力

2. **开始 Phase 2**
   - SEF 文件解析框架
   - 链接处理机制

### 中期行动（本季度）
1. **完成 Phase 2-3**
   - 完整的文件加载和链接
   - 基础指令集实现

2. **开始 Phase 4**
   - 事务和安全管理
   - 性能优化

---

## 📞 联系方式

如有问题或建议，请参考：
- **项目文档**: `docs/PROJECT_STATUS_REPORT.md`
- **架构对比**: `docs/ARCHITECTURE_COMPARISON_GCOS_VS_CREF.md`
- **开发者指南**: `docs/DEVELOPER_GUIDE.md`

---

**文档版本**: 1.0  
**最后更新**: 2026-05-11  
**下次更新**: Phase 1 完成后
