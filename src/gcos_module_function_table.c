/**
 * @file gcos_module_function_table.c
 * @brief GCOS Module Function Table Implementation
 * 
 * Implements module function table management for standard and custom methods.
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#include "gcos_vm.h"
#include "gcos_module_function_table.h"
#include "gcos_module_registry.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate simple checksum for function table
 */
static u16 calc_func_table_checksum(const GCOSModuleFunctionTable *table) {
    u16 checksum = 0;
    
    /* Sum all bytes in module_aid */
    for (int i = 0; i < 16; i++) {
        checksum += table->module_aid[i];
    }
    
    /* Add function count */
    checksum += table->function_count;
    
    /* Sum function entries */
    for (u16 i = 0; i < table->function_count && i < MAX_FUNCTIONS_PER_MODULE; i++) {
        checksum += table->functions[i].logical_address;
        checksum += table->functions[i].signature_type;
        checksum += table->functions[i].parameter_count;
    }
    
    return checksum;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

u16 gcos_func_table_get_standard_method(const GCOSModuleFunctionTable *func_table, u8 method_idx) {
    if (func_table == NULL) {
        return 0;
    }
    
    if (method_idx >= func_table->function_count) {
        printf("[FUNC_TABLE] WARNING: Method index %u out of range (count=%u)\n",
               method_idx, func_table->function_count);
        return 0;
    }
    
    u16 addr = func_table->functions[method_idx].logical_address;
    printf("[FUNC_TABLE] Standard method %u at logical address 0x%04X\n", method_idx, addr);
    
    return addr;
}

u16 gcos_func_table_call_standard_method(GCOSVM *vm, u8 module_id, u8 method_idx,
                                         const u8 *apdu, u16 apdu_len,
                                         u8 *response, u16 *resp_len) {
    if (vm == NULL || response == NULL || resp_len == NULL) {
        return 0x6F00;  /* Unknown error */
    }
    
    /* Validate module_id */
    if (module_id >= MAX_MODULES || !vm->module_registry[module_id].is_loaded) {
        printf("[FUNC_TABLE] ERROR: Module %u not loaded\n", module_id);
        return 0x6A88;  /* Referenced data not found */
    }
    
    GCOSModuleRegistry *module = &vm->module_registry[module_id];
    
    /* Check if function table exists */
    if (module->function_table_addr == 0) {
        printf("[FUNC_TABLE] ERROR: Module %u has no function table\n", module_id);
        return 0x6A88;
    }
    
    /* TODO: Get function table from Flash and call method through VM executor */
    /* For now, return simulated success */
    printf("[FUNC_TABLE] Calling standard method %u on module %u (simulated)\n",
           method_idx, module_id);
    
    *resp_len = 0;
    return 0x9000;  /* Success */
}

GCOSResult gcos_func_table_init_from_sef(GCOSVM *vm, u8 module_id, 
                                         const u8 *sef_data, u32 sef_len) {
    if (vm == NULL || sef_data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (module_id >= MAX_MODULES) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    printf("[FUNC_TABLE] Initializing function table for module %u from SEF...\n", module_id);
    
    GCOSModuleRegistry *module = &vm->module_registry[module_id];
    
    /* TODO: Parse SEF file to extract function information */
    /* For now, create a minimal function table with dummy entries */
    
    /* Allocate function table in Flash (simulated) */
    GCOSModuleFunctionTable *func_table = (GCOSModuleFunctionTable *)malloc(sizeof(GCOSModuleFunctionTable));
    if (func_table == NULL) {
        return GCOS_ERROR_OUT_OF_MEMORY;
    }
    
    memset(func_table, 0, sizeof(GCOSModuleFunctionTable));
    
    /* Copy module AID */
    if (module->module_aid.length <= 16) {
        memcpy(func_table->module_aid, module->module_aid.aid, module->module_aid.length);
    } else {
        memcpy(func_table->module_aid, module->module_aid.aid, 16);
    }
    
    /* Set function count (5 standard methods) */
    func_table->function_count = 5;
    
    /* Initialize standard method entries with dummy addresses */
    /* In real implementation, these would be parsed from SEF */
    func_table->functions[FUNC_IDX_INSTALL].logical_address = 0x1000;
    func_table->functions[FUNC_IDX_INSTALL].signature_type = FUNC_SIGNATURE_U16_U8_U16;
    func_table->functions[FUNC_IDX_INSTALL].parameter_count = 2;
    
    func_table->functions[FUNC_IDX_SELECT].logical_address = 0x1100;
    func_table->functions[FUNC_IDX_SELECT].signature_type = FUNC_SIGNATURE_U16_U8_U16;
    func_table->functions[FUNC_IDX_SELECT].parameter_count = 2;
    
    func_table->functions[FUNC_IDX_DESELECT].logical_address = 0x1200;
    func_table->functions[FUNC_IDX_DESELECT].signature_type = FUNC_SIGNATURE_VOID_VOID;
    func_table->functions[FUNC_IDX_DESELECT].parameter_count = 0;
    
    func_table->functions[FUNC_IDX_PROCESS].logical_address = 0x1300;
    func_table->functions[FUNC_IDX_PROCESS].signature_type = FUNC_SIGNATURE_U16_U8_U16;
    func_table->functions[FUNC_IDX_PROCESS].parameter_count = 2;
    
    func_table->functions[FUNC_IDX_UNINSTALL].logical_address = 0x1400;
    func_table->functions[FUNC_IDX_UNINSTALL].signature_type = FUNC_SIGNATURE_U16_VOID;
    func_table->functions[FUNC_IDX_UNINSTALL].parameter_count = 0;
    
    /* Calculate and set checksum */
    func_table->checksum = calc_func_table_checksum(func_table);
    
    /* Store function table address in module registry */
    /* TODO: Write to Flash and get actual logical address */
    module->function_table_addr = 0x2000;  /* Simulated address */
    
    printf("[FUNC_TABLE] Function table initialized:\n");
    printf("[FUNC_TABLE]   Install: 0x%04X\n", func_table->functions[FUNC_IDX_INSTALL].logical_address);
    printf("[FUNC_TABLE]   Select:  0x%04X\n", func_table->functions[FUNC_IDX_SELECT].logical_address);
    printf("[FUNC_TABLE]   Deselect: 0x%04X\n", func_table->functions[FUNC_IDX_DESELECT].logical_address);
    printf("[FUNC_TABLE]   Process: 0x%04X\n", func_table->functions[FUNC_IDX_PROCESS].logical_address);
    printf("[FUNC_TABLE]   Uninstall: 0x%04X\n", func_table->functions[FUNC_IDX_UNINSTALL].logical_address);
    printf("[FUNC_TABLE]   Checksum: 0x%04X\n", func_table->checksum);
    
    /* Free temporary allocation (in real impl, this would be in Flash) */
    free(func_table);
    
    return GCOS_SUCCESS;
}

bool gcos_func_table_validate(const GCOSModuleFunctionTable *func_table) {
    if (func_table == NULL) {
        return false;
    }
    
    /* Check function count */
    if (func_table->function_count == 0 || func_table->function_count > MAX_FUNCTIONS_PER_MODULE) {
        printf("[FUNC_TABLE] Validation failed: Invalid function count %u\n",
               func_table->function_count);
        return false;
    }
    
    /* Verify checksum */
    u16 calc_checksum = calc_func_table_checksum(func_table);
    if (calc_checksum != func_table->checksum) {
        printf("[FUNC_TABLE] Validation failed: Checksum mismatch\n");
        printf("[FUNC_TABLE]   Expected: 0x%04X, Got: 0x%04X\n",
               func_table->checksum, calc_checksum);
        return false;
    }
    
    /* Check that standard methods have valid addresses */
    for (u8 i = 0; i < 5 && i < func_table->function_count; i++) {
        if (func_table->functions[i].logical_address == 0) {
            printf("[FUNC_TABLE] WARNING: Standard method %u has null address\n", i);
        }
    }
    
    return true;
}

u16 gcos_func_table_get_custom_method(const GCOSModuleFunctionTable *func_table, u8 custom_idx) {
    if (func_table == NULL) {
        return 0;
    }
    
    if (custom_idx < FUNC_IDX_FIRST_CUSTOM) {
        printf("[FUNC_TABLE] ERROR: Custom index %u is not >= %u\n",
               custom_idx, FUNC_IDX_FIRST_CUSTOM);
        return 0;
    }
    
    if (custom_idx >= func_table->function_count) {
        printf("[FUNC_TABLE] WARNING: Custom index %u out of range (count=%u)\n",
               custom_idx, func_table->function_count);
        return 0;
    }
    
    return func_table->functions[custom_idx].logical_address;
}

u32 gcos_module_access_member(const GCOSModuleRegistry *module, u16 member_offset) {
    if (module == NULL) {
        return 0;
    }
    
    if (!module->is_loaded) {
        printf("[FUNC_TABLE] ERROR: Module not loaded\n");
        return 0;
    }
    
    /* Check offset bounds */
    if (member_offset >= module->global_data_size) {
        printf("[FUNC_TABLE] ERROR: Member offset %u exceeds data size %u\n",
               member_offset, module->global_data_size);
        return 0;
    }
    
    /* Calculate absolute address: global_data_base + offset */
    u32 absolute_addr = module->global_data_addr + member_offset;
    
    printf("[FUNC_TABLE] Accessing member at offset %u -> logical address 0x%08X\n",
           member_offset, absolute_addr);
    
    return absolute_addr;
}
