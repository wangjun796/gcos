# GCOS VM - 基于COS3规范的国产智能卡虚拟机

## 📖 项目概述

**GCOS VM** (GuoChao Operating System Virtual Machine) 是一个基于中华人民共和国国家标准 **GB/T 44901.3《卡及身份识别安全设备片上操作系统第3部分：支持面向过程应用后下载的基础层技术要求》** 实现的虚拟机系统。

### 🎯 设计目标

- ✅ **完全符合COS3规范** - 严格遵循国家标准要求
- ✅ **栈式字节码执行器** - 弹栈-执行-压栈计算模型
- ✅ **支持应用后下载** - SEF文件格式加载和安装
- ✅ **多通道应用隔离** - 最多8个逻辑通道独立运行
- ✅ **事务管理机制** - 原子性操作保证数据一致性
- ✅ **运行时安全管理** - 应用隔离、接口授权、异常处理
- ✅ **零动态内存分配** - 全局静态实例,适合嵌入式环境
- ✅ **参考成熟架构** - 借鉴 wasm3/iwasm 的设计模式

---

## 🏗️ 架构设计

### 系统架构图

```
┌─────────────────────────────────────────────────────┐
│                  应用层 (APDU Commands)               │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│              GCOS VM Public API                      │
│  gcos_vm.h - 统一的对外接口                         │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│              虚拟机核心层                             │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────┐  │
│  │  执行器      │  │  运行时数据   │  │  安全管理  │  │
│  │  Executor   │  │  Runtime     │  │  Security │  │
│  └─────────────┘  └──────────────┘  └───────────┘  │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│              模块管理层                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  │
│  │ 模块加载  │  │ 应用管理  │  │  通道管理        │  │
│  │ Loader   │  │ App Mgr  │  │  Channel Mgr    │  │
│  └──────────┘  └──────────┘  └──────────────────┘  │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│              指令集解释器                             │
│  gcos_instructions.h - 256+ 指令                    │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│          硬件抽象层 (HAL)                            │
│  Flash存储 | 随机数 | 密码算法 | 通信接口            │
└─────────────────────────────────────────────────────┘
```

### 核心组件

#### 1. 运行时数据区 (COS3规范表39)

| 组件 | 类型 | 大小 | 说明 |
|------|------|------|------|
| **执行器栈** | 易失性 | 256单元×4字节 | 栈帧、操作数、中间结果 |
| **间接访问变量栈** | 易失性 | 64单元×16字节 | 组合数据类型元素 |
| **全局数据区** | 易失性 | 4096字节 | 模块全局数据、临时数据 |
| **堆** | 非易失性 | 8192字节 | 持久性数据、应用域数据 |
| **程序计数器** | 易失性 | 4字节 | 当前指令地址 |
| **模块程序区** | 非易失性 | 16384字节 | 字节码指令 |

#### 2. 指令集架构 (COS3规范附录A)

**指令分类**:
- **控制指令** (0x00-0x1F): trap, nop, br, beqz, bnez, call, ret
- **数值指令** (0x20-0x9F): const, add, sub, mul, div, and, or, xor, cmp
- **变量指令** (0xA0-0xBF): load, store (局部变量访问)
- **内存指令** (0xC0-0xDF): load, store (内存访问)
- **异常处理** (0xE0-0xEF): throw, try, catch
- **复合指令** (0xF0-0xFB): dup, swap, over, rot
- **双字节扩展** (0xFC-0xFE): 可扩展指令集

**操作数编码**:
- LEB128可变长度编码
- 8/16/32位有符号/无符号常量
- 地址、地址偏移、局部变量索引
- 多功能指令标识符

#### 3. 文件格式 (COS3规范7.3)

**SEF (可加载文件) 结构**:
```
SEF File Header:
  - sef_type (u32): 'sef' = 0x00736566
  - version (u32): 版本号
  - sections[]: 段数组

Sections (按顺序):
  1. 首段 (0x01, 必选) - 模块信息、导入信息
  2. 导入段 (0x02, 可选) - 导入函数
  3. 函数段 (0x03, 必选) - 函数空间信息
  4. 应用段 (0x04, 可选) - 应用安装信息
  5. 全局段 (0x05, 必选) - 数据空间信息
  6. 导出段 (0x06, 可选) - 导出函数
  7. 元素段 (0x07, 可选) - 引用函数索引
  8. 数据段 (0x08, 可选) - 数据初始值
  9. 代码段 (0x09, 必选) - 字节码指令
  10. 自定义段 (0x0A-0x0F, 可选)
```

---

## 📁 项目结构

```
gcos_vm/
├── include/                    # 头文件目录
│   ├── gcos_vm.h              # ✨ 主API头文件 (750行)
│   ├── gcos_instructions.h    # ✨ 指令集定义 (204行)
│   ├── vm_types.h             # 类型定义 (旧版,待迁移)
│   ├── vm_core.h              # 核心结构 (旧版,待迁移)
│   ├── vm_memory.h            # 内存管理 (旧版,待迁移)
│   └── vm_loader.h            # 文件加载 (旧版,待迁移)
│
├── src/                        # 源代码目录
│   ├── gcos_vm.c              # 🚧 VM核心实现 (待创建)
│   ├── gcos_executor.c        # 🚧 执行器实现 (待创建)
│   ├── gcos_instructions.c    # 🚧 指令集实现 (待创建)
│   ├── gcos_loader.c          # 🚧 SEF文件加载器 (待创建)
│   ├── gcos_security.c        # 🚧 安全管理 (待创建)
│   └── gcos_transaction.c     # 🚧 事务管理 (待创建)
│
├── tests/                      # 测试目录
│   ├── test_vm.c              # 🚧 VM单元测试 (待创建)
│   ├── test_instructions.c    # 🚧 指令集测试 (待创建)
│   └── test_sef_loader.c      # 🚧 SEF加载测试 (待创建)
│
├── examples/                   # 示例程序
│   ├── hello_app.c            # 🚧 简单应用示例 (待创建)
│   └── apdu_handler.c         # 🚧 APDU命令处理示例 (待创建)
│
├── docs/                       # 文档目录
│   ├── COS3_SPEC_SUMMARY.md   # 📄 COS3规范摘要 (待创建)
│   ├── ARCHITECTURE.md        # 📄 架构设计文档 (已有)
│   └── API_REFERENCE.md       # 📄 API参考手册 (待创建)
│
├── tools/                      # 工具目录
│   ├── sef_compiler/          # 🚧 SEF编译器 (待创建)
│   └── sef_disassembler/      # 🚧 SEF反汇编器 (待创建)
│
├── CMakeLists.txt              # CMake构建配置
├── README.md                   # 本文件
└── CHANGELOG.md                # 版本历史
```

---

## 🚀 快速开始

### 编译要求

- **C编译器**: GCC 7.0+ / Clang 6.0+ / MSVC 2019+
- **C标准**: C99 或更高
- **构建系统**: CMake 3.10+

### 编译步骤

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置项目
cmake ..

# 3. 编译
make -j$(nproc)  # Linux/macOS
# 或
cmake --build . --config Release  # Windows

# 4. 运行测试
ctest -V
```

### 使用示例

```c
#include "gcos_vm.h"

int main(void) {
    // 1. 创建VM实例
    GCOSVM *vm = gcos_vm_create();
    if (!vm) {
        return -1;
    }
    
    // 2. 初始化VM
    if (gcos_vm_init(vm) != GCOS_OK) {
        gcos_vm_destroy(vm);
        return -1;
    }
    
    // 3. 加载SEF文件
    u8 module_index;
    GCOSResult ret = gcos_vm_load_module(vm, sef_data, sef_size, &module_index);
    if (ret != GCOS_OK) {
        gcos_vm_destroy(vm);
        return -1;
    }
    
    // 4. 安装应用
    GCOSAID app_aid = {{0xA0, 0x00, 0x00, 0x00, 0x63}, 5};
    GCOSAppInstance *app;
    ret = gcos_vm_install_app(vm, module_index, &app_aid, &app);
    
    // 5. 选择应用 (通道0)
    ret = gcos_vm_select_app(vm, 0, &app_aid);
    
    // 6. 执行APDU命令
    u8 apdu[] = {0x80, 0xCA, 0x00, 0x00, 0x00};
    u8 response[256];
    u32 response_len = sizeof(response);
    ret = gcos_vm_execute_apdu(vm, 0, apdu, sizeof(apdu), response, &response_len);
    
    // 7. 清理
    gcos_vm_deselect_app(vm, 0);
    gcos_vm_destroy(vm);
    
    return 0;
}
```

---

## 📊 技术特性对比

### vs WebAssembly 虚拟机 (wasm3/iwasm)

| 特性 | GCOS VM | wasm3 | iwasm (WAMR) |
|------|---------|-------|--------------|
| **规范基础** | COS3 (国标) | W3C WASM | W3C WASM |
| **应用场景** | 智能卡/安全芯片 | 通用嵌入式 | IoT/边缘计算 |
| **执行模型** | 栈式解释器 | 栈式解释器 | AOT/解释器/JIT |
| **内存模型** | 分区管理 (易失/非易失) | 线性内存 | 线性内存 |
| **事务支持** | ✅ 内置 | ❌ | ❌ |
| **多通道** | ✅ 8通道隔离 | ❌ | ❌ |
| **应用生命周期** | ✅ 完整管理 | ❌ | ❌ |
| **安全管理** | ✅ 运行时+系统级 | ⚠️ 沙箱 | ⚠️ 沙箱 |
| **掉电恢复** | ✅ 堆数据保持 | ❌ | ❌ |
| **零动态内存** | ✅ 全局静态 | ⚠️ 可选 | ❌ |
| **代码规模** | ~10KB (目标) | ~30KB | ~200KB |
| **RAM占用** | ~16KB (目标) | ~4KB | ~50KB |

### vs JavaCard VM

| 特性 | GCOS VM | JavaCard VM |
|------|---------|-------------|
| **编程语言** | C子集 → 字节码 | Java子集 → CAP |
| **指令集** | 自定义 (256+) | JavaCard bytecode |
| **对象模型** | 面向过程 | 面向对象 |
| **垃圾回收** | ❌ 手动管理 | ✅ 自动GC |
| **事务机制** | ✅ 内置 | ✅ JC Transaction |
| **文件大小** | 更小 | 较大 (CAP格式) |
| **性能** | 更快 (轻量) | 较慢 (OOP开销) |
| **标准化** | 中国国标 (GB/T) | 国际标准 (ISO) |

---

## 🔧 开发计划

### Phase 1: 核心框架 (已完成 ✅)

- [x] 项目结构设计
- [x] 主API头文件 (`gcos_vm.h`)
- [x] 指令集定义 (`gcos_instructions.h`)
- [x] 数据类型和常量定义
- [x] 架构文档

### Phase 2: 执行器实现 (进行中 🚧)

- [ ] 字节码解释器 (`gcos_executor.c`)
  - [ ] 指令解码
  - [ ] 栈操作实现
  - [ ] 控制流指令
  - [ ] 算术运算指令
  - [ ] 内存访问指令
- [ ] 运行时上下文管理
- [ ] 异常处理机制

### Phase 3: 模块和应用管理 (计划中 📋)

- [ ] SEF文件加载器 (`gcos_loader.c`)
  - [ ] 文件解析
  - [ ] 段提取
  - [ ] 链接解析
- [ ] 模块安装/卸载
- [ ] 应用实例管理
- [ ] 多通道选择机制

### Phase 4: 安全和事务 (计划中 📋)

- [ ] 事务管理器 (`gcos_transaction.c`)
  - [ ] begin/commit/abort
  - [ ] 数据备份/恢复
- [ ] 安全管理 (`gcos_security.c`)
  - [ ] 应用隔离
  - [ ] 接口授权
  - [ ] 访问控制

### Phase 5: 测试和优化 (计划中 📋)

- [ ] 单元测试套件
- [ ] 性能基准测试
- [ ] 安全审计
- [ ] 文档完善

---

## 📚 参考资料

### 标准文档

- **GB/T 44901.3-XXXX** 《卡及身份识别安全设备片上操作系统第3部分：支持面向过程应用后下载的基础层技术要求》
- **GB/T 44901.1-2024** 《卡及身份识别安全设备片上操作系统第1部分：总体要求》
- **GB/T 44901.2-XXXX** 《卡及身份识别安全设备片上操作系统第2部分：通用基础层技术要求》

### 参考实现

- **[wasm3](https://github.com/wasm3/wasm3)** - 高性能WebAssembly解释器
- **[iwasm/WAMR](https://github.com/bytecodealliance/wasm-micro-runtime)** - WebAssembly微运行时
- **[JavaCard](https://www.oracle.com/java/technologies/javacard.html)** - Java Card技术规范

### 相关资源

- [COS3规范摘要](docs/COS3_SPEC_SUMMARY.md) (待创建)
- [架构设计文档](ARCHITECTURE.md)
- [API参考手册](docs/API_REFERENCE.md) (待创建)

---

## 🤝 贡献指南

欢迎贡献代码、报告问题或提出建议!

### 提交Issue

- 🐛 Bug报告
- 💡 功能建议
- 📖 文档改进

### 提交PR

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

### 代码规范

- 遵循 C99 标准
- 使用 Google C++ Style Guide (C语言部分)
- 添加完整的注释和文档
- 编写单元测试

---

## 📄 许可证

本项目采用 **MIT License** - 详见 [LICENSE](LICENSE) 文件

---

## 👥 开发团队

**GCOS VM Development Team**

- 项目负责人: [待定]
- 核心开发者: [待定]
- 测试工程师: [待定]

---

## 📞 联系方式

- **Email**: [待定]
- **Issues**: [GitHub Issues](https://github.com/your-repo/gcos_vm/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-repo/gcos_vm/discussions)

---

## 🎉 致谢

感谢以下项目和团队的启发:

- **wasm3** - 优秀的WebAssembly解释器实现
- **iwasm/WAMR** - 功能丰富的WASM运行时
- **JavaCard** - 智能卡虚拟机的经典设计
- **COS3标准制定组** - 提供完善的国家标准规范

---

**最后更新**: 2026-05-08  
**版本**: v1.0.0 (开发中)  
**状态**: 🚧 核心框架已完成,执行器实现中

---

> **注意**: 本项目处于早期开发阶段,API可能会发生变化。欢迎参与讨论和贡献!
