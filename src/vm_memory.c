#include "vm_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 栈操作实现
 * ============================================================================ */

int vm_stack_push(VMContext *vm, u32 value) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->stack_pointer >= VM_EXECUTOR_STACK_SIZE) {
        vm->exception = EXCEPTION_STACK_OVERFLOW;
        return -2; /* 栈溢出 */
    }
    
    vm->executor_stack[vm->stack_pointer++] = value;
    return 0;
}

int vm_stack_pop(VMContext *vm, u32 *value) {
    if (vm == NULL || value == NULL) {
        return -1;
    }
    
    if (vm->stack_pointer == 0) {
        vm->exception = EXCEPTION_STACK_UNDERFLOW;
        return -2; /* 栈下溢 */
    }
    
    *value = vm->executor_stack[--vm->stack_pointer];
    return 0;
}

int vm_stack_peek(VMContext *vm, u32 *value) {
    if (vm == NULL || value == NULL) {
        return -1;
    }
    
    if (vm->stack_pointer == 0) {
        return -2; /* 栈为空 */
    }
    
    *value = vm->executor_stack[vm->stack_pointer - 1];
    return 0;
}

u32 vm_stack_depth(const VMContext *vm) {
    if (vm == NULL) {
        return 0;
    }
    return vm->stack_pointer;
}

int vm_indirect_stack_push(VMContext *vm, const u8 *data) {
    if (vm == NULL || data == NULL) {
        return -1;
    }
    
    if (vm->indirect_stack_pointer >= VM_INDIRECT_VAR_STACK_SIZE) {
        return -2; /* 间接栈溢出 */
    }
    
    memcpy(vm->indirect_var_stack[vm->indirect_stack_pointer], 
           data, VM_INDIRECT_UNIT_SIZE);
    vm->indirect_stack_pointer++;
    
    return 0;
}

int vm_indirect_stack_pop(VMContext *vm, u8 *data) {
    if (vm == NULL || data == NULL) {
        return -1;
    }
    
    if (vm->indirect_stack_pointer == 0) {
        return -2; /* 间接栈下溢 */
    }
    
    vm->indirect_stack_pointer--;
    memcpy(data, vm->indirect_var_stack[vm->indirect_stack_pointer],
           VM_INDIRECT_UNIT_SIZE);
    
    return 0;
}

/* ============================================================================
 * 堆管理实现（简单线性分配器）
 * ============================================================================ */

typedef struct {
    u32 size;           /* 块大小 */
    u32 used;           /* 是否已使用 */
    u32 magic;          /* 魔数，用于检测损坏 */
} HeapBlockHeader;

#define HEAP_BLOCK_MAGIC  0xHEAP1234

void* vm_heap_alloc(VMContext *vm, u32 size) {
    if (vm == NULL || size == 0) {
        return NULL;
    }
    
    /* 对齐到4字节边界 */
    size = (size + 3) & ~3;
    
    /* 计算总需求大小（头部 + 数据） */
    u32 total_size = sizeof(HeapBlockHeader) + size;
    
    /* 检查是否有足够空间 */
    if (vm->heap_used + total_size > VM_HEAP_SIZE) {
        vm->exception = EXCEPTION_OUT_OF_MEMORY;
        return NULL;
    }
    
    /* 分配块 */
    u32 offset = vm->heap_used;
    HeapBlockHeader *header = (HeapBlockHeader*)(vm->heap + offset);
    header->size = size;
    header->used = 1;
    header->magic = HEAP_BLOCK_MAGIC;
    
    /* 更新堆使用量 */
    vm->heap_used += total_size;
    
    /* 返回数据指针 */
    return (void*)(vm->heap + offset + sizeof(HeapBlockHeader));
}

int vm_heap_free(VMContext *vm, void *ptr) {
    if (vm == NULL || ptr == NULL) {
        return -1;
    }
    
    /* 检查指针是否在堆范围内 */
    u8 *addr = (u8*)ptr;
    if (addr < vm->heap || addr >= vm->heap + VM_HEAP_SIZE) {
        return -2; /* 无效指针 */
    }
    
    /* 获取块头部 */
    HeapBlockHeader *header = (HeapBlockHeader*)(addr - sizeof(HeapBlockHeader));
    
    /* 验证魔数 */
    if (header->magic != HEAP_BLOCK_MAGIC) {
        return -3; /* 块已损坏 */
    }
    
    if (!header->used) {
        return -4; /* 块未使用或已释放 */
    }
    
    /* 标记为未使用 */
    header->used = 0;
    
    /* 注意：这是一个简单的分配器，不进行实际的内存回收 */
    /* 在实际实现中，应该合并空闲块或使用更复杂的分配策略 */
    
    return 0;
}

void* vm_heap_realloc(VMContext *vm, void *ptr, u32 new_size) {
    if (vm == NULL) {
        return NULL;
    }
    
    if (ptr == NULL) {
        /* 相当于malloc */
        return vm_heap_alloc(vm, new_size);
    }
    
    if (new_size == 0) {
        /* 相当于free */
        vm_heap_free(vm, ptr);
        return NULL;
    }
    
    /* 获取旧块信息 */
    u8 *addr = (u8*)ptr;
    HeapBlockHeader *old_header = (HeapBlockHeader*)(addr - sizeof(HeapBlockHeader));
    u32 old_size = old_header->size;
    
    if (new_size <= old_size) {
        /* 新大小小于等于旧大小，无需重新分配 */
        return ptr;
    }
    
    /* 分配新块 */
    void *new_ptr = vm_heap_alloc(vm, new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    /* 复制数据 */
    memcpy(new_ptr, ptr, old_size);
    
    /* 释放旧块 */
    vm_heap_free(vm, ptr);
    
    return new_ptr;
}

void vm_heap_stats(const VMContext *vm, u32 *used, u32 *free) {
    if (vm == NULL) {
        return;
    }
    
    if (used != NULL) {
        *used = vm->heap_used;
    }
    if (free != NULL) {
        *free = VM_HEAP_SIZE - vm->heap_used;
    }
}

/* ============================================================================
 * 模块数据管理实现
 * ============================================================================ */

int vm_module_init_global_data(VMContext *vm, Module *module) {
    if (vm == NULL || module == NULL) {
        return -1;
    }
    
    /* 分配全局数据区 */
    module->global_data = (u8*)malloc(VM_GLOBAL_DATA_SIZE);
    if (module->global_data == NULL) {
        return -2;
    }
    
    /* 初始化为0 */
    memset(module->global_data, 0, VM_GLOBAL_DATA_SIZE);
    
    return 0;
}

int vm_module_alloc_domain_data(VMContext *vm, Module *module, u32 size) {
    if (vm == NULL || module == NULL || size == 0) {
        return -1;
    }
    
    /* 在堆上分配域数据 */
    module->domain_data = (u8*)vm_heap_alloc(vm, size);
    if (module->domain_data == NULL) {
        return -2;
    }
    
    /* 初始化为0 */
    memset(module->domain_data, 0, size);
    
    return 0;
}

int vm_module_free_global_data(VMContext *vm, Module *module) {
    if (vm == NULL || module == NULL) {
        return -1;
    }
    
    if (module->global_data != NULL) {
        free(module->global_data);
        module->global_data = NULL;
    }
    
    if (module->domain_data != NULL) {
        vm_heap_free(vm, module->domain_data);
        module->domain_data = NULL;
    }
    
    return 0;
}

int vm_module_read_rodata(const Module *module, u32 offset, u32 size, u8 *buffer) {
    if (module == NULL || buffer == NULL) {
        return -1;
    }
    
    if (module->readonly_data == NULL) {
        return -2; /* 没有只读数据 */
    }
    
    /* 边界检查 */
    /* 注意：这里需要知道只读数据的实际大小 */
    /* 简化处理，假设不会越界 */
    
    memcpy(buffer, module->readonly_data + offset, size);
    
    return 0;
}

/* ============================================================================
 * 应用数据管理实现
 * ============================================================================ */

void* vm_app_create_temp_static(VMContext *vm, AppInstance *app, u32 size) {
    if (vm == NULL || app == NULL || size == 0) {
        return NULL;
    }
    
    /* 在全局数据区分配 */
    /* 简化实现：直接在全局数据区找一个位置 */
    /* 实际应该有更复杂的管理机制 */
    
    void *ptr = vm_heap_alloc(vm, size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

void* vm_app_create_temp_dynamic(VMContext *vm, AppInstance *app, u32 size) {
    if (vm == NULL || app == NULL || size == 0) {
        return NULL;
    }
    
    /* 在全局数据区分配 */
    void *ptr = vm_heap_alloc(vm, size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

void* vm_app_create_cross_domain(VMContext *vm, AppInstance *app, u32 size) {
    if (vm == NULL || app == NULL || size == 0) {
        return NULL;
    }
    
    /* 跨域数据也分配在堆上 */
    void *ptr = vm_heap_alloc(vm, size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

void* vm_app_create_persistent(VMContext *vm, AppInstance *app, u32 size) {
    if (vm == NULL || app == NULL || size == 0) {
        return NULL;
    }
    
    /* 持久性数据分配在堆上（非易失性） */
    void *ptr = vm_heap_alloc(vm, size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

void* vm_app_create_app_domain(VMContext *vm, AppInstance *app, u32 size,
                                const u8 *init_data) {
    if (vm == NULL || app == NULL || size == 0) {
        return NULL;
    }
    
    /* 应用域数据分配在堆上 */
    void *ptr = vm_heap_alloc(vm, size);
    if (ptr != NULL) {
        if (init_data != NULL) {
            memcpy(ptr, init_data, size);
        } else {
            memset(ptr, 0, size);
        }
    }
    
    return ptr;
}

void* vm_app_create_ref_domain(VMContext *vm, AppInstance *app, u32 size) {
    if (vm == NULL || app == NULL || size == 0) {
        return NULL;
    }
    
    /* 引用域数据分配在堆上 */
    void *ptr = vm_heap_alloc(vm, size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    
    return ptr;
}

int vm_app_delete_data(VMContext *vm, AppInstance *app, void *data_ptr,
                       DataType data_type) {
    if (vm == NULL || app == NULL || data_ptr == NULL) {
        return -1;
    }
    
    /* 根据数据类型执行不同的清理操作 */
    switch (data_type) {
        case DATA_TYPE_TEMP_STATIC:
        case DATA_TYPE_TEMP_DYNAMIC:
        case DATA_TYPE_CROSS_DOMAIN:
        case DATA_TYPE_PERSISTENT:
        case DATA_TYPE_APP_DOMAIN:
        case DATA_TYPE_REF_DOMAIN:
            /* 释放堆内存 */
            return vm_heap_free(vm, data_ptr);
        
        default:
            return -2; /* 不支持的数据类型 */
    }
}

bool vm_check_ref_domain(VMContext *vm, const void *data_ptr) {
    if (vm == NULL || data_ptr == NULL) {
        return false;
    }
    
    /* 遍历所有应用的引用域，检查是否有引用 */
    for (u16 i = 0; i < vm->app_count; i++) {
        AppInstance *app = &vm->apps[i];
        if (app->ref_domain_data != NULL) {
            /* 简化检查：实际应该解析引用域数据结构 */
            /* 这里只是示例 */
        }
    }
    
    return false;
}

/* ============================================================================
 * 全局数据区操作实现
 * ============================================================================ */

int vm_global_write(VMContext *vm, u32 offset, u32 size, const u8 *data) {
    if (vm == NULL || data == NULL) {
        return -1;
    }
    
    if (offset + size > VM_GLOBAL_DATA_SIZE) {
        vm->exception = EXCEPTION_ACCESS_VIOLATION;
        return -2; /* 越界 */
    }
    
    memcpy(vm->global_data + offset, data, size);
    return 0;
}

int vm_global_read(const VMContext *vm, u32 offset, u32 size, u8 *buffer) {
    if (vm == NULL || buffer == NULL) {
        return -1;
    }
    
    if (offset + size > VM_GLOBAL_DATA_SIZE) {
        return -2; /* 越界 */
    }
    
    memcpy(buffer, vm->global_data + offset, size);
    return 0;
}

int vm_global_clear(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    memset(vm->global_data, 0, VM_GLOBAL_DATA_SIZE);
    return 0;
}

/* ============================================================================
 * 内存保护实现
 * ============================================================================ */

bool vm_check_memory_access(const VMContext *vm, u32 address, u32 size, bool write) {
    if (vm == NULL) {
        return false;
    }
    
    /* 检查地址范围 */
    u8 *addr = (u8*)(uintptr_t)address;
    
    /* 检查是否在堆范围内 */
    if (addr >= vm->heap && addr + size <= vm->heap + VM_HEAP_SIZE) {
        return true;
    }
    
    /* 检查是否在全局数据区范围内 */
    if (address < VM_GLOBAL_DATA_SIZE && address + size <= VM_GLOBAL_DATA_SIZE) {
        return !write || true; /* 全局数据区可读写 */
    }
    
    /* 检查是否在模块代码区范围内 */
    if (addr >= vm->module_code && 
        addr + size <= vm->module_code + VM_MODULE_CODE_SIZE) {
        return !write; /* 代码区只读 */
    }
    
    return false;
}

int vm_set_memory_permission(VMContext *vm, u32 start, u32 size,
                             bool readable, bool writable, bool executable) {
    /* 简化实现：实际应该有更复杂的内存保护机制 */
    /* 这里只是占位符 */
    (void)vm;
    (void)start;
    (void)size;
    (void)readable;
    (void)writable;
    (void)executable;
    
    return 0;
}

/* ============================================================================
 * 调试和诊断实现
 * ============================================================================ */

void vm_print_stack(const VMContext *vm) {
    if (vm == NULL) {
        printf("VM: NULL\n");
        return;
    }
    
    printf("=== Stack Contents ===\n");
    printf("Stack Pointer: %u / %u\n", 
           vm->stack_pointer, VM_EXECUTOR_STACK_SIZE);
    
    u32 start = vm->stack_pointer > 16 ? vm->stack_pointer - 16 : 0;
    for (u32 i = start; i < vm->stack_pointer; i++) {
        printf("[%u] = 0x%08X (%u)\n", i, 
               vm->executor_stack[i], vm->executor_stack[i]);
    }
    printf("=====================\n");
}

void vm_print_heap_usage(const VMContext *vm) {
    if (vm == NULL) {
        printf("VM: NULL\n");
        return;
    }
    
    u32 used, free_space;
    vm_heap_stats(vm, &used, &free_space);
    
    printf("=== Heap Usage ===\n");
    printf("Total: %u bytes\n", VM_HEAP_SIZE);
    printf("Used: %u bytes (%.2f%%)\n", used, 
           (float)used / VM_HEAP_SIZE * 100);
    printf("Free: %u bytes (%.2f%%)\n", free_space,
           (float)free_space / VM_HEAP_SIZE * 100);
    printf("==================\n");
}

void vm_print_memory_map(const VMContext *vm) {
    if (vm == NULL) {
        printf("VM: NULL\n");
        return;
    }
    
    printf("=== Memory Map ===\n");
    printf("Executor Stack: %p - %p (%u bytes)\n",
           (void*)vm->executor_stack,
           (void*)(vm->executor_stack + VM_EXECUTOR_STACK_SIZE),
           VM_EXECUTOR_STACK_SIZE * sizeof(u32));
    
    printf("Indirect Var Stack: %p - %p (%u bytes)\n",
           (void*)vm->indirect_var_stack,
           (void*)(vm->indirect_var_stack + VM_INDIRECT_VAR_STACK_SIZE),
           VM_INDIRECT_VAR_STACK_SIZE * VM_INDIRECT_UNIT_SIZE);
    
    printf("Global Data: %p - %p (%u bytes)\n",
           (void*)vm->global_data,
           (void*)(vm->global_data + VM_GLOBAL_DATA_SIZE),
           VM_GLOBAL_DATA_SIZE);
    
    printf("Heap: %p - %p (%u bytes)\n",
           (void*)vm->heap,
           (void*)(vm->heap + VM_HEAP_SIZE),
           VM_HEAP_SIZE);
    
    printf("Module Code: %p - %p (%u bytes)\n",
           (void*)vm->module_code,
           (void*)(vm->module_code + VM_MODULE_CODE_SIZE),
           VM_MODULE_CODE_SIZE);
    
    printf("==================\n");
}
