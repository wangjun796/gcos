/**
 * @file gcos_vm_full.h
 * @brief GCOS VM 完整头文件
 * 
 * 整合所有模块的接口，提供统一的虚拟机API
 */

#ifndef GCOS_VM_FULL_H
#define GCOS_VM_FULL_H

#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 版本信息
 * ============================================================================ */

#define GCOS_VM_VERSION_MAJOR       1
#define GCOS_VM_VERSION_MINOR       0
#define GCOS_VM_VERSION_PATCH       0
#define GCOS_VM_VERSION             "1.0.0"

/* ============================================================================
 * 虚拟机状态
 * ============================================================================ */

typedef enum {
    GCOS_STATE_IDLE,                /* 空闲状态 */
    GCOS_STATE_INITIALIZING,        /* 初始化中 */
    GCOS_STATE_RUNNING,             /* 运行中 */
    GCOS_STATE_SUSPENDED,           /* 挂起 */
    GCOS_STATE_EXCEPTION,           /* 异常状态 */
    GCOS_STATE_SHUTDOWN             /* 关闭状态 */
} GCOSVMState;

/* ============================================================================
 * 异常类型
 * ============================================================================ */

typedef enum {
    GCOS_EXCEPTION_NONE = 0,

    /* 栈相关异常 */
    GCOS_EXCEPTION_STACK_OVERFLOW,       /* 栈溢出 */
    GCOS_EXCEPTION_STACK_UNDERFLOW,      /* 栈下溢 */

    /* 算术异常 */
    GCOS_EXCEPTION_DIVISION_BY_ZERO,     /* 除零错误 */
    GCOS_EXCEPTION_ARITHMETIC_OVERFLOW,  /* 算术溢出 */

    /* 指令异常 */
    GCOS_EXCEPTION_INVALID_OPCODE,       /* 无效操作码 */
    GCOS_EXCEPTION_INVALID_OPERAND,      /* 无效操作数 */

    /* 内存异常 */
    GCOS_EXCEPTION_INVALID_ADDRESS,      /* 无效地址 */
    GCOS_EXCEPTION_ACCESS_VIOLATION,     /* 访问违例 */
    GCOS_EXCEPTION_OUT_OF_MEMORY,        /* 内存不足 */

    /* 类型异常 */
    GCOS_EXCEPTION_TYPE_MISMATCH,        /* 类型不匹配 */
    GCOS_EXCEPTION_ARRAY_BOUNDS,         /* 数组越界 */
    GCOS_EXCEPTION_NULL_REFERENCE,       /* 空引用 */

    /* 事务异常 */
    GCOS_EXCEPTION_TRANSACTION_ABORT,    /* 事务中止 */
    GCOS_EXCEPTION_TRANSACTION_NESTING,  /* 事务嵌套过深 */

    /* 安全异常 */
    GCOS_EXCEPTION_SECURITY_VIOLATION,   /* 安全违例 */
    GCOS_EXCEPTION_UNAUTHORIZED_ACCESS,  /* 未授权访问 */

    /* 应用生命周期异常 */
    GCOS_EXCEPTION_APP_NOT_SELECTED,     /* 应用未选择 */
    GCOS_EXCEPTION_APP_ALREADY_SELECTED, /* 应用已选择 */

    GCOS_EXCEPTION_MAX
} GCOSExceptionType;

/* ============================================================================
 * 返回码
 * ============================================================================ */

typedef enum {
    GCOS_OK = 0,                    /* 成功 */
    GCOS_ERR_INVALID_PARAM = -1,    /* 无效参数 */
    GCOS_ERR_OUT_OF_MEMORY = -2,    /* 内存不足 */
    GCOS_ERR_STACK_OVERFLOW = -3,   /* 栈溢出 */
    GCOS_ERR_STACK_UNDERFLOW = -4,  /* 栈下溢 */
    GCOS_ERR_INVALID_OPCODE = -5,   /* 无效操作码 */
    GCOS_ERR_ACCESS_DENIED = -6,    /* 访问拒绝 */
    GCOS_ERR_APP_NOT_FOUND = -7,    /* 应用未找到 */
    GCOS_ERR_MODULE_NOT_FOUND = -8, /* 模块未找到 */
    GCOS_ERR_TRANSACTION_FAIL = -9, /* 事务失败 */
    GCOS_ERR_FILE_FORMAT = -10,     /* 文件格式错误 */
    GCOS_ERR_VERSION_MISMATCH = -11,/* 版本不匹配 */
    GCOS_ERR_EXCEPTION = -12,       /* 异常 */
    GCOS_ERR_NOT_IMPLEMENTED = -99  /* 未实现 */
} GCOSResult;

/* ============================================================================
 * 前向声明
 * ============================================================================ */

typedef struct GCOSVM GCOSVM;
typedef struct GCOSModule GCOSModule;
typedef struct GCOSAppInstance GCOSAppInstance;
typedef struct GCOSRuntimeContext GCOSRuntimeContext;

/* ============================================================================
 * 数据结构
 * ============================================================================ */

/**
 * @brief AID (Application Identifier)
 */
typedef struct {
    u8 aid[16];                 /* AID数据 */
    u8 length;                  /* AID长度 (1-16) */
} GCOSAID;

/**
 * @brief 栈帧
 */
typedef struct {
    u32 return_address;         /* 返回地址 (PC) */
    u32 base_pointer;           /* 基址指针 (BP) */
    u32 frame_size;             /* 栈帧大小 (字节) */
    u32 local_vars_offset;      /* 局部变量偏移 */
    u32 param_count;            /* 参数数量 */
} GCOSStackFrame;

/**
 * @brief 函数头信息
 */
typedef struct {
    u16 function_id;            /* 函数ID */
    u16 param_count;            /* 参数数量 */
    u16 local_var_count;        /* 局部变量数量 */
    u16 max_stack_depth;        /* 最大栈深度 */
    u32 code_offset;            /* 代码偏移 (在模块代码区中的位置) */
    u32 code_size;              /* 代码大小 (字节) */
    bool is_exported;           /* 是否导出 */
    bool is_imported;           /* 是否导入 */
} GCOSFunctionHeader;

/**
 * @brief 模块信息
 */
struct GCOSModule {
    GCOSAID module_aid;             /* 模块AID */
    u8 module_type;                /* 模块类型 (0=应用, 1=库) */
    u32 version;                   /* 版本号 */

    /* 数据区 */
    u8 *global_data;              /* 模块全局数据 (易失性) */
    u32 global_data_size;         /* 全局数据大小 */

    const u8 *readonly_data;       /* 模块只读数据 (非易失性,ROM) */
    u32 readonly_data_size;      /* 只读数据大小 */

    u8 *domain_data;              /* 模块域数据 (非易失性) */
    u32 domain_data_size;         /* 域数据大小 */

    /* 代码区 */
    const u8 *code;                 /* 模块程序代码 (非易失性,ROM) */
    u32 code_size;                /* 代码大小 */

    /* 函数表 */
    GCOSFunctionHeader *functions;  /* 函数头数组 */
    u16 function_count;           /* 函数数量 */

    /* 导入/导出表 */
    void *import_table;            /* 导入表 */
    u16 import_count;             /* 导入数量 */

    void *export_table;            /* 导出表 */
    u16 export_count;             /* 导出数量 */

    bool loaded;                   /* 是否已加载 */
    bool initialized;              /* 是否已初始化 */
};

/**
 * @brief 应用实例
 */
struct GCOSAppInstance {
    GCOSAID app_aid;                /* 应用AID */
    u16 module_index;               /* 所属模块索引 */
    u8 lifecycle;                 /* 生命周期状态 */

    /* 应用数据 */
    u8 *app_domain_data;           /* 应用域数据 */
    u32 app_domain_data_size;      /* 应用域数据大小 */

    u8 *ref_domain_data;          /* 引用域数据 */
    u32 ref_domain_data_size;       /* 引用域数据大小 */

    u8 *persistent_data;           /* 持久性数据 */
    u32 persistent_data_size;       /* 持久性数据大小 */

    /* 运行时数据（每个通道独立） */
    struct {
        u8 *temp_dynamic_data;      /* 临时动态数据 */
        u32 temp_dynamic_data_size; /* 临时动态数据大小 */

        u8 *global_data_copy;       /* 模块全局数据副本 */
        u32 global_data_copy_size;  /* 副本大小 */

        bool active;                /* 是否激活 */
        bool selected;              /* 是否被选择 */
    } channel_data[MAX_CHANNELS];

    u8 current_channel;             /* 当前活动通道 */

    bool installed;                 /* 是否已安装 */
    u32 install_time;               /* 安装时间戳 */
};

/**
 * @brief 事务上下文
 */
typedef struct {
    bool active;                    /* 事务是否激活 */
    u8 *backup_data;              /* 备份数据指针 */
    u32 backup_size;              /* 备份大小 */
    u32 checkpoint_count;         /* 检查点数量 */
    u16 nesting_level;             /* 嵌套层级 */
} GCOSTransactionContext;

/**
 * @brief 运行时上下文
 */
struct GCOSRuntimeContext {
    /* 执行器栈 (易失性,栈单元4字节) */
    u32 executor_stack[256];
    u32 stack_pointer;              /* 栈指针 (SP) */
    u32 base_pointer;               /* 基址指针 (BP) */

    /* 栈帧栈 */
    GCOSStackFrame frame_stack[64]; /* 最大64层调用 */
    u32 frame_top;                 /* 栈帧栈顶 */

    /* 间接访问变量栈 (易失性,栈单元16字节) */
    u8 indirect_var_stack[64][16];
    u32 indirect_stack_pointer;    /* 间接栈指针 */

    /* 全局数据区 (易失性) */
    u8 global_data[4096];
    u32 global_data_used;          /* 已使用大小 */

    /* 堆 (非易失性) */
    u8 heap[8192];
    u32 heap_used;                 /* 堆已使用大小 */

    /* 程序计数器 */
    u32 program_counter;            /* PC - 当前指令地址 */

    /* 当前运行上下文 */
    GCOSModule *current_module;     /* 当前模块 */
    GCOSAppInstance *current_app;   /* 当前应用实例 */
    u8 current_channel;             /* 当前逻辑通道 */

    /* 事务上下文 */
    GCOSTransactionContext transaction;

    /* 异常状态 */
    GCOSExceptionType exception;    /* 当前异常类型 */
    u32 exception_address;          /* 异常发生地址 */
};

/**
 * @brief GCOS VM 主结构
 */
struct GCOSVM {
    /* VM 状态 */
    GCOSVMState state;            /* VM状态 */
    u32 uptime;                   /* 运行时间 (毫秒) */

    /* 运行时上下文 */
    GCOSRuntimeContext runtime;     /* 运行时上下文 */

    /* 模块管理 */
    GCOSModule modules[32];       /* 模块数组 */
    u8 module_count;               /* 已加载模块数 */

    /* 应用管理 */
    GCOSAppInstance *apps[32 * 16]; /* 应用实例数组 */
    u16 app_count;                /* 已安装应用数 */

    /* 通道管理 */
    struct {
        GCOSAppInstance *selected_app;  /* 该通道选择的应用 */
        bool active;                    /* 通道是否激活 */
    } channels[8];

    /* 统计信息 */
    struct {
        u64 instructions_executed;  /* 已执行指令数 */
        u64 function_calls;         /* 函数调用次数 */
        u32 gc_cycles;              /* GC周期数 */
        u32 exceptions_handled;     /* 异常处理次数 */
        u32 transactions_committed; /* 事务提交次数 */
        u32 transactions_aborted;   /* 事务中止次数 */
    } stats;

    /* 配置 */
    struct {
        bool enable_debug;            /* 启用调试模式 */
        bool enable_trace;            /* 启用指令追踪 */
        bool enable_security;         /* 启用安全检查 */
        u32 max_execution_time;     /* 最大执行时间 (毫秒, 0=无限制) */
        u32 stack_guard_size;       /* 栈保护区域大小 */
    } config;
};

/* ============================================================================
 * API 函数声明 - VM生命周期管理
 * ============================================================================ */

/**
 * @brief 创建 GCOS VM 实例
 * @return VM 指针, NULL 表示失败
 */
GCOSVM* gcos_vm_create(void);

/**
 * @brief 销毁 GCOS VM 实例
 * @param vm VM 指针
 */
void gcos_vm_destroy(GCOSVM *vm);

/**
 * @brief 初始化 VM
 * @param vm VM 指针
 * @return GCOS_OK 成功, 其他错误码
 */
GCOSResult gcos_vm_init(GCOSVM *vm);

/**
 * @brief 重置 VM 到初始状态
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_reset(GCOSVM *vm);

/* ============================================================================
 * API 函数声明 - 模块管理
 * ============================================================================ */

/**
 * @brief 加载模块 (从 SEF 文件)
 * @param vm VM 指针
 * @param sef_data SEF 文件数据
 * @param sef_size SEF 文件大小
 * @param[out] module_index 输出的模块索引
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_load_module(GCOSVM *vm, const u8 *sef_data, 
                             u32 sef_size, u8 *module_index);

/**
 * @brief 卸载模块
 * @param vm VM 指针
 * @param module_index 模块索引
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_unload_module(GCOSVM *vm, u8 module_index);

/**
 * @brief 根据 AID 查找模块
 * @param vm VM 指针
 * @param aid AID 指针
 * @return 模块索引, 0xFF 表示未找到
 */
u8 gcos_vm_find_module_by_aid(GCOSVM *vm, const GCOSAID *aid);

/* ============================================================================
 * API 函数声明 - 应用管理
 * ============================================================================ */

/**
 * @brief 安装应用实例
 * @param vm VM 指针
 * @param module_index 模块索引
 * @param app_aid 应用 AID
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_install_app(GCOSVM *vm, u8 module_index, 
                              const GCOSAID *app_aid);

/**
 * @brief 卸载应用实例
 * @param vm VM 指针
 * @param app_aid 应用 AID
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_uninstall_app(GCOSVM *vm, const GCOSAID *app_aid);

/**
 * @brief 选择应用 (单通道)
 * @param vm VM 指针
 * @param channel 逻辑通道号 (0-7)
 * @param app_aid 应用 AID
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_select_app(GCOSVM *vm, u8 channel, const GCOSAID *app_aid);

/**
 * @brief 取消选择应用
 * @param vm VM 指针
 * @param channel 逻辑通道号
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_deselect_app(GCOSVM *vm, u8 channel);

/* ============================================================================
 * API 函数声明 - 执行器控制
 * ============================================================================ */

/**
 * @brief 执行 APDU 命令
 * @param vm VM 指针
 * @param channel 逻辑通道号
 * @param apdu APDU 数据
 * @param apdu_len APDU 长度
 * @param[out] response 响应数据缓冲区
 * @param[in,out] response_len 响应长度 (输入缓冲区大小,输出实际长度)
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_execute_apdu(GCOSVM *vm, u8 channel, const u8 *apdu, 
                              u32 apdu_len, u8 *response, u32 *response_len);

/**
 * @brief 执行字节码指令 (单步)
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_step(GCOSVM *vm);

/**
 * @brief 运行 VM (直到暂停或异常)
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_run(GCOSVM *vm);

/**
 * @brief 暂停 VM
 * @param vm VM 指针
 */
void gcos_vm_pause(GCOSVM *vm);

/**
 * @brief 恢复 VM
 * @param vm VM 指针
 */
void gcos_vm_resume(GCOSVM *vm);

/* ============================================================================
 * API 函数声明 - 事务管理
 * ============================================================================ */

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

/* ============================================================================
 * API 函数声明 - 数据访问
 * ============================================================================ */

/**
 * @brief 读取应用数据
 * @param vm VM 指针
 * @param app 应用实例
 * @param data_type 数据类型
 * @param offset 偏移
 * @param[out] buffer 输出缓冲区
 * @param size 读取大小
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_read_app_data(GCOSVM *vm, GCOSAppInstance *app, 
                               u8 data_type, u32 offset, u8 *buffer, u32 size);

/**
 * @brief 写入应用数据
 * @param vm VM 指针
 * @param app 应用实例
 * @param data_type 数据类型
 * @param offset 偏移
 * @param buffer 数据缓冲区
 * @param size 写入大小
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_write_app_data(GCOSVM *vm, GCOSAppInstance *app, 
                                u8 data_type, u32 offset, const u8 *buffer, u32 size);

/* ============================================================================
 * API 函数声明 - 查询接口
 * ============================================================================ */

/**
 * @brief 获取 VM 状态
 * @param vm VM 指针
 * @return VM 状态
 */
GCOSVMState gcos_vm_get_state(const GCOSVM *vm);

/**
 * @brief 获取当前异常
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
 * @brief 获取异常描述字符串
 * @param exception 异常类型
 * @return 描述字符串
 */
const char* gcos_vm_exception_to_string(GCOSExceptionType exception);

/**
 * @brief 获取统计信息
 * @param vm VM 指针
 * @param[out] instructions_executed 已执行指令数
 * @param[out] function_calls 函数调用次数
 */
void gcos_vm_get_stats(const GCOSVM *vm, u64 *instructions_executed, u64 *function_calls);

/* ============================================================================
 * API 函数声明 - 调试支持
 * ============================================================================ */

/**
 * @brief 打印 VM 信息
 * @param vm VM 指针
 */
void gcos_vm_print_info(const GCOSVM *vm);

/**
 * @brief 打印栈状态
 * @param vm VM 指针
 */
void gcos_vm_print_stack(const GCOSVM *vm);

/**
 * @brief 打印模块信息
 * @param vm VM 指针
 * @param module_index 模块索引
 */
void gcos_vm_print_module_info(const GCOSVM *vm, u8 module_index);

#ifdef __cplusplus
}
#endif
