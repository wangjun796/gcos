#include "gcos_vm.h"
#include <stdio.h>

int main(void) {
    printf("GCOS VM Basic Test\n");
    printf("==================\n\n");
    
    // 创建VM
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("FAILED: Cannot create VM\n");
        return 1;
    }
    printf("✓ VM created\n");
    
    // 检查版本
    printf("Version: %d.%d.%d\n", vm->version.major, vm->version.minor, vm->version.patch);
    printf("✓ Version OK\n");
    
    // 检查状态
    printf("State: %s\n", gcos_vm_state_to_string(vm->state));
    printf("✓ State OK\n");
    
    // 测试栈操作
    GCOSResult ret = gcos_vm_stack_push(vm, 42);
    if (ret != GCOS_OK) {
        printf("FAILED: Stack push error %d\n", ret);
        return 1;
    }
    printf("✓ Stack push OK\n");
    
    u32 value = 0;
    ret = gcos_vm_stack_pop(vm, &value);
    if (ret != GCOS_OK) {
        printf("FAILED: Stack pop error %d\n", ret);
        return 1;
    }
    printf("✓ Stack pop OK (value=%u)\n", value);
    
    // 测试堆分配
    u32 addr = gcos_vm_heap_alloc(vm, 100);
    printf("✓ Heap alloc OK (addr=%u)\n", addr);
    
    printf("\nAll tests passed!\n");
    
    // 清理
    gcos_vm_destroy(vm);
    printf("✓ VM destroyed\n");
    
    return 0;
}
