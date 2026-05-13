/**
 * @file gcos_executor.c
 * @brief GCOS VM Execution Engine Implementation
 * 
 * Stack-based VM execution engine based on COS3 specification:
 * - Fetch-Decode-Execute cycle
 * - Pop-Execute-Push model
 * - Exception handling
 * - Performance statistics
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * Internal State (Statically Allocated)
 * ============================================================================ */

/**
 * @brief Executor internal state
 */
typedef struct {
    bool running;           /**< Is running */
    bool paused;            /**< Is paused */
    u32 breakpoints[16];    /**< Breakpoint array */
    u8 breakpoint_count;    /**< Breakpoint count */
    
    /* Performance statistics */
    u64 instruction_count;  /**< Instruction count */
    u64 start_time_us;      /**< Start time (microseconds) */
    u32 stack_peak;         /**< Stack usage peak */
} ExecutorState;

/**
 * @brief Global executor state (singleton)
 */
static ExecutorState g_executor_state = {
    .running = false,
    .paused = false,
    .breakpoint_count = 0,
    .instruction_count = 0,
    .stack_peak = 0
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 * @return Current timestamp
 * @note Simplified implementation, should use platform-specific high-precision timer in production
 */
static u64 get_current_time_us(void) {
    /* TODO: Implement platform-specific high-precision timing */
    return 0;
}

/**
 * @brief Check breakpoint
 * @param vm VM instance
 * @param pc Program counter
 * @return true if breakpoint hit, false to continue execution
 */
static bool check_breakpoint(GCOSVM *vm, u32 pc) {
    if (g_executor_state.breakpoint_count == 0) {
        return false;
    }
    
    for (u8 i = 0; i < g_executor_state.breakpoint_count; i++) {
        if (g_executor_state.breakpoints[i] == pc) {
            GCOS_PRINTF("[GCOS Executor] Breakpoint hit at PC=%u\n", pc);
            g_executor_state.paused = true;
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Update stack usage peak
 * @param vm VM instance
 */
static void update_stack_peak(GCOSVM *vm) {
    if (vm->runtime.stack_pointer > g_executor_state.stack_peak) {
        g_executor_state.stack_peak = vm->runtime.stack_pointer;
    }
}

/**
 * @brief Fetch instruction
 * @param vm VM instance
 * @return Opcode, 0xFF indicates error
 */
static u8 fetch_instruction(GCOSVM *vm) {
    if (vm == NULL || vm->current_module_index == GCOS_INVALID_INDEX) {
        return 0xFF;
    }
    
    /* TODO: Fetch instruction from module code area */
    /* Currently returns placeholder */
    return 0x00;
}

/**
 * @brief Decode instruction
 * @param vm VM instance
 * @param opcode Opcode
 * @param operands Output operand array
 * @param operand_count Output operand count
 * @return GCOS_SUCCESS on success, other values indicate failure
 */
static GCOSResult decode_instruction(GCOSVM *vm, u8 opcode, 
                                      u32 *operands, u8 *operand_count) {
    if (vm == NULL || operands == NULL || operand_count == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* TODO: Decode instruction based on opcode */
    /* Currently returns placeholder */
    *operand_count = 0;
    
    return GCOS_SUCCESS;
}

/**
 * @brief Execute instruction
 * @param vm VM instance
 * @param opcode Opcode
 * @param operands Operand array
 * @param operand_count Operand count
 * @return GCOS_SUCCESS on success, other values indicate failure
 */
static GCOSResult execute_instruction(GCOSVM *vm, u8 opcode,
                                       const u32 *operands, u8 operand_count) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* TODO: Execute instruction based on opcode */
    /* Currently returns placeholder */
    
    /* Update statistics */
    vm->stats.instructions_executed++;
    g_executor_state.instruction_count++;
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Execution Control
 * ============================================================================ */

GCOSResult gcos_executor_start(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (vm->state != GCOS_STATE_IDLE && vm->state != GCOS_STATE_SUSPENDED) {
        GCOS_PRINTF("[GCOS Executor] Error: Invalid VM state for start\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    g_executor_state.running = true;
    g_executor_state.paused = false;
    g_executor_state.start_time_us = get_current_time_us();
    g_executor_state.instruction_count = 0;
    g_executor_state.stack_peak = 0;
    
    vm->state = GCOS_STATE_RUNNING;
    
    GCOS_PRINTF("[GCOS Executor] Execution started\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_executor_stop(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    g_executor_state.running = false;
    vm->state = GCOS_STATE_IDLE;
    
    GCOS_PRINTF("[GCOS Executor] Execution stopped\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_executor_pause(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (!g_executor_state.running) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    g_executor_state.paused = true;
    vm->state = GCOS_STATE_SUSPENDED;
    
    GCOS_PRINTF("[GCOS Executor] Execution paused\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_executor_resume(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (vm->state != GCOS_VM_STATE_SUSPENDED) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    g_executor_state.paused = false;
    vm->state = GCOS_STATE_RUNNING;
    
    GCOS_PRINTF("[GCOS Executor] Execution resumed\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_executor_run(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Start execution */
    GCOSResult result = gcos_executor_start(vm);
    if (result != GCOS_SUCCESS) {
        return result;
    }
    
    /* Main execution loop */
    while (g_executor_state.running && !g_executor_state.paused) {
        /* Check exceptions */
        if (vm->runtime.exception != EXCEPTION_NONE) {
            GCOS_PRINTF("[GCOS Executor] Exception detected: %s\n",
                   gcos_vm_exception_to_string(vm->runtime.exception));
            vm->state = GCOS_STATE_EXCEPTION;
            break;
        }
        
        /* Fetch instruction */
        u8 opcode = fetch_instruction(vm);
        if (opcode == 0xFF) {
            GCOS_PRINTF("[GCOS Executor] Failed to fetch instruction\n");
            vm->runtime.exception = EXCEPTION_INVALID_OPCODE;
            vm->state = GCOS_STATE_EXCEPTION;
            break;
        }
        
        /* Check breakpoint */
        if (check_breakpoint(vm, vm->runtime.program_counter)) {
            break;
        }
        
        /* Decode */
        u32 operands[4];
        u8 operand_count = 0;
        result = decode_instruction(vm, opcode, operands, &operand_count);
        if (result != GCOS_SUCCESS) {
            GCOS_PRINTF("[GCOS Executor] Failed to decode instruction\n");
            vm->runtime.exception = EXCEPTION_INVALID_OPCODE;
            vm->state = GCOS_STATE_EXCEPTION;
            break;
        }
        
        /* Execute */
        result = execute_instruction(vm, opcode, operands, operand_count);
        if (result != GCOS_SUCCESS) {
            GCOS_PRINTF("[GCOS Executor] Failed to execute instruction\n");
            break;
        }
        
        /* Update stack peak */
        update_stack_peak(vm);
        
        /* Update program counter */
        vm->runtime.program_counter++;
    }
    
    /* Stop execution */
    gcos_executor_stop(vm);
    
    /* Calculate execution time */
    u64 end_time_us = get_current_time_us();
    vm->total_execution_time_us = end_time_us - g_executor_state.start_time_us;
    
    GCOS_PRINTF("[GCOS Executor] Execution completed: %llu instructions\n",
           (unsigned long long)g_executor_state.instruction_count);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_executor_step(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    /* Single-step one instruction */
    /* TODO: Implement single-stepping */
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Breakpoint Management
 * ============================================================================ */

GCOSResult gcos_executor_add_breakpoint(GCOSVM *vm, u32 address) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    if (g_executor_state.breakpoint_count >= 16) {
        GCOS_PRINTF("[GCOS Executor] Error: Too many breakpoints\n");
        return GCOS_ERROR_BREAKPOINT_LIMIT;
    }
    
    g_executor_state.breakpoints[g_executor_state.breakpoint_count] = address;
    g_executor_state.breakpoint_count++;
    
    GCOS_PRINTF("[GCOS Executor] Breakpoint added at address %u\n", address);
    return GCOS_SUCCESS;
}

GCOSResult gcos_executor_remove_breakpoint(GCOSVM *vm, u32 address) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    for (u8 i = 0; i < g_executor_state.breakpoint_count; i++) {
        if (g_executor_state.breakpoints[i] == address) {
            /* Remove breakpoint */
            for (u8 j = i; j < g_executor_state.breakpoint_count - 1; j++) {
                g_executor_state.breakpoints[j] = g_executor_state.breakpoints[j + 1];
            }
            g_executor_state.breakpoint_count--;
            
            GCOS_PRINTF("[GCOS Executor] Breakpoint removed at address %u\n", address);
            return GCOS_SUCCESS;
        }
    }
    
    GCOS_PRINTF("[GCOS Executor] Warning: Breakpoint not found at address %u\n", address);
    return GCOS_ERROR_NOT_FOUND;
}

GCOSResult gcos_executor_clear_breakpoints(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_NULL_POINTER;
    }
    
    g_executor_state.breakpoint_count = 0;
    memset(g_executor_state.breakpoints, 0, sizeof(g_executor_state.breakpoints));
    
    printf("[GCOS Executor] All breakpoints cleared\n");
    return GCOS_SUCCESS;
}

/* ============================================================================
 * API Implementation - Exception Handling
 * ============================================================================ */

void gcos_executor_throw_exception(GCOSVM *vm, GCOSExceptionType exception,
                                    const char *message) {
    if (vm == NULL) {
        return;
    }
    
    vm->runtime.exception = exception;
    vm->state = GCOS_STATE_EXCEPTION;
    
    if (message != NULL) {
        printf("[GCOS Executor] Exception thrown: %s - %s\n",
               gcos_vm_exception_to_string(exception), message);
    } else {
        printf("[GCOS Executor] Exception thrown: %s\n",
               gcos_vm_exception_to_string(exception));
    }
}

void gcos_executor_clear_exception(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    vm->runtime.exception = EXCEPTION_NONE;
    if (vm->state == GCOS_STATE_EXCEPTION) {
        vm->state = GCOS_STATE_IDLE;
    }
}

/* ============================================================================
 * API Implementation - Debug Support
 * ============================================================================ */

void gcos_executor_set_trace(GCOSVM *vm, bool enable) {
    if (vm == NULL) {
        return;
    }
    
    /* TODO: Implement instruction tracing */
    printf("[GCOS Executor] Trace %s\n", enable ? "enabled" : "disabled");
}

void gcos_executor_print_current_instruction(const GCOSVM *vm) {
    if (vm == NULL) {
        printf("[GCOS Executor] Error: NULL VM pointer\n");
        return;
    }
    
    printf("[GCOS Executor] Current PC: %u\n", vm->runtime.program_counter);
    /* TODO: Print current instruction details */
}

void gcos_executor_get_stats(const GCOSVM *vm, u64 *instruction_count,
                              u64 *execution_time_us, u32 *stack_peak) {
    if (vm == NULL) {
        return;
    }
    
    if (instruction_count != NULL) {
        *instruction_count = vm->stats.instructions_executed;
    }
    
    if (execution_time_us != NULL) {
        *execution_time_us = vm->total_execution_time_us;
    }
    
    if (stack_peak != NULL) {
        *stack_peak = g_executor_state.stack_peak;
    }
}

void gcos_executor_reset_stats(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    g_executor_state.instruction_count = 0;
    g_executor_state.stack_peak = 0;
    vm->stats.instructions_executed = 0;
    vm->total_execution_time_us = 0;
}

/* ============================================================================
 * Profiling Support
 * ============================================================================ */

void gcos_executor_start_profiling(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    g_executor_reset_stats(vm);
    printf("[GCOS Executor] Profiling started\n");
}

void gcos_executor_stop_profiling(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    printf("[GCOS Executor] Profiling stopped\n");
    printf("  Instructions: %llu\n", 
           (unsigned long long)g_executor_state.instruction_count);
    printf("  Execution Time: %llu us\n",
           (unsigned long long)vm->total_execution_time_us);
    printf("  Stack Peak: %u\n", g_executor_state.stack_peak);
    
    if (g_executor_state.instruction_count > 0 && vm->total_execution_time_us > 0) {
        double instr_per_sec = (double)g_executor_state.instruction_count / 
                               ((double)vm->total_execution_time_us / 1000000.0);
        printf("  Performance: %.2f instr/sec\n", instr_per_sec);
    }
}
