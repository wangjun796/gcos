/**
 * @file test_app_delete_simple.c
 * @brief Simple test for app_delete with GRT cleanup
 */

#include "test_helpers.h"
#include "gcos_module_registry.h"

int main(void) {
    printf("=== Simple App Delete Test ===\n\n");
    
    GCOSVM *vm = NULL;
    GCOSResult ret = test_vm_create_and_init(&vm);
    if (!vm || ret != GCOS_SUCCESS) {
        printf("Failed to create and initialize VM\n");
        return 1;
    }
    printf("VM initialized. Apps: %u\n", vm->app_count);
    
    // Create a dummy app (not through INSTALL, just for testing delete)
    GCOSAppInstance *app = &vm->apps[1];
    app->installed = true;
    app->app_id = 1;
    app->module_id = 0xFF;  // No module
    app->module = NULL;
    vm->app_count++;
    
    printf("Created dummy app ID=1\n");
    printf("Total apps: %u\n\n", vm->app_count);
    
    // Delete the app
    printf("Deleting app...\n");
    GCOSResult result = app_delete(vm, 1);
    
    if (result == GCOS_SUCCESS) {
        printf("✅ App deleted successfully\n");
        printf("Total apps: %u\n", vm->app_count);
    } else {
        printf("❌ Delete failed: %d\n", result);
        return 1;
    }
    
    gcos_vm_destroy(vm);
    printf("\n✅ Test passed!\n");
    return 0;
}
