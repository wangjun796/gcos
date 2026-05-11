#ifndef VM_LOADER_H
#define VM_LOADER_H

#include "vm_types.h"
#include "vm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 二进制文件格式结构 - 根据 COS3 规范 7.3
 * ============================================================================ */

/**
 * @brief 可加载文件（SEF）头部
 */
typedef struct {
    u32 sef_type;               /* 文件类型标识符（0x00736566） */
    u32 version;                /* 版本号 */
} SEFHeader;

/**
 * @brief 段头部
 */
typedef struct {
    u8 section_id;              /* 段标识符 */
    u32 size;                   /* 段内容字节数 */
} SectionHeader;

/**
 * @brief 可加载文件信息数据结构 (表 20)
 */
typedef struct {
    u32 sef_version;            /* 可加载文件版本 */
    u8 sef_aid_size;            /* 可加载文件标识符长度 */
    u8 *sef_aid;                /* 可加载文件标识符 */
} SEFInfo;

/**
 * @brief 首段数据结构 (表 19)
 */
typedef struct {
    SEFInfo sef_info;           /* 可加载文件信息 */
    u32 sef_len;                /* 可加载文件大小 */
    u8 import_module_count;     /* 导入模块个数 */
    u16 import_function_count;  /* 导入函数个数 */
    u8 app_num;                 /* 可执行模块安装信息个数 */
    u16 sec_func_len;           /* 函数段内容字节数 */
    u16 sec_elem_len;           /* 元素段内容字节数 */
    u16 sec_data_len;           /* 数据段内容字节数 */
    u32 sec_code_len;           /* 代码段内容字节数 */
} FirstSection;

/**
 * @brief 导入段 - 导入模块项 (表 22)
 */
typedef struct {
    u32 import_module_version;  /* 导入模块版本 */
    u8 import_module_aid_size;  /* 导入模块标识符长度 */
    u8 *import_module_aid;      /* 导入模块标识符 */
} ImportModuleItem;

/**
 * @brief 导入段 - 导入函数项 (表 24)
 */
typedef struct {
    u16 import_moduleidx;       /* 高5位: 模块索引 */
    u16 import_funcidx;         /* 低11位: 内部函数索引 */
} ImportFunctionItem;

/**
 * @brief 导入段 (表 21)
 */
typedef struct {
    u8 import_module_count;     /* 导入模块个数 */
    u16 import_function_count;  /* 导入函数个数 */
    ImportModuleItem *modules;  /* 导入模块数组 */
    ImportFunctionItem *functions; /* 导入函数数组 */
} ImportSection;

/**
 * @brief 函数段 (表 25)
 */
typedef struct {
    u32 function_count;         /* 函数数量 (size/2) */
    u16 *code_sizes;            /* 内部函数空间大小数组 */
} FunctionSection;

/**
 * @brief 应用段 - 可执行模块安装信息 (表 27)
 */
typedef struct {
    u8 aid_len;                 /* 可执行模块标识符长度 */
    u8 *app_aid;                /* 可执行模块标识符 */
    u16 app_builder_method_ID;  /* 应用安装接口函数索引 */
} AppInfo;

/**
 * @brief 应用段 (表 26)
 */
typedef struct {
    u8 app_num;                 /* 可执行模块安装信息个数 */
    AppInfo *app_infos;         /* 可执行模块安装信息数组 */
} AppSection;

/**
 * @brief 全局段 (表 28)
 */
typedef struct {
    u16 rodata_base;            /* 模块只读数据起始地址 */
    u16 rwdata_base;            /* 模块全局数据起始地址 */
    u16 refdata_base;           /* 引用域数据起始地址 */
    u16 moddata_base;           /* 模块域数据起始地址 */
    u16 appdata_base;           /* 应用域数据起始地址 */
    u16 data_end;               /* 应用域数据结束地址 */
} GlobalSection;

/**
 * @brief 导出段 (表 29)
 */
typedef struct {
    u32 export_count;           /* 导出项数量 (size/2) */
    u16 *function_idxs;         /* 导出函数索引数组 */
} ExportSection;

/**
 * @brief 元素段 (表 30)
 */
typedef struct {
    u32 element_count;          /* 元素数量 (size/2) */
    u16 *function_idxs;         /* 函数索引数组 */
} ElementSection;

/**
 * @brief 数据段 (表 31)
 */
typedef struct {
    u16 rodata_size;            /* 模块只读数据空间大小 */
    u16 rwdata_init_size;       /* 具有初始值的模块全局数据空间大小 */
    u16 moddata_init_size;      /* 具有初始值的模块域数据空间大小 */
    u16 appdata_init_size;      /* 具有初始值的应用域数据空间大小 */
    u8 *rodata;                 /* 模块只读数据 */
    u8 *rwdata;                 /* 模块全局数据初始值 */
    u8 *moddata;                /* 模块域数据初始值 */
    u8 *appdata;                /* 应用域数据初始值 */
} DataSection;

/**
 * @brief 代码段 - 函数头和字节码 (表 33)
 */
typedef struct {
    u16 param_count;            /* 参数个数 */
    u16 local_count;            /* 局部变量个数 */
    u8 opstack_max;             /* 操作数栈最大单元个数 */
    u8 indstack_max;            /* 间接访问变量栈单元个数 */
    u32 bytecode_size;          /* 字节码大小 (通过 function_section code_size 计算) */
    u8 *byte_code;              /* 字节码 */
} CodeInfo;

/**
 * @brief 代码段 (表 32)
 */
typedef struct {
    u32 code_count;             /* 代码项数量 (等于 function_count) */
    CodeInfo *codes;            /* 代码项数组 */
} CodeSection;

/**
 * @brief 链接文件（LINK）头部
 */
typedef struct {
    u32 link_type;              /* 文件类型标识符（0x6C696E6B） */
    u32 version;                /* 版本号 */
    u32 module_count;           /* 模块数量 */
} LinkHeader;

/**
 * @brief 链接文件 - 函数信息
 */
typedef struct {
    AID module_aid;             /* 模块AID */
    u16 function_id;            /* 函数ID */
    u32 address;                /* 函数地址 */
} LinkFunctionInfo;

/**
 * @brief 链接文件
 */
typedef struct {
    LinkHeader header;
    u32 function_count;
    LinkFunctionInfo *functions;
} LinkFile;

/* ============================================================================
 * 加载器API
 * ============================================================================ */

/**
 * @brief 从文件加载SEF文件
 * @param vm 虚拟机上下文
 * @param file_path 文件路径
 * @return 0成功，非0失败
 */
int vm_loader_load_sef_file(VMContext *vm, const char *file_path);

/**
 * @brief 从内存加载SEF文件
 * @param vm 虚拟机上下文
 * @param data 文件数据
 * @param size 数据大小
 * @return 0成功，非0失败
 */
int vm_loader_load_sef_memory(VMContext *vm, const u8 *data, u32 size);

/**
 * @brief 从文件加载LINK文件
 * @param vm 虚拟机上下文
 * @param file_path 文件路径
 * @return 0成功，非0失败
 */
int vm_loader_load_link_file(VMContext *vm, const char *file_path);

/**
 * @brief 解析SEF文件头部
 * @param data 文件数据
 * @param size 数据大小
 * @param header 输出：SEF头部
 * @return 0成功，非0失败
 */
int vm_loader_parse_sef_header(const u8 *data, u32 size, SEFHeader *header);

/**
 * @brief 解析段
 * @param vm 虚拟机上下文
 * @param section_data 段数据
 * @param section_size 段大小
 * @param section_id 段ID
 * @return 0成功，非0失败
 */
int vm_loader_parse_section(VMContext *vm, const u8 *section_data, 
                            u32 section_size, u8 section_id);

/**
 * @brief 验证SEF文件完整性
 * @param data 文件数据
 * @param size 数据大小
 * @return 0有效，非0无效
 */
int vm_loader_validate_sef(const u8 *data, u32 size);

/**
 * @brief 获取段名称
 * @param section_id 段ID
 * @return 段名称字符串
 */
const char* vm_loader_get_section_name(u8 section_id);

/**
 * @brief 打印SEF文件信息（调试用）
 * @param data 文件数据
 * @param size 数据大小
 */
void vm_loader_print_sef_info(const u8 *data, u32 size);

/**
 * @brief 卸载模块
 * @param vm 虚拟机上下文
 * @param module_index 模块索引
 * @return 0成功，非0失败
 */
int vm_loader_unload_module(VMContext *vm, u16 module_index);

/**
 * @brief 链接模块（解析导入/导出）
 * @param vm 虚拟机上下文
 * @param module_index 模块索引
 * @return 0成功，非0失败
 */
int vm_loader_link_module(VMContext *vm, u16 module_index);

#ifdef __cplusplus
}
#endif

#endif /* VM_LOADER_H */
