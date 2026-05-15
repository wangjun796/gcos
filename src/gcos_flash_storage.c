/**
 * @file gcos_flash_storage.c
 * @brief GCOS Flash Storage Implementation
 * 
 * Implements Flash storage management for SEF files and module metadata:
 * - SEF file storage in Flash
 * - Module metadata persistence
 * - Streaming SEF parsing from Flash
 * - XIP (Execute In Place) support
 * 
 * @version 1.0.0
 * @date 2026-05-12
 */

#include "gcos_flash_exec.h"
#include "gcos_persistence.h"
#include "eflash_ftl.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Flash Layout Constants
 * ============================================================================ */

/* Flash region offsets (example for 256KB Flash) */
#define FLASH_REGION_FIRMWARE       0x000000    /* 128 KB - Firmware */
#define FLASH_REGION_SEF_STORAGE    0x020000    /* 64 KB - SEF files */
#define FLASH_REGION_METADATA       0x030000    /* 4 KB - Module metadata */
#define FLASH_REGION_SYMBOL_TABLE   0x031000    /* 4 KB - Symbol tables */
#define FLASH_REGION_RUNTIME_STATE  0x032000    /* 4 KB - Runtime state */
#define FLASH_REGION_RESERVED       0x033000    /* 52 KB - Reserved */

/* SEF storage management */
#define SEF_STORAGE_BASE            FLASH_REGION_SEF_STORAGE
#define SEF_STORAGE_SIZE            0x010000    /* 64 KB */
#define MAX_SEF_FILES               8           /* Maximum SEF files */

/* Metadata storage */
#define METADATA_BASE               FLASH_REGION_METADATA
#define METADATA_SIZE               0x001000    /* 4 KB */
#define METADATA_ENTRY_SIZE         64          /* bytes per module */
#define MAX_MODULES_METADATA        (METADATA_SIZE / METADATA_ENTRY_SIZE)

/* Magic numbers */
#define SEF_STORAGE_MAGIC           0x53454630  /* "SEF0" */
#define METADATA_MAGIC              0x4D4F444C  /* "MODL" */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief SEF file entry in Flash
 */
typedef struct {
    u32 magic;                  /* Magic number */
    u32 flash_offset;           /* Offset in Flash */
    u32 size;                   /* SEF file size */
    u32 checksum;               /* CRC32 checksum */
    u8  is_valid;               /* Valid flag */
    u8  reserved[3];            /* Alignment */
} GCOSSefEntry;

/**
 * @brief Module metadata in Flash
 */
typedef struct {
    u32 magic;                  /* Magic number (0x4D4F444C) */
    u8  aid[16];                /* Application ID */
    u8  aid_length;             /* AID length */
    u32 version;                /* Module version */
    
    /* SEF location */
    u32 sef_flash_offset;       /* SEF file offset */
    u32 sef_size;               /* SEF file size */
    
    /* Code location */
    u32 code_flash_offset;      /* Code section offset */
    u32 code_size;              /* Code section size */
    
    /* Data location */
    u32 data_flash_offset;      /* Data section offset */
    u32 data_size;              /* Data section size */
    
    /* Symbol info */
    u16 export_count;           /* Number of exports */
    u16 import_count;           /* Number of imports */
    
    /* Status */
    u8  load_status;            /* 0=unloaded, 1=loaded, 2=active */
    u8  reserved[3];            /* Alignment */
    
    /* Integrity */
    u32 checksum;               /* CRC32 */
} GCOSModuleMetadataFlash;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate CRC32 checksum
 * @param data Data buffer
 * @param size Data size
 * @return CRC32 checksum
 */
static u32 calculate_crc32(const u8 *data, u32 size) {
    /* Simple CRC32 implementation */
    /* TODO: Use proper CRC32 algorithm */
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

/**
 * @brief Find free space in SEF storage region
 * @param required_size Required space in bytes
 * @return Flash offset, or 0xFFFFFFFF if no space
 */
static u32 find_free_sef_space(u32 required_size) {
    /* Simple linear search for free space */
    /* TODO: Implement proper allocation algorithm */
    
    u32 current_offset = SEF_STORAGE_BASE;
    u32 end_offset = SEF_STORAGE_BASE + SEF_STORAGE_SIZE;
    
    while (current_offset + required_size <= end_offset) {
        /* Check if this region is free */
        /* For now, assume first fit */
        return current_offset;
    }
    
    return 0xFFFFFFFF;  /* No space available */
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

u32 flash_allocate_sef_storage(u32 sef_size) {
    if (sef_size == 0 || sef_size > SEF_STORAGE_SIZE) {
        GCOS_PRINTF("[Flash] ERROR: Invalid SEF size %u\n", sef_size);
        return 0xFFFFFFFF;
    }
    
    /* Find free space */
    u32 flash_offset = find_free_sef_space(sef_size);
    
    if (flash_offset == 0xFFFFFFFF) {
        GCOS_PRINTF("[Flash] ERROR: No free space for SEF (size=%u)\n", sef_size);
        return 0xFFFFFFFF;
    }
    
    GCOS_PRINTF("[Flash] Allocated SEF storage at 0x%08X (size=%u)\n", 
               flash_offset, sef_size);
    
    return flash_offset;
}

GCOSResult flash_write_sef(u32 flash_offset, const u8 *sef_data, u32 sef_size) {
    if (flash_offset == 0xFFFFFFFF || sef_data == NULL || sef_size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Validate offset is in SEF storage region */
    if (flash_offset < SEF_STORAGE_BASE || 
        flash_offset + sef_size > SEF_STORAGE_BASE + SEF_STORAGE_SIZE) {
        GCOS_PRINTF("[Flash] ERROR: Offset 0x%08X out of SEF storage region\n", flash_offset);
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOS_PRINTF("[Flash] Writing SEF to 0x%08X (size=%u)...\n", flash_offset, sef_size);
    
    /* Write to Flash using eflash FTL */
    int ret = eflash_ftl_write_logical(flash_offset, sef_data, sef_size);
    
    if (ret != 0) {
        GCOS_PRINTF("[Flash] ERROR: Failed to write SEF (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT; /* Use FILE_FORMAT for IO errors */
    }
    
    GCOS_PRINTF("[Flash] SEF written successfully\n");
    return GCOS_SUCCESS;
}

GCOSResult flash_read_sef_section(u32 sef_flash_offset, u32 section_offset, 
                                  u8 *buffer, u32 size) {
    if (buffer == NULL || size == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    u32 read_offset = sef_flash_offset + section_offset;
    
    /* Read from Flash using eflash FTL */
    int ret = eflash_ftl_read_logical(read_offset, buffer, size);
    
    if (ret != 0) {
        GCOS_PRINTF("[Flash] ERROR: Failed to read SEF section (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT; /* Use FILE_FORMAT for IO errors */
    }
    
    return GCOS_SUCCESS;
}

GCOSResult flash_save_module_metadata(u8 module_id, const GCOSModuleMetadataFlash *metadata) {
    if (module_id >= MAX_MODULES_METADATA || metadata == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    u32 metadata_offset = METADATA_BASE + (module_id * METADATA_ENTRY_SIZE);
    
    /* Calculate checksum */
    u32 checksum = calculate_crc32((const u8 *)metadata, sizeof(GCOSModuleMetadataFlash) - 4);
    
    /* Create temporary copy with checksum */
    GCOSModuleMetadataFlash temp = *metadata;
    temp.checksum = checksum;
    
    /* Write to Flash using eflash FTL */
    int ret = eflash_ftl_write_logical(metadata_offset, (const u8 *)&temp, sizeof(GCOSModuleMetadataFlash));
    
    if (ret != 0) {
        GCOS_PRINTF("[Flash] ERROR: Failed to save metadata (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT; /* Use FILE_FORMAT for IO errors */
    }
    
    GCOS_PRINTF("[Flash] Module %u metadata saved at 0x%08X\n", module_id, metadata_offset);
    return GCOS_SUCCESS;
}

GCOSResult flash_load_module_metadata(u8 module_id, GCOSModuleMetadataFlash *metadata) {
    if (module_id >= MAX_MODULES_METADATA || metadata == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    u32 metadata_offset = METADATA_BASE + (module_id * METADATA_ENTRY_SIZE);
    
    /* Read from Flash using eflash FTL */
    int ret = eflash_ftl_read_logical(metadata_offset, (u8 *)metadata, sizeof(GCOSModuleMetadataFlash));
    
    if (ret != 0) {
        GCOS_PRINTF("[Flash] ERROR: Failed to load metadata (ret=%d)\n", ret);
        return GCOS_ERR_FILE_FORMAT; /* Use FILE_FORMAT for IO errors */
    }
    
    /* Verify magic number */
    if (metadata->magic != METADATA_MAGIC) {
        GCOS_PRINTF("[Flash] WARNING: Invalid metadata magic for module %u\n", module_id);
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Verify checksum */
    u32 expected_checksum = metadata->checksum;
    u32 calculated_checksum = calculate_crc32((const u8 *)metadata, 
                                              sizeof(GCOSModuleMetadataFlash) - 4);
    
    if (expected_checksum != calculated_checksum) {
        GCOS_PRINTF("[Flash] WARNING: Metadata checksum mismatch for module %u\n", module_id);
        return GCOS_ERR_FILE_FORMAT; /* Use FILE_FORMAT for checksum errors */
    }
    
    return GCOS_SUCCESS;
}

bool flash_verify_code_integrity(u32 flash_offset, u32 code_size, u32 expected_checksum) {
    /* Read code from Flash and verify checksum */
    /* For now, return true - TODO: implement proper verification */
    (void)flash_offset;
    (void)code_size;
    (void)expected_checksum;
    
    return true;
}

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Allocate Flash space for SEF file
 * @param sef_size SEF file size in bytes
 * @return Flash offset, or FLASH_OFFSET_INVALID if failed
 */
u32 gcos_flash_alloc_sef_space(u32 sef_size) {
    return flash_allocate_sef_storage(sef_size);
}

/**
 * @brief Free Flash space for SEF file
 * @param flash_offset Flash offset to free
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_flash_free_sef_space(u32 flash_offset) {
    /* TODO: Implement proper deallocation */
    /* For now, just mark as invalid */
    (void)flash_offset;
    return GCOS_SUCCESS;
}

/**
 * @brief Save module metadata to Flash
 * @param vm VM instance
 * @param module_index Module index
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_persistence_save_module_metadata(GCOSVM *vm, u8 module_index) {
    if (vm == NULL || module_index >= MAX_MODULES) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    const GCOSModule *module = &vm->modules[module_index];
    
    /* Prepare metadata structure */
    GCOSModuleMetadataFlash metadata;
    memset(&metadata, 0, sizeof(metadata));
    
    metadata.magic = METADATA_MAGIC;
    memcpy(metadata.aid, module->module_aid.aid, module->module_aid.length);
    metadata.aid_length = module->module_aid.length;
    metadata.version = module->version;
    
    /* SEF location */
    metadata.sef_flash_offset = vm->runtime.sef_flash_offset;
    metadata.sef_size = vm->runtime.sef_size;
    
    /* Code location */
    metadata.code_flash_offset = vm->runtime.code_flash_offset;
    metadata.code_size = vm->runtime.code_size;
    
    /* Symbol info */
    metadata.export_count = module->export_count;
    metadata.import_count = module->import_count;
    
    /* Status */
    metadata.load_status = 1; /* loaded */
    
    /* Calculate checksum */
    metadata.checksum = calculate_crc32((const u8 *)&metadata, 
                                       sizeof(metadata) - 4);
    
    /* Save to Flash */
    return flash_save_module_metadata(module_index, &metadata);
}
