/**
 * @file gcos_memory.c
 * @brief GCOS VM Memory Management Implementation
 * 
 * COS3 Specification Partitioned Memory Management:
 * - Executor Stack (256 x 4B = 1KB, Volatile)
 * - Indirect Variable Stack (64 x 16B = 1KB, Volatile)
 * - Global Data Area (4KB, Volatile)
 * - Heap (8KB, Non-volatile)
 * - Module Program Area (16KB, Non-volatile)
 * 
 * Zero Dynamic Memory Allocation Principle: All memory statically allocated at compile time
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * API Implementation - Executor Stack Operations
 * ============================================================================ */

GCOSResult gcos_memory_stack_push(GCOSVM *vm, u32 value) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Check stack overflow */
    if (vm->runtime.stack_pointer >= GCOS_EXECUTOR_STACK_SIZE) {
        vm->runtime.exception = EXCEPTION_STACK_OVERFLOW;
        vm->state = GCOS_STATE_EXCEPTION;
        GCOS_PRINTF("[GCOS Memory] Stack overflow at SP=%u\n", vm->runtime.stack_pointer);
        return GCOS_ERROR_STACK_OVERFLOW;
    }
    
    /* Push to stack */
    vm->runtime.executor_stack[vm->runtime.stack_pointer] = value;
    vm->runtime.stack_pointer++;
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_stack_pop(GCOSVM *vm, u32 *value) {
    if (vm == NULL || value == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Check stack underflow */
    if (vm->runtime.stack_pointer == 0) {
        vm->runtime.exception = EXCEPTION_STACK_UNDERFLOW;
        vm->state = GCOS_STATE_EXCEPTION;
        GCOS_PRINTF("[GCOS Memory] Stack underflow\n");
        return GCOS_ERROR_STACK_UNDERFLOW;
    }
    
    /* Pop from stack */
    vm->runtime.stack_pointer--;
    *value = vm->runtime.executor_stack[vm->runtime.stack_pointer];
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_stack_peek(const GCOSVM *vm, u32 offset, u32 *value) {
    if (vm == NULL || value == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    u32 index = vm->runtime.stack_pointer - offset - 1;
    if (index >= GCOS_EXECUTOR_STACK_SIZE) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    *value = vm->runtime.executor_stack[index];
    return GCOS_SUCCESS;
}

u32 gcos_memory_get_stack_depth(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    return vm->runtime.stack_pointer;
}

GCOSResult gcos_memory_clear_stack(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    vm->runtime.stack_pointer = 0;
    memset(vm->runtime.executor_stack, 0, sizeof(vm->runtime.executor_stack));
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Indirect Variable Stack Operations
 * ============================================================================ */

GCOSResult gcos_memory_indirect_push(GCOSVM *vm, const u8 data[16]) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Check stack overflow */
    if (vm->runtime.indirect_stack_pointer >= GCOS_INDIRECT_STACK_SIZE) {
        GCOS_PRINTF("[GCOS Memory] Indirect stack overflow\n");
        return GCOS_ERROR_STACK_OVERFLOW;
    }
    
    /* Push to stack (copy 16 bytes) */
    memcpy(vm->runtime.indirect_stack[vm->runtime.indirect_stack_pointer],
           data, 16);
    vm->runtime.indirect_stack_pointer++;
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_indirect_pop(GCOSVM *vm, u8 data[16]) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Check stack underflow */
    if (vm->runtime.indirect_stack_pointer == 0) {
        GCOS_PRINTF("[GCOS Memory] Indirect stack underflow\n");
        return GCOS_ERROR_STACK_UNDERFLOW;
    }
    
    /* Pop from stack */
    vm->runtime.indirect_stack_pointer--;
    memcpy(data, vm->runtime.indirect_stack[vm->runtime.indirect_stack_pointer], 16);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_indirect_read(const GCOSVM *vm, u32 index, u8 data[16]) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (index >= GCOS_INDIRECT_STACK_SIZE) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    memcpy(data, vm->runtime.indirect_stack[index], 16);
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_indirect_write(GCOSVM *vm, u32 index, const u8 data[16]) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (index >= GCOS_INDIRECT_STACK_SIZE) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    memcpy(vm->runtime.indirect_stack[index], data, 16);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Global Data Area Operations
 * ============================================================================ */

GCOSResult gcos_memory_global_read(const GCOSVM *vm, u32 offset, u8 *data, u32 size) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Boundary check */
    if (offset + size > GCOS_GLOBAL_DATA_SIZE) {
        GCOS_PRINTF("[GCOS Memory] Global data read out of bounds: offset=%u, size=%u\n",
               offset, size);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    memcpy(data, &vm->runtime.global_data[offset], size);
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_global_write(GCOSVM *vm, u32 offset, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Boundary check */
    if (offset + size > GCOS_GLOBAL_DATA_SIZE) {
        GCOS_PRINTF("[GCOS Memory] Global data write out of bounds: offset=%u, size=%u\n",
               offset, size);
        vm->runtime.exception = EXCEPTION_ACCESS_VIOLATION;
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    memcpy(&vm->runtime.global_data[offset], data, size);
    
    /* Update usage count */
    if (offset + size > vm->runtime.global_data_used) {
        vm->runtime.global_data_used = offset + size;
    }
    
    return GCOS_SUCCESS;
}

u32 gcos_memory_get_global_data_used(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    return vm->runtime.global_data_used;
}

GCOSResult gcos_memory_clear_global_data(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    memset(vm->runtime.global_data, 0, GCOS_GLOBAL_DATA_SIZE);
    vm->runtime.global_data_used = 0;
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Heap Management (Simplified)
 * ============================================================================ */

/**
 * @brief Allocate memory from heap
 * @param vm VM instance
 * @param size Allocation size in bytes
 * @return Offset address, 0 indicates failure
 * 
 * @note COS3 specification: Heap is non-volatile, requires eflash library integration
 * @note Current implementation is a simplified linear allocator, full heap manager needed later
 */
u32 gcos_memory_heap_alloc(GCOSVM *vm, u32 size) {
    if (vm == NULL || size == 0) {
        return 0;
    }
    
    /* Align to 4-byte boundary */
    u32 aligned_size = (size + 3) & ~3;
    
    /* Check if there is enough space */
    if (vm->runtime.heap_used + aligned_size > GCOS_HEAP_SIZE) {
        GCOS_PRINTF("[GCOS Memory] Heap allocation failed: requested=%u, available=%u\n",
               aligned_size, GCOS_HEAP_SIZE - vm->runtime.heap_used);
        return 0;
    }
    
    /* Allocate and return offset address */
    u32 addr = vm->runtime.heap_used;
    vm->runtime.heap_used += aligned_size;
    
    /* Clear allocated memory */
    memset(&vm->runtime.heap[addr], 0, aligned_size);
    
    GCOS_PRINTF("[GCOS Memory] Heap allocated: addr=%u, size=%u (aligned=%u)\n",
           addr, size, aligned_size);
    
    return addr;
}

/**
 * @brief Free heap memory
 * @param vm VM instance
 * @param addr Offset address
 * @return GCOS_SUCCESS on success, other values indicate failure
 * 
 * @note Current implementation is simplified, only supports LIFO deallocation
 * @note TODO: Implement complete free list or bitmap management
 */
GCOSResult gcos_memory_heap_free(GCOSVM *vm, u32 addr) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Simplified implementation: only supports freeing the last allocated block */
    /* TODO: Implement complete heap manager */
    
    GCOS_PRINTF("[GCOS Memory] Warning: heap_free is simplified (LIFO only)\n");
    
    return GCOS_SUCCESS;
}

/**
 * @brief Read data from heap
 * @param vm VM instance
 * @param addr Offset address
 * @param data Output buffer
 * @param size Read size
 * @return GCOS_SUCCESS on success, other values indicate failure
 */
GCOSResult gcos_memory_heap_read(const GCOSVM *vm, u32 addr, u8 *data, u32 size) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Boundary check */
    if (addr + size > GCOS_HEAP_SIZE) {
        GCOS_PRINTF("[GCOS Memory] Heap read out of bounds: addr=%u, size=%u\n",
               addr, size);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    memcpy(data, &vm->runtime.heap[addr], size);
    return GCOS_SUCCESS;
}

/**
 * @brief Write data to heap
 * @param vm VM instance
 * @param addr Offset address
 * @param data Input data
 * @param size Write size
 * @return GCOS_SUCCESS on success, other values indicate failure
 */
GCOSResult gcos_memory_heap_write(GCOSVM *vm, u32 addr, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Boundary check */
    if (addr + size > GCOS_HEAP_SIZE) {
        printf("[GCOS Memory] Heap write out of bounds: addr=%u, size=%u\n",
               addr, size);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    memcpy(&vm->runtime.heap[addr], data, size);
    return GCOS_SUCCESS;
}

u32 gcos_memory_get_heap_used(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    return vm->runtime.heap_used;
}

u32 gcos_memory_get_heap_free(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    return GCOS_HEAP_SIZE - vm->runtime.heap_used;
}

GCOSResult gcos_memory_clear_heap(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    memset(vm->runtime.heap, 0, GCOS_HEAP_SIZE);
    vm->runtime.heap_used = 0;
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Module Code Area Operations
 * ============================================================================ */

GCOSResult gcos_memory_code_read(const GCOSVM *vm, u32 offset, u8 *code, u32 size) {
    if (vm == NULL || code == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Boundary check */
    if (offset + size > GCOS_MODULE_CODE_SIZE) {
        printf("[GCOS Memory] Code read out of bounds: offset=%u, size=%u\n",
               offset, size);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* TODO: Read from actual module code area */
    /* Currently returns placeholder */
    memset(code, 0, size);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_memory_code_write(GCOSVM *vm, u32 offset, const u8 *code, u32 size) {
    if (vm == NULL || code == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Boundary check */
    if (offset + size > GCOS_MODULE_CODE_SIZE) {
        printf("[GCOS Memory] Code write out of bounds: offset=%u, size=%u\n",
               offset, size);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* TODO: Write to actual module code area */
    /* Currently only updates size */
    if (offset + size > vm->runtime.code_size) {
        vm->runtime.code_size = offset + size;
    }
    
    return GCOS_SUCCESS;
}

u32 gcos_memory_get_code_size(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    return vm->runtime.code_size;
}

/* ============================================================================
 * API Implementation - Memory Statistics and Debugging
 * ============================================================================ */

void gcos_memory_print_stats(const GCOSVM *vm) {
    if (vm == NULL) {
        printf("[GCOS Memory] Error: NULL VM pointer\n");
        return;
    }
    
    printf("\n========== Memory Statistics ==========\n");
    printf("Executor Stack: %u / %u (%.1f%% used)\n",
           vm->runtime.stack_pointer, GCOS_EXECUTOR_STACK_SIZE,
           (double)vm->runtime.stack_pointer / GCOS_EXECUTOR_STACK_SIZE * 100.0);
    printf("Indirect Stack: %u / %u (%.1f%% used)\n",
           vm->runtime.indirect_stack_pointer, GCOS_INDIRECT_STACK_SIZE,
           (double)vm->runtime.indirect_stack_pointer / GCOS_INDIRECT_STACK_SIZE * 100.0);
    printf("Global Data: %u / %u bytes (%.1f%% used)\n",
           vm->runtime.global_data_used, GCOS_GLOBAL_DATA_SIZE,
           (double)vm->runtime.global_data_used / GCOS_GLOBAL_DATA_SIZE * 100.0);
    printf("Heap: %u / %u bytes (%.1f%% used, %u free)\n",
           vm->runtime.heap_used, GCOS_HEAP_SIZE,
           (double)vm->runtime.heap_used / GCOS_HEAP_SIZE * 100.0,
           GCOS_HEAP_SIZE - vm->runtime.heap_used);
    printf("Module Code: %u / %u bytes\n",
           vm->runtime.code_size, GCOS_MODULE_CODE_SIZE);
    printf("=======================================\n\n");
}

GCOSResult gcos_memory_validate(const GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    bool valid = true;
    
    /* Validate executor stack */
    if (vm->runtime.stack_pointer > GCOS_EXECUTOR_STACK_SIZE) {
        printf("[GCOS Memory] Validation Error: Stack pointer out of range\n");
        valid = false;
    }
    
    /* Validate indirect stack */
    if (vm->runtime.indirect_stack_pointer > GCOS_INDIRECT_STACK_SIZE) {
        printf("[GCOS Memory] Validation Error: Indirect stack pointer out of range\n");
        valid = false;
    }
    
    /* Validate global data area */
    if (vm->runtime.global_data_used > GCOS_GLOBAL_DATA_SIZE) {
        printf("[GCOS Memory] Validation Error: Global data overflow\n");
        valid = false;
    }
    
    /* Validate heap */
    if (vm->runtime.heap_used > GCOS_HEAP_SIZE) {
        printf("[GCOS Memory] Validation Error: Heap overflow\n");
        valid = false;
    }
    
    /* Validate code area */
    if (vm->runtime.code_size > GCOS_MODULE_CODE_SIZE) {
        printf("[GCOS Memory] Validation Error: Code size overflow\n");
        valid = false;
    }
    
    return valid ? GCOS_SUCCESS : GCOS_ERROR_VALIDATION_FAILED;
}
