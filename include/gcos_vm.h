/**
 * @file gcos_vm.h
 * @brief GCOS VM - Domestic Smart Card Virtual Machine Based on COS3 Specification
 * 
 * GCOS VM (GuoChao Operating System Virtual Machine)
 * Implementation based on GB/T 44901.3 "Card and Identity Recognition Security Device 
 * On-Chip Operating System Part 3" specification
 * 
 * Features:
 * - Stack-based bytecode executor (Pop-Execute-Push model)
 * - Support for procedural application post-download
 * - Multi-channel application isolation
 * - Transaction management mechanism
 * - Runtime security management
 * - Zero dynamic memory allocation (suitable for embedded environments)
 * 
 * @version 1.0.0
 * @date 2026-05-08
 * @author GCOS VM Development Team
 */

#ifndef GCOS_VM_H
#define GCOS_VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define GCOS_VM_VERSION_MAJOR       1
#define GCOS_VM_VERSION_MINOR       0
#define GCOS_VM_VERSION_PATCH       0
#define GCOS_VM_VERSION             "1.0.0"

/* ============================================================================
 * Basic Type Definitions
 * ============================================================================ */

typedef uint8_t     u8;
typedef int8_t      s8;
typedef uint16_t    u16;
typedef int16_t     s16;
typedef uint32_t    u32;
typedef int32_t     s32;
typedef uint64_t    u64;
typedef int64_t     s64;
typedef float       f32;
typedef double      f64;

/* ============================================================================
 * Constant Definitions - Based on COS3 Specification Table 39 Runtime Data Area
 * ============================================================================ */

/* Executor Stack Configuration (Table 39: Stack unit is 4 bytes) */
#define GCOS_EXECUTOR_STACK_SIZE        256     /* Executor stack size (units) */
#define GCOS_STACK_UNIT_SIZE            4       /* Stack unit: 4 bytes */

/* Indirect Variable Stack (Table 39: Stack unit is 16 bytes) */
#define GCOS_INDIRECT_STACK_SIZE        64      /* Indirect variable stack size (units) */
#define GCOS_INDIRECT_UNIT_SIZE         16      /* Indirect unit: 16 bytes */

/* Global Data Area (Volatile storage area) */
#define GCOS_GLOBAL_DATA_SIZE           4096    /* Global data area size (bytes) */

/* Heap (Non-volatile storage area) */
#define GCOS_HEAP_SIZE                  8192    /* Heap size (bytes) */

/* Module Program Area (Non-volatile storage area) */
#define GCOS_MODULE_CODE_SIZE           16384   /* Module program area size (bytes) */

/* File Type Identifiers (COS3 Specification Table 10) */
#define FILE_TYPE_ASM                   0x0061736D  /* "asm" - Intermediate file */
#define FILE_TYPE_LINK                  0x6C696E6B  /* "link" - Link file */
#define FILE_TYPE_SEF                   0x00736566  /* "sef" - Loadable file */

/* Section Identifiers (COS3 Specification Table 18) */
#define SECTION_ID_FIRST                0x01    /* First section (required) */
#define SECTION_ID_IMPORT               0x02    /* 导入段 (可选) */
#define SECTION_ID_FUNCTION             0x03    /* 函数段 (必选) */
#define SECTION_ID_APP                  0x04    /* 应用段 (可选) */
#define SECTION_ID_GLOBAL               0x05    /* 全局段 (必选) */
#define SECTION_ID_EXPORT               0x06    /* 导出段 (可选) */
#define SECTION_ID_ELEMENT              0x07    /* 元素段 (可选) */
#define SECTION_ID_DATA                 0x08    /* 数据段 (可选) */
#define SECTION_ID_CODE                 0x09    /* 代码段 (必选) */
#define SECTION_ID_CUSTOM_START         0x0A    /* 自定义段起始 */
#define SECTION_ID_CUSTOM_END           0x0E    /* 自定义段结束 */
#define SECTION_ID_CUSTOM               0x0F    /* 自定义段 */

/* 系统限制 (COS3规范) */
#define MAX_MODULES                     32      /* 最大模块数 */
#define MAX_APPS_PER_MODULE             16      /* 每模块最大应用数 */
#define MAX_FUNCTIONS                   256     /* Maximum functions */
#define MAX_CHANNELS                    8       /* Maximum logical channels (COS3 spec) */
#define MAX_IMPORT_MODULES              31      /* Maximum import modules (Table 19) */
#define AID_MAX_LENGTH                  16      /* Maximum AID length */
#define GCOS_MAX_FRAME_DEPTH            64      /* Maximum call stack depth */

/* 操作码范围 (COS3规范表41) */
#define OPCODE_SINGLE_BYTE_MIN          0x00    /* 单字节操作码最小值 */
#define OPCODE_SINGLE_BYTE_MAX          0xFB    /* 单字节操作码最大值 */
#define OPCODE_DOUBLE_BYTE_MIN          0xFC    /* 双字节操作码最小值 */
#define OPCODE_DOUBLE_BYTE_MAX          0xFE    /* 双字节操作码最大值 */
#define OPCODE_RESERVED                 0xFF    /* 保留 */

/* ============================================================================
 * 枚举类型
 * ============================================================================ */

/**
 * @brief GCOS VM 状态 (COS3规范图1)
 */
typedef enum {
    GCOS_STATE_IDLE,                /* 空闲状态 */
    GCOS_STATE_INITIALIZING,        /* 初始化中 */
    GCOS_STATE_RUNNING,             /* 运行中 */
    GCOS_STATE_SUSPENDED,           /* 挂起 */
    GCOS_STATE_EXCEPTION,           /* 异常状态 */
    GCOS_STATE_SHUTDOWN             /* 关闭状态 */
} GCOSState;

/**
 * @brief 模块类型 (COS3规范表11)
 */
typedef enum {
    MODULE_TYPE_APP,                /* 应用模块 */
    MODULE_TYPE_LIB                 /* 库模块 */
} GCOSModuleType;

/**
 * @brief 数据类型 (COS3规范表12、表13)
 */
typedef enum {
    /* 模块数据 (表12) */
    DATA_TYPE_MODULE_GLOBAL,        /* 模块全局数据 - 易失性 */
    DATA_TYPE_MODULE_READONLY,      /* 模块只读数据 - 非易失性,不可修改 */
    DATA_TYPE_MODULE_LOCAL,         /* 模块局部数据 - 易失性,函数范围 */
    DATA_TYPE_MODULE_DOMAIN,        /* 模块域数据 - 非易失性 */
    
    /* 应用数据 (表13) */
    DATA_TYPE_TEMP_STATIC,          /* 临时静态数据 - 易失性 */
    DATA_TYPE_TEMP_DYNAMIC,         /* 临时动态数据 - 易失性 */
    DATA_TYPE_CROSS_DOMAIN,         /* 跨域数据 - 易失性,可共享 */
    DATA_TYPE_PERSISTENT,           /* 持久性数据 - 非易失性,事务保护 */
    DATA_TYPE_APP_DOMAIN,           /* 应用域数据 - 非易失性,事务保护 */
    DATA_TYPE_REF_DOMAIN            /* 引用域数据 - 非易失性,事务保护 */
} GCOSDataType;

/**
 * @brief 存储区类型 (COS3规范表39)
 */
typedef enum {
    STORAGE_VOLATILE,               /* 易失性存储区 */
    STORAGE_NON_VOLATILE            /* 非易失性存储区 */
} GCOSStorageType;

/**
 * @brief 指令操作码分类 (COS3规范附录A)
 */
typedef enum {
    OP_CATEGORY_CONTROL,            /* 控制流指令 (BR, BEQZ, BNEZ等) */
    OP_CATEGORY_ARITHMETIC,         /* 算术运算指令 (ADD, SUB, MUL, DIV等) */
    OP_CATEGORY_LOGIC,              /* 逻辑运算指令 (AND, OR, XOR, NOT等) */
    OP_CATEGORY_MEMORY,             /* 内存访问指令 (LOAD, STORE等) */
    OP_CATEGORY_CONVERSION,         /* 类型转换指令 (CAST, CONVERT等) */
    OP_CATEGORY_COMPARISON,         /* 比较指令 (CMP, TEST等) */
    OP_CATEGORY_STACK,              /* 栈操作指令 (PUSH, POP等) */
    OP_CATEGORY_CALL,               /* 函数调用指令 (CALL, RET等) */
    OP_CATEGORY_TRAP                /* 陷阱指令 (TRAP, ABORT等) */
} GCOSOpCodeCategory;

/**
 * @brief 异常类型 (COS3规范附录D)
 */
typedef enum {
    EXCEPTION_NONE = 0,
    
    /* 栈相关异常 */
    EXCEPTION_STACK_OVERFLOW,       /* 栈溢出 */
    EXCEPTION_STACK_UNDERFLOW,      /* 栈下溢 */
    
    /* 算术异常 */
    EXCEPTION_DIVISION_BY_ZERO,     /* 除零错误 */
    EXCEPTION_ARITHMETIC_OVERFLOW,  /* 算术溢出 */
    
    /* 指令异常 */
    EXCEPTION_INVALID_OPCODE,       /* 无效操作码 */
    EXCEPTION_INVALID_OPERAND,      /* 无效操作数 */
    
    /* 内存异常 */
    EXCEPTION_INVALID_ADDRESS,      /* 无效地址 */
    EXCEPTION_ACCESS_VIOLATION,     /* 访问违例 */
    EXCEPTION_OUT_OF_MEMORY,        /* 内存不足 */
    
    /* 类型异常 */
    EXCEPTION_TYPE_MISMATCH,        /* 类型不匹配 */
    EXCEPTION_ARRAY_BOUNDS,         /* 数组越界 */
    EXCEPTION_NULL_REFERENCE,       /* 空引用 */
    
    /* 事务异常 */
    EXCEPTION_TRANSACTION_ABORT,    /* 事务中止 */
    EXCEPTION_TRANSACTION_NESTING,  /* 事务嵌套过深 */
    EXCEPTION_TRANSACTION_OVERFLOW, /* 事务溢出 */
    
    /* 安全异常 */
    EXCEPTION_SECURITY_VIOLATION,   /* 安全违例 */
    EXCEPTION_UNAUTHORIZED_ACCESS,  /* 未授权访问 */
    
    /* 应用生命周期异常 */
    EXCEPTION_APP_NOT_SELECTED,     /* 应用未选择 */
    EXCEPTION_APP_ALREADY_SELECTED, /* 应用已选择 */
    
    EXCEPTION_MAX
} GCOSExceptionType;

/**
 * @brief 应用生命周期状态 (COS3规范)
 */
typedef enum {
    APP_LIFECYCLE_INSTALLED,        /* 已安装 */
    APP_LIFECYCLE_SELECTABLE,       /* 可选择 */
    APP_LIFECYCLE_SELECTED,         /* 已选择 (正在运行) */
    APP_LIFECYCLE_PERSONALIZED,     /* 已个性化 */
    APP_LIFECYCLE_LOCKED,           /* 已锁定 */
    APP_LIFECYCLE_TERMINATED        /* 已终止 */
} GCOSAppLifecycleState;

/**
 * @brief Application information structure
 */
typedef struct {
    u32 app_id;                     /* Application ID */
    GCOSAppLifecycleState state;    /* Application lifecycle state */
    u8 priority;                    /* Priority level */
    u8 aid_length;                  /* AID length */
    u8 aid[AID_MAX_LENGTH];         /* Application ID bytes */
    u32 runtime_data_offset;        /* Runtime data offset */
    u32 runtime_data_size;          /* Runtime data size */
} GCOSAppInfo;

/**
 * @brief Module information structure
 */
typedef struct {
    u16 module_id;                  /* Module ID */
    u32 function_count;             /* Number of functions */
    u32 import_count;               /* Number of imports */
    u8 app_count;                   /* Number of applications */
    u32 code_offset;                /* Code section offset */
} GCOSModuleInfo;

/**
 * @brief 返回码
 */
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

/**
 * @brief Memory access type for security validation
 */
typedef enum {
    GCOS_MEMORY_ACCESS_STACK = 0,     /* Stack access */
    GCOS_MEMORY_ACCESS_GLOBAL = 1,    /* Global data access */
    GCOS_MEMORY_ACCESS_HEAP = 2,      /* Heap access */
    GCOS_MEMORY_ACCESS_CODE = 3       /* Code access */
} GCOSMemoryAccessType;

/* ============================================================================
 * 兼容性宏定义 (用于简化代码编写)
 * ============================================================================ */

/* GCOSResult 别名 */
#define GCOS_SUCCESS                GCOS_OK
#define GCOS_ERROR_NULL_POINTER     GCOS_ERR_INVALID_PARAM
#define GCOS_ERROR_INVALID_PARAM    GCOS_ERR_INVALID_PARAM
#define GCOS_ERROR_INIT_FAILED      GCOS_ERR_OUT_OF_MEMORY
#define GCOS_ERROR_STACK_OVERFLOW   GCOS_ERR_STACK_OVERFLOW
#define GCOS_ERROR_STACK_UNDERFLOW  GCOS_ERR_STACK_UNDERFLOW
#define GCOS_ERROR_MEMORY_ACCESS    GCOS_ERR_ACCESS_DENIED
#define GCOS_ERROR_INVALID_STATE    GCOS_ERR_INVALID_PARAM
#define GCOS_ERROR_BREAKPOINT_LIMIT GCOS_ERR_OUT_OF_MEMORY
#define GCOS_ERROR_NOT_FOUND        GCOS_ERR_APP_NOT_FOUND
#define GCOS_ERROR_VALIDATION_FAILED GCOS_ERR_INVALID_PARAM

/* GCOSState 别名 */
#define GCOS_VM_STATE_IDLE          GCOS_STATE_IDLE
#define GCOS_VM_STATE_RUNNING       GCOS_STATE_RUNNING
#define GCOS_VM_STATE_SUSPENDED     GCOS_STATE_SUSPENDED
#define GCOS_VM_STATE_ERROR         GCOS_STATE_EXCEPTION
#define GCOS_VM_STATE_EXCEPTION     GCOS_STATE_EXCEPTION

/* GCOSExceptionType 别名 */
#define GCOS_EXCEPTION_NONE                EXCEPTION_NONE
#define GCOS_EXCEPTION_STACK_OVERFLOW      EXCEPTION_STACK_OVERFLOW
#define GCOS_EXCEPTION_STACK_UNDERFLOW     EXCEPTION_STACK_UNDERFLOW
#define GCOS_EXCEPTION_DIVISION_BY_ZERO    EXCEPTION_DIVISION_BY_ZERO
#define GCOS_EXCEPTION_INVALID_OPCODE      EXCEPTION_INVALID_OPCODE
#define GCOS_EXCEPTION_MEMORY_ACCESS       EXCEPTION_ACCESS_VIOLATION
#define GCOS_EXCEPTION_TRANSACTION_ABORT   EXCEPTION_TRANSACTION_ABORT
#define GCOS_EXCEPTION_SECURITY_VIOLATION  EXCEPTION_SECURITY_VIOLATION

/* GCOSAppLifecycleState 别名 (for compatibility) */
#define GCOS_APP_STATE_INSTALLED        APP_LIFECYCLE_INSTALLED
#define GCOS_APP_STATE_SELECTABLE       APP_LIFECYCLE_SELECTABLE
#define GCOS_APP_STATE_SELECTED         APP_LIFECYCLE_SELECTED
#define GCOS_APP_STATE_PERSONALIZED     APP_LIFECYCLE_PERSONALIZED
#define GCOS_APP_STATE_LOCKED           APP_LIFECYCLE_LOCKED
#define GCOS_APP_STATE_TERMINATED       APP_LIFECYCLE_TERMINATED

/* 其他常量 */
#define GCOS_INVALID_INDEX          0xFF
#define GCOS_MAX_MODULES            MAX_MODULES
#define GCOS_MAX_APPS               (MAX_MODULES * MAX_APPS_PER_MODULE)
#define GCOS_MAX_CHANNELS           MAX_CHANNELS

/* ============================================================================
 * 前向声明
 * ============================================================================ */

typedef struct GCOSVM GCOSVM;
typedef struct GCOSModule GCOSModule;
typedef struct GCOSAppInstance GCOSAppInstance;
typedef struct GCOSRuntimeContext GCOSRuntimeContext;

/**
 * @brief 版本信息结构
 */
typedef struct {
    u8 major;     /* 主版本号 */
    u8 minor;     /* 次版本号 */
    u8 patch;     /* 修订号 */
} GCOSVersion;

/* ============================================================================
 * 数据结构 - 基于COS3规范
 * ============================================================================ */

/**
 * @brief AID (Application Identifier) - COS3规范
 */
typedef struct {
    u8 aid[AID_MAX_LENGTH];         /* AID数据 */
    u8 length;                      /* AID长度 (1-16) */
} GCOSAID;

/**
 * @brief 栈帧 (COS3规范3.12)
 * 
 * 用于支持执行器执行函数调用时创建的临时数据结构,管理:
 * - 函数参数
 * - 局部变量
 * - 中间运行结果
 * - 返回值
 * - 调用方法信息
 */
typedef struct {
    u32 return_address;             /* 返回地址 (PC) */
    u32 base_pointer;               /* 基址指针 (BP) */
    u32 frame_size;                 /* 栈帧大小 (字节) */
    u32 local_vars_offset;          /* 局部变量偏移 */
    u32 param_count;                /* 参数数量 */
    u32 operand_stack_base;         /* 操作数栈基址 */
} GCOSStackFrame;

/**
 * @brief 函数头信息 (COS3规范7.2.4.3)
 */
typedef struct {
    u16 function_id;                /* 函数ID */
    u16 param_count;                /* 参数数量 */
    u16 local_var_count;            /* 局部变量数量 */
    u16 max_stack_depth;            /* 最大栈深度 */
    u32 code_offset;                /* 代码偏移 (在模块代码区中的位置) */
    u32 code_size;                  /* 代码大小 (字节) */
    bool is_exported;               /* 是否导出 */
    bool is_imported;               /* 是否导入 */
} GCOSFunctionHeader;

/**
 * @brief 段信息 (COS3规范表17)
 */
typedef struct {
    u8 section_id;                  /* 段标识符 */
    u32 size;                       /* 段内容字节数 */
    const u8 *contents;             /* 段内容指针 */
} GCOSSection;

/**
 * @brief 可加载文件头 (COS3规范表16)
 */
typedef struct {
    u32 sef_type;                   /* 文件类型 ('sef') */
    u32 version;                    /* 版本号 */
    u32 section_count;              /* 段数量 */
    GCOSSection sections[16];       /* 段数组 (最多16个段) */
} GCOSSefFile;

/**
 * @brief 模块信息 (COS3规范7.2)
 */
struct GCOSModule {
    GCOSAID module_aid;             /* 模块AID */
    GCOSModuleType type;            /* 模块类型 (应用/库) */
    u32 version;                    /* 版本号 */
    
    /* 数据区 */
    u8 *global_data;                /* 模块全局数据 (易失性) */
    u32 global_data_size;           /* 全局数据大小 */
    
    const u8 *readonly_data;        /* 模块只读数据 (非易失性,ROM) */
    u32 readonly_data_size;         /* 只读数据大小 */
    
    u8 *domain_data;                /* 模块域数据 (非易失性,堆) */
    u32 domain_data_size;           /* 域数据大小 */
    
    /* 代码区 */
    const u8 *code;                 /* 模块程序代码 (非易失性,ROM) */
    u32 code_size;                  /* 代码大小 */
    
    /* 函数表 */
    GCOSFunctionHeader *functions;  /* 函数头数组 */
    u16 function_count;             /* 函数数量 */
    
    /* 导入/导出表 */
    void *import_table;             /* 导入表 */
    u16 import_count;               /* 导入数量 */
    
    void *export_table;             /* 导出表 */
    u16 export_count;               /* 导出数量 */
    
    /* 应用实例列表 */
    GCOSAppInstance *app_instances[MAX_APPS_PER_MODULE];
    u8 app_instance_count;          /* 应用实例数量 */
    
    bool loaded;                    /* 是否已加载 */
    bool initialized;               /* 是否已初始化 */
};

/**
 * @brief 应用实例 (COS3规范3.7)
 */
struct GCOSAppInstance {
    GCOSAID app_aid;                /* 应用AID */
    u16 module_index;               /* 所属模块索引 */
    GCOSAppLifecycleState lifecycle;/* 生命周期状态 */
    
    /* 应用数据 (堆上,非易失性) */
    u8 *app_domain_data;            /* 应用域数据 */
    u32 app_domain_data_size;       /* 应用域数据大小 */
    
    u8 *ref_domain_data;            /* 引用域数据 */
    u32 ref_domain_data_size;       /* 引用域数据大小 */
    
    u8 *persistent_data;            /* 持久性数据 */
    u32 persistent_data_size;       /* 持久性数据大小 */
    
    /* 运行时数据 (每个通道独立,易失性) */
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
 * @brief 事务上下文 (COS3规范)
 */
typedef struct {
    bool active;                    /* 事务是否激活 */
    bool in_transaction;            /* 是否在事务中 */
    u8 transaction_depth;           /* 事务嵌套深度 */
    u8 *backup_data;                /* 备份数据指针 */
    u32 backup_size;                /* 备份大小 */
    u32 checkpoint_count;           /* 检查点数量 */
    u16 nesting_level;              /* 嵌套层级 */
} GCOTransactionContext;

/**
 * @brief 安全管理上下文 (COS3规范)
 */
typedef struct {
    u8 current_domain;              /* 当前运行域 */
    u8 authorization_table[256];    /* 授权表 */
    u32 authorization_table_size;   /* 授权表大小 */
} GCOSSecurityContext;

/**
 * @brief 运行时上下文 (COS3规范8.1.1)
 * 
 * 包含执行器运行时所需的所有数据环境:
 * - 执行器栈
 * - 间接访问变量栈
 * - 全局数据区
 * - 堆
 * - 程序计数器
 * - 当前应用/模块信息
 */
struct GCOSRuntimeContext {
    /* 执行器栈 (易失性,栈单元4字节) */
    u32 executor_stack[GCOS_EXECUTOR_STACK_SIZE];
    u32 stack_pointer;              /* 栈指针 (SP) */
    u32 base_pointer;               /* 基址指针 (BP) */
    
    /* 栈帧栈 */
    GCOSStackFrame frame_stack[64]; /* 最大64层调用 */
    u32 frame_top;                  /* 栈帧栈顶 */
    
    /* 间接访问变量栈 (易失性,栈单元16字节) */
    u8 indirect_stack[GCOS_INDIRECT_STACK_SIZE][GCOS_INDIRECT_UNIT_SIZE];
    u32 indirect_stack_pointer;     /* 间接栈指针 */
    
    /* 全局数据区 (易失性) */
    u8 global_data[GCOS_GLOBAL_DATA_SIZE];
    u32 global_data_used;           /* 已使用大小 */
    
    /* 堆 (非易失性) */
    u8 heap[GCOS_HEAP_SIZE];
    u32 heap_used;                  /* 堆已使用大小 */
    
    /* Module code area (non-volatile) */
    u8 module_code[GCOS_MODULE_CODE_SIZE];  /* Module code storage */
    u32 code_size;                  /* Code area used size */
    
    /* Program counter */
    u32 program_counter;            /* PC - current instruction address */
    
    /* 当前运行上下文 */
    GCOSModule *current_module;     /* 当前模块 */
    GCOSAppInstance *current_app;   /* 当前应用实例 */
    u8 current_channel;             /* 当前逻辑通道 */
    
    /* 事务上下文 */
    GCOTransactionContext transaction;
    
    /* 异常状态 */
    GCOSExceptionType exception;    /* 当前异常类型 */
    u32 exception_address;          /* 异常发生地址 */
};

/**
 * @brief GCOS VM 主结构
 */
struct GCOSVM {
    /* 版本信息 */
    struct {
        u8 major;                   /* 主版本号 */
        u8 minor;                   /* 次版本号 */
        u8 patch;                   /* 修订号 */
    } version;
    
    /* VM 状态 */
    GCOSState state;                /* VM状态 */
    u32 uptime;                     /* 运行时间 (毫秒) */
    
    /* 运行时上下文 */
    GCOSRuntimeContext runtime;     /* 运行时上下文 */
    
    /* 事务上下文 (顶层访问) */
    GCOTransactionContext transaction;
    
    /* 安全管理上下文 */
    struct {
        u8 current_domain;          /* 当前运行域 */
        u8 authorization_table[256];/* 授权表 */
        u32 authorization_table_size; /* 授权表大小 */
    } security;
    
    /* 模块管理 */
    GCOSModule modules[MAX_MODULES];/* 模块数组 */
    u8 module_count;                /* 已加载模块数 */
    u8 current_module_index;        /* 当前模块索引 */
    
    /* 应用管理 */
    GCOSAppInstance *apps[MAX_MODULES * MAX_APPS_PER_MODULE];
    u16 app_count;                  /* 已安装应用数 */
    u8 current_app_index;           /* 当前应用索引 */
    
    /* 通道管理 */
    struct {
        u8 selected_app_index;      /* 该通道选择的应用索引 */
        bool active;                /* 通道是否激活 */
    } channels[MAX_CHANNELS];
    u8 current_channel;             /* 当前通道 */
    u8 active_channels;             /* 激活的通道数 */
    
    /* 统计信息 */
    struct {
        u64 instructions_executed;  /* 已执行指令数 */
        u64 function_calls;         /* 函数调用次数 */
        u32 gc_cycles;              /* GC周期数 */
        u32 exceptions_handled;     /* 异常处理次数 */
        u32 transactions_committed; /* 事务提交次数 */
        u32 transactions_aborted;   /* 事务中止次数 */
    } stats;
    
    /* 扩展统计 */
    u64 total_execution_time_us;    /* 总执行时间（微秒）*/
    
    /* 配置 */
    struct {
        bool enable_debug;          /* 启用调试模式 */
        bool enable_trace;          /* 启用指令追踪 */
        bool enable_security;       /* 启用安全检查 */
        u32 max_execution_time;     /* 最大执行时间 (毫秒, 0=无限制) */
        u32 stack_guard_size;       /* 栈保护区域大小 */
    } config;
};

/* ============================================================================
 * API 函数声明
 * ============================================================================ */

/* --- VM 生命周期管理 --- */

/**
 * @brief 创建 GCOS VM 实例
 * @return VM 指针, NULL 表示失败
 */
GCOSVM* gcos_vm_create(void);

/**
 * @brief 销毁 GCOS VM 实例
 * @param vm VM 指针
 * @return GCOS_OK 成功, 其他错误码
 */
GCOSResult gcos_vm_destroy(GCOSVM *vm);

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

/* --- 模块管理 --- */

/**
 * @brief 加载模块 (从 SEF 文件)
 * @param vm VM 指针
 * @param sef_data SEF 文件数据
 * @param sef_size SEF 文件大小
 * @param[out] module_index 输出的模块索引
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_load_module(GCOSVM *vm, const u8 *sef_data, u32 sef_size, u8 *module_index);

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

/* --- 应用管理 --- */

/**
 * @brief 安装应用实例
 * @param vm VM 指针
 * @param module_index 模块索引
 * @param app_aid 应用 AID
 * @param[out] app_instance 输出的应用实例指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_install_app(GCOSVM *vm, u8 module_index, const GCOSAID *app_aid, GCOSAppInstance **app_instance);

/**
 * @brief 卸载应用实例
 * @param vm VM 指针
 * @param app 应用实例指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_uninstall_app(GCOSVM *vm, GCOSAppInstance *app);

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
GCOSResult gcos_vm_execute_apdu(GCOSVM *vm, u8 channel, const u8 *apdu, u32 apdu_len, u8 *response, u32 *response_len);

/* --- 执行器控制 --- */

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

/* --- 事务管理 --- */

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
 * @brief 中止事务 (回滚)
 * @param vm VM 指针
 * @return GCOS_OK 成功
 */
GCOSResult gcos_vm_transaction_abort(GCOSVM *vm);

/* --- 数据访问 --- */

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
GCOSResult gcos_vm_read_app_data(GCOSVM *vm, GCOSAppInstance *app, GCOSDataType data_type, u32 offset, u8 *buffer, u32 size);

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
GCOSResult gcos_vm_write_app_data(GCOSVM *vm, GCOSAppInstance *app, GCOSDataType data_type, u32 offset, const u8 *buffer, u32 size);

/* --- 查询接口 --- */

/**
 * @brief 获取 VM 状态
 * @param vm VM 指针
 * @return VM 状态
 */
GCOSState gcos_vm_get_state(const GCOSVM *vm);

/**
 * @brief 获取当前异常
 * @param vm VM 指针
 * @return 异常类型
 */
GCOSExceptionType gcos_vm_get_exception(const GCOSVM *vm);

/**
 * @brief 获取异常描述字符串
 * @param exception 异常类型
 * @return 描述字符串
 */
const char* gcos_vm_exception_to_string(GCOSExceptionType exception);

/**
 * @brief 获取状态描述字符串
 * @param state VM状态
 * @return 描述字符串
 */
const char* gcos_vm_state_to_string(GCOSState state);

/**
 * @brief 栈操作 - 压栈
 * @param vm VM指针
 * @param value 要压入的值
 * @return GCOS_OK 成功，其他值失败
 */
GCOSResult gcos_vm_stack_push(GCOSVM *vm, u32 value);

/**
 * @brief 栈操作 - 弹栈
 * @param vm VM指针
 * @param[out] value 输出的值
 * @return GCOS_OK 成功，其他值失败
 */
GCOSResult gcos_vm_stack_pop(GCOSVM *vm, u32 *value);

/**
 * @brief 堆分配（返回偏移地址）
 * @param vm VM指针
 * @param size 分配大小
 * @return 偏移地址，0表示失败
 */
u32 gcos_vm_heap_alloc(GCOSVM *vm, u32 size);

/**
 * @brief 打印VM信息
 * @param vm VM指针
 */
void gcos_vm_print_info(const GCOSVM *vm);

/**
 * @brief 验证VM状态一致性
 * @param vm VM指针
 * @return true 一致，false 不一致
 */
bool gcos_vm_validate(const GCOSVM *vm);

/**
 * @brief 打印内存统计信息
 * @param vm VM指针
 */
void gcos_memory_print_stats(const GCOSVM *vm);

/**
 * @brief 获取统计信息
 * @param vm VM 指针
 * @param[out] instructions_executed 已执行指令数
 * @param[out] function_calls 函数调用次数
 */
void gcos_vm_get_stats(const GCOSVM *vm, u64 *instructions_executed, u64 *function_calls);

/* --- 调试支持 --- */

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

/* --- 指令集执行 --- */

/**
 * @brief Execute a single instruction
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_instruction_execute(GCOSVM *vm);

/* --- SEF Loader --- */

/**
 * @brief Load SEF (Secure Executable Format) file
 * @param vm VM instance
 * @param sef_data SEF file data
 * @param sef_size SEF file size
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_loader_load_sef(GCOSVM *vm, const u8 *sef_data, u32 sef_size);

/**
 * @brief Validate loaded module
 * @param vm VM instance
 * @param module_index Module index
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_loader_validate_module(const GCOSVM *vm, u8 module_index);

/**
 * @brief Get module information
 * @param vm VM instance
 * @param module_index Module index
 * @param[out] info Module information structure
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_loader_get_module_info(const GCOSVM *vm, u8 module_index, 
                                       GCOSModuleInfo *info);

/* --- Transaction Management --- */

/**
 * @brief Begin transaction
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_transaction_begin(GCOSVM *vm);

/**
 * @brief Commit transaction
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_transaction_commit(GCOSVM *vm);

/**
 * @brief Abort transaction (rollback)
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_transaction_abort(GCOSVM *vm);

/**
 * @brief Check if in transaction
 * @param vm VM instance
 * @return true if in transaction, false otherwise
 */
bool gcos_vm_in_transaction(const GCOSVM *vm);

/**
 * @brief Get transaction depth
 * @param vm VM instance
 * @return Transaction depth
 */
u8 gcos_vm_get_transaction_depth(const GCOSVM *vm);

/* --- Application Manager --- */

/**
 * @brief Install application
 * @param vm VM instance
 * @param aid Application ID
 * @param aid_length AID length
 * @param install_data Installation parameters
 * @param install_data_size Installation data size
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_install_app(GCOSVM *vm, const u8 *aid, u8 aid_length,
                               const u8 *install_data, u32 install_data_size);

/**
 * @brief Delete application
 * @param vm VM instance
 * @param app_index Application index
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_delete_app(GCOSVM *vm, u8 app_index);

/**
 * @brief Select application
 * @param vm VM instance
 * @param aid Application ID
 * @param aid_length AID length
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_select_app(GCOSVM *vm, const u8 *aid, u8 aid_length);

/**
 * @brief Deselect current application
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_deselect_app(GCOSVM *vm);

/**
 * @brief Personalize application
 * @param vm VM instance
 * @param app_index Application index
 * @param personalization_data Personalization data
 * @param data_size Data size
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_personalize_app(GCOSVM *vm, u8 app_index,
                                   const u8 *personalization_data, u32 data_size);

/**
 * @brief Lock application
 * @param vm VM instance
 * @param app_index Application index
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_lock_app(GCOSVM *vm, u8 app_index);

/**
 * @brief Get application information
 * @param vm VM instance
 * @param app_index Application index
 * @param[out] info Application information structure
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_get_app_info(const GCOSVM *vm, u8 app_index, GCOSAppInfo *info);

/**
 * @brief Switch to channel
 * @param vm VM instance
 * @param channel Channel number
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_switch_channel(GCOSVM *vm, u8 channel);

/**
 * @brief Activate channel
 * @param vm VM instance
 * @param channel Channel number
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_activate_channel(GCOSVM *vm, u8 channel);

/**
 * @brief Deactivate channel
 * @param vm VM instance
 * @param channel Channel number
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_deactivate_channel(GCOSVM *vm, u8 channel);

/**
 * @brief Get active channel count
 * @param vm VM instance
 * @return Active channel count
 */
u8 gcos_vm_get_active_channel_count(const GCOSVM *vm);

/**
 * @brief Get current channel
 * @param vm VM instance
 * @return Current channel number
 */
u8 gcos_vm_get_current_channel(const GCOSVM *vm);

/**
 * @brief Get selected application on current channel
 * @param vm VM instance
 * @return Selected app instance or NULL
 */
GCOSAppInstance* gcos_vm_get_selected_app(GCOSVM *vm);

/* --- Security Management --- */

/**
 * @brief Initialize security manager
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_init(GCOSVM *vm);

/**
 * @brief Set current domain
 * @param vm VM instance
 * @param domain_id Domain ID
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_set_domain(GCOSVM *vm, u8 domain_id);

/**
 * @brief Get current domain
 * @param vm VM instance
 * @return Current domain ID
 */
u8 gcos_security_get_current_domain(const GCOSVM *vm);

/**
 * @brief Grant access to interface
 * @param vm VM instance
 * @param interface_id Interface ID
 * @param domain_mask Domain mask
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_grant_access(GCOSVM *vm, u8 interface_id, u8 domain_mask);

/**
 * @brief Revoke access to interface
 * @param vm VM instance
 * @param interface_id Interface ID
 * @param domain_mask Domain mask
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_revoke_access(GCOSVM *vm, u8 interface_id, u8 domain_mask);

/**
 * @brief Check permission for interface
 * @param vm VM instance
 * @param interface_id Interface ID
 * @return true if authorized, false otherwise
 */
bool gcos_security_check_permission(const GCOSVM *vm, u8 interface_id);

/**
 * @brief Validate memory access
 * @param vm VM instance
 * @param address Memory address
 * @param size Access size
 * @param access_type Access type
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_validate_memory_access(const GCOSVM *vm, u32 address, u32 size,
                                                GCOSMemoryAccessType access_type);

/**
 * @brief Verify caller authorization
 * @param vm VM instance
 * @param caller_address Caller address
 * @param target_address Target address
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_verify_caller(const GCOSVM *vm, u16 caller_address,
                                       u16 target_address);

/**
 * @brief Setup application isolation
 * @param vm VM instance
 * @param app_index Application index
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_security_setup_app_isolation(GCOSVM *vm, u8 app_index);

/**
 * @brief Dump authorization table (debug)
 * @param vm VM instance
 */
void gcos_security_dump_authorization_table(const GCOSVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_VM_H */
