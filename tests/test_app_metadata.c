/**
 * @file test_app_metadata.c
 * @brief Test application metadata fields (type, privileges, security domain)
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

/* Test install callback */
static GCOSResult test_app_install(GCOSAppInstance *app,
                                   const u8 *install_data,
                                   u16 install_data_len) {
    printf("[TestApp] install() called with %u bytes of data\n", install_data_len);
    
    // Store install parameter if provided
    if (install_data_len > 0) {
        app->install_param = install_data[0];
        printf("[TestApp] Install param set to: 0x%02X\n", app->install_param);
    }
    
    return GCOS_SUCCESS;
}

int main(void) {
    printf("========================================\n");
    printf("  GCOS VM Application Metadata Test\n");
    printf("========================================\n\n");
    
    // Create and initialize VM
    GCOSVM *vm = gcos_vm_create();
    GCOSResult result = gcos_vm_init(vm);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ERROR: VM initialization failed: %d\n", result);
        return 1;
    }
    
    printf("[TEST] VM initialized successfully\n\n");
    
    // Test 1: Verify ISD metadata
    printf("=== Test 1: ISD Metadata ===\n");
    GCOSAppInstance *isd = app_find_by_id(vm, APP_FIRST);
    
    if (isd == NULL) {
        printf("[TEST] ✗ FAIL: ISD not found\n\n");
        return 1;
    }
    
    printf("[TEST] ISD AID: ");
    for (int i = 0; i < isd->app_aid.length; i++) {
        printf("%02X", isd->app_aid.aid[i]);
    }
    printf("\n");
    
    printf("[TEST] ISD Type: %s ", 
           isd->app_type == APP_TYPE_ISD ? "ISD ✓" : "ERROR ✗");
    printf("(expected: APP_TYPE_ISD = 0x%02X, actual: 0x%02X)\n",
           APP_TYPE_ISD, isd->app_type);
    
    printf("[TEST] ISD Security Domain ID: %s ",
           isd->security_domain_id == APP_FIRST ? "Correct ✓" : "ERROR ✗");
    printf("(expected: 0x%02X, actual: 0x%02X)\n",
           APP_FIRST, isd->security_domain_id);
    
    printf("[TEST] ISD Privileges: 0x%02X 0x%02X 0x%02X ",
           isd->privilege_byte1, isd->privilege_byte2, isd->privilege_byte3);
    if (isd->privilege_byte1 == 0xFF && isd->privilege_byte2 == 0xFF && isd->privilege_byte3 == 0xFF) {
        printf("✓ (All privileges)\n\n");
    } else {
        printf("✗ (Expected: 0xFF 0xFF 0xFF)\n\n");
    }
    
    // Test 2: Register app with extended API
    printf("=== Test 2: Register App with Extended API ===\n");
    GCOSAID test_aid;
    test_aid.length = 8;
    test_aid.aid[0] = 0xA0;
    test_aid.aid[1] = 0x00;
    test_aid.aid[2] = 0x00;
    test_aid.aid[3] = 0x00;
    test_aid.aid[4] = 0x01;
    test_aid.aid[5] = 0x00;
    test_aid.aid[6] = 0x00;
    test_aid.aid[7] = 0x02;
    
    u8 new_app_id;
    result = app_register_ex(vm, &test_aid, 
                             test_app_process,
                             NULL,   // on_select
                             NULL,   // on_deselect
                             test_app_install,  // on_install ⭐
                             0,      // module_index
                             APP_TYPE_REGULAR,  // app_type
                             APP_FIRST,         // security_domain_id (belongs to ISD)
                             0x10,              // privilege_byte1 (some privilege)
                             &new_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ✗ FAIL: Application registration failed: %d\n\n", result);
        return 1;
    }
    
    printf("[TEST] Application registered. ID: %u\n", new_app_id);
    
    // Verify metadata
    GCOSAppInstance *test_app = app_find_by_id(vm, new_app_id);
    
    if (test_app == NULL) {
        printf("[TEST] ✗ FAIL: Application not found\n\n");
        return 1;
    }
    
    printf("[TEST] App Type: %s ",
           test_app->app_type == APP_TYPE_REGULAR ? "REGULAR ✓" : "ERROR ✗");
    printf("(expected: 0x%02X, actual: 0x%02X)\n",
           APP_TYPE_REGULAR, test_app->app_type);
    
    printf("[TEST] Security Domain ID: %s ",
           test_app->security_domain_id == APP_FIRST ? "ISD ✓" : "ERROR ✗");
    printf("(expected: 0x%02X, actual: 0x%02X)\n",
           APP_FIRST, test_app->security_domain_id);
    
    printf("[TEST] Privilege Byte 1: 0x%02X ", test_app->privilege_byte1);
    if (test_app->privilege_byte1 == 0x10) {
        printf("✓\n");
    } else {
        printf("✗ (expected: 0x10)\n");
    }
    
    printf("[TEST] Install Callback: %s\n",
           test_app->on_install != NULL ? "Set ✓" : "NULL ✗");
    
    // Set to SELECTABLE state
    test_app->lifecycle = APPLICATION_SELECTABLE;
    printf("[TEST] Application state set to SELECTABLE\n\n");
    
    // Test 3: Call install callback
    printf("=== Test 3: Call Install Callback ===\n");
    if (test_app->on_install != NULL) {
        u8 install_data[] = {0x42};  // Install parameter
        result = test_app->on_install(test_app, install_data, sizeof(install_data));
        
        if (result == GCOS_SUCCESS) {
            printf("[TEST] ✓ Install callback executed successfully\n");
            printf("[TEST] Install param stored: 0x%02X ", test_app->install_param);
            if (test_app->install_param == 0x42) {
                printf("✓\n\n");
            } else {
                printf("✗ (expected: 0x42)\n\n");
            }
        } else {
            printf("[TEST] ✗ FAIL: Install callback failed: %d\n\n", result);
        }
    } else {
        printf("[TEST] ✗ FAIL: Install callback is NULL\n\n");
    }
    
    // Test 4: Compare simple vs extended registration
    printf("=== Test 4: Simple vs Extended Registration ===\n");
    GCOSAID simple_aid;
    simple_aid.length = 8;
    memcpy(simple_aid.aid, test_aid.aid, 8);
    simple_aid.aid[7] = 0x03;
    
    u8 simple_app_id;
    result = app_register(vm, &simple_aid, test_app_process, NULL, NULL, 0, &simple_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[TEST] ✗ FAIL: Simple registration failed: %d\n\n", result);
        return 1;
    }
    
    GCOSAppInstance *simple_app = app_find_by_id(vm, simple_app_id);
    
    printf("[TEST] Simple registration defaults:\n");
    printf("[TEST]   Type: 0x%02X (should be APP_TYPE_REGULAR = 0x%02X) %s\n",
           simple_app->app_type, APP_TYPE_REGULAR,
           simple_app->app_type == APP_TYPE_REGULAR ? "✓" : "✗");
    
    printf("[TEST]   Security Domain: 0x%02X (should be 0xFF) %s\n",
           simple_app->security_domain_id,
           simple_app->security_domain_id == 0xFF ? "✓" : "✗");
    
    printf("[TEST]   Privilege Byte 1: 0x%02X (should be 0x00) %s\n",
           simple_app->privilege_byte1,
           simple_app->privilege_byte1 == 0x00 ? "✓" : "✗");
    
    printf("[TEST]   Install Callback: %s %s\n",
           simple_app->on_install == NULL ? "NULL" : "Set",
           simple_app->on_install == NULL ? "✓" : "✗");
    
    simple_app->lifecycle = APPLICATION_SELECTABLE;
    printf("\n");
    
    // Cleanup
    gcos_vm_destroy(vm);
    
    printf("========================================\n");
    printf("  All metadata tests completed!\n");
    printf("========================================\n");
    
    return 0;
}
