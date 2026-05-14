# GCOS 应用管理增强实施总结

## ✅ 已完成的工作

### 1. 添加缺失的关键字段

根据 Cref 的 AppTableEntry 结构，在 `GCOSAppInstance` 中添加了以下字段：

#### 新增字段列表

| 字段名 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `app_type` | `GCOSAppType` | 应用类型枚举 | APP_TYPE_REGULAR |
| `security_domain_id` | `u8` | 所属安全域 ID | 0xFF (ISD) |
| `privilege_byte1` | `u8` | 权限字节 1 | 0x00 |
| `privilege_byte2` | `u8` | 权限字节 2 | 0x00 |
| `privilege_byte3` | `u8` | 权限字节 3 | 0x00 |
| `install_param` | `u8` | 安装参数（来自 INSTALL P2） | 0x00 |

#### 新增枚举类型

```c
typedef enum {
    APP_TYPE_REGULAR = 0x00,        // 普通应用
    APP_TYPE_ISD = 0x01,            // 初始安全域
    APP_TYPE_SSD = 0x02,            // 补充安全域
    APP_TYPE_CASD = 0x04,           // 可认证安全域
    APP_TYPE_FCSD = 0x05,           // 最终卡安全域
} GCOSAppType;
```

---

### 2. 添加 install 方法支持

在 `GCOSAppInstance` 中添加了 `on_install` 回调函数指针：

```c
/**
 * @brief 应用的 install() 方法（可选）
 * 
 * 在应用创建时调用，用于初始化应用实例
 * 类似于 cref 中的 Applet.install(byte[], byte, byte)
 * 
 * @param app 应用实例指针
 * @param install_data 安装数据（来自 INSTALL 命令的数据域）
 * @param install_data_len 安装数据长度
 * @return GCOS_SUCCESS 成功，其他表示失败
 */
GCOSResult (*on_install)(struct GCOSAppInstance *app,
                         const u8 *install_data,
                         u16 install_data_len);
```

---

### 3. 扩展注册 API

创建了新的 `app_register_ex()` 函数，支持完整的参数配置：

```c
GCOSResult app_register_ex(GCOSVM *vm,
                           const GCOSAID *app_aid,
                           u16 (*process_func)(...),
                           GCOSResult (*on_select)(...),
                           void (*on_deselect)(...),
                           GCOSResult (*on_install)(...),  // ⭐ 新增
                           u16 module_index,
                           GCOSAppType app_type,           // ⭐ 新增
                           u8 security_domain_id,          // ⭐ 新增
                           u8 privilege_byte1,             // ⭐ 新增
                           u8 *app_id);
```

保留了原有的 `app_register()` 作为简化版本，内部调用 `app_register_ex()` 使用默认值。

---

### 4. ISD 初始化增强

在 `create_isd_application()` 中正确设置 ISD 的元数据：

```c
// ⭐ Set ISD type and privileges
isd->app_type = APP_TYPE_ISD;
isd->security_domain_id = APP_FIRST;  // ISD is its own security domain
isd->privilege_byte1 = 0xFF;  // ISD has all privileges
isd->privilege_byte2 = 0xFF;
isd->privilege_byte3 = 0xFF;
isd->install_param = 0x00;
```

---

## 📊 测试结果

### Test 1: ISD 元数据验证 ✅

```
[TEST] ISD Type: ISD ✓ (expected: APP_TYPE_ISD = 0x01, actual: 0x01)
[TEST] ISD Security Domain ID: Correct ✓ (expected: 0x00, actual: 0x00)
[TEST] ISD Privileges: 0xFF 0xFF 0xFF ✓ (All privileges)
```

### Test 2: 扩展 API 注册应用 ✅

```
[TEST] Application registered. ID: 1
[TEST] App Type: REGULAR ✓ (expected: 0x00, actual: 0x00)
[TEST] Security Domain ID: ISD ✓ (expected: 0x00, actual: 0x00)
[TEST] Privilege Byte 1: 0x10 ✓
[TEST] Install Callback: Set ✓
```

### Test 3: Install 回调调用 ✅

```
[TestApp] install() called with 1 bytes of data
[TestApp] Install param set to: 0x42
[TEST] ✓ Install callback executed successfully
[TEST] Install param stored: 0x42 ✓
```

### Test 4: 简单 vs 扩展注册对比 ✅

```
[TEST] Simple registration defaults:
[TEST]   Type: 0x00 (should be APP_TYPE_REGULAR = 0x00) ✓
[TEST]   Security Domain: 0xFF (should be 0xFF) ✓
[TEST]   Privilege Byte 1: 0x00 (should be 0x00) ✓
[TEST]   Install Callback: NULL ✓
```

---

## 🔧 修改的文件清单

### 头文件
1. **gcos_vm/include/gcos_vm.h**
   - 添加 `GCOSAppType` 枚举
   - 在 `GCOSAppInstance` 中添加 6 个新字段
   - 添加 `on_install` 方法指针

2. **gcos_vm/include/gcos_app_manager.h**
   - 添加 `app_register_ex()` 函数声明

### 源文件
3. **gcos_vm/src/gcos_app_manager.c**
   - 实现 `app_register_ex()` 函数
   - 修改 `app_register()` 调用 `app_register_ex()`
   - 更新 `create_isd_application()` 设置 ISD 元数据

### 测试文件
4. **gcos_vm/tests/test_app_metadata.c** (新建)
   - 验证 ISD 元数据
   - 测试扩展注册 API
   - 测试 install 回调
   - 对比简单和扩展注册

5. **gcos_vm/CMakeLists.txt**
   - 添加 `test_app_metadata` 目标

---

## 📝 设计决策说明

### 为什么保留 app_register()？

为了向后兼容，保持简单的 API 供不需要高级功能的场景使用。

### 为什么 install_param 是单个字节？

参考 cref 的实现，INSTALL 命令的 P2 参数通常是单字节的安装选项标志。如果需要更复杂的安装参数，可以通过 `install_data` 数组传递。

### 权限字节的使用

- **privilege_byte1**: GlobalPlatform 定义的特权位（如卡锁定、全局删除等）
- **privilege_byte2/3**: 预留用于扩展特权或自定义特权

目前设置为全 0（无特权），ISD 设置为全 0xFF（所有特权）。

---

## 🚀 后续工作建议

### Phase 2: INSTALL 命令实现

现在有了 `on_install` 回调，可以实现完整的 INSTALL APDU 处理：

1. 解析 INSTALL 命令的 TLV 数据
2. 从已加载的模块创建应用实例
3. 调用 `app_register_ex()` 注册应用
4. 调用应用的 `on_install()` 回调进行初始化
5. 设置生命周期状态为 INSTALLED → SELECTABLE

### Phase 3: 权限检查

在 GP 命令处理中添加权限验证：

```c
// 示例：检查应用是否有全局删除权限
if (!app_has_privilege(vm->selected_app, PRIVILEGE_GLOBAL_DELETE)) {
    return SW_SECURITY_STATUS_NOT_SATISFIED;
}
```

### Phase 4: 多安全域支持

利用 `security_domain_id` 字段实现：
- SSD (Supplementary Security Domain) 创建
- 安全域层次结构管理
- 跨安全域的访问控制

---

## ✨ 总结

本次增强使 GCOS VM 的应用管理更加符合 GlobalPlatform 规范：

✅ **完整的元数据支持** - 类型、权限、安全域  
✅ **Install 生命周期** - 支持应用初始化回调  
✅ **灵活的 API** - 简单和扩展两种注册方式  
✅ **向后兼容** - 现有代码无需修改  
✅ **充分测试** - 所有测试通过  

这些改进为后续的 LOAD/INSTALL 命令实现奠定了坚实的基础！🎊
