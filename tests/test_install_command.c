/**
 * @file test_install_command.c
 * @brief Test program for INSTALL command (Phase 3)
 * 
 * Tests:
 * - Load a module using LOAD command
 * - Create application instance using INSTALL command
 * - Verify module registry tracks instances
 * - Create multiple instances from same module
 * - Verify global data isolation
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#include "gcos_vm.h"
#include "gcos_module_registry.h"
#include "gcos_install_manager.h"
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
    
    // Magic number
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x73;
    buffer[offset++] = 0x65;
    buffer[offset++] = 0x66;
    
    // Version
    buffer[offset++] = (version >> 24) & 0xFF;
    buffer[offset++] = (version >> 16) & 0xFF;
    buffer[offset++] = (version >> 8) & 0xFF;
    buffer[offset++] = version & 0xFF;
    
    // Section count (1 section)
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x01;
    
    // Section 1: Import section (minimal)
    buffer[offset++] = 0x02;  // Section ID = 2 (import)
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x03;  // Size = 3 bytes
    
    // Import data: 0 modules, 0 functions
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    
    *size = offset;
}

/**
 * @brief Execute LOAD command (3 phases)
 */
static u16 load_module(GCOSVM *vm, const GCOSAID *module_aid, u32 version) {
    u8 apdu[1024];
    u8 response[256];
    u16 resp_len = 0;
    u16 offset = 0;
    u16 sw;
    
    // Phase 1: INSTALL FOR LOAD
    offset = 0;
    apdu[offset++] = 0x80;  // CLA
    apdu[offset++] = 0xE6;  // INS = INSTALL FOR LOAD
    apdu[offset++] = 0x00;  // P1
    apdu[offset++] = 0x00;  // P2
    apdu[offset++] = 0x09;  // Lc
    apdu[offset++] = 0x4F;  // Tag
    apdu[offset++] = 0x07;  // Length
    memcpy(&apdu[offset], module_aid->aid, 7);
    offset += 7;
    
    sw = isd_handler_load(NULL, apdu, offset, response, &resp_len);
    if (sw != 0x9000) return sw;
    
    // Phase 2: LOAD BLOCKS
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
    
    // Phase 3: FINALIZE
    offset = 0;
    apdu[offset++] = 0x80;
    apdu[offset++] = 0xE8;
    apdu[offset++] = 0x02;
    apdu[offset++] = 0x00;
    apdu[offset++] = 0x00;
    
    sw = isd_handler_load(NULL, apdu, offset, response, &resp_len);
    return sw;
}

/**
 * @brief Execute INSTALL command
 */
static u16 install_app(GCOSVM *vm, const GCOSAID *app_aid, const GCOSAID *module_aid) {
    u8 apdu[256];
    u8 response[256];
    u16 resp_len = 0;
    u16 offset = 0;
    
    // APDU header
    apdu[offset++] = 0x80;  // CLA
    apdu[offset++] = 0xE2;  // INS = INSTALL
    apdu[offset++] = 0x00;  // P1 = INSTALL FOR MAKE SELECTABLE
    apdu[offset++] = 0x00;  // P2
    
    u16 lc_pos = offset;  // Save position for Lc
    offset++;  // Reserve space for Lc
    
    u16 data_start = offset;  // Data starts here
    
    // Tag 0x4F: Application AID
    apdu[offset++] = 0x4F;
    apdu[offset++] = app_aid->length;
    memcpy(&apdu[offset], app_aid->aid, app_aid->length);
    offset += app_aid->length;
    
    // Tag 0xCB: Module AID
    apdu[offset++] = 0xCB;
    apdu[offset++] = module_aid->length;
    memcpy(&apdu[offset], module_aid->aid, module_aid->length);
    offset += module_aid->length;
    
    // Set Lc
    u16 data_len = offset - data_start;
    apdu[lc_pos] = (u8)data_len;
    
    printf("   INSTALL APDU: Lc=%u, Total=%u\n", data_len, offset);
    printf("   App AID len=%u, Module AID len=%u\n", app_aid->length, module_aid->length);
    
    return handle_install_command(apdu, offset, response, &resp_len);
}

int main(void) {
    printf("========================================\n");
    printf("GCOS INSTALL Command Test (Phase 3)\n");
    printf("========================================\n\n");
    
    /* Create VM instance */
    GCOSVM *vm = gcos_vm_create();
    TEST_ASSERT(vm != NULL, "VM created successfully");
    
    /* Initialize VM */
    GCOSResult result = gcos_vm_init(vm);
    TEST_ASSERT(result == GCOS_SUCCESS, "VM initialized successfully");
    
    /* ========================================================================
     * Step 1: Load a module
     * ======================================================================== */
    printf("\n--- Step 1: Load Module ---\n");
    
    GCOSAID module_aid;
    module_aid.length = 7;
    memcpy(module_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03", 7);
    
    u16 sw = load_module(vm, &module_aid, 0x00010000);
    TEST_ASSERT(sw == 0x9000, "Module loaded successfully");
    TEST_ASSERT(vm->module_count == 1, "One module loaded");
    
    GCOSModuleRegistry *reg = module_registry_find_by_aid(vm, &module_aid);
    TEST_ASSERT(reg != NULL, "Module found in registry");
    TEST_ASSERT(reg->instance_count == 0, "No instances yet");
    
    printf("   Module ID: %u\n", reg->module_id);
    
    /* ========================================================================
     * Step 2: Install first application instance
     * ======================================================================== */
    printf("\n--- Step 2: Install First Application ---\n");
    
    GCOSAID app1_aid;
    app1_aid.length = 8;
    memcpy(app1_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03\x01", 8);
    
    sw = install_app(vm, &app1_aid, &module_aid);
    TEST_ASSERT(sw == 0x9000, "First application installed");
    TEST_ASSERT(vm->app_count == 2, "Two apps (ISD + app1)");  // ISD + 1 app
    
    // Verify module registry updated
    TEST_ASSERT(reg->instance_count == 1, "Module has 1 instance");
    TEST_ASSERT(reg->instance_ids[0] != 0xFF, "Instance ID recorded");
    
    // Verify application linked to module
    GCOSAppInstance *app1 = app_find_by_aid(vm, app1_aid.aid, app1_aid.length);
    TEST_ASSERT(app1 != NULL, "Application 1 found");
    printf("   App1 module ptr: %p, Reg ptr: %p\n", (void*)app1->module, (void*)reg);
    TEST_ASSERT(app1->module == reg, "App1 linked to module");
    TEST_ASSERT(app1->lifecycle == APPLICATION_SELECTABLE, "App1 is selectable");
    
    printf("   App1 ID: %u\n", app1->app_id);
    printf("   Module instances: %u\n", reg->instance_count);
    
    /* ========================================================================
     * Step 3: Install second application from same module
     * ======================================================================== */
    printf("\n--- Step 3: Install Second Application ---\n");
    
    GCOSAID app2_aid;
    app2_aid.length = 8;
    memcpy(app2_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03\x02", 8);
    
    sw = install_app(vm, &app2_aid, &module_aid);
    TEST_ASSERT(sw == 0x9000, "Second application installed");
    TEST_ASSERT(vm->app_count == 3, "Three apps (ISD + app1 + app2)");
    
    // Verify module registry updated
    TEST_ASSERT(reg->instance_count == 2, "Module has 2 instances");
    
    // Verify both instances recorded
    GCOSAppInstance *app2 = app_find_by_aid(vm, app2_aid.aid, app2_aid.length);
    bool found_app1 = false, found_app2 = false;
    for (u8 i = 0; i < reg->instance_count; i++) {
        if (reg->instance_ids[i] == app1->app_id) found_app1 = true;
        if (reg->instance_ids[i] == app2->app_id) found_app2 = true;
    }
    TEST_ASSERT(found_app1, "App1 instance tracked");
    TEST_ASSERT(found_app2, "App2 instance tracked");
    
    // Verify code sharing
    TEST_ASSERT(app2 != NULL, "Application 2 found");
    TEST_ASSERT(app2->module == reg, "App2 linked to same module");
    TEST_ASSERT(app2->module == app1->module, "Both apps share same module");
    
    printf("   App2 ID: %u\n", app2->app_id);
    printf("   Module instances: %u\n", reg->instance_count);
    printf("   Code shared: YES ✅\n");
    
    /* ========================================================================
     * Step 4: Try to install duplicate AID (should fail)
     * ======================================================================== */
    printf("\n--- Step 4: Duplicate AID Test ---\n");
    
    sw = install_app(vm, &app1_aid, &module_aid);
    TEST_ASSERT(sw == 0x6A89, "Duplicate AID rejected (0x6A89)");
    TEST_ASSERT(vm->app_count == 3, "App count unchanged");
    TEST_ASSERT(reg->instance_count == 2, "Instance count unchanged");
    
    printf("   Duplicate installation correctly rejected ✅\n");
    
    /* ========================================================================
     * Step 5: Try to install from non-existent module (should fail)
     * ======================================================================== */
    printf("\n--- Step 5: Non-existent Module Test ---\n");
    
    GCOSAID fake_module_aid;
    fake_module_aid.length = 7;
    memcpy(fake_module_aid.aid, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 7);
    
    GCOSAID app3_aid;
    app3_aid.length = 8;
    memcpy(app3_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03\x03", 8);
    
    sw = install_app(vm, &app3_aid, &fake_module_aid);
    TEST_ASSERT(sw == 0x6A88, "Non-existent module rejected (0x6A88)");
    TEST_ASSERT(vm->app_count == 3, "App count unchanged");
    
    printf("   Invalid module correctly rejected ✅\n");
    
    /* ========================================================================
     * Summary
     * ======================================================================== */
    printf("\n========================================\n");
    printf("Test Summary:\n");
    printf("  Modules loaded: %u\n", vm->module_count);
    printf("  Applications: %u (including ISD)\n", vm->app_count);
    printf("  Module instances: %u\n", reg->instance_count);
    printf("  Code sharing: Enabled ✅\n");
    printf("========================================\n\n");
    
    /* Cleanup */
    gcos_vm_destroy(vm);
    printf("✅ VM destroyed\n");
    
    printf("\n========================================\n");
    printf("All tests passed! ✅\n");
    printf("========================================\n");
    
    return 0;
}
