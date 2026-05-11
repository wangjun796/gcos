#ifndef VM_EXECUTOR_H
#define VM_EXECUTOR_H

#include "vm_types.h"
#include "vm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 执行器配置
 * ============================================================================ */

/**
 * @brief 执行器配置选项
 */
typedef struct {
    bool enable_debug;          /* 启用调试模式 */
    bool enable_trace;          /* 启用指令追踪 */
    bool enable_profiling;      /* 启用性能分析 */
    u32 max_instructions;       /* 最大指令数限制（0=无限制） */
    u32 max_execution_time;     /* 最大执行时间（毫秒，0=无限制） */
} ExecutorConfig;

/* ============================================================================
 * 执行器API
 * ============================================================================ */

/**
 * @brief 初始化执行器
 * @param vm 虚拟机上下文
 * @param config 执行器配置（可为NULL使用默认配置）
 * @return 0成功，非0失败
 */
int vm_executor_init(VMContext *vm, const ExecutorConfig *config);

/**
 * @brief 启动执行器
 * @param vm 虚拟机上下文
 * @return 0成功，非0失败
 */
int vm_executor_start(VMContext *vm);

/**
 * @brief 停止执行器
 * @param vm 虚拟机上下文
 */
void vm_executor_stop(VMContext *vm);

/**
 * @brief 暂停执行器
 * @param vm 虚拟机上下文
 */
void vm_executor_pause(VMContext *vm);

/**
 * @brief 恢复执行器
 * @param vm 虚拟机上下文
 */
void vm_executor_resume(VMContext *vm);

/**
 * @brief 执行主循环
 * @param vm 虚拟机上下文
 * @return 0正常结束，非0异常
 */
int vm_executor_run(VMContext *vm);

/**
 * @brief 单步执行
 * @param vm 虚拟机上下文
 * @return 0成功，非0失败
 */
int vm_executor_step(VMContext *vm);

/**
 * @brief 设置断点
 * @param vm 虚拟机上下文
 * @param address 断点地址
 * @return 0成功，非0失败
 */
int vm_executor_set_breakpoint(VMContext *vm, u32 address);

/**
 * @brief 清除断点
 * @param vm 虚拟机上下文
 * @param address 断点地址
 * @return 0成功，非0失败
 */
int vm_executor_clear_breakpoint(VMContext *vm, u32 address);

/**
 * @brief 检查是否到达断点
 * @param vm 虚拟机上下文
 * @return true是断点，false不是
 */
bool vm_executor_is_breakpoint(const VMContext *vm);

/* ============================================================================
 * 栈帧管理API
 * ============================================================================ */

/**
 * @brief 创建新的栈帧
 * @param vm 虚拟机上下文
 * @param return_address 返回地址
 * @param frame_size 栈帧大小
 * @return 0成功，非0失败
 */
int vm_executor_push_frame(VMContext *vm, u32 return_address, u32 frame_size);

/**
 * @brief 弹出栈帧
 * @param vm 虚拟机上下文
 * @param return_address 输出：返回地址
 * @return 0成功，非0失败
 */
int vm_executor_pop_frame(VMContext *vm, u32 *return_address);

/**
 * @brief 获取当前栈帧信息
 * @param vm 虚拟机上下文
 * @param frame 输出：栈帧指针
 * @return 0成功，非0失败
 */
int vm_executor_get_current_frame(const VMContext *vm, StackFrame **frame);

/**
 * @brief 获取调用栈深度
 * @param vm 虚拟机上下文
 * @return 调用栈深度
 */
u32 vm_executor_get_call_depth(const VMContext *vm);

/* ============================================================================
 * 函数调用API
 * ============================================================================ */

/**
 * @brief 调用内部函数
 * @param vm 虚拟机上下文
 * @param function_id 函数ID
 * @return 0成功，非0失败
 */
int vm_executor_call_function(VMContext *vm, u16 function_id);

/**
 * @brief 从函数返回
 * @param vm 虚拟机上下文
 * @param return_value 返回值
 * @return 0成功，非0失败
 */
int vm_executor_return_from_function(VMContext *vm, u32 return_value);

/**
 * @brief 调用外部接口（TRAP）
 * @param vm 虚拟机上下文
 * @param trap_id 陷阱ID
 * @param params 参数
 * @param param_count 参数数量
 * @return 0成功，非0失败
 */
int vm_executor_call_trap(VMContext *vm, u16 trap_id, 
                          const u32 *params, u8 param_count);

/* ============================================================================
 * 异常处理API
 * ============================================================================ */

/**
 * @brief 抛出异常
 * @param vm 虚拟机上下文
 * @param exception 异常类型
 * @param message 异常消息
 */
void vm_executor_throw_exception(VMContext *vm, ExceptionType exception, 
                                 const char *message);

/**
 * @brief 捕获异常
 * @param vm 虚拟机上下文
 * @param handler_address 异常处理器地址
 * @return 0成功，非0失败
 */
int vm_executor_catch_exception(VMContext *vm, u32 handler_address);

/**
 * @brief 注册异常处理器
 * @param vm 虚拟机上下文
 * @param exception_type 异常类型
 * @param handler_address 处理器地址
 * @return 0成功，非0失败
 */
int vm_executor_register_handler(VMContext *vm, ExceptionType exception_type,
                                 u32 handler_address);

/**
 * @brief 清除异常处理器
 * @param vm 虚拟机上下文
 * @param exception_type 异常类型
 */
void vm_executor_clear_handler(VMContext *vm, ExceptionType exception_type);

/**
 * @brief 进入TRY块
 * @param vm 虚拟机上下文
 * @param catch_address CATCH块地址
 * @return 0成功，非0失败
 */
int vm_executor_enter_try(VMContext *vm, u32 catch_address);

/**
 * @brief 退出TRY块
 * @param vm 虚拟机上下文
 * @return 0成功，非0失败
 */
int vm_executor_exit_try(VMContext *vm);

/* ============================================================================
 * 调试和追踪API
 * ============================================================================ */

/**
 * @brief 启用/禁用指令追踪
 * @param vm 虚拟机上下文
 * @param enable true启用，false禁用
 */
void vm_executor_set_trace(VMContext *vm, bool enable);

/**
 * @brief 打印当前指令（调试用）
 * @param vm 虚拟机上下文
 */
void vm_executor_print_current_instruction(const VMContext *vm);

/**
 * @brief 打印调用栈（调试用）
 * @param vm 虚拟机上下文
 */
void vm_executor_print_call_stack(const VMContext *vm);

/**
 * @brief 获取执行统计信息
 * @param vm 虚拟机上下文
 * @param instruction_count 指令计数
 * @param execution_time_us 执行时间（微秒）
 * @param stack_peak 栈峰值深度
 */
void vm_executor_get_stats(const VMContext *vm, u64 *instruction_count,
                           u64 *execution_time_us, u32 *stack_peak);

/**
 * @brief 重置执行统计
 * @param vm 虚拟机上下文
 */
void vm_executor_reset_stats(VMContext *vm);

/* ============================================================================
 * 性能分析API
 * ============================================================================ */

/**
 * @brief 开始性能分析
 * @param vm 虚拟机上下文
 */
void vm_executor_start_profiling(VMContext *vm);

/**
 * @brief 停止性能分析
 * @param vm 虚拟机上下文
 */
void vm_executor_stop_profiling(VMContext *vm);

/**
 * @brief 获取函数执行次数
 * @param vm 虚拟机上下文
 * @param function_id 函数ID
 * @return 执行次数
 */
u32 vm_executor_get_function_call_count(const VMContext *vm, u16 function_id);

/**
 * @brief 获取热点函数列表
 * @param vm 虚拟机上下文
 * @param functions 函数ID数组（输出）
 * @param counts 执行次数数组（输出）
 * @param max_count 最大条目数
 * @return 实际条目数
 */
int vm_executor_get_hot_functions(const VMContext *vm, u16 *functions,
                                  u32 *counts, int max_count);

#ifdef __cplusplus
}
#endif

#endif /* VM_EXECUTOR_H */
