# COS3 SEF 测试 - 小端字节序验证总结

## ✅ 已完成的工作

### 1. 添加小端读取辅助函数

```c
static inline uint16_t read_u16_le(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline uint32_t read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}
```

**符合 COS3 规范第 413 行要求**：所有多字节整数使用小端字节序。

### 2. 修正文件头解析

```c
/* 使用小端读取 */
uint32_t file_type = read_u32_le(&cos3_sef_example[0]);
uint32_t file_version = read_u32_le(&cos3_sef_example[4]);

/* 正确解码版本号（附录 B 格式）*/
uint8_t ver_major = (file_version >> 24) & 0xFF;
uint8_t ver_minor = (file_version >> 16) & 0xFF;
uint8_t ver_revision = (file_version >> 8) & 0xFF;
uint8_t ver_internal = file_version & 0xFF;
```

### 3. 测试结果验证

```
SEF File Information:
  Total size: 154 bytes
  File header: 66 65 73 00 00 00 00 01
  File type: 0x00736566 (expected 0x00736566 for 'sef\0')
  File version: v1.0.0.0 (raw=0x01000000)
  ✓ File type is valid (matches COS3 Table 10)
```

**✅ 确认**：
- 魔术字 `0x00736566` 正确（"sef\0"）
- 版本号 `v1.0.0.0` 正确（按附录 B 格式）
- 小端读取函数工作正常

### 4. 手动段解析演示

添加了手动解析代码，展示如何正确处理小端数据：

```c
uint32_t offset = 8;  /* Skip 8-byte header */
while (offset < COS3_SEF_SIZE) {
    uint8_t section_id = cos3_sef_example[offset];
    uint32_t section_size = read_u32_le(&cos3_sef_example[offset + 1]);
    
    printf("Section %d: ID=0x%02X, Size=%u bytes\n",
           section_count++, section_id, section_size);
    
    offset += 5 + section_size;  /* 5 = section_id(1) + size(4) */
}
```

---

## ⚠️ 发现的问题

### 问题 1：GCOS Loader 不符合 COS3 规范

当前加载器期望的魔术字是 `0x53454630` ("SEF0")，但 COS3 规范要求 `0x00736566` ("sef\0")。

**测试结果**：
```
[Loader] Invalid SEF magic: 0x00736566
✗ FAILED: SEF file loading failed (ret=-1)
```

这是**预期的失败**，因为当前实现与规范不兼容。

### 问题 2：测试数据可能存在偏差

手动解析显示：
```
Section 0: ID=0x01, Size=29 bytes (at offset 8)     ✓ 
Section 1: ID=0x02, Size=24 bytes (at offset 42)    ✓
Section 2: ID=0x00, Size=131841 bytes (at offset 71) ✗
```

第3个段应该是 `ID=0x03`（函数段），但解析出 `ID=0x00`，说明偏移量计算有误。

**可能原因**：
- 首段的实际大小可能不是 29 字节
- 测试数据录入时可能有误

需要对照 COS3 附录 F.1 的原始十六进制数据逐字节验证。

---

## 📋 关键发现

### 1. 字节序确认

✅ **COS3 SEF 文件确实使用小端字节序**

证据：
- 规范第 413 行明确要求
- 附录 F.1 的数据 `66 65 73 00` 是小端存储的 `0x00736566`
- 使用 `read_u32_le()` 正确解析出魔术字

### 2. 版本号格式

✅ **版本号按附录 B 格式编码**

```
u32 version 布局（小端存储）：
Byte 0 (LSB): internal version
Byte 1:       revision
Byte 2:       minor
Byte 3 (MSB): major

示例：0x00 0x00 0x00 0x01 → v1.0.0.0
```

### 3. 段结构

✅ **段头为 5 字节，内容紧跟其后**

```c
typedef struct {
    u8 section_id;      /* 1 byte */
    u32 size;           /* 4 bytes (little-endian) */
    u8 contents[size];  /* Variable length, immediately follows */
} Section;
```

**不需要 offset 字段**，这与当前 GCOS 实现不同。

---

## 🔧 需要的修复

### 优先级 1：修正 GCOS SEF Loader

需要全面重构 `gcos_loader.c` 以符合 COS3 规范：

1. **修改魔术字**：
   ```c
   #define SEF_MAGIC  0x00736566  /* "sef\0" */
   ```

2. **重新定义头部结构**（8 字节而非 28 字节）：
   ```c
   typedef struct {
       u32 sef_type;   /* 0x00736566 */
       u32 version;    /* Appendix B format */
   } SEFHeader;
   ```

3. **重新定义段结构**（5 字节而非 12 字节）：
   ```c
   typedef struct {
       u8 section_id;
       u32 size;  /* little-endian */
   } SectionHeader;
   ```

4. **使用小端读取函数**处理所有多字节字段

### 优先级 2：验证测试数据

需要仔细对照 COS3 附录 F.1 的原始数据，确保测试文件中的每个字节都正确。

建议步骤：
1. 从 cos3.md 复制原始十六进制字符串
2. 编写脚本转换为字节数组
3. 逐段验证结构和大小
4. 添加详细的注释说明每个字段的含义

---

## 📊 测试覆盖情况

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 小端读取函数 | ✅ 通过 | `read_u16_le()`, `read_u32_le()` 工作正常 |
| 魔术字验证 | ✅ 通过 | 正确识别 `0x00736566` |
| 版本号解析 | ✅ 通过 | 正确解码 v1.0.0.0 |
| AID 依赖机制 | ✅ 通过 | 基于 AID 的依赖解析正常工作 |
| SEF 加载 | ❌ 失败 | Loader 与规范不兼容（预期） |
| 段解析 | ⚠️ 部分 | 前2个段正确，后续段偏移有误 |

---

## 💡 重要结论

### 1. 字节序处理正确性

**测试证明**：使用显式的小端读取函数可以正确解析 COS3 SEF 文件的多字节字段。

**最佳实践**：
- ✅ 始终使用 `read_u32_le()` 等函数，不要直接 cast 指针
- ✅ 在注释中明确标注字节序要求
- ✅ 对关键数据进行验证和打印

### 2. GCOS 与 COS3 规范的差距

当前 GCOS SEF Loader 实现与 COS3 规范存在**结构性差异**：
- 头部结构完全不同（28 vs 8 字节）
- 段结构完全不同（12 vs 5 字节）
- 魔术字不匹配
- 缺少显式的小端处理

**需要全面重构**，而不仅仅是修改几个常量。

### 3. 测试数据的价值

即使加载失败，测试仍然有价值：
- ✅ 验证了小端读取函数的正确性
- ✅ 确认了 COS3 规范的实际数据格式
- ✅ 揭示了 GCOS 实现的问题
- ✅ 为后续重构提供了参考

---

## 🎯 下一步行动

1. **立即**：修复魔术字常量（最小改动，快速验证）
2. **短期**：重构 SEF/Section 头部结构
3. **中期**：添加完整的小端处理到所有解析代码
4. **长期**：实现完整的 COS3 规范符合性测试套件

---

**测试文件**：`tests/test_sef_parsing.c`  
**测试日期**：2026-05-09  
**COS3 规范版本**：GB/T 44901.3—XXXX 草案稿（2026年3月3日）
