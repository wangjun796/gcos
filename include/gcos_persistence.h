/**
 * @file gcos_persistence.h
 * @brief GCOS VM Module Persistence Layer
 * 
 * Handles module persistence to Flash using eflash library.
 * 
 * Storage Layout:
 * - Object Headers: eflash object headers for modules
 * - Module Data: SEF binary data stored in logical sectors
 * - Module Metadata: Module info (AID, version, state) stored in sectors
 * 
 * Key Features:
 * - Load module from Flash at startup
 * - Save module to Flash after LOAD FINALIZE
 * - Delete module from Flash on DELETE command
 * - Support multiple modules with unique IDs
 * 
 * Reference: COS3 Specification Section 8.2 (Loading/Installation/Deletion Management)
 */

#ifndef GCOS_PERSISTENCE_H
#define GCOS_PERSISTENCE_H

#include "gcos_vm.h"
#include "gcos_platform.h"  /* For GCOS_PACKED macros */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Flash Storage Constants
 * ============================================================================ */

/** Maximum number of modules that can be stored in Flash */
#define GCOS_MAX_PERSISTED_MODULES  16

/** Object header class ID for GCOS modules */
#define GCOS_MODULE_CLASS_ID        0xC001  /* Custom class for GCOS modules */

/** Object header package ID range for modules */
#define GCOS_MODULE_PKG_ID_START    0x1000
#define GCOS_MODULE_PKG_ID_END      0x1FFF

/** Module metadata magic number */
#define GCOS_MODULE_META_MAGIC      0x474D4554  /* "GMET" */

/** User data size per sector (from eflash.h) */
#define GCOS_SECTOR_USER_DATA_SIZE  464

/* ============================================================================
 * Module Metadata Structure (stored in Flash)
 * ============================================================================ */

/**
 * @brief Module metadata stored in Flash
 * 
 * This structure is stored at the beginning of module data in Flash.
 * It contains essential information needed to restore the module to memory.
 * Size: 128 bytes (fits in first sector with room for SEF data)
 */
typedef struct {
    /* === Header === */
    u32 magic;                      /* Magic number (GCOS_MODULE_META_MAGIC) */
    u16 version;                    /* Metadata structure version (for compatibility) */
    
    /* === Module Identity === */
    u8 module_id;                   /* Internal module ID */
    u8 security_domain_id;          /* Security domain ID */
    GCOSModuleState state;          /* Module state */
    u32 version_code;               /* Module version (u32 format) */
    GCOSAID module_aid;             /* Module AID */
    GCOSModuleType type;            /* Module type (app/library) */
    
    /* === Memory Area Sizes === */
    u32 global_data_size;
    u32 readonly_data_size;
    u32 domain_data_size;
    u32 code_size;
    
    /* === Function Table === */
    u16 function_count;
    u32 function_table_size;        /* Size of function table in bytes */
    
    /* === Export Table === */
    u16 export_count;
    u32 export_table_size;          /* Size of export table in bytes */
    
    /* === Import Dependencies === */
    u8 import_count;
    u32 import_table_size;          /* Size of import table in bytes */
    
    /* === Application Instances === */
    u8 app_instance_count;
    u32 app_table_size;             /* Size of app instance table in bytes */
    
    /* === SEF File === */
    u32 sef_file_size;              /* Original SEF file size */
    u32 sef_sectors;                /* Number of sectors used for SEF data */
    
    /* === Integrity Check === */
    u16 checksum;                   /* CRC-16 of all preceding fields */
    u8 reserved[6];                 /* Alignment padding */
} GCOS_PACKED GCOSModuleMeta;

/* ============================================================================
 * Flash Storage Layout
 * ============================================================================ */

/**
 * Module Storage Layout in Flash:
 * 
 * Object Header (eflash object header table):
 *   - pkg_id: module_id + GCOS_MODULE_PKG_ID_START
 *   - class_id: GCOS_MODULE_CLASS_ID
 *   - body_addr: starting logical sector
 *   - body_size: total size (metadata + SEF data)
 * 
 * Sector Layout:
 *   Sector 0: [GCOSModuleMeta (128 bytes) | SEF data part 1 (336 bytes)]
 *   Sector 1: [SEF data part 2 (464 bytes)]
 *   Sector 2: [SEF data part 3 (464 bytes)]
 *   ...
 *   Sector N: [SEF data part N+1 (464 bytes)]
 */

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize persistence layer
 * 
 * Must be called after eflash initialization.
 * Scans Flash for existing modules and rebuilds in-memory mapping.
 * 
 * @param vm VM instance
 * @return 0 on success, -1 on failure
 */
int gcos_persist_init(GCOSVM *vm);

/**
 * @brief Save module to Flash
 * 
 * Called after LOAD FINALIZE to persist module to non-volatile storage.
 * Uses eflash transaction mechanism for atomicity.
 * 
 * Process:
 * 1. Allocate eflash object header
 * 2. Serialize module to SEF format
 * 3. Write metadata + SEF data to sectors
 * 4. Commit transaction
 * 
 * @param vm VM instance
 * @param module_id Module ID to save
 * @return 0 on success, -1 on failure
 */
int gcos_persist_save_module(GCOSVM *vm, u8 module_id);

/**
 * @brief Load module from Flash
 * 
 * Called during VM startup to restore module from Flash to memory.
 * 
 * Process:
 * 1. Read metadata from first sector
 * 2. Validate metadata (magic + CRC)
 * 3. Allocate memory for module structures
 * 4. Read SEF data from sectors
 * 5. Parse SEF and populate module
 * 
 * @param vm VM instance
 * @param module_id Module ID to load
 * @return 0 on success, -1 if module not found or corrupt
 */
int gcos_persist_load_module(GCOSVM *vm, u8 module_id);

/**
 * @brief Delete module from Flash
 * 
 * Called during DELETE command to remove module from Flash.
 * 
 * Process:
 * 1. Invalidate object header (mark as deleted)
 * 2. Free allocated sectors
 * 3. Remove from mapping table
 * 4. Commit transaction
 * 
 * @param vm VM instance
 * @param module_id Module ID to delete
 * @return 0 on success, -1 on failure
 */
int gcos_persist_delete_module(GCOSVM *vm, u8 module_id);

/**
 * @brief Check if module exists in Flash
 * 
 * @param vm VM instance
 * @param module_id Module ID to check
 * @return true if module exists, false otherwise
 */
bool gcos_persist_module_exists(GCOSVM *vm, u8 module_id);

/**
 * @brief Get number of persisted modules
 * 
 * @param vm VM instance
 * @return Number of modules stored in Flash
 */
u8 gcos_persist_get_module_count(GCOSVM *vm);

/**
 * @brief List all module IDs in Flash
 * 
 * @param vm VM instance
 * @param module_ids Output buffer for module IDs
 * @param max_count Maximum number of IDs to retrieve
 * @return Number of module IDs retrieved
 */
u8 gcos_persist_list_modules(GCOSVM *vm, u8 *module_ids, u8 max_count);

/**
 * @brief Calculate CRC-16 checksum
 * 
 * @param data Data buffer
 * @param length Data length
 * @return CRC-16 checksum
 */
u16 gcos_persist_calc_crc16(const u8 *data, u32 length);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_PERSISTENCE_H */
