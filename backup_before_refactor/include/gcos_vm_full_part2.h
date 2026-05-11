/**
 * @file gcos_vm_full_part2.h
 * @brief GCOS VM 完整头文件 - 第二部分
 * 
 * 继续定义API函数声明
 */

#ifndef GCOS_VM_FULL_PART2_H
#define GCOS_VM_FULL_PART2_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * API 函数声明 - 执行器控制
 * ============================================================================ */

/**
 * @brief 启动执行器
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_executor_start(GCOSVM *vm);

/**
 * @brief 停止执行器
 * @param vm VM 指针
 */
void gcos_vm_executor_stop(GCOSVM *vm);

/**
 * @brief 暂停执行器
 * @param vm VM 指针
 */
void gcos_vm_executor_pause(GCOSVM *vm);

/**
 * @brief 恢复执行器
 * @param vm VM 指针
 */
void gcos_vm_executor_resume(GCOSVM *vm);

/**
 * @brief 设置追踪模式
 * @param vm VM 指针
 * @param enable 是否启用追踪
 */
void gcos_vm_executor_set_trace(GCOSVM *vm, bool enable);

/**
 * @brief 设置最大指令数
 * @param vm VM 指针
 * @param max 最大指令数
 */
void gcos_vm_executor_set_max_instructions(GCOSVM *vm, u32 max);

/**
 * @brief 执行虚拟机直到暂停或异常
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_executor_run(GCOSVM *vm);

/**
 * @brief 单步执行一条指令
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_executor_step(GCOSVM *vm);

/* ============================================================================
 * API 函数声明 - 事务管理
 * ============================================================================ */

/**
 * @brief 初始化事务管理器
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_transaction_init(GCOSVM *vm);

/**
 * @brief 开始事务
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_transaction_begin(GCOSVM *vm);

/**
 * @brief 提交事务
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_transaction_commit(GCOSVM *vm);

/**
 * @brief 回滚事务
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_transaction_rollback(GCOSVM *vm);

/**
 * @brief 获取事务状态
 * @param vm VM 指针
 * @return true 激活, false 未激活
 */
bool gcos_vm_transaction_is_active(const GCOSVM *vm);

/**
 * @brief 获取嵌套层级
 * @param vm VM 指针
 * @return 嵌套层级
 */
u8 gcos_vm_transaction_get_nesting_level(const GCOSVM *vm);

/**
 * @brief 获取检查点数量
 * @param vm VM 指针
 * @return 检查点数量
 */
u32 gcos_vm_transaction_get_checkpoint_count(const GCOSVM *vm);

/**
 * @brief 清理事务管理器
 * @param vm VM 指针
 */
void gcos_vm_transaction_cleanup(GCOSVM *vm);

/* ============================================================================
 * API 函数声明 - 应用管理
 * ============================================================================ */

/**
 * @brief 初始化应用管理器
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_app_manager_init(GCOSVM *vm);

/**
 * @brief 安装应用
 * @param vm VM 指针
 * @param module_index 模块索引
 * @param app_aid 应用AID
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_install_app(GCOSVM *vm, u8 module_index, 
                             const GCOSAID *app_aid);

/**
 * @brief 卸载应用
 * @param vm VM 指针
 * @param app_aid 应用AID
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_uninstall_app(GCOSVM *vm, const GCOSAID *app_aid);

/**
 * @brief 选择应用 (单通道)
 * @param vm VM 指针
 * @param channel 逻辑通道号 (0-7)
 * @param app_aid 应用AID
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_select_app(GCOSVM *vm, u8 channel, const GCOSAID *app_aid);

/**
 * @brief 取消选择应用
 * @param vm VM 指针
 * @param channel 逻辑通道号 (0-7)
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_deselect_app(GCOSVM *vm, u8 channel);

/* ============================================================================
 * API 函数声明 - 查询接口
 * ============================================================================ */

/**
 * @brief 获取虚拟机状态
 * @param vm VM 指针
 * @return VM 状态
 */
GCOSVMState gcos_vm_get_state(const GCOSVM *vm);

/**
 * @brief 获取异常信息
 * @param vm VM 指针
 * @return 异常类型
 */
GCOSExceptionType gcos_vm_get_exception(const GCOSVM *vm);

/**
 * @brief 清除异常
 * @param vm VM 指针
 */
void gcos_vm_clear_exception(GCOSVM *vm);

/**
 * @brief 获取统计信息
 * @param vm VM 指针
 * @param instr_count 指令计数指针
 * @param exec_time 执行时间指针
 */
void gcos_vm_get_stats(const GCOSVM *vm, u64 *instr_count, u64 *exec_time);

/**
 * @brief 打印虚拟机信息 (调试用)
 * @param vm VM 指针
 */
void gcos_vm_print_info(const GCOSVM *vm);

/**
 * @brief 打印栈状态 (调试用)
 * @param vm VM 指针
 */
void gcos_vm_print_stack(const GCOSVM *vm);

/**
 * @brief 打印模块信息 (调试用)
 * @param vm VM 指针
 * @param module_index 模块索引
 */
void gcos_vm_print_module_info(const GCOSVM *vm, u8 module_index);

/* ============================================================================
 * API 函数声明 - APDU 命令处理
 * ============================================================================ */

/**
 * @brief 执行 APDU 命令
 * @param vm VM 指针
 * @param channel 逻辑通道号
 * @param apdu APDU 数据
 * @param apdu_len APDU 长度
 * @param response 响应缓冲区
 * @param response_len 响应长度 (输入/输出)
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_execute_apdu(GCOSVM *vm, u8 channel, const u8 *apdu, u32 apdu_len,
                          u8 *response, u32 *response_len);

/* ============================================================================
 * API 函数声明 - 指令集
 * ============================================================================ */

/**
 * @brief 获取指令信息
 * @param opcode 操作码
 * @return 指令信息指针, NULL表示无效操作码
 */
const GCOSInstructionInfo* gcos_instruction_get_info(u8 opcode);

/**
 * @brief 解码指令
 * @param code 字节码指针
 * @param code_size 代码大小
 * @param offset 当前偏移
 * @param opcode 输出:操作码
 * @param operands 输出:操作数数组
 * @param operand_count 输出:操作数数量
 * @return 指令长度(字节), 失败返回0
 */
int gcos_decode_instruction(const u8 *code, u32 code_size, u32 offset,
                           u8 *opcode, u32 *operands, u8 *operand_count);

/**
 * @brief 执行单条指令
 * @param vm VM 指针
 * @param opcode 操作码
 * @param operands 操作数数组
 * @param operand_count 操作数数量
 * @return GCOS_OK 成功
 */
GCOSResult gcos_execute_instruction(GCOSVM *vm, u8 opcode,
                            const u32 *operands, u8 operand_count);

/**
 * @brief 打印指令 (调试用)
 * @param opcode 操作码
 * @param operands 操作数数组
 * @param operand_count 操作数数量
 */
void gcos_print_instruction(u8 opcode, const u32 *operands, u8 operand_count);

/**
 * @brief 获取操作码分类
 * @param opcode 操作码
 * @return 操作码分类
 */
GCOSOpCodeCategory gcos_instruction_get_category(u8 opcode);

/**
 * @brief 检查操作码是否有效
 * @param opcode 操作码
 * @return true 有效, false 无效
 */
bool gcos_instruction_is_valid(u8 opcode);

/**
 * @brief 获取所有指令数量
 * @return 指令总数
 */
u32 gcos_instruction_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_VM_FULL_PART2_H */
