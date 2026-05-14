/**
 * @file gcos_persistence.c
 * @brief GCOS VM Module Persistence Implementation
 * 
 * Implements module persistence to Flash using eflash library.
 * 
 * Storage Strategy:
 * - Each module is stored as an eflash object
 * - Object header: Module metadata (AID, version, state, sizes)
 * - Object data: Complete SEF binary data
 * - Module ID mapping: module_id -> eflash object_id
 * 
 * Key Features:
 * - Atomic save/load using eflash transactions
 * - CRC-16 integrity check
 * - Module enumeration and management
 * - Flash space management
 * 
 * Reference: COS3 Specification Section 8.2 (Loading/Installation/Deletion Management)
 */

#include "gcos_persistence.h"
#include "gcos_vm.h"
#include "eflash_ftl.h"  /* Include eflash_ftl.h first - it defines obj_header_t */
#include "eflash.h"      /* Then include eflash.h for API declarations */
#include "eflash_mgr.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Module-to-object mapping entry
 * 
 * Maps GCOS module_id to eflash object_id.
 * Stored in RAM for fast lookup.
 */
typedef struct {
    u8 module_id;         /* GCOS module ID */
    u16 obj_id;           /* eflash object ID */
    u16 sector_count;     /* Number of sectors used */
    bool valid;           /* Entry is valid */
} GCOSModuleMapping;

/**
 * @brief Persistence manager context
 * 
 * Tracks all persisted modules and their Flash mappings.
 */
typedef struct {
    GCOSModuleMapping mappings[GCOS_MAX_PERSISTED_MODULES];
    u8 module_count;
    bool initialized;
} GCOSPersistenceContext;

/* Global persistence context */
static GCOSPersistenceContext g_persist_ctx = {0};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate CRC-16 (CCITT) checksum
 * 
 * @param data Data buffer
 * @param length Data length
 * @return CRC-16 checksum
 */
u16 gcos_persist_calc_crc16(const u8 *data, u32 length) {
    u16 crc = 0xFFFF;
    u32 i;
    
    for (i = 0; i < length; i++) {
        u8 j;
        crc ^= (u16)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Find mapping entry by module_id
 * 
 * @param module_id Module ID to search
 * @return Pointer to mapping entry, or NULL if not found
 */
static GCOSModuleMapping* find_mapping_by_module_id(u8 module_id) {
    u8 i;
    for (i = 0; i < GCOS_MAX_PERSISTED_MODULES; i++) {
        if (g_persist_ctx.mappings[i].valid &&
            g_persist_ctx.mappings[i].module_id == module_id) {
            return &g_persist_ctx.mappings[i];
        }
    }
    return NULL;
}

/**
 * @brief Find mapping entry by obj_id
 * 
 * @param obj_id eflash object ID to search
 * @return Pointer to mapping entry, or NULL if not found
 */
static GCOSModuleMapping* find_mapping_by_obj_id(u16 obj_id) {
    u8 i;
    for (i = 0; i < GCOS_MAX_PERSISTED_MODULES; i++) {
        if (g_persist_ctx.mappings[i].valid &&
            g_persist_ctx.mappings[i].obj_id == obj_id) {
            return &g_persist_ctx.mappings[i];
        }
    }
    return NULL;
}

/**
 * @brief Create new mapping entry
 * 
 * @param module_id Module ID
 * @param obj_id eflash object ID
 * @param sector_count Number of sectors
 * @return true on success, false if no space
 */
static bool create_mapping(u8 module_id, u16 obj_id, u16 sector_count) {
    u8 i;
    for (i = 0; i < GCOS_MAX_PERSISTED_MODULES; i++) {
        if (!g_persist_ctx.mappings[i].valid) {
            g_persist_ctx.mappings[i].module_id = module_id;
            g_persist_ctx.mappings[i].obj_id = obj_id;
            g_persist_ctx.mappings[i].sector_count = sector_count;
            g_persist_ctx.mappings[i].valid = true;
            g_persist_ctx.module_count++;
            return true;
        }
    }
    return false;
}

/**
 * @brief Delete mapping entry
 * 
 * @param module_id Module ID to delete
 * @return true on success, false if not found
 */
static bool delete_mapping(u8 module_id) {
    u8 i;
    for (i = 0; i < GCOS_MAX_PERSISTED_MODULES; i++) {
        if (g_persist_ctx.mappings[i].valid &&
            g_persist_ctx.mappings[i].module_id == module_id) {
            g_persist_ctx.mappings[i].valid = false;
            g_persist_ctx.mappings[i].module_id = 0;
            g_persist_ctx.mappings[i].obj_id = 0;
            g_persist_ctx.mappings[i].sector_count = 0;
            g_persist_ctx.module_count--;
            return true;
        }
    }
    return false;
}

/**
 * @brief Prepare module metadata
 * 
 * @param module Pointer to GCOSModule
 * @param meta Output metadata buffer
 * @param sef_size SEF file size
 * @return true on success, false on error
 */
static bool prepare_module_metadata(const GCOSModule *module,
                                     GCOSModuleMeta *meta,
                                     u32 sef_size) {
    memset(meta, 0, sizeof(GCOSModuleMeta));
    
    meta->magic = GCOS_MODULE_META_MAGIC;
    meta->module_id = module->module_id;
    meta->security_domain_id = module->security_domain_id;
    meta->state = module->state;
    meta->version = module->version;
    memcpy(&meta->module_aid, &module->module_aid, sizeof(GCOSAID));
    meta->type = module->type;
    
    /* Memory areas */
    meta->global_data_size = module->global_data_size;
    meta->readonly_data_size = module->readonly_data_size;
    meta->domain_data_size = module->domain_data_size;
    meta->code_size = module->code_size;
    
    /* Function table */
    meta->function_count = module->function_count;
    meta->function_table_size = module->function_count * sizeof(GCOSFunctionHeader);
    
    /* Export table */
    meta->export_count = module->export_count;
    /* TODO: Calculate export_table_size based on actual export structure */
    
    /* Import dependencies */
    meta->import_count = module->import_count;
    meta->import_table_size = module->import_count * sizeof(GCOSImportInfo);
    
    /* Application instances */
    meta->app_instance_count = module->app_instance_count;
    /* TODO: Calculate app_table_size based on actual app instance structure */
    
    /* SEF file size */
    meta->sef_file_size = sef_size;
    
    /* Calculate checksum (excluding checksum field itself) */
    meta->checksum = gcos_persist_calc_crc16((const u8*)meta,
                                              sizeof(GCOSModuleMeta) - sizeof(u16) - 2);
    
    return true;
}

/**
 * @brief Validate module metadata
 * 
 * @param meta Metadata to validate
 * @return true if valid, false if corrupt
 */
static bool validate_module_metadata(const GCOSModuleMeta *meta) {
    if (meta->magic != GCOS_MODULE_META_MAGIC) {
        printf("[PERSIST] ERROR: Invalid magic number 0x%08X\n", meta->magic);
        return false;
    }
    
    u16 expected_checksum = gcos_persist_calc_crc16((const u8*)meta,
                                                      sizeof(GCOSModuleMeta) - sizeof(u16) - 2);
    if (meta->checksum != expected_checksum) {
        printf("[PERSIST] ERROR: Checksum mismatch (expected 0x%04X, got 0x%04X)\n",
               expected_checksum, meta->checksum);
        return false;
    }
    
    return true;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int gcos_persist_init(GCOSVM *vm) {
    if (g_persist_ctx.initialized) {
        printf("[PERSIST] Already initialized\n");
        return 0;
    }
    
    printf("[PERSIST] Initializing persistence layer...\n");
    
    /* Initialize eflash simulation layer first */
    int ret = eflash_init("gcos_flash.bin");
    if (ret != 0) {
        printf("[PERSIST] ERROR: eflash_init failed (%d)\n", ret);
        return -1;
    }
    
    /* Initialize eflash FTL layer */
    ret = eflash_ftl_init();
    if (ret != 0) {
        printf("[PERSIST] ERROR: eflash_ftl_init failed (%d)\n", ret);
        return -1;
    }
    
    /* Scan existing objects and rebuild mapping */
    /* TODO: Implement object enumeration by scanning object headers */
    
    g_persist_ctx.initialized = true;
    printf("[PERSIST] Persistence layer initialized successfully\n");
    return 0;
}

int gcos_persist_save_module(GCOSVM *vm, u8 module_id) {
    GCOSModule *module;
    GCOSModuleMeta meta;
    u16 obj_id;
    u32 sef_size;
    u8 *sef_data;
    u16 sector_count;
    u16 i;
    
    /* Find module in VM */
    module = gcos_vm_find_module_by_id(vm, module_id);
    if (!module) {
        printf("[PERSIST] ERROR: Module %d not found in VM\n", module_id);
        return -1;
    }
    
    printf("[PERSIST] Saving module %d to Flash...\n", module_id);
    printf("[PERSIST] Module AID: ");
    for (i = 0; i < module->module_aid.length; i++) {
        printf("%02X", module->module_aid.aid[i]);
    }
    printf("\n");
    
    /* Calculate SEF size (simplified - actual SEF generation needed) */
    sef_size = module->code_size + module->global_data_size +
               module->readonly_data_size + module->domain_data_size;
    
    /* Check if module already persisted */
    GCOSModuleMapping *existing = find_mapping_by_module_id(module_id);
    if (existing) {
        obj_id = existing->obj_id;
        printf("[PERSIST] Updating existing module (obj_id=%d)\n", obj_id);
    } else {
        /* Allocate new object */
        obj_id = eflash_ftl_obj_alloc_header();
        if (obj_id == 0xFFFF) {
            printf("[PERSIST] ERROR: Failed to allocate object header\n");
            return -1;
        }
        printf("[PERSIST] Allocated new object (obj_id=%d)\n", obj_id);
    }
    
    /* Calculate sector count (USER_DATA_SIZE = 464 bytes per sector) */
    /* We need metadata + SEF data stored across sectors */
    u32 total_data_size = sizeof(GCOSModuleMeta) + sef_size;
    sector_count = (u16)((total_data_size + GCOS_SECTOR_USER_DATA_SIZE - 1) / GCOS_SECTOR_USER_DATA_SIZE);
    
    /* Prepare metadata */
    if (!prepare_module_metadata(module, &meta, sef_size)) {
        printf("[PERSIST] ERROR: Failed to prepare metadata\n");
        return -1;
    }
    
    /* Allocate logical address space for module data */
    u32 logical_addr;
    int ret = 0;
    if (eflash_mgr_alloc(total_data_size, &logical_addr) != 0) {
        printf("[PERSIST] ERROR: Failed to allocate Flash space\n");
        return -1;
    }
    printf("[PERSIST] Allocated Flash space: logical_addr=0x%08X, size=%u bytes\n",
           logical_addr, total_data_size);
    
    /* Begin transaction */
    eflash_ftl_txn_begin();
    
    /* Write metadata using logical address */
    ret = eflash_ftl_write_logical(logical_addr, (const uint8_t*)&meta, sizeof(GCOSModuleMeta));
    if (ret != 0) {
        printf("[PERSIST] ERROR: Failed to write metadata\n");
        eflash_ftl_txn_abort();
        eflash_mgr_free(logical_addr, total_data_size);
        return -1;
    }
    
    /* TODO: Write SEF data to logical_addr + sizeof(GCOSModuleMeta) */
    /* This requires SEF file serialization from module structure */
    
    /* Commit transaction */
    ret = eflash_ftl_txn_commit();
    if (ret != 0) {
        printf("[PERSIST] ERROR: Transaction commit failed\n");
        eflash_mgr_free(logical_addr, total_data_size);
        return -1;
    }
    
    /* Update object header with storage info */
    obj_header_t hdr;
    memset(&hdr, 0, sizeof(obj_header_t));
    hdr.pkg_id = (uint16_t)(GCOS_MODULE_PKG_ID_START + module_id);
    hdr.class_id = GCOS_MODULE_CLASS_ID;
    hdr.body_addr = logical_addr;
    hdr.body_size = total_data_size;
    
    ret = eflash_ftl_obj_set_header(obj_id, &hdr);
    if (ret != 0) {
        printf("[PERSIST] ERROR: Failed to update object header\n");
        eflash_ftl_txn_abort();
        eflash_mgr_free(logical_addr, total_data_size);
        return -1;
    }
    
    /* Update mapping */
    if (existing) {
        existing->sector_count = sector_count;
    } else {
        if (!create_mapping(module_id, obj_id, sector_count)) {
            printf("[PERSIST] ERROR: Failed to create mapping\n");
            eflash_mgr_free(logical_addr, total_data_size);
            return -1;
        }
    }
    
    printf("[PERSIST] Module %d saved successfully (obj_id=%d, sectors=%d)\n",
           module_id, obj_id, sector_count);
    return 0;
}

int gcos_persist_load_module(GCOSVM *vm, u8 module_id) {
    GCOSModuleMapping *mapping;
    GCOSModuleMeta meta;
    obj_header_t hdr;
    int ret;
    
    /* Find mapping */
    mapping = find_mapping_by_module_id(module_id);
    if (!mapping) {
        printf("[PERSIST] ERROR: Module %d not found in Flash\n", module_id);
        return -1;
    }
    
    printf("[PERSIST] Loading module %d from Flash (obj_id=%d)...\n",
           module_id, mapping->obj_id);
    
    /* Read object header first */
    ret = eflash_ftl_obj_get_header(mapping->obj_id, &hdr);
    if (ret != 0) {
        printf("[PERSIST] ERROR: Failed to read object header\n");
        return -1;
    }
    
    /* Read metadata from Flash using logical address from header */
    u8 meta_buffer[sizeof(GCOSModuleMeta)];
    ret = eflash_ftl_read_logical(hdr.body_addr, meta_buffer, sizeof(GCOSModuleMeta));
    if (ret != 0) {
        printf("[PERSIST] ERROR: Failed to read metadata\n");
        return -1;
    }
    
    memcpy(&meta, meta_buffer, sizeof(GCOSModuleMeta));
    
    /* Validate metadata */
    if (!validate_module_metadata(&meta)) {
        printf("[PERSIST] ERROR: Module metadata corrupt\n");
        return -1;
    }
    
    /* Create module in VM */
    if (vm->module_count >= MAX_MODULES) {
        printf("[PERSIST] ERROR: No space for new module in VM\n");
        return -1;
    }
    GCOSModule *module = &vm->modules[vm->module_count];
    
    /* Restore module fields from metadata */
    module->module_id = meta.module_id;
    module->security_domain_id = meta.security_domain_id;
    module->state = meta.state;
    module->version = meta.version;
    memcpy(&module->module_aid, &meta.module_aid, sizeof(GCOSAID));
    module->type = meta.type;
    
    /* Allocate memory areas */
    if (meta.global_data_size > 0) {
        /* TODO: Use static allocation or eflash-backed storage */
        module->global_data_size = meta.global_data_size;
    }
    if (meta.readonly_data_size > 0) {
        module->readonly_data_size = meta.readonly_data_size;
    }
    if (meta.domain_data_size > 0) {
        module->domain_data_size = meta.domain_data_size;
    }
    if (meta.code_size > 0) {
        module->code_size = meta.code_size;
    }
    
    /* TODO: Restore function table, export table, import table, app instances from Flash */
    /* TODO: Read SEF data from logical_addr + sizeof(GCOSModuleMeta) and parse */
    
    /* Mark module as loaded */
    module->loaded = true;
    module->initialized = true;
    vm->module_count++;
    
    printf("[PERSIST] Module %d loaded successfully\n", module_id);
    return 0;
}

int gcos_persist_delete_module(GCOSVM *vm, u8 module_id) {
    GCOSModuleMapping *mapping;
    obj_header_t obj_hdr;
    int ret;
    
    /* Find mapping */
    mapping = find_mapping_by_module_id(module_id);
    if (!mapping) {
        printf("[PERSIST] ERROR: Module %d not found in Flash\n", module_id);
        return -1;
    }
    
    printf("[PERSIST] Deleting module %d from Flash (obj_id=%d)...\n",
           module_id, mapping->obj_id);
    
    /* Begin transaction */
    eflash_ftl_txn_begin();
    
    /* Invalidate object header */
    memset(&obj_hdr, 0, sizeof(obj_header_t));
    obj_hdr.type = 0xFF;  /* Mark as invalid */
    ret = eflash_ftl_obj_set_header(mapping->obj_id, &obj_hdr);
    if (ret != 0) {
        printf("[PERSIST] ERROR: Failed to invalidate object header\n");
        eflash_ftl_txn_abort();
        return -1;
    }
    
    /* Commit transaction */
    ret = eflash_ftl_txn_commit();
    if (ret != 0) {
        printf("[PERSIST] ERROR: Transaction commit failed\n");
        return -1;
    }
    
    /* Remove mapping */
    if (!delete_mapping(module_id)) {
        printf("[PERSIST] ERROR: Failed to delete mapping\n");
        return -1;
    }
    
    printf("[PERSIST] Module %d deleted successfully\n", module_id);
    return 0;
}

bool gcos_persist_module_exists(GCOSVM *vm, u8 module_id) {
    return find_mapping_by_module_id(module_id) != NULL;
}

u8 gcos_persist_get_module_count(GCOSVM *vm) {
    return g_persist_ctx.module_count;
}

u8 gcos_persist_list_modules(GCOSVM *vm, u8 *module_ids, u8 max_count) {
    u8 i;
    u8 count = 0;
    
    for (i = 0; i < GCOS_MAX_PERSISTED_MODULES && count < max_count; i++) {
        if (g_persist_ctx.mappings[i].valid) {
            module_ids[count++] = g_persist_ctx.mappings[i].module_id;
        }
    }
    return count;
}
