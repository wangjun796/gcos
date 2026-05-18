# GCOS Import 结构修正说明

## ❌ 原始设计问题

最初参考 cref 设计了 `GCOSImportInfo` 结构,使用了分离的 major/minor 版本号:

```c
// 错误的设计 (过度借鉴 cref)
typedef struct {
    GCOSAID package_aid;
    u8 minor_version;               // ❌ 不需要
    u8 major_version;               // ❌ 不需要
    bool resolved;
} GCOSImportInfo;
```

**问题**:
1. **不符合 COS3 规范** - COS3 使用 u32 完整版本号
2. **过度复杂化** - GCOS 是 C 语言实现,不需要 Java 的对象模型
3. **不必要的拆分** - 版本号比较可以直接用 u32

---

## ✅ 修正后的设计

根据 **COS3 规范表 22 (IMPORT_MODULE_ITEMS)** 和 **附录 B (版本号数据结构)**:

```c
/**
 * @brief Import dependency information (based on COS3 Specification Table 22)
 * 
 * Corresponds to IMPORT_MODULE_ITEMS in SEF file import section.
 */
typedef struct {
    u32 module_version;               /* Required module version (u32 format per Appendix B) */
    GCOSAID module_aid;               /* Dependency module AID */
    bool resolved;                    /* Whether dependency is resolved */
    u8 resolved_module_id;            /* Resolved module ID (0xFF if not resolved) */
} GCOSImportInfo;
```

### 关键改进

1. **使用 u32 版本号** - 符合 COS3 附录B的版本格式
   - 最高字节: 主版本号
   - 次高字节: 次版本号
   - 次低字节: 修订号
   - 最低字节: 内部版本号

2. **添加 resolved_module_id** - 便于快速查找已解析的模块
   - 加载时填充为实际的 module_id
   - 未解析时为 0xFF

3. **简化版本比较** - 直接比较 u32 值即可
   ```c
   if (loaded_module->version >= required_version) {
       // 版本兼容
   }
   ```

---

## 📊 COS3 vs Cref 版本管理对比

| 特性 | COS3 规范 | Cref (JavaCard) | GCOS 实现 |
|------|----------|----------------|----------|
| 版本号类型 | u32 (4字节) | u8 major + u8 minor | u32 (遵循 COS3) |
| 版本格式 | major.minor.revision.internal | major.minor | major.minor.revision.internal |
| 版本比较 | 直接数值比较 | 分别比较 major/minor | 直接数值比较 |
| 存储方式 | 单一 u32 字段 | 分离的两个 u8 字段 | 单一 u32 字段 |

---

## 🔧 相关结构同步修正

### 1. GCOSModule 结构

```c
struct GCOSModule {
    u8 module_id;
    GCOSAID module_aid;
    GCOSModuleType type;
    
    // ✅ 修正: 使用 u32 版本号
    u32 version;                    /* Module version (major.minor.revision.internal) */
    
    GCOSModuleState state;
    u8 security_domain_id;
    // ...
};
```

### 2. GCOSLoadContext 结构

```c
typedef struct {
    GCOSLoadState state;
    u8 target_module_id;
    GCOSAID package_aid;
    
    // ✅ 修正: 使用 u32 版本号
    u32 package_version;            /* Package version (u32 format per COS3 Appendix B) */
    
    u8 sd_id;
    // ...
} GCOSLoadContext;
```

---

## 💡 设计原则

### 为什么不用 cref 的 major/minor 分离?

1. **语言差异**:
   - Cref 是 Java VM,需要对象模型支持
   - GCOS 是纯 C 实现,可以更简洁

2. **规范差异**:
   - JavaCard 规范定义 major/minor 分离
   - COS3 规范定义 u32 完整版本号

3. **实用性**:
   - u32 版本号更容易序列化和传输
   - 版本比较更简单 (直接数值比较)
   - 符合 COS3 SEF 文件格式

4. **兼容性**:
   - 如果需要提取 major/minor,可以通过位运算:
     ```c
     #define VERSION_MAJOR(v)  (((v) >> 24) & 0xFF)
     #define VERSION_MINOR(v)  (((v) >> 16) & 0xFF)
     #define VERSION_REVISION(v) (((v) >> 8) & 0xFF)
     #define VERSION_INTERNAL(v) ((v) & 0xFF)
     ```

---

## ✅ 验证结果

编译和测试均通过:

```bash
$ cmake --build build --target test_app_metadata
$ ./build/Debug/test_app_metadata.exe

========================================
  All metadata tests completed!
========================================
```

---

## 📝 总结

**核心教训**: 
- ❌ 不要盲目照搬其他架构的设计
- ✅ 应该严格遵循项目规范 (COS3)
- ✅ 保持设计简洁,避免不必要的复杂性

**修正要点**:
1. 版本号统一使用 u32 格式
2. 遵循 COS3 规范的 IMPORT_MODULE_ITEMS 结构
3. 添加 resolved_module_id 提高运行时效率
4. 所有相关结构同步更新 (GCOSModule, GCOSLoadContext)

---

**修正日期**: 2026-05-09  
**参考文档**: cos3-qw.md 表22、附录B  
**状态**: 已完成 ✅
