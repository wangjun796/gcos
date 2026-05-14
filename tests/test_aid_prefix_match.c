/**
 * @file test_aid_prefix_match.c
 * @brief Test AID prefix matching (partial AID selection)
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
    return 0x9000;
}

int main(void) {
    printf("========================================\n");
    printf("  GCOS VM AID Prefix Matching Test\n");
    printf("========================================\n\n");
    
    // Create and initialize VM
    GCOSVM *vm = gcos_vm_create();
    GCOSResult result = gcos_vm_init(vm);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ERROR: VM initialization failed: %d\n", result);
        return 1;
    }
    
    printf("[TEST] VM initialized successfully\n\n");
    
    // Register an application with 10-byte AID
    GCOSAID long_aid;
    long_aid.length = 10;
    long_aid.aid[0] = 0xA0;
    long_aid.aid[1] = 0x00;
    long_aid.aid[2] = 0x00;
    long_aid.aid[3] = 0x00;
    long_aid.aid[4] = 0x62;
    long_aid.aid[5] = 0x03;
    long_aid.aid[6] = 0x01;
    long_aid.aid[7] = 0x0C;
    long_aid.aid[8] = 0x06;
    long_aid.aid[9] = 0x01;
    
    u8 new_app_id;
    result = app_register(vm, &long_aid, test_app_process, NULL, NULL, 0, &new_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ERROR: Application registration failed: %d\n", result);
        return 1;
    }
    
    printf("[TEST] Registered app with 10-byte AID: ");
    for (int i = 0; i < long_aid.length; i++) {
        printf("%02X", long_aid.aid[i]);
    }
    printf("\n");
    
    // Set application to SELECTABLE state
    GCOSAppInstance *test_app = app_find_by_id(vm, new_app_id);
    if (test_app) {
        test_app->lifecycle = APPLICATION_SELECTABLE;
    }
    printf("[TEST] Application state set to SELECTABLE\n\n");
    
    // Test 1: Select with full 10-byte AID
    printf("=== Test 1: SELECT with Full 10-byte AID ===\n");
    u8 select_full[] = {
        0x00, 0xA4, 0x04, 0x00, 0x0A,  // Lc=10
        0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x0C, 0x06, 0x01
    };
    u8 response[260];
    u16 resp_len = 0;
    
    u16 sw = gcos_process_apdu(vm, select_full, sizeof(select_full), response, &resp_len);
    printf("[TEST] Result: SW=0x%04X ", sw);
    if (sw == 0x9000 && vm->selected_app != NULL && vm->selected_app->app_id == new_app_id) {
        printf("✓ PASS\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    // Test 2: Select with 8-byte prefix (should match)
    printf("=== Test 2: SELECT with 8-byte Prefix ===\n");
    u8 select_8byte[] = {
        0x00, 0xA4, 0x04, 0x00, 0x08,  // Lc=8
        0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x0C
    };
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    sw = gcos_process_apdu(vm, select_8byte, sizeof(select_8byte), response, &resp_len);
    printf("[TEST] Result: SW=0x%04X ", sw);
    if (sw == 0x9000 && vm->selected_app != NULL && vm->selected_app->app_id == new_app_id) {
        printf("✓ PASS (prefix matched)\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    // Test 3: Select with 5-byte prefix (should match)
    printf("=== Test 3: SELECT with 5-byte Prefix ===\n");
    u8 select_5byte[] = {
        0x00, 0xA4, 0x04, 0x00, 0x05,  // Lc=5
        0xA0, 0x00, 0x00, 0x00, 0x62
    };
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    sw = gcos_process_apdu(vm, select_5byte, sizeof(select_5byte), response, &resp_len);
    printf("[TEST] Result: SW=0x%04X ", sw);
    if (sw == 0x9000 && vm->selected_app != NULL && vm->selected_app->app_id == new_app_id) {
        printf("✓ PASS (prefix matched)\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    // Test 4: Select ISD with partial AID (first 5 bytes)
    printf("=== Test 4: SELECT ISD with 5-byte Prefix ===\n");
    u8 select_isd_partial[] = {
        0x00, 0xA4, 0x04, 0x00, 0x05,  // Lc=5
        0xA0, 0x00, 0x00, 0x01, 0x51
    };
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    sw = gcos_process_apdu(vm, select_isd_partial, sizeof(select_isd_partial), response, &resp_len);
    printf("[TEST] Result: SW=0x%04X ", sw);
    if (sw == 0x9000 && vm->selected_app != NULL && vm->selected_app->app_id == APP_FIRST) {
        printf("✓ PASS (ISD selected by prefix)\n\n");
    } else {
        printf("✗ FAIL\n\n");
    }
    
    // Test 5: Select with wrong prefix (should fail)
    printf("=== Test 5: SELECT with Wrong Prefix ===\n");
    u8 select_wrong[] = {
        0x00, 0xA4, 0x04, 0x00, 0x05,  // Lc=5
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    
    memset(response, 0, sizeof(response));
    resp_len = 0;
    
    sw = gcos_process_apdu(vm, select_wrong, sizeof(select_wrong), response, &resp_len);
    printf("[TEST] Result: SW=0x%04X ", sw);
    if (sw == 0x6A82) {
        printf("✓ PASS (correctly returned FILE_NOT_FOUND)\n\n");
    } else {
        printf("✗ FAIL (expected 0x6A82)\n\n");
    }
    
    // Cleanup
    gcos_vm_destroy(vm);
    
    printf("========================================\n");
    printf("  All prefix matching tests completed!\n");
    printf("========================================\n");
    
    return 0;
}
