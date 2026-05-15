/**
 * @file test_delete_command.c
 * @brief Test program for DELETE command (Phase 5)
 * 
 * Tests:
 * - Load a module
 * - Install multiple application instances
 * - Delete single application via DELETE command
 * - Delete module with related apps
 * - Verify proper cleanup
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#include "gcos_vm.h"
#include "gcos_module_registry.h"
#include "gcos_install_manager.h"
#include "gcos_delete_manager.h"
#include <stdio.h>
#include <string.h>

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

/**
 * @brief Create a minimal SEF file for testing
 */
static void create_test_sef(u8 *buffer, u32 *size, const GCOSAID *aid, u32 version) {
    u32 offset = 0;
    
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x73;
    buffer[offset++] = 0x65;
    buffer[offset++] = 0x66;
    
    buffer[offset++] = (version >> 24) & 0xFF;
    buffer[offset++] = (version >> 16) & 0xFF;
    buffer[offset++] = (version >> 8) & 0xFF;
    buffer[offset++] = version & 0xFF;
    
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x01;
    
    buffer[offset++] = 0x02;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x03;
    
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    
    *size = offset;
}

/**
 * @brief Execute LOAD command
 */
static u16 load_module(GCOSVM *vm, const GCOSAID *module_aid, u32 version) {
    u8 apdu[1024];
    u8 response[256];
    u16 resp_len = 0;
    u16 offset = 0;
    
    // Phase 1
    offset = 0;
    apdu[offset++] = 0x80;
    apdu[offset++] = 0xE6;
    apdu[offset++] = 0x00;
    apdu[offset++] = 0x00;
    apdu[offset++] = 0x09;
    apdu[offset++] = 0x4F;
    apdu[offset++] = 0x07;
    memcpy(&apdu[offset], module_aid->aid, 7);
    offset += 7;
    
    u16 sw = isd_handler_load(NULL, apdu, offset, response, &resp_len);
    if (sw != 0x9000) return sw;
    
    // Phase 2
    u8 sef_buffer[1024];
    u32 sef_size = 0;
    create_test_sef(sef_buffer, &sef_size, module_aid, version);
    
    offset = 0;
    apdu[offset++] = 0x80;
    apdu[offset++] = 0xE8;
    apdu[offset++] = 0x01;
    apdu[offset++] = 0x00;
    apdu[offset++] = (u8)sef_size;
    memcpy(&apdu[offset], sef_buffer, sef_size);
    offset += sef_size;
    
    sw = isd_handler_load(NULL, apdu, offset, response, &resp_len);
    if (sw != 0x9000) return sw;
    
    // Phase 3
    offset = 0;
    apdu[offset++] = 0x80;
    apdu[offset++] = 0xE8;
    apdu[offset++] = 0x02;
    apdu[offset++] = 0x00;
    apdu[offset++] = 0x00;
    
    return isd_handler_load(NULL, apdu, offset, response, &resp_len);
}

/**
 * @brief Execute INSTALL command
 */
static u16 install_app(GCOSVM *vm, const GCOSAID *app_aid, const GCOSAID *module_aid) {
    u8 apdu[256];
    u8 response[256];
    u16 resp_len = 0;
    u16 offset = 0;
    
    apdu[offset++] = 0x80;
    apdu[offset++] = 0xE2;
    apdu[offset++] = 0x00;
    apdu[offset++] = 0x00;
    
    u16 lc_pos = offset++;
    u16 data_start = offset;
    
    apdu[offset++] = 0x4F;
    apdu[offset++] = app_aid->length;
    memcpy(&apdu[offset], app_aid->aid, app_aid->length);
    offset += app_aid->length;
    
    apdu[offset++] = 0xCB;
    apdu[offset++] = module_aid->length;
    memcpy(&apdu[offset], module_aid->aid, module_aid->length);
    offset += module_aid->length;
    
    apdu[lc_pos] = (u8)(offset - data_start);
    
    return handle_install_command(apdu, offset, response, &resp_len);
}

int main(void) {
    printf("========================================\n");
    printf("GCOS DELETE Command Test (Phase 5)\n");
    printf("========================================\n\n");
    
    GCOSVM *vm = gcos_vm_create();
    TEST_ASSERT(vm != NULL, "VM created");
    
    GCOSResult result = gcos_vm_init(vm);
    TEST_ASSERT(result == GCOS_SUCCESS, "VM initialized");
    
    /* ========================================================================
     * Step 1: Load a module
     * ======================================================================== */
    printf("\n--- Step 1: Load Module ---\n");
    
    GCOSAID module_aid;
    module_aid.length = 7;
    memcpy(module_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03", 7);
    
    u16 sw = load_module(vm, &module_aid, 0x00010000);
    TEST_ASSERT(sw == 0x9000, "Module loaded");
    
    GCOSModuleRegistry *reg = module_registry_find_by_aid(vm, &module_aid);
    TEST_ASSERT(reg != NULL, "Module found");
    u8 module_id = reg->module_id;
    
    /* ========================================================================
     * Step 2: Install two applications
     * ======================================================================== */
    printf("\n--- Step 2: Install Two Applications ---\n");
    
    GCOSAID app1_aid, app2_aid;
    app1_aid.length = 8;
    memcpy(app1_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03\x01", 8);
    
    app2_aid.length = 8;
    memcpy(app2_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03\x02", 8);
    
    sw = install_app(vm, &app1_aid, &module_aid);
    TEST_ASSERT(sw == 0x9000, "App1 installed");
    
    sw = install_app(vm, &app2_aid, &module_aid);
    TEST_ASSERT(sw == 0x9000, "App2 installed");
    
    TEST_ASSERT(reg->instance_count == 2, "Module has 2 instances");
    printf("   Total apps: %u (including ISD)\n", vm->app_count);
    
    /* ========================================================================
     * Step 3: Delete App1 using DELETE command
     * ======================================================================== */
    printf("\n--- Step 3: Delete App1 via DELETE Command ---\n");
    
    // Build DELETE APDU for App1
    u8 delete_apdu[256];
    u8 response[256];
    u16 resp_len = 0;
    u16 offset = 0;
    
    delete_apdu[offset++] = 0x80;  // CLA
    delete_apdu[offset++] = 0xE6;  // INS = DELETE
    delete_apdu[offset++] = 0x02;  // P1 = Delete from package (app instance)
    delete_apdu[offset++] = 0x00;  // P2
    delete_apdu[offset++] = 0x0A;  // Lc = 10 (tag + len + 8-byte AID)
    delete_apdu[offset++] = 0x4F;  // Tag
    delete_apdu[offset++] = 0x08;  // Length
    memcpy(&delete_apdu[offset], app1_aid.aid, 8);
    offset += 8;
    
    sw = handle_delete_command(delete_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "DELETE command succeeded");
    TEST_ASSERT(reg->instance_count == 1, "Module now has 1 instance");
    TEST_ASSERT(vm->app_count == 2, "Apps: ISD + App2");
    
    printf("   Remaining instances: %u\n", reg->instance_count);
    
    /* ========================================================================
     * Step 4: Try to delete module without deleting apps (should fail)
     * ======================================================================== */
    printf("\n--- Step 4: Try to Delete Module (Should Fail) ---\n");
    
    offset = 0;
    delete_apdu[offset++] = 0x80;
    delete_apdu[offset++] = 0xE6;
    delete_apdu[offset++] = 0x04;  // P1 = Delete package only
    delete_apdu[offset++] = 0x00;
    delete_apdu[offset++] = 0x09;
    delete_apdu[offset++] = 0x4F;
    delete_apdu[offset++] = 0x07;
    memcpy(&delete_apdu[offset], module_aid.aid, 7);
    offset += 7;
    
    sw = handle_delete_command(delete_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x6985, "DELETE module failed (has instances)");
    TEST_ASSERT(reg->is_loaded == true, "Module still loaded");
    
    printf("   Module correctly protected ✅\n");
    
    /* ========================================================================
     * Step 5: Delete module with related apps
     * ======================================================================== */
    printf("\n--- Step 5: Delete Module with Related Apps ---\n");
    
    offset = 0;
    delete_apdu[offset++] = 0x80;
    delete_apdu[offset++] = 0xE6;
    delete_apdu[offset++] = 0x05;  // P1 = Delete package + related (0x04 | 0x01)
    delete_apdu[offset++] = 0x00;
    delete_apdu[offset++] = 0x09;
    delete_apdu[offset++] = 0x4F;
    delete_apdu[offset++] = 0x07;
    memcpy(&delete_apdu[offset], module_aid.aid, 7);
    offset += 7;
    
    sw = handle_delete_command(delete_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "DELETE module with related apps succeeded");
    TEST_ASSERT(reg->is_loaded == false, "Module unloaded");
    TEST_ASSERT(vm->module_count == 0, "No modules remaining");
    
    printf("   Module and all instances deleted ✅\n");
    
    /* ========================================================================
     * Summary
     * ======================================================================== */
    printf("\n========================================\n");
    printf("Test Summary:\n");
    printf("  ✅ DELETE command works correctly\n");
    printf("  ✅ Single app deletion works\n");
    printf("  ✅ Module protection when instances exist\n");
    printf("  ✅ Batch deletion (module + apps) works\n");
    printf("  ✅ Proper cleanup performed\n");
    printf("========================================\n\n");
    
    gcos_vm_destroy(vm);
    printf("✅ VM destroyed\n");
    
    printf("\n========================================\n");
    printf("All tests passed! ✅\n");
    printf("========================================\n");
    
    return 0;
}
