/**
 * @file gcos_flash_storage.h
 * @brief GCOS Flash Storage Management API
 * 
 * Provides Flash storage management functions for SEF files and module metadata.
 * Supports XIP (Execute In Place) execution from Flash.
 * 
 * @version 1.0.0
 * @date 2026-05-12
 */

#ifndef GCOS_FLASH_STORAGE_H
#define GCOS_FLASH_STORAGE_H

#include "gcos_vm.h"
#include "gcos_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Flash Storage Constants
 * ============================================================================ */

/** Invalid Flash offset marker */
#define FLASH_OFFSET_INVALID    0xFFFFFFFF

/* ============================================================================
 * Public API - Flash Space Management
 * ============================================================================ */

/**
 * @brief Allocate Flash space for SEF file
 * @param sef_size SEF file size in bytes
 * @return Flash offset, or FLASH_OFFSET_INVALID if failed
 */
u32 gcos_flash_alloc_sef_space(u32 sef_size);

/**
 * @brief Free Flash space for SEF file
 * @param flash_offset Flash offset to free
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_flash_free_sef_space(u32 flash_offset);

/* ============================================================================
 * Public API - Module Metadata Persistence
 * ============================================================================ */

/**
 * @brief Save module metadata to Flash (smart card optimized)
 * 
 * This function saves module metadata including Flash offsets for XIP execution.
 * Used by the new Flash-based loader.
 * 
 * @param vm VM instance
 * @param module_index Module index
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_persistence_save_module_metadata(GCOSVM *vm, u8 module_index);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_FLASH_STORAGE_H */
