#include "vm_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

static const char* get_section_name(u8 section_id) {
    switch (section_id) {
        case SECTION_ID_FIRST:       return "First";
        case SECTION_ID_IMPORT:      return "Import";
        case SECTION_ID_FUNCTION:    return "Function";
        case SECTION_ID_APP:         return "App";
        case SECTION_ID_GLOBAL:      return "Global";
        case SECTION_ID_EXPORT:      return "Export";
        case SECTION_ID_ELEMENT:     return "Element";
        case SECTION_ID_DATA:        return "Data";
        case SECTION_ID_CODE:        return "Code";
        case SECTION_ID_CUSTOM:      return "Custom";
        default:                     return "Unknown/Reserved";
    }
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

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

int vm_loader_load_sef_memory(VMContext *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return -1;
    }
    
    /* 验证文件 */
    int ret = vm_loader_validate_sef(data, size);
    if (ret != 0) {
        printf("Error: Invalid SEF file\n");
        return ret;
    }
    
    /* 解析SEF头部 */
    SEFHeader header;
    ret = vm_loader_parse_sef_header(data, size, &header);
    if (ret != 0) {
        return ret;
    }
    
    printf("Loading SEF file: version=%u\n", header.version);
    
    /* 解析各个段 */
    u32 offset = sizeof(SEFHeader);
    
    while (offset < size) {
        if (offset + sizeof(SectionHeader) > size) {
            break;
        }
        
        SectionHeader section_header;
        memcpy(&section_header, data + offset, sizeof(SectionHeader));
        offset += sizeof(SectionHeader);
        
        if (offset + section_header.size > size) {
            printf("Error: Section size out of bounds\n");
            break;
        }
        
        printf("Parsing section: %s (id=%u, size=%u)\n",
               get_section_name(section_header.section_id),
               section_header.section_id, section_header.size);
        
        ret = vm_loader_parse_section(vm, data + offset, 
                                      section_header.size,
                                      section_header.section_id);
        if (ret != 0) {
            printf("Error: Failed to parse section %u\n", section_header.section_id);
            return ret;
        }
        
        offset += section_header.size;
    }
    
    return 0;
}

int vm_loader_load_link_file(VMContext *vm, const char *file_path) {
    /* 简化实现：暂不支持LINK文件 */
    (void)vm;
    (void)file_path;
    printf("Warning: LINK file loading not implemented\n");
    return -1;
}

int vm_loader_parse_sef_header(const u8 *data, u32 size, SEFHeader *header) {
    if (data == NULL || header == NULL) {
        return -1;
    }
    
    if (size < sizeof(SEFHeader)) {
        return -2;
    }
    
    memcpy(header, data, sizeof(SEFHeader));
    
    /* 验证文件类型 */
    if (header->sef_type != FILE_TYPE_SEF) {
        printf("Error: Invalid file type 0x%08X (expected 0x%08X)\n",
               header->sef_type, FILE_TYPE_SEF);
        return -3;
    }
    
    return 0;
}

int vm_loader_parse_section(VMContext *vm, const u8 *section_data,
                            u32 section_size, u8 section_id) {
    if (vm == NULL || section_data == NULL) {
        return -1;
    }
    
    switch (section_id) {
        case SECTION_ID_FIRST: {
            /* 首段 */
            if (section_size < sizeof(FirstSection)) {
                return -2;
            }
            FirstSection *first = (FirstSection*)section_data;
            printf("  SEF Version: %u, AID Size: %u, Total Len: %u\n",
                   first->sef_info.sef_version, first->sef_info.sef_aid_size, first->sef_len);
                   
            /* 创建新模块 */
            if (vm->module_count < MAX_MODULES) {
                Module *module = &vm->modules[vm->module_count];
                memset(module, 0, sizeof(Module));
                /* 复制AID (假设aid_size合法，不超过16) */
                u8 aid_len = first->sef_info.sef_aid_size > 16 ? 16 : first->sef_info.sef_aid_size;
                module->module_aid.length = aid_len;
                /* Note: first->sef_info.sef_aid 是指针，在真实解析中应该基于内存偏移读取，这里简单处理 */
                /* TODO: 正确处理变长结构体 */
                module->type = MODULE_TYPE_APP; 
                module->version = first->sef_info.sef_version;
                module->loaded = true;
                
                vm->module_count++;
            }
            break;
        }
        
        case SECTION_ID_IMPORT: {
            printf("  Parsing Import Section\n");
            break;
        }
        
        case SECTION_ID_FUNCTION: {
            /* 函数段 */
            if (vm->module_count == 0) {
                return -2;
            }
            Module *module = &vm->modules[vm->module_count - 1];
            
            u32 func_count = section_size / 2; /* 规范定义: size/2 */
            module->function_count = func_count;
            
            module->functions = (FunctionHeader*)malloc(func_count * sizeof(FunctionHeader));
            if (module->functions == NULL) {
                return -3;
            }
            memset(module->functions, 0, func_count * sizeof(FunctionHeader));
            
            /* 从 Function Section 中读取每个函数的 total_size (header + bytecode) */
            for (u32 i = 0; i < func_count; i++) {
                u16 total_sz;
                memcpy(&total_sz, section_data + i * 2, sizeof(u16));
                module->functions[i].code_size = total_sz; /* 暂存总大小 */
            }
            
            printf("  Loaded %u functions\n", func_count);
            break;
        }
        
        case SECTION_ID_APP: {
            /* 应用段 */
            printf("  Application Section parsed\n");
            break;
        }
        
        case SECTION_ID_GLOBAL: {
            /* 全局段 */
            printf("  Global Section parsed\n");
            break;
        }
        
        case SECTION_ID_EXPORT: {
            /* 导出段 */
            printf("  Export Section parsed\n");
            break;
        }
        
        case SECTION_ID_ELEMENT: {
            /* 元素段 */
            printf("  Element Section parsed\n");
            break;
        }
        
        case SECTION_ID_CODE: {
            /* 代码段 */
            if (vm->module_count == 0) {
                return -2;
            }
            Module *module = &vm->modules[vm->module_count - 1];
            
            if (section_size > VM_MODULE_CODE_SIZE) {
                return -3; /* 代码太大 */
            }
            
            memcpy(vm->module_code, section_data, section_size);
            module->code = vm->module_code;
            module->code_size = section_size;
            
            u32 current_offset = 0;
            for (u16 i = 0; i < module->function_count; i++) {
                if (current_offset >= section_size) break;
                
                FunctionHeader *fh = &module->functions[i];
                u16 total_size = fh->code_size; /* previously stored from Function Section */
                u8 flag = section_data[current_offset];
                u8 header_size = 0;
                
                if ((flag & 0x80) == 0) {
                    /* 2字节头部 */
                    if (current_offset + 2 > section_size) break;
                    u8 byte0 = section_data[current_offset];
                    u8 byte1 = section_data[current_offset + 1];
                    fh->param_count = (byte0 >> 4) & 0x07;
                    fh->local_var_count = byte0 & 0x0F;
                    fh->max_stack_depth = (byte1 >> 5) & 0x07;
                    fh->max_indstack_depth = byte1 & 0x1F;
                    header_size = 2;
                } else {
                    /* 4字节头部 */
                    if (current_offset + 4 > section_size) break;
                    fh->param_count = section_data[current_offset] & 0x7F;
                    fh->local_var_count = section_data[current_offset + 1] & 0x7F;
                    fh->max_stack_depth = section_data[current_offset + 2];
                    fh->max_indstack_depth = section_data[current_offset + 3];
                    header_size = 4;
                }
                
                fh->code_offset = current_offset + header_size;
                fh->code_size = total_size - header_size; /* actual bytecode size */
                
                current_offset += total_size;
            }
            
            printf("  Loaded %u bytes of code\n", section_size);
            break;
        }
        
        case SECTION_ID_DATA: {
            /* 数据段 */
            if (vm->module_count == 0) {
                return -2;
            }
            Module *module = &vm->modules[vm->module_count - 1];
            
            if (section_size < 8) {
                return -3;
            }
            
            u16 rodata_size, rwdata_init_size, moddata_init_size, appdata_init_size;
            memcpy(&rodata_size, section_data, 2);
            memcpy(&rwdata_init_size, section_data + 2, 2);
            memcpy(&moddata_init_size, section_data + 4, 2);
            memcpy(&appdata_init_size, section_data + 6, 2);
            
            u32 current_offset = 8;
            
            if (rodata_size > 0 && current_offset + rodata_size <= section_size) {
                module->readonly_data = (u8*)malloc(rodata_size);
                if (module->readonly_data) {
                    memcpy(module->readonly_data, section_data + current_offset, rodata_size);
                }
                current_offset += rodata_size;
            }
            
            /* TODO: 在真实场景中，rwdata, moddata, appdata 会被应用实例在运行时分配。
             * 目前可以暂存在 module 中以便在安装应用时复制初始值。 */
            
            printf("  Loaded data section, RO: %u, RW_init: %u, MOD_init: %u, APP_init: %u\n",
                   rodata_size, rwdata_init_size, moddata_init_size, appdata_init_size);
            break;
        }
        
        case SECTION_ID_CUSTOM: {
            printf("  Custom Section parsed\n");
            break;
        }
        
        default:
            printf("  Skipping unknown section type %u\n", section_id);
            break;
    }
    
    return 0;
}

int vm_loader_validate_sef(const u8 *data, u32 size) {
    if (data == NULL || size < sizeof(SEFHeader)) {
        return -1;
    }
    
    /* 检查文件类型标识符 */
    u32 file_type;
    memcpy(&file_type, data, sizeof(u32));
    
    if (file_type != FILE_TYPE_SEF) {
        return -2;
    }
    
    return 0;
}

const char* vm_loader_get_section_name(u8 section_id) {
    return get_section_name(section_id);
}

void vm_loader_print_sef_info(const u8 *data, u32 size) {
    if (data == NULL || size < sizeof(SEFHeader)) {
        printf("Invalid SEF file\n");
        return;
    }
    
    SEFHeader header;
    memcpy(&header, data, sizeof(SEFHeader));
    
    printf("=== SEF File Info ===\n");
    printf("File Type: 0x%08X\n", header.sef_type);
    printf("Version: %u\n", header.version);
    printf("=====================\n");
}

int vm_loader_unload_module(VMContext *vm, u16 module_index) {
    if (vm == NULL || module_index >= vm->module_count) {
        return -1;
    }
    
    Module *module = &vm->modules[module_index];
    
    /* 释放模块资源 */
    if (module->functions != NULL) {
        free(module->functions);
        module->functions = NULL;
    }
    
    if (module->global_data != NULL) {
        free(module->global_data);
        module->global_data = NULL;
    }
    
    if (module->readonly_data != NULL) {
        free(module->readonly_data);
        module->readonly_data = NULL;
    }
    
    if (module->domain_data != NULL) {
        free(module->domain_data);
        module->domain_data = NULL;
    }
    
    memset(module, 0, sizeof(Module));
    
    return 0;
}

int vm_loader_link_module(VMContext *vm, u16 module_index) {
    if (vm == NULL || module_index >= vm->module_count) {
        return -1;
    }
    
    /* 解析导入/导出表，进行链接 */
    /* 简化实现：暂不实现复杂的链接逻辑 */
    
    printf("Linking module %u...\n", module_index);
    
    return 0;
}
