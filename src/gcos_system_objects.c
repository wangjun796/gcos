/**
 * @file gcos_system_objects.c
 * @brief GCOS System Objects Implementation
 * 
 * Implements system object management using eflash object mechanism.
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#include "gcos_vm.h"
#include "gcos_platform.h"  /* For SYS_OBJ_* debug macros */
#include "gcos_system_objects.h"
#include "eflash_ftl.h"
#include "eflash_mgr.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Variables
 * ============================================================================ */

static GCOS_SystemConfigObject g_sys_config;
static GCOS_ModuleRegistryObject *g_module_registry_obj = NULL;
static GCOS_AppInstanceObject *g_app_instance_obj = NULL;
static GCOS_GRTObject *g_grt_obj = NULL;
/* Note: No free list object - eflash manages it at LPN 8-11 */

/* ============================================================================
 * CRC32 Implementation
 * ============================================================================ */

u32 gcos_calc_crc32(const void *data, u32 length) {
    const u8 *bytes = (const u8 *)data;
    u32 crc = 0xFFFFFFFFU;
    const u32 poly = 0xEDB88320U;
    
    for (u32 i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ poly;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFFU;
}

/* ============================================================================
 * Object Access Wrappers
 * ============================================================================ */

GCOSResult gcos_object_read(u16 obj_id, void *buffer, u32 buffer_size, u32 *out_actual_size) {
    obj_header_t hdr;
    
    /* Get object header */
    if (eflash_ftl_obj_get_header(obj_id, &hdr) != 0) {
        printf("[SYS_OBJ] ERROR: Failed to get header for object %u\n", obj_id);
        return GCOS_ERROR_INVALID_PARAM;  /* Object not found */
    }
    
    /* Check buffer size */
    if (buffer_size < hdr.body_size) {
        printf("[SYS_OBJ] ERROR: Buffer too small (%u < %u)\n", buffer_size, hdr.body_size);
        return GCOS_ERROR_INVALID_PARAM;  /* Buffer too small */
    }
    
    /* Read object data using logical address */
    if (eflash_ftl_read_logical(hdr.body_addr, (uint8_t *)buffer, hdr.body_size) != 0) {
        printf("[SYS_OBJ] ERROR: Failed to read object %u from logical addr 0x%08X\n",
               obj_id, hdr.body_addr);
        return GCOS_ERROR_INVALID_PARAM;  /* Flash read error */
    }
    
    if (out_actual_size) {
        *out_actual_size = hdr.body_size;
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_object_write(u16 obj_id, const void *data, u32 size) {
    obj_header_t hdr;
    
    /* Get object header */
    if (eflash_ftl_obj_get_header(obj_id, &hdr) != 0) {
        printf("[SYS_OBJ] ERROR: Failed to get header for object %u\n", obj_id);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Check size match */
    if (size != hdr.body_size) {
        printf("[SYS_OBJ] WARNING: Size mismatch (%u != %u), updating header...\n",
               size, hdr.body_size);
        hdr.body_size = size;
        eflash_ftl_obj_set_header(obj_id, &hdr);
    }
    
    /* Write object data using logical address */
    if (eflash_ftl_write_logical(hdr.body_addr, (const uint8_t *)data, (int16_t)size) != 0) {
        printf("[SYS_OBJ] ERROR: Failed to write object %u to logical addr 0x%08X\n",
               obj_id, hdr.body_addr);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_object_allocate(u16 pkg_id, u16 class_id, u32 size,
                                u16 *out_obj_id, uint32_t *out_logic_addr) {
    obj_header_t hdr;
    uint32_t logic_addr;
    
    /* TODO: Find next available object ID */
    /* For now, use a simple heuristic (this needs proper implementation) */
    static u16 next_obj_id = 7;  /* Start after system objects */
    
    if (next_obj_id >= BASE_OBJ_CAPACITY) {
        printf("[SYS_OBJ] ERROR: No more object IDs available\n");
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    /* Allocate space in Flash */
    if (eflash_mgr_alloc(size, &logic_addr) != 0) {
        printf("[SYS_OBJ] ERROR: Failed to allocate %u bytes\n", size);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    /* Set object header */
    memset(&hdr, 0, sizeof(hdr));
    hdr.pkg_id = pkg_id;
    hdr.class_id = class_id;
    hdr.type = OBJ_TYPE_NORMAL;
    hdr.body_addr = logic_addr;
    hdr.body_size = size;
    
    if (eflash_ftl_obj_set_header(next_obj_id, &hdr) != 0) {
        printf("[SYS_OBJ] ERROR: Failed to set header for object %u\n", next_obj_id);
        eflash_mgr_free(logic_addr, size);  /* Rollback */
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    *out_obj_id = next_obj_id;
    *out_logic_addr = logic_addr;
    next_obj_id++;
    
    printf("[SYS_OBJ] Allocated object %u at logical addr 0x%08X (size=%u)\n",
           *out_obj_id, *out_logic_addr, size);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_object_free(u16 obj_id) {
    obj_header_t hdr;
    
    /* Get object header */
    if (eflash_ftl_obj_get_header(obj_id, &hdr) != 0) {
        printf("[SYS_OBJ] ERROR: Object %u not found\n", obj_id);
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Free the data body */
    eflash_mgr_free(hdr.body_addr, hdr.body_size);
    
    /* Clear object header */
    memset(&hdr, 0, sizeof(hdr));
    eflash_ftl_obj_set_header(obj_id, &hdr);
    
    printf("[SYS_OBJ] Freed object %u\n", obj_id);
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * System Initialization
 * ============================================================================ */

GCOSResult gcos_system_objects_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    printf("[SYS_OBJ] Initializing GCOS system objects...\n");
    
    /* Check if system configuration object exists */
    obj_header_t sys_config_hdr;
    int ret = eflash_ftl_obj_get_header(GCOS_OBJ_ID_SYS_CONFIG, &sys_config_hdr);
    
    if (ret != 0 || sys_config_hdr.pkg_id != GCOS_PKG_ID || 
        sys_config_hdr.class_id != GCOS_CLASS_SYS_CONFIG) {
        /* First boot: create all system objects */
        printf("[SYS_OBJ] First boot detected (Obj 5 not found or invalid)\n");
        printf("[SYS_OBJ] Creating system objects...\n");
        return gcos_system_objects_create(vm);
    }
    
    /* Normal boot: load existing objects */
    printf("[SYS_OBJ] System objects exist, loading from Flash...\n");
    return gcos_system_objects_load(vm);
}

GCOSResult gcos_system_objects_create(GCOSVM *vm) {
    uint32_t logic_addr;
    obj_header_t hdr;
    int ret;
    
    SYS_OBJ_INFO("=== Creating System Objects ===\n");
    SYS_OBJ_TRACE("  VM pointer: %p\n", (void*)vm);
    SYS_OBJ_TRACE("  Flash size: %u bytes\n", vm->flash_size);
    
    /* Step 1: Create Object 5 - System Configuration */
    SYS_OBJ_INFO("Creating System Config (Obj ID 5)...\n");
    SYS_OBJ_DBG("  Size: %u bytes\n", sizeof(g_sys_config));
    
    memset(&g_sys_config, 0, sizeof(g_sys_config));
    g_sys_config.magic = GCOS_MAGIC_SYSC;
    g_sys_config.version = 0x0100;
    g_sys_config.flags = GCOS_SYS_FLAG_INITIALIZED;
    g_sys_config.max_modules = GCOS_DEFAULT_MAX_MODULES;
    g_sys_config.max_apps = GCOS_DEFAULT_MAX_APPS;
    g_sys_config.max_grt_entries = GCOS_DEFAULT_MAX_GRT;
    g_sys_config.flash_total_size = vm->flash_size;
    
    /* Set object ID references */
    g_sys_config.module_registry_obj_id = GCOS_OBJ_ID_MODULE_REGISTRY;
    g_sys_config.app_instance_obj_id = GCOS_OBJ_ID_APP_INSTANCE;
    g_sys_config.grt_obj_id = GCOS_OBJ_ID_GRT;
    /* Note: Obj ID 4 and 6 are reserved but not used */
    
    /* Calculate checksum (exclude checksum field itself) */
    g_sys_config.checksum = gcos_calc_crc32(&g_sys_config, 
                                            sizeof(g_sys_config) - 4);
    SYS_OBJ_DBG("  Checksum: 0x%08X\n", g_sys_config.checksum);
    
    /* Allocate and write to Flash */
    SYS_OBJ_DBG("  Calling eflash_mgr_alloc(size=%u)...\n", sizeof(g_sys_config));
    ret = eflash_mgr_alloc(sizeof(g_sys_config), &logic_addr);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to allocate space for System Config (ret=%d)\n", ret);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    SYS_OBJ_DBG("  Allocated at logical_addr=0x%08X\n", logic_addr);
    
    SYS_OBJ_DBG("  Calling eflash_ftl_write_logical(addr=0x%08X, size=%d)...\n", 
                logic_addr, (int)sizeof(g_sys_config));
    ret = eflash_ftl_write_logical(logic_addr, (const uint8_t *)&g_sys_config, (int16_t)sizeof(g_sys_config));
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to write System Config (ret=%d)\n", ret);
        return GCOS_ERROR_INVALID_PARAM;
    }
    SYS_OBJ_DBG("  Write successful\n");
    
    /* Set object header */
    SYS_OBJ_DBG("  Setting object header...\n");
    memset(&hdr, 0, sizeof(hdr));
    hdr.pkg_id = GCOS_PKG_ID;
    hdr.class_id = GCOS_CLASS_SYS_CONFIG;
    hdr.type = OBJ_TYPE_NORMAL;
    hdr.body_addr = logic_addr;
    hdr.body_size = sizeof(g_sys_config);
    
    ret = eflash_ftl_obj_set_header(GCOS_OBJ_ID_SYS_CONFIG, &hdr);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to set header for Obj 5 (ret=%d)\n", ret);
        return GCOS_ERROR_INVALID_PARAM;
    }
    SYS_OBJ_DBG("  Header set successfully\n");
    
    SYS_OBJ_INFO("  System Config created at logical addr 0x%08X\n", logic_addr);
    
    /* Step 2: Create Object 1 - Module Registry */
    printf("[SYS_OBJ] Creating Module Registry (Obj ID 1)...\n");
    gcos_create_module_registry_object(vm);
    
    /* Step 3: Create Object 2 - App Instance Table */
    printf("[SYS_OBJ] Creating App Instance Table (Obj ID 2)...\n");
    gcos_create_app_instance_object(vm);
    
    /* Step 4: Create Object 3 - GRT */
    printf("[SYS_OBJ] Creating GRT (Obj ID 3)...\n");
    gcos_create_grt_object(vm);
    
    /* Note: Obj ID 4 is NOT created - eflash already manages free list at LPN 8-11 */
    printf("[SYS_OBJ] Skipping Obj ID 4 (eflash manages free list)\n");
    
    /* Store pointer in VM */
    vm->system_config = &g_sys_config;
    
    printf("[SYS_OBJ] === All System Objects Created Successfully ===\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_system_objects_load(GCOSVM *vm) {
    obj_header_t hdr;
    uint32_t actual_size;
    
    printf("[SYS_OBJ] === Loading System Objects ===\n");
    
    /* Load Object 5 - System Configuration */
    printf("[SYS_OBJ] Loading System Config (Obj ID 5)...\n");
    
    GCOSResult ret = gcos_object_read(GCOS_OBJ_ID_SYS_CONFIG, &g_sys_config,
                                      sizeof(g_sys_config), &actual_size);
    if (ret != GCOS_SUCCESS) {
        printf("[SYS_OBJ] ERROR: Failed to load System Config\n");
        return ret;
    }
    
    /* Validate */
    if (!gcos_validate_system_config(&g_sys_config)) {
        printf("[SYS_OBJ] ERROR: System Config validation failed\n");
        return GCOS_ERROR_VALIDATION_FAILED;
    }
    
    vm->system_config = &g_sys_config;
    
    printf("[SYS_OBJ]   System Config loaded:\n");
    printf("[SYS_OBJ]     Version: %u.%u\n", 
           g_sys_config.version >> 8, g_sys_config.version & 0xFF);
    printf("[SYS_OBJ]     Modules: %u/%u\n", 
           g_sys_config.module_count, g_sys_config.max_modules);
    printf("[SYS_OBJ]     Apps: %u/%u\n", 
           g_sys_config.app_count, g_sys_config.max_apps);
    printf("[SYS_OBJ]     GRT: %u/%u\n", 
           g_sys_config.grt_used_count, g_sys_config.max_grt_entries);
    
    /* Load Object 1 - Module Registry */
    printf("[SYS_OBJ] Loading Module Registry (Obj ID %u)...\n",
           g_sys_config.module_registry_obj_id);
    gcos_load_module_registry_object(vm);
    
    /* Load Object 2 - App Instance Table */
    printf("[SYS_OBJ] Loading App Instance Table (Obj ID %u)...\n",
           g_sys_config.app_instance_obj_id);
    gcos_load_app_instance_object(vm);
    
    /* Load Object 3 - GRT */
    printf("[SYS_OBJ] Loading GRT (Obj ID %u)...\n",
           g_sys_config.grt_obj_id);
    gcos_load_grt_object(vm);
    
    printf("[SYS_OBJ] === All System Objects Loaded Successfully ===\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_system_objects_save(GCOSVM *vm) {
    if (vm == NULL || vm->system_config == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    printf("[SYS_OBJ] Saving system objects to Flash...\n");
    
    /* Update System Config checksum */
    g_sys_config.checksum = gcos_calc_crc32(&g_sys_config, 
                                            sizeof(g_sys_config) - 4);
    g_sys_config.flags |= GCOS_SYS_FLAG_PERSISTENT;
    
    /* Save Object 5 - System Configuration */
    GCOSResult ret = gcos_object_write(GCOS_OBJ_ID_SYS_CONFIG, 
                                       &g_sys_config, sizeof(g_sys_config));
    if (ret != GCOS_SUCCESS) {
        printf("[SYS_OBJ] ERROR: Failed to save System Config\n");
        return ret;
    }
    
    /* Save Object 1 - Module Registry */
    gcos_module_registry_object_update(vm);
    
    /* Save Object 2 - App Instance Table */
    gcos_app_instance_object_update(vm);
    
    /* Save Object 3 - GRT */
    gcos_grt_object_update(vm);
    
    printf("[SYS_OBJ] All system objects saved successfully\n");
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Validation Functions
 * ============================================================================ */

bool gcos_validate_system_config(const GCOS_SystemConfigObject *config) {
    if (config == NULL) {
        return false;
    }
    
    /* Check magic number */
    if (config->magic != GCOS_MAGIC_SYSC) {
        printf("[SYS_OBJ] Validation failed: Invalid magic (0x%08X)\n", config->magic);
        return false;
    }
    
    /* Check version */
    if ((config->version >> 8) != 1) {
        printf("[SYS_OBJ] Validation failed: Unsupported version %u.%u\n",
               config->version >> 8, config->version & 0xFF);
        return false;
    }
    
    /* Check flags */
    if (!(config->flags & GCOS_SYS_FLAG_INITIALIZED)) {
        printf("[SYS_OBJ] Validation failed: Not initialized\n");
        return false;
    }
    
    /* Verify checksum */
    u32 calc_checksum = gcos_calc_crc32(config, sizeof(*config) - 4);
    if (calc_checksum != config->checksum) {
        printf("[SYS_OBJ] Validation failed: Checksum mismatch\n");
        printf("[SYS_OBJ]   Expected: 0x%08X, Got: 0x%08X\n",
               config->checksum, calc_checksum);
        return false;
    }
    
    /* Verify object ID references */
    if (config->module_registry_obj_id != GCOS_OBJ_ID_MODULE_REGISTRY) {
        printf("[SYS_OBJ] WARNING: Module Registry Obj ID mismatch (%u != %u)\n",
               config->module_registry_obj_id, GCOS_OBJ_ID_MODULE_REGISTRY);
    }
    
    return true;
}

GCOS_SystemConfigObject* gcos_system_config_get(GCOSVM *vm) {
    if (vm == NULL) {
        return NULL;
    }
    
    return vm->system_config;
}

/* ============================================================================
 * Helper Functions - Create Individual Objects
 * ============================================================================ */

static GCOSResult gcos_create_module_registry_object(GCOSVM *vm) {
    uint32_t logic_addr;
    obj_header_t hdr;
    int ret;
    
    SYS_OBJ_INFO("Creating Module Registry (Obj ID 1)...\n");
    
    /* Calculate size needed */
    u32 size = sizeof(GCOS_ModuleRegistryObject) + 
               (GCOS_DEFAULT_MAX_MODULES - 1) * sizeof(GCOSModuleRegistry);
    SYS_OBJ_DBG("  Calculated size: %u bytes\n", size);
    SYS_OBJ_DBG("    Base struct: %u bytes\n", sizeof(GCOS_ModuleRegistryObject));
    SYS_OBJ_DBG("    Per module: %u bytes x %u modules\n", 
                sizeof(GCOSModuleRegistry), GCOS_DEFAULT_MAX_MODULES);
    
    /* Allocate space */
    SYS_OBJ_DBG("  Calling eflash_mgr_alloc(size=%u)...\n", size);
    ret = eflash_mgr_alloc(size, &logic_addr);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to allocate Module Registry (ret=%d)\n", ret);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    SYS_OBJ_DBG("  Allocated at logical_addr=0x%08X\n", logic_addr);
    
    /* Initialize in memory first */
    SYS_OBJ_DBG("  Allocating memory for initialization...\n");
    GCOS_ModuleRegistryObject *obj = malloc(size);
    if (obj == NULL) {
        SYS_OBJ_ERR("malloc failed for size %u\n", size);
        eflash_mgr_free(logic_addr, size);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(obj, 0, size);
    obj->magic = GCOS_MAGIC_MODR;
    obj->version = 0x0100;
    obj->module_count = 0;
    obj->max_modules = GCOS_DEFAULT_MAX_MODULES;
    obj->checksum = 0;  /* Will be calculated */
    SYS_OBJ_DBG("  Initialized object in memory\n");
    SYS_OBJ_DBG("    Magic: 0x%08X\n", obj->magic);
    SYS_OBJ_DBG("    Version: 0x%04X\n", obj->version);
    
    /* Write to Flash */
    SYS_OBJ_DBG("  Calling eflash_ftl_write_logical(addr=0x%08X, size=%d)...\n", 
                logic_addr, (int)size);
    ret = eflash_ftl_write_logical(logic_addr, (const uint8_t *)obj, (int16_t)size);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to write Module Registry (ret=%d)\n", ret);
        free(obj);
        return GCOS_ERROR_INVALID_PARAM;
    }
    SYS_OBJ_DBG("  Write successful\n");
    free(obj);
    
    /* Set object header */
    SYS_OBJ_DBG("  Setting object header...\n");
    memset(&hdr, 0, sizeof(hdr));
    hdr.pkg_id = GCOS_PKG_ID;
    hdr.class_id = GCOS_CLASS_MODULE_REGISTRY;
    hdr.type = OBJ_TYPE_NORMAL;
    hdr.body_addr = logic_addr;
    hdr.body_size = size;
    
    ret = eflash_ftl_obj_set_header(GCOS_OBJ_ID_MODULE_REGISTRY, &hdr);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to set header for Obj 1 (ret=%d)\n", ret);
        return GCOS_ERROR_INVALID_PARAM;
    }
    SYS_OBJ_DBG("  Header set successfully\n");
    
    SYS_OBJ_INFO("  Module Registry created at 0x%08X (size=%u)\n", logic_addr, size);
    return GCOS_SUCCESS;
}

static GCOSResult gcos_create_app_instance_object(GCOSVM *vm) {
    uint32_t logic_addr;
    obj_header_t hdr;
    
    u32 size = sizeof(GCOS_AppInstanceObject) + 
               (GCOS_DEFAULT_MAX_APPS - 1) * sizeof(GCOSAppInstance);
    
    if (eflash_mgr_alloc(size, &logic_addr) != 0) {
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    GCOS_AppInstanceObject *obj = malloc(size);
    memset(obj, 0, size);
    obj->magic = GCOS_MAGIC_APPT;
    obj->version = 0x0100;
    obj->app_count = 0;
    obj->max_apps = GCOS_DEFAULT_MAX_APPS;
    
    eflash_ftl_write_logical(logic_addr, (uint8_t *)obj, (int16_t)size);
    free(obj);
    
    memset(&hdr, 0, sizeof(hdr));
    hdr.pkg_id = GCOS_PKG_ID;
    hdr.class_id = GCOS_CLASS_APP_INSTANCE;
    hdr.type = OBJ_TYPE_NORMAL;
    hdr.body_addr = logic_addr;
    hdr.body_size = size;
    eflash_ftl_obj_set_header(GCOS_OBJ_ID_APP_INSTANCE, &hdr);
    
    printf("[SYS_OBJ]   App Instance Table created at 0x%08X (size=%u)\n", logic_addr, size);
    return GCOS_SUCCESS;
}

static GCOSResult gcos_create_grt_object(GCOSVM *vm) {
    uint32_t logic_addr;
    obj_header_t hdr;
    int ret;
    
    SYS_OBJ_INFO("Creating GRT (Obj ID 3)...\n");
    
    u32 size = sizeof(GCOS_GRTObject) + 
               (GCOS_DEFAULT_MAX_GRT - 1) * sizeof(GCOSGlobalRefEntry);
    SYS_OBJ_DBG("  Calculated size: %u bytes\n", size);
    SYS_OBJ_DBG("    Base struct: %u bytes\n", sizeof(GCOS_GRTObject));
    SYS_OBJ_DBG("    Per entry: %u bytes x %u entries\n", 
                sizeof(GCOSGlobalRefEntry), GCOS_DEFAULT_MAX_GRT);
    
    SYS_OBJ_DBG("  Calling eflash_mgr_alloc(size=%u)...\n", size);
    ret = eflash_mgr_alloc(size, &logic_addr);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to allocate GRT (ret=%d)\n", ret);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    SYS_OBJ_DBG("  Allocated at logical_addr=0x%08X\n", logic_addr);
    
    SYS_OBJ_DBG("  Allocating memory for initialization...\n");
    GCOS_GRTObject *obj = malloc(size);
    if (obj == NULL) {
        SYS_OBJ_ERR("malloc failed for size %u\n", size);
        eflash_mgr_free(logic_addr, size);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(obj, 0, size);
    obj->magic = GCOS_MAGIC_GRT0;
    obj->version = 0x0100;
    obj->capacity = GCOS_DEFAULT_MAX_GRT;
    obj->used_count = 0;
    SYS_OBJ_DBG("  Initialized object in memory\n");
    SYS_OBJ_DBG("    Magic: 0x%08X\n", obj->magic);
    
    SYS_OBJ_DBG("  Calling eflash_ftl_write_logical(addr=0x%08X, size=%d)...\n", 
                logic_addr, (int)size);
    ret = eflash_ftl_write_logical(logic_addr, (const uint8_t *)obj, (int16_t)size);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to write GRT (ret=%d)\n", ret);
        free(obj);
        return GCOS_ERROR_INVALID_PARAM;
    }
    SYS_OBJ_DBG("  Write successful\n");
    free(obj);
    
    SYS_OBJ_DBG("  Setting object header...\n");
    memset(&hdr, 0, sizeof(hdr));
    hdr.pkg_id = GCOS_PKG_ID;
    hdr.class_id = GCOS_CLASS_GRT;
    hdr.type = OBJ_TYPE_NORMAL;
    hdr.body_addr = logic_addr;
    hdr.body_size = size;
    
    ret = eflash_ftl_obj_set_header(GCOS_OBJ_ID_GRT, &hdr);
    if (ret != 0) {
        SYS_OBJ_ERR("Failed to set header for Obj 3 (ret=%d)\n", ret);
        return GCOS_ERROR_INVALID_PARAM;
    }
    SYS_OBJ_DBG("  Header set successfully\n");
    
    SYS_OBJ_INFO("  GRT created at 0x%08X (size=%u)\n", logic_addr, size);
    return GCOS_SUCCESS;
}

/* Note: gcos_create_free_list_object() is NOT implemented.
 * eflash already manages the free list at LPN 8-11 via eflash_mgr.
 * GCOS uses eflash_mgr_alloc() and eflash_mgr_free() directly.
 */

/* Stub functions for loading individual objects */
static GCOSResult gcos_load_module_registry_object(GCOSVM *vm) {
    /* TODO: Implement */
    return GCOS_SUCCESS;
}

static GCOSResult gcos_load_app_instance_object(GCOSVM *vm) {
    /* TODO: Implement */
    return GCOS_SUCCESS;
}

static GCOSResult gcos_load_grt_object(GCOSVM *vm) {
    /* TODO: Implement */
    return GCOS_SUCCESS;
}

/* Stub functions for updating individual objects */
GCOSResult gcos_module_registry_object_update(GCOSVM *vm) {
    /* TODO: Implement */
    return GCOS_SUCCESS;
}

GCOSResult gcos_app_instance_object_update(GCOSVM *vm) {
    /* TODO: Implement */
    return GCOS_SUCCESS;
}

GCOSResult gcos_grt_object_update(GCOSVM *vm) {
    /* TODO: Implement */
    return GCOS_SUCCESS;
}
