#ifndef VM_MEMORY_H
#define VM_MEMORY_H

#include "vm_types.h"
#include "vm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 栈操作API
 * ============================================================================ */

/**
 * @brief 压入执行器栈
 * @param vm 虚拟机上下文
 * @param value 值（32位）
 * @return 0成功，非0失败（栈溢出）
 */
int vm_stack_push(VMContext *vm, u32 value);

/**
 * @brief 从执行器栈弹出
 * @param vm 虚拟机上下文
 * @param value 输出：弹出的值
 * @return 0成功，非0失败（栈下溢）
 */
int vm_stack_pop(VMContext *vm, u32 *value);

/**
 * @brief 查看栈顶元素（不弹出）
 * @param vm 虚拟机上下文
 * @param value 输出：栈顶值
 * @return 0成功，非0失败
 */
int vm_stack_peek(VMContext *vm, u32 *value);

/**
 * @brief 获取栈深度
 * @param vm 虚拟机上下文
 * @return 当前栈深度
 */
u32 vm_stack_depth(const VMContext *vm);

/**
 * @brief 压入间接访问变量栈
 * @param vm 虚拟机上下文
 * @param data 数据指针（16字节）
 * @return 0成功，非0失败
 */
int vm_indirect_stack_push(VMContext *vm, const u8 *data);

/**
 * @brief 从间接访问变量栈弹出
 * @param vm 虚拟机上下文
 * @param data 输出：数据缓冲区（16字节）
 * @return 0成功，非0失败
 */
int vm_indirect_stack_pop(VMContext *vm, u8 *data);

/* ============================================================================
 * 堆管理API
 * ============================================================================ */

/**
 * @brief 从堆分配内存
 * @param vm 虚拟机上下文
 * @param size 分配大小（字节）
 * @return 分配的内存指针，失败返回NULL
 */
void* vm_heap_alloc(VMContext *vm, u32 size);

/**
 * @brief 释放堆内存
 * @param vm 虚拟机上下文
 * @param ptr 内存指针
 * @return 0成功，非0失败
 */
int vm_heap_free(VMContext *vm, void *ptr);

/**
 * @brief 重新分配堆内存
 * @param vm 虚拟机上下文
 * @param ptr 原内存指针
 * @param new_size 新大小
 * @return 新内存指针，失败返回NULL
 */
void* vm_heap_realloc(VMContext *vm, void *ptr, u32 new_size);

/**
 * @brief 获取堆使用情况
 * @param vm 虚拟机上下文
 * @param used 已使用大小指针
 * @param free 空闲大小指针
 */
void vm_heap_stats(const VMContext *vm, u32 *used, u32 *free);

/* ============================================================================
 * 模块数据管理API
 * ============================================================================ */

/**
 * @brief 初始化模块全局数据
 * @param vm 虚拟机上下文
 * @param module 模块指针
 * @return 0成功，非0失败
 */
int vm_module_init_global_data(VMContext *vm, Module *module);

/**
 * @brief 分配模块域数据
 * @param vm 虚拟机上下文
 * @param module 模块指针
 * @param size 数据大小
 * @return 0成功，非0失败
 */
int vm_module_alloc_domain_data(VMContext *vm, Module *module, u32 size);

/**
 * @brief 回收模块全局数据
 * @param vm 虚拟机上下文
 * @param module 模块指针
 * @return 0成功，非0失败
 */
int vm_module_free_global_data(VMContext *vm, Module *module);

/**
 * @brief 读取模块只读数据
 * @param module 模块指针
 * @param offset 偏移
 * @param size 读取大小
 * @param buffer 输出缓冲区
 * @return 0成功，非0失败
 */
int vm_module_read_rodata(const Module *module, u32 offset, u32 size, u8 *buffer);

/* ============================================================================
 * 应用数据管理API
 * ============================================================================ */

/**
 * @brief 创建临时静态数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param size 数据大小
 * @return 数据指针，失败返回NULL
 */
void* vm_app_create_temp_static(VMContext *vm, AppInstance *app, u32 size);

/**
 * @brief 创建临时动态数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param size 数据大小
 * @return 数据指针，失败返回NULL
 */
void* vm_app_create_temp_dynamic(VMContext *vm, AppInstance *app, u32 size);

/**
 * @brief 创建跨域数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param size 数据大小
 * @return 数据指针，失败返回NULL
 */
void* vm_app_create_cross_domain(VMContext *vm, AppInstance *app, u32 size);

/**
 * @brief 创建持久性数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param size 数据大小
 * @return 数据指针，失败返回NULL
 */
void* vm_app_create_persistent(VMContext *vm, AppInstance *app, u32 size);

/**
 * @brief 创建应用域数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param size 数据大小
 * @param init_data 初始化数据（可为NULL）
 * @return 数据指针，失败返回NULL
 */
void* vm_app_create_app_domain(VMContext *vm, AppInstance *app, u32 size, 
                                const u8 *init_data);

/**
 * @brief 创建引用域数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param size 数据大小
 * @return 数据指针，失败返回NULL
 */
void* vm_app_create_ref_domain(VMContext *vm, AppInstance *app, u32 size);

/**
 * @brief 删除应用数据
 * @param vm 虚拟机上下文
 * @param app 应用实例指针
 * @param data_ptr 数据指针
 * @param data_type 数据类型
 * @return 0成功，非0失败
 */
int vm_app_delete_data(VMContext *vm, AppInstance *app, void *data_ptr, 
                       DataType data_type);

/**
 * @brief 检查引用域是否有引用
 * @param vm 虚拟机上下文
 * @param data_ptr 数据指针
 * @return true有引用，false无引用
 */
bool vm_check_ref_domain(VMContext *vm, const void *data_ptr);

/* ============================================================================
 * 全局数据区操作API
 * ============================================================================ */

/**
 * @brief 写入全局数据区
 * @param vm 虚拟机上下文
 * @param offset 偏移
 * @param size 写入大小
 * @param data 数据指针
 * @return 0成功，非0失败
 */
int vm_global_write(VMContext *vm, u32 offset, u32 size, const u8 *data);

/**
 * @brief 读取全局数据区
 * @param vm 虚拟机上下文
 * @param offset 偏移
 * @param size 读取大小
 * @param buffer 输出缓冲区
 * @return 0成功，非0失败
 */
int vm_global_read(const VMContext *vm, u32 offset, u32 size, u8 *buffer);

/**
 * @brief 清零全局数据区
 * @param vm 虚拟机上下文
 * @return 0成功，非0失败
 */
int vm_global_clear(VMContext *vm);

/* ============================================================================
 * 内存保护API
 * ============================================================================ */

/**
 * @brief 检查地址是否可访问
 * @param vm 虚拟机上下文
 * @param address 地址
 * @param size 访问大小
 * @param write 是否为写操作
 * @return true可访问，false不可访问
 */
bool vm_check_memory_access(const VMContext *vm, u32 address, u32 size, bool write);

/**
 * @brief 设置内存区域权限
 * @param vm 虚拟机上下文
 * @param start 起始地址
 * @param size 区域大小
 * @param readable 可读
 * @param writable 可写
 * @param executable 可执行
 * @return 0成功，非0失败
 */
int vm_set_memory_permission(VMContext *vm, u32 start, u32 size,
                             bool readable, bool writable, bool executable);

/* ============================================================================
 * 调试和诊断API
 * ============================================================================ */

/**
 * @brief 打印栈内容（调试用）
 * @param vm 虚拟机上下文
 */
void vm_print_stack(const VMContext *vm);

/**
 * @brief 打印堆使用情况（调试用）
 * @param vm 虚拟机上下文
 */
void vm_print_heap_usage(const VMContext *vm);

/**
 * @brief 打印内存映射（调试用）
 * @param vm 虚拟机上下文
 */
void vm_print_memory_map(const VMContext *vm);

#ifdef __cplusplus
}
#endif

#endif /* VM_MEMORY_H */
