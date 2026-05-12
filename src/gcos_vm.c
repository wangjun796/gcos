/**
 * @file gcos_vm.c
 * @brief GCOS VM Core Implementation - Based on COS3 Specification
 * 
 * Features:
 * - Zero dynamic memory allocation (all memory statically allocated)
 * - Partitioned memory management (5 independent regions)
 * - COS3-compliant API design
 * - Support for procedural application post-download
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * Global Static Instance - Zero Dynamic Memory Allocation
 * ============================================================================ */

/**
 * @brief Global VM instance (singleton pattern)
 * @note COS3 specification requires zero dynamic memory allocation, suitable for embedded environments
 */
static GCOSVM g_gcos_vm_instance;

/**
 * @brief VM initialization flag
 */
static bool g_vm_initialized = false;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize runtime context
 * @param ctx Runtime context pointer
 */
static void runtime_context_init(GCOSRuntimeContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    memset(ctx, 0, sizeof(GCOSRuntimeContext));
    
    /* Executor stack initialization */
    ctx->stack_pointer = 0;
    ctx->base_pointer = 0;
    
    /* Indirect variable stack initialization */
    ctx->indirect_stack_pointer = 0;
    
    /* Global data area initialization */
    ctx->global_data_used = 0;
    
    /* Heap initialization */
    ctx->heap_used = 0;
    
    /* Program counter initialization */
    ctx->program_counter = 0;
    
    /* Frame stack initialization */
    ctx->frame_top = 0;
}

/**
 * @brief Initialize transaction context
 * @param trans Transaction context pointer
 */
static void transaction_context_init(GCOTransactionContext *trans) {
    if (trans == NULL) {
        return;
    }
    
    memset(trans, 0, sizeof(GCOTransactionContext));
    trans->active = false;
    trans->backup_size = 0;
    trans->checkpoint_count = 0;
}

/**
 * @brief Initialize security management context
 * @param security Security management context pointer
 */
static void security_context_init(GCOSSecurityContext *security) {
    if (security == NULL) {
        return;
    }
    
    memset(security, 0, sizeof(GCOSSecurityContext));
    security->current_domain = 0;
    security->authorization_table_size = 0;
}

/**
 * @brief Initialize VM instance
 * @param vm VM instance pointer
 * @return GCOSResult Success or error code
 */
static GCOSResult vm_instance_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Clear entire structure */
    memset(vm, 0, sizeof(GCOSVM));
    
    /* Initialize version information */
    vm->version.major = GCOS_VM_VERSION_MAJOR;
    vm->version.minor = GCOS_VM_VERSION_MINOR;
    vm->version.patch = GCOS_VM_VERSION_PATCH;
    
    /* Initialize runtime context */
    runtime_context_init(&vm->runtime);
    
    /* Initialize transaction context */
    transaction_context_init(&vm->transaction);
    
    /* Initialize security management context */
    security_context_init(&vm->security);
    
    /* Initialize module and application counts */
    vm->module_count = 0;
    vm->app_count = 0;
    vm->current_module_index = GCOS_INVALID_INDEX;
    vm->current_app_index = GCOS_INVALID_INDEX;
    
    /* Initialize channel management */
    vm->current_channel = 0;
    vm->active_channels = 0;
    for (int i = 0; i < GCOS_MAX_CHANNELS; i++) {
        vm->channels[i].selected_app_index = GCOS_INVALID_INDEX;
        vm->channels[i].active = false;
    }
    
    /* Initialize state */
    vm->state = GCOS_STATE_IDLE;
    vm->runtime.exception = EXCEPTION_NONE;
    
    /* Initialize statistics */
    vm->stats.instructions_executed = 0;
    vm->total_execution_time_us = 0;
    
    return GCOS_OK;
}

/* ============================================================================
 * API Implementation - Lifecycle Management
 * ============================================================================ */

GCOSVM* gcos_vm_create(void) {
    /* If already initialized, return existing instance */
    if (g_vm_initialized) {
        GCOS_PRINTF("[GCOS VM] Warning: VM already created, returning existing instance\n");
        return &g_gcos_vm_instance;
    }
    
    /* Initialize global instance */
    GCOSResult result = vm_instance_init(&g_gcos_vm_instance);
    if (result != GCOS_OK) {
        GCOS_PRINTF("[GCOS VM] Error: Failed to initialize VM (error=%d)\n", result);
        return NULL;
    }
    
    g_vm_initialized = true;
    GCOS_PRINTF("[GCOS VM] VM created successfully (static allocation)\n");
    
    return &g_gcos_vm_instance;
}

GCOSResult gcos_vm_destroy(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if this is the global instance */
    if (vm != &g_gcos_vm_instance) {
        GCOS_PRINTF("[GCOS VM] Error: Invalid VM instance pointer\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Stop any ongoing operations */
    if (vm->state == GCOS_STATE_RUNNING) {
        vm->state = GCOS_STATE_IDLE;
    }
    
    /* Clean up transaction backup data (if any) */
    if (vm->transaction.backup_data != NULL) {
        /* Note: In zero dynamic memory allocation mode, there should be no dynamically allocated memory here */
        GCOS_PRINTF("[GCOS VM] Warning: Transaction backup data exists (should not happen in static mode)\n");
        vm->transaction.backup_data = NULL;
        vm->transaction.backup_size = 0;
    }
    
    /* Reset instance but do not release memory (statically allocated) */
    vm_instance_init(vm);
    g_vm_initialized = false;
    
    GCOS_PRINTF("[GCOS VM] VM destroyed (memory retained for reuse)\n");
    return GCOS_OK;
}

GCOSResult gcos_vm_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* If not created, create first */
    if (!g_vm_initialized) {
        if (gcos_vm_create() == NULL) {
            return GCOS_ERR_OUT_OF_MEMORY;
        }
        return GCOS_OK;
    }
    
    /* If already initialized, reset first */
    return gcos_vm_reset(vm);
}

GCOSResult gcos_vm_reset(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Save version information */
    u8 saved_major = vm->version.major;
    u8 saved_minor = vm->version.minor;
    u8 saved_patch = vm->version.patch;
    
    /* Re-initialize */
    GCOSResult result = vm_instance_init(vm);
    if (result != GCOS_OK) {
        return result;
    }
    
    /* Restore version information */
    vm->version.major = saved_major;
    vm->version.minor = saved_minor;
    vm->version.patch = saved_patch;
    
    GCOS_PRINTF("[GCOS VM] VM reset completed\n");
    return GCOS_OK;
}

/* ============================================================================
 * API Implementation - State Query
 * ============================================================================ */

GCOSState gcos_vm_get_state(const GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_STATE_EXCEPTION;
    }
    return vm->state;
}

const char* gcos_vm_state_to_string(GCOSState state) {
    switch (state) {
        case GCOS_STATE_IDLE:      return "IDLE";
        case GCOS_STATE_RUNNING:   return "RUNNING";
        case GCOS_STATE_SUSPENDED: return "SUSPENDED";
        case GCOS_STATE_EXCEPTION: return "EXCEPTION";
        default:                   return "UNKNOWN";
    }
}

GCOSExceptionType gcos_vm_get_exception(const GCOSVM *vm) {
    if (vm == NULL) {
        return EXCEPTION_NONE;
    }
    return vm->runtime.exception;
}

const char* gcos_vm_exception_to_string(GCOSExceptionType exception) {
    switch (exception) {
        case EXCEPTION_NONE:                return "NONE";
        case EXCEPTION_STACK_OVERFLOW:      return "STACK_OVERFLOW";
        case EXCEPTION_STACK_UNDERFLOW:     return "STACK_UNDERFLOW";
        case EXCEPTION_DIVISION_BY_ZERO:    return "DIVISION_BY_ZERO";
        case EXCEPTION_INVALID_OPCODE:      return "INVALID_OPCODE";
        case EXCEPTION_ACCESS_VIOLATION:    return "MEMORY_ACCESS";
        case EXCEPTION_TRANSACTION_ABORT:   return "TRANSACTION_ABORT";
        case EXCEPTION_SECURITY_VIOLATION:  return "SECURITY_VIOLATION";
        default:                            return "UNKNOWN";
    }
}

/* ============================================================================
 * API Implementation - Statistics
 * ============================================================================ */

void gcos_vm_get_stats(const GCOSVM *vm, u64 *instr_count, u64 *exec_time_us) {
    if (vm == NULL) {
        return;
    }
    
    if (instr_count != NULL) {
        *instr_count = vm->stats.instructions_executed;
    }
    
    if (exec_time_us != NULL) {
        *exec_time_us = vm->total_execution_time_us;
    }
}

void gcos_vm_print_info(const GCOSVM *vm) {
    if (vm == NULL) {
        GCOS_PRINTF("[GCOS VM] Error: NULL VM pointer\n");
        return;
    }
        
    GCOS_PRINTF("\n========== GCOS VM Information ==========\n");
    GCOS_PRINTF("Version: %d.%d.%d\n",
                vm->version.major, vm->version.minor, vm->version.patch);
    GCOS_PRINTF("State: %s\n", gcos_vm_state_to_string(vm->state));
    GCOS_PRINTF("Exception: %s\n", gcos_vm_exception_to_string(vm->runtime.exception));
    GCOS_PRINTF("\n--- Runtime Context ---\n");
    GCOS_PRINTF("Stack Pointer: %u / %u\n",
                vm->runtime.stack_pointer, GCOS_EXECUTOR_STACK_SIZE);
    GCOS_PRINTF("Indirect Stack Pointer: %u / %u\n",
                vm->runtime.indirect_stack_pointer, GCOS_INDIRECT_STACK_SIZE);
    GCOS_PRINTF("Global Data Used: %u / %u bytes\n",
                vm->runtime.global_data_used, GCOS_GLOBAL_DATA_SIZE);
    GCOS_PRINTF("Heap Used: %u / %u bytes\n",
                vm->runtime.heap_used, GCOS_HEAP_SIZE);
    GCOS_PRINTF("Program Counter: %u\n", vm->runtime.program_counter);
    GCOS_PRINTF("\n--- Modules & Apps ---\n");
    GCOS_PRINTF("Module Count: %u / %u\n",
                vm->module_count, GCOS_MAX_MODULES);
    GCOS_PRINTF("App Count: %u / %u\n",
                vm->app_count, GCOS_MAX_APPS);
    GCOS_PRINTF("Current Channel: %u\n", vm->current_channel);
    GCOS_PRINTF("\n--- Statistics ---\n");
    GCOS_PRINTF("Instructions Executed: %llu\n",
                (unsigned long long)vm->stats.instructions_executed);
    GCOS_PRINTF("Total Execution Time: %llu us\n",
                (unsigned long long)vm->total_execution_time_us);
    GCOS_PRINTF("=========================================\n\n");
}

/* ============================================================================
 * API Implementation - Memory Access (Safe Version)
 * ============================================================================ */

/**
 * @brief Pop value from executor stack
 * @param vm VM instance
 * @param value Output value
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_stack_pop(GCOSVM *vm, u32 *value) {
    if (vm == NULL || value == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (vm->runtime.stack_pointer == 0) {
        vm->runtime.exception = EXCEPTION_STACK_UNDERFLOW;
        vm->state = GCOS_STATE_EXCEPTION;
        return GCOS_ERROR_STACK_UNDERFLOW;
    }
    
    vm->runtime.stack_pointer--;
    *value = vm->runtime.executor_stack[vm->runtime.stack_pointer];
    
    return GCOS_OK;
}

/**
 * @brief Push value to executor stack
 * @param vm VM instance
 * @param value Value to push
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_vm_stack_push(GCOSVM *vm, u32 value) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (vm->runtime.stack_pointer >= GCOS_EXECUTOR_STACK_SIZE) {
        vm->runtime.exception = EXCEPTION_STACK_OVERFLOW;
        vm->state = GCOS_STATE_EXCEPTION;
        return GCOS_ERROR_STACK_OVERFLOW;
    }
    
    vm->runtime.executor_stack[vm->runtime.stack_pointer] = value;
    vm->runtime.stack_pointer++;
    
    return GCOS_OK;
}

/**
 * @brief 从堆分配内存（返回偏移地址）
 * @param vm VM实例
 * @param size 分配大小
 * @return 偏移地址，0表示失败
 * @note COS3规范：堆是非易失性的，需要集成eflash库
 */
u32 gcos_vm_heap_alloc(GCOSVM *vm, u32 size) {
    if (vm == NULL || size == 0) {
        return 0;
    }
    
    /* 对齐到4字节边界 */
    u32 aligned_size = (size + 3) & ~3;
    
    /* 检查是否有足够空间 */
    if (vm->runtime.heap_used + aligned_size > GCOS_HEAP_SIZE) {
        GCOS_PRINTF("[GCOS VM] Heap allocation failed: insufficient space\n");
        return 0;
    }
    
    /* 分配并返回偏移地址 */
    u32 addr = vm->runtime.heap_used;
    vm->runtime.heap_used += aligned_size;
    
    /* 清零分配的内存 */
    memset(&vm->runtime.heap[addr], 0, aligned_size);
    
    return addr;
}

/**
 * @brief 释放堆内存（简化版，实际应使用标记清除）
 * @param vm VM实例
 * @param addr 偏移地址
 * @return GCOSResult 成功，其他值失败
 * @note TODO: 实现完整的堆管理器
 */
GCOSResult gcos_vm_heap_free(GCOSVM *vm, u32 addr) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* 简化实现：仅标记为可用 */
    /* TODO: 实现完整的空闲链表或位图管理 */
    
    return GCOS_OK;
}

/* ============================================================================
 * 调试辅助函数
 * ============================================================================ */

/**
 * @brief 打印调用栈
 * @param vm VM实例
 */
void gcos_vm_print_call_stack(const GCOSVM *vm) {
    if (vm == NULL) {
        GCOS_PRINTF("[GCOS VM] Error: NULL VM pointer\n");
        return;
    }
    
    GCOS_PRINTF("\n--- Call Stack ---\n");
    GCOS_PRINTF("Frame Top: %u\n", vm->runtime.frame_top);
    
    for (u32 i = 0; i < vm->runtime.frame_top && i < 64; i++) {
        const GCOSStackFrame *frame = &vm->runtime.frame_stack[i];
        GCOS_PRINTF("Frame[%u]: ReturnAddr=%u, BasePtr=%u, Size=%u\n",
               i, frame->return_address, frame->base_pointer, frame->frame_size);
    }
    GCOS_PRINTF("------------------\n\n");
}

/**
 * @brief 验证VM状态一致性
 * @param vm VM实例
 * @return true 一致，false 不一致
 */
bool gcos_vm_validate(const GCOSVM *vm) {
    if (vm == NULL) {
        return false;
    }
    
    bool valid = true;
    
    /* 检查栈指针范围 */
    if (vm->runtime.stack_pointer > GCOS_EXECUTOR_STACK_SIZE) {
        GCOS_PRINTF("[GCOS VM] Validation Error: Stack pointer out of range\n");
        valid = false;
    }
    
    /* 检查间接栈指针范围 */
    if (vm->runtime.indirect_stack_pointer > GCOS_INDIRECT_STACK_SIZE) {
        GCOS_PRINTF("[GCOS VM] Validation Error: Indirect stack pointer out of range\n");
        valid = false;
    }
    
    /* 检查全局数据区使用量 */
    if (vm->runtime.global_data_used > GCOS_GLOBAL_DATA_SIZE) {
        GCOS_PRINTF("[GCOS VM] Validation Error: Global data overflow\n");
        valid = false;
    }
    
    /* 检查堆使用量 */
    if (vm->runtime.heap_used > GCOS_HEAP_SIZE) {
        printf("[GCOS VM] Validation Error: Heap overflow\n");
        valid = false;
    }
    
    /* 检查模块数量 */
    if (vm->module_count > GCOS_MAX_MODULES) {
        printf("[GCOS VM] Validation Error: Too many modules\n");
        valid = false;
    }
    
    /* 检查应用数量 */
    if (vm->app_count > GCOS_MAX_APPS) {
        printf("[GCOS VM] Validation Error: Too many apps\n");
        valid = false;
    }
    
    return valid;
}

/* ============================================================================
 * 版本信息
 * ============================================================================ */

const char* gcos_vm_get_version(void) {
    return GCOS_VM_VERSION;
}

void gcos_vm_get_version_info(u8 *major, u8 *minor, u8 *patch) {
    if (major != NULL) *major = GCOS_VM_VERSION_MAJOR;
    if (minor != NULL) *minor = GCOS_VM_VERSION_MINOR;
    if (patch != NULL) *patch = GCOS_VM_VERSION_PATCH;
}

/**
 * @brief Get global VM instance pointer
 * 
 * Used by JCShell server and other modules that need access to the VM.
 * 
 * @return Pointer to global VM instance, or NULL if not initialized
 */
GCOSVM* gcos_vm_get_instance(void) {
    if (!g_vm_initialized) {
        return NULL;
    }
    return &g_gcos_vm_instance;
}
