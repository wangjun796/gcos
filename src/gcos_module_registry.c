/**
 * @file gcos_module_registry.c
 * @brief GCOS Module Registry Implementation
 * 
 * Implements module registry management functions for code sharing
 * across multiple application instances.
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#include "gcos_vm.h"
#include "gcos_module_registry.h"
#include "gcos_platform.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Find a free slot in the module registry
 * 
 * @param vm VM instance
 * @return Free module ID, or 0xFF if registry is full
 */
static u8 find_free_registry_slot(GCOSVM *vm) {
    for (u8 i = 0; i < MAX_MODULES; i++) {
        if (!vm->module_registry[i].is_loaded) {
            return i;
        }
    }
    return 0xFF;  /* Registry full */
}

/**
 * @brief Compare two AIDs for equality
 * 
 * @param aid1 First AID
 * @param len1 Length of first AID
 * @param aid2 Second AID
 * @param len2 Length of second AID
 * @return true if AIDs are equal
 */
static bool aid_equal(const u8 *aid1, u8 len1, const u8 *aid2, u8 len2) {
    if (len1 != len2) {
        return false;
    }
    return memcmp(aid1, aid2, len1) == 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult module_registry_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Initialize all registry entries as empty */
    memset(vm->module_registry, 0, sizeof(vm->module_registry));
    vm->registry_count = 0;
    
    GCOS_PRINTF("[Module Registry] Initialized (%d slots)\n", MAX_MODULES);
    
    return GCOS_SUCCESS;
}

GCOSResult module_registry_register(GCOSVM *vm, const u8 *sef_data, u32 sef_size, u8 *module_id) {
    if (vm == NULL || sef_data == NULL || module_id == NULL) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Find a free slot */
    u8 new_module_id = find_free_registry_slot(vm);
    if (new_module_id == 0xFF) {
        GCOS_PRINTF("[Module Registry] ERROR: Registry full (max %d modules)\n", MAX_MODULES);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[new_module_id];
    
    /* Parse SEF header to extract module metadata */
    /* Note: Full SEF parsing will be done by the loader */
    /* Here we just set up the basic registry entry */
    
    reg->module_id = new_module_id;
    reg->is_loaded = true;   /* Mark as registered (will be fully loaded later) */
    reg->state = MODULE_LOADED;
    reg->instance_count = 0;
    memset(reg->instance_ids, 0xFF, sizeof(reg->instance_ids));
    
    /* TODO: Parse SEF file and populate:
     * - module_aid
     * - module_version
     * - code_base, code_size
     * - function_count, functions
     * - export_count, exports
     * - import_count, imports
     * - global_data_template, global_data_size
     */
    
    vm->registry_count++;
    *module_id = new_module_id;
    
    GCOS_PRINTF("[Module Registry] Registered module %d\n", new_module_id);
    
    return GCOS_SUCCESS;
}

GCOSModuleRegistry* module_registry_find_by_aid(GCOSVM *vm, const GCOSAID *aid) {
    if (vm == NULL || aid == NULL) {
        return NULL;
    }
    
    for (u8 i = 0; i < MAX_MODULES; i++) {
        GCOSModuleRegistry *reg = &vm->module_registry[i];
        
        if (reg->is_loaded && aid_equal(reg->module_aid.aid, reg->module_aid.length,
                                        aid->aid, aid->length)) {
            return reg;
        }
    }
    
    return NULL;  /* Not found */
}

GCOSModuleRegistry* module_registry_find_by_id(GCOSVM *vm, u8 module_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return NULL;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    if (!reg->is_loaded) {
        return NULL;
    }
    
    return reg;
}

GCOSResult module_registry_add_instance(GCOSVM *vm, u8 module_id, u8 app_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    if (!reg->is_loaded) {
        GCOS_PRINTF("[Module Registry] ERROR: Module %d not loaded\n", module_id);
        return GCOS_ERROR_MODULE_NOT_FOUND;
    }
    
    /* Check if instance list is full */
    if (reg->instance_count >= MAX_APPS_PER_MODULE) {
        GCOS_PRINTF("[Module Registry] ERROR: Module %d instance list full\n", module_id);
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    /* Add app_id to instance list */
    reg->instance_ids[reg->instance_count] = app_id;
    reg->instance_count++;
    
    GCOS_PRINTF("[Module Registry] Added app %d to module %d (total: %d instances)\n",
               app_id, module_id, reg->instance_count);
    
    return GCOS_SUCCESS;
}

GCOSResult module_registry_remove_instance(GCOSVM *vm, u8 module_id, u8 app_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    if (!reg->is_loaded) {
        return GCOS_ERROR_MODULE_NOT_FOUND;
    }
    
    /* Find and remove app_id from instance list */
    bool found = false;
    for (u8 i = 0; i < reg->instance_count; i++) {
        if (reg->instance_ids[i] == app_id) {
            /* Shift remaining entries */
            for (u8 j = i; j < reg->instance_count - 1; j++) {
                reg->instance_ids[j] = reg->instance_ids[j + 1];
            }
            reg->instance_ids[reg->instance_count - 1] = 0xFF;
            reg->instance_count--;
            found = true;
            break;
        }
    }
    
    if (!found) {
        GCOS_PRINTF("[Module Registry] WARNING: App %d not found in module %d\n",
                   app_id, module_id);
        return GCOS_ERROR_APP_NOT_FOUND;
    }
    
    GCOS_PRINTF("[Module Registry] Removed app %d from module %d (remaining: %d instances)\n",
               app_id, module_id, reg->instance_count);
    
    return GCOS_SUCCESS;
}

u8 module_registry_get_instance_count(GCOSVM *vm, u8 module_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return 0;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    if (!reg->is_loaded) {
        return 0;
    }
    
    return reg->instance_count;
}

GCOSResult module_registry_unload(GCOSVM *vm, u8 module_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    if (!reg->is_loaded) {
        return GCOS_ERROR_MODULE_NOT_FOUND;
    }
    
    /* Check if any instances are still using this module */
    if (reg->instance_count > 0) {
        GCOS_PRINTF("[Module Registry] ERROR: Cannot unload module %d, %d instances still active\n",
                   module_id, reg->instance_count);
        return GCOS_ERROR_MODULE_IN_USE;
    }
    
    /* Clear registry entry */
    reg->is_loaded = false;
    reg->state = MODULE_NOT_LOADED;
    reg->code_base = NULL;
    reg->code_size = 0;
    reg->function_count = 0;
    reg->export_count = 0;
    reg->import_count = 0;
    reg->global_data_template = NULL;
    reg->global_data_size = 0;
    
    vm->registry_count--;
    
    GCOS_PRINTF("[Module Registry] Unloaded module %d\n", module_id);
    
    return GCOS_SUCCESS;
}

GCOSResult module_registry_verify_dependencies(GCOSVM *vm, u8 module_id) {
    if (vm == NULL || module_id >= MAX_MODULES) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSModuleRegistry *reg = &vm->module_registry[module_id];
    
    if (!reg->is_loaded) {
        return GCOS_ERROR_MODULE_NOT_FOUND;
    }
    
    /* Check each imported module */
    for (u8 i = 0; i < reg->import_count; i++) {
        GCOSImportInfo *import = &reg->imports[i];
        
        if (!import->resolved) {
            /* Try to resolve by finding module with matching AID */
            GCOSModuleRegistry *dep_mod = module_registry_find_by_aid(vm, &import->module_aid);
            
            if (dep_mod == NULL) {
                GCOS_PRINTF("[Module Registry] ERROR: Dependency not found for module %d\n",
                           module_id);
                return GCOS_ERROR_MODULE_NOT_FOUND;
            }
            
            /* TODO: Check version compatibility */
            /* For now, just mark as resolved */
            import->resolved = true;
            import->resolved_module_id = dep_mod->module_id;
            
            GCOS_PRINTF("[Module Registry] Resolved dependency: module %d -> module %d\n",
                       module_id, dep_mod->module_id);
        }
    }
    
    GCOS_PRINTF("[Module Registry] All dependencies satisfied for module %d\n", module_id);
    
    return GCOS_SUCCESS;
}

void module_registry_dump(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    GCOS_PRINTF("\n=== Module Registry Dump ===\n");
    GCOS_PRINTF("Total registered modules: %d / %d\n\n", vm->registry_count, MAX_MODULES);
    
    for (u8 i = 0; i < MAX_MODULES; i++) {
        GCOSModuleRegistry *reg = &vm->module_registry[i];
        
        if (reg->is_loaded) {
            GCOS_PRINTF("[%d] Module ID: %d\n", i, reg->module_id);
            GCOS_PRINTF("    State: %s\n", 
                       reg->state == MODULE_VERIFIED ? "VERIFIED" :
                       reg->state == MODULE_LOADED ? "LOADED" : "ERROR");
            GCOS_PRINTF("    Instances: %d\n", reg->instance_count);
            GCOS_PRINTF("    Code size: %u bytes\n", reg->code_size);
            GCOS_PRINTF("    Functions: %u\n", reg->function_count);
            GCOS_PRINTF("    Exports: %u\n", reg->export_count);
            GCOS_PRINTF("    Imports: %u\n", reg->import_count);
            GCOS_PRINTF("    Global data: %u bytes\n", reg->global_data_size);
            GCOS_PRINTF("\n");
        }
    }
    
    GCOS_PRINTF("==========================\n\n");
}
