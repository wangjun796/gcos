# GCOS VM APDU架构调整 - 实施路线图

## 🎯 核心目标

将gcos_vm从**WASM风格**（C API调用）调整为**JavaCard风格**（APDU驱动），参考cref实现。

---

## 📊 当前状态 vs 目标状态

### 当前架构 (WASM风格) ❌
```
外部程序 → C API调用 → gcos_vm
           ├─ gcos_vm_load_sef(vm, data, size)
           ├─ gcos_vm_install_app(vm, aid, ...)
           └─ gcos_vm_execute(vm, function_id)
```

### 目标架构 (APDU驱动) ✅
```
智能卡读卡器 → APDU命令流 → gcos_apdu_handler → gcos_vm
                ├─ LOAD (0xE8)     → 流式加载状态机
                ├─ INSTALL (0xE6)  → 应用安装
                ├─ SELECT (0xA4)   → 应用选择
                └─ CUSTOM (0xXX)   → 应用自定义命令
```

---

## 🗺️ 分阶段实施计划

### 阶段1: APDU基础设施框架 (3天) 🔴 高优先级

#### Day 1: APDU解析层
- [ ] 创建 `include/gcos_apdu.h`
  - 定义GCOSSApdu结构体
  - 定义GCOSSwStatus状态码
  - 声明APDU处理函数签名
  
- [ ] 创建 `src/gcos_apdu.c`
  - 实现`parse_apdu()` - APDU字节流解析
  - 实现`make_sw_status()` - 状态码构建
  - 实现`find_apdu_handler()` - 命令分发

- [ ] 定义标准APDU指令集
  ```c
  #define INS_LOAD        0xE8
  #define INS_INSTALL     0xE6
  #define INS_DELETE      0xE4
  #define INS_SELECT      0xA4
  #define INS_DESELECT    0xAA
  #define INS_GET_STATUS  0xF2
  ```

#### Day 2: APDU命令表
- [ ] 实现ApduCommandTable结构
- [ ] 创建默认命令表
- [ ] 实现命令注册机制
- [ ] 添加CLA字节解析（提取逻辑通道）

#### Day 3: 主处理循环
- [ ] 实现`gcos_vm_process_apdu()`主入口
- [ ] 集成到现有VM架构
- [ ] 编写基础单元测试
- [ ] 验证APDU解析正确性

**交付物**: 
- ✅ 可解析APDU并分发到处理函数的框架
- ✅ 基础状态码管理
- ✅ 单元测试通过

---

### 阶段2: 流式加载器 (4天) 🔴 高优先级

#### Day 4: 加载上下文管理
- [ ] 创建 `include/gcos_stream_loader.h`
  - 定义StreamLoaderState枚举
  - 定义StreamLoadContext结构体
  
- [ ] 创建 `src/gcos_stream_loader.c`
  - 实现`init_load_context()` - 初始化加载
  - 实现`cleanup_load_context()` - 清理资源

#### Day 5-6: LOAD状态机实现
- [ ] 实现LOAD_P1_INIT处理
  - 分配缓冲区
  - 验证总大小
  - 创建加载上下文
  
- [ ] 实现LOAD_P1_DATA处理
  - 验证序列号
  - 复制到缓冲区
  - 更新校验和
  - 检测最终块
  
- [ ] 实现LOAD_P1_LINK处理
  - 解析SEF头
  - 验证完整性
  - 切换到链接状态

#### Day 7: 链接与安装
- [ ] 实现符号链接解析
- [ ] 集成现有gcos_loader功能
- [ ] 实现INSTALL APDU处理器
- [ ] 端到端测试（LOAD + INSTALL）

**交付物**:
- ✅ 支持多APDU分段传输的加载器
- ✅ 完整的LOAD状态机
- ✅ 数据完整性验证
- ✅ 与现有SEF解析器集成

---

### 阶段3: 应用选择机制 (3天) 🟡 中优先级

#### Day 8: SELECT/DESELECT处理器
- [ ] 创建 `include/gcos_selector.h`
- [ ] 创建 `src/gcos_selector.c`
- [ ] 实现`handle_select_apdu()`
  - AID查找
  - 生命周期检查
  - 通道选择
  
- [ ] 实现`handle_deselect_apdu()`
  - 清除选择状态
  - 恢复默认上下文

#### Day 9: FCI响应构建
- [ ] 实现FCI (File Control Information)格式
- [ ] 构建SELECT成功响应
- [ ] 添加应用元数据（AID、版本、状态）

#### Day 10: 通道集成
- [ ] 增强通道管理与APDU绑定
- [ ] 每个通道维护独立的选择状态
- [ ] 实现通道切换时的上下文保存/恢复

**交付物**:
- ✅ SELECT/DESELECT命令完整实现
- ✅ FCI响应符合ISO7816-4规范
- ✅ 通道与应用选择解耦

---

### 阶段4: 事务与APDU集成 (2天) 🟡 中优先级

#### Day 11: 事务APDU
- [ ] 实现BEGIN TRANSACTION APDU
- [ ] 实现COMMIT TRANSACTION APDU
- [ ] 实现ABORT TRANSACTION APDU

#### Day 12: 自动回滚
- [ ] 异常时自动触发事务回滚
- [ ] APDU错误状态与事务状态同步
- [ ] 测试事务原子性

**交付物**:
- ✅ 事务控制通过APDU触发
- ✅ 异常安全保证

---

### 阶段5: 测试与优化 (4天) 🟢 低优先级

#### Day 13-14: APDU测试套件
- [ ] 创建模拟智能卡读卡器
- [ ] 编写LOAD流式传输测试
- [ ] 编写SELECT/DESELECT测试
- [ ] 编写事务回滚测试

#### Day 15: 性能优化
- [ ] 缓冲区管理优化
- [ ] AID查找算法优化（哈希表）
- [ ] 减少内存拷贝

#### Day 16: 文档与示例
- [ ] 编写APDU使用指南
- [ ] 创建示例APDU序列
- [ ] 更新ARCHITECTURE.md

**交付物**:
- ✅ 完整的APDU测试套件
- ✅ 性能基准报告
- ✅ 开发者文档

---

## 📁 文件结构变更

### 新增文件
```
gcos_vm/
├── include/
│   ├── gcos_apdu.h              ← 新增
│   ├── gcos_stream_loader.h     ← 新增
│   ├── gcos_selector.h          ← 新增
│   └── gcos_channel.h           ← 新增（增强）
├── src/
│   ├── gcos_apdu.c              ← 新增
│   ├── gcos_stream_loader.c     ← 新增
│   ├── gcos_selector.c          ← 新增
│   └── gcos_channel.c           ← 新增（增强）
└── tests/
    ├── test_apdu_parser.c       ← 新增
    ├── test_stream_loader.c     ← 新增
    └── test_select_mechanism.c  ← 新增
```

### 修改文件
```
gcos_vm/
├── include/gcos_vm.h            ← 添加APDU相关API
├── src/gcos_loader.c            ← 重构为状态机模式
├── src/gcos_transaction.c       ← 添加APDU触发接口
└── CMakeLists.txt               ← 添加新源文件
```

---

## ✅ 验收标准

### 功能验收
- [ ] 能够通过APDU流式加载SEF文件（至少分3次传输）
- [ ] 能够安装、选择、执行应用
- [ ] 支持8个逻辑通道并发
- [ ] 事务提交/回滚正常工作
- [ ] 所有APDU返回正确的SW1SW2状态码

### 兼容性验收
- [ ] 与cref的行为一致（相同APDU产生相同响应）
- [ ] 符合COS3规范要求
- [ ] 符合ISO7816-4 APDU格式

### 性能验收
- [ ] 单次APDU处理时间 < 10ms (10MHz CPU)
- [ ] 流式加载吞吐量 > 1KB/s
- [ ] 内存占用 < 原有实现的110%

### 代码质量验收
- [ ] 所有新代码有英文注释
- [ ] 单元测试覆盖率 > 80%
- [ ] 无编译器警告
- [ ] 通过静态代码分析

---

## 🔧 开发环境准备

### 工具链
```bash
# 编译
cmake -B build -S . -DGCOS_PLATFORM=WIN32
cmake --build build --config Debug

# 测试
./build/Debug/test_apdu_parser.exe
./build/Debug/test_stream_loader.exe
```

### 调试工具
- **APDU模拟器**: 需要创建或使用现有的智能卡模拟器
- **日志输出**: 启用GCOS_PRINTF查看内部状态
- **断点调试**: VS Code + C/C++扩展

---

## 📚 参考资料

1. **cref源代码** (关键文件):
   - `cref/adapter/win32/t0.c` - APDU传输层
   - `cref/native/native_install.c` - LOAD状态机
   - `cref/native/native_GP.c` - GP命令处理
   - `cref/common/interpreter.c` - 执行引擎

2. **规范文档**:
   - `cos3-qw.md` - COS3规范全文
   - ISO/IEC 7816-4 - APDU格式标准
   - Global Platform Card Spec v2.3

3. **设计文档**:
   - `gcos_vm/docs/APDU_ARCHITECTURE_ADJUSTMENT.md` - 详细架构分析

---

## 🚀 快速开始

### Step 1: 创建APDU框架骨架
```bash
cd e:\views\gcos\prog\cos\gcos_vm
touch include/gcos_apdu.h src/gcos_apdu.c
touch include/gcos_stream_loader.h src/gcos_stream_loader.c
touch include/gcos_selector.h src/gcos_selector.c
```

### Step 2: 实现最小化APDU处理器
```c
// src/gcos_apdu.c - 最小实现
u16 gcos_vm_process_apdu(GCOSVM *vm, const u8 *apdu, u8 length,
                         u8 *response, u16 *resp_len) {
    if (length < 4) return 0x6700;  // SW_WRONG_LENGTH
    
    u8 ins = apdu[1];
    switch (ins) {
        case 0xE8: return handle_load_apdu(vm, apdu, length, response, resp_len);
        case 0xA4: return handle_select_apdu(vm, apdu, length, response, resp_len);
        default: return 0x6D00;  // SW_INS_NOT_SUPPORTED
    }
}
```

### Step 3: 编译并测试
```bash
cmake --build build_test --config Debug
./build_test/Debug/test_basic.exe
```

---

## ⚠️ 注意事项

1. **不要破坏现有功能**: 保留gcos_vm的C API作为内部使用，APDU层作为外部接口
2. **逐步迁移**: 先实现LOAD，再实现SELECT，最后完善其他命令
3. **充分测试**: 每个阶段都要编写对应的单元测试
4. **参考cref**: 遇到设计问题时，优先查看cref如何实现
5. **保持零动态内存**: 所有缓冲区使用静态分配或预分配池

---

## 📞 联系与支持

如有问题，请参考：
- 架构详细分析: `docs/APDU_ARCHITECTURE_ADJUSTMENT.md`
- cref源代码: `../cref/`目录
- COS3规范: `../cos3-qw.md`

---

**最后更新**: 2026-05-11  
**状态**: 规划完成，准备实施
