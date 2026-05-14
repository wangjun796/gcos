/**
 * @file test_generated_sef.c
 * @brief Test loading the generated COS3-compliant SEF file
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>  /* for malloc/free */
#include <string.h>
#include "gcos_vm.h"

int main(void) {
    GCOSVM *vm;
    GCOSResult ret;
    
    printf("========================================\n");
    printf("Test: Loading Generated COS3 SEF File\n");
    printf("========================================\n\n");
    
    /* Create VM */
    vm = gcos_vm_create();
    if (!vm) {
        printf("✗ FAILED: Cannot create VM\n");
        return 1;
    }
    printf("✓ VM created\n\n");
    
    /* Initialize VM */
    ret = gcos_vm_init(vm);
    if (ret != GCOS_SUCCESS) {
        printf("✗ FAILED: VM initialization failed (ret=%d)\n", ret);
        gcos_vm_destroy(vm);
        return 1;
    }
    printf("✓ VM initialized\n\n");
    
    /* Load SEF file */
    const char *sef_filename = "test_module.sef";
    FILE *fp = fopen(sef_filename, "rb");
    if (!fp) {
        printf("✗ FAILED: Cannot open %s\n", sef_filename);
        printf("  Make sure to run generate_test_sef.exe first!\n");
        gcos_vm_destroy(vm);
        return 1;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("File size: %ld bytes\n", file_size);
    
    if (file_size <= 0 || file_size > 1024 * 1024) {
        printf("✗ FAILED: Invalid file size %ld\n", file_size);
        fclose(fp);
        gcos_vm_destroy(vm);
        return 1;
    }
    
    uint8_t *sef_data = (uint8_t *)malloc((size_t)file_size);
    if (!sef_data) {
        printf("✗ FAILED: Cannot allocate memory for %ld bytes\n", file_size);
        fclose(fp);
        gcos_vm_destroy(vm);
        return 1;
    }
    
    size_t bytes_read = fread(sef_data, 1, (size_t)file_size, fp);
    fclose(fp);
    
    if ((long)bytes_read != file_size) {
        printf("✗ FAILED: Read %zu bytes, expected %ld\n", bytes_read, file_size);
        free(sef_data);
        gcos_vm_destroy(vm);
        return 1;
    }
    
    printf("Loaded SEF file: %ld bytes\n", file_size);
    printf("First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n\n",
           sef_data[0], sef_data[1], sef_data[2], sef_data[3],
           sef_data[4], sef_data[5], sef_data[6], sef_data[7]);
    
    /* Attempt to load the SEF file */
    printf("Calling gcos_loader_load_sef...\n");
    fflush(stdout);
    ret = gcos_loader_load_sef(vm, sef_data, (uint32_t)file_size);
    printf("gcos_loader_load_sef returned: %d\n\n", ret);
    
    free(sef_data);
    
    if (ret == GCOS_SUCCESS) {
        printf("\n✓ SUCCESS: SEF file loaded successfully!\n");
        printf("  Modules loaded: %u\n", vm->module_count);
        printf("  Apps loaded: %u\n", vm->app_count);
        
        if (vm->module_count > 0) {
            printf("\n  Module 0 Info:\n");
            printf("    Function count: %u\n", vm->modules[0].function_count);
            printf("    Import count: %u\n", vm->modules[0].import_count);
        }
    } else {
        printf("\n✗ FAILED: SEF file loading failed (ret=%d)\n", ret);
    }
    
    /* Cleanup */
    gcos_vm_destroy(vm);
    
    printf("\n========================================\n");
    printf("Test completed\n");
    printf("========================================\n");
    
    return (ret == GCOS_SUCCESS) ? 0 : 1;
}
