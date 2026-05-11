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
 * SEF File Format Constants
 * ============================================================================ */

#define SEF_MAGIC           0x53454630  /* "SEF0" */
#define SEF_HEADER_SIZE     32
#define SECTION_HEADER_SIZE 12

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief SEF File Header
 */
#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct {
    u32 magic;              /* Magic number: 0x53454630 */
    u16 version_major;      /* Major version */
    u16 version_minor;      /* Minor version */
    u32 file_size;          /* Total file size */
    u32 section_count;      /* Number of sections */
    u32 checksum;           /* File checksum */
    u8 reserved[8];         /* Reserved for future use */
} SEFHeader;

/**
 * @brief SEF Section Header
 */
typedef struct {
    u8 section_id;          /* Section identifier */
    u8 flags;               /* Section flags */
    u16 reserved;           /* Reserved */
    u32 offset;             /* Offset in file */
    u32 size;               /* Section size */
} SEFSectionHeader;
#pragma pack(pop)
#else
typedef struct {
    u32 magic;              /* Magic number: 0x53454630 */
    u16 version_major;      /* Major version */
    u16 version_minor;      /* Minor version */
    u32 file_size;          /* Total file size */
    u32 section_count;      /* Number of sections */
    u32 checksum;           /* File checksum */
    u8 reserved[8];         /* Reserved for future use */
} __attribute__((packed)) SEFHeader;

/**
 * @brief SEF Section Header
 */
typedef struct {
    u8 section_id;          /* Section identifier */
    u8 flags;               /* Section flags */
    u16 reserved;           /* Reserved */
    u32 offset;             /* Offset in file */
    u32 size;               /* Section size */
} __attribute__((packed)) SEFSectionHeader;
#endif

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate simple checksum
 * @param data Data buffer
 * @param size Data size
 * @return Checksum value
 */
static u32 calculate_checksum(const u8 *data, u32 size) {
    u32 checksum = 0;
    for (u32 i = 0; i < size; i++) {
        checksum += data[i];
    }
    return checksum;
}

/**
 * @brief Validate SEF header
 * @param header SEF header pointer
 * @param file_size File size
 * @return GCOS_OK if valid, error code otherwise
 */
static GCOSResult validate_sef_header(const SEFHeader *header, u32 file_size) {
    if (header == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check magic number */
    if (header->magic != SEF_MAGIC) {
        GCOS_PRINTF("[Loader] Invalid SEF magic: 0x%08X\n", header->magic);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Check version compatibility */
    if (header->version_major > GCOS_VM_VERSION_MAJOR) {
        GCOS_PRINTF("[Loader] Unsupported SEF version: %d.%d\n", 
                   header->version_major, header->version_minor);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Check file size */
    if (header->file_size != file_size) {
        GCOS_PRINTF("[Loader] File size mismatch: header=%u, actual=%u\n",
                   header->file_size, file_size);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Verify checksum */
    u32 calculated = calculate_checksum((const u8*)header + 4, SEF_HEADER_SIZE - 4);
    if (calculated != header->checksum) {
        GCOS_PRINTF("[Loader] Checksum verification failed\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOS_PRINTF("[Loader] SEF header validated successfully\n");
    return GCOS_SUCCESS;
}

/**
 * @brief Load first section (required)
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_first_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size < 8) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* First section contains module metadata */
    u16 module_id = (data[0] << 8) | data[1];
    u16 app_count = (data[2] << 8) | data[3];
    u32 code_offset = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    
    GCOS_PRINTF("[Loader] First section: module_idx=%u, apps=%u, code_offset=%u\n",
               vm->module_count, app_count, code_offset);
    
    /* Store module info - module will be initialized when fully loaded */
    (void)module_id; /* Module ID will be set from AID */
    (void)code_offset; /* Code offset tracked internally */
    
    return GCOS_SUCCESS;
}

/**
 * @brief Load function section
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_function_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse function table */
    u32 func_count = size / 8; /* Each function entry is 8 bytes */
    
    GCOS_PRINTF("[Loader] Loading %u functions\n", func_count);
    
    /* Store function entries in module */
    if (vm->module_count > 0 && vm->module_count <= MAX_MODULES) {
        GCOSModule *module = &vm->modules[vm->module_count - 1];
        module->function_count = func_count < MAX_FUNCTIONS ? func_count : MAX_FUNCTIONS;
        
        for (u32 i = 0; i < module->function_count; i++) {
            u32 offset = i * 8;
            /* Function header stores code_offset and max_stack_depth */
            module->functions[i].code_offset = (data[offset] << 24) | (data[offset+1] << 16) | 
                                               (data[offset+2] << 8) | data[offset+3];
            module->functions[i].max_stack_depth = (data[offset+4] << 24) | (data[offset+5] << 16) | 
                                                   (data[offset+6] << 8) | data[offset+7];
            
            GCOS_PRINTF("[Loader] Function %u: offset=0x%04X, stack_depth=%u\n",
                       i, module->functions[i].code_offset, module->functions[i].max_stack_depth);
        }
    }
    
    return GCOS_SUCCESS;
}

/**
 * @brief Load application section
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_app_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size < 16) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse application descriptor */
    u32 app_id = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    u16 aid_length = (data[4] << 8) | data[5];
    u8 state = data[6];
    u8 priority = data[7];
    
    GCOS_PRINTF("[Loader] Loading app: state=%u, priority=%u\n",
               state, priority);
    
    /* Create app instance - using pointer array */
    if (vm->app_count < (MAX_MODULES * MAX_APPS_PER_MODULE) && aid_length <= AID_MAX_LENGTH) {
        /* Allocate app instance (in real implementation, this would use static pool) */
        /* For now, we'll skip actual allocation and just log */
        GCOS_PRINTF("[Loader] App descriptor parsed (allocation deferred)\n");
        
        /* In a full implementation, you would:
         * 1. Allocate from static pool: vm->apps[vm->app_count] = &app_pool[vm->app_count];
         * 2. Initialize fields: app->lifecycle, app->app_aid, etc.
         */
        
        vm->app_count++;
        GCOS_PRINTF("[Loader] App count: %u\n", vm->app_count);
    } else {
        GCOS_PRINTF("[Loader] Cannot load app: max apps reached or invalid AID length\n");
        return GCOS_ERR_APP_NOT_FOUND;
    }
    
    return GCOS_SUCCESS;
}

/**
 * @brief Load code section
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_code_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if code fits in module code area */
    if (size > GCOS_MODULE_CODE_SIZE) {
        GCOS_PRINTF("[Loader] Code too large: %u > %u\n", size, GCOS_MODULE_CODE_SIZE);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Copy code to VM code area */
    memcpy(vm->runtime.module_code, data, size);
    vm->runtime.code_size = size;
    
    GCOS_PRINTF("[Loader] Code section loaded: %u bytes\n", size);
    return GCOS_SUCCESS;
}

/**
 * @brief Load import section
 * @param vm VM instance
 * @param data Section data
 * @param size Section size
 * @return GCOSResult Success or error code
 */
static GCOSResult load_import_section(GCOSVM *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse import table */
    u32 import_count = size / 12; /* Each import entry is 12 bytes */
    
    GCOS_PRINTF("[Loader] Loading %u imports\n", import_count);
    
    /* Store import entries - simplified for now */
    if (vm->module_count > 0) {
        GCOSModule *module = &vm->modules[vm->module_count - 1];
        module->import_count = import_count < MAX_IMPORT_MODULES ? import_count : MAX_IMPORT_MODULES;
        
        /* Import resolution would happen here in full implementation */
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
    
    /* Validate SEF header */
    const SEFHeader *header = (const SEFHeader *)sef_data;
    GCOSResult result = validate_sef_header(header, sef_size);
    if (result != GCOS_SUCCESS) {
        return result;
    }
    
    /* Parse sections */
    u32 section_offset = SEF_HEADER_SIZE;
    
    for (u32 i = 0; i < header->section_count; i++) {
        if (section_offset + SECTION_HEADER_SIZE > sef_size) {
            GCOS_PRINTF("[Loader] Section header out of bounds\n");
            return GCOS_ERROR_INVALID_PARAM;
        }
        
        const SEFSectionHeader *section_header = (const SEFSectionHeader *)(sef_data + section_offset);
        section_offset += SECTION_HEADER_SIZE;
        
        GCOS_PRINTF("[Loader] Processing section %u: id=%u, offset=%u, size=%u\n",
                   i, section_header->section_id, section_header->offset, section_header->size);
        
        /* Validate section bounds */
        if (section_header->offset + section_header->size > sef_size) {
            GCOS_PRINTF("[Loader] Section data out of bounds\n");
            return GCOS_ERROR_INVALID_PARAM;
        }
        
        const u8 *section_data = sef_data + section_header->offset;
        
        /* Load section based on type */
        switch (section_header->section_id) {
            case SECTION_ID_FIRST:
                result = load_first_section(vm, section_data, section_header->size);
                break;
            case SECTION_ID_FUNCTION:
                result = load_function_section(vm, section_data, section_header->size);
                break;
            case SECTION_ID_APP:
                result = load_app_section(vm, section_data, section_header->size);
                break;
            case SECTION_ID_CODE:
                result = load_code_section(vm, section_data, section_header->size);
                break;
            case SECTION_ID_IMPORT:
                result = load_import_section(vm, section_data, section_header->size);
                break;
            default:
                GCOS_PRINTF("[Loader] Unknown section type: %u, skipping\n", section_header->section_id);
                continue;
        }
        
        if (result != GCOS_SUCCESS) {
            GCOS_PRINTF("[Loader] Failed to load section %u: error=%d\n", i, result);
            return result;
        }
    }
    
    GCOS_PRINTF("[Loader] SEF file loaded successfully\n");
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
