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
#include "gcos_app_manager.h"  // ⭐ Application manager
#include "gcos_system_objects.h"  // ⭐ System objects management (Flash persistence)
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
    // Note: app_count will be set by app_manager_init()
    vm->current_module_index = GCOS_INVALID_INDEX;
    
    /* Initialize channel management */
    vm->current_channel = 0;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        vm->channels[i].selected_app = NULL;
        vm->channels[i].active = false;
    }
    
    /* Initialize selected application */
    vm->selected_app = NULL;
    
    /* Initialize state */
    vm->state = GCOS_STATE_IDLE;
    vm->runtime.exception = EXCEPTION_NONE;
    
    /* Initialize statistics */
    vm->stats.instructions_executed = 0;
    vm->total_execution_time_us = 0;
    
    /* ⭐ Initialize application manager (creates ISD) */
    GCOSResult result = app_manager_init(vm);
    if (result != GCOS_SUCCESS) {
        printf("[VM_INIT] ERROR: Failed to initialize application manager\n");
        return result;
    }
    
    /* ⭐ RE-ENABLED: System objects initialization with detailed debugging */
    SYS_OBJ_INFO("Initializing system objects...\n");
    result = gcos_system_objects_init(vm);
    if (result != GCOS_SUCCESS) {
        SYS_OBJ_ERR("Failed to initialize system objects (error=%d)\n", result);
        return result;
    }
    SYS_OBJ_INFO("System objects initialized successfully\n");
    
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
    
    printf("[VM_DESTROY] === Starting VM Destruction ===\n");
    printf("[VM_DESTROY] VM pointer: %p\n", (void*)vm);
    printf("[VM_DESTROY] Is global instance: %s\n", (vm == &g_gcos_vm_instance) ? "YES" : "NO");
    fflush(stdout);
    
    /* Check if this is the global instance */
    if (vm != &g_gcos_vm_instance) {
        GCOS_PRINTF("[GCOS VM] Error: Invalid VM instance pointer\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    /* Stop any ongoing operations */
    if (vm->state == GCOS_STATE_RUNNING) {
        vm->state = GCOS_STATE_IDLE;
    }
    printf("[VM_DESTROY] VM state set to IDLE\n");
    fflush(stdout);
    
    /* ⭐ TEMPORARILY DISABLED: Save system objects (debugging crash) */
    // GCOSResult result = gcos_system_objects_save(vm);
    // if (result != GCOS_SUCCESS) {
    //     printf("[VM_DESTROY] WARNING: Failed to save system objects (error=%d)\n", result);
    //     /* Continue with destruction even if save fails */
    // }
    
    /* Clean up transaction backup data (if any) */
    if (vm->transaction.backup_data != NULL) {
        /* Note: In zero dynamic memory allocation mode, there should be no dynamically allocated memory here */
        GCOS_PRINTF("[GCOS VM] Warning: Transaction backup data exists (should not happen in static mode)\n");
        vm->transaction.backup_data = NULL;
        vm->transaction.backup_size = 0;
    }
    printf("[VM_DESTROY] Transaction cleanup done\n");
    fflush(stdout);
    
    /* Reset instance but do not release memory (statically allocated) */
    printf("[VM_DESTROY] Calling vm_instance_init() to reset...\n");
    fflush(stdout);
    vm_instance_init(vm);
    printf("[VM_DESTROY] vm_instance_init() returned\n");
    fflush(stdout);
    
    g_vm_initialized = false;
    printf("[VM_DESTROY] g_vm_initialized set to false\n");
    fflush(stdout);
    
    GCOS_PRINTF("[GCOS VM] VM destroyed (memory retained for reuse)\n");
    printf("[VM_DESTROY] === VM Destruction Complete ===\n");
    fflush(stdout);
    return GCOS_OK;
}

GCOSResult gcos_vm_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* If already initialized, just return success (don't re-initialize) */
    if (g_vm_initialized) {
        GCOS_PRINTF("[GCOS VM] Warning: VM already initialized, skipping re-initialization\n");
        return GCOS_OK;
    }
    
    /* If not created, create first */
    if (gcos_vm_create() == NULL) {
        return GCOS_ERR_OUT_OF_MEMORY;
    }
    
    return GCOS_OK;
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
 * API Implementation - Module Lookup
 * ============================================================================ */

GCOSModule* gcos_vm_find_module_by_id(GCOSVM *vm, u8 module_id) {
    if (vm == NULL) {
        return NULL;
    }
    
    for (u8 i = 0; i < vm->module_count; i++) {
        if (vm->modules[i].module_id == module_id) {
            return &vm->modules[i];
        }
    }
    return NULL;
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
 * @brief Allocate memory from heap (returns offset address)
 * @param vm VM instance
 * @param size Allocation size
 * @return Offset address, 0 indicates failure
 * @note COS3 specification: Heap is non-volatile, requires eflash library integration
 */
u32 gcos_vm_heap_alloc(GCOSVM *vm, u32 size) {
    if (vm == NULL || size == 0) {
        return 0;
    }
    
    /* Align to 4-byte boundary */
    u32 aligned_size = (size + 3) & ~3;
    
    /* Check if there is enough space */
    if (vm->runtime.heap_used + aligned_size > GCOS_HEAP_SIZE) {
        GCOS_PRINTF("[GCOS VM] Heap allocation failed: insufficient space\n");
        return 0;
    }
    
    /* Allocate and return offset address */
    u32 addr = vm->runtime.heap_used;
    vm->runtime.heap_used += aligned_size;
    
    /* Clear allocated memory */
    memset(&vm->runtime.heap[addr], 0, aligned_size);
    
    return addr;
}

/**
 * @brief Free heap memory (simplified version, should use mark-sweep in production)
 * @param vm VM instance
 * @param addr Offset address
 * @return GCOSResult Success, other values indicate failure
 * @note TODO: Implement complete heap manager
 */
GCOSResult gcos_vm_heap_free(GCOSVM *vm, u32 addr) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Simplified implementation: only mark as available */
    /* TODO: Implement complete free list or bitmap management */
    
    return GCOS_OK;
}

/* ============================================================================
 * Debug Helper Functions
 * ============================================================================ */

/**
 * @brief Print call stack
 * @param vm VM instance
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
 * @brief Validate VM state consistency
 * @param vm VM instance
 * @return true if consistent, false if inconsistent
 */
bool gcos_vm_validate(const GCOSVM *vm) {
    if (vm == NULL) {
        return false;
    }
    
    bool valid = true;
    
    /* Check stack pointer range */
    if (vm->runtime.stack_pointer > GCOS_EXECUTOR_STACK_SIZE) {
        GCOS_PRINTF("[GCOS VM] Validation Error: Stack pointer out of range\n");
        valid = false;
    }
    
    /* Check indirect stack pointer range */
    if (vm->runtime.indirect_stack_pointer > GCOS_INDIRECT_STACK_SIZE) {
        GCOS_PRINTF("[GCOS VM] Validation Error: Indirect stack pointer out of range\n");
        valid = false;
    }
    
    /* Check global data area usage */
    if (vm->runtime.global_data_used > GCOS_GLOBAL_DATA_SIZE) {
        GCOS_PRINTF("[GCOS VM] Validation Error: Global data overflow\n");
        valid = false;
    }
    
    /* Check heap usage */
    if (vm->runtime.heap_used > GCOS_HEAP_SIZE) {
        printf("[GCOS VM] Validation Error: Heap overflow\n");
        valid = false;
    }
    
    /* Check module count */
    if (vm->module_count > GCOS_MAX_MODULES) {
        printf("[GCOS VM] Validation Error: Too many modules\n");
        valid = false;
    }
    
    /* Check application count */
    if (vm->app_count > GCOS_MAX_APPS) {
        printf("[GCOS VM] Validation Error: Too many apps\n");
        valid = false;
    }
    
    return valid;
}

/* ============================================================================
 * Version Information
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
