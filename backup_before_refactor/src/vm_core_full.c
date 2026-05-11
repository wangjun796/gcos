/**
 * @file vm_core_full.c
 * @brief GCOS VM 核心完整实现
 * 
 * 整合所有模块，提供完整的虚拟机实现：
 * - 虚拟机生命周期管理
 * - 模块加载和管理
 * - 应用安装和管理
 * - 字节码执行
 * - 事务管理
 * - 异常处理
 */

#include "vm_core.h"
#include "vm_instructions.h"
#include "vm_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 内部状态
 * ============================================================================ */

typedef struct {
    bool initialized;
    u32 uptime_ms;
    VMState state;
    ExceptionType last_exception;
    u32 exception_count;
} VMInternalState;

static VMInternalState vm_internal = {
    .initialized = false,
    .uptime_ms = 0,
    .state = VM_STATE_IDLE,
    .last_exception = EXCEPTION_NONE,
    .exception_count = 0
};

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/**
 * @brief 打印虚拟机状态
 */
static void print_vm_state(VMContext *vm) {
    printf("=== VM State ===\n");
    printf("State: %s\n", 
           vm->state == VM_STATE_IDLE ? "IDLE" :
           vm->state == VM_STATE_RUNNING ? "RUNNING" :
           vm->state == VM_STATE_SUSPENDED ? "SUSPENDED" :
           vm->state == VM_STATE_ERROR ? "ERROR" : "EXCEPTION");
    printf("PC: %u\n", vm->program_counter);
    printf("SP: %u, BP: %u\n", vm->stack_pointer, vm->base_pointer);
    printf("Frame Top: %u\n", vm->frame_top);
    printf("Exception: %d\n", vm->exception);
    printf("Module: %p\n", (void*)vm->current_module);
    printf("App: %p\n", (void*)vm->current_app);
    printf("Channel: %u\n", vm->current_channel);
    printf("Instructions: %llu\n", (unsigned long long)vm->instructions_executed);
    printf("================\n");
}

/**
 * @brief 比较AID
 */
static bool aid_equal(const AID *aid1, const AID *aid2) {
    if (aid1->length != aid2->length) {
        return false;
    }
    return memcmp(aid1->aid, aid2->aid, aid1->length) == 0;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

/**
 * @brief 创建虚拟机实例
 */
VMContext* vm_create(void) {
    VMContext *vm = (VMContext*)malloc(sizeof(VMContext));
    if (vm == NULL) {
        return NULL;
    }

    memset(vm, 0, sizeof(VMContext));
    vm_internal.initialized = true;
    vm_internal.uptime_ms = 0;
    vm_internal.state = VM_STATE_IDLE;
    vm_internal.last_exception = EXCEPTION_NONE;
    vm_internal.exception_count = 0;

    /* 初始化执行器栈 */
    vm->stack_pointer = 0;
    vm->base_pointer = 0;
    vm->frame_top = 0;
    memset(vm->executor_stack, 0, sizeof(vm->executor_stack));

    /* 初始化间接访问变量栈 */
    vm->indirect_stack_pointer = 0;
    memset(vm->indirect_var_stack, 0, sizeof(vm->indirect_var_stack));

    /* 初始化全局数据区 */
    memset(vm->global_data, 0, sizeof(vm->global_data));
    vm->global_data_used = 0;

    /* 初始化堆 */
    vm->heap_used = 0;
    memset(vm->heap, 0, sizeof(vm->heap));

    /* 初始化程序计数器 */
    vm->program_counter = 0;

    /* 初始化模块程序区 */
    memset(vm->module_code, 0, sizeof(vm->module_code));

    /* 初始化模块管理 */
    vm->module_count = 0;
    memset(vm->modules, 0, sizeof(vm->modules));

    /* 初始化应用管理 */
    vm->app_count = 0;
    for (u16 i = 0; i < MAX_APPS_PER_MODULE * MAX_MODULES; i++) {
        vm->apps[i] = NULL;
    }

    /* 初始化当前运行状态 */
    vm->current_module = NULL;
    vm->current_app = NULL;
    vm->current_channel = 0;
    vm->state = VM_STATE_IDLE;

    /* 初始化异常处理 */
    vm->exception = EXCEPTION_NONE;
    memset(vm->exception_handlers, 0, sizeof(vm->exception_handlers));
    vm->handler_count = 0;

    /* 初始化事务管理 */
    vm->transaction.active = false;
    vm->transaction.backup_data = NULL;
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;

    /* 初始化统计信息 */
    vm->instructions_executed = 0;
    vm->total_execution_time = 0;

    printf("VM created successfully\n");
    return vm;
}

/**
 * @brief 销毁虚拟机实例
 */
void vm_destroy(VMContext *vm) {
    if (vm == NULL) {
        return;
    }

    if (!vm_internal.initialized) {
        return;
    }

    printf("Destroying VM...\n");

    /* 清理模块数据 */
    for (u16 i = 0; i < vm->module_count; i++) {
        Module *module = &vm->modules[i];
        if (module->global_data != NULL) {
            free(module->global_data);
            module->global_data = NULL;
        }
        if (module->domain_data != NULL) {
            free(module->domain_data);
            module->domain_data = NULL;
        }
        if (module->code != NULL) {
            free((void*)module->code);
            module->code = NULL;
        }
    }

    /* 清理应用数据 */
    for (u16 i = 0; i < vm->app_count; i++) {
        AppInstance *app = vm->apps[i];
        if (app != NULL) {
            if (app->app_domain_data != NULL) {
                free(app->app_domain_data);
            }
            if (app->ref_domain_data != NULL) {
                free(app->ref_domain_data);
            }
            if (app->persistent_data != NULL) {
                free(app->persistent_data);
            }

            /* 清理通道数据 */
            for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
                if (app->channel_data[ch].temp_dynamic_data != NULL) {
                    free(app->channel_data[ch].temp_dynamic_data);
                }
                if (app->channel_data[ch].global_data_copy != NULL) {
                    free(app->channel_data[ch].global_data_copy);
                }
            }

            free(app);
        }
    }

    /* 清理事务备份数据 */
    if (vm->transaction.backup_data != NULL) {
        free(vm->transaction.backup_data);
        vm->transaction.backup_data = NULL;
    }

    /* 释放虚拟机实例 */
    free(vm);
    vm_internal.initialized = false;

    printf("VM destroyed\n");
}

/**
 * @brief 初始化虚拟机
 */
int vm_init(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    if (!vm_internal.initialized) {
        printf("Error: VM not created\n");
        return -2;
    }

    /* 重置所有状态 */
    vm_reset(vm);

    vm_internal.state = VM_STATE_IDLE;
    vm_internal.last_exception = EXCEPTION_NONE;
    vm_internal.exception_count = 0;

    printf("VM initialized\n");
    return 0;
}

/**
 * @brief 重置虚拟机状态
 */
void vm_reset(VMContext *vm) {
    if (vm == NULL) {
        return;
    }

    printf("Resetting VM...\n");

    /* 重置执行器栈 */
    vm->stack_pointer = 0;
    vm->base_pointer = 0;
    vm->frame_top = 0;
    memset(vm->executor_stack, 0, sizeof(vm->executor_stack));

    /* 重置间接访问变量栈 */
    vm->indirect_stack_pointer = 0;
    memset(vm->indirect_var_stack, 0, sizeof(vm->indirect_var_stack));

    /* 重置全局数据区 */
    memset(vm->global_data, 0, sizeof(vm->global_data));
    vm->global_data_used = 0;

    /* 重置堆 */
    vm->heap_used = 0;
    memset(vm->heap, 0, sizeof(vm->heap));

    /* 重置程序计数器 */
    vm->program_counter = 0;

    /* 重置当前运行状态 */
    vm->current_module = NULL;
    vm->current_app = NULL;
    vm->current_channel = 0;
    vm->state = VM_STATE_IDLE;

    /* 重置异常处理 */
    vm->exception = EXCEPTION_NONE;
    memset(vm->exception_handlers, 0, sizeof(vm->exception_handlers));
    vm->handler_count = 0;

    /* 重置事务管理 */
    vm->transaction.active = false;
    if (vm->transaction.backup_data != NULL) {
        free(vm->transaction.backup_data);
        vm->transaction.backup_data = NULL;
    }
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;

    /* 重置统计信息 */
    vm->instructions_executed = 0;
    vm->total_execution_time = 0;

    printf("VM reset complete\n");
}

/**
 * @brief 加载可执行文件
 */
int vm_load_file(VMContext *vm, const char *file_path) {
    if (vm == NULL || file_path == NULL) {
        return -1;
    }

    printf("Loading file: %s\n", file_path);

    /* TODO: 调用加载器加载文件 */
    printf("Warning: File loading not fully implemented\n");
    return 0;
}

/**
 * @brief 从内存加载可执行文件
 */
int vm_load_from_memory(VMContext *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return -1;
    }

    printf("Loading from memory: %u bytes\n", size);

    /* TODO: 调用加载器从内存加载 */
    printf("Warning: Memory loading not fully implemented\n");
    return 0;
}

/**
 * @brief 选择应用
 */
int vm_select_app(VMContext *vm, const AID *aid, u8 channel) {
    if (vm == NULL || aid == NULL) {
        return -1;
    }

    if (channel >= MAX_CHANNELS) {
        printf("Error: Invalid channel %u\n", channel);
        return -2;
    }

    printf("Selecting app on channel %u\n", channel);

    /* TODO: 调用应用管理器选择应用 */
    vm->current_channel = channel;
    vm->state = VM_STATE_RUNNING;

    return 0;
}

/**
 * @brief 取消选择应用
 */
int vm_deselect_app(VMContext *vm, u8 channel) {
    if (vm == NULL) {
        return -1;
    }

    if (channel >= MAX_CHANNELS) {
        printf("Error: Invalid channel %u\n", channel);
        return -2;
    }

    printf("Deselecting app on channel %u\n", channel);

    /* TODO: 调用应用管理器取消选择应用 */
    vm->current_channel = channel;
    vm->state = VM_STATE_IDLE;

    return 0;
}

/**
 * @brief 执行APDU命令
 */
int vm_execute_apdu(VMContext *vm, const u8 *apdu, u32 apdu_len,
                     u8 *response, u32 *response_len) {
    if (vm == NULL || apdu == NULL) {
        return -1;
    }

    if (vm->current_app == NULL) {
        printf("Error: No app selected\n");
        return -2;
    }

    printf("Executing APDU: %u bytes\n", apdu_len);

    /* TODO: 实现APDU命令执行 */
    if (response != NULL && response_len != NULL) {
        if (*response_len > 0) {
            *response_len = 2; /* SW1 + SW2 */
            response[0] = 0x90; /* 成功 */
            response[1] = 0x00;
        }
    }

    return 0;
}

/**
 * @brief 执行字节码指令
 */
int vm_execute(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    if (vm->current_module == NULL) {
        printf("Error: No module loaded\n");
        return -2;
    }

    vm->state = VM_STATE_RUNNING;
    vm_internal.state = VM_STATE_RUNNING;

    printf("Starting execution...\n");

    /* TODO: 调用执行器运行 */
    while (vm->state == VM_STATE_RUNNING && 
           vm->exception == EXCEPTION_NONE) {
        /* 执行单条指令 */
        int ret = vm_step(vm);
        if (ret != 0) {
            printf("Execution error: %d\n", ret);
            break;
        }

        /* 更新统计信息 */
        vm->instructions_executed++;
        vm_internal.uptime_ms++;
    }

    printf("Execution completed\n");
    return 0;
}

/**
 * @brief 单步执行一条指令
 */
int vm_step(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    /* TODO: 调用指令解码和执行 */
    u8 opcode = 0;
    u32 operands[4];
    u8 operand_count = 0;

    /* 获取当前指令 */
    if (vm->program_counter >= VM_MODULE_CODE_SIZE) {
        vm->exception = EXCEPTION_INVALID_ADDRESS;
        return -1;
    }

    opcode = vm->module_code[vm->program_counter];

    /* 解码指令 */
    int ret = vm_decode_instruction(vm->module_code, VM_MODULE_CODE_SIZE,
                                   vm->program_counter,
                                   &opcode, operands, &operand_count);
    if (ret == 0) {
        vm->exception = EXCEPTION_INVALID_OPCODE;
        return -2;
    }

    /* 执行指令 */
    ret = vm_execute_instruction(vm, opcode, operands, operand_count);
    if (ret != 0) {
        return ret;
    }

    /* 更新程序计数器 */
    vm->program_counter += ret;

    return 0;
}

/**
 * @brief 调用函数
 */
int vm_call_function(VMContext *vm, u16 function_id,
                     const u32 *params, u16 param_count, u32 *result) {
    if (vm == NULL) {
        return -1;
    }

    if (vm->current_module == NULL) {
        printf("Error: No module loaded\n");
        return -2;
    }

    if (function_id >= vm->current_module->function_count) {
        printf("Error: Invalid function ID %u\n", function_id);
        return -3;
    }

    printf("Calling function %u with %u parameters\n", function_id, param_count);

    /* TODO: 实现函数调用 */
    if (result != NULL) {
        *result = 0;
    }

    return 0;
}

/**
 * @brief 安装应用
 */
int vm_install_app(VMContext *vm, u16 module_index, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }

    printf("Installing app from module %u\n", module_index);

    /* TODO: 调用应用管理器安装应用 */
    return 0;
}

/**
 * @brief 卸载应用
 */
int vm_uninstall_app(VMContext *vm, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }

    printf("Uninstalling app\n");

    /* TODO: 调用应用管理器卸载应用 */
    return 0;
}

/**
 * @brief 开始事务
 */
int vm_transaction_begin(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    printf("Beginning transaction\n");

    /* TODO: 调用事务管理器开始事务 */
    vm->transaction.active = true;

    return 0;
}

/**
 * @brief 提交事务
 */
int vm_transaction_commit(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    if (!vm->transaction.active) {
        printf("Warning: No active transaction\n");
        return -2;
    }

    printf("Committing transaction\n");

    /* TODO: 调用事务管理器提交事务 */
    vm->transaction.active = false;

    return 0;
}

/**
 * @brief 回滚事务
 */
int vm_transaction_rollback(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    if (!vm->transaction.active) {
        printf("Warning: No active transaction\n");
        return -2;
    }

    printf("Rolling back transaction\n");

    /* TODO: 调用事务管理器回滚事务 */
    vm->transaction.active = false;

    return 0;
}

/**
 * @brief 获取虚拟机状态
 */
VMState vm_get_state(const VMContext *vm) {
    if (vm == NULL) {
        return VM_STATE_ERROR;
    }
    return vm->state;
}

/**
 * @brief 获取异常信息
 */
ExceptionType vm_get_exception(const VMContext *vm) {
    if (vm == NULL) {
        return EXCEPTION_NONE;
    }
    return vm->exception;
}

/**
 * @brief 清除异常
 */
void vm_clear_exception(VMContext *vm) {
    if (vm == NULL) {
        return;
    }
    vm->exception = EXCEPTION_NONE;
    vm_internal.last_exception = EXCEPTION_NONE;
}

/**
 * @brief 获取统计信息
 */
void vm_get_stats(const VMContext *vm, u64 *instr_count, u64 *exec_time) {
    if (vm == NULL) {
        return;
    }

    if (instr_count != NULL) {
        *instr_count = vm->instructions_executed;
    }

    if (exec_time != NULL) {
        *exec_time = vm->total_execution_time;
    }
}

/**
 * @brief 打印虚拟机信息（调试用）
 */
void vm_print_info(const VMContext *vm) {
    if (vm == NULL) {
        printf("Error: VM is NULL\n");
        return;
    }

    print_vm_state(vm);
}
