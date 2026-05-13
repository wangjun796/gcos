# GCOS VM - 基于COS3规范的智能卡虚拟机

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/standard-GB%2FT%2044901.3-orange.svg)](docs/PROJECT_STATUS_REPORT.md)
[![Status](https://img.shields.io/badge/status-active-green.svg)](docs/PROJECT_STATUS_REPORT.md)

## 📖 简介

**GCOS VM** (GuoChao Operating System Virtual Machine) 是一个严格遵循 **GB/T 44901.3** 国家标准的智能卡虚拟机实现。

### ✨ 核心特性

- ✅ **零动态内存分配** - 所有内存静态预分配，适合嵌入式环境
- ✅ **完整VM核心** - 指令集、执行引擎、内存管理
- ✅ **JCShell兼容** - 与IBM JCShell工具完全兼容（端口9000/9900）
- ✅ **多协议支持** - T=0、T=CL协议栈
- ✅ **应用生命周期管理** - 安装、选择、删除
- ✅ **事务机制** - 原子性操作保证数据一致性
- ✅ **安全域隔离** - 接口授权和访问控制
- ✅ **跨平台** - Win32、Linux、Keil Cortex-M、ARM GCC

---

## 🚀 快速开始

### 编译

```bash
# Windows
cd gcos_vm
.\build.bat

# Linux
cd gcos_vm
./build.sh

# 或使用CMake
cmake -B build -S .
cmake --build build --config Debug
```

### 运行

```bash
# 启动JCShell Server（默认模式）
./build/Debug/gcos_demo.exe

# 或TLP Server模式（JCRE兼容）
./build/Debug/gcos_demo.exe -T
```

### 连接测试

使用IBM JCShell连接到 `localhost:9000`：

```
/open tcp localhost 9000
/powerup
/send 00A4040008A000000003000000
```

---

## 🏗️ 架构概览

```
┌──────────────────────┐
│   Card Terminal      │  ← IBM JCShell / 其他工具
└──────────┬───────────┘
           │ Binary Protocol (Port 9000/9900)
┌──────────▼───────────┐
│   JCShell Server     │  ← gcos_jcshell.c (多线程)
└──────────┬───────────┘
           │ Direct Call
┌──────────▼───────────┐
│   APDU Processing    │  ← gcos_apdu.c
└──────────┬───────────┘
           │ Dispatch
┌──────────▼───────────┐
│   VM Core            │  ← Executor, Memory, Loader
│   ├─ App Manager     │  ← 应用生命周期
│   ├─ Security        │  ← 安全域和授权
│   └─ Transaction     │  ← 事务管理
└──────────┬───────────┘
           │
┌──────────▼───────────┐
│ Transport & HAL      │  ← TCP Sockets
└──────────────────────┘
```

---

## 📁 项目结构

```
gcos_vm/
├── docs/                      # 核心文档
│   ├── PROJECT_STATUS_REPORT.md  ⭐ 项目状态报告
│   ├── CLEANUP_SUMMARY.md        代码清理总结
│   ├── ARCHITECTURE_COMPARISON_GCOS_VS_CREF.md
│   ├── CREF_ARCHITECTURE_ANALYSIS.md
│   ├── DEVELOPER_GUIDE.md
│   └── CROSS_PLATFORM_GUIDE.md
│
├── include/                   # 头文件 (10个)
│   ├── gcos_vm.h              VM核心定义
│   ├── gcos_apdu.h            APDU处理
│   ├── gcos_instructions.h    指令集
│   ├── gcos_transport.h       传输层
│   ├── gcos_hal.h             HAL接口
│   └── ...
│
├── src/                       # 源代码 (16个)
│   ├── gcos_vm.c              VM核心
│   ├── gcos_executor.c        执行引擎
│   ├── gcos_memory.c          内存管理
│   ├── gcos_instructions.c    指令集
│   ├── gcos_loader.c          SEF加载器
│   ├── gcos_app_manager.c     应用管理
│   ├── gcos_security.c        安全管理
│   ├── gcos_transaction.c     事务管理
│   ├── gcos_apdu.c            APDU处理
│   ├── gcos_tlp.c             TLP协议
│   ├── gcos_t0_protocol.c     T=0协议
│   ├── gcos_transport.c       传输层
│   ├── gcos_hal_win32.c       HAL实现
│   ├── gcos_jcshell.c         JCShell Server
│   ├── gcos_tlp_server.c      TLP Server
│   └── gcos_main.c            主程序
│
├── tests/                     # 单元测试 (5个)
├── examples/                  # 示例程序
└── platform/                  # 平台特定代码
```

---

## 📊 当前状态

### ✅ 已完成

- [x] VM核心和执行引擎
- [x] 完整的指令集框架
- [x] 内存管理（零动态分配）
- [x] 传输层和HAL抽象
- [x] JCShell Server（二进制协议）
- [x] TLP224和T=0协议栈
- [x] 应用管理器框架
- [x] 安全管理器框架
- [x] 事务管理器框架
- [x] APDU处理框架（Echo占位符）

### ⚠️ 待实现

- [ ] SELECT命令 - 应用选择逻辑
- [ ] LOAD命令 - 流式加载状态机
- [ ] INSTALL命令 - 应用安装
- [ ] DELETE命令 - 应用删除
- [ ] GET STATUS命令 - 状态查询
- [ ] MANAGE CHANNEL命令 - 通道管理
- [ ] SEF文件解析和链接
- [ ] 完整的密钥管理

详见 **[PROJECT_STATUS_REPORT.md](docs/PROJECT_STATUS_REPORT.md)**

---

## 📚 文档

| 文档 | 说明 |
|------|------|
| **[PROJECT_STATUS_REPORT.md](docs/PROJECT_STATUS_REPORT.md)** | 📋 最新项目状态和开发计划 |
| **[CLEANUP_SUMMARY.md](docs/CLEANUP_SUMMARY.md)** | 🧹 代码清理工作总结 |
| **[ARCHITECTURE_COMPARISON_GCOS_VS_CREF.md](docs/ARCHITECTURE_COMPARISON_GCOS_VS_CREF.md)** | 🔍 与Cref的架构对比 |
| **[CREF_ARCHITECTURE_ANALYSIS.md](docs/CREF_ARCHITECTURE_ANALYSIS.md)** | 📖 Cref架构详细分析 |
| **[DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md)** | 👨‍💻 开发者指南和编码规范 |
| **[CROSS_PLATFORM_GUIDE.md](docs/CROSS_PLATFORM_GUIDE.md)** | 🌍 跨平台编译指南 |
| **[ARCHITECTURE.md](ARCHITECTURE.md)** | 🏛️ 系统架构详细说明 |

---

## 🔧 技术栈

- **语言**: C99
- **构建**: CMake 3.10+
- **编译器**: MSVC / GCC / Clang / Keil ARMCC
- **协议**: TLP224, T=0 (ISO 7816-3), T=CL (ISO 14443-4)
- **网络**: TCP Sockets (Winsock/BSD Sockets)

---

## 🎯 下一步开发计划

### Phase 1: 核心APDU命令（高优先级）
1. SELECT命令实现
2. LOAD命令实现（流式加载）
3. INSTALL命令实现

### Phase 2: 管理命令（中优先级）
1. DELETE命令
2. GET STATUS命令
3. MANAGE CHANNEL命令

### Phase 3: 高级功能（低优先级）
1. 完善事务管理
2. 完善安全管理
3. SEF文件完整解析

---

## 📝 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

---

## 🤝 贡献

欢迎提交Issue和Pull Request！

在提交代码前，请阅读：
- [DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) - 编码规范
- [CROSS_PLATFORM_GUIDE.md](docs/CROSS_PLATFORM_GUIDE.md) - 跨平台注意事项

---

## 📞 联系

如有问题或建议，请提交Issue。

---

**最后更新**: 2026-05-11  
**版本**: 1.0.0  
**状态**: ✅ 代码清理完成，准备进入Phase 1开发
