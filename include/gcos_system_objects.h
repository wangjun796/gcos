/**
 * @file gcos_system_objects.h
 * @brief GCOS System Objects Management
 * 
 * Manages system management data using eflash object mechanism.
 * Uses fixed object IDs (1-6) to anchor all system tables.
 * 
 * Object ID Allocation:
 *   Obj 1: Module Registry Table
 *   Obj 2: Application Instance Table
 *   Obj 3: Global Reference Table (GRT)
 *   Obj 4: GCOS Free List
 *   Obj 5: System Configuration (root anchor)
 *   Obj 6: Symbol Resolution Table
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#ifndef GCOS_SYSTEM_OBJECTS_H
#define GCOS_SYSTEM_OBJECTS_H

#include "gcos_vm.h"
#include "eflash_ftl.h"
#include "gcos_symbol_resolver.h"  /* For GCOSGlobalRefEntry */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - System Object IDs
 * ============================================================================ */

/* Reserved system object IDs */
#define GCOS_OBJ_ID_MODULE_REGISTRY     1   /* Module Registry Table */
#define GCOS_OBJ_ID_APP_INSTANCE        2   /* Application Instance Table */
#define GCOS_OBJ_ID_GRT                 3   /* Global Reference Table (includes symbol resolution) */
/* Obj ID 4 is NOT used - eflash already manages free list at LPN 8-11 */
/* Obj ID 6 is NOT used - symbol resolution is handled by GRT (Obj 3) */
#define GCOS_OBJ_ID_SYS_CONFIG          5   /* System Configuration (root) */

/* System object pkg_id and class_id */
#define GCOS_PKG_ID                     0x4743  /* "GC" */
#define GCOS_CLASS_MODULE_REGISTRY      0x0001
#define GCOS_CLASS_APP_INSTANCE         0x0002
#define GCOS_CLASS_GRT                  0x0003
/* Class ID 0x0004 is reserved but not used (eflash manages free list) */
#define GCOS_CLASS_SYS_CONFIG           0x0005
/* Class ID 0x0006 is reserved but not used (symbol resolution in GRT) */

/* Magic numbers for validation */
#define GCOS_MAGIC_MODR                 0x4D4F4452U  /* "MODR" */
#define GCOS_MAGIC_APPT                 0x41505054U  /* "APPT" */
#define GCOS_MAGIC_GRT0                 0x47525430U  /* "GRT0" */
/* Magic 0x4652454C ("FREL") is reserved but not used */
#define GCOS_MAGIC_SYSC                 0x53595343U  /* "SYSC" */

/* Default configuration values */
#define GCOS_DEFAULT_MAX_MODULES        16
#define GCOS_DEFAULT_MAX_APPS           32
#define GCOS_DEFAULT_MAX_GRT            64

/* System flags */
#define GCOS_SYS_FLAG_INITIALIZED       0x0001U
#define GCOS_SYS_FLAG_PERSISTENT        0x0002U
#define GCOS_SYS_FLAG_RECOVERY_MODE     0x0004U

/* ============================================================================
 * Type Definitions - System Object Structures
 * ============================================================================ */

/**
 * @brief System Configuration Object (Obj ID 5)
 * 
 * This is the root anchor object that contains references to all other
 * system objects. It's the first object loaded during initialization.
 */
typedef struct {
    /* Header */
    u32 magic;                      /* 0x53595343 = "SYSC" */
    u16 version;                    /* Version 1.0 = 0x0100 */
    u16 flags;                      /* System flags */
    
    /* System parameters */
    u32 max_modules;                /* Maximum modules */
    u32 max_apps;                   /* Maximum applications */
    u32 max_grt_entries;            /* Maximum GRT entries */
    u32 flash_total_size;           /* Total Flash size in bytes */
    
    /* ⭐ Object ID references (KEY DESIGN) */
    u16 module_registry_obj_id;     /* Should be GCOS_OBJ_ID_MODULE_REGISTRY */
    u16 app_instance_obj_id;        /* Should be GCOS_OBJ_ID_APP_INSTANCE */
    u16 grt_obj_id;                 /* Should be GCOS_OBJ_ID_GRT (includes symbol resolution) */
    /* Note: Obj ID 4 and 6 are reserved but not used */
    
    /* Runtime state */
    u32 module_count;               /* Current loaded modules */
    u32 app_count;                  /* Current app instances */
    u32 grt_used_count;             /* GRT used entries */
    
    /* Integrity check */
    u32 checksum;                   /* CRC32 (covers all fields except this) */
} GCOS_SystemConfigObject;

/**
 * @brief Module Registry Object (Obj ID 1)
 */
typedef struct {
    /* Header */
    u32 magic;                      /* 0x4D4F4452 = "MODR" */
    u16 version;                    /* Version 1.0 */
    u16 module_count;               /* Current module count */
    u16 max_modules;                /* Maximum modules */
    u16 checksum;                   /* CRC16 */
    u16 reserved;
    
    /* Module entries (variable length array) */
    GCOSModuleRegistry modules[1];  /* Flexible array member */
} GCOS_ModuleRegistryObject;

/**
 * @brief Application Instance Object (Obj ID 2)
 */
typedef struct {
    /* Header */
    u32 magic;                      /* 0x41505054 = "APPT" */
    u16 version;                    /* Version 1.0 */
    u16 app_count;                  /* Current app count */
    u16 max_apps;                   /* Maximum apps */
    u16 checksum;                   /* CRC16 */
    u16 reserved;
    
    /* Application entries (variable length array) */
    GCOSAppInstance apps[1];        /* Flexible array member */
} GCOS_AppInstanceObject;

/**
 * @brief GRT Object (Obj ID 3)
 */
typedef struct {
    /* Header */
    u32 magic;                      /* 0x47525430 = "GRT0" */
    u16 version;                    /* Version 1.0 */
    u16 capacity;                   /* GRT capacity */
    u16 used_count;                 /* Currently used entries */
    u16 checksum;                   /* CRC16 */
    u16 reserved;
    
    /* GRT entries (compact 32-bit format) */
    GCOSGlobalRefEntry entries[1];  /* Flexible array member */
} GCOS_GRTObject;

/* Note: Obj ID 4 is NOT used by GCOS.
 * eflash already manages the free list at LPN 8-11 via eflash_mgr.
 * GCOS uses eflash_mgr_alloc() and eflash_mgr_free() directly for space management.
 */

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize GCOS system objects
 * 
 * Checks if system objects exist. If not (first boot), creates them.
 * If they exist, loads them from Flash.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_system_objects_init(GCOSVM *vm);

/**
 * @brief Create system objects on first boot
 * 
 * Creates all 6 system objects with initial values.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_system_objects_create(GCOSVM *vm);

/**
 * @brief Load system objects from Flash
 * 
 * Reads all system objects and validates integrity.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_system_objects_load(GCOSVM *vm);

/**
 * @brief Save system objects to Flash
 * 
 * Writes all system objects with updated checksums.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_system_objects_save(GCOSVM *vm);

/**
 * @brief Get system configuration object
 * 
 * @param vm VM instance
 * @return Pointer to system config object, or NULL on error
 */
GCOS_SystemConfigObject* gcos_system_config_get(GCOSVM *vm);

/**
 * @brief Update module registry object
 * 
 * Saves updated module registry to Flash.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_module_registry_object_update(GCOSVM *vm);

/**
 * @brief Update application instance object
 * 
 * Saves updated app instance table to Flash.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_app_instance_object_update(GCOSVM *vm);

/**
 * @brief Update GRT object
 * 
 * Saves updated GRT to Flash.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_grt_object_update(GCOSVM *vm);

/**
 * @brief Validate system configuration integrity
 * 
 * Checks magic number, version, and checksum.
 * 
 * @param config Pointer to config object
 * @return true if valid, false otherwise
 */
bool gcos_validate_system_config(const GCOS_SystemConfigObject *config);

/**
 * @brief Calculate CRC32 checksum
 * 
 * @param data Pointer to data buffer
 * @param length Data length in bytes
 * @return CRC32 value
 */
u32 gcos_calc_crc32(const void *data, u32 length);

/**
 * @brief Allocate new object for application/module
 * 
 * Wrapper around eflash object allocation with GCOS-specific logic.
 * 
 * @param pkg_id Package ID
 * @param class_id Class ID
 * @param size Object size in bytes
 * @param out_obj_id Output allocated object ID
 * @param out_logic_addr Output logical address of object body
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_object_allocate(u16 pkg_id, u16 class_id, u32 size,
                                u16 *out_obj_id, uint32_t *out_logic_addr);

/**
 * @brief Free object and reclaim space
 * 
 * @param obj_id Object ID to free
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_object_free(u16 obj_id);

/**
 * @brief Read object data from Flash
 * 
 * @param obj_id Object ID
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param out_actual_size Actual data size read
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_object_read(u16 obj_id, void *buffer, u32 buffer_size, u32 *out_actual_size);

/**
 * @brief Write object data to Flash
 * 
 * @param obj_id Object ID
 * @param data Data to write
 * @param size Data size
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_object_write(u16 obj_id, const void *data, u32 size);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_SYSTEM_OBJECTS_H */
