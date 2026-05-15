# Cref GRT Entry 结构详细解析

## 📋 核心问题

**用户疑问：** cref 中 GRT entry 存放了 packageID 吗？`removeReferencesFromPackage()` 函数中的 `OBM_SIZE` 是什么？

**答案：** ✅ **是的！** Cref 的 GRT entry **确实存放了 packageID**，它是 entry 的最后一个字节。

---

## 🔍 GRT Entry 完整结构

### 1. 32位模式（BIT16 未定义）

```c
// memory.h 中的定义
#ifdef BIT16
    #define GRT_SIZE  3   // 16位地址模式
    #define OBM_SIZE  2   // Object Base Memory size (16位指针)
#else
    #define GRT_SIZE  5   // 32位地址模式 ← 常用
    #define OBM_SIZE  4   // Object Base Memory size (32位指针)
#endif
```

**GRT Entry 布局（32位模式，5字节）：**

```
┌─────────────────────────────────────────┐
│         GRT Entry (5 bytes)             │
├──────────┬──────────┬──────────┬────────┤
│ Byte 0   │ Byte 1   │ Byte 2   │ Byte 3 │ Byte 4
├──────────┼──────────┼──────────┼────────┼────────┤
│ Address  │ Address  │ Address  │Address │PackageID
│ [31:24]  │ [23:16]  │ [15:8]   │ [7:0]  │
└──────────┴──────────┴──────────┴────────┴────────┘
     ↑                                            ↑
  OBM_SIZE = 4 bytes                      最后1字节
  (32位物理地址)                        (packageID)
```

**内存布局示意：**

```
Offset:  0        1        2        3        4
         ┌────────┬────────┬────────┬────────┬──────┐
         │  A[3]  │  A[2]  │  A[1]  │  A[0]  │ PkgID│
         └────────┴────────┴────────┴────────┴──────┘
         ←──── OBM_SIZE = 4 bytes ────→← 1 byte →
         ←──────── GRT_SIZE = 5 bytes ──────────→
```

---

### 2. 16位模式（BIT16 定义）

```
┌─────────────────────────────────┐
│     GRT Entry (3 bytes)         │
├──────────┬──────────┬───────────┤
│ Byte 0   │ Byte 1   │ Byte 2    │
├──────────┼──────────┼───────────┤
│ Address  │ Address  │ PackageID │
│ [15:8]   │ [7:0]    │           │
└──────────┴──────────┴───────────┘
     ↑                              ↑
  OBM_SIZE = 2 bytes          最后1字节
  (16位物理地址)              (packageID)
```

---

## 💡 代码详解

### 1. 添加引用时写入 packageID

```c
jref addReference(memref address, u8 packageID) {
    // ... 查找空闲槽位 ...
    
    ref = getGrtRef(key);  // 获取 GRT entry 的地址
    
    // 准备要写入的数据（5字节）
    u8 tempGRTArray[GRT_SIZE];  // tempGRTArray[5]
    
    // 写入 32 位地址（大端序）
    tempGRTArray[0] = getFourthLastByte(address);   // Byte 0: Address[31:24]
    tempGRTArray[1] = getThirdLastByte(address);    // Byte 1: Address[23:16]
    tempGRTArray[2] = getSecondLastByte(address);   // Byte 2: Address[15:8]
    tempGRTArray[3] = getLastByte(address);         // Byte 3: Address[7:0]
    
    // ✅ 写入 packageID（最后一个字节）
    tempGRTArray[4] = packageID;                     // Byte 4: PackageID
    
    // 写入 EEPROM
    E2P_writeArrayNoAtomic(ref, tempGRTArray, GRT_SIZE);
    
    return (key | 0x8000);
}
```

**示例：**
```
假设：
  address = 0x00123456
  packageID = 0x05

写入的 5 字节：
  tempGRTArray[0] = 0x00  // Address byte 3
  tempGRTArray[1] = 0x12  // Address byte 2
  tempGRTArray[2] = 0x34  // Address byte 1
  tempGRTArray[3] = 0x56  // Address byte 0
  tempGRTArray[4] = 0x05  // PackageID

EEPROM 中的布局：
  Offset 0: 0x00
  Offset 1: 0x12
  Offset 2: 0x34
  Offset 3: 0x56
  Offset 4: 0x05  ← packageID 在这里！
```

---

### 2. 读取 packageID

```c
u8 getReferencePackageID(jref referenceID) {
    memref ref = getGrtRef(referenceID);  // 获取 GRT entry 地址
    
    if (ref == REF_NULL) {
        throw_error(0x6a88);
    }
    
    // ✅ 读取 packageID（地址 + OBM_SIZE）
    // OBM_SIZE = 4（32位模式），所以 ref+4 指向第5个字节
    return OBJHEAD_read_u8(ref + OBM_SIZE);
}
```

**图解：**
```
ref 指向 GRT entry 的起始地址

ref:     ┌────────┬────────┬────────┬────────┬──────┐
         │  A[3]  │  A[2]  │  A[1]  │  A[0]  │ PkgID│
         └────────┴────────┴────────┴────────┴──────┘
         ↑                                        ↑
      ref (offset 0)                         ref + OBM_SIZE
                                             (offset 4)
                                             
OBJHEAD_read_u8(ref + OBM_SIZE) 读取的就是 packageID！
```

---

### 3. 读取地址

```c
memref getReferenceAddress(jref referenceID) {
    memref ref = getGrtRef(referenceID);
    
    if (ref == REF_NULL) {
        throw_error(0x6a88);
    }
    
    // ✅ 读取 32 位地址（前 4 个字节）
    return OBJHEAD_read_u32_By_Byte(ref);
}
```

**图解：**
```
ref:     ┌────────┬────────┬────────┬────────┬──────┐
         │  A[3]  │  A[2]  │  A[1]  │  A[0]  │ PkgID│
         └────────┴────────┴────────┴────────┴──────┘
         ←──── OBJHEAD_read_u32_By_Byte(ref) ────→
         (读取前4字节，组合成32位地址)
```

---

### 4. 删除引用（批量软删除）

这是你问的核心函数：

```c
void removeReferencesFromPackage(u8 packageID) {
    boolean grtTransactionStarted = 0;
    
    // 开始事务
    if (!app_get_transaction_depth()) {
        app_begin_transaction();
        grtTransactionStarted = 1;
    }
    
    // ✅ 遍历所有扩展表条目
    for (u16 i = 0; i < g_GRT_EXT_Count; i++) {
        // 计算 packageID 字段的地址
        // getGrtTableRef(i) 返回第 i 个 GRT entry 的起始地址
        // + OBM_SIZE 跳过前面的地址字段，指向 packageID
        memref ref = getGrtTableRef(i) + OBM_SIZE;
        
        // ✅ 检查是否属于要删除的包
        if (E2P_read_u8(ref) == packageID) {
            // ✅ 标记为无效（软删除）
            // 将 packageID 设置为 0xFF
            storeByte(ref, (jbyte)0xFF);
        }
        
        // 定期提交事务（避免缓冲区溢出）
        if ((i != 0) && (get_unused_commit_capacity() <= EEPROM_PAGE * 2)) {
            app_commit_transaction();
            app_begin_transaction();
        }
    }
    
    // 提交事务
    if (grtTransactionStarted) {
        app_commit_transaction();
    }
}
```

**详细图解：**

```
假设要删除 packageID = 0x05 的所有引用

GRT 扩展表：
┌─────────────────────────────────────────────────┐
│ Entry 0                                          │
│ ┌────┬────┬────┬────┬────┐                      │
│ │0x00│0x10│0x20│0x30│0x03│  ← packageID=0x03  │
│ └────┴────┴────┴────┴────┘                      │
│   ↑                    ↑                         │
│getGrtTableRef(0)    +OBM_SIZE                   │
│                     (指向这里)                    │
├─────────────────────────────────────────────────┤
│ Entry 1                                          │
│ ┌────┬────┬────┬────┬────┐                      │
│ │0x00│0x20│0x40│0x60│0x05│  ← packageID=0x05  │
│ └────┴────┴────┴────┴────┘                      │
│   ↑                    ↑                         │
│getGrtTableRef(1)    +OBM_SIZE                   │
│                     (指向这里) → 匹配！标记为0xFF│
├─────────────────────────────────────────────────┤
│ Entry 2                                          │
│ ┌────┬────┬────┬────┬────┐                      │
│ │0x00│0x30│0x60│0x90│0x05│  ← packageID=0x05  │
│ └────┴────┴────┴────┴────┘                      │
│   ↑                    ↑                         │
│getGrtTableRef(2)    +OBM_SIZE                   │
│                     (指向这里) → 匹配！标记为0xFF│
├─────────────────────────────────────────────────┤
│ Entry 3                                          │
│ ┌────┬────┬────┬────┬────┐                      │
│ │0x00│0x40│0x80│0xC0│0x07│  ← packageID=0x07  │
│ └────┴────┴────┴────┴────┘                      │
│   ↑                    ↑                         │
│getGrtTableRef(3)    +OBM_SIZE                   │
│                     (指向这里)                    │
└─────────────────────────────────────────────────┘

执行后：
Entry 1: packageID 从 0x05 → 0xFF（无效）
Entry 2: packageID 从 0x05 → 0xFF（无效）
Entry 0 和 3: 不变（不属于 package 0x05）
```

---

## 🎯 关键概念解释

### 1. OBM_SIZE 是什么？

**OBM** = **Object Base Memory**

```c
#ifdef BIT16
    #define OBM_SIZE  2   // 16位指针大小
#else
    #define OBM_SIZE  4   // 32位指针大小
#endif
```

**含义：**
- OBM_SIZE 是**地址字段的大小**
- 在 32 位模式下，地址占 4 字节
- 在 16 位模式下，地址占 2 字节

**用途：**
- `ref + OBM_SIZE` 跳过地址字段，指向 packageID
- 用于定位 GRT entry 中的 packageID 字段

---

### 2. GRT_SIZE 是什么？

```c
#ifdef BIT16
    #define GRT_SIZE  3   // 16位模式：2字节地址 + 1字节packageID
#else
    #define GRT_SIZE  5   // 32位模式：4字节地址 + 1字节packageID
#endif
```

**含义：**
- GRT_SIZE 是**整个 GRT entry 的大小**
- GRT_SIZE = OBM_SIZE + 1（1字节 packageID）

---

### 3. getGrtTableRef(i) 是什么？

```c
memref getGrtTableRef(u16 index) {
    // 返回第 index 个 GRT entry 的起始地址
    // 这个地址指向 entry 的第一个字节（地址字段的开始）
}
```

**返回值：**
- 指向 GRT entry 的**起始地址**
- 从这个地址开始，前 OBM_SIZE 字节是物理地址
- 第 OBM_SIZE 字节是 packageID

---

### 4. 为什么用 `ref + OBM_SIZE`？

```c
memref ref = getGrtTableRef(i);      // ref 指向 entry 起始
memref pkg_ref = ref + OBM_SIZE;     // pkg_ref 指向 packageID
```

**原因：**
```
ref 指向：  ┌────────┬────────┬────────┬────────┬──────┐
            │  A[3]  │  A[2]  │  A[1]  │  A[0]  │ PkgID│
            └────────┴────────┴────────┴────────┴──────┘
            ↑                                        ↑
         ref (offset 0)                          ref + 4
                                                 (= ref + OBM_SIZE)
                                                 
所以要访问 packageID，需要偏移 OBM_SIZE 字节！
```

---

## 📊 完整示例

### 示例 1：创建和删除引用

```c
// 1. 创建引用
memref addr1 = 0x00123456;
u8 pkg_id = 0x05;
jref ref1 = addReference(addr1, pkg_id);
// → GRT Entry 0: [0x00, 0x12, 0x34, 0x56, 0x05]

memref addr2 = 0x00234567;
jref ref2 = addReference(addr2, pkg_id);
// → GRT Entry 1: [0x00, 0x23, 0x45, 0x67, 0x05]

memref addr3 = 0x00345678;
u8 pkg_id2 = 0x07;
jref ref3 = addReference(addr3, pkg_id2);
// → GRT Entry 2: [0x00, 0x34, 0x56, 0x78, 0x07]

// 2. 删除 package 0x05 的所有引用
removeReferencesFromPackage(0x05);

// 结果：
// GRT Entry 0: [0x00, 0x12, 0x34, 0x56, 0xFF]  ← 标记为无效
// GRT Entry 1: [0x00, 0x23, 0x45, 0x67, 0xFF]  ← 标记为无效
// GRT Entry 2: [0x00, 0x34, 0x56, 0x78, 0x07]  ← 不变

// 3. 验证
u8 pkg1 = getReferencePackageID(ref1);  // 返回 0xFF（无效）
u8 pkg2 = getReferencePackageID(ref2);  // 返回 0xFF（无效）
u8 pkg3 = getReferencePackageID(ref3);  // 返回 0x07（仍然有效）
```

---

### 示例 2：槽位重用

```c
// 1. 初始状态
addReference(0x00111111, 0x01);  // Entry 0: [0x00, 0x11, 0x11, 0x11, 0x01]
addReference(0x00222222, 0x02);  // Entry 1: [0x00, 0x22, 0x22, 0x22, 0x02]

// 2. 删除 package 0x01
removeReferencesFromPackage(0x01);
// Entry 0: [0x00, 0x11, 0x11, 0x11, 0xFF]  ← 无效
// Entry 1: [0x00, 0x22, 0x22, 0x22, 0x02]  ← 有效

// 3. 添加新引用（自动重用 Entry 0）
addReference(0x00333333, 0x03);
// → 找到 Entry 0（packageID == 0xFF），重用
// Entry 0: [0x00, 0x33, 0x33, 0x33, 0x03]  ← 重用！
// Entry 1: [0x00, 0x22, 0x22, 0x22, 0x02]  ← 不变
```

---

## 🔑 总结

### 核心要点

1. **✅ GRT entry 确实存放了 packageID**
   - 位置：entry 的**最后一个字节**
   - 偏移：`ref + OBM_SIZE`

2. **GRT Entry 结构（32位模式）**
   ```
   [Byte 0-3: 32位地址] [Byte 4: packageID]
   ←── OBM_SIZE=4 ──→ ← 1 byte →
   ←──── GRT_SIZE=5 ─────→
   ```

3. **无效标记**
   - `packageID = 0xFF` 表示条目已删除/未使用
   - `packageID = 0x00 ~ 0xFE` 表示有效条目

4. **批量回收**
   - `removeReferencesFromPackage(pkg_id)` 遍历所有 entry
   - 将匹配的 entry 的 packageID 设置为 0xFF
   - 下次 `addReference()` 会自动重用这些槽位

5. **OBM_SIZE 的作用**
   - 表示地址字段的大小（4字节或2字节）
   - 用于定位 packageID 字段：`ref + OBM_SIZE`

---

### 与 GCOS 的对比

| 特性 | Cref GRT | 当前 GCOS | 优化后 GCOS |
|------|----------|-----------|------------|
| 每条目大小 | 5 B | 12 B | 5 B |
| 地址字段 | 4 B | 4 B | 3 B (24位) |
| ID字段 | 1 B (packageID) | 1 B (module_id) + 2 B (symbol_index) | 1 B (module_id) |
| 无效标记 | 0xFF | is_valid bool | 0xFF |
| 回收机制 | 批量软删除 | ❌ 无 | 批量软删除 |
| 槽位重用 | ✅ 自动 | ❌ 无 | ✅ 自动 |

**结论：** GCOS 应该采用 cref 的设计，简化到 5 字节/条目，实现批量回收和槽位重用。

---

**文档版本：** 1.0.0  
**最后更新：** 2026-05-12  
**作者：** GCOS VM Development Team
