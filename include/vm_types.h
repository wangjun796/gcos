#ifndef VM_TYPES_H
#define VM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 基本类型定义
 * ============================================================================ */

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef int16_t   s16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint64_t  u64;
typedef int64_t   s64;

/* ============================================================================
 * 常量定义
 * ============================================================================ */

/* 栈大小配置 */
#define VM_EXECUTOR_STACK_SIZE      256     /* 执行器栈大小（单元数） */
#define VM_INDIRECT_VAR_STACK_SIZE  64      /* 间接访问变量栈大小（单元数） */
#define VM_GLOBAL_DATA_SIZE         4096    /* 全局数据区大小（字节） */
#define VM_HEAP_SIZE                8192    /* 堆大小（字节） */
#define VM_MODULE_CODE_SIZE         16384   /* 模块程序区大小（字节） */

/* 栈单元大小 */
#define VM_STACK_UNIT_SIZE          4       /* 执行器栈单元：4字节 */
#define VM_INDIRECT_UNIT_SIZE       16      /* 间接访问变量栈单元：16字节 */

/* 文件类型标识符 */
#define FILE_TYPE_ASM               0x0061736D  /* "asm" - 中间文件 */
#define FILE_TYPE_LINK              0x6C696E6B  /* "link" - 链接文件 */
#define FILE_TYPE_SEF               0x00736566  /* "sef" - 可加载文件 */

/* 段标识符 - 根据 COS3 规范 7.3.3 */
#define SECTION_ID_FIRST            0x01    /* 首段 */
#define SECTION_ID_IMPORT           0x02    /* 导入段 */
#define SECTION_ID_FUNCTION         0x03    /* 函数段 */
#define SECTION_ID_APP              0x04    /* 应用段 */
#define SECTION_ID_GLOBAL           0x05    /* 全局段 */
#define SECTION_ID_EXPORT           0x06    /* 导出段 */
#define SECTION_ID_ELEMENT          0x07    /* 元素段 */
#define SECTION_ID_DATA             0x08    /* 数据段 */
#define SECTION_ID_CODE             0x09    /* 代码段 */
#define SECTION_ID_CUSTOM           0x0F    /* 自定义段 */

/* 最大数量限制 */
#define MAX_MODULES                 32      /* 最大模块数 */
#define MAX_APPS_PER_MODULE         16      /* 每模块最大应用数 */
#define MAX_FUNCTIONS               256     /* 最大函数数 */
#define MAX_CHANNELS                8       /* 最大逻辑通道数 */

/* ============================================================================
 * 枚举类型
 * ============================================================================ */

/**
 * @brief 虚拟机状态
 */
typedef enum {
    VM_STATE_IDLE,              /* 空闲状态 */
    VM_STATE_RUNNING,           /* 运行中 */
    VM_STATE_SUSPENDED,         /* 挂起 */
    VM_STATE_ERROR,             /* 错误状态 */
    VM_STATE_EXCEPTION          /* 异常状态 */
} VMState;

/**
 * @brief 模块类型
 */
typedef enum {
    MODULE_TYPE_APP,            /* 应用模块 */
    MODULE_TYPE_LIB             /* 库模块 */
} ModuleType;

/**
 * @brief 数据类型
 */
typedef enum {
    DATA_TYPE_GLOBAL,           /* 模块全局数据 */
    DATA_TYPE_READONLY,         /* 模块只读数据 */
    DATA_TYPE_LOCAL,            /* 模块局部数据 */
    DATA_TYPE_DOMAIN,           /* 模块域数据 */
    DATA_TYPE_TEMP_STATIC,      /* 临时静态数据 */
    DATA_TYPE_TEMP_DYNAMIC,     /* 临时动态数据 */
    DATA_TYPE_CROSS_DOMAIN,     /* 跨域数据 */
    DATA_TYPE_PERSISTENT,       /* 持久性数据 */
    DATA_TYPE_APP_DOMAIN,       /* 应用域数据 */
    DATA_TYPE_REF_DOMAIN        /* 引用域数据 */
} DataType;

/**
 * @brief 存储区类型
 */
typedef enum {
    STORAGE_VOLATILE,           /* 易失性存储区 */
    STORAGE_NON_VOLATILE        /* 非易失性存储区 */
} StorageType;

/**
 * @brief 指令操作码分类
 */
typedef enum {
    OP_CATEGORY_CONTROL,        /* 控制流指令 */
    OP_CATEGORY_ARITHMETIC,     /* 算术运算指令 */
    OP_CATEGORY_LOGIC,          /* 逻辑运算指令 */
    OP_CATEGORY_MEMORY,         /* 内存访问指令 */
    OP_CATEGORY_CONVERSION,     /* 类型转换指令 */
    OP_CATEGORY_COMPARISON,     /* 比较指令 */
    OP_CATEGORY_TRAP            /* 陷阱指令 */
} OpCodeCategory;

/**
 * @brief 异常类型
 */
typedef enum {
    EXCEPTION_NONE = 0,
    EXCEPTION_STACK_OVERFLOW,       /* 栈溢出 */
    EXCEPTION_STACK_UNDERFLOW,      /* 栈下溢 */
    EXCEPTION_DIVISION_BY_ZERO,     /* 除零错误 */
    EXCEPTION_INVALID_OPCODE,       /* 无效操作码 */
    EXCEPTION_INVALID_ADDRESS,      /* 无效地址 */
    EXCEPTION_ACCESS_VIOLATION,     /* 访问违例 */
    EXCEPTION_OUT_OF_MEMORY,        /* 内存不足 */
    EXCEPTION_TYPE_MISMATCH,        /* 类型不匹配 */
    EXCEPTION_ARRAY_BOUNDS,         /* 数组越界 */
    EXCEPTION_NULL_REFERENCE,       /* 空引用 */
    EXCEPTION_TRANSACTION_ABORT,    /* 事务中止 */
    EXCEPTION_SECURITY_VIOLATION,   /* 安全违例 */
    EXCEPTION_MAX
} ExceptionType;

/**
 * @brief 应用生命周期状态
 */
typedef enum {
    APP_LIFECYCLE_INSTALLED,    /* 已安装 */
    APP_LIFECYCLE_SELECTABLE,   /* 可选择 */
    APP_LIFECYCLE_SELECTED,     /* 已选择 */
    APP_LIFECYCLE_PERSONALIZED, /* 已个性化 */
    APP_LIFECYCLE_LOCKED,       /* 已锁定 */
    APP_LIFECYCLE_TERMINATED    /* 已终止 */
} AppLifecycleState;

/* ============================================================================
 * 数据结构
 * ============================================================================ */

/**
 * @brief AID (Application Identifier)
 */
typedef struct {
    u8 aid[16];                 /* AID数据 */
    u8 length;                  /* AID长度 */
} AID;

/**
 * @brief 栈帧
 */
typedef struct {
    u32 return_address;         /* 返回地址 */
    u32 base_pointer;           /* 基址指针 */
    u32 frame_size;             /* 栈帧大小 */
    u32 local_vars_offset;      /* 局部变量偏移 */
} StackFrame;

/**
 * @brief 函数头信息 (对应规范中 CODE 数据结构)
 */
typedef struct {
    u16 param_count;            /* 参数数量 */
    u16 local_var_count;        /* 局部变量数量 */
    u16 max_stack_depth;        /* 最大栈深度 (对应 opstack) */
    u16 max_indstack_depth;     /* 最大间接栈深度 (对应 indstack) */
    u32 code_offset;            /* 代码在模块代码区的偏移 */
    u32 code_size;              /* 字节码大小 */
} FunctionHeader;

/**
 * @brief 模块信息
 */
typedef struct {
    AID module_aid;             /* 模块AID */
    ModuleType type;            /* 模块类型 */
    u32 version;                /* 版本号 */
    
    /* 数据区 */
    u8 *global_data;            /* 模块全局数据 */
    u8 *readonly_data;          /* 模块只读数据 */
    u8 *domain_data;            /* 模块域数据 */
    
    /* 代码区 */
    u8 *code;                   /* 模块程序代码 */
    u32 code_size;              /* 代码大小 */
    
    /* 函数表 */
    FunctionHeader *functions;  /* 函数头数组 */
    u16 function_count;         /* 函数数量 */
    
    /* 导入/导出表 */
    void *import_table;         /* 导入表 */
    void *export_table;         /* 导出表 */
    
    bool loaded;                /* 是否已加载 */
} Module;

/**
 * @brief 应用实例
 */
typedef struct {
    AID app_aid;                /* 应用AID */
    u16 module_index;           /* 所属模块索引 */
    AppLifecycleState lifecycle;/* 生命周期状态 */
    
    /* 应用数据 */
    u8 *app_domain_data;        /* 应用域数据 */
    u8 *ref_domain_data;        /* 引用域数据 */
    u8 *persistent_data;        /* 持久性数据 */
    
    /* 运行时数据（每个通道独立） */
    struct {
        u8 *temp_dynamic_data;  /* 临时动态数据 */
        u8 *global_data_copy;   /* 模块全局数据副本 */
        bool active;            /* 是否激活 */
    } channel_data[MAX_CHANNELS];
    
    bool installed;             /* 是否已安装 */
} AppInstance;

/**
 * @brief 事务上下文
 */
typedef struct {
    bool active;                /* 事务是否激活 */
    u8 *backup_data;            /* 备份数据 */
    u32 backup_size;            /* 备份大小 */
    u32 checkpoint_count;       /* 检查点数量 */
} TransactionContext;

#ifdef __cplusplus
}
#endif

#endif /* VM_TYPES_H */
