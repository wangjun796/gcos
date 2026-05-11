#include "vm_executor.h"
#include "vm_instructions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 内部结构
 * ============================================================================ */

typedef struct {
    ExecutorConfig config;
    bool running;
    bool paused;
    u32 breakpoints[16];
    u8 breakpoint_count;
    
    /* 性能统计 */
    u64 instruction_count;
    u64 start_time_us;
    u32 stack_peak;
    
    /* 函数调用统计 */
    u32 function_call_counts[MAX_FUNCTIONS];
} ExecutorState;

static ExecutorState executor_state;

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

static u64 get_current_time_us(void) {
    /* 简化实现：返回0 */
    /* 实际应该使用平台相关的高精度计时器 */
    return 0;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

int vm_executor_init(VMContext *vm, const ExecutorConfig *config) {
    if (vm == NULL) {
        return -1;
    }
    
    memset(&executor_state, 0, sizeof(ExecutorState));
    
    /* 设置默认配置 */
    if (config != NULL) {
        memcpy(&executor_state.config, config, sizeof(ExecutorConfig));
    } else {
        executor_state.config.enable_debug = false;
        executor_state.config.enable_trace = false;
        executor_state.config.enable_profiling = false;
        executor_state.config.max_instructions = 0;
        executor_state.config.max_execution_time = 0;
    }
    
    executor_state.running = false;
    executor_state.paused = false;
    executor_state.breakpoint_count = 0;
    executor_state.instruction_count = 0;
    executor_state.stack_peak = 0;
    
    return 0;
}

int vm_executor_start(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    executor_state.running = true;
    executor_state.paused = false;
    executor_state.start_time_us = get_current_time_us();
    
    return 0;
}

void vm_executor_stop(VMContext *vm) {
    (void)vm;
    executor_state.running = false;
}

void vm_executor_pause(VMContext *vm) {
    (void)vm;
    executor_state.paused = true;
}

void vm_executor_resume(VMContext *vm) {
    (void)vm;
    executor_state.paused = false;
}

int vm_executor_run(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    if (!executor_state.running) {
        return -2;
    }
    
    /* 主执行循环 */
    while (executor_state.running && !executor_state.paused) {
        /* 检查异常 */
        if (vm->exception != EXCEPTION_NONE) {
            return -3;
        }
        
        /* 检查指令数限制 */
        if (executor_state.config.max_instructions > 0 &&
            executor_state.instruction_count >= executor_state.config.max_instructions) {
            return -4; /* 超出指令限制 */
        }
        
        /* 检查是否到达代码末尾 */
        if (vm->program_counter >= VM_MODULE_CODE_SIZE) {
            break; /* 正常结束 */
        }
        
        /* 单步执行 */
        int ret = vm_executor_step(vm);
        if (ret != 0) {
            return ret;
        }
    }
    
    return 0;
}

int vm_executor_step(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    /* 获取当前指令 */
    u32 pc = vm->program_counter;
    u8 *code = vm->module_code;
    
    /* 解码指令 */
    u8 opcode;
    u32 operands[4];
    u8 operand_count;
    
    int instr_len = vm_decode_instruction(code, VM_MODULE_CODE_SIZE, pc,
                                          &opcode, operands, &operand_count);
    if (instr_len == 0) {
        vm->exception = EXCEPTION_INVALID_OPCODE;
        return -1;
    }
    
    /* 打印调试信息 */
    if (executor_state.config.enable_trace) {
        printf("[%llu] PC=%u: ", 
               (unsigned long long)executor_state.instruction_count, pc);
        vm_print_instruction(opcode, operands, operand_count);
        printf("\n");
    }
    
    /* 执行指令 */
    int ret = vm_execute_instruction(vm, opcode, operands, operand_count);
    if (ret != 0) {
        return ret;
    }
    
    /* 更新程序计数器 */
    vm->program_counter += instr_len;
    
    /* 更新统计信息 */
    executor_state.instruction_count++;
    vm->instructions_executed++;
    
    /* 更新栈峰值 */
    u32 current_depth = vm_stack_depth(vm);
    if (current_depth > executor_state.stack_peak) {
        executor_state.stack_peak = current_depth;
    }
    
    return 0;
}

int vm_executor_set_breakpoint(VMContext *vm, u32 address) {
    (void)vm;
    
    if (executor_state.breakpoint_count >= 16) {
        return -1; /* 断点已满 */
    }
    
    executor_state.breakpoints[executor_state.breakpoint_count++] = address;
    return 0;
}

int vm_executor_clear_breakpoint(VMContext *vm, u32 address) {
    (void)vm;
    
    for (u8 i = 0; i < executor_state.breakpoint_count; i++) {
        if (executor_state.breakpoints[i] == address) {
            /* 移除断点 */
            for (u8 j = i; j < executor_state.breakpoint_count - 1; j++) {
                executor_state.breakpoints[j] = executor_state.breakpoints[j + 1];
            }
            executor_state.breakpoint_count--;
            return 0;
        }
    }
    
    return -1; /* 断点不存在 */
}

bool vm_executor_is_breakpoint(const VMContext *vm) {
    (void)vm;
    
    for (u8 i = 0; i < executor_state.breakpoint_count; i++) {
        if (executor_state.breakpoints[i] == vm->program_counter) {
            return true;
        }
    }
    return false;
}

int vm_executor_push_frame(VMContext *vm, u32 return_address, u32 frame_size) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->frame_top >= 64) {
        return -2; /* 栈帧溢出 */
    }
    
    StackFrame *frame = &vm->frame_stack[vm->frame_top];
    frame->return_address = return_address;
    frame->base_pointer = vm->base_pointer;
    frame->frame_size = frame_size;
    frame->local_vars_offset = vm->stack_pointer;
    
    vm->base_pointer = vm->stack_pointer;
    vm->frame_top++;
    
    return 0;
}

int vm_executor_pop_frame(VMContext *vm, u32 *return_address) {
    if (vm == NULL || return_address == NULL) {
        return -1;
    }
    
    if (vm->frame_top == 0) {
        return -2; /* 没有栈帧 */
    }
    
    vm->frame_top--;
    StackFrame *frame = &vm->frame_stack[vm->frame_top];
    
    *return_address = frame->return_address;
    vm->base_pointer = frame->base_pointer;
    
    /* 清理栈帧 */
    vm->stack_pointer = frame->local_vars_offset;
    
    return 0;
}

int vm_executor_get_current_frame(const VMContext *vm, StackFrame **frame) {
    if (vm == NULL || frame == NULL) {
        return -1;
    }
    
    if (vm->frame_top == 0) {
        return -2; /* 没有栈帧 */
    }
    
    *frame = &vm->frame_stack[vm->frame_top - 1];
    return 0;
}

u32 vm_executor_get_call_depth(const VMContext *vm) {
    if (vm == NULL) {
        return 0;
    }
    return vm->frame_top;
}

int vm_executor_call_function(VMContext *vm, u16 function_id) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->current_module == NULL) {
        return -2;
    }
    
    if (function_id >= vm->current_module->function_count) {
        return -3; /* 函数不存在 */
    }
    
    FunctionHeader *func = &vm->current_module->functions[function_id];
    
    /* 保存返回地址 */
    u32 return_addr = vm->program_counter;
    
    /* 创建新栈帧 */
    int ret = vm_executor_push_frame(vm, return_addr, func->max_stack_depth);
    if (ret != 0) {
        return ret;
    }
    
    /* 跳转到函数代码 */
    vm->program_counter = func->code_offset;
    
    /* 更新统计 */
    if (function_id < MAX_FUNCTIONS) {
        executor_state.function_call_counts[function_id]++;
    }
    
    return 0;
}

int vm_executor_return_from_function(VMContext *vm, u32 return_value) {
    if (vm == NULL) {
        return -1;
    }
    
    u32 return_addr;
    int ret = vm_executor_pop_frame(vm, &return_addr);
    if (ret != 0) {
        return ret;
    }
    
    /* 将返回值压入栈 */
    vm_stack_push(vm, return_value);
    
    /* 跳转回调用处 */
    vm->program_counter = return_addr;
    
    return 0;
}

int vm_executor_call_trap(VMContext *vm, u16 trap_id,
                          const u32 *params, u8 param_count) {
    if (vm == NULL) {
        return -1;
    }
    
    /* TRAP指令用于调用系统接口 */
    /* 这里应该根据trap_id调用相应的系统功能 */
    /* 简化实现：返回错误 */
    
    printf("TRAP %u called with %u params\n", trap_id, param_count);
    
    return 0;
}

void vm_executor_throw_exception(VMContext *vm, ExceptionType exception,
                                 const char *message) {
    if (vm == NULL) {
        return;
    }
    
    vm->exception = exception;
    
    if (executor_state.config.enable_debug && message != NULL) {
        printf("Exception thrown: %s (type=%d)\n", message, exception);
    }
}

int vm_executor_catch_exception(VMContext *vm, u32 handler_address) {
    if (vm == NULL) {
        return -1;
    }
    
    /* 清除异常状态 */
    vm->exception = EXCEPTION_NONE;
    
    /* 跳转到异常处理器 */
    vm->program_counter = handler_address;
    
    return 0;
}

int vm_executor_register_handler(VMContext *vm, ExceptionType exception_type,
                                 u32 handler_address) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->handler_count >= 16) {
        return -2; /* 处理器已满 */
    }
    
    vm->exception_handlers[vm->handler_count++] = handler_address;
    return 0;
}

void vm_executor_clear_handler(VMContext *vm, ExceptionType exception_type) {
    (void)vm;
    (void)exception_type;
    /* 简化实现 */
}

int vm_executor_enter_try(VMContext *vm, u32 catch_address) {
    if (vm == NULL) {
        return -1;
    }
    
    /* 注册CATCH块地址 */
    return vm_executor_register_handler(vm, EXCEPTION_NONE, catch_address);
}

int vm_executor_exit_try(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    /* 移除最后一个异常处理器 */
    if (vm->handler_count > 0) {
        vm->handler_count--;
    }
    
    return 0;
}

void vm_executor_set_trace(VMContext *vm, bool enable) {
    (void)vm;
    executor_state.config.enable_trace = enable;
}

void vm_executor_print_current_instruction(const VMContext *vm) {
    if (vm == NULL) {
        return;
    }
    
    u8 opcode;
    u32 operands[4];
    u8 operand_count;
    
    vm_decode_instruction(vm->module_code, VM_MODULE_CODE_SIZE,
                         vm->program_counter, &opcode, operands, &operand_count);
    
    printf("PC=%u: ", vm->program_counter);
    vm_print_instruction(opcode, operands, operand_count);
    printf("\n");
}

void vm_executor_print_call_stack(const VMContext *vm) {
    if (vm == NULL) {
        return;
    }
    
    printf("=== Call Stack ===\n");
    printf("Depth: %u\n", vm->frame_top);
    
    for (u32 i = 0; i < vm->frame_top; i++) {
        const StackFrame *frame = &vm->frame_stack[i];
        printf("[%u] Return Address: %u, Base: %u, Size: %u\n",
               i, frame->return_address, frame->base_pointer, frame->frame_size);
    }
    printf("==================\n");
}

void vm_executor_get_stats(const VMContext *vm, u64 *instruction_count,
                           u64 *execution_time_us, u32 *stack_peak) {
    (void)vm;
    
    if (instruction_count != NULL) {
        *instruction_count = executor_state.instruction_count;
    }
    if (execution_time_us != NULL) {
        *execution_time_us = get_current_time_us() - executor_state.start_time_us;
    }
    if (stack_peak != NULL) {
        *stack_peak = executor_state.stack_peak;
    }
}

void vm_executor_reset_stats(VMContext *vm) {
    (void)vm;
    executor_state.instruction_count = 0;
    executor_state.start_time_us = get_current_time_us();
    executor_state.stack_peak = 0;
    memset(executor_state.function_call_counts, 0, sizeof(executor_state.function_call_counts));
}

void vm_executor_start_profiling(VMContext *vm) {
    (void)vm;
    executor_state.config.enable_profiling = true;
    vm_executor_reset_stats(vm);
}

void vm_executor_stop_profiling(VMContext *vm) {
    (void)vm;
    executor_state.config.enable_profiling = false;
}

u32 vm_executor_get_function_call_count(const VMContext *vm, u16 function_id) {
    (void)vm;
    
    if (function_id >= MAX_FUNCTIONS) {
        return 0;
    }
    
    return executor_state.function_call_counts[function_id];
}

int vm_executor_get_hot_functions(const VMContext *vm, u16 *functions,
                                  u32 *counts, int max_count) {
    (void)vm;
    
    if (functions == NULL || counts == NULL || max_count <= 0) {
        return 0;
    }
    
    /* 找到调用次数最多的函数 */
    int count = 0;
    for (u16 i = 0; i < MAX_FUNCTIONS && count < max_count; i++) {
        if (executor_state.function_call_counts[i] > 0) {
            functions[count] = i;
            counts[count] = executor_state.function_call_counts[i];
            count++;
        }
    }
    
    return count;
}
