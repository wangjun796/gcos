/**
 * @file gcos_symbol_resolver.c
 * @brief GCOS Symbol Resolution Implementation
 * 
 * Implements complete symbol resolution system:
 * - Import/export symbol management
 * - System module pre-registration (like iwasm)
 * - 16-bit compact addressing with global reference table (like cref)
 * - Cross-module symbol linking
 */

#include "gcos_symbol_resolver.h"
#include "gcos_vm.h"
#include "gcos_platform.h"  /* For GCOS_PRINTF */
#include "eflash_ftl.h"     /* For Flash operations */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>         /* For malloc/free */

/* External function declarations */
/* extern u32 calculate_crc32(const u8 *data, u32 size);  /* From gcos_flash_storage.c */

/**
 * @brief Calculate CRC32 checksum (simple implementation)
 * @param data Data buffer
 * @param size Data size
 * @return CRC32 checksum
 */
static u32 calculate_crc32_local(const u8 *data, u32 size) {
    u32 crc = 0xFFFFFFFF;
    
    for (u32 i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

/* Forward declarations for page cache functions */
static void init_page_cache(void);

/* Global symbol resolver instance */
static GCOSSymbolResolver g_symbol_resolver;
static bool g_resolver_initialized = false;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Check if address is a global reference
 */
static inline bool is_global_ref(u16 addr) {
    return (addr & ADDR_FLAG_GLOBAL) != 0;
}

/**
 * @brief Extract index from compact address
 */
static inline u16 get_index(u16 addr) {
    return addr & ADDR_MASK_INDEX;
}

/**
 * @brief Create compact local address
 */
static inline u16 make_local_addr(u16 index) {
    return index & ADDR_MASK_INDEX;  /* Ensure bit 15 is clear */
}

/**
 * @brief Create compact global address
 */
static inline u16 make_global_addr(u16 index) {
    return (index & ADDR_MASK_INDEX) | ADDR_FLAG_GLOBAL;
}

/**
 * @brief Compare two AIDs
 */
static bool aid_equals(const u8 *aid1, u8 len1, const u8 *aid2, u8 len2) {
    if (len1 != len2) return false;
    return memcmp(aid1, aid2, len1) == 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_symbol_resolver_init(GCOSVM *vm) {
    if (g_resolver_initialized) {
        return GCOS_OK;
    }
    
    memset(&g_symbol_resolver, 0, sizeof(GCOSSymbolResolver));
    
    /* Initialize global reference table (Flash-backed with dynamic expansion)
     * Initial capacity: MAX_GLOBAL_REFS (64 entries)
     * Can be expanded up to MAX_GLOBAL_REFS_MAX (256 entries) */
    g_symbol_resolver.global_ref_count = 0;
    g_symbol_resolver.global_ref_capacity = MAX_GLOBAL_REFS;
    g_symbol_resolver.global_ref_table_ext = NULL;
    g_symbol_resolver.global_ref_flash_offset = 0;
    
    for (int i = 0; i < MAX_GLOBAL_REFS; i++) {
        g_symbol_resolver.global_ref_table[i].is_valid = false;
    }
    
    /* Initialize system modules */
    g_symbol_resolver.system_module_count = 0;
    
    /* Initialize statistics */
    g_symbol_resolver.total_resolutions = 0;
    g_symbol_resolver.failed_resolutions = 0;
    
    /* Initialize page cache (Flash optimization) */
    init_page_cache();
    
    g_resolver_initialized = true;
    
    GCOS_PRINTF("[Symbol Resolver] Initialized (base table: %u entries, %u bytes)\n",
               MAX_GLOBAL_REFS, (unsigned int)(MAX_GLOBAL_REFS * sizeof(GCOSGlobalRefEntry)));
    GCOS_PRINTF("[Symbol Resolver] Max capacity: %u entries (expandable)\n", MAX_GLOBAL_REFS_MAX);
    
    /* Try to load from Flash (if exists) */
    GCOSResult ret = gcos_symbol_load_global_ref_table_from_flash(vm);
    if (ret == GCOS_SUCCESS) {
        GCOS_PRINTF("[Symbol Resolver] Loaded from Flash successfully\n");
    } else {
        GCOS_PRINTF("[Symbol Resolver] No Flash data found (fresh start)\n");
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_symbol_register_system_module(GCOSVM *vm, const u8 *aid, 
                                               u8 aid_length, const char *name) {
    if (!g_resolver_initialized) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (g_symbol_resolver.system_module_count >= MAX_SYSTEM_MODULES) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Max system modules reached\n");
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[g_symbol_resolver.system_module_count];
    
    /* Copy AID */
    memcpy(sys_mod->aid.aid, aid, aid_length < AID_MAX_LENGTH ? aid_length : AID_MAX_LENGTH);
    sys_mod->aid.length = aid_length;
    sys_mod->aid_length = aid_length;
    
    /* Store name pointer (system modules use static strings) */
    sys_mod->name = name;
    
    /* Initialize exports */
    sys_mod->export_count = 0;
    sys_mod->is_registered = true;
    
    GCOS_PRINTF("[Symbol Resolver] Registered system module '%s' (AID=", name);
    for (u8 i = 0; i < aid_length && i < 16; i++) {
        GCOS_PRINTF("%02X", aid[i]);
    }
    GCOS_PRINTF(")\n");
    
    g_symbol_resolver.system_module_count++;
    return GCOS_OK;
}

GCOSResult gcos_symbol_add_system_export(GCOSVM *vm, const char *system_module_name,
                                          const char *func_name, void *func_ptr,
                                          u8 param_count, u8 return_size) {
    if (!g_resolver_initialized) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Find system module */
    int sys_idx = gcos_symbol_find_system_module_by_name(vm, system_module_name);
    if (sys_idx < 0) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: System module '%s' not found\n", system_module_name);
        return GCOS_ERR_MODULE_NOT_FOUND;
    }
    
    GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[sys_idx];
    
    if (sys_mod->export_count >= MAX_EXPORT_SYMBOLS) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Max exports reached for '%s'\n", system_module_name);
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Add export */
    u8 exp_idx = sys_mod->export_count;
    sys_mod->exports[exp_idx].name = func_name;  /* Store pointer to static string */
    sys_mod->exports[exp_idx].func_ptr = func_ptr;
    sys_mod->exports[exp_idx].param_count = param_count;
    sys_mod->exports[exp_idx].return_size = return_size;
    
    sys_mod->export_count++;
    
    GCOS_PRINTF("[Symbol Resolver] Added export '%s' to '%s'\n", func_name, system_module_name);
    return GCOS_OK;
}

GCOSResult gcos_symbol_resolve_imports(GCOSVM *vm, u8 module_id) {
    if (!g_resolver_initialized || module_id >= vm->module_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOSModule *module = &vm->modules[module_id];
    u8 import_count = g_symbol_resolver.import_counts[module_id];
    
    GCOS_PRINTF("[Symbol Resolver] Resolving %u imports for module %u\n", 
               import_count, module_id);
    
    for (u8 i = 0; i < import_count; i++) {
        GCOSImportSymbol *import = &g_symbol_resolver.import_tables[module_id][i];
        
        if (import->is_resolved) {
            continue;  /* Already resolved */
        }
        
        /* Decode COS3 format: high 5 bits = module idx, low 11 bits = func idx */
        u8 target_module_idx = (import->module_idx_func_idx >> 11) & 0x1F;
        u16 target_func_idx = import->module_idx_func_idx & 0x7FF;
        
        GCOS_PRINTF("[Symbol Resolver]   Import %u: module=%u, func=%u\n",
                   i, target_module_idx, target_func_idx);
        
        u32 target_address = 0;
        bool found = false;
        
        /* Check if it's a system module (module_idx >= 0x10 indicates system) */
        if (target_module_idx >= 0x10) {
            /* System module reference */
            u8 sys_module_idx = target_module_idx - 0x10;
            
            if (sys_module_idx < g_symbol_resolver.system_module_count) {
                GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[sys_module_idx];
                
                if (target_func_idx < sys_mod->export_count) {
                    /* Found system function - create global reference */
                    target_address = (u32)(uintptr_t)sys_mod->exports[target_func_idx].func_ptr;
                    
                    /* Create global reference entry */
                    u16 global_ref = gcos_symbol_create_global_ref(vm, target_address, 
                                                                    0xFF, target_func_idx);
                    
                    if (global_ref != SYMBOL_IDX_INVALID) {
                        import->resolved_address = global_ref;
                        import->is_resolved = true;
                        found = true;
                        
                        GCOS_PRINTF("[Symbol Resolver]     ✓ Resolved to system func (global ref 0x%04X)\n",
                                   global_ref);
                    }
                }
            }
        }
        else {
            /* Regular module reference */
            if (target_module_idx < vm->module_count) {
                GCOSModule *target_module = &vm->modules[target_module_idx];
                
                /* Check if target module has the function */
                if (target_func_idx < target_module->function_count) {
                    /* Calculate logical address of target function */
                    /* For now, use simplified calculation - in real impl would use actual code base */
                    target_address = 0x1000 + (target_func_idx * 64);  /* Simplified placeholder */
                    
                    /* Create global reference */
                    u16 global_ref = gcos_symbol_create_global_ref(vm, target_address,
                                                                    target_module_idx, target_func_idx);
                    
                    if (global_ref != SYMBOL_IDX_INVALID) {
                        import->resolved_address = global_ref;
                        import->is_resolved = true;
                        found = true;
                        
                        GCOS_PRINTF("[Symbol Resolver]     ✓ Resolved to module %u func %u (global ref 0x%04X)\n",
                                   target_module_idx, target_func_idx, global_ref);
                    }
                }
            }
        }
        
        if (!found) {
            GCOS_PRINTF("[Symbol Resolver]     ✗ FAILED to resolve import\n");
            g_symbol_resolver.failed_resolutions++;
            return GCOS_ERR_MODULE_NOT_FOUND;
        }
        
        g_symbol_resolver.total_resolutions++;
    }
    
    GCOS_PRINTF("[Symbol Resolver] All imports resolved for module %u\n", module_id);
    return GCOS_OK;
}

GCOSResult gcos_symbol_add_export(GCOSVM *vm, u8 module_id, u16 function_index,
                                   u32 logical_address, const char *name) {
    if (!g_resolver_initialized || module_id >= MAX_MODULES) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    u8 export_count = g_symbol_resolver.export_counts[module_id];
    
    if (export_count >= MAX_EXPORT_SYMBOLS) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Max exports reached for module %u\n", module_id);
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    GCOSExportSymbol *export = &g_symbol_resolver.export_tables[module_id][export_count];
    export->function_index = function_index;
    export->logical_address = logical_address;
    
    if (name) {
        strncpy(export->name, name, 31);
        export->name[31] = '\0';
    } else {
        snprintf(export->name, 32, "func_%u", function_index);
    }
    
    g_symbol_resolver.export_counts[module_id]++;
    
    GCOS_PRINTF("[Symbol Resolver] Added export '%s' for module %u func %u\n",
               export->name, module_id, function_index);
    
    return GCOS_OK;
}

GCOSResult gcos_symbol_add_import(GCOSVM *vm, u8 module_id, u16 module_idx_func_idx) {
    if (!g_resolver_initialized || module_id >= MAX_MODULES) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    u8 import_count = g_symbol_resolver.import_counts[module_id];
    
    if (import_count >= MAX_IMPORT_SYMBOLS) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Max imports reached for module %u\n", module_id);
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    GCOSImportSymbol *import = &g_symbol_resolver.import_tables[module_id][import_count];
    import->module_idx_func_idx = module_idx_func_idx;
    import->resolved_address = SYMBOL_IDX_INVALID;
    import->is_resolved = false;
    
    g_symbol_resolver.import_counts[module_id]++;
    
    return GCOS_OK;
}

bool gcos_symbol_resolve_address(GCOSVM *vm, u16 compact_addr, u32 *out_logical_addr) {
    if (!g_resolver_initialized) {
        return false;
    }
    
    if (is_global_ref(compact_addr)) {
        /* Global reference - look up in global reference table (base or extension) */
        u16 index = get_index(compact_addr);
        
        if (index >= g_symbol_resolver.global_ref_count) {
            GCOS_PRINTF("[Symbol Resolver] ERROR: Invalid global ref index %u (count=%u)\n", 
                       index, g_symbol_resolver.global_ref_count);
            return false;
        }
        
        /* Get entry from base table or extension */
        GCOSGlobalRefEntry *entry;
        if (index < MAX_GLOBAL_REFS) {
            entry = &g_symbol_resolver.global_ref_table[index];
        } else {
            if (g_symbol_resolver.global_ref_table_ext == NULL) {
                GCOS_PRINTF("[Symbol Resolver] ERROR: Extended table not allocated for index %u\n", index);
                return false;
            }
            entry = &g_symbol_resolver.global_ref_table_ext[index - MAX_GLOBAL_REFS];
        }
        
        if (!entry->is_valid) {
            GCOS_PRINTF("[Symbol Resolver] ERROR: Invalid global ref entry %u\n", index);
            return false;
        }
        
        *out_logical_addr = entry->logical_address;
        return true;
    }
    else {
        /* Local reference - direct address */
        *out_logical_addr = (u32)compact_addr;
        return true;
    }
}

u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index) {
    if (!g_resolver_initialized) {
        return SYMBOL_IDX_INVALID;
    }
    
    /* Check if table is full - try to expand */
    if (g_symbol_resolver.global_ref_count >= g_symbol_resolver.global_ref_capacity) {
        GCOS_PRINTF("[Symbol Resolver] Global ref table FULL (%u/%u). Attempting expansion...\n",
                   g_symbol_resolver.global_ref_count, g_symbol_resolver.global_ref_capacity);
        
        /* Try to expand the table */
        GCOSResult ret = gcos_symbol_expand_global_ref_table(vm);
        if (ret != GCOS_SUCCESS) {
            GCOS_PRINTF("[Symbol Resolver] ERROR: Failed to expand global ref table\n");
            return SYMBOL_IDX_INVALID;
        }
        
        GCOS_PRINTF("[Symbol Resolver] Table expanded successfully. New capacity: %u\n",
                   g_symbol_resolver.global_ref_capacity);
    }
    
    u16 index = g_symbol_resolver.global_ref_count;
    
    /* Get entry pointer (base table or extension) */
    GCOSGlobalRefEntry *entry;
    if (index < MAX_GLOBAL_REFS) {
        entry = &g_symbol_resolver.global_ref_table[index];
    } else {
        entry = &g_symbol_resolver.global_ref_table_ext[index - MAX_GLOBAL_REFS];
    }
    
    entry->logical_address = logical_address;
    entry->module_id = module_id;
    entry->symbol_index = symbol_index;
    entry->is_valid = true;
    
    g_symbol_resolver.global_ref_count++;
    
    /* NOTE: Global ref table changes should be saved via page_cache_write if needed */
    /* For now, rely on explicit flush calls after module loading */
    
    return make_global_addr(index);
}

int gcos_symbol_find_system_module(GCOSVM *vm, const u8 *aid, u8 aid_length) {
    if (!g_resolver_initialized) {
        return -1;
    }
    
    for (u8 i = 0; i < g_symbol_resolver.system_module_count; i++) {
        GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[i];
        
        if (aid_equals(sys_mod->aid.aid, sys_mod->aid_length, aid, aid_length)) {
            return i;
        }
    }
    
    return -1;
}

int gcos_symbol_find_system_module_by_name(GCOSVM *vm, const char *name) {
    if (!g_resolver_initialized || !name) {
        return -1;
    }
    
    for (u8 i = 0; i < g_symbol_resolver.system_module_count; i++) {
        GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[i];
        
        if (strcmp((const char *)sys_mod->name, name) == 0) {
            return i;
        }
    }
    
    return -1;
}

GCOSResult gcos_symbol_call_system_func(GCOSVM *vm, const char *system_module_name,
                                         const char *func_name, 
                                         const u32 *args, u8 arg_count,
                                         u32 *out_result) {
    if (!g_resolver_initialized) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Find system module */
    int sys_idx = gcos_symbol_find_system_module_by_name(vm, system_module_name);
    if (sys_idx < 0) {
        return GCOS_ERR_MODULE_NOT_FOUND;
    }
    
    GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[sys_idx];
    
    /* Find function */
    for (u8 i = 0; i < sys_mod->export_count; i++) {
        if (strcmp(sys_mod->exports[i].name, func_name) == 0) {
            /* Found function - call it */
            typedef u32 (*SysFuncPtr)(const u32*, u8);
            SysFuncPtr func = (SysFuncPtr)sys_mod->exports[i].func_ptr;
            
            if (out_result) {
                *out_result = func(args, arg_count);
            } else {
                func(args, arg_count);
            }
            
            return GCOS_SUCCESS;
        }
    }
    
    return GCOS_ERR_APP_NOT_FOUND;  /* Function not found in module */
}

void gcos_symbol_print_stats(GCOSVM *vm) {
    if (!g_resolver_initialized) {
        return;
    }
    
    GCOS_PRINTF("\n=== Symbol Resolver Statistics ===\n");
    GCOS_PRINTF("Total resolutions:    %u\n", g_symbol_resolver.total_resolutions);
    GCOS_PRINTF("Failed resolutions:   %u\n", g_symbol_resolver.failed_resolutions);
    GCOS_PRINTF("Global ref entries:   %u / %u (static, %u bytes)\n", 
               g_symbol_resolver.global_ref_count,
               MAX_GLOBAL_REFS,
               (unsigned int)(MAX_GLOBAL_REFS * sizeof(GCOSGlobalRefEntry)));
    GCOS_PRINTF("System modules:       %u / %u\n",
               g_symbol_resolver.system_module_count, MAX_SYSTEM_MODULES);
    GCOS_PRINTF("================================\n\n");
}

void gcos_symbol_dump_tables(GCOSVM *vm, u8 module_id) {
    if (!g_resolver_initialized) {
        return;
    }
    
    GCOS_PRINTF("\n=== Symbol Tables Dump ===\n");
    
    if (module_id == 0xFF) {
        /* Dump all modules */
        for (u8 mod = 0; mod < vm->module_count; mod++) {
            GCOS_PRINTF("\n--- Module %u ---\n", mod);
            GCOS_PRINTF("Exports: %u\n", g_symbol_resolver.export_counts[mod]);
            for (u8 i = 0; i < g_symbol_resolver.export_counts[mod]; i++) {
                GCOSExportSymbol *exp = &g_symbol_resolver.export_tables[mod][i];
                GCOS_PRINTF("  [%u] %s -> 0x%08X\n", i, exp->name, exp->logical_address);
            }
            
            GCOS_PRINTF("Imports: %u\n", g_symbol_resolver.import_counts[mod]);
            for (u8 i = 0; i < g_symbol_resolver.import_counts[mod]; i++) {
                GCOSImportSymbol *imp = &g_symbol_resolver.import_tables[mod][i];
                GCOS_PRINTF("  [%u] mod=%u func=%u -> 0x%04X (%s)\n",
                           i,
                           (imp->module_idx_func_idx >> 11) & 0x1F,
                           imp->module_idx_func_idx & 0x7FF,
                           imp->resolved_address,
                           imp->is_resolved ? "resolved" : "unresolved");
            }
        }
    }
    else if (module_id < vm->module_count) {
        GCOS_PRINTF("\n--- Module %u ---\n", module_id);
        /* Similar dump for single module */
    }
    
    /* Dump global reference table */
    GCOS_PRINTF("\n--- Global Reference Table ---\n");
    GCOS_PRINTF("Entries: %u / %u (static, %u bytes)\n", 
               g_symbol_resolver.global_ref_count,
               MAX_GLOBAL_REFS,
               (unsigned int)(MAX_GLOBAL_REFS * sizeof(GCOSGlobalRefEntry)));
    for (u16 i = 0; i < g_symbol_resolver.global_ref_count; i++) {
        GCOSGlobalRefEntry *entry = &g_symbol_resolver.global_ref_table[i];
        if (entry->is_valid) {
            GCOS_PRINTF("  [%u] 0x%04X -> 0x%08X (mod=%u, sym=%u)\n",
                       i, make_global_addr(i), entry->logical_address,
                       entry->module_id, entry->symbol_index);
        }
    }
    
    /* Dump system modules */
    GCOS_PRINTF("\n--- System Modules ---\n");
    for (u8 i = 0; i < g_symbol_resolver.system_module_count; i++) {
        GCOSSystemModule *sys_mod = &g_symbol_resolver.system_modules[i];
        GCOS_PRINTF("  [%u] %s (AID=", i, sys_mod->name);
        for (u8 j = 0; j < sys_mod->aid_length && j < 16; j++) {
            GCOS_PRINTF("%02X", sys_mod->aid.aid[j]);
        }
        GCOS_PRINTF(") - %u exports\n", sys_mod->export_count);
        
        for (u8 j = 0; j < sys_mod->export_count; j++) {
            GCOS_PRINTF("    [%u] %s -> %p\n", j, sys_mod->exports[j].name,
                       sys_mod->exports[j].func_ptr);
        }
    }
    
    GCOS_PRINTF("==========================\n\n");
}

/* ============================================================================
 * Flash Persistence and Dynamic Expansion Implementation
 * ============================================================================ */

GCOSResult gcos_symbol_expand_global_ref_table(GCOSVM *vm) {
    if (!g_resolver_initialized) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Check if we've reached maximum capacity */
    if (g_symbol_resolver.global_ref_capacity >= MAX_GLOBAL_REFS_MAX) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Global ref table at maximum capacity (%u)\n",
                   MAX_GLOBAL_REFS_MAX);
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Calculate new capacity */
    u16 new_capacity = g_symbol_resolver.global_ref_capacity + GLOBAL_REF_GROWTH_STEP;
    if (new_capacity > MAX_GLOBAL_REFS_MAX) {
        new_capacity = MAX_GLOBAL_REFS_MAX;
    }
    
    /* Calculate extension size */
    u16 ext_size = new_capacity - MAX_GLOBAL_REFS;
    
    GCOS_PRINTF("[Symbol Resolver] Expanding global ref table: %u -> %u entries\n",
               g_symbol_resolver.global_ref_capacity, new_capacity);
    GCOS_PRINTF("[Symbol Resolver] Extension size: %u entries (%u bytes)\n",
               ext_size, (unsigned int)(ext_size * sizeof(GCOSGlobalRefEntry)));
    
    /* Allocate extension table if not already allocated */
    if (g_symbol_resolver.global_ref_table_ext == NULL) {
        /* For smart card environment, we use a static extension buffer
         * In production, this would be allocated from a pre-reserved RAM pool */
        static GCOSGlobalRefEntry static_ext_table[MAX_GLOBAL_REFS_MAX - MAX_GLOBAL_REFS];
        g_symbol_resolver.global_ref_table_ext = static_ext_table;
        
        /* Initialize extension table */
        for (u16 i = 0; i < (MAX_GLOBAL_REFS_MAX - MAX_GLOBAL_REFS); i++) {
            static_ext_table[i].is_valid = false;
        }
    }
    
    /* Update capacity */
    g_symbol_resolver.global_ref_capacity = new_capacity;
    
    GCOS_PRINTF("[Symbol Resolver] Expansion complete. New capacity: %u\n", new_capacity);
    
    /* Save expanded table to Flash */
    return gcos_symbol_save_global_ref_table_to_flash(vm);
}

GCOSResult gcos_symbol_save_global_ref_table_to_flash(GCOSVM *vm) {
    if (!g_resolver_initialized) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Calculate total size to save */
    u32 total_entries = g_symbol_resolver.global_ref_count;
    u32 base_size = (total_entries < MAX_GLOBAL_REFS) ? 
                    total_entries : MAX_GLOBAL_REFS;
    u32 ext_size = (total_entries > MAX_GLOBAL_REFS) ? 
                   (total_entries - MAX_GLOBAL_REFS) : 0;
    
    u32 total_size = sizeof(u32) +           /* Magic number */
                     sizeof(u16) +           /* Count */
                     sizeof(u16) +           /* Capacity */
                     (base_size * sizeof(GCOSGlobalRefEntry)) +
                     (ext_size * sizeof(GCOSGlobalRefEntry)) +
                     sizeof(u32);            /* CRC32 */
    
    GCOS_PRINTF("[Symbol Resolver] Saving global ref table to Flash: %u entries, %u bytes\n",
               total_entries, total_size);
    
    /* Allocate temporary buffer for serialization */
    u8 *buffer = (u8 *)malloc(total_size);
    if (buffer == NULL) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Failed to allocate buffer for Flash save\n");
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Serialize data */
    u32 offset = 0;
    
    /* Magic number */
    u32 magic = 0x47524546; /* "GREF" */
    memcpy(buffer + offset, &magic, sizeof(u32));
    offset += sizeof(u32);
    
    /* Count and capacity */
    memcpy(buffer + offset, &g_symbol_resolver.global_ref_count, sizeof(u16));
    offset += sizeof(u16);
    memcpy(buffer + offset, &g_symbol_resolver.global_ref_capacity, sizeof(u16));
    offset += sizeof(u16);
    
    /* Base table entries */
    memcpy(buffer + offset, g_symbol_resolver.global_ref_table, 
           base_size * sizeof(GCOSGlobalRefEntry));
    offset += base_size * sizeof(GCOSGlobalRefEntry);
    
    /* Extension table entries (if any) */
    if (ext_size > 0 && g_symbol_resolver.global_ref_table_ext != NULL) {
        memcpy(buffer + offset, g_symbol_resolver.global_ref_table_ext,
               ext_size * sizeof(GCOSGlobalRefEntry));
        offset += ext_size * sizeof(GCOSGlobalRefEntry);
    }
    
    /* Calculate CRC32 */
    u32 crc = calculate_crc32_local(buffer, offset);
    memcpy(buffer + offset, &crc, sizeof(u32));
    offset += sizeof(u32);
    
    /* Write to Flash using eflash FTL */
    int ret = eflash_ftl_write_logical(g_symbol_resolver.global_ref_flash_offset, 
                                       buffer, total_size);
    
    free(buffer);
    
    if (ret != 0) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Failed to write to Flash (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    GCOS_PRINTF("[Symbol Resolver] Global ref table saved to Flash at offset 0x%08X\n",
               g_symbol_resolver.global_ref_flash_offset);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_symbol_load_global_ref_table_from_flash(GCOSVM *vm) {
    if (!g_resolver_initialized) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Use a fixed Flash offset for global ref table storage */
    /* This should match the Flash layout defined in gcos_flash_storage.c */
    u32 flash_offset = 0x031000; /* Symbol Table Region */
    g_symbol_resolver.global_ref_flash_offset = flash_offset;
    
    /* Read header (magic + count + capacity) */
    u8 header_buffer[sizeof(u32) + sizeof(u16) + sizeof(u16)];
    int ret = eflash_ftl_read_logical(flash_offset, header_buffer, sizeof(header_buffer));
    
    if (ret != 0) {
        GCOS_PRINTF("[Symbol Resolver] WARNING: Failed to read from Flash (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    /* Check magic number */
    u32 magic;
    memcpy(&magic, header_buffer, sizeof(u32));
    if (magic != 0x47524546) { /* "GREF" */
        GCOS_PRINTF("[Symbol Resolver] WARNING: Invalid magic number (0x%08X)\n", magic);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    /* Read count and capacity */
    u16 count, capacity;
    memcpy(&count, header_buffer + sizeof(u32), sizeof(u16));
    memcpy(&capacity, header_buffer + sizeof(u32) + sizeof(u16), sizeof(u16));
    
    GCOS_PRINTF("[Symbol Resolver] Loading global ref table from Flash: %u entries, capacity %u\n",
               count, capacity);
    
    /* Validate count */
    if (count > MAX_GLOBAL_REFS_MAX) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Invalid count %u (max %u)\n", 
                   count, MAX_GLOBAL_REFS_MAX);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    /* Calculate total data size */
    u32 base_size = (count < MAX_GLOBAL_REFS) ? count : MAX_GLOBAL_REFS;
    u32 ext_size = (count > MAX_GLOBAL_REFS) ? (count - MAX_GLOBAL_REFS) : 0;
    u32 total_data_size = sizeof(header_buffer) +
                         (base_size * sizeof(GCOSGlobalRefEntry)) +
                         (ext_size * sizeof(GCOSGlobalRefEntry)) +
                         sizeof(u32); /* CRC */
    
    /* Allocate buffer for full read */
    u8 *buffer = (u8 *)malloc(total_data_size);
    if (buffer == NULL) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Failed to allocate buffer for Flash load\n");
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Read entire table from Flash */
    ret = eflash_ftl_read_logical(flash_offset, buffer, total_data_size);
    if (ret != 0) {
        free(buffer);
        GCOS_PRINTF("[Symbol Resolver] ERROR: Failed to read table from Flash (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    /* Verify CRC */
    u32 stored_crc;
    memcpy(&stored_crc, buffer + total_data_size - sizeof(u32), sizeof(u32));
    u32 calculated_crc = calculate_crc32_local(buffer, total_data_size - sizeof(u32));
    
    if (stored_crc != calculated_crc) {
        free(buffer);
        GCOS_PRINTF("[Symbol Resolver] ERROR: CRC mismatch (stored=0x%08X, calc=0x%08X)\n",
                   stored_crc, calculated_crc);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    /* Restore count and capacity */
    g_symbol_resolver.global_ref_count = count;
    g_symbol_resolver.global_ref_capacity = capacity;
    
    /* Restore base table */
    u32 offset = sizeof(header_buffer);
    memcpy(g_symbol_resolver.global_ref_table, buffer + offset,
           base_size * sizeof(GCOSGlobalRefEntry));
    offset += base_size * sizeof(GCOSGlobalRefEntry);
    
    /* Restore extension table (if any) */
    if (ext_size > 0) {
        /* Allocate extension if needed */
        if (g_symbol_resolver.global_ref_table_ext == NULL) {
            static GCOSGlobalRefEntry static_ext_table[MAX_GLOBAL_REFS_MAX - MAX_GLOBAL_REFS];
            g_symbol_resolver.global_ref_table_ext = static_ext_table;
            
            /* Initialize */
            for (u16 i = 0; i < (MAX_GLOBAL_REFS_MAX - MAX_GLOBAL_REFS); i++) {
                static_ext_table[i].is_valid = false;
            }
        }
        
        memcpy(g_symbol_resolver.global_ref_table_ext, buffer + offset,
               ext_size * sizeof(GCOSGlobalRefEntry));
    }
    
    free(buffer);
    
    GCOS_PRINTF("[Symbol Resolver] Global ref table loaded successfully from Flash\n");
    GCOS_PRINTF("[Symbol Resolver] Restored: %u entries, capacity %u\n", count, capacity);
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Page Cache Implementation (4-Page Write Buffer)
 * ============================================================================ */

/**
 * @brief Initialize page cache
 * Called during symbol resolver initialization
 */
static void init_page_cache(void) {
    for (int i = 0; i < GLOBAL_REF_WRITE_BUFFER_PAGES; i++) {
        g_symbol_resolver.page_cache[i].lpn = 0xFFFFFFFF;  /* Invalid */
        g_symbol_resolver.page_cache[i].dirty = false;
        g_symbol_resolver.page_cache[i].valid = false;
        memset(g_symbol_resolver.page_cache[i].data, 0, USER_DATA_SIZE);
    }
    g_symbol_resolver.last_flush_time = 0;
}

/**
 * @brief Find a cached page by LPN
 * @param lpn Logical Page Number
 * @return Cache slot index, or -1 if not found
 */
static int find_cached_page(u32 lpn) {
    for (int i = 0; i < GLOBAL_REF_WRITE_BUFFER_PAGES; i++) {
        if (g_symbol_resolver.page_cache[i].valid && 
            g_symbol_resolver.page_cache[i].lpn == lpn) {
            return i;
        }
    }
    return -1;  /* Not found */
}

/**
 * @brief Find an empty cache slot
 * @return Cache slot index, or -1 if all slots are full
 */
static int find_empty_slot(void) {
    for (int i = 0; i < GLOBAL_REF_WRITE_BUFFER_PAGES; i++) {
        if (!g_symbol_resolver.page_cache[i].valid) {
            return i;
        }
    }
    return -1;  /* No empty slot */
}

/**
 * @brief Find the least recently used (LRU) slot for eviction
 * For simplicity, we evict the first clean slot, or the first slot if all are dirty
 * @return Cache slot index to evict
 */
static int find_eviction_slot(void) {
    /* First, try to find a clean slot */
    for (int i = 0; i < GLOBAL_REF_WRITE_BUFFER_PAGES; i++) {
        if (g_symbol_resolver.page_cache[i].valid && 
            !g_symbol_resolver.page_cache[i].dirty) {
            return i;
        }
    }
    
    /* If all are dirty, evict the first one (simple LRU approximation) */
    return 0;
}

GCOSResult gcos_symbol_page_cache_read(GCOSVM *vm, u32 lpn, u8 *out_data) {
    if (!g_resolver_initialized || out_data == NULL) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Check if page is already in cache */
    int slot = find_cached_page(lpn);
    if (slot >= 0) {
        /* Cache hit - copy data from cache */
        memcpy(out_data, g_symbol_resolver.page_cache[slot].data, USER_DATA_SIZE);
        GCOS_PRINTF("[Page Cache] HIT: LPN 0x%08X (slot %d)\n", lpn, slot);
        return GCOS_SUCCESS;
    }
    
    /* Cache miss - read from Flash */
    GCOS_PRINTF("[Page Cache] MISS: LPN 0x%08X, reading from Flash...\n", lpn);
    
    int ret = eflash_ftl_read_logical(lpn, out_data, USER_DATA_SIZE);
    if (ret != 0) {
        GCOS_PRINTF("[Page Cache] ERROR: Failed to read LPN 0x%08X from Flash (ret=%d)\n", 
                   lpn, ret);
        return GCOS_ERR_FILE_FORMAT;
    }
    
    /* Cache the page */
    slot = find_empty_slot();
    if (slot < 0) {
        /* Cache is full, need to evict */
        slot = find_eviction_slot();
        
        /* If evicted slot is dirty, flush it first */
        if (g_symbol_resolver.page_cache[slot].dirty) {
            GCOS_PRINTF("[Page Cache] Evicting dirty page LPN 0x%08X (slot %d), flushing...\n",
                       g_symbol_resolver.page_cache[slot].lpn, slot);
            
            int write_ret = eflash_ftl_write_logical(
                g_symbol_resolver.page_cache[slot].lpn,
                g_symbol_resolver.page_cache[slot].data,
                USER_DATA_SIZE);
            
            if (write_ret != 0) {
                GCOS_PRINTF("[Page Cache] ERROR: Failed to flush evicted page (ret=%d)\n", write_ret);
                return GCOS_ERR_FILE_FORMAT;
            }
        }
        
        GCOS_PRINTF("[Page Cache] Evicted LPN 0x%08X (slot %d)\n",
                   g_symbol_resolver.page_cache[slot].lpn, slot);
    }
    
    /* Load new page into cache slot */
    g_symbol_resolver.page_cache[slot].lpn = lpn;
    memcpy(g_symbol_resolver.page_cache[slot].data, out_data, USER_DATA_SIZE);
    g_symbol_resolver.page_cache[slot].dirty = false;
    g_symbol_resolver.page_cache[slot].valid = true;
    
    GCOS_PRINTF("[Page Cache] Cached LPN 0x%08X (slot %d)\n", lpn, slot);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_symbol_page_cache_write(GCOSVM *vm, u32 lpn, const u8 *data) {
    if (!g_resolver_initialized || data == NULL) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Check if page is already in cache */
    int slot = find_cached_page(lpn);
    
    if (slot < 0) {
        /* Not in cache, need to allocate a slot */
        slot = find_empty_slot();
        
        if (slot < 0) {
            /* Cache is full, need to evict */
            slot = find_eviction_slot();
            
            /* If evicted slot is dirty, flush it first */
            if (g_symbol_resolver.page_cache[slot].dirty) {
                GCOS_PRINTF("[Page Cache] Evicting dirty page LPN 0x%08X (slot %d), flushing...\n",
                           g_symbol_resolver.page_cache[slot].lpn, slot);
                
                int write_ret = eflash_ftl_write_logical(
                    g_symbol_resolver.page_cache[slot].lpn,
                    g_symbol_resolver.page_cache[slot].data,
                    USER_DATA_SIZE);
                
                if (write_ret != 0) {
                    GCOS_PRINTF("[Page Cache] ERROR: Failed to flush evicted page (ret=%d)\n", write_ret);
                    return GCOS_ERR_FILE_FORMAT;
                }
            }
            
            GCOS_PRINTF("[Page Cache] Evicted LPN 0x%08X (slot %d)\n",
                       g_symbol_resolver.page_cache[slot].lpn, slot);
        }
        
        /* Load new page into cache slot */
        g_symbol_resolver.page_cache[slot].lpn = lpn;
        g_symbol_resolver.page_cache[slot].valid = true;
        
        GCOS_PRINTF("[Page Cache] Allocated slot %d for LPN 0x%08X\n", slot, lpn);
    }
    
    /* Write data to cache and mark as dirty */
    memcpy(g_symbol_resolver.page_cache[slot].data, data, USER_DATA_SIZE);
    g_symbol_resolver.page_cache[slot].dirty = true;
    
    GCOS_PRINTF("[Page Cache] WRITE: LPN 0x%08X (slot %d) marked dirty\n", lpn, slot);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_symbol_page_cache_invalidate(GCOSVM *vm, u32 lpn) {
    if (!g_resolver_initialized) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    int slot = find_cached_page(lpn);
    if (slot < 0) {
        /* Page not in cache, nothing to do */
        return GCOS_SUCCESS;
    }
    
    /* Invalidate the slot */
    g_symbol_resolver.page_cache[slot].lpn = 0xFFFFFFFF;
    g_symbol_resolver.page_cache[slot].dirty = false;
    g_symbol_resolver.page_cache[slot].valid = false;
    memset(g_symbol_resolver.page_cache[slot].data, 0, USER_DATA_SIZE);
    
    GCOS_PRINTF("[Page Cache] INVALIDATE: LPN 0x%08X (slot %d)\n", lpn, slot);
    
    return GCOS_SUCCESS;
}

bool gcos_symbol_page_cache_contains(GCOSVM *vm, u32 lpn) {
    (void)vm;
    return find_cached_page(lpn) >= 0;
}

GCOSResult gcos_symbol_flush_write_buffer(GCOSVM *vm) {
    if (!g_resolver_initialized) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    int flushed_count = 0;
    
    /* Flush all dirty pages */
    for (int i = 0; i < GLOBAL_REF_WRITE_BUFFER_PAGES; i++) {
        if (g_symbol_resolver.page_cache[i].valid && 
            g_symbol_resolver.page_cache[i].dirty) {
            
            GCOS_PRINTF("[Page Cache] Flushing dirty page LPN 0x%08X (slot %d)...\n",
                       g_symbol_resolver.page_cache[i].lpn, i);
            
            int ret = eflash_ftl_write_logical(
                g_symbol_resolver.page_cache[i].lpn,
                g_symbol_resolver.page_cache[i].data,
                USER_DATA_SIZE);
            
            if (ret != 0) {
                GCOS_PRINTF("[Page Cache] ERROR: Failed to flush slot %d (ret=%d)\n", i, ret);
                return GCOS_ERR_FILE_FORMAT;
            }
            
            /* Clear dirty flag */
            g_symbol_resolver.page_cache[i].dirty = false;
            flushed_count++;
        }
    }
    
    if (flushed_count > 0) {
        g_symbol_resolver.last_flush_time++;
        GCOS_PRINTF("[Page Cache] Flushed %d dirty pages successfully\n", flushed_count);
    } else {
        GCOS_PRINTF("[Page Cache] No dirty pages to flush\n");
    }
    
    return GCOS_SUCCESS;
}
