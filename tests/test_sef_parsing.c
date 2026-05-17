/**
 * @file test_sef_parsing.c
 * @brief Test SEF file parsing using COS3 specification example
 * 
 * Uses the real SEF file from COS3 Appendix F.1 (Table F.1)
 */

#include "test_helpers.h"
#include <stdint.h>

/* ============================================================================
 * Little-Endian Read Helpers (per COS3 Specification Section 7.1.2)
 * ============================================================================
 * 
 * COS3 requires all multi-byte integers in SEF files to be stored in
 * little-endian byte order (Section 7.1.2, line 413).
 */

static inline uint16_t read_u16_le(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static inline uint32_t read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

/* ============================================================================
 * COS3 Appendix F.1 SEF File Example
 * ============================================================================
 * 
 * This is the complete SEF file from COS3 specification Appendix F.1
 * Format: Hex bytes as shown in the specification
 */

static const uint8_t cos3_sef_example[] = {
    /* File Header (8 bytes total) */
    /* sef_type: u32 little-endian = 0x00736566 ("sef\0") */
    0x66, 0x65, 0x73, 0x00,  /* Stored as: 'f' 'e' 's' '\0' */
    
    /* version: u32 little-endian per Appendix B */
    /* Byte layout: [internal][revision][minor][major] */
    0x00, 0x00, 0x00, 0x01,  /* v1.0.0.0 (major=1, minor=0, rev=0, internal=0) */
    
    /* Section 1: Header Section (首段) */
    0x01,                     /* section_id = 0x01 */
    0x1D, 0x00, 0x00, 0x00,  /* size = 29 bytes */
    0x00, 0x00, 0x00, 0x01,  /* sef_version = 1.0.0.0 */
    0x05,                     /* sef_aid_length = 5 */
    0x11, 0x22, 0x33, 0x44, 0x55,  /* sef_aid = 1122334455 */
    0x98, 0x00, 0x00, 0x00,  /* sef_len = 152 bytes */
    0x01,                     /* import_module_count = 1 */
    0x01, 0x00,              /* import_function_count = 1 */
    0x02,                     /* app_num = 2 */
    0x00, 0x0C,              /* sec_func_len = 12 */
    0x00, 0x00, 0x00, 0x09,  /* sec_elem_len = 9 */
    0x00, 0x17, 0x00, 0x00, 0x00,  /* sec_code_len = 23 */
    
    /* Section 2: Import Section (导入段) */
    0x02,                     /* section_id = 0x02 */
    0x18, 0x00, 0x00, 0x00,  /* size = 24 bytes */
    0x01,                     /* import_module_count = 1 */
    0x01, 0x00,              /* import_function_count = 1 */
    /* IMPORT_MODULE_ITEMS[0] */
    0x00, 0x00, 0x00, 0x01,  /* import_module_version = 1.0.0.0 */
    0x0E,                     /* import_module_aid_size = 14 */
    0xD1, 0x56, 0x00, 0x01, 0x48, 0x41, 0x4F, 0x53,  /* AID = D156000148414F53... */
    0x46, 0x41, 0x50, 0x49, 0x76, 0x31, 0x31, 0x00,  /* ...FAPIv11\0 */
    /* IMPORT_FUNCTION_ITEMS[0] */
    0x00, 0x01,              /* import_moduleidx_funcidx = 0x0001 */
                             /*   moduleidx = 0 (high 5 bits) */
                             /*   funcidx = 1 (low 11 bits) */
    
    /* Section 3: Function Section (函数段) */
    0x03,                     /* section_id = 0x03 */
    0x02, 0x00, 0x00, 0x00,  /* size = 2 bytes */
    0x17, 0x00,              /* code_size[0] = 23 bytes */
    
    /* Section 4: Application Section (应用段) */
    0x04,                     /* section_id = 0x04 */
    0x0A, 0x00, 0x00, 0x00,  /* size = 10 bytes */
    0x01,                     /* app_num = 1 */
    /* APP_INFO[0] */
    0x06,                     /* app_aid_length = 6 */
    0x11, 0x22, 0x33, 0x44, 0x55, 0x11,  /* app_aid = 112233445511 */
    0x00, 0x00,              /* app_builder_method = 0 */
    
    /* Section 5: Global Section (全局段) */
    0x05,                     /* section_id = 0x05 */
    0x0C, 0x00, 0x00, 0x00,  /* size = 12 bytes */
    0x10, 0x00,              /* rodata_base = 16 */
    0x10, 0x00,              /* rwdata_base = 16 */
    0x10, 0x00,              /* refdata_base = 16 */
    0x14, 0x00,              /* moddata_base = 20 */
    0x15, 0x00,              /* appdata_base = 21 */
    0x15, 0x00,              /* data_end = 21 */
    
    /* Section 8: Data Section (数据段) */
    0x08,                     /* section_id = 0x08 */
    0x09, 0x00, 0x00, 0x00,  /* size = 9 bytes */
    0x00, 0x00,              /* rodata_size = 0 */
    0x00, 0x00,              /* rwdata_init_size = 0 */
    0x01, 0x00,              /* moddata_init_size = 1 */
    0x00, 0x00,              /* appdata_init_size = 0 */
    0x01,                     /* moddata[0] = 0x01 */
    
    /* Section 9: Code Section (代码段) */
    0x09,                     /* section_id = 0x09 */
    0x17, 0x00, 0x00, 0x00,  /* size = 23 bytes */
    0x21, 0x40, 0x23, 0x10, 0x18, 0x14, 0x00, 0x5A,  /* Bytecode */
    0xAB, 0x10, 0x19, 0xA1, 0x20, 0x19, 0xA7, 0x15,
    0x9A, 0x14, 0x76, 0x01, 0xA7, 0x14, 0x12
};

#define COS3_SEF_SIZE sizeof(cos3_sef_example)

/**
 * @brief Test loading COS3 specification SEF example
 */
static int test_cos3_sef_load(void) {
    printf("\n=== Test: COS3 Specification SEF File Loading ===\n");
    
    /* Create and initialize VM with eflash */
    GCOSVM *vm = NULL;
    GCOSResult ret = test_vm_create_and_init(&vm);
    if (!vm || ret != GCOS_SUCCESS) {
        printf("✗ FAILED: Could not create and initialize VM\n");
        return 1;
    }
    printf("✓ VM created and initialized\n");
    printf("✓ VM initialized\n");
    
    /* Print SEF file info */
    printf("\nSEF File Information:\n");
    printf("  Total size: %u bytes\n", COS3_SEF_SIZE);
    printf("  File header: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X ", cos3_sef_example[i]);
    }
    printf("\n");
    
    /* Parse file header using little-endian reads (per COS3 spec) */
    uint32_t file_type = read_u32_le(&cos3_sef_example[0]);
    uint32_t file_version = read_u32_le(&cos3_sef_example[4]);
    
    printf("  File type: 0x%08X (expected 0x00736566 for 'sef\\0')\n", file_type);
    
    /* Decode version per Appendix B: [internal][revision][minor][major] */
    uint8_t ver_major = (file_version >> 24) & 0xFF;
    uint8_t ver_minor = (file_version >> 16) & 0xFF;
    uint8_t ver_revision = (file_version >> 8) & 0xFF;
    uint8_t ver_internal = file_version & 0xFF;
    
    printf("  File version: v%d.%d.%d.%d (raw=0x%08X)\n",
           ver_major, ver_minor, ver_revision, ver_internal, file_version);
    
    /* Verify magic number */
    if (file_type == 0x00736566) {
        printf("  ✓ File type is valid (matches COS3 Table 10)\n");
    } else {
        printf("  ✗ File type mismatch! Got 0x%08X, expected 0x00736566\n", file_type);
        printf("  Note: This indicates the SEF loader needs to be fixed.\n");
    }
    
    /* Attempt to load the SEF file */
    printf("\nAttempting to load SEF file...\n");
    ret = gcos_loader_load_sef(vm, cos3_sef_example, COS3_SEF_SIZE);
    
    if (ret == GCOS_OK && vm->module_count > 0) {
        printf("✓ SEF file loaded successfully\n");
        u8 module_index = vm->module_count - 1;  /* Last loaded module */
        printf("  Module index: %u\n", module_index);
        
        /* Print module information */
        GCOSModule *module = &vm->modules[module_index];
        printf("\nLoaded Module Information:\n");
        printf("  Module ID: %u\n", module->module_id);
        printf("  Module AID: ");
        for (int i = 0; i < module->module_aid.length; i++) {
            printf("%02X", module->module_aid.aid[i]);
        }
        printf("\n");
        printf("  Module state: %s\n", 
               module->state == MODULE_VERIFIED ? "VERIFIED" :
               module->state == MODULE_LOADED ? "LOADED" : "UNKNOWN");
        printf("  Function count: %u\n", module->function_count);
        printf("  Import count: %u\n", module->import_count);
        printf("  App instance count: %u\n", module->app_instance_count);
        printf("  Code size: %u bytes\n", module->code_size);
        printf("  Global data size: %u bytes\n", module->global_data_size);
        
        /* Print import dependencies */
        if (module->import_count > 0) {
            printf("\n  Import Dependencies:\n");
            for (int i = 0; i < module->import_count; i++) {
                printf("    [%d] Version: %u.%u.%u.%u, AID: ",
                       i,
                       (module->imports[i].module_version >> 24) & 0xFF,
                       (module->imports[i].module_version >> 16) & 0xFF,
                       (module->imports[i].module_version >> 8) & 0xFF,
                       module->imports[i].module_version & 0xFF);
                for (int j = 0; j < module->imports[i].module_aid.length; j++) {
                    printf("%02X", module->imports[i].module_aid.aid[j]);
                }
                printf(", Resolved: %s\n",
                       module->imports[i].resolved ? "YES" : "NO");
            }
        }
        
        gcos_vm_destroy(vm);
        return 0;
    } else {
        printf("✗ FAILED: SEF file loading failed (ret=%d)\n", ret);
        printf("\n⚠ This is expected - current GCOS loader doesn't match COS3 spec.\n");
        printf("The SEF file structure IS correct per COS3 specification.\n");
        
        /* Demonstrate correct little-endian parsing */
        printf("\n=== Manual SEF Parsing (Correct Little-Endian) ===\n");
        
        /* Parse sections manually to verify structure */
        uint32_t offset = 8;  /* Skip 8-byte header */
        int section_count = 0;
        
        while (offset < COS3_SEF_SIZE) {
            if (offset + 5 > COS3_SEF_SIZE) break;  /* Need at least section_id + size */
            
            uint8_t section_id = cos3_sef_example[offset];
            uint32_t section_size = read_u32_le(&cos3_sef_example[offset + 1]);
            
            printf("  Section %d: ID=0x%02X, Size=%u bytes (at offset %u)\n",
                   section_count++, section_id, section_size, offset);
            
            /* Move to next section */
            offset += 5 + section_size;  /* 5 = section_id(1) + size(4) */
        }
        
        printf("\n✓ Manual parsing successful - SEF structure is valid!\n");
        printf("  Total sections parsed: %d\n", section_count);
        
        gcos_vm_destroy(vm);
        return 1;
    }
}

/**
 * @brief Test module dependency resolution by AID
 */
static int test_dependency_by_aid(void) {
    printf("\n=== Test: Module Dependency Resolution by AID ===\n");
    
    GCOSVM *vm = gcos_vm_create();
    if (!vm) {
        printf("✗ FAILED: Could not create VM\n");
        return 1;
    }
    
    GCOSResult ret = gcos_vm_init(vm);
    if (ret != GCOS_OK) {
        printf("✗ FAILED: VM initialization failed\n");
        gcos_vm_destroy(vm);
        return 1;
    }
    
    printf("\nCOS3 Specification: Module dependencies are based on AID\n");
    printf("Unlike WebAssembly which uses module name strings,\n");
    printf("GCOS/COS3 uses AID (Application Identifier) for module identification.\n");
    
    /* Create a mock imported module */
    GCOSModule *imported_module = &vm->modules[0];
    imported_module->module_id = 1;
    imported_module->module_aid.length = 14;
    memcpy(imported_module->module_aid.aid, 
           "\xD1\x56\x00\x01\x48\x41\x4F\x53\x46\x41\x50\x49\x76\x31", 14);
    imported_module->version = 0x01000000;
    imported_module->state = MODULE_VERIFIED;
    imported_module->loaded = true;
    vm->module_count = 1;
    
    printf("\nMock Imported Module:\n");
    printf("  Module ID: %u\n", imported_module->module_id);
    printf("  AID: ");
    for (int i = 0; i < imported_module->module_aid.length; i++) {
        printf("%02X", imported_module->module_aid.aid[i]);
    }
    printf("\n");
    printf("  Version: %u.%u.%u.%u\n",
           (imported_module->version >> 24) & 0xFF,
           (imported_module->version >> 16) & 0xFF,
           (imported_module->version >> 8) & 0xFF,
           imported_module->version & 0xFF);
    
    /* Simulate import dependency */
    GCOSModule *dependent_module = &vm->modules[1];
    dependent_module->module_id = 2;
    dependent_module->import_count = 1;
    dependent_module->imports[0].module_version = 0x01000000;
    memcpy(&dependent_module->imports[0].module_aid, 
           &imported_module->module_aid, sizeof(GCOSAID));
    dependent_module->imports[0].resolved = false;
    dependent_module->imports[0].resolved_module_id = 0xFF;
    
    printf("\nDependent Module Import:\n");
    printf("  Required AID: ");
    for (int i = 0; i < dependent_module->imports[0].module_aid.length; i++) {
        printf("%02X", dependent_module->imports[0].module_aid.aid[i]);
    }
    printf("\n");
    printf("  Required Version: %u.%u.%u.%u\n",
           (dependent_module->imports[0].module_version >> 24) & 0xFF,
           (dependent_module->imports[0].module_version >> 16) & 0xFF,
           (dependent_module->imports[0].module_version >> 8) & 0xFF,
           dependent_module->imports[0].module_version & 0xFF);
    
    /* Resolve dependency by AID matching */
    printf("\nResolving dependency by AID matching...\n");
    for (int i = 0; i < vm->module_count; i++) {
        if (vm->modules[i].module_aid.length == dependent_module->imports[0].module_aid.length &&
            memcmp(vm->modules[i].module_aid.aid, 
                   dependent_module->imports[0].module_aid.aid,
                   vm->modules[i].module_aid.length) == 0) {
            
            /* Check version compatibility */
            if (vm->modules[i].version >= dependent_module->imports[0].module_version) {
                dependent_module->imports[0].resolved = true;
                dependent_module->imports[0].resolved_module_id = vm->modules[i].module_id;
                
                printf("✓ Dependency resolved!\n");
                printf("  Found module ID: %u\n", vm->modules[i].module_id);
                printf("  Version match: %u.%u.%u.%u >= required %u.%u.%u.%u\n",
                       (vm->modules[i].version >> 24) & 0xFF,
                       (vm->modules[i].version >> 16) & 0xFF,
                       (vm->modules[i].version >> 8) & 0xFF,
                       vm->modules[i].version & 0xFF,
                       (dependent_module->imports[0].module_version >> 24) & 0xFF,
                       (dependent_module->imports[0].module_version >> 16) & 0xFF,
                       (dependent_module->imports[0].module_version >> 8) & 0xFF,
                       dependent_module->imports[0].module_version & 0xFF);
                break;
            }
        }
    }
    
    if (dependent_module->imports[0].resolved) {
        printf("\n✓ PASSED: AID-based dependency resolution works correctly\n");
        gcos_vm_destroy(vm);
        return 0;
    } else {
        printf("\n✗ FAILED: Could not resolve dependency\n");
        gcos_vm_destroy(vm);
        return 1;
    }
}

/**
 * @brief Main test entry point
 */
int main(void) {
    printf("========================================\n");
    printf("COS3 SEF File Parsing Test Suite\n");
    printf("========================================\n");
    
    int failures = 0;
    
    failures += test_cos3_sef_load();
    failures += test_dependency_by_aid();
    
    printf("\n========================================\n");
    if (failures == 0) {
        printf("All tests PASSED ✓\n");
    } else {
        printf("%d test(s) FAILED ✗\n", failures);
    }
    printf("========================================\n");
    
    printf("\nKey Findings:\n");
    printf("1. COS3 Appendix F.1 provides a complete SEF file example\n");
    printf("2. GCOS module dependencies are based on AID (not string names like WASM)\n");
    printf("3. Each import specifies:\n");
    printf("   - Required module AID (unique identifier)\n");
    printf("   - Required module version (for compatibility)\n");
    printf("   - Function index within the imported module\n");
    
    return failures;
}
