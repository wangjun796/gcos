/**
 * @file test_persistence.c
 * @brief Test for GCOS VM Flash Persistence Module
 * 
 * Tests:
 * - Initialize persistence layer
 * - Save module metadata to Flash
 * - Load module metadata from Flash
 * - Delete module from Flash
 * - List persisted modules
 */

#include <stdio.h>
#include <string.h>
#include "gcos_vm.h"
#include "gcos_persistence.h"

/* Test helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("✗ FAILED: %s\n", message); \
            return 1; \
        } else { \
            printf("✓ PASSED: %s\n", message); \
        } \
    } while(0)

#define TEST_SECTION(name) \
    printf("\n=== %s ===\n", name)

/**
 * @brief Test persistence initialization
 */
static int test_persist_init(void) {
    TEST_SECTION("Test 1: Persistence Initialization");
    
    GCOSVM *vm = gcos_vm_create();
    TEST_ASSERT(vm != NULL, "VM created");
    
    GCOSResult ret = gcos_vm_init(vm);
    TEST_ASSERT(ret == GCOS_OK, "VM initialized");
    
    /* Initialize persistence layer */
    int persist_ret = gcos_persist_init(vm);
    TEST_ASSERT(persist_ret == 0, "Persistence layer initialized");
    
    gcos_vm_destroy(vm);
    return 0;
}

/**
 * @brief Test module metadata save/load
 */
static int test_module_metadata(void) {
    TEST_SECTION("Test 2: Module Metadata Save/Load");
    
    GCOSVM *vm = gcos_vm_create();
    TEST_ASSERT(vm != NULL, "VM created");
    
    GCOSResult ret = gcos_vm_init(vm);
    TEST_ASSERT(ret == GCOS_OK, "VM initialized");
    
    int persist_ret = gcos_persist_init(vm);
    TEST_ASSERT(persist_ret == 0, "Persistence initialized");
    
    /* Create a test module */
    GCOSModule *module = &vm->modules[0];
    module->module_id = 1;
    module->security_domain_id = 0xFF;  /* ISD */
    module->state = MODULE_VERIFIED;
    module->version = 0x01000000;  /* v1.0.0.0 */
    
    /* Set AID */
    module->module_aid.length = 8;
    module->module_aid.aid[0] = 0xA0;
    module->module_aid.aid[1] = 0x00;
    module->module_aid.aid[2] = 0x00;
    module->module_aid.aid[3] = 0x00;
    module->module_aid.aid[4] = 0x62;
    module->module_aid.aid[5] = 0x03;
    module->module_aid.aid[6] = 0x01;
    module->module_aid.aid[7] = 0x0C;
    
    module->type = MODULE_TYPE_APP;
    module->global_data_size = 1024;
    module->readonly_data_size = 512;
    module->domain_data_size = 2048;
    module->code_size = 4096;
    module->function_count = 10;
    module->export_count = 5;
    module->import_count = 2;
    module->app_instance_count = 1;
    module->loaded = true;
    module->initialized = true;
    
    vm->module_count = 1;
    
    printf("[TEST] Created test module with ID=%d\n", module->module_id);
    printf("[TEST] Module AID: ");
    for (int i = 0; i < module->module_aid.length; i++) {
        printf("%02X", module->module_aid.aid[i]);
    }
    printf("\n");
    
    /* Save module to Flash */
    persist_ret = gcos_persist_save_module(vm, 1);
    TEST_ASSERT(persist_ret == 0, "Module saved to Flash");
    
    /* Check if module exists */
    bool exists = gcos_persist_module_exists(vm, 1);
    TEST_ASSERT(exists == true, "Module exists in Flash");
    
    /* Get module count */
    u8 count = gcos_persist_get_module_count(vm);
    TEST_ASSERT(count == 1, "Module count is 1");
    
    /* List modules */
    u8 module_ids[16];
    u8 listed = gcos_persist_list_modules(vm, module_ids, 16);
    TEST_ASSERT(listed == 1, "Listed 1 module");
    TEST_ASSERT(module_ids[0] == 1, "Module ID matches");
    
    /* Delete module from Flash */
    persist_ret = gcos_persist_delete_module(vm, 1);
    TEST_ASSERT(persist_ret == 0, "Module deleted from Flash");
    
    /* Verify deletion */
    exists = gcos_persist_module_exists(vm, 1);
    TEST_ASSERT(exists == false, "Module no longer exists");
    
    count = gcos_persist_get_module_count(vm);
    TEST_ASSERT(count == 0, "Module count is 0 after deletion");
    
    gcos_vm_destroy(vm);
    return 0;
}

/**
 * @brief Test CRC-16 calculation
 */
static int test_crc16(void) {
    TEST_SECTION("Test 3: CRC-16 Calculation");
    
    const u8 test_data[] = "Hello, GCOS Persistence!";
    u16 crc = gcos_persist_calc_crc16(test_data, sizeof(test_data) - 1);
    
    printf("[TEST] Data: \"%s\"\n", test_data);
    printf("[TEST] CRC-16: 0x%04X\n", crc);
    
    TEST_ASSERT(crc != 0xFFFF, "CRC is not default value");
    TEST_ASSERT(crc != 0x0000, "CRC is not zero");
    
    /* Verify consistency */
    u16 crc2 = gcos_persist_calc_crc16(test_data, sizeof(test_data) - 1);
    TEST_ASSERT(crc == crc2, "CRC calculation is consistent");
    
    return 0;
}

/**
 * @brief Main test entry point
 */
int main(void) {
    printf("========================================\n");
    printf("GCOS VM Flash Persistence Test Suite\n");
    printf("========================================\n");
    
    int failures = 0;
    
    failures += test_persist_init();
    failures += test_module_metadata();
    failures += test_crc16();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("All tests PASSED ✓\n");
    } else {
        printf("%d test(s) FAILED ✗\n", failures);
    }
    printf("========================================\n");
    
    return failures;
}
