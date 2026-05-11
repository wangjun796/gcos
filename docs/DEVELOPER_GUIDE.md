# GCOS VM 开发指南

## 📖 快速导航

- [实现计划](IMPLEMENTATION_PLAN.md) - 详细的模块设计、接口规范和开发路线图
- [架构文档](ARCHITECTURE.md) - 系统架构图和数据流图
- [项目概述](README_COS3_VM.md) - 项目介绍和快速开始

---

## 🚀 当前状态

**版本**: v1.0.0 (开发中)  
**阶段**: Phase 2 - 执行引擎实现  
**完成度**: 15% (核心框架已完成)

### ✅ 已完成

- [x] 项目结构设计
- [x] 主API头文件 (`include/gcos_vm.h`, 750行)
- [x] 指令集定义 (`include/gcos_instructions.h`, 204行)
- [x] 架构文档
- [x] 实现计划文档

### 🚧 进行中

- [ ] 执行引擎实现 (`src/gcos_executor.c`)
- [ ] 指令集实现 (`src/gcos_instructions.c`)

### 📋 待开始

- [ ] SEF文件加载器
- [ ] 应用管理器
- [ ] 安全管理器
- [ ] 事务管理器
- [ ] 内存管理
- [ ] 调试支持

---

## 📁 项目结构

```
gcos_vm/
├── include/                    # 头文件目录
│   ├── gcos_vm.h              # ✅ VM主API (750行)
│   └── gcos_instructions.h    # ✅ 指令集定义 (204行)
│
├── src/                        # 源代码目录
│   ├── gcos_vm.c              # 🚧 VM核心实现 (待创建)
│   ├── gcos_executor.c        # 🚧 执行引擎 (待创建) ⭐P0
│   ├── gcos_instructions.c    # 🚧 指令集实现 (待创建) ⭐P0
│   ├── gcos_loader.c          # 📋 SEF文件加载器 (待创建) P1
│   ├── gcos_app_manager.c     # 📋 应用管理器 (待创建) P1
│   ├── gcos_security.c        # 📋 安全管理器 (待创建) P2
│   ├── gcos_transaction.c     # 📋 事务管理器 (待创建) P2
│   ├── gcos_memory.c          # 📋 内存管理 (待创建) P1
│   └── gcos_debug.c           # 📋 调试支持 (待创建) P3
│
├── tests/                      # 测试目录
│   ├── test_executor.c        # 📋 执行器测试 (待创建)
│   ├── test_loader.c          # 📋 加载器测试 (待创建)
│   ├── test_app_manager.c     # 📋 应用管理测试 (待创建)
│   ├── test_security.c        # 📋 安全测试 (待创建)
│   ├── test_transaction.c     # 📋 事务测试 (待创建)
│   └── test_memory.c          # 📋 内存测试 (待创建)
│
├── examples/                   # 示例程序
│   └── hello_app.c            # 📋 简单应用示例 (待创建)
│
├── docs/                       # 文档目录
│   ├── IMPLEMENTATION_PLAN.md # ✅ 实现计划 (本文档的详细说明)
│   ├── ARCHITECTURE.md        # ✅ 架构设计
│   ├── README_COS3_VM.md      # ✅ 项目概述
│   └── DEVELOPER_GUIDE.md     # 📄 本文件
│
├── CMakeLists.txt              # CMake构建配置
└── README.md                   # 项目README
```

---

## 🛠️ 开发环境设置

### 编译要求

- **C编译器**: GCC 7.0+ / Clang 6.0+ / MSVC 2019+
- **C标准**: C99 或更高
- **构建系统**: CMake 3.10+
- **测试框架**: Unity 或 CMocka (可选)

### 编译步骤

```bash
# 1. 克隆仓库
git clone <repository-url>
cd gcos_vm

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置项目
cmake ..

# 4. 编译
make -j$(nproc)  # Linux/macOS
# 或
cmake --build . --config Release  # Windows

# 5. 运行测试
ctest -V
```

---

## 📝 编码规范

### 命名约定

```c
// 类型: PascalCase + GCOS前缀
typedef struct GCOSVM GCOSVM;
typedef enum GCOSResult GCOSResult;

// 函数: snake_case + 模块前缀
GCOSResult gcos_vm_create(void);
GCOSResult gcos_executor_run(GCOSVM *vm);

// 常量: UPPER_CASE + GCOS前缀
#define GCOS_EXECUTOR_STACK_SIZE 256
#define GCOS_OK 0

// 局部变量: snake_case
u32 stack_pointer;
const u8 *code_ptr;
```

### 注释规范

使用Doxygen风格注释：

```c
/**
 * @brief 简短描述 (一行)
 * 
 * 详细描述 (多行)
 * 
 * @param vm VM指针
 * @param opcode 操作码
 * @return GCOS_OK 成功, 其他错误码
 * 
 * @note 注意事项
 * @warning 警告信息
 */
GCOSResult gcos_executor_execute(GCOSVM *vm, u16 opcode);
```

### 错误处理

```c
// 统一使用GCOSResult返回错误
GCOSResult some_function(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOSResult ret = another_function(vm);
    if (ret != GCOS_OK) {
        return ret;  // 向上传播错误
    }
    
    return GCOS_OK;
}

// 使用宏简化错误检查
#define GCOS_CHECK(expr) do { \
    GCOSResult _ret = (expr); \
    if (_ret != GCOS_OK) return _ret; \
} while(0)
```

---

## 🧪 测试指南

### 编写单元测试

```c
#include "unity.h"
#include "gcos_vm.h"

void test_add_instruction(void) {
    GCOSVM *vm = gcos_vm_create();
    TEST_ASSERT_NOT_NULL(vm);
    
    gcos_vm_init(vm);
    
    // 压入两个操作数
    gcos_executor_push(vm, 5);
    gcos_executor_push(vm, 3);
    
    // 执行ADD指令
    GCOSResult ret = handler_add(vm, NULL, 0);
    TEST_ASSERT_EQUAL(GCOS_OK, ret);
    
    // 检查结果
    u32 result;
    gcos_executor_pop(vm, &result);
    TEST_ASSERT_EQUAL_UINT32(8, result);
    
    gcos_vm_destroy(vm);
}
```

### 运行测试

```bash
# 运行所有测试
ctest

# 运行特定测试
ctest -R test_executor

# 详细输出
ctest -V
```

---

## 📊 开发进度跟踪

### Phase 2: 执行引擎实现 (当前阶段)

**目标日期**: 2026-05-17  
**负责人**: 开发团队

#### Week 1: 基础框架和核心指令

| 任务 | 状态 | 预计完成 | 实际完成 | 备注 |
|------|------|----------|----------|------|
| 创建gcos_executor.c框架 | 📋 | Day 1 | - | - |
| 实现主循环gcos_executor_run() | 📋 | Day 1 | - | - |
| 实现取指/解码/执行 | 📋 | Day 2 | - | - |
| 实现栈操作push/pop | 📋 | Day 2 | - | - |
| 实现栈帧管理 | 📋 | Day 2 | - | - |
| 完善指令元数据表 | 📋 | Day 3 | - | - |
| 实现控制流指令 | 📋 | Day 4 | - | NOP, TRAP, BR, BEQZ, BNEZ, RET, CALL |
| 实现常量指令 | 📋 | Day 4 | - | CONST.I8/U8, I16/U16, I32/U32 |
| 实现算术指令 | 📋 | Day 5 | - | ADD, SUB, MUL, DIV, MOD, NEG |
| 实现逻辑指令 | 📋 | Day 5 | - | AND, OR, XOR, NOT, SHL, SHR, SHRU |
| 实现比较指令 | 📋 | Day 5 | - | EQ, NE, LT, GT, LE, GE (+无符号) |

#### Week 2: 高级指令和测试

| 任务 | 状态 | 预计完成 | 实际完成 | 备注 |
|------|------|----------|----------|------|
| 实现变量指令 | 📋 | Day 6 | - | LOAD/STORE (T2_C6, T4_C12, T8, T16) |
| 实现内存指令 | 📋 | Day 6 | - | LOAD/STORE (A8, A16, A32, BASE_A16) |
| 实现复合指令 | 📋 | Day 6 | - | DUP, SWAP, OVER, ROT |
| 实现异常指令 | 📋 | Day 7 | - | THROW, TRY, CATCH, ENDTRY |
| 编写单元测试 | 📋 | Day 8-9 | - | 覆盖率 > 90% |
| 性能优化 | 📋 | Day 9 | - | > 100K instr/sec |

---

## 🔍 代码审查清单

在提交代码前，请确保：

### 功能性
- [ ] 代码符合COS3规范要求
- [ ] 所有边界情况都已处理
- [ ] 错误处理完整且一致
- [ ] 没有内存泄漏

### 代码质量
- [ ] 遵循命名约定
- [ ] 添加了完整的注释
- [ ] 函数长度合理 (< 100行)
- [ ] 没有重复代码

### 测试
- [ ] 添加了单元测试
- [ ] 测试覆盖率 > 90%
- [ ] 所有测试通过
- [ ] 性能达标

### 安全性
- [ ] 所有输入都经过验证
- [ ] 所有内存访问都有边界检查
- [ ] 没有使用不安全的函数
- [ ] 敏感数据及时清零

---

## 📚 参考资料

### 标准文档
- **GB/T 44901.3-XXXX** 《卡及身份识别安全设备片上操作系统第3部分》
- **GB/T 44901.1-2024** 《卡及身份识别安全设备片上操作系统第1部分》
- **GB/T 44901.2-XXXX** 《卡及身份识别安全设备片上操作系统第2部分》

### 参考实现
- **[wasm3](https://github.com/wasm3/wasm3)** - 高性能WebAssembly解释器
- **[iwasm/WAMR](https://github.com/bytecodealliance/wasm-micro-runtime)** - WebAssembly微运行时
- **[JavaCard](https://www.oracle.com/java/technologies/javacard.html)** - Java Card技术规范

### 工具
- **CMake** - 构建系统
- **Unity/CMocka** - 测试框架
- **Doxygen** - 文档生成
- **Valgrind** - 内存泄漏检测
- **gcov/lcov** - 代码覆盖率

---

## 🤝 贡献指南

### 提交Issue

- 🐛 Bug报告：描述问题、复现步骤、期望行为
- 💡 功能建议：说明需求背景和预期效果
- 📖 文档改进：指出不清楚或缺失的部分

### 提交PR

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

### Code Review流程

1. 开发者提交PR
2. 自动CI检查（编译、测试）
3. 至少1名维护者审查代码
4. 解决审查意见
5. 合并到主分支

---

## 📞 联系方式

- **Email**: [待定]
- **Issues**: [GitHub Issues](https://github.com/your-repo/gcos_vm/issues)
- **Discussions**: [GitHub Discussions](https://github.com/your-repo/gcos_vm/discussions)

---

## 🎯 下一步行动

1. **立即开始**: 实现 `src/gcos_executor.c` 基础框架
2. **本周目标**: 完成核心指令Handler（控制流、常量、算术、逻辑、比较）
3. **下周目标**: 完成高级指令和单元测试

**加油！让我们一起打造优秀的国产智能卡虚拟机！** 🚀

---

**最后更新**: 2026-05-09  
**文档版本**: v1.0
