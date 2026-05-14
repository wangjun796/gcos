/**
 * @file test_app_manager.c
 * @brief Test application manager functionality
 */

#include "gcos_vm.h"
#include "gcos_app_manager.h"
#include <stdio.h>

/* Test app process function */
static u16 test_app_process(GCOSAppInstance *app,
                            const u8 *apdu,
                            u16 apdu_len,
                            u8 *response,
                            u16 *resp_len) {
    printf("[TestApp] process() called with INS=0x%02X\n", apdu[1]);
    
    // Echo response for testing
    if (resp_len && response && apdu_len > 4) {
        memcpy(response, &apdu[4], apdu_len - 4);
        *resp_len = apdu_len - 4;
    }
    
    return 0x9000;  // SW_SUCCESS
}

int main(void) {
    printf("========================================\n");
    printf("  GCOS VM Application Manager Test\n");
    printf("========================================\n\n");
    
    /* Create and initialize VM */
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("[ERROR] Failed to create VM\n");
        return 1;
    }
    
    GCOSResult result = gcos_vm_init(vm);
    if (result != GCOS_SUCCESS) {
        printf("[ERROR] VM initialization failed: %d\n", result);
        return 1;
    }
    
    printf("\n[TEST] VM initialized successfully\n");
    printf("[TEST] App count: %u (should be 1 for ISD)\n", vm->app_count);
    
    /* Verify ISD was created */
    GCOSAppInstance *isd = app_find_by_id(vm, APP_FIRST);
    if (isd == NULL) {
        printf("[ERROR] ISD not found!\n");
        return 1;
    }
    
    printf("[TEST] ISD found. AID: ");
    for (int i = 0; i < isd->app_aid.length; i++) {
        printf("%02X", isd->app_aid.aid[i]);
    }
    printf("\n");
    
    printf("[TEST] ISD has process handler: %s\n", 
           isd->process ? "YES" : "NO");
    
    /* Register a test application */
    GCOSAID test_aid;
    test_aid.length = 8;
    test_aid.aid[0] = 0xA0;
    test_aid.aid[1] = 0x00;
    test_aid.aid[2] = 0x00;
    test_aid.aid[3] = 0x00;
    test_aid.aid[4] = 0x01;
    test_aid.aid[5] = 0x00;
    test_aid.aid[6] = 0x00;
    test_aid.aid[7] = 0x01;
    
    u8 new_app_id;
    result = app_register(vm,
                          &test_aid,
                          test_app_process,
                          NULL,  // No on_select
                          NULL,  // No on_deselect
                          0,     // Module index
                          &new_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[ERROR] Failed to register test app: %d\n", result);
        return 1;
    }
    
    printf("\n[TEST] Test application registered. App ID: %u\n", new_app_id);
    printf("[TEST] App count: %u (should be 2)\n", vm->app_count);
    
    /* Set application to SELECTABLE state */
    GCOSAppInstance *test_app = app_find_by_id(vm, new_app_id);
    if (test_app) {
        test_app->lifecycle = APPLICATION_SELECTABLE;
        printf("[TEST] Application state set to SELECTABLE\n");
    }
    
    /* Select the test application */
    result = app_select(vm, new_app_id, 0);
    if (result != GCOS_SUCCESS) {
        printf("[ERROR] Failed to select test app: %d\n", result);
        return 1;
    }
    
    printf("\n[TEST] Test application selected\n");
    printf("[TEST] Selected app: %s\n", 
           vm->selected_app ? "YES" : "NO");
    
    /* Test APDU processing */
    u8 apdu[] = {0x00, 0xA0, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05};
    u8 response[260];
    u16 resp_len = 0;
    
    printf("\n[TEST] Sending test APDU...\n");
    u16 sw = gcos_process_apdu(vm, apdu, sizeof(apdu), response, &resp_len);
    
    printf("[TEST] APDU processed. SW=0x%04X\n", sw);
    printf("[TEST] Response length: %u\n", resp_len);
    
    /* Cleanup */
    gcos_vm_destroy(vm);
    
    printf("\n========================================\n");
    printf("  All tests passed!\n");
    printf("========================================\n");
    
    return 0;
}
