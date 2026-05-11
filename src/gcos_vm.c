/**
 * @file gcos_vm.c
 * @brief GCOS VM 核心实现 - 基于COS3规范
 * 
 * 特性:
 * - 零动态内存分配（所有内存静态分配）
 * - 分区内存管理（5个独立区域）
 * - 符合COS3规范的API设计
 * - 支持面向过程应用后下载
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 全局静态实例 - 零动态内存分配
 * ============================================================================ */

/**
 * @brief 全局VM实例（单例模式）
 * @note COS3规范要求零动态内存分配，适合嵌入式环境
 */
static GCOSVM g_gcos_vm_instance;

/**
 * @brief VM是否已初始化标志
 */
static bool g_vm_initialized = false;

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 初始化运行时上下文
 * @param ctx 运行时上下文指针
 */
static void runtime_context_init(GCOSRuntimeContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    memset(ctx, 0, sizeof(GCOSRuntimeContext));
    
    /* 执行器栈初始化 */
    ctx->stack_pointer = 0;
    ctx->base_pointer = 0;
    
    /* 间接变量栈初始化 */
    ctx->indirect_stack_pointer = 0;
    
    /* 全局数据区初始化 */
    ctx->global_data_used = 0;
    
    /* 堆初始化 */
    ctx->heap_used = 0;
    
    /* 程序计数器初始化 */
    ctx->program_counter = 0;
    
    /* 帧栈初始化 */
    ctx->frame_top = 0;
}

/**
 * @brief 初始化事务上下文
 * @param trans 事务上下文指针
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
 * @brief 初始化安全管理上下文
 * @param security 安全管理上下文指针
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
 * @brief 初始化虚拟机实例
 * @param vm VM实例指针
 * @return GCOSResult 成功，其他值失败
 */
static GCOSResult vm_instance_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* 清零整个结构 */
    memset(vm, 0, sizeof(GCOSVM));
    
    /* 初始化版本信息 */
    vm->version.major = GCOS_VM_VERSION_MAJOR;
    vm->version.minor = GCOS_VM_VERSION_MINOR;
    vm->version.patch = GCOS_VM_VERSION_PATCH;
    
    /* 初始化运行时上下文 */
    runtime_context_init(&vm->runtime);
    
    /* 初始化事务上下文 */
    transaction_context_init(&vm->transaction);
    
    /* 初始化安全管理上下文 */
    security_context_init(&vm->security);
    
    /* 初始化模块和应用计数 */
    vm->module_count = 0;
    vm->app_count = 0;
    vm->current_module_index = GCOS_INVALID_INDEX;
    vm->current_app_index = GCOS_INVALID_INDEX;
    
    /* 初始化通道管理 */
    vm->current_channel = 0;
    vm->active_channels = 0;
    for (int i = 0; i < GCOS_MAX_CHANNELS; i++) {
        vm->channels[i].selected_app_index = GCOS_INVALID_INDEX;
        vm->channels[i].active = false;
    }
    
    /* 初始化状态 */
    vm->state = GCOS_STATE_IDLE;
    vm->runtime.exception = EXCEPTION_NONE;
    
    /* 初始化统计信息 */
    vm->stats.instructions_executed = 0;
    vm->total_execution_time_us = 0;
    
    return GCOS_OK;
}

/* ============================================================================
 * API 实现 - 生命周期管理
 * ============================================================================ */

GCOSVM* gcos_vm_create(void) {
    /* 如果已经初始化，返回现有实例 */
    if (g_vm_initialized) {
        printf("[GCOS VM] Warning: VM already created, returning existing instance\n");
        return &g_gcos_vm_instance;
    }
    
    /* 初始化全局实例 */
    GCOSResult result = vm_instance_init(&g_gcos_vm_instance);
    if (result != GCOS_OK) {
        printf("[GCOS VM] Error: Failed to initialize VM (error=%d)\n", result);
        return NULL;
    }
    
    g_vm_initialized = true;
    printf("[GCOS VM] VM created successfully (static allocation)\n");
    
    return &g_gcos_vm_instance;
}

GCOSResult gcos_vm_destroy(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* 检查是否是全局实例 */
    if (vm != &g_gcos_vm_instance) {
        printf("[GCOS VM] Error: Invalid VM instance pointer\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* 停止任何正在执行的操作 */
    if (vm->state == GCOS_STATE_RUNNING) {
        vm->state = GCOS_STATE_IDLE;
    }
    
    /* 清理事务备份数据（如果有）*/
    if (vm->transaction.backup_data != NULL) {
        /* 注意：在零动态内存分配模式下，这里不应该有动态分配的内存 */
        printf("[GCOS VM] Warning: Transaction backup data exists (should not happen in static mode)\n");
        vm->transaction.backup_data = NULL;
        vm->transaction.backup_size = 0;
    }
    
    /* 重置实例但不释放内存（因为是静态分配）*/
    vm_instance_init(vm);
    g_vm_initialized = false;
    
    printf("[GCOS VM] VM destroyed (memory retained for reuse)\n");
    return GCOS_OK;
}

GCOSResult gcos_vm_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* 如果未创建，先创建 */
    if (!g_vm_initialized) {
        if (gcos_vm_create() == NULL) {
            return GCOS_ERR_OUT_OF_MEMORY;
        }
        return GCOS_OK;
    }
    
    /* 如果已经初始化，先重置 */
    return gcos_vm_reset(vm);
}

GCOSResult gcos_vm_reset(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* 保存版本信息 */
    u8 saved_major = vm->version.major;
    u8 saved_minor = vm->version.minor;
    u8 saved_patch = vm->version.patch;
    
    /* 重新初始化 */
    GCOSResult result = vm_instance_init(vm);
    if (result != GCOS_OK) {
        return result;
    }
    
    /* 恢复版本信息 */
    vm->version.major = saved_major;
    vm->version.minor = saved_minor;
    vm->version.patch = saved_patch;
    
    printf("[GCOS VM] VM reset completed\n");
    return GCOS_OK;
}

/* ============================================================================
 * API 实现 - 状态查询
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
 * API 实现 - 统计信息
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
        printf("[GCOS VM] Error: NULL VM pointer\n");
        return;
    }
    
    printf("\n========== GCOS VM Information ==========\n");
    printf("Version: %d.%d.%d\n", 
           vm->version.major, vm->version.minor, vm->version.patch);
    printf("State: %s\n", gcos_vm_state_to_string(vm->state));
    printf("Exception: %s\n", gcos_vm_exception_to_string(vm->runtime.exception));
    printf("\n--- Runtime Context ---\n");
    printf("Stack Pointer: %u / %u\n", 
           vm->runtime.stack_pointer, GCOS_EXECUTOR_STACK_SIZE);
    printf("Indirect Stack Pointer: %u / %u\n",
           vm->runtime.indirect_stack_pointer, GCOS_INDIRECT_STACK_SIZE);
    printf("Global Data Used: %u / %u bytes\n",
           vm->runtime.global_data_used, GCOS_GLOBAL_DATA_SIZE);
    printf("Heap Used: %u / %u bytes\n",
           vm->runtime.heap_used, GCOS_HEAP_SIZE);
    printf("Program Counter: %u\n", vm->runtime.program_counter);
    printf("\n--- Modules & Apps ---\n");
    printf("Module Count: %u / %u\n", 
           vm->module_count, GCOS_MAX_MODULES);
    printf("App Count: %u / %u\n",
           vm->app_count, GCOS_MAX_APPS);
    printf("Current Channel: %u\n", vm->current_channel);
    printf("\n--- Statistics ---\n");
    printf("Instructions Executed: %llu\n", 
           (unsigned long long)vm->stats.instructions_executed);
    printf("Total Execution Time: %llu us\n",
           (unsigned long long)vm->total_execution_time_us);
    printf("=========================================\n\n");
}

/* ============================================================================
 * API 实现 - 内存访问（安全版本）
 * ============================================================================ */

/**
 * @brief 从执行器栈弹出值
 * @param vm VM实例
 * @param value 输出值
 * @return GCOSResult 成功，其他值失败
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
 * @brief 向执行器栈压入值
 * @param vm VM实例
 * @param value 要压入的值
 * @return GCOSResult 成功，其他值失败
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
        printf("[GCOS VM] Heap allocation failed: insufficient space\n");
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
        printf("[GCOS VM] Error: NULL VM pointer\n");
        return;
    }
    
    printf("\n--- Call Stack ---\n");
    printf("Frame Top: %u\n", vm->runtime.frame_top);
    
    for (u32 i = 0; i < vm->runtime.frame_top && i < 64; i++) {
        const GCOSStackFrame *frame = &vm->runtime.frame_stack[i];
        printf("Frame[%u]: ReturnAddr=%u, BasePtr=%u, Size=%u\n",
               i, frame->return_address, frame->base_pointer, frame->frame_size);
    }
    printf("------------------\n\n");
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
        printf("[GCOS VM] Validation Error: Stack pointer out of range\n");
        valid = false;
    }
    
    /* 检查间接栈指针范围 */
    if (vm->runtime.indirect_stack_pointer > GCOS_INDIRECT_STACK_SIZE) {
        printf("[GCOS VM] Validation Error: Indirect stack pointer out of range\n");
        valid = false;
    }
    
    /* 检查全局数据区使用量 */
    if (vm->runtime.global_data_used > GCOS_GLOBAL_DATA_SIZE) {
        printf("[GCOS VM] Validation Error: Global data overflow\n");
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
