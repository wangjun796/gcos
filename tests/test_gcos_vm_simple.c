/**
 * @file test_gcos_vm_simple.c
 * @brief GCOS VM 简单测试 - 验证零动态内存分配和基础API
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

int main(void) {
    GCOS_PRINTF("========== GCOS VM Simple Test ==========\n\n");
    
    /* 测试1: 创建VM实例 */
    GCOS_PRINTF("[Test 1] Creating VM instance...\n");
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        GCOS_PRINTF("FAILED: Could not create VM\n");
        return 1;
    }
    GCOS_PRINTF("PASSED: VM created at address %p\n", (void*)vm);
    
    /* 测试2: 检查版本信息 */
    GCOS_PRINTF("\n[Test 2] Checking version info...\n");
    GCOS_PRINTF("Version: %d.%d.%d\n", vm->version.major, vm->version.minor, vm->version.patch);
    if (vm->version.major == 1 && vm->version.minor == 0 && vm->version.patch == 0) {
        GCOS_PRINTF("PASSED: Version is correct\n");
    } else {
        GCOS_PRINTF("FAILED: Version mismatch\n");
        return 1;
    }
    
    /* 测试3: 检查初始状态 */
    GCOS_PRINTF("\n[Test 3] Checking initial state...\n");
    GCOS_PRINTF("State: %s\n", gcos_vm_state_to_string(vm->state));
    if (vm->state == GCOS_STATE_IDLE) {
        GCOS_PRINTF("PASSED: Initial state is IDLE\n");
    } else {
        GCOS_PRINTF("FAILED: Initial state is not IDLE\n");
        return 1;
    }
    
    /* 测试4: 检查内存分区 */
    GCOS_PRINTF("\n[Test 4] Checking memory partitions...\n");
    GCOS_PRINTF("Executor Stack: %u / %u\n", vm->runtime.stack_pointer, GCOS_EXECUTOR_STACK_SIZE);
    GCOS_PRINTF("Indirect Stack: %u / %u\n", vm->runtime.indirect_stack_pointer, GCOS_INDIRECT_STACK_SIZE);
    GCOS_PRINTF("Global Data: %u / %u bytes\n", vm->runtime.global_data_used, GCOS_GLOBAL_DATA_SIZE);
    GCOS_PRINTF("Heap: %u / %u bytes\n", vm->runtime.heap_used, GCOS_HEAP_SIZE);
    GCOS_PRINTF("PASSED: Memory partitions initialized\n");
    
    /* 测试5: 栈操作 */
    GCOS_PRINTF("\n[Test 5] Testing stack operations...\n");
    GCOSResult result = gcos_vm_stack_push(vm, 42);
    if (result != GCOS_OK) {
        GCOS_PRINTF("FAILED: Stack push failed with error %d\n", result);
        return 1;
    }
    
    u32 value = 0;
    result = gcos_vm_stack_pop(vm, &value);
    if (result != GCOS_OK) {
        GCOS_PRINTF("FAILED: Stack pop failed with error %d\n", result);
        return 1;
    }
    
    if (value == 42) {
        GCOS_PRINTF("PASSED: Stack push/pop works correctly (value=%u)\n", value);
    } else {
        GCOS_PRINTF("FAILED: Stack value mismatch (expected 42, got %u)\n", value);
        return 1;
    }
    
    /* 测试6: 堆分配 */
    GCOS_PRINTF("\n[Test 6] Testing heap allocation...\n");
    u32 addr = gcos_vm_heap_alloc(vm, 100);
    if (addr == 0) {
        GCOS_PRINTF("FAILED: Heap allocation failed\n");
        return 1;
    }
    GCOS_PRINTF("PASSED: Heap allocated at offset %u\n", addr);
    
    /* 测试7: 打印VM信息 */
    GCOS_PRINTF("\n[Test 7] Printing VM info...\n");
    gcos_vm_print_info(vm);
    
    /* 测试8: 内存统计 */
    GCOS_PRINTF("\n[Test 8] Memory statistics...\n");
    gcos_memory_print_stats(vm);
    
    /* 测试9: 验证VM状态一致性 */
    GCOS_PRINTF("\n[Test 9] Validating VM consistency...\n");
    bool valid = gcos_vm_validate(vm);
    if (valid) {
        GCOS_PRINTF("PASSED: VM state is consistent\n");
    } else {
        GCOS_PRINTF("FAILED: VM state validation failed\n");
        return 1;
    }
    
    /* 测试10: 重置VM */
    GCOS_PRINTF("\n[Test 10] Resetting VM...\n");
    result = gcos_vm_reset(vm);
    if (result != GCOS_OK) {
        GCOS_PRINTF("FAILED: VM reset failed with error %d\n", result);
        return 1;
    }
    GCOS_PRINTF("PASSED: VM reset successful\n");
    
    /* 清理 */
    GCOS_PRINTF("\n[Cleanup] Destroying VM...\n");
    result = gcos_vm_destroy(vm);
    if (result != GCOS_OK) {
        GCOS_PRINTF("FAILED: VM destroy failed with error %d\n", result);
        return 1;
    }
    GCOS_PRINTF("PASSED: VM destroyed\n");
    
    GCOS_PRINTF("\n========== All Tests PASSED ==========\n");
    return 0;
}
