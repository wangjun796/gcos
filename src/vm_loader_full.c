/**
 * @file vm_loader_full.c
 * @brief GCOS VM SEF文件加载器完整实现
 * 
 * 实现基于COS3规范的SEF文件加载器，包括：
 * - SEF文件格式解析
 * - 段提取和验证
 * - 符号链接
 * - 模块实例化
 */

#include "vm_core.h"
#include "vm_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * SEF文件结构定义
 * ============================================================================ */

/**
 * @brief SEF文件头
 */
typedef struct {
    u32 sef_type;              /* 文件类型 ('sef') */
    u32 version;               /* 版本号 */
    u32 section_count;          /* 段数量 */
} SEFHeader;

/**
 * @brief 段头
 */
typedef struct {
    u8 section_id;             /* 段ID */
    u32 size;                  /* 段大小 */
} SectionHeader;

/**
 * @brief 首段
 */
typedef struct {
    u32 sef_version;           /* SEF版本 */
    u8 sef_aid_size;          /* SEF AID长度 */
    u8 sef_aid[16];           /* SEF AID */
    u8 import_module_count;    /* 导入模块数 */
    u16 import_function_count;  /* 导入函数数 */
    u8 app_num;               /* 应用数 */
    u16 sec_func_len;          /* 函数段长度 */
    u16 sec_elem_len;          /* 元素段长度 */
    u16 sec_data_len;          /* 数据段长度 */
    u32 sec_code_len;          /* 代码段长度 */
} FirstSection;

/**
 * @brief 导入模块项
 */
typedef struct {
    u32 import_module_version;  /* 导入模块版本 */
    u8 import_module_aid_size; /* 导入模块AID长度 */
    u8 import_module_aid[16]; /* 导入模块AID */
} ImportModuleItem;

/**
 * @brief 应用信息
 */
typedef struct {
    u8 aid_len;                /* AID长度 */
    u8 aid[16];               /* AID */
    u16 builder_method_id;      /* 构建方法ID */
} AppInfo;

/**
 * @brief 全局段
 */
typedef struct {
    u16 rodata_base;           /* 只读数据基址 */
    u16 rwdata_base;           /* 读写数据基址 */
    u16 refdata_base;          /* 引用数据基址 */
    u16 moddata_base;          /* 模块数据基址 */
    u16 appdata_base;          /* 应用数据基址 */
    u16 data_end;              /* 数据结束地址 */
} GlobalSection;

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/**
 * @brief 读取小端序32位整数
 */
static u32 read_le32(const u8 *data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

/**
 * @brief 读取小端序16位整数
 */
static u16 read_le16(const u8 *data) {
    return data[0] | (data[1] << 8);
}

/**
 * @brief 比较AID
 */
static bool aid_equal(const AID *aid1, const AID *aid2) {
    if (aid1->length != aid2->length) {
        return false;
    }
    return memcmp(aid1->aid, aid2->aid, aid1->length) == 0;
}

/**
 * @brief 解析SEF文件头
 */
static int parse_sef_header(const u8 *data, u32 size, SEFHeader *header) {
    if (size < sizeof(SEFHeader)) {
        return -1;
    }

    header->sef_type = read_le32(data);
    header->version = read_le32(data + 4);
    header->section_count = read_le32(data + 8);

    /* 验证文件类型 */
    if (header->sef_type != FILE_TYPE_SEF) {
        printf("Error: Invalid SEF file type 0x%08X\n", header->sef_type);
        return -2;
    }

    return 0;
}

/**
 * @brief 解析段头
 */
static int parse_section_header(const u8 *data, u32 size, u32 offset, SectionHeader *header) {
    if (offset + sizeof(SectionHeader) > size) {
        return -1;
    }

    header->section_id = data[offset];
    header->size = read_le32(data + offset + 1);

    return 0;
}

/**
 * @brief 解析首段
 */
static int parse_first_section(const u8 *data, u32 size, u32 offset, FirstSection *first) {
    u32 pos = offset;

    if (pos + 4 > size) {
        return -1;
    }

    first->sef_version = read_le32(data + pos);
    pos += 4;

    if (pos + 1 > size) {
        return -2;
    }

    first->sef_aid_size = data[pos];
    pos++;

    if (pos + first->sef_aid_size > size) {
        return -3;
    }

    memcpy(first->sef_aid, data + pos, first->sef_aid_size);
    pos += first->sef_aid_size;

    if (pos + 1 > size) {
        return -4;
    }

    first->import_module_count = data[pos];
    pos++;

    if (pos + 2 > size) {
        return -5;
    }

    first->import_function_count = read_le16(data + pos);
    pos += 2;

    if (pos + 1 > size) {
        return -6;
    }

    first->app_num = data[pos];
    pos++;

    if (pos + 2 > size) {
        return -7;
    }

    first->sec_func_len = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -8;
    }

    first->sec_elem_len = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -9;
    }

    first->sec_data_len = read_le16(data + pos);
    pos += 2;

    if (pos + 4 > size) {
        return -10;
    }

    first->sec_code_len = read_le32(data + pos);

    return 0;
}

/**
 * @brief 解析应用段
 */
static int parse_app_section(const u8 *data, u32 size, u32 offset, u8 app_num, AppInfo *apps) {
    u32 pos = offset;

    for (u8 i = 0; i < app_num; i++) {
        if (pos + 1 > size) {
            return -1;
        }

        apps[i].aid_len = data[pos];
        pos++;

        if (pos + apps[i].aid_len > size) {
            return -2;
        }

        memcpy(apps[i].aid, data + pos, apps[i].aid_len);
        pos += apps[i].aid_len;

        if (pos + 2 > size) {
            return -3;
        }

        apps[i].builder_method_id = read_le16(data + pos);
        pos += 2;
    }

    return 0;
}

/**
 * @brief 解析全局段
 */
static int parse_global_section(const u8 *data, u32 size, u32 offset, GlobalSection *global) {
    u32 pos = offset;

    if (pos + 2 > size) {
        return -1;
    }

    global->rodata_base = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -2;
    }

    global->rwdata_base = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -3;
    }

    global->refdata_base = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -4;
    }

    global->moddata_base = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -5;
    }

    global->appdata_base = read_le16(data + pos);
    pos += 2;

    if (pos + 2 > size) {
        return -6;
    }

    global->data_end = read_le16(data + pos);

    return 0;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

/**
 * @brief 从文件加载SEF
 */
int vm_loader_load_sef_file(VMContext *vm, const char *file_path) {
    if (vm == NULL || file_path == NULL) {
        return -1;
    }

    /* 打开文件 */
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        printf("Error: Cannot open file %s\n", file_path);
        return -2;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        return -3;
    }

    /* 读取文件内容 */
    u8 *data = (u8*)malloc(file_size);
    if (data == NULL) {
        fclose(fp);
        return -4;
    }

    size_t read_size = fread(data, 1, file_size, fp);
    fclose(fp);

    if ((long)read_size != file_size) {
        free(data);
        return -5;
    }

    /* 从内存加载 */
    int ret = vm_loader_load_sef_memory(vm, data, (u32)file_size);

    free(data);
    return ret;
}

/**
 * @brief 从内存加载SEF
 */
int vm_loader_load_sef_memory(VMContext *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return -1;
    }

    /* 解析SEF头 */
    SEFHeader header;
    int ret = parse_sef_header(data, size, &header);
    if (ret != 0) {
        return ret;
    }

    printf("Loading SEF: type=0x%08X, version=%u, sections=%u\n",
           header.sef_type, header.version, header.section_count);

    /* 解析各个段 */
    u32 offset = sizeof(SEFHeader);
    FirstSection first;
    AppInfo apps[MAX_APPS_PER_MODULE];
    GlobalSection global;

    for (u32 i = 0; i < header.section_count && offset < size; i++) {
        SectionHeader section;
        ret = parse_section_header(data, size, offset, &section);
        if (ret != 0) {
            printf("Error: Failed to parse section header at offset %u\n", offset);
            return ret;
        }

        printf("Section %u: id=0x%02X, size=%u\n", i, section.section_id, section.size);

        /* 根据段ID处理 */
        switch (section.section_id) {
            case SECTION_ID_FIRST: {
                ret = parse_first_section(data, size, offset + sizeof(SectionHeader), &first);
                if (ret != 0) {
                    return ret;
                }
                printf("  SEF version=%u, AID len=%u\n",
                       first.sef_version, first.sef_aid_size);
                break;
            }

            case SECTION_ID_APP: {
                ret = parse_app_section(data, size, offset + sizeof(SectionHeader),
                                       first.app_num, apps);
                if (ret != 0) {
                    return ret;
                }
                printf("  Apps: %u\n", first.app_num);
                break;
            }

            case SECTION_ID_GLOBAL: {
                ret = parse_global_section(data, size, offset + sizeof(SectionHeader), &global);
                if (ret != 0) {
                    return ret;
                }
                printf("  Global: rodata=0x%04X, rwdata=0x%04X\n",
                       global.rodata_base, global.rwdata_base);
                break;
            }

            case SECTION_ID_CODE: {
                /* 加载代码段 */
                if (section.size > VM_MODULE_CODE_SIZE) {
                    printf("Error: Code section too large: %u > %u\n",
                           section.size, VM_MODULE_CODE_SIZE);
                    return -10;
                }

                memcpy(vm->module_code, data + offset + sizeof(SectionHeader), section.size);
                printf("  Code loaded: %u bytes\n", section.size);
                break;
            }

            case SECTION_ID_DATA: {
                /* 加载数据段 */
                /* TODO: 实现数据段加载 */
                printf("  Data section: %u bytes\n", section.size);
                break;
            }

            case SECTION_ID_IMPORT: {
                /* 加载导入段 */
                /* TODO: 实现导入段处理 */
                printf("  Import section: %u bytes\n", section.size);
                break;
            }

            case SECTION_ID_FUNCTION: {
                /* 加载函数段 */
                /* TODO: 实现函数段处理 */
                printf("  Function section: %u bytes\n", section.size);
                break;
            }

            case SECTION_ID_EXPORT: {
                /* 加载导出段 */
                /* TODO: 实现导出段处理 */
                printf("  Export section: %u bytes\n", section.size);
                break;
            }

            case SECTION_ID_ELEMENT: {
                /* 加载元素段 */
                /* TODO: 实现元素段处理 */
                printf("  Element section: %u bytes\n", section.size);
                break;
            }

            default:
                printf("  Unknown section: 0x%02X\n", section.section_id);
                break;
        }

        offset += sizeof(SectionHeader) + section.size;
    }

    /* 创建模块 */
    if (vm->module_count >= MAX_MODULES) {
        printf("Error: Maximum modules reached\n");
        return -11;
    }

    Module *module = &vm->modules[vm->module_count];
    memset(module, 0, sizeof(Module));

    /* 设置模块信息 */
    module->module_aid.length = first.sef_aid_size;
    memcpy(module->module_aid.aid, first.sef_aid, first.sef_aid_size);
    module->type = MODULE_TYPE_APP;
    module->version = first.sef_version;
    module->loaded = true;

    /* 分配模块数据 */
    module->global_data = (u8*)malloc(VM_GLOBAL_DATA_SIZE);
    if (module->global_data == NULL) {
        return -12;
    }
    memset(module->global_data, 0, VM_GLOBAL_DATA_SIZE);
    module->global_data_size = VM_GLOBAL_DATA_SIZE;

    /* 分配函数表 */
    module->functions = (FunctionHeader*)malloc(MAX_FUNCTIONS * sizeof(FunctionHeader));
    if (module->functions == NULL) {
        free(module->global_data);
        return -13;
    }
    memset(module->functions, 0, MAX_FUNCTIONS * sizeof(FunctionHeader));
    module->function_count = 0;

    /* 设置代码区 */
    module->code = vm->module_code;
    module->code_size = VM_MODULE_CODE_SIZE;

    vm->module_count++;

    printf("Module loaded: AID len=%u, version=%u\n",
           module->module_aid.length, module->version);

    return 0;
}

/**
 * @brief 验证SEF文件
 */
int vm_loader_validate_sef(const u8 *data, u32 size) {
    if (data == NULL || size < sizeof(SEFHeader)) {
        return -1;
    }

    SEFHeader header;
    int ret = parse_sef_header(data, size, &header);
    if (ret != 0) {
        return ret;
    }

    /* 验证文件类型 */
    if (header.sef_type != FILE_TYPE_SEF) {
        return -2;
    }

    /* 验证版本号 */
    if (header.version == 0) {
        return -3;
    }

    return 0;
}

/**
 * @brief 解析SEF文件头
 */
int vm_loader_parse_sef_header(const u8 *data, u32 size, SEFHeader *header) {
    if (data == NULL || header == NULL || size < sizeof(SEFHeader)) {
        return -1;
    }

    return parse_sef_header(data, size, header);
}

/**
 * @brief 解析段
 */
int vm_loader_parse_section(VMContext *vm, const u8 *section_data,
                             u32 section_size, u8 section_id) {
    if (vm == NULL || section_data == NULL) {
        return -1;
    }

    /* 根据段ID处理 */
    switch (section_id) {
        case SECTION_ID_FIRST: {
            FirstSection first;
            return parse_first_section(section_data, section_size, 0, &first);
        }

        case SECTION_ID_APP: {
            AppInfo apps[MAX_APPS_PER_MODULE];
            return parse_app_section(section_data, section_size, 0, 1, apps);
        }

        case SECTION_ID_GLOBAL: {
            GlobalSection global;
            return parse_global_section(section_data, section_size, 0, &global);
        }

        default:
            printf("Warning: Unhandled section ID: 0x%02X\n", section_id);
            return -2;
    }
}
