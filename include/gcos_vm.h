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
#define SECTION_ID_IMPORT               0x02    /* Import section (optional) */
#define SECTION_ID_FUNCTION             0x03    /* Function section (required) */
#define SECTION_ID_APP                  0x04    /* Application section (optional) */
#define SECTION_ID_GLOBAL               0x05    /* Global section (required) */
#define SECTION_ID_EXPORT               0x06    /* Export section (optional) */
#define SECTION_ID_ELEMENT              0x07    /* Element section (optional) */
#define SECTION_ID_DATA                 0x08    /* Data section (optional) */
#define SECTION_ID_CODE                 0x09    /* Code section (required) */
#define SECTION_ID_CUSTOM_START         0x0A    /* Custom section start */
#define SECTION_ID_CUSTOM_END           0x0E    /* Custom section end */
#define SECTION_ID_CUSTOM               0x0F    /* Custom section */

/* System limits (COS3 specification) */
#define MAX_MODULES                     32      /* Maximum number of modules */
#define MAX_APPS                        64      /* Maximum applications (including ISD) */
#define MAX_APPS_PER_MODULE             16      /* Maximum applications per module */
#define MAX_IMPORTS                     8       /* ⭐ NEW: Maximum imports per module (like cref) */
#define MAX_EXPORTS                     32      /* ⭐ NEW: Maximum exports per module */
#define MAX_FUNCTIONS                   256     /* Maximum functions */
#define MAX_CHANNELS                    8       /* Maximum logical channels (COS3 spec) */
#define MAX_IMPORT_MODULES              31      /* Maximum import modules (Table 19) */
#define AID_MAX_LENGTH                  16      /* Maximum AID length */
#define GCOS_MAX_FRAME_DEPTH            64      /* Maximum call stack depth */

/* Application IDs */
#define APP_FIRST                       0       /* ISD's ID (Initial Security Domain) */
#define APP_NULL                        0xFF    /* Invalid application ID */

/* Buffer sizes */
#define APDU_BUFFER_SIZE                260     /* APDU buffer size */
#define RESPONSE_BUFFER_SIZE           260     /* Response buffer size */

/* Opcode ranges (COS3 specification Table 41) */
#define OPCODE_SINGLE_BYTE_MIN          0x00    /* Single-byte opcode minimum */
#define OPCODE_SINGLE_BYTE_MAX          0xFB    /* Single-byte opcode maximum */
#define OPCODE_DOUBLE_BYTE_MIN          0xFC    /* Double-byte opcode minimum */
#define OPCODE_DOUBLE_BYTE_MAX          0xFE    /* Double-byte opcode maximum */
#define OPCODE_RESERVED                 0xFF    /* Reserved */

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

/* Application Manager Error Codes */
#define GCOS_ERROR_APP_TABLE_FULL   ((GCOSResult)0x8001)
#define GCOS_ERROR_APP_NOT_FOUND    ((GCOSResult)0x8002)
#define GCOS_ERROR_APP_NOT_SELECTABLE ((GCOSResult)0x8003)
#define GCOS_ERROR_CANNOT_DELETE_ISD ((GCOSResult)0x8004)

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
 * @brief Connection type (T=0 or T=CL)
 * 
 * Used to distinguish between contacted (T=0) and contactless (T=CL) protocols.
 * Port 9000 -> T=0, Port 9900 -> T=CL
 */
typedef enum {
    GCOS_CONN_TYPE_T0 = 0,   /**< T=0 protocol (contacted, port 9000) */
    GCOS_CONN_TYPE_T5 = 2    /**< T=CL protocol (contactless, port 9900) */
} GCOSConnType;

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
 * @brief Module lifecycle states (similar to cref PackageEntry status)
 */
typedef enum {
    MODULE_NOT_LOADED = 0x00,       /* Not loaded */
    MODULE_LOADED = 0x01,           /* Loaded but not verified */
    MODULE_VERIFIED = 0x02,         /* Verified, ready to create app instances */
    MODULE_ERROR = 0xFF             /* Load error */
} GCOSModuleState;

/**
 * @brief Import dependency information (based on COS3 Specification Table 22)
 * 
 * Corresponds to IMPORT_MODULE_ITEMS in SEF file import section.
 */
typedef struct {
    u32 module_version;               /* Required module version (u32 format per Appendix B) */
    GCOSAID module_aid;               /* Dependency module AID */
    bool resolved;                    /* Whether dependency is resolved */
    u8 resolved_module_id;            /* Resolved module ID (0xFF if not resolved) */
} GCOSImportInfo;

/**
 * @brief Section types as per COS3 Specification Table 18
 */
typedef enum {
    SECTION_UNKNOWN = 0x00,
    SECTION_HEADER = 0x01,          /* 首段 - 模块信息、导入信息和段信息 */
    SECTION_IMPORT = 0x02,          /* 导入段 - 导入函数信息 */
    SECTION_FUNCTION = 0x03,        /* 函数段 - 内部函数空间信息 */
    SECTION_APPLICATION = 0x04,     /* 应用段 - 可执行模块安装信息 */
    SECTION_GLOBAL = 0x05,          /* 全局段 - 模块数据、应用数据空间信息 */
    SECTION_EXPORT = 0x06,          /* 导出段 - 导出函数信息 */
    SECTION_ELEMENT = 0x07,         /* 元素段 - 被引用的函数索引信息 */
    SECTION_DATA = 0x08,            /* 数据段 - 模块数据、应用数据初始值信息 */
    SECTION_CODE = 0x09,            /* 代码段 - 模块程序代码 */
    SECTION_RESERVED_A = 0x0A,      /* 保留段 */
    SECTION_RESERVED_B = 0x0B,
    SECTION_RESERVED_C = 0x0C,
    SECTION_RESERVED_D = 0x0D,
    SECTION_RESERVED_E = 0x0E,
    SECTION_CUSTOM = 0x0F           /* 自定义段 */
} GCOSSectionType;

/**
 * @brief Section parsing sub-states
 * 
 * Tracks progress within a section for streaming parsing.
 * Each section may span multiple APDUs, so we need to track:
 * 1. Which section we're parsing
 * 2. Which item within the section we're on
 * 3. How many bytes of current item received so far
 */
typedef enum {
    SECTION_PARSE_IDLE = 0x00,              /* Not parsing any section */
    
    /* Section header parsing (common to all sections) */
    SECTION_PARSE_HEADER = 0x01,            /* Parsing section_id + size (5 bytes) */
    
    /* 首段 (SECTION_HEADER) sub-states */
    SECTION_HEADER_PARSE_SEF_INFO = 0x10,   /* Parsing sef_info structure */
    SECTION_HEADER_PARSE_SEF_LEN = 0x11,    /* Parsing sef_len (4 bytes) */
    SECTION_HEADER_PARSE_IMPORT_COUNT = 0x12, /* Parsing import_module_count (1 byte) */
    SECTION_HEADER_PARSE_IMPORT_FUNC_COUNT = 0x13, /* Parsing import_function_count (2 bytes) */
    SECTION_HEADER_PARSE_APP_NUM = 0x14,    /* Parsing app_num (1 byte) */
    SECTION_HEADER_PARSE_SEC_FUNC_LEN = 0x15, /* Parsing sec_func_len (2 bytes) */
    SECTION_HEADER_PARSE_SEC_ELEM_LEN = 0x16, /* Parsing sec_elem_len (2 bytes) */
    SECTION_HEADER_PARSE_SEC_DATA_LEN = 0x17, /* Parsing sec_data_len (2 bytes) */
    SECTION_HEADER_PARSE_SEC_CODE_LEN = 0x18, /* Parsing sec_code_len (4 bytes) */
    
    /* 导入段 (SECTION_IMPORT) sub-states */
    SECTION_IMPORT_PARSE_MODULE_COUNT = 0x20, /* Parsing import_module_count (1 byte) */
    SECTION_IMPORT_PARSE_FUNC_COUNT = 0x21,   /* Parsing import_function_count (2 bytes) */
    SECTION_IMPORT_PARSE_MODULE_ITEMS = 0x22, /* Parsing import_module_items array */
    SECTION_IMPORT_PARSE_FUNC_ITEMS = 0x23,   /* Parsing import_function_items array */
    
    /* 函数段 (SECTION_FUNCTION) sub-states */
    SECTION_FUNCTION_PARSE_CODE_SIZES = 0x30, /* Parsing code_size array */
    
    /* 应用段 (SECTION_APPLICATION) sub-states */
    SECTION_APPLICATION_PARSE_APP_NUM = 0x40, /* Parsing app_num (1 byte) */
    SECTION_APPLICATION_PARSE_APP_INFO = 0x41, /* Parsing app_info array */
    
    /* 全局段 (SECTION_GLOBAL) sub-states */
    SECTION_GLOBAL_PARSE_BASES = 0x50,        /* Parsing base addresses (6 x u16) */
    
    /* 导出段 (SECTION_EXPORT) sub-states */
    SECTION_EXPORT_PARSE_FUNC_IDXS = 0x60,    /* Parsing function_idxs array */
    
    /* 元素段 (SECTION_ELEMENT) sub-states */
    SECTION_ELEMENT_PARSE_FUNC_IDXS = 0x70,   /* Parsing function_idxs array */
    
    /* 数据段 (SECTION_DATA) sub-states */
    SECTION_DATA_PARSE_SIZES = 0x80,          /* Parsing size fields */
    SECTION_DATA_PARSE_RODATA = 0x81,         /* Parsing rodata */
    SECTION_DATA_PARSE_RWDATA = 0x82,         /* Parsing rwdata */
    SECTION_DATA_PARSE_MODDATA = 0x83,        /* Parsing moddata */
    SECTION_DATA_PARSE_APPDATA = 0x84,        /* Parsing appdata */
    
    /* 代码段 (SECTION_CODE) sub-states */
    SECTION_CODE_PARSE_BYTECODE = 0x90,       /* Parsing bytecode */
    
    /* 自定义段 (SECTION_CUSTOM) sub-states */
    SECTION_CUSTOM_PARSE_DATA = 0xF0          /* Parsing custom data */
} GCOSSectionParseState;

/**
 * @brief LOAD command state machine (based on COS3 Specification Section 8.2.1)
 * 
 * Enhanced to support streaming section parsing across multiple APDUs.
 */
typedef enum {
    LOAD_STATE_IDLE = 0x00,              /* Idle - no active load session */
    LOAD_STATE_INITIALIZATION = 0x01,    /* Initialization phase (INSTALL FOR LOAD, P1=0x00) */
    LOAD_STATE_LOADING_BLOCKS = 0x02,    /* Loading SEF data blocks (LOAD BLOCKS, P1=0x01) */
    LOAD_STATE_PARSING_SECTIONS = 0x03,  /* Parsing sections from buffered SEF data */
    LOAD_STATE_LINKING = 0x04,           /* Linking imports and resolving references */
    LOAD_STATE_FINALIZATION = 0x05,      /* Finalization phase (FINALIZE, P1=0x02) */
    LOAD_STATE_ERROR = 0xFF              /* Error state */
} GCOSLoadState;

/**
 * @brief Item parsing context for streaming section parsing
 * 
 * Used to track progress when parsing array items that may span APDUs.
 */
typedef struct {
    u32 item_index;                   /* Current item index in array */
    u32 item_count;                   /* Total number of items */
    u32 item_size;                    /* Size of each item (bytes) */
    u32 bytes_received;               /* Bytes received for current item */
    u8 item_buffer[32];               /* Buffer for accumulating current item */
} GCOSItemParseContext;

/**
 * @brief LOAD context (maintains state across multiple APDUs)
 * 
 * Tracks LOAD command progress with enhanced section parsing state machine.
 * Supports streaming parsing where sections may span multiple APDUs.
 */
typedef struct {
    GCOSLoadState state;              /* Current LOAD state */
    u8 target_module_id;              /* Target module ID */
    GCOSAID package_aid;              /* Package AID */
    u32 package_version;              /* Package version (u32 format per COS3 Appendix B) */
    u8 sd_id;                         /* Security domain ID */
    
    /* === SEF Data Buffer === */
    u8 buffer[GCOS_MODULE_CODE_SIZE]; /* Temporary buffer for SEF data */
    u32 buffer_size;                  /* Actual data size in buffer */
    u32 buffer_offset;                /* Current read position in buffer */
    
    /* === Section Parsing State === */
    GCOSSectionType current_section;  /* Section being parsed */
    u8 section_index;                 /* Index in expected section sequence (0-9) */
    GCOSSectionParseState parse_state; /* Current parse sub-state */
    u32 section_size;                 /* Expected section content size (from header) */
    u32 section_offset;               /* Bytes parsed in current section */
    
    /* === Section Headers (from 首段) === */
    u32 sef_version;
    GCOSAID sef_aid;
    u32 sef_len;
    u8 expected_import_module_count;
    u16 expected_import_func_count;
    u8 expected_app_num;
    u16 sec_func_len;
    u16 sec_elem_len;
    u16 sec_data_len;
    u32 sec_code_len;
    
    /* === 首段 (Header Section) === */
    u32 header_offset;                /* Progress in parsing 首段 */
    
    /* === 导入段 (Import Section) === */
    GCOSItemParseContext import_module_ctx;  /* Context for parsing import_module_items */
    GCOSItemParseContext import_func_ctx;    /* Context for parsing import_function_items */
    u8 import_count;
    GCOSImportInfo imports[MAX_IMPORTS];
    
    /* === 函数段 (Function Section) === */
    GCOSItemParseContext function_ctx; /* Context for parsing code_size array */
    u16 function_count;
    
    /* === 应用段 (Application Section) === */
    GCOSItemParseContext app_info_ctx; /* Context for parsing app_info array */
    u8 app_count;
    GCOSAID app_aids[MAX_APPS];
    u16 app_builder_methods[MAX_APPS];
    
    /* === 全局段 (Global Section) === */
    u16 rodata_base;
    u16 rwdata_base;
    u16 refdata_base;
    u16 moddata_base;
    u16 appdata_base;
    u16 data_end;
    
    /* === 导出段 (Export Section) === */
    GCOSItemParseContext export_ctx;   /* Context for parsing function_idxs */
    u16 export_count;
    
    /* === 元素段 (Element Section) === */
    GCOSItemParseContext element_ctx;  /* Context for parsing function_idxs */
    u16 element_count;
    
    /* === 数据段 (Data Section) === */
    u16 rodata_size;
    u16 rwdata_init_size;
    u16 moddata_init_size;
    u16 appdata_init_size;
    GCOSItemParseContext rodata_ctx;   /* Context for parsing rodata */
    GCOSItemParseContext rwdata_ctx;   /* Context for parsing rwdata */
    GCOSItemParseContext moddata_ctx;  /* Context for parsing moddata */
    GCOSItemParseContext appdata_ctx;  /* Context for parsing appdata */
    
    /* === 代码段 (Code Section) === */
    GCOSItemParseContext code_ctx;     /* Context for parsing bytecode */
    
    /* === 自定义段 (Custom Section) === */
    u32 custom_section_size;
    GCOSItemParseContext custom_ctx;   /* Context for parsing custom data */
} GCOSLoadContext;

/**
 * @brief Application Lifecycle States (Based on GP Specification)
 */
typedef enum {
    APPLICATION_INSTALLED = 0x03,      /* Installed but not selectable */
    APPLICATION_SELECTABLE = 0x07,     /* Can be selected */
    APPLICATION_PERSONALIZED = 0x0F,   /* Personalized and selectable */
    APPLICATION_LOCKED = 0x1F,         /* Locked */
    APPLICATION_TERMINATED = 0x7F      /* Terminated */
} GCOSAppLifecycleState;

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
 * 
 * Enhanced with cref PackageEntry-like fields for better package management.
 */
struct GCOSModule {
    /* === Basic Information === */
    u8 module_id;                   /* ⭐ NEW: Internal module ID (like cref package_id) */
    GCOSAID module_aid;             /* Module AID */
    GCOSModuleType type;            /* Module type (application/library) */
    
    /* ⭐ NEW: Module version (u32 format per COS3 Appendix B) */
    u32 version;                    /* Module version (major.minor.revision.internal) */
    
    /* ⭐ NEW: Module state (replaces simple loaded bool) */
    GCOSModuleState state;          /* Module state */
    
    /* ⭐ NEW: Security domain ID (like cref sdID) */
    u8 security_domain_id;          /* Owning security domain ID (0xFF = ISD) */
    
    /* === Dependency Management (like cref importedPackages) === */
    GCOSImportInfo imports[MAX_IMPORTS];  /* ⭐ NEW: Import dependency list */
    u8 import_count;                        /* Number of imported packages */
    
    /* === Data Areas === */
    u8 *global_data;                /* Module global data (volatile) */
    u32 global_data_size;           /* Global data size */
    
    const u8 *readonly_data;        /* Module read-only data (non-volatile, ROM) */
    u32 readonly_data_size;         /* Read-only data size */
    
    u8 *domain_data;                /* Module domain data (non-volatile, heap) */
    u32 domain_data_size;           /* Domain data size */
    
    /* === Code Area === */
    const u8 *code;                 /* Module program code (non-volatile, ROM) */
    u32 code_size;                  /* Code size */
    
    /* === Function Table === */
    GCOSFunctionHeader *functions;  /* Function header array */
    u16 function_count;             /* Number of functions */
    
    /* === Export Table === */
    void *export_table;             /* Export table */
    u16 export_count;               /* Number of exports */
    
    /* === Application Instance List === */
    GCOSAppInstance *app_instances[MAX_APPS_PER_MODULE];
    u8 app_instance_count;          /* Number of application instances */
    
    /* ⭐ DEPRECATED: Use 'state' field instead */
    bool loaded;                    /* Legacy flag (kept for compatibility) */
    bool initialized;               /* Whether initialized */
    
    /* === Resource Quotas (Phase 2 - Optional) === */
    // u32 code_quota;              /* Code space quota */
    // u32 data_quota;              /* Data space quota */
    // u32 ram_quota;               /* RAM quota */
    // u32 used_code;               /* Used code space */
    // u32 used_data;               /* Used data space */
    // u32 used_ram;                /* Used RAM */
};

/**
 * @brief 应用类型枚举
 */
typedef enum {
    APP_TYPE_REGULAR = 0x00,        /* 普通应用 */
    APP_TYPE_ISD = 0x01,            /* 初始安全域 (Initial Security Domain) */
    APP_TYPE_SSD = 0x02,            /* 补充安全域 (Supplementary Security Domain) */
    APP_TYPE_CASD = 0x04,           /* 可认证安全域 */
    APP_TYPE_FCSD = 0x05,           /* 最终卡安全域 */
} GCOSAppType;

/**
 * @brief 应用实例结构
 * 
 * 参考 cref 的设计，每个应用只有一个 process() 方法
 * 同时添加了 GlobalPlatform 规范要求的元数据字段
 */
struct GCOSAppInstance {
    /* === 基本信息 === */
    GCOSAID app_aid;                /* 应用AID */
    u8 app_id;                      /* 应用 ID (0 = ISD) */
    u16 module_index;               /* 所属模块索引 */
    GCOSAppLifecycleState lifecycle;/* 生命周期状态 */
    
    /* === 新增：类型、权限和安全域 ⭐ === */
    GCOSAppType app_type;           /* 应用类型 (APP/ISD/SSD) */
    u8 security_domain_id;          /* 所属安全域 ID (0xFF = ISD) */
    u8 privilege_byte1;             /* 权限字节 1 (特权标志) */
    u8 privilege_byte2;             /* 权限字节 2 */
    u8 privilege_byte3;             /* 权限字节 3 */
    u8 install_param;               /* 安装参数 (来自 INSTALL 命令 P2) */
    
    /* === APDU 处理方法 ⭐ === */
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
    
    /**
     * @brief 应用的 process() 方法指针
     * 
     * 类似于 cref 中的 Applet.process(APDU)
     * 这个方法负责处理所有非 GP 管理的 APDU 命令
     * 
     * @param app 应用实例指针
     * @param apdu APDU 数据
     * @param apdu_len APDU 长度
     * @param response 响应缓冲区
     * @param resp_len 响应长度（输出）
     * @return 状态字 SW
     */
    u16 (*process)(struct GCOSAppInstance *app,
                   const u8 *apdu,
                   u16 apdu_len,
                   u8 *response,
                   u16 *resp_len);
    
    /**
     * @brief 应用的 select() 方法（可选）
     * 
     * 在 SELECT 命令成功后调用，用于初始化
     * 
     * @param app 应用实例指针
     * @return GCOS_SUCCESS 成功，其他表示失败
     */
    GCOSResult (*on_select)(struct GCOSAppInstance *app);
    
    /**
     * @brief 应用的 deselect() 方法（可选）
     * 
     * 在取消选择时调用，用于清理资源
     * 
     * @param app 应用实例指针
     */
    void (*on_deselect)(struct GCOSAppInstance *app);
    
    /* === 应用数据 (堆上,非易失性) === */
    u8 *app_domain_data;            /* 应用域数据 */
    u32 app_domain_data_size;       /* 应用域数据大小 */
    
    u8 *ref_domain_data;            /* 引用域数据 */
    u32 ref_domain_data_size;       /* 引用域数据大小 */
    
    u8 *persistent_data;            /* 持久性数据 */
    u32 persistent_data_size;       /* 持久性数据大小 */
    
    /* === 运行时数据 (每个通道独立,易失性) === */
    struct {
        u8 *temp_dynamic_data;      /* 临时动态数据 */
        u32 temp_dynamic_data_size; /* 临时动态数据大小 */
        
        u8 *global_data_copy;       /* 模块全局数据副本 */
        u32 global_data_copy_size;  /* 副本大小 */
        
        bool active;                /* 是否激活 */
        bool selected;              /* 是否被选择 */
    } channel_data[MAX_CHANNELS];
    
    u8 current_channel;             /* 当前活动通道 */
    
    /* === 状态标志 === */
    bool is_selected;               /* 是否被选中 */
    u8 selected_channel;            /* 选中的通道 */
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
    
    /* ⭐ NEW: LOAD command context (for multi-APDU loading) */
    GCOSLoadContext load_context;   /* LOAD state machine context */
    
    /* 应用管理 */
    GCOSAppInstance apps[MAX_APPS];     /* 应用实例数组（静态分配）*/
    u8 app_count;                        /* 已安装应用数 */
    
    /* ⭐ 当前选中的应用（指针）*/
    GCOSAppInstance *selected_app;      /* 当前选中的应用实例 */
    
    /* 通道管理 */
    struct {
        GCOSAppInstance *selected_app;  /* 该通道选择的应用 */
        bool active;                    /* 通道是否激活 */
    } channels[MAX_CHANNELS];
    u8 current_channel;                 /* 当前通道 */
    
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
    
    /* Connection type (T=0 or T=CL) */
    GCOSConnType current_conn_type; /**< Current connection type from JCShell */
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
 * @brief Get global VM instance
 * @return Pointer to the global VM instance
 */
GCOSVM* gcos_vm_get_instance(void);

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

/**
 * @brief 根据模块 ID 查找模块
 * @param vm VM 指针
 * @param module_id 模块 ID
 * @return 模块指针, NULL 表示未找到
 */
GCOSModule* gcos_vm_find_module_by_id(GCOSVM *vm, u8 module_id);

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

/* ============================================================================
 * Channel Management
 * ============================================================================ */

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

/**
 * @brief Get global VM instance pointer
 * 
 * Used by JCShell server and other modules that need access to the VM.
 * 
 * @return Pointer to global VM instance, or NULL if not initialized
 */
GCOSVM* gcos_vm_get_instance(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_VM_H */
