/**
 * @file test_load_module_registry.c
 * @brief Test program for LOAD command with Module Registry integration
 * 
 * Tests:
 * - Phase 1: INSTALL FOR LOAD
 * - Phase 2: LOAD BLOCKS
 * - Phase 3: FINALIZE (with module registry)
 * - Module lookup by AID and ID after loading
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

/**
 * @brief Create a minimal SEF file for testing
 * 
 * SEF format (simplified):
 * - Magic: 0x00736566 ("sef")
 * - Version: u32
 * - Section count: u32
 * - Sections...
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
    
    // Section count (1 section for simplicity)
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x01;
    
    // Section 1: Import section (minimal)
    buffer[offset++] = 0x02;  // Section ID = 2 (import)
    
    // Section size (empty import section)
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x03;  // Size = 3 bytes
    
    // Import data: 0 modules, 0 functions
    buffer[offset++] = 0x00;  // import_module_count
    buffer[offset++] = 0x00;  // import_function_count (high byte)
    buffer[offset++] = 0x00;  // import_function_count (low byte)
    
    *size = offset;
}

int main(void) {
    printf("========================================\n");
    printf("GCOS LOAD Command + Module Registry Test\n");
    printf("========================================\n\n");
    
    /* Create VM instance with eflash */
    GCOSVM *vm = NULL;
    GCOSResult result = test_vm_create_and_init(&vm);
    TEST_ASSERT(vm != NULL && result == GCOS_SUCCESS, "VM created and initialized successfully");
    
    /* ========================================================================
     * Test 1: INSTALL FOR LOAD (Phase 1)
     * ======================================================================== */
    printf("\n--- Test 1: INSTALL FOR LOAD ---\n");
    
    // Prepare APDU for INSTALL FOR LOAD
    u8 install_apdu[100];
    u8 offset = 0;
    
    // CLA INS P1 P2
    install_apdu[offset++] = 0x80;  // CLA
    install_apdu[offset++] = 0xE6;  // INS = INSTALL FOR LOAD
    install_apdu[offset++] = 0x00;  // P1 = INSTALL FOR LOAD
    install_apdu[offset++] = 0x00;  // P2
    
    // Package AID (Tag 0x4F)
    GCOSAID test_aid;
    test_aid.length = 7;
    memcpy(test_aid.aid, "\xA0\x00\x00\x00\x01\x02\x03", 7);
    
    install_apdu[offset++] = 0x09;  // Lc = 9 (1 tag + 1 len + 7 aid)
    install_apdu[offset++] = 0x4F;  // Tag = Package AID
    install_apdu[offset++] = 0x07;  // Length = 7
    memcpy(&install_apdu[offset], test_aid.aid, 7);
    offset += 7;
    
    u16 resp_len = 0;
    u8 response[256];
    
    u16 sw = isd_handler_load(NULL, install_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "INSTALL FOR LOAD succeeded");
    TEST_ASSERT(vm->load_context.state == LOAD_STATE_INITIALIZATION, 
                "Load context in INITIALIZATION state");
    TEST_ASSERT(vm->load_context.target_module_id != 0xFF, 
                "Module ID allocated");
    
    printf("   Module ID allocated: %u\n", vm->load_context.target_module_id);
    
    /* ========================================================================
     * Test 2: LOAD BLOCKS (Phase 2)
     * ======================================================================== */
    printf("\n--- Test 2: LOAD BLOCKS ---\n");
    
    // Create test SEF file
    u8 sef_buffer[1024];
    u32 sef_size = 0;
    create_test_sef(sef_buffer, &sef_size, &test_aid, 0x00010000);
    
    printf("   Created test SEF: %u bytes\n", sef_size);
    
    // Prepare APDU for LOAD BLOCKS
    u8 load_apdu[1024];
    offset = 0;
    
    load_apdu[offset++] = 0x80;  // CLA
    load_apdu[offset++] = 0xE8;  // INS = LOAD
    load_apdu[offset++] = 0x01;  // P1 = LOAD BLOCKS
    load_apdu[offset++] = 0x00;  // P2
    load_apdu[offset++] = (u8)sef_size;  // Lc
    
    memcpy(&load_apdu[offset], sef_buffer, sef_size);
    offset += sef_size;
    
    resp_len = 0;
    sw = isd_handler_load(NULL, load_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "LOAD BLOCKS succeeded");
    TEST_ASSERT(vm->load_context.state == LOAD_STATE_LOADING_BLOCKS,
                "Load context in LOADING_BLOCKS state");
    TEST_ASSERT(vm->load_context.buffer_size == sef_size,
                "Buffer size matches SEF size");
    
    printf("   Buffer size: %u bytes\n", vm->load_context.buffer_size);
    
    /* ========================================================================
     * Test 3: FINALIZE (Phase 3)
     * ======================================================================== */
    printf("\n--- Test 3: FINALIZE ---\n");
    
    // Prepare APDU for FINALIZE
    u8 finalize_apdu[5];
    offset = 0;
    
    finalize_apdu[offset++] = 0x80;  // CLA
    finalize_apdu[offset++] = 0xE8;  // INS = LOAD
    finalize_apdu[offset++] = 0x02;  // P1 = FINALIZE
    finalize_apdu[offset++] = 0x00;  // P2
    finalize_apdu[offset++] = 0x00;  // Lc = 0 (no data)
    
    resp_len = 0;
    sw = isd_handler_load(NULL, finalize_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "FINALIZE succeeded");
    TEST_ASSERT(vm->load_context.state == LOAD_STATE_IDLE,
                "Load context reset to IDLE");
    
    /* ========================================================================
     * Test 4: Verify Module Registry Entry
     * ======================================================================== */
    printf("\n--- Test 4: Verify Module Registry ---\n");
    
    u8 module_id = vm->module_count > 0 ? 0 : 0xFF;
    TEST_ASSERT(module_id != 0xFF, "Module exists in registry");
    
    // Find module by ID
    GCOSModuleRegistry *reg = module_registry_find_by_id(vm, module_id);
    TEST_ASSERT(reg != NULL, "Module found by ID");
    TEST_ASSERT(reg->is_loaded == true, "Module is loaded");
    TEST_ASSERT(reg->module_id == module_id, "Module ID matches");
    
    // Verify AID
    TEST_ASSERT(reg->module_aid.length == test_aid.length, "AID length matches");
    TEST_ASSERT(memcmp(reg->module_aid.aid, test_aid.aid, test_aid.length) == 0,
                "AID content matches");
    
    // Verify version
    TEST_ASSERT(reg->module_version == 0x00010000, "Version matches");
    
    // Verify code size
    TEST_ASSERT(reg->code_size == sef_size, "Code size matches SEF size");
    
    // Verify no instances yet
    TEST_ASSERT(reg->instance_count == 0, "No app instances yet");
    
    printf("   Module ID: %u\n", reg->module_id);
    printf("   AID: ");
    for (int i = 0; i < reg->module_aid.length; i++) {
        printf("%02X", reg->module_aid.aid[i]);
    }
    printf("\n");
    printf("   Version: 0x%08X\n", reg->module_version);
    printf("   Code Size: %u bytes\n", reg->code_size);
    printf("   Instances: %u\n", reg->instance_count);
    
    /* ========================================================================
     * Test 5: Find Module by AID
     * ======================================================================== */
    printf("\n--- Test 5: Find Module by AID ---\n");
    
    GCOSModuleRegistry *reg_by_aid = module_registry_find_by_aid(vm, &test_aid);
    TEST_ASSERT(reg_by_aid != NULL, "Module found by AID");
    TEST_ASSERT(reg_by_aid == reg, "Same module returned");
    
    /* ========================================================================
     * Test 6: Load Second Module
     * ======================================================================== */
    printf("\n--- Test 6: Load Second Module ---\n");
    
    // Create second module with different AID
    GCOSAID test_aid_2;
    test_aid_2.length = 7;
    memcpy(test_aid_2.aid, "\xA0\x00\x00\x00\x01\x02\x04", 7);
    
    // INSTALL FOR LOAD
    offset = 0;
    install_apdu[offset++] = 0x80;
    install_apdu[offset++] = 0xE6;
    install_apdu[offset++] = 0x00;
    install_apdu[offset++] = 0x00;
    install_apdu[offset++] = 0x09;
    install_apdu[offset++] = 0x4F;
    install_apdu[offset++] = 0x07;
    memcpy(&install_apdu[offset], test_aid_2.aid, 7);
    offset += 7;
    
    resp_len = 0;
    sw = isd_handler_load(NULL, install_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "Second INSTALL FOR LOAD succeeded");
    
    // LOAD BLOCKS
    u8 sef_buffer_2[1024];
    u32 sef_size_2 = 0;
    create_test_sef(sef_buffer_2, &sef_size_2, &test_aid_2, 0x00020000);
    
    offset = 0;
    load_apdu[offset++] = 0x80;
    load_apdu[offset++] = 0xE8;
    load_apdu[offset++] = 0x01;
    load_apdu[offset++] = 0x00;
    load_apdu[offset++] = (u8)sef_size_2;
    memcpy(&load_apdu[offset], sef_buffer_2, sef_size_2);
    offset += sef_size_2;
    
    resp_len = 0;
    sw = isd_handler_load(NULL, load_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "Second LOAD BLOCKS succeeded");
    
    // FINALIZE
    offset = 0;
    finalize_apdu[offset++] = 0x80;
    finalize_apdu[offset++] = 0xE8;
    finalize_apdu[offset++] = 0x02;
    finalize_apdu[offset++] = 0x00;
    finalize_apdu[offset++] = 0x00;
    
    resp_len = 0;
    sw = isd_handler_load(NULL, finalize_apdu, offset, response, &resp_len);
    TEST_ASSERT(sw == 0x9000, "Second FINALIZE succeeded");
    
    // Verify two modules exist
    TEST_ASSERT(vm->module_count == 2, "Two modules loaded");
    
    GCOSModuleRegistry *reg2 = module_registry_find_by_aid(vm, &test_aid_2);
    TEST_ASSERT(reg2 != NULL, "Second module found");
    TEST_ASSERT(reg2->module_version == 0x00020000, "Second module version correct");
    
    printf("   Total modules: %u\n", vm->module_count);
    
    /* ========================================================================
     * Cleanup
     * ======================================================================== */
    printf("\n--- Cleanup ---\n");
    
    gcos_vm_destroy(vm);
    printf("✅ VM destroyed\n");
    
    printf("\n========================================\n");
    printf("All tests passed! ✅\n");
    printf("========================================\n");
    
    return 0;
}
