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

/* Write buffer configuration for Flash optimization */
#define GLOBAL_REF_WRITE_BUFFER_PAGES   4       /* Number of cacheable pages */
#define USER_DATA_SIZE                  464     /* eflash user data size per page */
#define GLOBAL_REF_WRITE_BUFFER_SIZE    (GLOBAL_REF_WRITE_BUFFER_PAGES * USER_DATA_SIZE)  /* 1,856 bytes */

/* Page cache entry structure */
typedef struct {
    u32 lpn;                    /* Logical Page Number (0xFFFFFFFF = invalid/empty) */
    u8 data[USER_DATA_SIZE];    /* Cached page data (464 bytes) */
    bool dirty;                 /* Has unsaved changes */
    bool valid;                 /* Slot is in use */
} GCOSPageCacheEntry;

/* Note: GCOS runs on resource-constrained smart cards (8-64KB RAM).
 * Global reference table is stored in Flash and loaded to RAM when needed.
 * Dynamic expansion is supported within limits (MAX_GLOBAL_REFS_MAX).
 * Write buffer reduces Flash write frequency by batching updates.
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
 * @brief Global reference table entry (optimized compact format, inspired by cref GRT)
 * 
 * Memory layout (4 bytes per entry, packed into u32):
 * - Bits 31-24: Module ID (8 bits, 0xFF = invalid/recycled)
 * - Bits 23-0:  Logical Address (24 bits, supports up to 16 MB addressing)
 * 
 * This design is inspired by cref's Global Reference Table (GRT):
 * - Minimal size: only 4 bytes per entry
 * - Fast access: single u32 read gets both module_id and address
 * - Easy recycling: set module_id to 0xFF to mark as invalid
 * - Slot reuse: find_free_slot() searches for module_id == 0xFF
 * 
 * Total table size: MAX_GLOBAL_REFS × 4 bytes = 256 bytes (for 64 entries)
 * Maximum table size: MAX_GLOBAL_REFS_MAX × 4 bytes = 1,024 bytes (for 256 entries)
 * This MUST fit in RAM and be persistable to Flash.
 */
typedef struct {
    u32 packed_data;  /* High 8 bits = module_id, Low 24 bits = logical_address */
} GCOSGlobalRefEntry;

/* Accessor macros for packed GRT entry */
#define GRT_MODULE_ID_MASK      0xFF000000U  /* Bits 31-24 */
#define GRT_ADDRESS_MASK        0x00FFFFFFU  /* Bits 23-0 */
#define GRT_MODULE_ID_INVALID   0xFF         /* Invalid module ID marker */

/* Get module_id from packed entry */
#define GRT_GET_MODULE_ID(entry)    ((u8)(((entry).packed_data & GRT_MODULE_ID_MASK) >> 24))

/* Get logical_address from packed entry */
#define GRT_GET_ADDRESS(entry)      ((u32)((entry).packed_data & GRT_ADDRESS_MASK))

/* Set packed entry from module_id and logical_address */
#define GRT_SET_ENTRY(entry, addr, mod) \
    ((entry).packed_data = (((u32)(mod) << 24) | ((addr) & GRT_ADDRESS_MASK)))

/* Check if entry is valid */
#define GRT_IS_VALID(entry)     (GRT_GET_MODULE_ID(entry) != GRT_MODULE_ID_INVALID)

/* Invalidate entry (soft delete) */
#define GRT_INVALIDATE(entry)   ((entry).packed_data = ((u32)GRT_MODULE_ID_INVALID << 24))

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
    
    /* Write buffer for Flash optimization (reduces write frequency) */
    GCOSPageCacheEntry page_cache[GLOBAL_REF_WRITE_BUFFER_PAGES];  /* 4-page cache */
    u32 last_flush_time;                                   /* Last flush timestamp (for periodic flush) */
    
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
 * @brief Create global reference entry (inspired by cref GRT)
 * 
 * Creates a new global reference or reuses an invalid slot.
 * The entry stores module_id (8 bits) and logical_address (24 bits) in packed format.
 * 
 * @param vm VM instance
 * @param logical_address 32-bit logical address (will be masked to 24 bits, supports 16 MB)
 * @param module_id Module ID that owns this reference (0xFF = invalid)
 * @param symbol_index Symbol index within module (currently unused, kept for API compatibility)
 * @return 16-bit compact address (with bit 15 set), or SYMBOL_IDX_INVALID on error
 */
u16 gcos_symbol_create_global_ref(GCOSVM *vm, u32 logical_address, 
                                   u8 module_id, u16 symbol_index);

/**
 * @brief Find a free slot in global reference table (module_id == 0xFF)
 * 
 * Searches for an invalid entry that can be reused.
 * This implements cref's slot reuse mechanism.
 * 
 * @param vm VM instance
 * @return Slot index, or SYMBOL_IDX_INVALID if no free slot
 */
u16 gcos_symbol_find_free_slot(GCOSVM *vm);

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
 * @brief Flush write buffer to Flash (batch save)
 * 
 * Writes all dirty cached pages to Flash in a single operation.
 * This reduces Flash write frequency and extends Flash lifespan.
 * 
 * Call this function:
 * - After loading a module (batch save all changes)
 * - Periodically (every N operations)
 * - Before system shutdown
 * 
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_flush_write_buffer(GCOSVM *vm);

/**
 * @brief Read a page from cache or Flash
 * 
 * If the page is in cache, return cached data.
 * Otherwise, read from Flash and cache it.
 * 
 * @param vm VM instance
 * @param lpn Logical Page Number to read
 * @param out_data Output buffer for page data (must be USER_DATA_SIZE bytes)
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_page_cache_read(GCOSVM *vm, u32 lpn, u8 *out_data);

/**
 * @brief Write a page to cache (deferred Flash write)
 * 
 * Writes data to cache and marks it as dirty.
 * The actual Flash write is deferred until flush_write_buffer() is called.
 * 
 * @param vm VM instance
 * @param lpn Logical Page Number to write
 * @param data Data to write (USER_DATA_SIZE bytes)
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_page_cache_write(GCOSVM *vm, u32 lpn, const u8 *data);

/**
 * @brief Invalidate a cached page
 * 
 * Removes a page from cache without writing to Flash.
 * Use this when the page is no longer needed or has been invalidated.
 * 
 * @param vm VM instance
 * @param lpn Logical Page Number to invalidate
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_symbol_page_cache_invalidate(GCOSVM *vm, u32 lpn);

/**
 * @brief Check if a page is in cache
 * 
 * @param vm VM instance
 * @param lpn Logical Page Number to check
 * @return true if page is in cache
 */
bool gcos_symbol_page_cache_contains(GCOSVM *vm, u32 lpn);

/**
 * @brief Delete all global references owned by a module (batch soft delete)
 * 
 * This function implements cref's removeReferencesFromPackage() mechanism:
 * - Traverses all GRT entries
 * - Marks entries with matching module_id as invalid (module_id = 0xFF)
 * - Slots can be reused by future create_global_ref() calls
 * 
 * Call this function before deleting a module to recycle its global references.
 * 
 * @param vm VM instance
 * @param module_id Module ID to delete
 */
void gcos_symbol_delete_module_global_refs(GCOSVM *vm, u8 module_id);

/**
 * @brief Dump symbol tables for debugging
 * @param vm VM instance
 * @param module_id Module ID (or 0xFF for all modules)
 */
void gcos_symbol_dump_tables(GCOSVM *vm, u8 module_id);

#endif /* GCOS_SYMBOL_RESOLVER_H */
