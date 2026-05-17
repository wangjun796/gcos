/**
 * @file test_helpers.h
 * @brief GCOS VM Test Helper Functions
 * 
 * Provides common initialization functions for all test cases.
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "gcos_vm.h"
#include "eflash_sim.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize eflash simulator for testing
 * @param flash_file Path to flash simulation file
 * @return 0 on success, -1 on failure
 */
static inline int test_eflash_init(const char *flash_file) {
    if (flash_file == NULL) {
        flash_file = "test_flash.bin";
    }
    
    /* Remove old flash file if exists */
    remove(flash_file);
    
    /* Step 1: Initialize eflash simulator (file mapping) */
    if (eflash_init(flash_file) != 0) {
        printf("[TEST] ERROR: Failed to initialize eflash with file: %s\n", flash_file);
        return -1;
    }
    
    /* Step 2: Initialize FTL (Flash Translation Layer) */
    if (eflash_ftl_init() != 0) {
        printf("[TEST] ERROR: Failed to initialize FTL\n");
        return -1;
    }
    
    printf("[TEST] Eflash initialized successfully (%s)\n", flash_file);
    return 0;
}

/**
 * @brief Create and initialize VM with eflash
 * @param vm Pointer to VM pointer (will be allocated)
 * @return GCOS_SUCCESS on success, error code on failure
 */
static inline GCOSResult test_vm_create_and_init(GCOSVM **vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Step 1: Initialize eflash */
    if (test_eflash_init(NULL) != 0) {
        return GCOS_ERR_OUT_OF_MEMORY;  /* Use existing error code */
    }
    
    /* Step 2: Create VM */
    *vm = gcos_vm_create();
    if (*vm == NULL) {
        printf("[TEST] ERROR: Failed to create VM\n");
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    /* Step 3: Initialize VM (includes system objects) */
    GCOSResult result = gcos_vm_init(*vm);
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ERROR: VM initialization failed (error=%d)\n", result);
        return result;
    }
    
    printf("[TEST] VM created and initialized successfully\n");
    return GCOS_SUCCESS;
}

/**
 * @brief Cleanup VM and eflash
 * @param vm VM instance to destroy
 */
static inline void test_vm_cleanup(GCOSVM *vm) {
    if (vm != NULL) {
        gcos_vm_destroy(vm);
        printf("[TEST] VM destroyed\n");
    }
}

#ifdef __cplusplus
}
#endif

#endif /* TEST_HELPERS_H */
