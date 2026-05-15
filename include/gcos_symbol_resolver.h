/**
 * @file gcos_symbol_resolver.h
 * @brief GCOS Symbol Resolution System
 * 
 * Implements symbol resolution for cross-module function/variable references:
 * - Import/export symbol tables
 * - System module pre-registration (like iwasm)
 * - 16-bit compact addressing with global reference table (like cref)
 * - Symbol linking during SEF loading
 * 
 * Address Format:
 * - Local symbols: 16-bit direct address (0x0000-0x7FFF)
 * - Global symbols: 16-bit index into global reference table (0x8000-0xFFFF)
 *   - Bit 15 = 1 indicates global reference
 *   - Bits 14-0 = index into global_ref_table
 * 
 * @version 1.0.0
 * @date 2026-05-12
 */

#ifndef GCOS_SYMBOL_RESOLVER_H
#define GCOS_SYMBOL_RESOLVER_H

#include "gcos_vm.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Maximum number of exported symbols per module */
#define MAX_EXPORT_SYMBOLS      64

/* Maximum number of imported symbols per module */
#define MAX_IMPORT_SYMBOLS      64

/* Maximum number of system modules */
#define MAX_SYSTEM_MODULES      8

/* Maximum number of global references (initial capacity for smart card environment) */
#define MAX_GLOBAL_REFS         64      /* Initial static capacity */
#define MAX_GLOBAL_REFS_MAX     256     /* Absolute maximum after expansion */
#define GLOBAL_REF_GROWTH_STEP  32      /* Expansion step size */

/* Note: GCOS runs on resource-constrained smart cards (8-64KB RAM).
 * Global reference table is stored in Flash and loaded to RAM when needed.
 * Dynamic expansion is supported within limits (MAX_GLOBAL_REFS_MAX).
 * Use eflash library for persistence. */

/* Address format flags */
#define ADDR_FLAG_GLOBAL        0x8000  /* Bit 15: global reference flag */
#define ADDR_MASK_INDEX         0x7FFF  /* Bits 14-0: index mask */

/* Invalid symbol index */
#define SYMBOL_IDX_INVALID      0xFFFF

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Exported symbol entry
 */
typedef struct {
    u16 function_index;         /* Function index within module */
    u32 logical_address;        /* Logical address of the symbol */
    char name[32];              /* Symbol name (optional, for debugging) */
} GCOSExportSymbol;

/**
 * @brief Imported symbol entry
 */
typedef struct {
    u16 module_idx_func_idx;    /* COS3 format: high 5 bits = module idx, low 11 bits = func idx */
    u16 resolved_address;       /* Resolved 16-bit address (may be global ref) */
    bool is_resolved;           /* Whether symbol has been resolved */
} GCOSImportSymbol;

/**
 * @brief Global reference table entry (compact format for Flash storage)
 * Maps 16-bit global reference index to 32-bit logical address
 * 
 * Memory layout (12 bytes per entry):
 * - logical_address: 4 bytes (u32)
 * - module_id: 1 byte (u8)
 * - symbol_index: 2 bytes (u16)
 * - is_valid: 1 byte (bool, padded)
 * 
 * Total table size: MAX_GLOBAL_REFS × 12 bytes = 768 bytes (for 64 entries)
 * This MUST fit in RAM and be persistable to Flash.
 */
typedef struct {
    u32 logical_address;        /* 32-bit logical address */
    u8 module_id;               /* Module that owns this symbol */
    u16 symbol_index;           /* Symbol index within module */
    bool is_valid;              /* Whether entry is valid */
} GCOSGlobalRefEntry;

/**
 * @brief System module registration
 * Pre-registered system APIs (like iwasm's built-in modules)
 */
typedef struct {
    GCOSAID aid;                /* System module AID */
    u8 aid_length;              /* AID length */
    const char *name;           /* Module name (e.g., "sys", "math", "io") */
    
    /* Exported functions */
    struct {
        const char *name;       /* Function name */
        void *func_ptr;         /* Function pointer (native code) */
        u8 param_count;         /* Number of parameters */
        u8 return_size;         /* Return value size in bytes */
    } exports[MAX_EXPORT_SYMBOLS];
    
    u8 export_count;            /* Number of exported functions */
    bool is_registered;         /* Whether module is registered */
} GCOSSystemModule;

/**
 * @brief Symbol resolver context
 */
typedef struct {
    /* Export tables per module */
    GCOSExportSymbol export_tables[MAX_MODULES][MAX_EXPORT_SYMBOLS];
    u8 export_counts[MAX_MODULES];
    
    /* Import tables per module */
    GCOSImportSymbol import_tables[MAX_MODULES][MAX_IMPORT_SYMBOLS];
    u8 import_counts[MAX_MODULES];
    
    /* Global reference table (Flash-backed with dynamic expansion)
     * - Initial capacity: MAX_GLOBAL_REFS (64 entries, 768 bytes)
     * - Maximum capacity: MAX_GLOBAL_REFS_MAX (256 entries, 3 KB)
     * - Stored in Flash via eflash library
     * - Loaded to RAM on demand for fast access */
    GCOSGlobalRefEntry global_ref_table[MAX_GLOBAL_REFS];  /* Static base table */
    GCOSGlobalRefEntry *global_ref_table_ext;              /* Extension pointer (NULL if not expanded) */
    u16 global_ref_capacity;                               /* Current capacity */
    u16 global_ref_count;                                  /* Current usage count */
    u32 global_ref_flash_offset;                           /* Flash storage offset */
    
    /* System modules */
    GCOSSystemModule system_modules[MAX_SYSTEM_MODULES];
    u8 system_module_count;
    
    /* Resolution statistics */
    u32 total_resolutions;
    u32 failed_resolutions;
} GCOSSymbolResolver;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize symbol resolver
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_resolver_init(GCOSVM *vm);

/**
 * @brief Register a system module (like iwasm's built-in modules)
 * @param vm VM instance
 * @param aid System module AID
 * @param aid_length AID length
 * @param name Module name
 * @return GCOSResult Success or error code
 * 
 * Example:
 *   gcos_symbol_register_system_module(vm, sys_aid, 5, "sys");
 *   gcos_symbol_add_system_export(vm, "sys_print", sys_print_func, 1, 0);
 */
GCOSResult gcos_symbol_register_system_module(GCOSVM *vm, const u8 *aid, 
                                               u8 aid_length, const char *name);

/**
 * @brief Add export to system module
 * @param vm VM instance
 * @param system_module_name System module name
 * @param func_name Function name
 * @param func_ptr Function pointer
 * @param param_count Number of parameters
 * @param return_size Return value size
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_add_system_export(GCOSVM *vm, const char *system_module_name,
                                          const char *func_name, void *func_ptr,
                                          u8 param_count, u8 return_size);

/**
 * @brief Resolve import symbols for a module
 * Called after all modules are loaded
 * @param vm VM instance
 * @param module_id Module ID
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_resolve_imports(GCOSVM *vm, u8 module_id);

/**
 * @brief Add export symbol for a module
 * @param vm VM instance
 * @param module_id Module ID
 * @param function_index Function index
 * @param logical_address Logical address of function
 * @param name Symbol name (optional)
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_add_export(GCOSVM *vm, u8 module_id, u16 function_index,
                                   u32 logical_address, const char *name);

/**
 * @brief Add import symbol for a module
 * @param vm VM instance
 * @param module_id Module ID
 * @param module_idx_func_idx COS3 format import descriptor
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_add_import(GCOSVM *vm, u8 module_id, u16 module_idx_func_idx);

/**
 * @brief Convert 16-bit compact address to 32-bit logical address
 * Handles both local (direct) and global (indirect) addresses
 * @param vm VM instance
 * @param compact_addr 16-bit compact address
 * @param out_logical_addr Output: 32-bit logical address
 * @return true if successful, false if invalid address
 */
bool gcos_symbol_resolve_address(GCOSVM *vm, u16 compact_addr, u32 *out_logical_addr);

/**
 * @brief Create global reference entry
 * Allocates entry in static global reference table
 * 
 * IMPORTANT: This function will fail if table is full (MAX_GLOBAL_REFS reached).
 * For smart card environment, table size is fixed and cannot be expanded.
 * Plan your module design to stay within the limit.
 * 
 * @param vm VM instance
 * @param logical_address 32-bit logical address
 * @param module_id Owning module ID
 * @param symbol_index Symbol index
 * @return 16-bit global reference address (with bit 15 set), or SYMBOL_IDX_INVALID if table full
 */
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index);

/**
 * @brief Find system module by AID
 * @param vm VM instance
 * @param aid Module AID
 * @param aid_length AID length
 * @return System module index, or -1 if not found
 */
int gcos_symbol_find_system_module(GCOSVM *vm, const u8 *aid, u8 aid_length);

/**
 * @brief Find system module by name
 * @param vm VM instance
 * @param name Module name
 * @return System module index, or -1 if not found
 */
int gcos_symbol_find_system_module_by_name(GCOSVM *vm, const char *name);

/**
 * @brief Call system module function
 * Similar to iwasm's native call mechanism
 * @param vm VM instance
 * @param system_module_name System module name
 * @param func_name Function name
 * @param args Arguments array
 * @param arg_count Number of arguments
 * @param out_result Output: result value
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_call_system_func(GCOSVM *vm, const char *system_module_name,
                                         const char *func_name, 
                                         const u32 *args, u8 arg_count,
                                         u32 *out_result);

/**
 * @brief Print symbol resolution statistics
 * @param vm VM instance
 */
void gcos_symbol_print_stats(GCOSVM *vm);

/**
 * @brief Expand global reference table (dynamic expansion)
 * 
 * When the static table is full, this function allocates an extension table.
 * The extension is stored in Flash and loaded to RAM when needed.
 * 
 * Maximum capacity: MAX_GLOBAL_REFS_MAX (256 entries)
 * Growth step: GLOBAL_REF_GROWTH_STEP (32 entries per expansion)
 * 
 * @param vm VM instance
 * @return GCOSResult Success or error code
 * 
 * Note: This uses a small temporary buffer for expansion, then persists to Flash.
 * The expanded table is split into base (static) + extension (dynamic).
 */
GCOSResult gcos_symbol_expand_global_ref_table(GCOSVM *vm);

/**
 * @brief Save global reference table to Flash
 * 
 * Persists the entire global reference table (base + extension) to Flash.
 * Called after module loading or when table is modified.
 * 
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_save_global_ref_table_to_flash(GCOSVM *vm);

/**
 * @brief Load global reference table from Flash
 * 
 * Restores the global reference table from Flash during system startup.
 * Automatically handles both static and expanded tables.
 * 
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_load_global_ref_table_from_flash(GCOSVM *vm);

/**
 * @brief Dump symbol tables for debugging
 * @param vm VM instance
 * @param module_id Module ID (or 0xFF for all modules)
 */
void gcos_symbol_dump_tables(GCOSVM *vm, u8 module_id);

#endif /* GCOS_SYMBOL_RESOLVER_H */
