/**
 * @file gcos_module_function_table.h
 * @brief GCOS Module Function Table Definition
 * 
 * Defines the function table structure for modules, similar to cref's method table.
 * Standard methods (install/select/deselect/process) are at fixed positions (0-3).
 * 
 * Reference: cref package/applet model, COS3 specification section 7
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#ifndef GCOS_MODULE_FUNCTION_TABLE_H
#define GCOS_MODULE_FUNCTION_TABLE_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Standard method indices in function table (fixed positions) */
#define FUNC_IDX_INSTALL        0   /* Module installation method */
#define FUNC_IDX_SELECT         1   /* Application selection method */
#define FUNC_IDX_DESELECT       2   /* Application deselection method */
#define FUNC_IDX_PROCESS        3   /* APDU processing method */
#define FUNC_IDX_UNINSTALL      4   /* Module uninstallation method (optional) */
#define FUNC_IDX_FIRST_CUSTOM   5   /* First custom method index */

/* Maximum number of functions per module */
#define MAX_FUNCTIONS_PER_MODULE    64

/* Function signature types */
typedef enum {
    FUNC_SIGNATURE_VOID_VOID = 0,       /* void func(void) */
    FUNC_SIGNATURE_U16_U8_U16 = 1,      /* u16 func(u8*, u16) - APDU handler */
    FUNC_SIGNATURE_U16_VOID = 2,        /* u16 func(void) - status return */
    FUNC_SIGNATURE_VOID_U32 = 3,        /* void func(u32) - init with param */
} GCOSFunctionSignature;

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Function Entry in Module Function Table
 * 
 * Each entry represents one exported function from a module.
 */
typedef struct {
    u16 logical_address;            /* Logical address of function code in Flash */
    u8 signature_type;              /* Function signature type */
    u8 parameter_count;             /* Number of parameters */
    u16 reserved;                   /* Reserved for alignment */
} GCOSFunctionEntry;

/**
 * @brief Module Function Table
 * 
 * This table contains all exported functions from a module.
 * Stored in Flash at module's function_table_addr.
 * 
 * Layout:
 *   Header (8 bytes):
 *     - module_aid (16 bytes, may be truncated)
 *     - function_count (2 bytes)
 *     - checksum (2 bytes)
 *   Function entries (variable):
 *     - Array of GCOSFunctionEntry
 */
typedef struct {
    /* Function table header */
    u8 module_aid[16];              /* Module AID (for validation) */
    u16 function_count;             /* Number of functions in table */
    u16 checksum;                   /* Simple checksum for validation */
    
    /* Function entries array */
    GCOSFunctionEntry functions[MAX_FUNCTIONS_PER_MODULE];
} GCOSModuleFunctionTable;

/**
 * @brief Module Member Offset Table
 * 
 * Tracks offsets of global/static variables within module's data area.
 * Used for member access: base_address + offset.
 * 
 * Similar to cref's field table but for module-level data.
 */
typedef struct {
    u16 member_count;               /* Number of members */
    u16 total_data_size;            /* Total size of module data area */
    
    /* Member entries: each is just an offset from data base */
    /* Members are accessed as: module_data_base + offset */
    u16 member_offsets[1];          /* Variable length array */
} GCOSModuleMemberTable;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get standard method address from function table
 * 
 * Retrieves the logical address of a standard method (install/select/deselect/process).
 * 
 * @param func_table Pointer to function table
 * @param method_idx Method index (FUNC_IDX_INSTALL, etc.)
 * @return Logical address of method, or 0 if not found
 */
u16 gcos_func_table_get_standard_method(const GCOSModuleFunctionTable *func_table, u8 method_idx);

/**
 * @brief Call standard method (install/select/deselect/process)
 * 
 * Invokes a standard method through the VM executor.
 * 
 * @param vm VM instance
 * @param module_id Module ID
 * @param method_idx Method index (FUNC_IDX_INSTALL, etc.)
 * @param apdu APDU data (for process method)
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word (0x9000 = success)
 */
u16 gcos_func_table_call_standard_method(GCOSVM *vm, u8 module_id, u8 method_idx,
                                         const u8 *apdu, u16 apdu_len,
                                         u8 *response, u16 *resp_len);

/**
 * @brief Initialize function table for loaded module
 * 
 * Creates and populates the function table when a module is loaded.
 * Parses SEF file to extract function information.
 * 
 * @param vm VM instance
 * @param module_id Module ID
 * @param sef_data SEF file data
 * @param sef_len SEF file length
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_func_table_init_from_sef(GCOSVM *vm, u8 module_id, 
                                         const u8 *sef_data, u32 sef_len);

/**
 * @brief Validate function table integrity
 * 
 * Checks function table checksum and structure validity.
 * 
 * @param func_table Pointer to function table
 * @return true if valid, false otherwise
 */
bool gcos_func_table_validate(const GCOSModuleFunctionTable *func_table);

/**
 * @brief Get custom method address by index
 * 
 * Retrieves address of custom (non-standard) method.
 * 
 * @param func_table Pointer to function table
 * @param custom_idx Custom method index (>= FUNC_IDX_FIRST_CUSTOM)
 * @return Logical address of method, or 0 if not found
 */
u16 gcos_func_table_get_custom_method(const GCOSModuleFunctionTable *func_table, u8 custom_idx);

/**
 * @brief Access module member by offset
 * 
 * Calculates absolute address of module member: data_base + offset.
 * 
 * @param module Pointer to module registry entry
 * @param member_offset Offset from module data base
 * @return Absolute logical address of member
 */
u32 gcos_module_access_member(const GCOSModuleRegistry *module, u16 member_offset);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_MODULE_FUNCTION_TABLE_H */
