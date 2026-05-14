/**
 * @file test_select_command.c
 * @brief Test SELECT command handling
 */

#include <stdio.h>
#include <string.h>
#include "gcos_vm.h"
#include "gcos_app_manager.h"
#include "gcos_apdu.h"

/* Test app process function */
static u16 test_app_process(GCOSAppInstance *app,
                            const u8 *apdu,
                            u16 apdu_len,
                            u8 *response,
                            u16 *resp_len) {
    printf("[TestApp] process() called with INS=0x%02X\n", apdu[1]);
    
    if (resp_len && response && apdu_len > 4) {
        memcpy(response, &apdu[4], apdu_len - 4);
        *resp_len = apdu_len - 4;
    }
    
    return 0x9000;
}

int main(void) {
    printf("========================================\n");
    printf("  GCOS VM SELECT Command Test\n");
    printf("========================================\n\n");
    
    // Create and initialize VM
    GCOSVM *vm = gcos_vm_create();
    GCOSResult result = gcos_vm_init(vm);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ERROR: VM initialization failed: %d\n", result);
        return 1;
    }
    
    printf("[TEST] VM initialized successfully\n\n");
    
    // Register a test application
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
    result = app_register(vm, &test_aid, test_app_process, NULL, NULL, 0, &new_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ERROR: Application registration failed: %d\n", result);
        return 1;
    }
    
    printf("[TEST] Test application registered. App ID: %u\n\n", new_app_id);
    
    // Set application to SELECTABLE state
    GCOSAppInstance *test_app = app_find_by_id(vm, new_app_id);
    if (test_app) {
        test_app->lifecycle = APPLICATION_SELECTABLE;
        printf("[TEST] Application state set to SELECTABLE\n\n");
    }
    
    // Test 1: SELECT ISD (implicit selection - no AID)
    printf("=== Test 1: SELECT ISD (Implicit Selection) ===\n");
    u8 select_isd_apdu[] = {0x00, 0xA4, 0x04, 0x00};  // SELECT without AID
    u8 response[260];
    u16 resp_len = 0;
    
    printf("[TEST] Sending SELECT ISD APDU...\n");
    u16 sw = gcos_process_apdu(vm, select_isd_apdu, sizeof(select_isd_apdu), response, &resp_len);
    
    printf("[TEST] SELECT ISD result: SW=0x%04X\n", sw);
    printf("[TEST] Response length: %u\n", resp_len);
    
    if (sw == 0x9000) {
        printf("[TEST] ✓ SELECT ISD succeeded\n");
        
        // Verify ISD is selected
        if (vm->selected_app != NULL) {
            printf("[TEST] Selected app AID: ");
            for (int i = 0; i < vm->selected_app->app_aid.length; i++) {
                printf("%02X", vm->selected_app->app_aid.aid[i]);
            }
            printf("\n");
            
            if (vm->selected_app->app_id == APP_FIRST) {
                printf("[TEST] ✓ ISD is correctly selected\n\n");
            } else {
                printf("[TEST] ✗ ERROR: Wrong application selected\n\n");
            }
        } else {
            printf("[TEST] ✗ ERROR: No application selected after SELECT ISD\n\n");
        }
    } else {
        printf("[TEST] ✗ ERROR: SELECT ISD failed\n\n");
    }
    
    // Test 2: SELECT test application by AID
    printf("=== Test 2: SELECT Application by AID ===\n");
    u8 select_app_apdu[] = {
        0x00, 0xA4, 0x04, 0x00,  // CLA INS P1 P2
        0x08,                     // Lc = 8
        0xA0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01  // AID
    };
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    printf("[TEST] Sending SELECT Application APDU...\n");
    sw = gcos_process_apdu(vm, select_app_apdu, sizeof(select_app_apdu), response, &resp_len);
    
    printf("[TEST] SELECT Application result: SW=0x%04X\n", sw);
    printf("[TEST] Response length: %u\n", resp_len);
    
    if (sw == 0x9000) {
        printf("[TEST] ✓ SELECT Application succeeded\n");
        
        // Verify test app is selected
        if (vm->selected_app != NULL) {
            printf("[TEST] Selected app AID: ");
            for (int i = 0; i < vm->selected_app->app_aid.length; i++) {
                printf("%02X", vm->selected_app->app_aid.aid[i]);
            }
            printf("\n");
            
            if (vm->selected_app->app_id == new_app_id) {
                printf("[TEST] ✓ Test application is correctly selected\n\n");
            } else {
                printf("[TEST] ✗ ERROR: Wrong application selected\n\n");
            }
        } else {
            printf("[TEST] ✗ ERROR: No application selected after SELECT\n\n");
        }
    } else {
        printf("[TEST] ✗ ERROR: SELECT Application failed\n\n");
    }
    
    // Test 3: Send command to selected application
    printf("=== Test 3: Send Command to Selected Application ===\n");
    u8 test_apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05};
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    printf("[TEST] Sending test APDU to selected application...\n");
    sw = gcos_process_apdu(vm, test_apdu, sizeof(test_apdu), response, &resp_len);
    
    printf("[TEST] APDU result: SW=0x%04X\n", sw);
    printf("[TEST] Response length: %u\n", resp_len);
    
    if (sw == 0x9000) {
        printf("[TEST] ✓ Application command processed successfully\n\n");
    } else {
        printf("[TEST] ✗ ERROR: Application command failed\n\n");
    }
    
    // Test 4: SELECT non-existent application
    printf("=== Test 4: SELECT Non-Existent Application ===\n");
    u8 select_nonexistent_apdu[] = {
        0x00, 0xA4, 0x04, 0x00,
        0x08,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Non-existent AID
    };
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    printf("[TEST] Sending SELECT non-existent APDU...\n");
    sw = gcos_process_apdu(vm, select_nonexistent_apdu, sizeof(select_nonexistent_apdu), response, &resp_len);
    
    printf("[TEST] SELECT non-existent result: SW=0x%04X\n", sw);
    
    if (sw == 0x6A82) {
        printf("[TEST] ✓ Correctly returned FILE_NOT_FOUND (0x6A82)\n\n");
    } else {
        printf("[TEST] ✗ ERROR: Expected 0x6A82 but got 0x%04X\n\n", sw);
    }
    
    // Cleanup
    gcos_vm_destroy(vm);
    
    printf("========================================\n");
    printf("  All SELECT tests completed!\n");
    printf("========================================\n");
    
    return 0;
}
