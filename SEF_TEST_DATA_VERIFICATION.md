# COS3 SEF 测试数据验证与修正

## 问题分析

手动解析显示第3个段解析错误：
```
Section 0: ID=0x01, Size=29 bytes (at offset 8)     ✓ 正确
Section 1: ID=0x02, Size=24 bytes (at offset 42)    ✓ 正确  
Section 2: ID=0x00, Size=131841 bytes (at offset 71) ✗ 错误！
```

**预期应该是**：
```
Section 2: ID=0x03, Size=2 bytes (函数段)
```

## 根本原因

当前测试数据中，**首段（Section 1）的大小或内容不正确**，导致偏移量计算错误。

### 正确的首段结构（COS3 规范表 19）

首段应包含：
1. sef_version (u32) = 4 bytes
2. sef_aid_length (u8) = 1 byte
3. sef_aid[sef_aid_length] = variable
4. sef_len (u32) = 4 bytes
5. import_module_count (u8) = 1 byte
6. import_function_count (u16) = 2 bytes
7. app_num (u8) = 1 byte
8. sec_func_len (u16) = 2 bytes
9. sec_elem_len (u16) = 2 bytes
10. sec_data_len (u16) = 2 bytes
11. sec_code_len (u32) = 4 bytes

**总计**：4 + 1 + 5 + 4 + 1 + 2 + 1 + 2 + 2 + 2 + 4 = **28 bytes**

但当前数据标注的是 29 bytes，多了一个字节！

## 修正方案

需要仔细对照 COS3 附录 F.1 的原始十六进制数据，逐字节验证。

原始数据（cos3.md 第 3697 行）：
```
011D000000 000000010511223344559800000001010002000C000000090017000000
```

分解：
- `01` = section_id
- `1D000000` = size (小端) = 0x0000001D = 29 bytes ❓

等等，规范说 size 是 29，但我计算是 28。让我重新检查...

实际上，可能还有一个字段我没考虑到。需要查看规范的完整定义。
