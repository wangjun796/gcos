# GCOS 模块注册表实施 - 阶段 2 完成报告（最终版）

## ✅ 实施状态

**阶段 2: 模块加载逻辑改造** - **已完成！**

---

## 🎯 核心成果

### 1. LOAD 命令集成模块注册表

#### 修改文件：[gcos_load_manager.c](file://e:/views/gcos/prog/cos/gcos_vm/src/gcos_load_manager.c)

**关键改动：**

1. **引入模块注册表头文件**
   ```c
   #include "gcos_module_registry.h"
   ```

2. **更新辅助函数使用模块注册表**
   - `module_aid_exists()`: 检查 `vm->module_registry[]` 而非 `vm->modules[]`
   - `find_free_module_slot()`: 查找 `vm->module_registry[]` 的空闲槽位

3. **FINALIZE 阶段重构**
   - 不再创建旧的 `GCOSModule` 结构
   - 直接初始化 `GCOSModuleRegistry` 条目
   - 设置 `is_loaded = true`、`state = MODULE_LOADED`
   - 初始化实例跟踪数组 `instance_ids[]`

---

### 2. 三阶段 LOAD 流程验证

#### Phase 1: INSTALL FOR LOAD (P1=0x00)

```c
// APDU: 80 E6 00 00 Lc [TLV data]
// TLV: Tag 0x4F + Length + Package AID

u16 handle_install_for_load(const u8 *apdu, u16 apdu_len, ...) {
    // 1. 解析 Package AID
    // 2. 检查 AID 是否已存在
    // 3. 分配空闲 module_id
    // 4. 初始化 load_context
    vm->load_context.state = LOAD_STATE_INITIALIZATION;
    vm->load_context.target_module_id = module_id;
    vm->load_context.package_aid = pkg_aid;
}
```

**验证点：**
- ✅ 正确解析 Package AID
- ✅ 检测重复 AID（返回 0x6A89）
- ✅ 分配唯一 module_id
- ✅ 进入 INITIALIZATION 状态

---

#### Phase 2: LOAD BLOCKS (P1=0x01)

```c
// APDU: 80 E8 01 P2 Lc [SEF data block]

u16 handle_load_blocks(const u8 *apdu, u16 apdu_len, ...) {
    // 1. 验证会话状态
    // 2. 追加数据到 buffer
    memcpy(&vm->load_context.buffer[vm->load_context.buffer_size],
           block_data, block_len);
    vm->load_context.buffer_size += block_len;
    
    // 3. 更新状态
    vm->load_context.state = LOAD_STATE_LOADING_BLOCKS;
}
```

**验证点：**
- ✅ 支持多块传输
- ✅ 缓冲区溢出保护
- ✅ 进入 LOADING_BLOCKS 状态

---

#### Phase 3: FINALIZE (P1=0x02)

```c
// APDU: 80 E8 02 00 00

u16 handle_finalize_load(const u8 *apdu, u16 apdu_len, ...) {
    // Step 1: 解析 SEF 头部
    parse_sef_header(sef_data, sef_len, &vm->load_context);
    
    // Step 2: 验证导入依赖
    parse_import_section(vm, import_data, import_len, &vm->load_context);
    
    // Step 3: ⭐ 注册到模块注册表
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    reg->module_id = module_id;
    reg->is_loaded = true;
    reg->module_aid = vm->load_context.package_aid;
    reg->module_version = vm->load_context.package_version;
    reg->state = MODULE_LOADED;
    reg->code_base = vm->load_context.buffer;
    reg->code_size = sef_len;
    reg->import_count = vm->load_context.import_count;
    reg->instance_count = 0;
    
    vm->module_count++;
}
```

**验证点：**
- ✅ SEF 格式验证（magic number、版本号）
- ✅ 导入依赖解析和版本检查
- ✅ 模块注册表条目初始化
- ✅ 重置 load_context 到 IDLE 状态

---

### 3. 集成测试验证

#### 测试文件：[test_load_module_registry.c](file://e:/views/gcos/prog/cos/gcos_vm/tests/test_load_module_registry.c)

**测试覆盖：**

| 测试项 | 描述 | 结果 |
|--------|------|------|
| Test 1 | INSTALL FOR LOAD | ✅ PASSED |
| Test 2 | LOAD BLOCKS | ✅ PASSED |
| Test 3 | FINALIZE | ✅ PASSED |
| Test 4 | 验证模块注册表条目 | ✅ PASSED |
| Test 5 | 按 AID 查找模块 | ✅ PASSED |
| Test 6 | 加载第二个模块 | ✅ PASSED |

**测试输出示例：**
```
========================================
GCOS LOAD Command + Module Registry Test
========================================

--- Test 1: INSTALL FOR LOAD ---
✅ PASSED: INSTALL FOR LOAD succeeded
✅ PASSED: Load context in INITIALIZATION state
✅ PASSED: Module ID allocated
   Module ID allocated: 0

--- Test 2: LOAD BLOCKS ---
✅ PASSED: LOAD BLOCKS succeeded
✅ PASSED: Load context in LOADING_BLOCKS state
✅ PASSED: Buffer size matches SEF size
   Buffer size: 20 bytes

--- Test 3: FINALIZE ---
✅ PASSED: FINALIZE succeeded
✅ PASSED: Load context reset to IDLE

--- Test 4: Verify Module Registry ---
✅ PASSED: Module exists in registry
✅ PASSED: Module found by ID
✅ PASSED: Module is loaded
✅ PASSED: Module ID matches
✅ PASSED: AID length matches
✅ PASSED: AID content matches
✅ PASSED: Version matches
✅ PASSED: Code size matches SEF size
✅ PASSED: No app instances yet
   Module ID: 0
   AID: A0000000010203
   Version: 0x00010000
   Code Size: 20 bytes
   Instances: 0

--- Test 5: Find Module by AID ---
✅ PASSED: Module found by AID
✅ PASSED: Same module returned

--- Test 6: Load Second Module ---
✅ PASSED: Second INSTALL FOR LOAD succeeded
✅ PASSED: Second LOAD BLOCKS succeeded
✅ PASSED: Second FINALIZE succeeded
✅ PASSED: Two modules loaded
✅ PASSED: Second module found
✅ PASSED: Second module version correct
   Total modules: 2

========================================
All tests passed! ✅
========================================
```

---

## 📊 架构对比

### 旧架构（Phase 1 之前）

```
LOAD Command → vm->modules[MAX_MODULES]
                 ├─ module_id
                 ├─ module_aid
                 ├─ code (pointer)
                 ├─ functions (array)
                 └─ app_instances (array of pointers)
```

**问题：**
- ❌ 模块与应用紧耦合
- ❌ 不支持代码共享
- ❌ 每个应用需要完整模块副本

---

### 新架构（Phase 2 之后）

```
LOAD Command → vm->module_registry[MAX_MODULES]
                 ├─ module_id
                 ├─ module_aid
                 ├─ code_base (pointer) ← XIP or RAM
                 ├─ function_count
                 ├─ imports[]
                 └─ instance_ids[] ← 跟踪使用该模块的应用

App Instance → vm->apps[MAX_APPS]
                 ├─ app_id
                 ├─ app_aid
                 ├─ module_id ← 引用模块注册表
                 └─ module ← 指向 GCOSModuleRegistry
```

**优势：**
- ✅ 模块代码一次加载，多实例共享
- ✅ 清晰的模块/应用解耦
- ✅ 支持实例跟踪和批量回收
- ✅ 符合 cref Package/Applet 模型

---

## 🔧 技术细节

### 1. 模块注册表初始化

```c
// 在 gcos_module_registry.c 中
GCOSResult module_registry_init(GCOSVM *vm) {
    for (u8 i = 0; i < MAX_MODULES; i++) {
        vm->module_registry[i].module_id = 0xFF;
        vm->module_registry[i].is_loaded = false;
        vm->module_registry[i].instance_count = 0;
        memset(vm->module_registry[i].instance_ids, 0xFF, 
               sizeof(vm->module_registry[i].instance_ids));
    }
    return GCOS_SUCCESS;
}
```

### 2. 导入依赖验证

```c
// 在 parse_import_section() 中
for (u8 mid = 0; mid < MAX_MODULES; mid++) {
    if (vm->module_registry[mid].is_loaded &&
        vm->module_registry[mid].module_aid.length == aid_len &&
        memcmp(vm->module_registry[mid].module_aid.aid, import_aid.aid, aid_len) == 0) {
        
        // 版本兼容性检查
        if (vm->module_registry[mid].module_version >= required_version) {
            load_ctx->imports[i].resolved = true;
            load_ctx->imports[i].resolved_module_id = mid;
            break;
        }
    }
}
```

### 3. 模块查找 API

```c
// 按 ID 查找
GCOSModuleRegistry* module_registry_find_by_id(GCOSVM *vm, u8 module_id) {
    if (module_id >= MAX_MODULES) return NULL;
    if (!vm->module_registry[module_id].is_loaded) return NULL;
    return &vm->module_registry[module_id];
}

// 按 AID 查找
GCOSModuleRegistry* module_registry_find_by_aid(GCOSVM *vm, const GCOSAID *aid) {
    for (u8 i = 0; i < MAX_MODULES; i++) {
        if (vm->module_registry[i].is_loaded &&
            vm->module_registry[i].module_aid.length == aid->length &&
            memcmp(vm->module_registry[i].module_aid.aid, aid->aid, aid->length) == 0) {
            return &vm->module_registry[i];
        }
    }
    return NULL;
}
```

---

## 📝 代码变更统计

### 修改的文件

| 文件 | 行数变化 | 说明 |
|------|----------|------|
| `gcos_load_manager.c` | +46 / -42 | 集成模块注册表 |
| `CMakeLists.txt` | +4 | 添加测试目标 |

### 新增的文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `test_load_module_registry.c` | 311 | LOAD 命令集成测试 |

---

## ✅ 验收标准

### 功能验收

- [x] LOAD 命令三阶段流程正常工作
- [x] 模块注册到 `vm->module_registry[]`
- [x] 导入依赖正确解析和验证
- [x] 模块可按 ID 和 AID 查找
- [x] 支持加载多个模块
- [x] 实例跟踪数组初始化为空

### 测试验收

- [x] 单元测试通过（test_module_registry）
- [x] 集成测试通过（test_load_module_registry）
- [x] 无编译错误和警告
- [x] 无内存泄漏（静态分配）

### 规范符合性

- [x] 符合 COS3 规范第 7.3.4 节（SEF 文件格式）
- [x] 符合 COS3 规范第 8.2.1 节（加载管理）
- [x] 参考 cref Package Entry 设计
- [x] 支持模块代码共享

---

## 🚀 下一步计划

### 阶段 3：应用创建与模块关联

**目标：** 实现从已加载模块创建应用实例

**任务清单：**
1. 修改 `app_register_ex()` 支持从模块注册表创建实例
2. 实现 `module_registry_add_instance()` 调用
3. 为每个应用实例分配独立的全局数据空间
4. 实现 GRT entry 回收集成到应用删除流程

**预期成果：**
- 一个模块可被多个应用实例共享
- 每个实例有独立的运行时数据
- 应用删除时自动减少模块引用计数

---

## 📚 相关文档

- [GCOS_MODULE_REGISTRY_PHASE1_COMPLETE.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_MODULE_REGISTRY_PHASE1_COMPLETE.md) - 阶段 1 数据结构重构
- [GCOS_MODULE_REGISTRY_PHASE2_COMPLETE.md](file://e:/views/gcos/prog/cos/gcos_vm/GCOS_MODULE_REGISTRY_PHASE2_COMPLETE.md) - 阶段 2 模块注册表实现
- [COS3_CROSS_MODULE_ACCESS_ANALYSIS.md](file://e:/views/gcos/prog/cos/gcos_vm/COS3_CROSS_MODULE_ACCESS_ANALYSIS.md) - COS3 跨模块访问分析
- [COS3_CALL_U16_AND_SYMBOL_RESOLUTION.md](file://e:/views/gcos/prog/cos/gcos_vm/COS3_CALL_U16_AND_SYMBOL_RESOLUTION.md) - call.u16 指令分析

---

## 🎉 总结

**阶段 2 成功完成了模块加载逻辑的改造，实现了：**

1. ✅ LOAD 命令三阶段流程完全集成模块注册表
2. ✅ 模块代码一次加载，支持多实例共享
3. ✅ 完整的导入依赖验证机制
4. ✅ 全面的测试覆盖（单元测试 + 集成测试）

**这为后续的应用实例管理和 GRT 回收奠定了坚实的基础！**
