#ifndef VM_CORE_H
#define VM_CORE_H

#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 虚拟机上下文结构
 * ============================================================================ */

/**
 * @brief 虚拟机核心上下文
 */
typedef struct VMContext {
    /* 运行时数据区 */
    
    /* 执行器栈 */
    u32 executor_stack[VM_EXECUTOR_STACK_SIZE];  /* 执行器栈（4字节单元） */
    u32 stack_pointer;                            /* 栈指针 */
    u32 base_pointer;                             /* 基址指针 */
    StackFrame frame_stack[64];                   /* 栈帧栈 */
    u32 frame_top;                                /* 栈帧栈顶 */
    
    /* 间接访问变量栈 */
    u8 indirect_var_stack[VM_INDIRECT_VAR_STACK_SIZE][VM_INDIRECT_UNIT_SIZE];
    u32 indirect_stack_pointer;                   /* 间接栈指针 */
    
    /* 全局数据区 */
    u8 global_data[VM_GLOBAL_DATA_SIZE];          /* 全局数据区 */
    
    /* 堆 */
    u8 heap[VM_HEAP_SIZE];                        /* 堆区 */
    u32 heap_used;                                /* 已使用堆大小 */
    
    /* 程序计数器 */
    u32 program_counter;                          /* 程序计数器 */
    
    /* 模块程序区 */
    u8 module_code[VM_MODULE_CODE_SIZE];          /* 模块代码区 */
    
    /* 模块管理 */
    Module modules[MAX_MODULES];                  /* 模块数组 */
    u16 module_count;                             /* 模块数量 */
    
    /* 应用管理 */
    AppInstance apps[MAX_APPS_PER_MODULE * MAX_MODULES]; /* 应用实例数组 */
    u16 app_count;                                /* 应用数量 */
    
    /* 当前运行状态 */
    Module *current_module;                       /* 当前模块 */
    AppInstance *current_app;                     /* 当前应用 */
    u8 current_channel;                           /* 当前逻辑通道 */
    VMState state;                                /* 虚拟机状态 */
    
    /* 异常处理 */
    ExceptionType exception;                      /* 当前异常 */
    u32 exception_handlers[16];                   /* 异常处理器地址 */
    u32 handler_count;                            /* 处理器数量 */
    
    /* 事务管理 */
    TransactionContext transaction;               /* 事务上下文 */
    
    /* 统计信息 */
    u64 instructions_executed;                    /* 已执行指令数 */
    u64 total_execution_time;                     /* 总执行时间（微秒） */
    
} VMContext;

/* ============================================================================
 * API 函数声明
 * ============================================================================ */

/**
 * @brief 创建虚拟机实例
 * @return VMContext* 虚拟机上下文指针，失败返回NULL
 */
VMContext* vm_create(void);

/**
 * @brief 销毁虚拟机实例
 * @param vm 虚拟机上下文指针
 */
void vm_destroy(VMContext *vm);

/**
 * @brief 初始化虚拟机
 * @param vm 虚拟机上下文指针
 * @return 0成功，非0失败
 */
int vm_init(VMContext *vm);

/**
 * @brief 重置虚拟机状态
 * @param vm 虚拟机上下文指针
 */
void vm_reset(VMContext *vm);

/**
 * @brief 加载可执行文件
 * @param vm 虚拟机上下文指针
 * @param file_path 文件路径
 * @return 0成功，非0失败
 */
int vm_load_file(VMContext *vm, const char *file_path);

/**
 * @brief 从内存加载可执行文件
 * @param vm 虚拟机上下文指针
 * @param data 文件数据
 * @param size 数据大小
 * @return 0成功，非0失败
 */
int vm_load_from_memory(VMContext *vm, const u8 *data, u32 size);

/**
 * @brief 选择应用
 * @param vm 虚拟机上下文指针
 * @param aid 应用AID
 * @param channel 逻辑通道号
 * @return 0成功，非0失败
 */
int vm_select_app(VMContext *vm, const AID *aid, u8 channel);

/**
 * @brief 取消选择应用
 * @param vm 虚拟机上下文指针
 * @param channel 逻辑通道号
 * @return 0成功，非0失败
 */
int vm_deselect_app(VMContext *vm, u8 channel);

/**
 * @brief 执行APDU命令
 * @param vm 虚拟机上下文指针
 * @param apdu APDU数据
 * @param apdu_len APDU长度
 * @param response 响应缓冲区
 * @param response_len 响应长度（输入/输出）
 * @return 0成功，非0失败
 */
int vm_execute_apdu(VMContext *vm, const u8 *apdu, u32 apdu_len, 
                    u8 *response, u32 *response_len);

/**
 * @brief 执行字节码指令
 * @param vm 虚拟机上下文指针
 * @return 0成功，非0失败
 */
int vm_execute(VMContext *vm);

/**
 * @brief 单步执行一条指令
 * @param vm 虚拟机上下文指针
 * @return 0成功，非0失败
 */
int vm_step(VMContext *vm);

/**
 * @brief 调用函数
 * @param vm 虚拟机上下文指针
 * @param function_id 函数ID
 * @param params 参数数组
 * @param param_count 参数数量
 * @param result 结果指针
 * @return 0成功，非0失败
 */
int vm_call_function(VMContext *vm, u16 function_id, 
                     const u32 *params, u16 param_count, u32 *result);

/**
 * @brief 安装应用
 * @param vm 虚拟机上下文指针
 * @param module_index 模块索引
 * @param app_aid 应用AID
 * @return 0成功，非0失败
 */
int vm_install_app(VMContext *vm, u16 module_index, const AID *app_aid);

/**
 * @brief 卸载应用
 * @param vm 虚拟机上下文指针
 * @param app_aid 应用AID
 * @return 0成功，非0失败
 */
int vm_uninstall_app(VMContext *vm, const AID *app_aid);

/**
 * @brief 开始事务
 * @param vm 虚拟机上下文指针
 * @return 0成功，非0失败
 */
int vm_transaction_begin(VMContext *vm);

/**
 * @brief 提交事务
 * @param vm 虚拟机上下文指针
 * @return 0成功，非0失败
 */
int vm_transaction_commit(VMContext *vm);

/**
 * @brief 回滚事务
 * @param vm 虚拟机上下文指针
 * @return 0成功，非0失败
 */
int vm_transaction_rollback(VMContext *vm);

/**
 * @brief 获取虚拟机状态
 * @param vm 虚拟机上下文指针
 * @return 虚拟机状态
 */
VMState vm_get_state(const VMContext *vm);

/**
 * @brief 获取异常信息
 * @param vm 虚拟机上下文指针
 * @return 异常类型
 */
ExceptionType vm_get_exception(const VMContext *vm);

/**
 * @brief 清除异常
 * @param vm 虚拟机上下文指针
 */
void vm_clear_exception(VMContext *vm);

/**
 * @brief 获取统计信息
 * @param vm 虚拟机上下文指针
 * @param instr_count 指令计数指针
 * @param exec_time 执行时间指针
 */
void vm_get_stats(const VMContext *vm, u64 *instr_count, u64 *exec_time);

/**
 * @brief 打印虚拟机信息（调试用）
 * @param vm 虚拟机上下文指针
 */
void vm_print_info(const VMContext *vm);

#ifdef __cplusplus
}
#endif

#endif /* VM_CORE_H */
