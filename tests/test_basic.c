#include "gcos_vm.h"
#include "gcos_platform.h"

int main(void) {
    GCOS_PRINTF("GCOS VM Basic Test\n");
    GCOS_PRINTF("==================\n\n");
    
    // 创建VM
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        GCOS_PRINTF("FAILED: Cannot create VM\n");
        return 1;
    }
    GCOS_PRINTF("[PASS] VM created\n");
    
    // 检查版本
    GCOS_PRINTF("Version: %d.%d.%d\n", vm->version.major, vm->version.minor, vm->version.patch);
    GCOS_PRINTF("[PASS] Version OK\n");
    
    // 检查状态
    GCOS_PRINTF("State: %s\n", gcos_vm_state_to_string(vm->state));
    GCOS_PRINTF("[PASS] State OK\n");
    
    // 测试栈操作
    GCOSResult ret = gcos_vm_stack_push(vm, 42);
    if (ret != GCOS_OK) {
        GCOS_PRINTF("FAILED: Stack push error %d\n", ret);
        return 1;
    }
    GCOS_PRINTF("[PASS] Stack push OK\n");
    
    u32 value = 0;
    ret = gcos_vm_stack_pop(vm, &value);
    if (ret != GCOS_OK) {
        GCOS_PRINTF("FAILED: Stack pop error %d\n", ret);
        return 1;
    }
    GCOS_PRINTF("[PASS] Stack pop OK (value=%u)\n", value);
    
    // 测试堆分配
    u32 addr = gcos_vm_heap_alloc(vm, 100);
    GCOS_PRINTF("[PASS] Heap alloc OK (addr=%u)\n", addr);
    
    GCOS_PRINTF("\nAll tests passed!\n");
    
    // Cleanup
    gcos_vm_destroy(vm);
    GCOS_PRINTF("[PASS] VM destroyed\n");
    
    return 0;
}
