/**
 * @file test_load_command.c
 * @brief Test LOAD command three-phase state machine
 */

#include "test_helpers.h"
#include "gcos_app_manager.h"

/* Helper function to print status word */
static void print_sw(u16 sw) {
    printf("<= %02X %02X", (sw >> 8) & 0xFF, sw & 0xFF);
    
    switch (sw) {
        case 0x9000: printf("  (Success)\n"); break;
        case 0x6A80: printf("  (Wrong data)\n"); break;
        case 0x6A84: printf("  (Memory failure)\n"); break;
        case 0x6A86: printf("  (Incorrect P1P2)\n"); break;
        case 0x6A88: printf("  (Referenced data not found)\n"); break;
        case 0x6A89: printf("  (File already exists)\n"); break;
        case 0x6985: printf("  (Conditions not satisfied)\n"); break;
        case 0x6F00: printf("  (No precise diagnosis)\n"); break;
        default:     printf("\n"); break;
    }
}

/* Test Phase 1: INSTALL FOR LOAD */
static void test_install_for_load(void) {
    printf("\n=== Test 1: INSTALL FOR LOAD ===\n");
    
    GCOSVM *vm = gcos_vm_get_instance();
    if (!vm) {
        printf("ERROR: Cannot get VM instance\n");
        return;
    }
    
    // Create a simple SEF-like package AID
    u8 install_apdu[] = {
        0x80, 0xE6, 0x00, 0x00,  // CLA INS P1 P2
        0x0A,                     // Lc = 10
        0x4F, 0x08,              // Tag 0x4F, Length 8
        0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x0C  // Package AID
    };
    
    u8 response[256];
    u16 resp_len = 0;
    
    printf("Sending INSTALL FOR LOAD...\n");
    u16 sw = isd_handler_load(NULL, install_apdu, sizeof(install_apdu), response, &resp_len);
    print_sw(sw);
    
    if (sw == 0x9000) {
        printf("✓ Load session initialized\n");
        printf("  State: %u (should be 1 = INITIALIZATION)\n", vm->load_context.state);
        printf("  Module ID: %u\n", vm->load_context.target_module_id);
        printf("  Package AID: ");
        for (int i = 0; i < vm->load_context.package_aid.length; i++) {
            printf("%02X", vm->load_context.package_aid.aid[i]);
        }
        printf("\n");
    } else {
        printf("✗ INSTALL FOR LOAD failed\n");
    }
}

/* Test Phase 2: LOAD BLOCKS */
static void test_load_blocks(void) {
    printf("\n=== Test 2: LOAD BLOCKS ===\n");
    
    GCOSVM *vm = gcos_vm_get_instance();
    
    // Create a minimal SEF file header
    u8 sef_header[] = {
        0x00, 0x73, 0x65, 0x66,  // Magic "sef"
        0x00, 0x00, 0x01, 0x00,  // Version 0x00000100
        0x00, 0x00, 0x00, 0x02,  // Section count = 2
        
        // Section 1: First section (ID=0x01)
        0x01,                     // Section ID
        0x00, 0x00, 0x00, 0x10,  // Size = 16 bytes
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Dummy data
        0x00, 0x00, 0x00, 0x00,
        
        // Section 2: Import section (ID=0x02)
        0x02,                     // Section ID
        0x00, 0x00, 0x00, 0x03,  // Size = 3 bytes
        0x00,                     // Import module count = 0
        0x00, 0x00                // Import function count = 0
    };
    
    // Split into blocks for testing
    u8 block1_apdu[] = {
        0x80, 0xE8, 0x01, 0x00,  // CLA INS P1 P2
        0x20                      // Lc = 32 (first half)
    };
    
    u8 block2_apdu[] = {
        0x80, 0xE8, 0x01, 0x01,  // CLA INS P1 P2
        0x0B                      // Lc = 11 (second half)
    };
    
    u8 response[256];
    u16 resp_len = 0;
    
    // Send first block
    printf("Sending Block 1 (%zu bytes)...\n", sizeof(block1_apdu) - 5 + 32);
    u8 full_block1[37];
    memcpy(full_block1, block1_apdu, 5);
    memcpy(&full_block1[5], sef_header, 32);
    full_block1[4] = 32;  // Update Lc
    
    u16 sw1 = isd_handler_load(NULL, full_block1, sizeof(full_block1), response, &resp_len);
    printf("Block 1: ");
    print_sw(sw1);
    
    // Send second block
    printf("Sending Block 2 (%zu bytes)...\n", sizeof(block2_apdu) - 5 + 11);
    u8 full_block2[16];
    memcpy(full_block2, block2_apdu, 5);
    memcpy(&full_block2[5], &sef_header[32], 11);
    full_block2[4] = 11;  // Update Lc
    
    u16 sw2 = isd_handler_load(NULL, full_block2, sizeof(full_block2), response, &resp_len);
    printf("Block 2: ");
    print_sw(sw2);
    
    if (sw1 == 0x9000 && sw2 == 0x9000) {
        printf("✓ All blocks loaded successfully\n");
        printf("  Buffer size: %u bytes\n", vm->load_context.buffer_size);
    } else {
        printf("✗ Block loading failed\n");
    }
}

/* Test Phase 3: FINALIZE */
static void test_finalize_load(void) {
    printf("\n=== Test 3: FINALIZE LOAD ===\n");
    
    GCOSVM *vm = gcos_vm_get_instance();
    
    u8 finalize_apdu[] = {
        0x80, 0xE8, 0x02, 0x00, 0x00  // CLA INS P1 P2 Lc=0
    };
    
    u8 response[256];
    u16 resp_len = 0;
    
    printf("Sending FINALIZE...\n");
    u16 sw = isd_handler_load(NULL, finalize_apdu, sizeof(finalize_apdu), response, &resp_len);
    print_sw(sw);
    
    if (sw == 0x9000) {
        printf("✓ Module created successfully\n");
        printf("  Module count: %u\n", vm->module_count);
        
        // Check the created module
        u8 module_id = vm->load_context.target_module_id;
        if (module_id < MAX_MODULES && vm->modules[module_id].loaded) {
            GCOSModule *module = &vm->modules[module_id];
            printf("  Module ID: %u\n", module->module_id);
            printf("  Module AID: ");
            for (int i = 0; i < module->module_aid.length; i++) {
                printf("%02X", module->module_aid.aid[i]);
            }
            printf("\n");
            printf("  Version: 0x%08X\n", module->version);
            printf("  State: %u\n", module->state);
            printf("  Imports: %u\n", module->import_count);
        }
    } else {
        printf("✗ FINALIZE failed\n");
    }
}

/* Test error cases */
static void test_error_cases(void) {
    printf("\n=== Test 4: Error Cases ===\n");
    
    GCOSVM *vm = gcos_vm_get_instance();
    u8 response[256];
    u16 resp_len = 0;
    
    // Test 1: Duplicate AID
    printf("\nTest 4.1: Duplicate Package AID\n");
    u8 dup_apdu[] = {
        0x80, 0xE6, 0x00, 0x00,
        0x0A,
        0x4F, 0x08,
        0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x0C  // Same AID as before
    };
    
    u16 sw = isd_handler_load(NULL, dup_apdu, sizeof(dup_apdu), response, &resp_len);
    printf("Expected: 0x6A89 (File already exists)\n");
    printf("Got:      ");
    print_sw(sw);
    
    // Test 2: Invalid P1
    printf("\nTest 4.2: Invalid P1 value\n");
    u8 invalid_p1[] = {
        0x80, 0xE8, 0x05, 0x00, 0x00  // P1=0x05 (invalid)
    };
    
    sw = isd_handler_load(NULL, invalid_p1, sizeof(invalid_p1), response, &resp_len);
    printf("Expected: 0x6A86 (Incorrect P1P2)\n");
    printf("Got:      ");
    print_sw(sw);
    
    // Test 3: No active session
    printf("\nTest 4.3: LOAD without active session\n");
    reset_load_context(vm);  // Reset context
    
    u8 no_session[] = {
        0x80, 0xE8, 0x01, 0x00, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05
    };
    
    sw = isd_handler_load(NULL, no_session, sizeof(no_session), response, &resp_len);
    printf("Expected: 0x6985 (Conditions not satisfied)\n");
    printf("Got:      ");
    print_sw(sw);
}

int main(void) {
    printf("========================================\n");
    printf("  GCOS LOAD Command Test\n");
    printf("========================================\n");
    
    // Initialize VM with eflash
    GCOSVM *vm = NULL;
    GCOSResult result = test_vm_create_and_init(&vm);
    if (!vm || result != GCOS_SUCCESS) {
        printf("ERROR: Failed to create and initialize VM\n");
        return 1;
    }
    
    printf("[TEST] VM initialized successfully\n");
    
    // Run tests
    test_install_for_load();
    test_load_blocks();
    test_finalize_load();
    test_error_cases();
    
    printf("\n========================================\n");
    printf("  All LOAD command tests completed!\n");
    printf("========================================\n");
    
    gcos_vm_destroy(vm);
    
    return 0;
}
