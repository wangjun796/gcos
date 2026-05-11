# GCOS VM 代码完善计划

## 📋 现状分析

### 1. 现有代码结构

```
gcos_vm/
├── include/                    # 头文件 (10个文件)
│   ├── gcos_vm.h              # ✅ 新API (750行) - 基于COS3规范
│   ├── gcos_instructions.h    # ✅ 新指令集定义 (204行)
│   ├── gcos_vm_full.h         # ⚠️ 旧API (混合命名)
│   ├── vm_core.h              # ⚠️ 旧核心定义
│   ├── vm_executor.h          # ⚠️ 旧执行器
│   ├── vm_instructions.h      # ⚠️ 旧指令集
│   ├── vm_loader.h            # ⚠️ 旧加载器
│   ├── vm_memory.h            # ⚠️ 旧内存管理
│   └── vm_types.h             # ⚠️ 旧类型定义
│
├── src/                        # 源代码 (11个文件)
│   ├── vm_core.c              # ⚠️ 旧实现 (674行) - 使用malloc
│   ├── vm_core_full.c         # ⚠️ 完整版本
│   ├── vm_executor.c          # ⚠️ 旧执行器 (部分实现)
│   ├── vm_executor_part2.c    # ⚠️ 执行器补充
│   ├── vm_instructions.c      # ⚠️ 旧指令集
│   ├── vm_instructions_full.c # ⚠️ 完整指令集 (54.8KB)
│   ├── vm_loader.c            # ⚠️ 旧加载器
│   ├── vm_loader_full.c       # ⚠️ 完整加载器
│   ├── vm_memory.c            # ⚠️ 旧内存管理
│   ├── vm_app_manager_full.c  # ⚠️ 应用管理器
│   └── vm_transaction_full.c  # ⚠️ 事务管理器
│
├── tests/                      # 测试
│   └── test_vm.c              # 测试程序
│
├── examples/                   # 示例
│   └── hello_app.c            # 示例应用
│
└── docs/                       # 文档
    ├── IMPLEMENTATION_PLAN.md           # ✅ 实现计划
    ├── COS3_VS_WASM_COMPARISON.md       # ✅ 对比分析
    ├── COMPARISON_SUMMARY.md            # ✅ 对比总结
    ├── DEVELOPER_GUIDE.md               # ✅ 开发指南
    └── TASK_TRACKER.md                  # ✅ 任务跟踪
```

---

## ⚠️ 发现的问题

### 问题1: 两套API并存，命名混乱

**现状**:
- `gcos_vm.h` / `gcos_instructions.h` - 新的COS3规范API
- `vm_*.h` / `vm_*.c` - 旧的通用VM API

**问题**:
- ❌ 两套API功能重叠
- ❌ 命名风格不一致 (`gcos_vm_` vs `vm_`)
- ❌ 数据结构不统一 (`GCOSVM` vs `VMContext`)
- ❌ CMakeLists.txt引用的是旧文件 (`vm_core_full.c`)

**影响**:
- 开发者困惑，不知道使用哪套API
- 维护困难，需要同步两套代码
- 不符合COS3规范要求

---

### 问题2: 违反"零动态内存分配"原则

**现状** (`vm_core.c`):
```c
VMContext* vm_create(void) {
    VMContext *vm = (VMContext*)malloc(sizeof(VMContext));  // ❌ 动态分配
    // ...
}

void* vm_heap_alloc(VMContext *vm, u32 size) {
    // 使用malloc/realloc  // ❌ 动态分配
}
```

**COS3规范要求**:
- ✅ 所有内存应在编译时静态分配
- ✅ 适合资源受限的嵌入式环境
- ✅ 避免内存碎片和泄漏

**影响**:
- 不符合智能卡应用场景
- 可能产生内存泄漏
- 增加系统复杂性

---

### 问题3: 缺少COS3核心特性实现

根据COS3规范和对比分析，以下关键特性缺失或不完整：

| 特性 | 状态 | 说明 |
|------|------|------|
| **分区内存管理** | ⚠️ 不完整 | 需要5个独立区域（栈/间接栈/全局数据/堆/代码区） |
| **非易失性堆** | ❌ 缺失 | 需要集成eflash库，掉电数据保持 |
| **事务管理** | ⚠️ 部分实现 | `vm_transaction_full.c`存在但未验证 |
| **多通道支持** | ⚠️ 部分实现 | `vm_app_manager_full.c`存在但未验证 |
| **SEF文件解析** | ⚠️ 部分实现 | `vm_loader_full.c`存在但需验证符合COS3规范 |
| **应用生命周期** | ⚠️ 部分实现 | 6种状态机需要完整实现 |
| **运行域隔离** | ❌ 缺失 | 安全管理需要加强 |
| **接口授权** | ❌ 缺失 | 需要授权表机制 |

---

### 问题4: 指令集实现与COS3规范不完全一致

**现状**:
- `vm_instructions_full.c` (54.8KB) - 大量指令实现
- `gcos_instructions.h` - 新的指令定义

**问题**:
- ❌ 两套指令集定义可能不一致
- ❌ 需要验证是否符合COS3附录A的256+条指令
- ❌ 操作码编码需要确认（单字节0x00-0xFB，双字节0xFC-0xFE）

---

### 问题5: 构建系统配置错误

**现状** (`CMakeLists.txt`):
```cmake
set(VM_SOURCES
    src/vm_core_full.c          # ❌ 应该用新的gcos_vm.c
    src/vm_executor_full.c      # ❌ 文件不存在
    src/vm_instructions_full.c  # ❌ 应该用新的gcos_instructions.c
    # ...
)
```

**问题**:
- ❌ 引用了不存在的文件 (`vm_executor_full.c`)
- ❌ 没有包含新的COS3规范实现文件
- ❌ 缺少必要的源文件

---

## 🎯 完善目标

### 目标1: 统一到COS3规范API

**行动**:
1. ✅ 废弃旧的`vm_*`命名，统一使用`gcos_vm_*`
2. ✅ 删除或重命名旧的头文件和源文件
3. ✅ 确保所有实现符合COS3规范

**优先级**: P0 (最高)

---

### 目标2: 实现零动态内存分配

**行动**:
1. ✅ 将所有`malloc/free`改为静态分配
2. ✅ 使用全局静态实例
3. ✅ 实现静态内存池

**示例**:
```c
// 旧代码 (❌)
VMContext* vm_create(void) {
    return (VMContext*)malloc(sizeof(VMContext));
}

// 新代码 (✅)
static GCOSVM g_gcos_vm_instance;  // 全局静态实例

GCOSVM* gcos_vm_create(void) {
    memset(&g_gcos_vm_instance, 0, sizeof(GCOSVM));
    return &g_gcos_vm_instance;
}
```

**优先级**: P0 (最高)

---

### 目标3: 完善COS3核心特性

#### 3.1 分区内存管理

**需要实现的5个区域**:

```c
typedef struct {
    // 1. 执行器栈 (256 × 4B = 1KB, 易失性)
    u32 executor_stack[256];
    u32 stack_pointer;
    
    // 2. 间接变量栈 (64 × 16B = 1KB, 易失性)
    u8 indirect_stack[64][16];
    u32 indirect_stack_pointer;
    
    // 3. 全局数据区 (4KB, 易失性)
    u8 global_data[4096];
    u32 global_data_used;
    
    // 4. 堆 (8KB, 非易失性 - 需要eflash集成)
    u8 heap[8192];
    u32 heap_used;
    
    // 5. 模块程序区 (16KB, 非易失性)
    u8 module_code[16384];
    u32 code_size;
} GCOSRuntimeContext;
```

**优先级**: P0

#### 3.2 事务管理

**需要实现的功能**:
```c
GCOSResult gcos_vm_transaction_begin(GCOSVM *vm);
GCOSResult gcos_vm_transaction_commit(GCOSVM *vm);
GCOSResult gcos_vm_transaction_abort(GCOSVM *vm);
```

**实现要点**:
- 备份堆数据和全局数据区
- 支持嵌套事务
- 异常时自动回滚

**优先级**: P1

#### 3.3 多通道管理

**需要实现的功能**:
```c
typedef struct {
    GCOSAppInstance *selected_app;  // 该通道选择的应用
    bool active;                     // 通道是否激活
    u8 channel_data[...];            // 通道独立数据
} GCOSChannel;

GCOSChannel channels[8];  // 8个逻辑通道
```

**优先级**: P1

#### 3.4 SEF文件解析

**需要验证**:
- ✅ 文件头格式 (sef_type = 0x00736566)
- ✅ 段解析 (First/Import/Function/App/Global/Export/Element/Data/Code)
- ✅ 符号链接
- ✅ 小端字节序

**优先级**: P1

---

### 目标4: 统一指令集实现

**行动**:
1. ✅ 以`gcos_instructions.h`为准
2. ✅ 创建`src/gcos_instructions.c`实现所有256+条指令
3. ✅ 删除旧的`vm_instructions*.c`
4. ✅ 实现指令跳转表

**优先级**: P0

---

### 目标5: 修复构建系统

**行动**:
1. ✅ 更新CMakeLists.txt引用正确的源文件
2. ✅ 添加新的COS3规范实现文件
3. ✅ 移除不存在的文件引用
4. ✅ 添加编译选项检查

**优先级**: P0

---

## 📝 实施计划

### Phase 1: 代码重构和统一 (P0)

**时间**: 2-3天

#### Day 1: 清理旧代码

1. **备份现有代码**
   ```bash
   git add .
   git commit -m "Backup before refactoring"
   ```

2. **重命名/删除旧文件**
   ```
   保留:
   - include/gcos_vm.h          ✅
   - include/gcos_instructions.h ✅
   
   删除或移动到deprecated/:
   - include/vm_*.h             ⚠️
   - src/vm_*.c                 ⚠️
   ```

3. **创建新的源文件结构**
   ```
   src/
   ├── gcos_vm.c              # VM主控制 (新建)
   ├── gcos_executor.c        # 执行引擎 (新建)
   ├── gcos_instructions.c    # 指令集实现 (新建)
   ├── gcos_loader.c          # SEF加载器 (新建)
   ├── gcos_memory.c          # 内存管理 (新建)
   ├── gcos_transaction.c     # 事务管理 (新建)
   ├── gcos_app_manager.c     # 应用管理 (新建)
   └── gcos_security.c        # 安全管理 (新建)
   ```

#### Day 2: 实现零动态内存分配

1. **修改VM创建/销毁**
   ```c
   // gcos_vm.c
   static GCOSVM g_gcos_vm_instance;
   
   GCOSVM* gcos_vm_create(void) {
       memset(&g_gcos_vm_instance, 0, sizeof(GCOSVM));
       return &g_gcos_vm_instance;
   }
   
   void gcos_vm_destroy(GCOSVM *vm) {
       // 只需重置，不需要free
       memset(vm, 0, sizeof(GCOSVM));
   }
   ```

2. **修改内存管理**
   ```c
   // gcos_memory.c
   u32 gcos_memory_heap_alloc(GCOSVM *vm, u32 size) {
       if (vm->runtime.heap_used + size > GCOS_HEAP_SIZE) {
           return 0;  // 内存不足
       }
       u32 addr = vm->runtime.heap_used;
       vm->runtime.heap_used += size;
       return addr;  // 返回偏移地址，不是指针
   }
   ```

3. **更新所有使用malloc的地方**

#### Day 3: 更新构建系统

1. **更新CMakeLists.txt**
   ```cmake
   set(VM_SOURCES
       src/gcos_vm.c
       src/gcos_executor.c
       src/gcos_instructions.c
       src/gcos_loader.c
       src/gcos_memory.c
       src/gcos_transaction.c
       src/gcos_app_manager.c
       src/gcos_security.c
   )
   ```

2. **测试编译**
   ```bash
   mkdir build && cd build
   cmake ..
   make -j4
   ```

---

### Phase 2: 完善核心特性 (P0-P1)

**时间**: 5-7天

#### Day 4-5: 分区内存管理

1. **实现5个区域的初始化**
2. **实现边界检查**
3. **实现访问控制**

#### Day 6-7: 事务管理

1. **实现备份/恢复机制**
2. **实现嵌套事务**
3. **与异常处理集成**

#### Day 8-9: 多通道和应用管理

1. **实现8通道管理**
2. **实现SELECT/DESELECT**
3. **实现生命周期状态机**

#### Day 10: SEF文件解析

1. **验证文件格式**
2. **实现段解析**
3. **实现符号链接**

---

### Phase 3: 指令集完善 (P0)

**时间**: 3-4天

#### Day 11-12: 实现指令Handler

1. **创建指令跳转表**
2. **实现所有256+条指令**
3. **参考`vm_instructions_full.c`的实现**

#### Day 13-14: 测试指令集

1. **编写单元测试**
2. **测试每条指令**
3. **性能测试**

---

### Phase 4: 测试和优化 (P1)

**时间**: 3-4天

#### Day 15-16: 集成测试

1. **端到端测试**
2. **多通道测试**
3. **事务测试**

#### Day 17-18: 性能优化

1. **Profiling分析**
2. **代码优化**
3. **内存优化**

---

## 📊 工作量评估

| 阶段 | 任务 | 预计天数 | 优先级 |
|------|------|----------|--------|
| Phase 1 | 代码重构和统一 | 3天 | P0 |
| Phase 2 | 完善核心特性 | 7天 | P0-P1 |
| Phase 3 | 指令集完善 | 4天 | P0 |
| Phase 4 | 测试和优化 | 4天 | P1 |
| **总计** | | **18天** | |

---

## 🎯 验收标准

### 功能性

- [ ] 所有API使用`gcos_vm_*`命名
- [ ] 零动态内存分配（无malloc/free）
- [ ] 5个分区内存区域正常工作
- [ ] 事务管理（begin/commit/abort）正常
- [ ] 8个通道独立运行
- [ ] SEF文件正确解析
- [ ] 256+条指令全部实现

### 性能

- [ ] RAM占用 < 32KB
- [ ] Flash占用 < 64KB
- [ ] 指令执行速度 > 10K instr/sec

### 代码质量

- [ ] 无编译警告
- [ ] 单元测试覆盖率 > 90%
- [ ] 符合COS3规范
- [ ] 代码注释完整

---

## 🚀 下一步行动

### 立即执行 (今天)

1. **阅读本计划文档**
2. **确认实施方案**
3. **备份现有代码**
   ```bash
   git add .
   git commit -m "Backup before COS3 refactoring"
   ```

### 明天开始

1. **创建新的源文件结构**
2. **实现gcos_vm.c基础框架**
3. **实现零动态内存分配**

---

## 📚 参考资料

- [IMPLEMENTATION_PLAN.md](docs/IMPLEMENTATION_PLAN.md) - 详细实现计划
- [COS3_VS_WASM_COMPARISON.md](docs/COS3_VS_WASM_COMPARISON.md) - 规范对比
- [COMPARISON_SUMMARY.md](docs/COMPARISON_SUMMARY.md) - 对比总结
- cos3-qw.md - COS3规范全文

---

**文档版本**: v1.0  
**创建日期**: 2026-05-09  
**作者**: GCOS VM Development Team  
**状态**: 📋 待审批
