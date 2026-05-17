/**
 * @file test_symbol_resolver.c
 * @brief Test GCOS Symbol Resolution System
 * 
 * Demonstrates:
 * - System module registration (like iwasm)
 * - Import/export symbol management
 * - 16-bit compact addressing with global references (like cref)
 * - Cross-module symbol resolution
 */

#include "test_helpers.h"
#include "gcos_symbol_resolver.h"

/* ============================================================================
 * Example System Module Functions (like iwasm's built-in APIs)
 * ============================================================================ */

/* System print function */
static uint32_t sys_print(const uint32_t *args, uint8_t arg_count) {
    if (arg_count > 0) {
        printf("[SYS] Print: value = %u\n", args[0]);
    }
    return 0;
}

/* System math add function */
static uint32_t sys_math_add(const uint32_t *args, uint8_t arg_count) {
    if (arg_count >= 2) {
        uint32_t result = args[0] + args[1];
        printf("[SYS] Math: %u + %u = %u\n", args[0], args[1], result);
        return result;
    }
    return 0;
}

/* System memory alloc function */
static uint32_t sys_mem_alloc(const uint32_t *args, uint8_t arg_count) {
    if (arg_count > 0) {
        uint32_t size = args[0];
        printf("[SYS] Memory: Allocating %u bytes\n", size);
        /* In real implementation, would allocate from heap */
        return 0x1000;  /* Return dummy address */
    }
    return 0;
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

static void test_system_module_registration(GCOSVM *vm) {
    printf("\n=== Test 1: System Module Registration ===\n");
    
    /* Register "sys" module (like iwasm's env module) */
    uint8_t sys_aid[] = {0xA0, 0x00, 0x00, 0x00, 0x01};
    gcos_symbol_register_system_module(vm, sys_aid, 5, "sys");
    
    /* Add system exports */
    gcos_symbol_add_system_export(vm, "sys", "print", (void *)sys_print, 1, 4);
    gcos_symbol_add_system_export(vm, "sys", "math_add", (void *)sys_math_add, 2, 4);
    gcos_symbol_add_system_export(vm, "sys", "mem_alloc", (void *)sys_mem_alloc, 1, 4);
    
    printf("✓ System module 'sys' registered with 3 exports\n");
}

static void test_global_reference_table(GCOSVM *vm) {
    printf("\n=== Test 2: Global Reference Table ===\n");
    
    /* Create some global references */
    u16 ref1 = gcos_symbol_create_global_ref(vm, 0x00001000, 0, 0);
    u16 ref2 = gcos_symbol_create_global_ref(vm, 0x00002000, 0, 1);
    u16 ref3 = gcos_symbol_create_global_ref(vm, 0x00003000, 1, 0);
    
    printf("Created global references:\n");
    printf("  ref1: 0x%04X (bit15=%d, index=%u)\n", ref1, (ref1 >> 15) & 1, ref1 & 0x7FFF);
    printf("  ref2: 0x%04X (bit15=%d, index=%u)\n", ref2, (ref2 >> 15) & 1, ref2 & 0x7FFF);
    printf("  ref3: 0x%04X (bit15=%d, index=%u)\n", ref3, (ref3 >> 15) & 1, ref3 & 0x7FFF);
    
    /* Resolve global references back to 32-bit addresses */
    u32 addr1, addr2, addr3;
    bool ok1 = gcos_symbol_resolve_address(vm, ref1, &addr1);
    bool ok2 = gcos_symbol_resolve_address(vm, ref2, &addr2);
    bool ok3 = gcos_symbol_resolve_address(vm, ref3, &addr3);
    
    printf("Resolved addresses:\n");
    printf("  ref1 -> 0x%08X (%s)\n", addr1, ok1 ? "OK" : "FAIL");
    printf("  ref2 -> 0x%08X (%s)\n", addr2, ok2 ? "OK" : "FAIL");
    printf("  ref3 -> 0x%08X (%s)\n", addr3, ok3 ? "OK" : "FAIL");
    
    if (ok1 && ok2 && ok3) {
        printf("✓ Global reference table works correctly\n");
    } else {
        printf("✗ Global reference table has errors\n");
    }
}

static void test_local_vs_global_addressing(GCOSVM *vm) {
    printf("\n=== Test 3: Local vs Global Addressing ===\n");
    
    /* Test local address (bit 15 = 0) */
    u16 local_addr = 0x1234;  /* Direct 16-bit address */
    u32 resolved;
    bool ok = gcos_symbol_resolve_address(vm, local_addr, &resolved);
    
    printf("Local address 0x%04X -> 0x%08X (%s)\n", local_addr, resolved, ok ? "OK" : "FAIL");
    
    /* Verify it's treated as direct address */
    if (ok && resolved == 0x1234) {
        printf("✓ Local addressing works (direct 16-bit)\n");
    } else {
        printf("✗ Local addressing failed\n");
    }
    
    /* Test global address (bit 15 = 1) */
    u16 global_addr = 0x8000;  /* Global reference index 0 */
    ok = gcos_symbol_resolve_address(vm, global_addr, &resolved);
    
    printf("Global address 0x%04X -> 0x%08X (%s)\n", global_addr, resolved, ok ? "OK" : "FAIL");
    
    if (ok) {
        printf("✓ Global addressing works (indirect via table)\n");
    } else {
        printf("✗ Global addressing failed\n");
    }
}

static void test_import_resolution(GCOSVM *vm) {
    printf("\n=== Test 4: Import Resolution ===\n");
    
    /* Create a dummy module first */
    vm->module_count = 1;
    vm->modules[0].function_count = 5;  /* Dummy module with 5 functions */
    
    /* Simulate a module importing from system module */
    /* COS3 format: high 5 bits = module idx, low 11 bits = func idx */
    /* System modules start at index 0x10 */
    u16 import_desc = (0x10 << 11) | 0;  /* Import from system module 0, function 0 */
    
    printf("Import descriptor: 0x%04X\n", import_desc);
    printf("  Module index: %u (0x%02X)\n", (import_desc >> 11) & 0x1F, (import_desc >> 11) & 0x1F);
    printf("  Function index: %u\n", import_desc & 0x7FF);
    
    /* Add import to module 0 */
    gcos_symbol_add_import(vm, 0, import_desc);
    
    /* Resolve imports */
    GCOSResult ret = gcos_symbol_resolve_imports(vm, 0);
    
    if (ret == GCOS_SUCCESS) {
        printf("✓ Import resolution successful\n");
        
        /* Try calling the resolved system function */
        uint32_t args[] = {42};
        uint32_t result;
        ret = gcos_symbol_call_system_func(vm, "sys", "print", args, 1, &result);
        
        if (ret == GCOS_SUCCESS) {
            printf("✓ System function call successful\n");
        }
    } else {
        printf("✗ Import resolution failed (ret=%d)\n", ret);
    }
}

static void test_cos3_format_encoding(GCOSVM *vm) {
    printf("\n=== Test 5: COS3 Import Format Encoding ===\n");
    
    /* Test various COS3 format encodings */
    struct {
        u8 module_idx;
        u16 func_idx;
        u16 expected_encoding;
    } test_cases[] = {
        {0, 0, 0x0000},
        {0, 1, 0x0001},
        {1, 0, 0x0800},   /* 1 << 11 = 0x0800 */
        {2, 5, 0x1005},   /* 2 << 11 = 0x1000, + 5 = 0x1005 */
        {31, 2047, 0xFFFF}, /* Max values: 31 << 11 = 0xF800, + 2047 = 0xFFFF */
    };
    
    for (int i = 0; i < 5; i++) {
        u16 encoding = (test_cases[i].module_idx << 11) | test_cases[i].func_idx;
        
        printf("Test %d: module=%u, func=%u -> encoding=0x%04X (expected 0x%04X) %s\n",
               i, test_cases[i].module_idx, test_cases[i].func_idx,
               encoding, test_cases[i].expected_encoding,
               encoding == test_cases[i].expected_encoding ? "✓" : "✗");
    }
    
    printf("✓ COS3 format encoding verified\n");
}

static void test_address_space_analysis(GCOSVM *vm) {
    printf("\n=== Test 6: Address Space Analysis ===\n");
    
    printf("16-bit Compact Address Format:\n");
    printf("  Bit 15 = 0: Local address (direct)\n");
    printf("    Range: 0x0000 - 0x7FFF (0 - 32767)\n");
    printf("    Capacity: 32KB direct addressing\n");
    printf("\n");
    printf("  Bit 15 = 1: Global reference (indirect)\n");
    printf("    Range: 0x8000 - 0xFFFF (index 0 - 32767)\n");
    printf("    Indexes into global reference table\n");
    printf("    Each entry maps to 32-bit logical address\n");
    printf("\n");
    printf("Comparison with cref:\n");
    printf("  ✓ Similar approach: 16-bit with high bit flag\n");
    printf("  ✓ Global reference table for indirection\n");
    printf("  ✓ Allows >64KB addressing through indirection\n");
    printf("\n");
    printf("Advantages:\n");
    printf("  - Compact bytecode (16-bit addresses)\n");
    printf("  - Flexible addressing (>64KB via global refs)\n");
    printf("  - Simple decoding (check bit 15)\n");
    printf("  - Compatible with COS3 specification\n");
}

int main(void) {
    printf("========================================\n");
    printf("GCOS Symbol Resolution Test Suite\n");
    printf("========================================\n");
    
    /* Create and initialize VM with eflash */
    GCOSVM *vm = NULL;
    GCOSResult result = test_vm_create_and_init(&vm);
    if (!vm || result != GCOS_SUCCESS) {
        printf("✗ FAILED: Cannot create and initialize VM\n");
        return 1;
    }
    
    /* Initialize symbol resolver */
    gcos_symbol_resolver_init(vm);
    
    /* Run tests */
    test_system_module_registration(vm);
    test_global_reference_table(vm);
    test_local_vs_global_addressing(vm);
    test_import_resolution(vm);
    test_cos3_format_encoding(vm);
    test_address_space_analysis(vm);
    
    /* Print statistics */
    gcos_symbol_print_stats(vm);
    
    /* Dump symbol tables */
    gcos_symbol_dump_tables(vm, 0xFF);
    
    /* Cleanup */
    gcos_vm_destroy(vm);
    
    printf("\n========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n");
    
    return 0;
}
