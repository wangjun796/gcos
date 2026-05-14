# GCOS SEF Loader 与 COS3 规范符合性确认报告

## 📋 执行摘要

根据对 `cos3-qw.md`（COS3 规范）的详细分析和当前 GCOS 加载器代码的审查，**确认需要全面重构 SEF 加载器以符合 COS3 规范**。

---

## ✅ COS3 规范要求确认

### 1. 字节序要求（第 413 行）

**规范原文**：
> 链接文件和可加载文件中 i32、s16、s32、u16、u32 类型的数据项应按照小端顺序存储。

**确认**：
- ✅ **COS3 SEF 文件使用小端字节序（Little-Endian）**
- ✅ 所有多字节整数必须低位字节在前
- ✅ 测试代码已添加 `read_u16_le()` 和 `read_u32_le()` 辅助函数

---

### 2. 魔术字定义（表 10，第 443 行）

**规范要求**：

| 文件类型 | 文件类型标识符 | 描述 |
|----------|---------------|------|
| 可加载文件 | `'00736566'` | GB/T 1988编码字符"sef" |

**解析**：
- 数值：`0x00736566`
- 小端存储：`66 65 73 00`
- ASCII："sef\0"

**当前实现（❌ 错误）**：
```c
// gcos_loader.c Line 23
#define SEF_MAGIC           0x53454630  /* "SEF0" */
```

**差异**：
- ❌ 当前值：`0x53454630` ("SEF0")
- ✅ 规范值：`0x00736566` ("sef\0")

---

### 3. 文件头部结构（表 16，第 524-530 行）

**规范要求**：

| 数据项 | 数据类型 | 说明 |
|--------|----------|------|
| `sef_type` | u32 | 可加载文件类型，默认为 `'00736566'` |
| `version` | u32 | 版本号，数据结构应符合附录B的规定 |
| `sections[]` | 结构体 | 可加载文件的段数据，数据结构应符合表17的规定 |

**头部大小**：**8 字节固定头**（仅 sef_type + version）

**当前实现（❌ 错误）**：
```c
// gcos_loader.c Lines 34-44
typedef struct {
    u32 magic;              /* Magic number: 0x53454630 */
    u16 version_major;      /* Major version */
    u16 version_minor;      /* Minor version */
    u32 file_size;          /* Total file size */
    u32 section_count;      /* Number of sections */
    u32 checksum;           /* File checksum */
    u8 reserved[8];         /* Reserved for future use */
} SEFHeader;  // 28 bytes (packed)
```

**差异对比**：

| 字段 | 规范要求 | 当前实现 | 状态 |
|------|---------|---------|------|
| 魔术字 | `sef_type` (u32) = `0x00736566` | `magic` (u32) = `0x53454630` | ❌ 值错误 |
| 版本 | `version` (u32) 单字段 | `version_major` + `version_minor` (2×u16) | ❌ 结构错误 |
| 文件大小 | **无此字段** | `file_size` (u32) | ❌ 多余字段 |
| 段数量 | **无此字段** | `section_count` (u32) | ❌ 多余字段 |
| 校验和 | **无此字段** | `checksum` (u32) | ❌ 多余字段 |
| 保留区 | **无此字段** | `reserved[8]` | ❌ 多余字段 |
| **总大小** | **8 字节** | **28 字节** | ❌ **严重不符** |

---

### 4. 段头部结构（表 17，第 534-540 行）

**规范要求**：

| 数据项 | 数据类型 | 说明 |
|--------|----------|------|
| `section_id` | u8 | 段标识符，其分配规则应符合表18的规定 |
| `size` | u32 | 段内容字节数 |
| `contents[size]` | u8 | 段内容 |

**段头部大小**：**5 字节**（section_id + size）

**当前实现（❌ 错误）**：
```c
// gcos_loader.c Lines 49-55
typedef struct {
    u8 section_id;          /* Section identifier */
    u8 flags;               /* Section flags */
    u16 reserved;           /* Reserved */
    u32 offset;             /* Offset in file */
    u32 size;               /* Section size */
} SEFSectionHeader;  // 12 bytes (packed)
```

**差异对比**：

| 字段 | 规范要求 | 当前实现 | 状态 |
|------|---------|---------|------|
| 段ID | `section_id` (u8) | `section_id` (u8) | ✅ 正确 |
| 段大小 | `size` (u32, 小端) | `size` (u32) | ⚠️ 需确认小端读取 |
| 标志位 | **无此字段** | `flags` (u8) | ❌ 多余字段 |
| 保留区 | **无此字段** | `reserved` (u16) | ❌ 多余字段 |
| 偏移量 | **无此字段** | `offset` (u32) | ❌ 多余字段 |
| **总大小** | **5 字节** | **12 字节** | ❌ **严重不符** |

**关键问题**：
- ❌ 规范中段是**连续存储**的，没有 `offset` 字段
- ❌ 段按顺序紧接在头部之后，不需要偏移量
- ❌ 当前实现假设段可以分散在文件中，这与规范不符

---

### 5. 版本号格式（附录 B）

**规范要求**：

版本号采用 u32 格式，字节布局如下：

```
Byte 3 (MSB): 主版本号 (major)
Byte 2:       次版本号 (minor)
Byte 1:       修订号 (revision)
Byte 0 (LSB): 内部版本号 (internal)
```

**示例**：
- `0x01000000` = v1.0.0.0
- `0x02010305` = v2.1.3.5

**当前实现（❌ 错误）**：
```c
// 分开的大/小版本号
u16 version_major;
u16 version_minor;
```

**差异**：
- ❌ 当前使用两个独立的 u16 字段
- ✅ 规范使用单个 u32 字段，按字节编码四个版本号

---

### 6. 段标识符分配（表 18，第 548-560 行）

**规范要求**：

| 段标识符 | 段名称 | 必选/可选 |
|----------|--------|----------|
| `'01'` | 首段 | 必选 |
| `'02'` | 导入段 | 可选 |
| `'03'` | 函数段 | 必选 |
| `'04'` | 应用段 | 可选 |
| `'05'` | 全局段 | 必选 |
| `'06'` | 导出段 | 可选 |
| `'07'` | 元素段 | 可选 |
| `'08'` | 数据段 | 可选 |
| `'09'` | 代码段 | 必选 |
| `'0A'` - `'0E'` | 保留段 | 可选 |
| `'0F'` | 自定义段 | 可选 |

**当前实现**：需要检查是否支持所有必需的段类型。

---

## 🔴 需要重构的内容清单

### 高优先级（必须修复）

#### 1. 修改魔术字定义
```c
// 当前（错误）
#define SEF_MAGIC           0x53454630  /* "SEF0" */

// 修正为
#define SEF_MAGIC           0x00736566  /* "sef\0" per COS3 Table 10 */
```

#### 2. 重新定义文件头部结构
```c
// 当前（错误 - 28 字节）
typedef struct {
    u32 magic;
    u16 version_major;
    u16 version_minor;
    u32 file_size;
    u32 section_count;
    u32 checksum;
    u8 reserved[8];
} SEFHeader;

// 修正为（正确 - 8 字节）
typedef struct {
    u32 sef_type;     /* 0x00736566 per COS3 Table 10 */
    u32 version;      /* Version per Appendix B format */
} __attribute__((packed)) SEFHeader;
```

#### 3. 重新定义段头部结构
```c
// 当前（错误 - 12 字节）
typedef struct {
    u8 section_id;
    u8 flags;
    u16 reserved;
    u32 offset;
    u32 size;
} SEFSectionHeader;

// 修正为（正确 - 5 字节）
typedef struct {
    u8 section_id;    /* Section ID per COS3 Table 18 */
    u32 size;         /* Section content size (little-endian) */
} __attribute__((packed)) SEFSectionHeader;
```

#### 4. 所有多字节字段使用小端读取
```c
// 添加辅助函数（已在 test_sef_parsing.c 中实现）
static inline uint16_t read_u16_le(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline uint32_t read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

// 在解析时使用
uint32_t sef_type = read_u32_le(&data[0]);
uint32_t version = read_u32_le(&data[4]);
```

#### 5. 修改段解析逻辑
```c
// 当前逻辑（错误 - 假设有 offset 字段）
for (i = 0; i < header->section_count; i++) {
    SEFSectionHeader *sec = &sections[i];
    parse_section(data + sec->offset, sec->size);
}

// 修正逻辑（正确 - 段连续存储）
uint32_t offset = sizeof(SEFHeader);  // 从第 8 字节开始
while (offset < file_size) {
    u8 section_id = data[offset];
    u32 section_size = read_u32_le(&data[offset + 1]);
    
    parse_section(&data[offset + 5], section_size);  // 跳过 5 字节头
    
    offset += 5 + section_size;  // 移动到下一段
}
```

#### 6. 版本号解码
```c
// 当前（错误）
printf("Version: %d.%d", header->version_major, header->version_minor);

// 修正为（正确 - 按附录 B 格式）
uint32_t version = read_u32_le(&data[4]);
uint8_t ver_major = (version >> 24) & 0xFF;
uint8_t ver_minor = (version >> 16) & 0xFF;
uint8_t ver_revision = (version >> 8) & 0xFF;
uint8_t ver_internal = version & 0xFF;
printf("Version: v%d.%d.%d.%d", ver_major, ver_minor, ver_revision, ver_internal);
```

---

### 中优先级（建议优化）

#### 7. 移除不必要的字段
- ❌ 删除 `file_size`（可通过实际文件大小获取）
- ❌ 删除 `section_count`（通过遍历段计算）
- ❌ 删除 `checksum`（可在应用层实现）
- ❌ 删除 `reserved` 字段

#### 8. 添加段类型验证
```c
// 验证段标识符是否符合表 18
if (section_id < 0x01 || section_id > 0x0F) {
    return GCOS_ERROR_INVALID_SECTION_ID;
}

// 验证必需段是否存在
bool has_header = false, has_functions = false, 
     has_global = false, has_code = false;

// 解析后检查
if (!has_header || !has_functions || !has_global || !has_code) {
    return GCOS_ERROR_MISSING_REQUIRED_SECTION;
}
```

#### 9. 添加段顺序验证
```c
// 规范 7.3.3 要求段按特定顺序出现
// 首段 → 导入段 → 函数段 → 应用段 → 全局段 → 导出段 → 
// 元素段 → 数据段 → 代码段 → 自定义段

u8 expected_order[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
                       0x07, 0x08, 0x09, 0x0A};
u8 last_section_id = 0;

while (parsing) {
    if (section_id < last_section_id) {
        return GCOS_ERROR_INVALID_SECTION_ORDER;
    }
    last_section_id = section_id;
}
```

---

## 📊 影响范围评估

### 受影响的文件

1. **gcos_vm/src/gcos_loader.c**（主要重构）
   - 修改 `SEFHeader` 结构定义
   - 修改 `SEFSectionHeader` 结构定义
   - 重写 `validate_sef_header()` 函数
   - 重写 `parse_sections()` 函数
   - 添加小端读取辅助函数

2. **gcos_vm/include/gcos_vm.h**（可能需要更新）
   - 检查是否有依赖旧结构的代码

3. **gcos_vm/tests/test_sef_parsing.c**（已完成）
   - ✅ 已添加小端读取函数
   - ✅ 已使用正确的魔术字验证

4. **gcos_vm/src/gcos_persistence.c**（待检查）
   - 检查持久化模块是否依赖旧的 SEF 结构

### 向后兼容性

⚠️ **破坏性变更**：此次重构将导致：
- ❌ 无法加载之前生成的 SEF 文件（如果有的话）
- ❌ 需要使用符合 COS3 规范的编译器重新生成所有 SEF 文件

---

## ✅ 测试验证计划

### 1. 单元测试

使用 `test_sef_parsing.c` 验证：
- ✅ 文件头解析（8 字节）
- ✅ 魔术字验证（`0x00736566`）
- ✅ 版本号解码（附录 B 格式）
- ✅ 段解析（5 字节头）
- ✅ 小端读取正确性

### 2. 集成测试

创建完整的 SEF 文件测试：
```c
// 使用 COS3 附录 F.1 的真实数据
static const uint8_t cos3_sef_example[] = {
    0x66, 0x65, 0x73, 0x00,  // sef_type = 0x00736566
    0x00, 0x00, 0x00, 0x01,  // version = 0x01000000 (v1.0.0.0)
    // ... 段数据
};
```

### 3. 回归测试

确保以下功能正常工作：
- 模块加载
- 段解析
- 内存分配
- 代码执行

---

## 🎯 实施建议

### 阶段 1：结构定义修正（1-2 小时）
1. 修改 `SEFHeader` 结构（8 字节）
2. 修改 `SEFSectionHeader` 结构（5 字节）
3. 更新魔术字定义为 `0x00736566`

### 阶段 2：解析逻辑重构（2-3 小时）
1. 添加小端读取辅助函数到 `gcos_loader.c`
2. 重写文件头验证逻辑
3. 重写段解析循环（移除 offset 依赖）
4. 实现版本号解码（附录 B 格式）

### 阶段 3：测试与验证（1-2 小时）
1. 运行 `test_sef_parsing.exe` 验证基础解析
2. 使用 COS3 附录 F.1 数据进行完整测试
3. 修复发现的任何问题

### 阶段 4：集成测试（1-2 小时）
1. 测试完整的模块加载流程
2. 验证持久化功能（如果使用）
3. 执行回归测试

**总预计时间**：5-9 小时

---

## 📝 结论

### ✅ 确认事项

1. **COS3 SEF 文件确实使用小端字节序**（规范第 413 行明确规定）
2. **测试代码已正确处理小端读取**（`read_u16_le()` / `read_u32_le()`）
3. **当前 GCOS 加载器需要全面重构**以符合 COS3 规范

### 🔴 必须修复的问题

| # | 问题 | 严重程度 | 规范依据 |
|---|------|---------|---------|
| 1 | 魔术字错误（`0x53454630` vs `0x00736566`） | 🔴 严重 | 表 10, 第 443 行 |
| 2 | 头部结构过大（28 字节 vs 8 字节） | 🔴 严重 | 表 16, 第 524 行 |
| 3 | 段头部结构过大（12 字节 vs 5 字节） | 🔴 严重 | 表 17, 第 534 行 |
| 4 | 缺少小端读取机制 | 🔴 严重 | 第 413 行 |
| 5 | 版本号格式错误 | 🟡 中等 | 附录 B |

### 💡 建议

**立即开始重构工作**，按照上述实施建议分阶段进行。优先修复魔术字和结构定义，然后逐步完善解析逻辑。

---

## 📚 参考资料

- COS3 规范：`e:/views/gcos/prog/cos/cos3-qw.md`
  - 第 413 行：小端字节序要求
  - 表 10（第 437-443 行）：文件类型标识符
  - 表 16（第 524-530 行）：可加载文件数据结构
  - 表 17（第 534-540 行）：段数据结构
  - 表 18（第 548-560 行）：段标识符分配规则
  - 附录 B：版本号数据结构
  - 附录 F.1（第 3693-3703 行）：SEF 文件示例

- 当前实现：`e:/views/gcos/prog/cos/gcos_vm/src/gcos_loader.c`

- 测试代码：`e:/views/gcos/prog/cos/gcos_vm/tests/test_sef_parsing.c`
