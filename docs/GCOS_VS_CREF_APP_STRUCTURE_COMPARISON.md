# GCOS vs Cref 应用结构对比分析

## 📊 对比概览

本文档详细对比 GCOS VM 和 Cref (JavaCard Reference Implementation) 的应用相关数据结构，分析 GCOS 是否缺少关键成员。

---

## 1️⃣ Cref AppTableEntry 结构

### 1.1 基础字段（所有版本）

根据 `cref/common/objAccess.h` 和 `cref/adapter/win32/mask32.c`：

| 偏移 | 字段名 | 类型 | 说明 |
|------|--------|------|------|
| 0 | `theApplet` | jref (2 bytes) | **Applet 对象引用** ⭐ |
| 1 | `theAID` | jref (2 bytes) | AID 对象引用 |
| 2 | `appIdentifer` | u8 (1 byte) | 应用标识符 |
| 3 | `privByte1` | u8 (1 byte) | 权限字节 1 |
| 4 | `privByte2` | u8 (1 byte) | 权限字节 2 |
| 5 | `privByte3` | u8 (1 byte) | 权限字节 3 |
| 6 | `theContext` | u16 (2 bytes) | Context ID |
| 7 | `sdID` | u8 (1 byte) | 安全域 ID |
| 8 | `status` | u8 (1 byte) | **生命周期状态** ⭐ |
| 9 | `type` | u8 (1 byte) | 应用类型 (APP/ISD/SSD) |
| 10 | `attr` | u8 (1 byte) | 属性标志 |
| 11 | `ucInstallParamD9` | u8 (1 byte) | 安装参数 |

**总大小**: `AppTableEntry_Size = 0x0C` (12 个条目索引，实际占用 ~24-30 字节)

### 1.2 GP 2.2.1 扩展字段

```c
#if (GP_VERSION==221)
#define APP_ENTRY_regService            // 注册服务
#define APP_ENTRY_uniqueGlobalService   // 唯一全局服务
#define APP_ENTRY_dwFlashQuotaSpaceC8   // Flash 配额空间
#define APP_ENTRY_dwRamQuotaSpaceC7     // RAM 配额空间
#define APP_ENTRY_usedhFlashQuotaSpace  // 已用 Flash 空间
#define APP_ENTRY_usedRamQuotaSpace     // 已用 RAM 空间
#define APP_ENTRY_dwFlashQuotaSpaceD8   // Flash 配额空间 D8
#define APP_ENTRY_dwRamQuotaSpaceD7     // RAM 配额空间 D7
#endif
```

### 1.3 CRS (Contactless Reader Support) 扩展字段

```c
#if GP_CRS
#define APP_ENTRY_gpclRegistry          // GPCL 注册信息
#define APP_ENTRY_usInstancePrio        // 实例优先级
#define APP_ENTRY_usTempInstancePrio    // 临时实例优先级
#endif
```

### 1.4 其他可选字段

```c
#define APP_ENTRY_select_security_ctrl      // 选择安全控制
#define APP_ENTRY_appInfoDisplay_flag       // 应用信息显示标志
#define APP_ENTRY_selectCurAppClrDTR_flag   // 选择当前应用清除 DTR 标志
// #define APP_ENTRY_implicitSelParam        // 隐式选择参数（注释掉）
```

---

## 2️⃣ GCOS GCOSAppInstance 结构

根据 `gcos_vm/include/gcos_vm.h`：

### 2.1 基本信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `app_aid` | GCOSAID | 应用 AID |
| `app_id` | u8 | 应用 ID (0 = ISD) |
| `module_index` | u16 | 所属模块索引 |
| `lifecycle` | GCOSAppLifecycleState | 生命周期状态 |

### 2.2 APDU 处理方法指针 ⭐

| 字段 | 类型 | 说明 |
|------|------|------|
| `process` | 函数指针 | **核心方法**：处理所有非 GP 命令 |
| `on_select` | 函数指针 | SELECT 成功后的回调 |
| `on_deselect` | 函数指针 | 取消选择时的回调 |

### 2.3 应用数据区域

| 字段 | 类型 | 说明 |
|------|------|------|
| `app_domain_data` | u8* | 应用域数据 |
| `app_domain_data_size` | u32 | 应用域数据大小 |
| `ref_domain_data` | u8* | 引用域数据 |
| `ref_domain_data_size` | u32 | 引用域数据大小 |
| `persistent_data` | u8* | 持久性数据 |
| `persistent_data_size` | u32 | 持久性数据大小 |

### 2.4 通道数据（每个通道独立）

```c
struct {
    u8 *temp_dynamic_data;      // 临时动态数据
    u32 temp_dynamic_data_size; // 临时动态数据大小
    u8 *global_data_copy;       // 模块全局数据副本
    u32 global_data_copy_size;  // 副本大小
    bool active;                // 是否激活
    bool selected;              // 是否被选择
} channel_data[MAX_CHANNELS];
```

### 2.5 状态标志

| 字段 | 类型 | 说明 |
|------|------|------|
| `current_channel` | u8 | 当前活动通道 |
| `is_selected` | bool | 是否被选中 |
| `selected_channel` | u8 | 选中的通道 |
| `installed` | bool | 是否已安装 |
| `install_time` | u32 | 安装时间戳 |

---

## 3️⃣ 缺失成员分析

### ❌ GCOS 缺少的关键字段

#### 1. **权限管理字段** (重要)

Cref 有：
- `privByte1` - 权限字节 1（包含安全域标志、特权等）
- `privByte2` - 权限字节 2
- `privByte3` - 权限字节 3

GCOS **缺少**这些字段。

**影响**：
- 无法实现细粒度的权限控制
- 无法区分 ISD/SSD/普通应用的特权级别
- 无法实现 GlobalPlatform 的安全域管理

**建议添加**：
```c
u8 privilege_byte1;  // 权限字节 1
u8 privilege_byte2;  // 权限字节 2  
u8 privilege_byte3;  // 权限字节 3
```

#### 2. **安全域 ID** (中等重要)

Cref 有：
- `sdID` - 安全域 ID（标识该应用属于哪个安全域）

GCOS **缺少**此字段。

**影响**：
- 无法实现多安全域架构
- 无法正确管理 SSD (Supplementary Security Domain)

**建议添加**：
```c
u8 security_domain_id;  // 所属安全域 ID (0xFF = ISD)
```

#### 3. **应用类型** (中等重要)

Cref 有：
- `type` - 应用类型 (TYPE_APP, TYPE_ISD, TYPE_SSD, etc.)

GCOS **缺少**明确的类型字段（虽然可以通过 app_id == 0 判断 ISD）。

**建议添加**：
```c
typedef enum {
    APP_TYPE_REGULAR = 0x00,  // 普通应用
    APP_TYPE_ISD = 0x01,      // 初始安全域
    APP_TYPE_SSD = 0x02,      // 补充安全域
    APP_TYPE_CASD = 0x04,     // CASD
} GCOSAppType;

GCOSAppType app_type;  // 应用类型
```

#### 4. **Context ID** (低重要性)

Cref 有：
- `theContext` - Context ID（用于多逻辑通道）

GCOS 使用 `app_id` 作为隐式的 Context ID，但缺少显式字段。

**建议**：可以保持现状，因为 `app_id` 已经起到类似作用。

#### 5. **安装参数** (低重要性)

Cref 有：
- `ucInstallParamD9` - 安装参数字节

GCOS **缺少**此字段。

**建议添加**：
```c
u8 install_param;  // 安装参数（来自 INSTALL 命令的 P2）
```

#### 6. **资源配额管理** (GP 2.2.1 特有)

Cref GP 2.2.1 有：
- `dwFlashQuotaSpaceC8/D8` - Flash 配额
- `dwRamQuotaSpaceC7/D7` - RAM 配额
- `usedhFlashQuotaSpace` - 已用 Flash
- `usedRamQuotaSpace` - 已用 RAM

GCOS **完全缺少**配额管理机制。

**影响**：
- 无法限制应用的资源使用
- 可能导致某个应用占用过多资源

**建议**：Phase 2 或 Phase 3 时添加配额管理。

#### 7. **隐式选择参数** (可选)

Cref 有（但被注释掉）：
- `implicitSelParam` - 隐式选择参数

GCOS **缺少**。

**影响**：无法实现上电自动选择特定应用的功能。

**可选添加**：
```c
u8 implicit_select_param;  // 隐式选择参数
```

---

## 4️⃣ GCOS 的优势设计

### ✅ GCOS 比 Cref 更好的地方

#### 1. **直接函数指针 vs Java 对象引用**

- **Cref**: 存储 `jref theApplet` → 需要查找 Class → VMT → 方法地址
- **GCOS**: 直接存储 `process()` 函数指针 → 零开销调用

**优势**：
- 更快的执行速度
- 更简单的实现
- 适合嵌入式环境

#### 2. **多通道数据隔离**

GCOS 有完整的 `channel_data[MAX_CHANNELS]` 结构，每个通道独立的数据副本。

Cref 通过 `theContext` 和全局状态管理，相对复杂。

#### 3. **模块化设计**

GCOS 有 `module_index` 字段，支持模块化加载和管理。

Cref 没有明确的模块概念。

#### 4. **时间戳跟踪**

GCOS 有 `install_time` 字段，便于审计和管理。

Cref 没有此功能。

---

## 5️⃣ 总结与建议

### 🔴 高优先级缺失（建议立即添加）

1. **权限字节** (`privilege_byte1/2/3`)
   - 原因：GlobalPlatform 规范要求
   - 影响：安全域管理、权限控制
   
2. **安全域 ID** (`security_domain_id`)
   - 原因：多安全域架构必需
   - 影响：SSD 管理

3. **应用类型** (`app_type`)
   - 原因：明确区分 ISD/SSD/普通应用
   - 影响：代码可读性和维护性

### 🟡 中优先级缺失（Phase 2 添加）

4. **安装参数** (`install_param`)
   - 原因：INSTALL 命令需要
   
5. **资源配额** (Flash/RAM quota)
   - 原因：防止资源滥用

### 🟢 低优先级缺失（可选）

6. **隐式选择参数** (`implicit_select_param`)
   - 原因：特殊应用场景需要

### ✅ GCOS 不需要添加的 Cref 字段

- `theApplet` (jref) - GCOS 用函数指针替代，更好
- `theAID` (jref) - GCOS 直接用 GCOSAID 结构，更好
- `theContext` - GCOS 用 `app_id` 替代
- `appIdentifer` - GCOS 用 `app_id` 替代
- `attr` - GCOS 可以用 `privilege_byte` 替代
- `regService/uniqueGlobalService` - GP 2.2.1 高级特性，暂不需要
- `gpclRegistry/usInstancePrio` - CRS 特性，暂不需要

---

## 6️⃣ 推荐的 GCOSAppInstance 增强版本

```c
struct GCOSAppInstance {
    /* === 基本信息 === */
    GCOSAID app_aid;                    /* 应用AID */
    u8 app_id;                          /* 应用 ID (0 = ISD) */
    u16 module_index;                   /* 所属模块索引 */
    GCOSAppLifecycleState lifecycle;    /* 生命周期状态 */
    
    /* === 新增：类型和权限 ⭐ === */
    GCOSAppType app_type;               /* 应用类型 (APP/ISD/SSD) */
    u8 security_domain_id;              /* 所属安全域 ID */
    u8 privilege_byte1;                 /* 权限字节 1 */
    u8 privilege_byte2;                 /* 权限字节 2 */
    u8 privilege_byte3;                 /* 权限字节 3 */
    u8 install_param;                   /* 安装参数 */
    
    /* === APDU 处理方法 ⭐ === */
    u16 (*process)(struct GCOSAppInstance *app, ...);
    GCOSResult (*on_select)(struct GCOSAppInstance *app);
    void (*on_deselect)(struct GCOSAppInstance *app);
    
    /* === 应用数据 === */
    u8 *app_domain_data;
    u32 app_domain_data_size;
    u8 *ref_domain_data;
    u32 ref_domain_data_size;
    u8 *persistent_data;
    u32 persistent_data_size;
    
    /* === 运行时数据 (每个通道独立) === */
    struct {
        u8 *temp_dynamic_data;
        u32 temp_dynamic_data_size;
        u8 *global_data_copy;
        u32 global_data_copy_size;
        bool active;
        bool selected;
    } channel_data[MAX_CHANNELS];
    
    u8 current_channel;
    
    /* === 状态标志 === */
    bool is_selected;
    u8 selected_channel;
    bool installed;
    u32 install_time;
};
```

---

## 7️⃣ 实施计划

### Phase 1 (当前)
- ✅ 基础应用管理框架
- ✅ SELECT 命令实现
- ✅ AID 前缀匹配

### Phase 1.5 (建议立即实施)
- [ ] 添加 `app_type` 枚举和字段
- [ ] 添加 `privilege_byte1/2/3`
- [ ] 添加 `security_domain_id`
- [ ] 更新 ISD 创建逻辑设置正确的类型和权限

### Phase 2
- [ ] 添加 `install_param`
- [ ] 实现 LOAD/INSTALL 命令
- [ ] 解析安装参数并存储

### Phase 3
- [ ] 添加资源配额管理
- [ ] 实现配额检查和限制

---

**结论**：GCOS 的核心设计（函数指针、多通道隔离）比 Cref 更适合嵌入式环境，但缺少一些 GlobalPlatform 规范要求的元数据字段（权限、类型、安全域 ID）。建议在 Phase 1.5 中添加这些字段以符合 GP 规范。
