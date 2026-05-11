#include "vm_core.h"
#include "vm_memory.h"
#include "vm_executor.h"
#include "vm_instructions.h"
#include "vm_loader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

static void vm_context_init(VMContext *vm) {
    memset(vm, 0, sizeof(VMContext));
    
    /* 初始化栈指针 */
    vm->stack_pointer = 0;
    vm->base_pointer = 0;
    vm->frame_top = 0;
    vm->indirect_stack_pointer = 0;
    
    /* 初始化程序计数器 */
    vm->program_counter = 0;
    
    /* 初始化堆 */
    vm->heap_used = 0;
    
    /* 初始化状态 */
    vm->state = VM_STATE_IDLE;
    vm->exception = EXCEPTION_NONE;
    vm->current_channel = 0;
    
    /* 初始化模块和应用计数 */
    vm->module_count = 0;
    vm->app_count = 0;
    vm->current_module = NULL;
    vm->current_app = NULL;
    
    /* 初始化事务 */
    vm->transaction.active = false;
    vm->transaction.backup_data = NULL;
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;
    
    /* 初始化统计信息 */
    vm->instructions_executed = 0;
    vm->total_execution_time = 0;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

VMContext* vm_create(void) {
    VMContext *vm = (VMContext*)malloc(sizeof(VMContext));
    if (vm == NULL) {
        return NULL;
    }
    
    vm_context_init(vm);
    
    /* 初始化执行器 */
    if (vm_executor_init(vm, NULL) != 0) {
        free(vm);
        return NULL;
    }
    
    return vm;
}

void vm_destroy(VMContext *vm) {
    if (vm == NULL) {
        return;
    }
    
    /* 停止执行器 */
    vm_executor_stop(vm);
    
    /* 释放事务备份数据 */
    if (vm->transaction.backup_data != NULL) {
        free(vm->transaction.backup_data);
        vm->transaction.backup_data = NULL;
    }
    
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
    }
    
    /* 清理应用数据 */
    for (u16 i = 0; i < vm->app_count; i++) {
        AppInstance *app = &vm->apps[i];
        if (app->app_domain_data != NULL) {
            free(app->app_domain_data);
            app->app_domain_data = NULL;
        }
        if (app->ref_domain_data != NULL) {
            free(app->ref_domain_data);
            app->ref_domain_data = NULL;
        }
        if (app->persistent_data != NULL) {
            free(app->persistent_data);
            app->persistent_data = NULL;
        }
        
        /* 清理通道数据 */
        for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
            if (app->channel_data[ch].temp_dynamic_data != NULL) {
                free(app->channel_data[ch].temp_dynamic_data);
                app->channel_data[ch].temp_dynamic_data = NULL;
            }
            if (app->channel_data[ch].global_data_copy != NULL) {
                free(app->channel_data[ch].global_data_copy);
                app->channel_data[ch].global_data_copy = NULL;
            }
        }
    }
    
    /* 释放虚拟机上下文 */
    free(vm);
}

int vm_init(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    /* 重置所有状态 */
    vm_reset(vm);
    
    return 0;
}

void vm_reset(VMContext *vm) {
    if (vm == NULL) {
        return;
    }
    
    /* 停止执行器 */
    vm_executor_stop(vm);
    
    /* 清除栈 */
    vm->stack_pointer = 0;
    vm->base_pointer = 0;
    vm->frame_top = 0;
    memset(vm->executor_stack, 0, sizeof(vm->executor_stack));
    
    /* 清除间接栈 */
    vm->indirect_stack_pointer = 0;
    memset(vm->indirect_var_stack, 0, sizeof(vm->indirect_var_stack));
    
    /* 清除全局数据区 */
    memset(vm->global_data, 0, sizeof(vm->global_data));
    
    /* 清除堆 */
    vm->heap_used = 0;
    memset(vm->heap, 0, sizeof(vm->heap));
    
    /* 重置程序计数器 */
    vm->program_counter = 0;
    
    /* 重置状态 */
    vm->state = VM_STATE_IDLE;
    vm->exception = EXCEPTION_NONE;
    vm->current_channel = 0;
    vm->current_module = NULL;
    vm->current_app = NULL;
    
    /* 清除事务 */
    vm->transaction.active = false;
    if (vm->transaction.backup_data != NULL) {
        free(vm->transaction.backup_data);
        vm->transaction.backup_data = NULL;
    }
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;
    
    /* 清除异常处理器 */
    vm->handler_count = 0;
    memset(vm->exception_handlers, 0, sizeof(vm->exception_handlers));
    
    /* 重置统计信息 */
    vm->instructions_executed = 0;
    vm->total_execution_time = 0;
}

int vm_load_file(VMContext *vm, const char *file_path) {
    if (vm == NULL || file_path == NULL) {
        return -1;
    }
    
    return vm_loader_load_sef_file(vm, file_path);
}

int vm_load_from_memory(VMContext *vm, const u8 *data, u32 size) {
    if (vm == NULL || data == NULL || size == 0) {
        return -1;
    }
    
    return vm_loader_load_sef_memory(vm, data, size);
}

int vm_select_app(VMContext *vm, const AID *aid, u8 channel) {
    if (vm == NULL || aid == NULL || channel >= MAX_CHANNELS) {
        return -1;
    }
    
    /* 查找匹配的应用 */
    AppInstance *target_app = NULL;
    for (u16 i = 0; i < vm->app_count; i++) {
        if (memcmp(&vm->apps[i].app_aid, aid, sizeof(AID)) == 0) {
            target_app = &vm->apps[i];
            break;
        }
    }
    
    if (target_app == NULL) {
        return -2; /* 应用未找到 */
    }
    
    /* 检查生命周期状态 */
    if (target_app->lifecycle_state != APP_LIFECYCLE_SELECTABLE &&
        target_app->lifecycle_state != APP_LIFECYCLE_SELECTED) {
        return -3; /* 应用不可选择 */
    }
    
    /* 检查通道是否已被占用 */
    if (target_app->channel_data[channel].active) {
        return -4; /* 通道已被占用 */
    }
    
    /* 分配模块全局数据（如果尚未分配） */
    if (target_app->channel_data[channel].global_data_copy == NULL) {
        Module *module = &vm->modules[target_app->module_index];
        u32 global_size = VM_GLOBAL_DATA_SIZE;
        target_app->channel_data[channel].global_data_copy = 
            (u8*)malloc(global_size);
        if (target_app->channel_data[channel].global_data_copy == NULL) {
            return -5; /* 内存不足 */
        }
        
        /* 复制初始全局数据 */
        if (module->global_data != NULL) {
            memcpy(target_app->channel_data[channel].global_data_copy,
                   module->global_data, global_size);
        } else {
            memset(target_app->channel_data[channel].global_data_copy, 
                   0, global_size);
        }
    }
    
    /* 分配临时动态数据（如果尚未分配） */
    if (target_app->channel_data[channel].temp_dynamic_data == NULL) {
        /* 这里应该根据应用的实际需求分配 */
        target_app->channel_data[channel].temp_dynamic_data = 
            (u8*)malloc(256); /* 示例大小 */
        if (target_app->channel_data[channel].temp_dynamic_data == NULL) {
            free(target_app->channel_data[channel].global_data_copy);
            target_app->channel_data[channel].global_data_copy = NULL;
            return -5; /* 内存不足 */
        }
        memset(target_app->channel_data[channel].temp_dynamic_data, 0, 256);
    }
    
    /* 激活通道 */
    target_app->channel_data[channel].active = true;
    target_app->lifecycle_state = APP_LIFECYCLE_SELECTED;
    
    /* 设置当前运行上下文 */
    vm->current_app = target_app;
    vm->current_module = &vm->modules[target_app->module_index];
    vm->current_channel = channel;
    
    /* 调用应用选择接口（函数ID通常为0或1） */
    u32 result = 0;
    int ret = vm_call_function(vm, 0, NULL, 0, &result);
    if (ret != 0) {
        /* 选择失败，回滚 */
        target_app->channel_data[channel].active = false;
        vm->current_app = NULL;
        vm->current_module = NULL;
        return ret;
    }
    
    return 0;
}

int vm_deselect_app(VMContext *vm, u8 channel) {
    if (vm == NULL || channel >= MAX_CHANNELS) {
        return -1;
    }
    
    if (vm->current_app == NULL || vm->current_channel != channel) {
        return -2; /* 没有应用被选择或通道不匹配 */
    }
    
    AppInstance *app = vm->current_app;
    
    /* 调用应用取消选择接口 */
    u32 result = 0;
    vm_call_function(vm, 2, NULL, 0, &result); /* 假设取消选择接口是函数2 */
    
    /* 回收临时动态数据 */
    if (app->channel_data[channel].temp_dynamic_data != NULL) {
        free(app->channel_data[channel].temp_dynamic_data);
        app->channel_data[channel].temp_dynamic_data = NULL;
    }
    
    /* 回收模块全局数据副本 */
    if (app->channel_data[channel].global_data_copy != NULL) {
        free(app->channel_data[channel].global_data_copy);
        app->channel_data[channel].global_data_copy = NULL;
    }
    
    /* 停用通道 */
    app->channel_data[channel].active = false;
    app->lifecycle_state = APP_LIFECYCLE_SELECTABLE;
    
    /* 清除当前运行上下文 */
    vm->current_app = NULL;
    vm->current_module = NULL;
    vm->current_channel = 0;
    
    return 0;
}

int vm_execute_apdu(VMContext *vm, const u8 *apdu, u32 apdu_len,
                    u8 *response, u32 *response_len) {
    if (vm == NULL || apdu == NULL || response == NULL || response_len == NULL) {
        return -1;
    }
    
    if (vm->current_app == NULL) {
        return -2; /* 没有应用被选择 */
    }
    
    /* 调用应用命令执行接口（通常是函数3） */
    u32 params[2];
    params[0] = (u32)(uintptr_t)apdu;
    params[1] = apdu_len;
    
    u32 result = 0;
    int ret = vm_call_function(vm, 3, params, 2, &result);
    
    if (ret != 0) {
        return ret;
    }
    
    /* 这里应该从返回值中获取响应数据和长度 */
    /* 简化处理：假设响应已经写入response缓冲区 */
    *response_len = 2; /* 默认返回状态字SW1 SW2 */
    response[0] = 0x90;
    response[1] = 0x00;
    
    return 0;
}

int vm_execute(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->state == VM_STATE_RUNNING) {
        return -2; /* 已经在运行 */
    }
    
    vm->state = VM_STATE_RUNNING;
    
    /* 启动执行器 */
    int ret = vm_executor_start(vm);
    if (ret != 0) {
        vm->state = VM_STATE_ERROR;
        return ret;
    }
    
    /* 运行主循环 */
    ret = vm_executor_run(vm);
    
    vm->state = (ret == 0) ? VM_STATE_IDLE : VM_STATE_ERROR;
    
    return ret;
}

int vm_step(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    return vm_executor_step(vm);
}

int vm_call_function(VMContext *vm, u16 function_id,
                     const u32 *params, u16 param_count, u32 *result) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->current_module == NULL) {
        return -2; /* 没有加载模块 */
    }
    
    /* 检查函数ID有效性 */
    if (function_id >= vm->current_module->function_count) {
        return -3; /* 函数不存在 */
    }
    
    /* 推送参数到栈 */
    for (u16 i = 0; i < param_count; i++) {
        int ret = vm_stack_push(vm, params[i]);
        if (ret != 0) {
            return ret;
        }
    }
    
    /* 调用函数 */
    int ret = vm_executor_call_function(vm, function_id);
    if (ret != 0) {
        return ret;
    }
    
    /* 执行直到函数返回 */
    ret = vm_execute(vm);
    
    /* 获取返回值 */
    if (result != NULL) {
        vm_stack_pop(vm, result);
    }
    
    return ret;
}

int vm_install_app(VMContext *vm, u16 module_index, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }
    
    if (module_index >= vm->module_count) {
        return -2; /* 模块不存在 */
    }
    
    if (vm->app_count >= MAX_APPS_PER_MODULE * MAX_MODULES) {
        return -3; /* 应用数量已达上限 */
    }
    
    /* 创建新的应用实例 */
    AppInstance *app = &vm->apps[vm->app_count];
    memset(app, 0, sizeof(AppInstance));
    
    memcpy(&app->app_aid, app_aid, sizeof(AID));
    app->module_index = module_index;
    app->lifecycle_state = APP_LIFECYCLE_INSTALLED;
    app->installed = true;
    
    /* 调用应用安装接口 */
    u32 params[1];
    params[0] = vm->app_count;
    u32 result = 0;
    int ret = vm_call_function(vm, 1, params, 1, &result); /* 假设安装接口是函数1 */
    
    if (ret != 0) {
        /* 安装失败，回滚 */
        memset(app, 0, sizeof(AppInstance));
        return ret;
    }
    
    vm->app_count++;
    
    return 0;
}

int vm_uninstall_app(VMContext *vm, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }
    
    /* 查找应用 */
    u16 app_index = 0xFFFF;
    for (u16 i = 0; i < vm->app_count; i++) {
        if (memcmp(&vm->apps[i].app_aid, app_aid, sizeof(AID)) == 0) {
            app_index = i;
            break;
        }
    }
    
    if (app_index == 0xFFFF) {
        return -2; /* 应用不存在 */
    }
    
    AppInstance *app = &vm->apps[app_index];
    
    /* 检查是否有活动的通道 */
    for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
        if (app->channel_data[ch].active) {
            return -3; /* 应用正在使用中 */
        }
    }
    
    /* 调用应用卸载接口 */
    u32 params[1];
    params[0] = app_index;
    u32 result = 0;
    int ret = vm_call_function(vm, 4, params, 1, &result); /* 假设卸载接口是函数4 */
    
    if (ret != 0) {
        return ret;
    }
    
    /* 释放应用数据 */
    if (app->app_domain_data != NULL) {
        free(app->app_domain_data);
        app->app_domain_data = NULL;
    }
    if (app->ref_domain_data != NULL) {
        free(app->ref_domain_data);
        app->ref_domain_data = NULL;
    }
    if (app->persistent_data != NULL) {
        free(app->persistent_data);
        app->persistent_data = NULL;
    }
    
    /* 移除应用（将最后一个应用移到此位置） */
    if (app_index < vm->app_count - 1) {
        memcpy(app, &vm->apps[vm->app_count - 1], sizeof(AppInstance));
    }
    memset(&vm->apps[vm->app_count - 1], 0, sizeof(AppInstance));
    vm->app_count--;
    
    return 0;
}

int vm_transaction_begin(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    if (vm->transaction.active) {
        return -2; /* 事务已在进行中 */
    }
    
    /* 备份需要保护的数据 */
    vm->transaction.backup_size = VM_HEAP_SIZE + VM_GLOBAL_DATA_SIZE;
    vm->transaction.backup_data = (u8*)malloc(vm->transaction.backup_size);
    if (vm->transaction.backup_data == NULL) {
        return -3; /* 内存不足 */
    }
    
    /* 备份堆和全局数据 */
    memcpy(vm->transaction.backup_data, vm->heap, VM_HEAP_SIZE);
    memcpy(vm->transaction.backup_data + VM_HEAP_SIZE, 
           vm->global_data, VM_GLOBAL_DATA_SIZE);
    
    vm->transaction.active = true;
    vm->transaction.checkpoint_count = 0;
    
    return 0;
}

int vm_transaction_commit(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    if (!vm->transaction.active) {
        return -2; /* 没有活动的事务 */
    }
    
    /* 提交事务：释放备份数据 */
    if (vm->transaction.backup_data != NULL) {
        free(vm->transaction.backup_data);
        vm->transaction.backup_data = NULL;
    }
    
    vm->transaction.active = false;
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;
    
    return 0;
}

int vm_transaction_rollback(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }
    
    if (!vm->transaction.active) {
        return -2; /* 没有活动的事务 */
    }
    
    if (vm->transaction.backup_data == NULL) {
        return -3; /* 没有备份数据 */
    }
    
    /* 回滚事务：恢复备份数据 */
    memcpy(vm->heap, vm->transaction.backup_data, VM_HEAP_SIZE);
    memcpy(vm->global_data, vm->transaction.backup_data + VM_HEAP_SIZE,
           VM_GLOBAL_DATA_SIZE);
    
    /* 释放备份数据 */
    free(vm->transaction.backup_data);
    vm->transaction.backup_data = NULL;
    
    vm->transaction.active = false;
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;
    
    return 0;
}

VMState vm_get_state(const VMContext *vm) {
    if (vm == NULL) {
        return VM_STATE_ERROR;
    }
    return vm->state;
}

ExceptionType vm_get_exception(const VMContext *vm) {
    if (vm == NULL) {
        return EXCEPTION_NONE;
    }
    return vm->exception;
}

void vm_clear_exception(VMContext *vm) {
    if (vm == NULL) {
        return;
    }
    vm->exception = EXCEPTION_NONE;
}

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

void vm_print_info(const VMContext *vm) {
    if (vm == NULL) {
        printf("VM: NULL\n");
        return;
    }
    
    printf("=== VM Information ===\n");
    printf("State: %d\n", vm->state);
    printf("Stack Pointer: %u\n", vm->stack_pointer);
    printf("Base Pointer: %u\n", vm->base_pointer);
    printf("Frame Top: %u\n", vm->frame_top);
    printf("Program Counter: %u\n", vm->program_counter);
    printf("Heap Used: %u / %u\n", vm->heap_used, VM_HEAP_SIZE);
    printf("Module Count: %u\n", vm->module_count);
    printf("App Count: %u\n", vm->app_count);
    printf("Current Channel: %u\n", vm->current_channel);
    printf("Exception: %d\n", vm->exception);
    printf("Instructions Executed: %llu\n", 
           (unsigned long long)vm->instructions_executed);
    printf("=====================\n");
}
