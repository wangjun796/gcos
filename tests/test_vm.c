#include "vm_core.h"
#include "vm_memory.h"
#include "vm_instructions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 测试用例
 * ============================================================================ */

static void test_vm_create_destroy(void) {
    printf("\n=== Test: VM Create/Destroy ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    printf("PASS: VM created successfully\n");
    
    vm_print_info(vm);
    
    vm_destroy(vm);
    printf("PASS: VM destroyed successfully\n");
}

static void test_stack_operations(void) {
    printf("\n=== Test: Stack Operations ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    /* 测试压栈 */
    for (u32 i = 0; i < 10; i++) {
        int ret = vm_stack_push(vm, i * 100);
        if (ret != 0) {
            printf("FAIL: Stack push failed at %u\n", i);
            vm_destroy(vm);
            return;
        }
    }
    
    printf("PASS: Pushed 10 values to stack\n");
    printf("Stack depth: %u\n", vm_stack_depth(vm));
    
    /* 测试查看栈顶 */
    u32 top_value;
    int ret = vm_stack_peek(vm, &top_value);
    if (ret != 0 || top_value != 900) {
        printf("FAIL: Stack peek failed\n");
        vm_destroy(vm);
        return;
    }
    printf("PASS: Stack peek = %u\n", top_value);
    
    /* 测试弹栈 */
    for (int i = 9; i >= 0; i--) {
        u32 value;
        ret = vm_stack_pop(vm, &value);
        if (ret != 0 || value != (u32)(i * 100)) {
            printf("FAIL: Stack pop failed at %d\n", i);
            vm_destroy(vm);
            return;
        }
    }
    
    printf("PASS: Popped 10 values from stack\n");
    printf("Stack depth: %u\n", vm_stack_depth(vm));
    
    vm_destroy(vm);
    printf("PASS: All stack operations successful\n");
}

static void test_arithmetic_instructions(void) {
    printf("\n=== Test: Arithmetic Instructions ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    /* 测试加法: 5 + 3 = 8 */
    vm_stack_push(vm, 5);
    vm_stack_push(vm, 3);
    int ret = vm_execute_instruction(vm, OP_ADD, NULL, 0);
    if (ret != 0) {
        printf("FAIL: ADD instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    u32 result;
    vm_stack_pop(vm, &result);
    if (result != 8) {
        printf("FAIL: ADD result incorrect: %u (expected 8)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 5 + 3 = %u\n", result);
    
    /* 测试减法: 10 - 4 = 6 */
    vm_stack_push(vm, 10);
    vm_stack_push(vm, 4);
    ret = vm_execute_instruction(vm, OP_SUB, NULL, 0);
    if (ret != 0) {
        printf("FAIL: SUB instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 6) {
        printf("FAIL: SUB result incorrect: %u (expected 6)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 10 - 4 = %u\n", result);
    
    /* 测试乘法: 7 * 6 = 42 */
    vm_stack_push(vm, 7);
    vm_stack_push(vm, 6);
    ret = vm_execute_instruction(vm, OP_MUL, NULL, 0);
    if (ret != 0) {
        printf("FAIL: MUL instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 42) {
        printf("FAIL: MUL result incorrect: %u (expected 42)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 7 * 6 = %u\n", result);
    
    /* 测试除法: 20 / 4 = 5 */
    vm_stack_push(vm, 20);
    vm_stack_push(vm, 4);
    ret = vm_execute_instruction(vm, OP_DIVU, NULL, 0);
    if (ret != 0) {
        printf("FAIL: DIVU instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 5) {
        printf("FAIL: DIVU result incorrect: %u (expected 5)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 20 / 4 = %u\n", result);
    
    vm_destroy(vm);
    printf("PASS: All arithmetic instructions successful\n");
}

static void test_logic_instructions(void) {
    printf("\n=== Test: Logic Instructions ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    /* 测试AND: 0xFF & 0x0F = 0x0F */
    vm_stack_push(vm, 0xFF);
    vm_stack_push(vm, 0x0F);
    int ret = vm_execute_instruction(vm, OP_AND, NULL, 0);
    if (ret != 0) {
        printf("FAIL: AND instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    u32 result;
    vm_stack_pop(vm, &result);
    if (result != 0x0F) {
        printf("FAIL: AND result incorrect: 0x%X (expected 0x0F)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 0xFF & 0x0F = 0x%X\n", result);
    
    /* 测试OR: 0xF0 | 0x0F = 0xFF */
    vm_stack_push(vm, 0xF0);
    vm_stack_push(vm, 0x0F);
    ret = vm_execute_instruction(vm, OP_OR, NULL, 0);
    if (ret != 0) {
        printf("FAIL: OR instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 0xFF) {
        printf("FAIL: OR result incorrect: 0x%X (expected 0xFF)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 0xF0 | 0x0F = 0x%X\n", result);
    
    /* 测试XOR: 0xFF ^ 0xAA = 0x55 */
    vm_stack_push(vm, 0xFF);
    vm_stack_push(vm, 0xAA);
    ret = vm_execute_instruction(vm, OP_XOR, NULL, 0);
    if (ret != 0) {
        printf("FAIL: XOR instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 0x55) {
        printf("FAIL: XOR result incorrect: 0x%X (expected 0x55)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 0xFF ^ 0xAA = 0x%X\n", result);
    
    vm_destroy(vm);
    printf("PASS: All logic instructions successful\n");
}

static void test_comparison_instructions(void) {
    printf("\n=== Test: Comparison Instructions ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    /* 测试EQ: 5 == 5 -> 1 */
    vm_stack_push(vm, 5);
    vm_stack_push(vm, 5);
    int ret = vm_execute_instruction(vm, OP_EQ, NULL, 0);
    if (ret != 0) {
        printf("FAIL: EQ instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    u32 result;
    vm_stack_pop(vm, &result);
    if (result != 1) {
        printf("FAIL: EQ result incorrect: %u (expected 1)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 5 == 5 -> %u\n", result);
    
    /* 测试NE: 5 != 3 -> 1 */
    vm_stack_push(vm, 5);
    vm_stack_push(vm, 3);
    ret = vm_execute_instruction(vm, OP_NE, NULL, 0);
    if (ret != 0) {
        printf("FAIL: NE instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 1) {
        printf("FAIL: NE result incorrect: %u (expected 1)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 5 != 3 -> %u\n", result);
    
    /* 测试LT: 3 < 5 -> 1 */
    vm_stack_push(vm, 3);
    vm_stack_push(vm, 5);
    ret = vm_execute_instruction(vm, OP_LTS, NULL, 0);
    if (ret != 0) {
        printf("FAIL: LTS instruction failed\n");
        vm_destroy(vm);
        return;
    }
    
    vm_stack_pop(vm, &result);
    if (result != 1) {
        printf("FAIL: LTS result incorrect: %u (expected 1)\n", result);
        vm_destroy(vm);
        return;
    }
    printf("PASS: 3 < 5 -> %u\n", result);
    
    vm_destroy(vm);
    printf("PASS: All comparison instructions successful\n");
}

static void test_heap_allocation(void) {
    printf("\n=== Test: Heap Allocation ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    /* 分配内存 */
    void *ptr1 = vm_heap_alloc(vm, 64);
    if (ptr1 == NULL) {
        printf("FAIL: Heap allocation failed\n");
        vm_destroy(vm);
        return;
    }
    printf("PASS: Allocated 64 bytes at %p\n", ptr1);
    
    /* 写入数据 */
    memset(ptr1, 0xAA, 64);
    
    /* 再分配一块 */
    void *ptr2 = vm_heap_alloc(vm, 128);
    if (ptr2 == NULL) {
        printf("FAIL: Second heap allocation failed\n");
        vm_destroy(vm);
        return;
    }
    printf("PASS: Allocated 128 bytes at %p\n", ptr2);
    
    /* 检查堆使用情况 */
    u32 used, free_space;
    vm_heap_stats(vm, &used, &free_space);
    printf("Heap usage: used=%u, free=%u\n", used, free_space);
    
    /* 释放内存 */
    vm_heap_free(vm, ptr1);
    vm_heap_free(vm, ptr2);
    
    vm_destroy(vm);
    printf("PASS: Heap operations successful\n");
}

static void test_transaction(void) {
    printf("\n=== Test: Transaction Management ===\n");
    
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("FAIL: Failed to create VM\n");
        return;
    }
    
    /* 开始事务 */
    int ret = vm_transaction_begin(vm);
    if (ret != 0) {
        printf("FAIL: Transaction begin failed\n");
        vm_destroy(vm);
        return;
    }
    printf("PASS: Transaction started\n");
    
    /* 修改一些数据 */
    vm_stack_push(vm, 100);
    vm_stack_push(vm, 200);
    
    /* 回滚事务 */
    ret = vm_transaction_rollback(vm);
    if (ret != 0) {
        printf("FAIL: Transaction rollback failed\n");
        vm_destroy(vm);
        return;
    }
    printf("PASS: Transaction rolled back\n");
    
    /* 再次开始事务 */
    ret = vm_transaction_begin(vm);
    if (ret != 0) {
        printf("FAIL: Second transaction begin failed\n");
        vm_destroy(vm);
        return;
    }
    
    /* 修改数据 */
    vm_stack_push(vm, 300);
    
    /* 提交事务 */
    ret = vm_transaction_commit(vm);
    if (ret != 0) {
        printf("FAIL: Transaction commit failed\n");
        vm_destroy(vm);
        return;
    }
    printf("PASS: Transaction committed\n");
    
    vm_destroy(vm);
    printf("PASS: Transaction management successful\n");
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(void) {
    printf("========================================\n");
    printf("GCOS VM Test Suite\n");
    printf("========================================\n");
    
    test_vm_create_destroy();
    test_stack_operations();
    test_arithmetic_instructions();
    test_logic_instructions();
    test_comparison_instructions();
    test_heap_allocation();
    test_transaction();
    
    printf("\n========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n");
    
    return 0;
}
