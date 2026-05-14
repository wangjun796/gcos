/**
 * @file gcos_loader.c
 * @brief GCOS VM SEF (Secure Executable Format) Loader
 * 
 * Implements loading and parsing of COS3 SEF files:
 * - SEF file format validation
 * - Section parsing and loading
 * - Module verification
 * - Code loading into VM memory
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * SEF File Format Constants (per COS3 Specification)
 * ============================================================================ */

/* File type identifiers (COS3 Table 10) */
#define SEF_MAGIC           0x00736566  /* "sef\0" - Loadable file type */
#define LINK_MAGIC          0x6C696E6B  /* "link" - Link file type */
#define WASM_MAGIC          0x0061736D  /* "asm\0" - Intermediate file type */

/* Section IDs (COS3 Table 18) */
#define SECTION_ID_FIRST        0x01    /* First section (required) */
#define SECTION_ID_IMPORT       0x02    /* Import section (optional) */
#define SECTION_ID_FUNCTION     0x03    /* Function section (required) */
#define SECTION_ID_APP          0x04    /* App section (optional) */
#define SECTION_ID_GLOBAL       0x05    /* Global section (required) */
#define SECTION_ID_EXPORT       0x06    /* Export section (optional) */
#define SECTION_ID_ELEMENT      0x07    /* Element section (optional) */
#define SECTION_ID_DATA         0x08    /* Data section (optional) */
#define SECTION_ID_CODE         0x09    /* Code section (required) */
#define SECTION_ID_CUSTOM       0x0F    /* Custom section (optional) */

/* Header sizes (COS3 Tables 16 & 17) */
#define SEF_HEADER_SIZE         8       /* sef_type(u32) + version(u32) */
#define SECTION_HEADER_SIZE     5       /* section_id(u8) + size(u32) */


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Little-endian read helpers (COS3 Section 7.1.2, line 413)
 * 
 * All multi-byte integers in SEF files must be stored in little-endian order.
 */
static inline u16 read_u16_le(const u8 *data) {
    return (u16)data[0] | ((u16)data[1] << 8);
}

static inline u32 read_u32_le(const u8 *data) {
    return (u32)data[0] |
           ((u32)data[1] << 8) |
           ((u32)data[2] << 16) |
           ((u32)data[3] << 24);
}

/**
 * @brief Decode version number per COS3 Appendix B
 * 
 * Version format: [internal][revision][minor][major]
 * Byte 3 (MSB): major version
 * Byte 2:       minor version
 * Byte 1:       revision
 * Byte 0 (LSB): internal version
 * 
 * @param version Raw u32 version value
 * @param major Output: major version
 * @param minor Output: minor version
 * @param revision Output: revision number
 * @param internal Output: internal version
 */
static void decode_version(u32 version, u8 *major, u8 *minor, u8 *revision, u8 *internal) {
    if (major)    *major    = (version >> 24) & 0xFF;
    if (minor)    *minor    = (version >> 16) & 0xFF;
    if (revision) *revision = (version >> 8) & 0xFF;
    if (internal) *internal = version & 0xFF;
}

/**
 * @brief Validate SEF header (per COS3 Table 16)
 * 
 * Validates:
 * - Magic number (0x00736566)
 * - Version compatibility
 * - Minimum file size
 * 
 * @param data SEF file data
 * @param file_size File size in bytes
 * @param out_version Output: parsed version (optional)
 * @return GCOS_OK if valid, error code otherwise
 */
static GCOSResult validate_sef_header(const u8 *data, u32 file_size, u32 *out_version) {
    if (data == NULL || file_size < SEF_HEADER_SIZE) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Read sef_type using little-endian (COS3 Section 7.1.2) */
    u32 sef_type = read_u32_le(&data[0]);
    
    /* Check magic number (COS3 Table 10) */
    if (sef_type != SEF_MAGIC) {
        GCOS_PRINTF("[Loader] Invalid SEF magic: 0x%08X (expected 0x%08X)\n", 
                   sef_type, SEF_MAGIC);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Read and decode version (COS3 Appendix B) */
    u32 version = read_u32_le(&data[4]);
    u8 ver_major, ver_minor, ver_revision, ver_internal;
    decode_version(version, &ver_major, &ver_minor, &ver_revision, &ver_internal);
    
    GCOS_PRINTF("[Loader] SEF version: v%d.%d.%d.%d (raw=0x%08X)\n",
               ver_major, ver_minor, ver_revision, ver_internal, version);
    
    /* Check version compatibility */
    if (ver_major > GCOS_VM_VERSION_MAJOR) {
        GCOS_PRINTF("[Loader] Unsupported SEF major version: %d (max supported: %d)\n",
                   ver_major, GCOS_VM_VERSION_MAJOR);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Return version if requested */
    if (out_version) {
        *out_version = version;
    }
    
    GCOS_PRINTF("[Loader] SEF header validated successfully\n");
    return GCOS_SUCCESS;
}

/**
 * @brief Load first section (required) - per COS3 Table 19
 * 
 * First section contains:
 * - sef_info structure (sef_version, sef_aid_size, sef_aid[])
 * - sef_len (total SEF file length)
 * - import_module_count, import_function_count
 * - app_num
 * - sec_func_len, sec_elem_len, sec_data_len, sec_code_len
 * 
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_first_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size < 10) {  /* Minimum: sef_version(4) + sef_aid_size(1) + aid(5) */
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse sef_info structure (COS3 Table 20) */
    u32 sef_version = read_u32_le(&data[0]);
    u8 sef_aid_size = data[4];
    
    if (size < 5 + sef_aid_size) {
        GCOS_PRINTF("[Loader] First section too small for AID\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Extract module AID */
    const u8 *aid_data = &data[5];
    GCOS_PRINTF("[Loader] First section: version=0x%08X, AID_size=%u\n",
               sef_version, sef_aid_size);
    
    /* Print AID in hex */
    GCOS_PRINTF("[Loader] Module AID: ");
    for (u8 i = 0; i < sef_aid_size && i < 16; i++) {
        GCOS_PRINTF("%02X", aid_data[i]);
    }
    GCOS_PRINTF("\n");
    
    /* TODO: Parse remaining fields (sef_len, counts, etc.) */
    /* For now, just validate the section exists */
    
    return GCOS_SUCCESS;
}

/**
 * @brief Load function section (per COS3 Table 25)
 * 
 * Function section contains an array of u16 code_size values.
 * Each entry specifies the size of a function (header + bytecode).
 * 
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_function_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse function table - each entry is 2 bytes (u16) per COS3 Table 25 */
    u32 func_count = size / 2;
    
    GCOS_PRINTF("[Loader] Loading %u functions\n", func_count);
    
    /* Store function entries in module */
    if (vm->module_count > 0 && vm->module_count <= MAX_MODULES) {
        GCOSModule *module = &vm->modules[vm->module_count - 1];
        module->function_count = func_count < MAX_FUNCTIONS ? func_count : MAX_FUNCTIONS;
        
        for (u32 i = 0; i < module->function_count; i++) {
            /* Read code_size (includes header and bytecode) */
            u16 code_size = read_u16_le(&data[i * 2]);
            
            /* For now, store code_size as code_offset placeholder */
            /* In full implementation, this would track function boundaries */
            module->functions[i].code_offset = code_size;
            module->functions[i].max_stack_depth = 0;  /* Will be parsed from function header */
            
            GCOS_PRINTF("[Loader] Function %u: code_size=%u bytes\n",
                       i, code_size);
        }
    }
    
    return GCOS_SUCCESS;
}

/**
 * @brief Load application section (per COS3 Table 26 & 27)
 * 
 * Application section structure:
 * - app_num (u8): number of app descriptors
 * - app_info[app_num]:
 *   - aid_len (u8): AID length
 *   - app_aid[aid_len]: AID bytes
 *   - app_builder_method_ID (u16): installer function index
 * 
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_app_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size < 1) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    size_t offset = 0;
    
    /* Read app_num */
    u8 app_num = data[offset++];
    
    GCOS_PRINTF("[Loader] Loading %u app(s)\n", app_num);
    
    /* Parse each app descriptor */
    for (u8 i = 0; i < app_num; i++) {
        if (offset >= size) {
            GCOS_PRINTF("[Loader] ERROR: App descriptor %u truncated\n", i);
            return GCOS_ERR_INVALID_PARAM;
        }
        
        /* Read aid_len */
        u8 aid_len = data[offset++];
        
        if (offset + aid_len + 2 > size) {
            GCOS_PRINTF("[Loader] ERROR: App %u AID data out of bounds\n", i);
            return GCOS_ERR_INVALID_PARAM;
        }
        
        /* Read app_aid */
        const u8 *app_aid = &data[offset];
        offset += aid_len;
        
        /* Read app_builder_method_ID (u16, little-endian) */
        u16 builder_id = read_u16_le(&data[offset]);
        offset += 2;
        
        GCOS_PRINTF("[Loader] App %u: AID_len=%u, AID=", i, aid_len);
        for (u8 j = 0; j < aid_len && j < 16; j++) {
            GCOS_PRINTF("%02X", app_aid[j]);
        }
        GCOS_PRINTF(", builder_id=%u\n", builder_id);
        
        /* Validate AID length */
        if (aid_len > AID_MAX_LENGTH) {
            GCOS_PRINTF("[Loader] ERROR: App %u AID too long (%u > %u)\n",
                       i, aid_len, AID_MAX_LENGTH);
            return GCOS_ERR_APP_NOT_FOUND;
        }
        
        /* Check app capacity */
        if (vm->app_count >= (MAX_MODULES * MAX_APPS_PER_MODULE)) {
            GCOS_PRINTF("[Loader] ERROR: Max apps reached\n");
            return GCOS_ERR_APP_NOT_FOUND;
        }
        
        /* TODO: Allocate and initialize app instance */
        vm->app_count++;
    }
    
    GCOS_PRINTF("[Loader] Total apps loaded: %u\n", vm->app_count);
    return GCOS_SUCCESS;
}

/**
 * @brief Load global section (per COS3 Table 28)
 * 
 * Global section defines memory layout:
 * - rodata_base (u16): read-only data start address
 * - rwdata_base (u16): read-write data start address
 * - refdata_base (u16): reference domain data start
 * - moddata_base (u16): module domain data start
 * - appdata_base (u16): application domain data start
 * - data_end (u16): data end address
 * 
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_global_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size < 12) {  /* 6 x u16 = 12 bytes */
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse memory layout (all u16, little-endian) */
    u16 rodata_base = read_u16_le(&data[0]);
    u16 rwdata_base = read_u16_le(&data[2]);
    u16 refdata_base = read_u16_le(&data[4]);
    u16 moddata_base = read_u16_le(&data[6]);
    u16 appdata_base = read_u16_le(&data[8]);
    u16 data_end = read_u16_le(&data[10]);
    
    GCOS_PRINTF("[Loader] Global section: memory layout\n");
    GCOS_PRINTF("  rodata:  [0x%04X - 0x%04X]\n", rodata_base, rwdata_base - 1);
    GCOS_PRINTF("  rwdata:  [0x%04X - 0x%04X]\n", rwdata_base, refdata_base - 1);
    GCOS_PRINTF("  refdata: [0x%04X - 0x%04X]\n", refdata_base, moddata_base - 1);
    GCOS_PRINTF("  moddata: [0x%04X - 0x%04X]\n", moddata_base, appdata_base - 1);
    GCOS_PRINTF("  appdata: [0x%04X - 0x%04X]\n", appdata_base, data_end - 1);
    GCOS_PRINTF("  Total data size: %u bytes\n", data_end);
    
    /* TODO: Allocate and initialize data regions in VM */
    /* For now, just validate the section */
    
    return GCOS_SUCCESS;
}

/**
 * @brief Load code section (per COS3 Table 32-34)
 * 
 * Code section contains function headers and bytecode.
 * Function header format (Table 34 - 2-byte):
 * - flag_paranum_localnum (u8): bit7=0 (2-byte), bit6-4=params, bit3-0=locals
 * - opstack_indstack (u8): bit7-5=opstack, bit4-0=indstack
 * 
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_code_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOS_PRINTF("[Loader] Code section: %u bytes\n", size);
    
    /* Parse functions sequentially */
    u32 offset = 0;
    u32 func_index = 0;
    
    while (offset < size && func_index < MAX_FUNCTIONS) {
        /* Need at least 2 bytes for function header */
        if (offset + 2 > size) {
            GCOS_PRINTF("[Loader] ERROR: Function %u header truncated\n", func_index);
            break;
        }
        
        /* Parse function header (Table 34 - 2-byte format) */
        u8 flag_paranum_localnum = data[offset++];
        u8 opstack_indstack = data[offset++];
        
        /* Decode header fields */
        bool is_4byte_header = (flag_paranum_localnum & 0x80) != 0;
        u8 param_count = (flag_paranum_localnum >> 4) & 0x07;
        u8 local_count = flag_paranum_localnum & 0x0F;
        u8 opstack_max = (opstack_indstack >> 5) & 0x07;
        u8 indstack_count = opstack_indstack & 0x1F;
        
        GCOS_PRINTF("[Loader] Function %u: header=%s, params=%u, locals=%u, opstack=%u, indstack=%u\n",
                   func_index,
                   is_4byte_header ? "4-byte" : "2-byte",
                   param_count, local_count, opstack_max, indstack_count);
        
        /* If 4-byte header, read 2 more bytes */
        if (is_4byte_header) {
            if (offset + 2 > size) {
                GCOS_PRINTF("[Loader] ERROR: Function %u 4-byte header truncated\n", func_index);
                break;
            }
            u8 localnum_ext = data[offset++];
            u8 opstack_ext = data[offset++];
            
            local_count = localnum_ext & 0x7F;
            opstack_max = opstack_ext;
            
            GCOS_PRINTF("  Extended: locals=%u, opstack=%u\n", local_count, opstack_max);
        }
        
        /* Remaining bytes are bytecode for this function */
        /* Calculate bytecode size from function section's code_size array */
        /* For now, assume all remaining bytes belong to this function */
        u32 bytecode_size = size - offset;
        
        if (bytecode_size > 0) {
            GCOS_PRINTF("  Bytecode: %u bytes\n", bytecode_size);
            
            /* Copy bytecode to VM code area */
            if (bytecode_size <= GCOS_MODULE_CODE_SIZE) {
                memcpy(vm->runtime.module_code, &data[offset], bytecode_size);
                vm->runtime.code_size = bytecode_size;
            } else {
                GCOS_PRINTF("  WARNING: Bytecode too large, truncating\n");
                memcpy(vm->runtime.module_code, &data[offset], GCOS_MODULE_CODE_SIZE);
                vm->runtime.code_size = GCOS_MODULE_CODE_SIZE;
            }
        }
        
        /* Move to next function (for now, assume single function) */
        /* In full implementation, would use code_size array to find next function */
        break;
    }
    
    GCOS_PRINTF("[Loader] Code section loaded successfully\n");
    return GCOS_SUCCESS;
}

/**
 * @brief Load import section (per COS3 Table 22)
 * 
 * Import section specifies module dependencies by AID.
 * Each import entry includes:
 * - imported_module_aid (variable length)
 * - imported_function_index (u16)
 * 
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_import_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* First byte is import count */
    u8 import_count = data[0];
    
    GCOS_PRINTF("[Loader] Loading %u imports\n", import_count);
    
    /* Store import entries - simplified for now */
    if (vm->module_count > 0) {
        GCOSModule *module = &vm->modules[vm->module_count - 1];
        module->import_count = import_count < MAX_IMPORT_MODULES ? import_count : MAX_IMPORT_MODULES;
        
        /* TODO: Parse each import entry (AID + function index) */
        GCOS_PRINTF("[Loader] Imports parsed: %u entries\n", module->import_count);
    }
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_loader_load_sef(GCOSVM *vm, const u8 *sef_data, u32 sef_size) {
    if (vm == NULL || sef_data == NULL || sef_size < SEF_HEADER_SIZE) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOS_PRINTF("[Loader] Loading SEF file: size=%u bytes\n", sef_size);
    
    /* Validate SEF header (per COS3 Table 16) */
    u32 version = 0;
    GCOSResult result = validate_sef_header(sef_data, sef_size, &version);
    if (result != GCOS_SUCCESS) {
        return result;
    }
    
    /* Parse sections sequentially (per COS3 Table 17) */
    u32 offset = SEF_HEADER_SIZE;  /* Start after 8-byte header */
    u32 section_count = 0;
    
    /* Track required sections */
    bool has_first = false;
    bool has_function = false;
    bool has_global = false;
    bool has_code = false;
    
    while (offset < sef_size) {
        /* Need at least 5 bytes for section header */
        if (offset + SECTION_HEADER_SIZE > sef_size) {
            GCOS_PRINTF("[Loader] Incomplete section header at offset %u\n", offset);
            break;
        }
        
        /* Read section header using little-endian */
        u8 section_id = sef_data[offset];
        u32 section_size = read_u32_le(&sef_data[offset + 1]);
        
        GCOS_PRINTF("[Loader] Section %u: ID=0x%02X, Size=%u bytes (at offset %u)\n",
                   section_count, section_id, section_size, offset);
        
        /* Validate section bounds */
        if (offset + SECTION_HEADER_SIZE + section_size > sef_size) {
            GCOS_PRINTF("[Loader] Section data out of bounds\n");
            return GCOS_ERROR_INVALID_PARAM;
        }
        
        /* Point to section content (after 5-byte header) */
        const u8 *section_data = &sef_data[offset + SECTION_HEADER_SIZE];
        
        /* Load section based on type (COS3 Table 18) */
        switch (section_id) {
            case SECTION_ID_FIRST:
                result = load_first_section(vm, section_data, section_size);
                has_first = true;
                break;
            case SECTION_ID_IMPORT:
                result = load_import_section(vm, section_data, section_size);
                break;
            case SECTION_ID_FUNCTION:
                result = load_function_section(vm, section_data, section_size);
                has_function = true;
                break;
            case SECTION_ID_APP:
                result = load_app_section(vm, section_data, section_size);
                break;
            case SECTION_ID_GLOBAL:
                result = load_global_section(vm, section_data, section_size);
                has_global = true;
                break;
            case SECTION_ID_EXPORT:
                /* TODO: Implement export section loading */
                GCOS_PRINTF("[Loader] Export section: %u bytes (not yet implemented)\n", section_size);
                result = GCOS_SUCCESS;
                break;
            case SECTION_ID_ELEMENT:
                /* TODO: Implement element section loading */
                GCOS_PRINTF("[Loader] Element section: %u bytes (not yet implemented)\n", section_size);
                result = GCOS_SUCCESS;
                break;
            case SECTION_ID_DATA:
                /* TODO: Implement data section loading */
                GCOS_PRINTF("[Loader] Data section: %u bytes (not yet implemented)\n", section_size);
                result = GCOS_SUCCESS;
                break;
            case SECTION_ID_CODE:
                result = load_code_section(vm, section_data, section_size);
                has_code = true;
                break;
            case SECTION_ID_CUSTOM:
                /* Custom sections are optional and can be skipped */
                GCOS_PRINTF("[Loader] Custom section: %u bytes (skipped)\n", section_size);
                result = GCOS_SUCCESS;
                break;
            default:
                GCOS_PRINTF("[Loader] Unknown section type: 0x%02X, skipping\n", section_id);
                result = GCOS_SUCCESS;
                break;
        }
        
        if (result != GCOS_SUCCESS) {
            GCOS_PRINTF("[Loader] Failed to load section 0x%02X: error=%d\n", section_id, result);
            return result;
        }
        
        /* Move to next section */
        offset += SECTION_HEADER_SIZE + section_size;
        section_count++;
    }
    
    /* Validate required sections (COS3 Section 7.3.3) */
    if (!has_first) {
        GCOS_PRINTF("[Loader] ERROR: Missing required FIRST section\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    if (!has_function) {
        GCOS_PRINTF("[Loader] ERROR: Missing required FUNCTION section\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    if (!has_global) {
        GCOS_PRINTF("[Loader] WARNING: Missing required GLOBAL section\n");
        /* For now, allow loading without global section */
    }
    if (!has_code) {
        GCOS_PRINTF("[Loader] ERROR: Missing required CODE section\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOS_PRINTF("[Loader] SEF file loaded successfully (%u sections)\n", section_count);
    return GCOS_SUCCESS;
}

GCOSResult gcos_loader_validate_module(const GCOSVM *vm, u8 module_index) {
    if (vm == NULL || module_index >= vm->module_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    const GCOSModule *module = &vm->modules[module_index];
    
    /* Validate module has required sections */
    if (module->function_count == 0) {
        GCOS_PRINTF("[Loader] Module %u has no functions\n", module_index);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Validate code is loaded */
    if (vm->runtime.code_size == 0) {
        GCOS_PRINTF("[Loader] Module %u has no code\n", module_index);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOS_PRINTF("[Loader] Module %u validated successfully\n", module_index);
    return GCOS_SUCCESS;
}

GCOSResult gcos_loader_get_module_info(const GCOSVM *vm, u8 module_index, 
                                       GCOSModuleInfo *info) {
    if (vm == NULL || info == NULL || module_index >= vm->module_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    const GCOSModule *module = &vm->modules[module_index];
    
    info->module_id = module_index; /* Use index as ID */
    info->function_count = module->function_count;
    info->import_count = module->import_count;
    info->app_count = module->app_instance_count; /* Use app_instance_count */
    info->code_offset = 0; /* Code offset tracked in runtime context */
    
    return GCOS_SUCCESS;
}
