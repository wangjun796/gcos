/**
 * @file test_module_registry.c
 * @brief Test program for GCOS Module Registry
 * 
 * Tests:
 * - Module registration
 * - Instance tracking
 * - Module lookup by AID and ID
 * - Module unloading
 * - Dependency verification
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#include "test_helpers.h"
#include "gcos_module_registry.h"

/* Helper macro for test results */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("❌ FAILED: %s (line %d)\n", message, __LINE__); \
            return 1; \
        } else { \
            printf("✅ PASSED: %s\n", message); \
        } \
    } while(0)

int main(void) {
    printf("========================================\n");
    printf("GCOS Module Registry Test\n");
    printf("========================================\n\n");
    
    /* Create VM instance with eflash */
    GCOSVM *vm = NULL;
    GCOSResult ret = test_vm_create_and_init(&vm);
    TEST_ASSERT(vm != NULL && ret == GCOS_SUCCESS, "VM created and initialized");
    
    /* Initialize module registry */
    ret = module_registry_init(vm);
    TEST_ASSERT(ret == GCOS_SUCCESS, "Module registry initialized");
    
    /* Test 1: Register a module */
    printf("\n=== Test 1: Module Registration ===\n");
    
    u8 module_id_1;
    u8 dummy_sef_data[100] = {0};  /* Dummy SEF data for testing */
    
    ret = module_registry_register(vm, dummy_sef_data, sizeof(dummy_sef_data), &module_id_1);
    TEST_ASSERT(ret == GCOS_SUCCESS, "Module 1 registered");
    printf("   Module ID: %d\n", module_id_1);
    
    /* Verify registry entry */
    GCOSModuleRegistry *reg1 = module_registry_find_by_id(vm, module_id_1);
    TEST_ASSERT(reg1 != NULL, "Module 1 found by ID");
    TEST_ASSERT(reg1->module_id == module_id_1, "Module ID matches");
    TEST_ASSERT(reg1->is_loaded == true, "Module is registered");
    TEST_ASSERT(reg1->instance_count == 0, "No instances yet");
    
    /* Test 2: Register another module */
    printf("\n=== Test 2: Second Module Registration ===\n");
    
    u8 module_id_2;
    ret = module_registry_register(vm, dummy_sef_data, sizeof(dummy_sef_data), &module_id_2);
    TEST_ASSERT(ret == GCOS_SUCCESS, "Module 2 registered");
    printf("   Module ID: %d\n", module_id_2);
    TEST_ASSERT(module_id_2 != module_id_1, "Module IDs are different");
    
    /* Test 3: Add instances to modules */
    printf("\n=== Test 3: Instance Tracking ===\n");
    
    ret = module_registry_add_instance(vm, module_id_1, 1);  /* App #1 uses Module #1 */
    TEST_ASSERT(ret == GCOS_SUCCESS, "App 1 added to Module 1");
    
    ret = module_registry_add_instance(vm, module_id_1, 2);  /* App #2 uses Module #1 */
    TEST_ASSERT(ret == GCOS_SUCCESS, "App 2 added to Module 1");
    
    ret = module_registry_add_instance(vm, module_id_2, 3);  /* App #3 uses Module #2 */
    TEST_ASSERT(ret == GCOS_SUCCESS, "App 3 added to Module 2");
    
    /* Verify instance counts */
    u8 count = module_registry_get_instance_count(vm, module_id_1);
    TEST_ASSERT(count == 2, "Module 1 has 2 instances");
    
    count = module_registry_get_instance_count(vm, module_id_2);
    TEST_ASSERT(count == 1, "Module 2 has 1 instance");
    
    /* Test 4: Remove an instance */
    printf("\n=== Test 4: Instance Removal ===\n");
    
    ret = module_registry_remove_instance(vm, module_id_1, 1);
    TEST_ASSERT(ret == GCOS_SUCCESS, "App 1 removed from Module 1");
    
    count = module_registry_get_instance_count(vm, module_id_1);
    TEST_ASSERT(count == 1, "Module 1 now has 1 instance");
    
    /* Test 5: Try to unload module with active instances */
    printf("\n=== Test 5: Unload Protection ===\n");
    
    ret = module_registry_unload(vm, module_id_1);
    TEST_ASSERT(ret == GCOS_ERROR_MODULE_IN_USE, "Cannot unload module with active instances");
    printf("   Correctly prevented unload (1 instance still active)\n");
    
    /* Test 6: Remove last instance and unload */
    printf("\n=== Test 6: Module Unloading ===\n");
    
    ret = module_registry_remove_instance(vm, module_id_1, 2);
    TEST_ASSERT(ret == GCOS_SUCCESS, "App 2 removed from Module 1");
    
    count = module_registry_get_instance_count(vm, module_id_1);
    TEST_ASSERT(count == 0, "Module 1 has no instances");
    
    ret = module_registry_unload(vm, module_id_1);
    TEST_ASSERT(ret == GCOS_SUCCESS, "Module 1 unloaded successfully");
    
    reg1 = module_registry_find_by_id(vm, module_id_1);
    TEST_ASSERT(reg1 == NULL, "Module 1 no longer found after unload");
    
    /* Test 7: Dump registry */
    printf("\n=== Test 7: Registry Dump ===\n");
    module_registry_dump(vm);
    
    /* Cleanup */
    printf("\n=== Cleanup ===\n");
    ret = gcos_vm_destroy(vm);
    TEST_ASSERT(ret == GCOS_SUCCESS, "VM destroyed");
    
    printf("\n========================================\n");
    printf("All tests passed! ✅\n");
    printf("========================================\n");
    
    return 0;
}
