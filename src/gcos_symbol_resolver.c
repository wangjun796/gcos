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
#include <string.h>
#include <stdio.h>

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
    
    /* Initialize global reference table */
    for (int i = 0; i < MAX_GLOBAL_REFS; i++) {
        g_symbol_resolver.global_ref_table[i].is_valid = false;
    }
    g_symbol_resolver.global_ref_count = 0;
    
    /* Initialize system modules */
    g_symbol_resolver.system_module_count = 0;
    
    /* Initialize statistics */
    g_symbol_resolver.total_resolutions = 0;
    g_symbol_resolver.failed_resolutions = 0;
    
    g_resolver_initialized = true;
    
    GCOS_PRINTF("[Symbol Resolver] Initialized\n");
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
        /* Global reference - look up in global reference table */
        u16 index = get_index(compact_addr);
        
        if (index >= g_symbol_resolver.global_ref_count) {
            GCOS_PRINTF("[Symbol Resolver] ERROR: Invalid global ref index %u\n", index);
            return false;
        }
        
        GCOSGlobalRefEntry *entry = &g_symbol_resolver.global_ref_table[index];
        
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
    
    if (g_symbol_resolver.global_ref_count >= MAX_GLOBAL_REFS) {
        GCOS_PRINTF("[Symbol Resolver] ERROR: Global ref table full\n");
        return SYMBOL_IDX_INVALID;
    }
    
    u16 index = g_symbol_resolver.global_ref_count;
    GCOSGlobalRefEntry *entry = &g_symbol_resolver.global_ref_table[index];
    
    entry->logical_address = logical_address;
    entry->module_id = module_id;
    entry->symbol_index = symbol_index;
    entry->is_valid = true;
    
    g_symbol_resolver.global_ref_count++;
    
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
    GCOS_PRINTF("Global ref entries:   %u / %u\n", 
               g_symbol_resolver.global_ref_count, MAX_GLOBAL_REFS);
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
    GCOS_PRINTF("Entries: %u / %u\n", g_symbol_resolver.global_ref_count, MAX_GLOBAL_REFS);
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
