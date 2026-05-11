/**
 * @file vm_app_manager_full.c
 * @brief GCOS VM 应用管理器完整实现
 * 
 * 实现基于COS3规范的应用管理器，包括：
 * - 应用安装/卸载
 * - 应用选择/取消选择
 * - 多通道管理
 * - 应用生命周期管理
 */

#include "vm_core.h"
#include "vm_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 应用管理器状态
 * ============================================================================ */

typedef struct {
    AppInstance *apps[MAX_APPS_PER_MODULE * MAX_MODULES];
    u16 app_count;
    u8 selected_apps[MAX_CHANNELS];  /* 每个通道选择的应用索引 */
    bool channels_active[MAX_CHANNELS];
} AppManager;

static AppManager app_mgr = {
    .app_count = 0,
    .selected_apps = {0},
    .channels_active = {false}
};

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/**
 * @brief 查找应用实例
 */
static AppInstance* find_app_by_aid(const AID *aid) {
    for (u16 i = 0; i < app_mgr.app_count; i++) {
        AppInstance *app = app_mgr.apps[i];
        if (app != NULL && app->installed) {
            if (aid_equal(&app->app_aid, aid)) {
                return app;
            }
        }
    }
    return NULL;
}

/**
 * @brief 查找应用索引
 */
static s16 find_app_index_by_aid(const AID *aid) {
    for (u16 i = 0; i < app_mgr.app_count; i++) {
        AppInstance *app = app_mgr.apps[i];
        if (app != NULL && app->installed) {
            if (aid_equal(&app->app_aid, aid)) {
                return (s16)i;
            }
        }
    }
    return -1;
}

/**
 * @brief 查找模块中的应用实例
 */
static u16 count_apps_in_module(u16 module_index) {
    u16 count = 0;
    for (u16 i = 0; i < app_mgr.app_count; i++) {
        AppInstance *app = app_mgr.apps[i];
        if (app != NULL && app->installed && app->module_index == module_index) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 检查应用生命周期状态
 */
static bool check_lifecycle_state(AppInstance *app, AppLifecycleState required_state) {
    if (app == NULL || !app->installed) {
        return false;
    }

    switch (required_state) {
        case APP_LIFECYCLE_INSTALLED:
            return app->lifecycle >= APP_LIFECYCLE_INSTALLED;
        case APP_LIFECYCLE_SELECTABLE:
            return app->lifecycle >= APP_LIFECYCLE_SELECTABLE;
        case APP_LIFECYCLE_SELECTED:
            return app->lifecycle >= APP_LIFECYCLE_SELECTED;
        case APP_LIFECYCLE_PERSONALIZED:
            return app->lifecycle >= APP_LIFECYCLE_PERSONALIZED;
        case APP_LIFECYCLE_LOCKED:
            return app->lifecycle >= APP_LIFECYCLE_LOCKED;
        case APP_LIFECYCLE_TERMINATED:
            return app->lifecycle >= APP_LIFECYCLE_TERMINATED;
        default:
            return false;
    }
}

/**
 * @brief 调用应用生命周期方法
 */
static int call_lifecycle_method(VMContext *vm, AppInstance *app, u8 method) {
    if (vm->current_module == NULL || app->module_index >= vm->module_count) {
        return -1;
    }

    Module *module = &vm->modules[app->module_index];

    /* 设置当前应用 */
    vm->current_app = app;
    vm->current_module = module;

    /* TODO: 调用应用生命周期方法 */
    switch (method) {
        case 0: /* 安装方法 */
            printf("Calling install method for app\n");
            break;
        case 1: /* 选择方法 */
            printf("Calling select method for app\n");
            break;
        case 2: /* 取消选择方法 */
            printf("Calling deselect method for app\n");
            break;
        case 3: /* 多通道选择方法 */
            printf("Calling multi-channel select method for app\n");
            break;
        case 4: /* 多通道取消选择方法 */
            printf("Calling multi-channel deselect method for app\n");
            break;
        case 5: /* 卸载方法 */
            printf("Calling uninstall method for app\n");
            break;
        default:
            return -2;
    }

    return 0;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

/**
 * @brief 初始化应用管理器
 */
int vm_app_manager_init(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    memset(&app_mgr, 0, sizeof(AppManager));
    app_mgr.app_count = 0;
    memset(app_mgr.selected_apps, 0xFF, sizeof(app_mgr.selected_apps));
    memset(app_mgr.channels_active, 0, sizeof(app_mgr.channels_active));

    /* 初始化VM中的应用数组 */
    for (u16 i = 0; i < MAX_APPS_PER_MODULE * MAX_MODULES; i++) {
        vm->apps[i] = NULL;
    }
    vm->app_count = 0;

    return 0;
}

/**
 * @brief 安装应用
 */
int vm_app_manager_install(VMContext *vm, u16 module_index, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }

    /* 检查模块是否存在 */
    if (module_index >= vm->module_count) {
        printf("Error: Module index %u out of range\n", module_index);
        return -2;
    }

    Module *module = &vm->modules[module_index];
    if (!module->loaded) {
        printf("Error: Module %u not loaded\n", module_index);
        return -3;
    }

    /* 检查应用是否已存在 */
    if (find_app_by_aid(app_aid) != NULL) {
        printf("Error: App already installed\n");
        return -4;
    }

    /* 检查应用数量限制 */
    u16 app_count_in_module = count_apps_in_module(module_index);
    if (app_count_in_module >= MAX_APPS_PER_MODULE) {
        printf("Error: Maximum apps per module reached\n");
        return -5;
    }

    /* 检查总应用数量限制 */
    if (vm->app_count >= MAX_APPS_PER_MODULE * MAX_MODULES) {
        printf("Error: Maximum total apps reached\n");
        return -6;
    }

    /* 分配应用实例 */
    AppInstance *app = (AppInstance*)malloc(sizeof(AppInstance));
    if (app == NULL) {
        printf("Error: Failed to allocate app instance\n");
        return -7;
    }

    memset(app, 0, sizeof(AppInstance));

    /* 设置应用信息 */
    app->app_aid.length = app_aid->length;
    memcpy(app->app_aid.aid, app_aid->aid, app_aid->length);
    app->module_index = module_index;
    app->lifecycle = APP_LIFECYCLE_INSTALLED;
    app->installed = true;

    /* 分配应用域数据 */
    app->app_domain_data = (u8*)malloc(VM_HEAP_SIZE);
    if (app->app_domain_data == NULL) {
        free(app);
        return -8;
    }
    memset(app->app_domain_data, 0, VM_HEAP_SIZE);

    /* 分配引用域数据 */
    app->ref_domain_data = (u8*)malloc(VM_HEAP_SIZE);
    if (app->ref_domain_data == NULL) {
        free(app->app_domain_data);
        free(app);
        return -9;
    }
    memset(app->ref_domain_data, 0, VM_HEAP_SIZE);

    /* 分配持久性数据 */
    app->persistent_data = (u8*)malloc(VM_HEAP_SIZE);
    if (app->persistent_data == NULL) {
        free(app->app_domain_data);
        free(app->ref_domain_data);
        free(app);
        return -10;
    }
    memset(app->persistent_data, 0, VM_HEAP_SIZE);

    /* 初始化通道数据 */
    for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
        app->channel_data[ch].active = false;
        app->channel_data[ch].selected = false;
        app->channel_data[ch].temp_dynamic_data = NULL;
        app->channel_data[ch].global_data_copy = NULL;
    }

    /* 添加到应用管理器 */
    for (u16 i = 0; i < MAX_APPS_PER_MODULE * MAX_MODULES; i++) {
        if (app_mgr.apps[i] == NULL) {
            app_mgr.apps[i] = app;
            vm->apps[vm->app_count] = app;
            app_mgr.app_count++;
            vm->app_count++;
            break;
        }
    }

    printf("App installed: module=%u, app_count=%u\n", module_index, vm->app_count);

    return 0;
}

/**
 * @brief 卸载应用
 */
int vm_app_manager_uninstall(VMContext *vm, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }

    /* 查找应用 */
    s16 app_index = find_app_index_by_aid(app_aid);
    if (app_index < 0) {
        printf("Error: App not found\n");
        return -2;
    }

    AppInstance *app = app_mgr.apps[app_index];

    /* 检查应用是否被选择 */
    for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
        if (app_mgr.selected_apps[ch] == (u8)app_index) {
            printf("Error: App is selected on channel %u\n", ch);
            return -3;
        }
    }

    /* 调用卸载方法 */
    int ret = call_lifecycle_method(vm, app, 5);
    if (ret != 0) {
        printf("Warning: Uninstall method returned error\n");
    }

    /* 释放应用数据 */
    if (app->app_domain_data != NULL) {
        free(app->app_domain_data);
    }
    if (app->ref_domain_data != NULL) {
        free(app->ref_domain_data);
    }
    if (app->persistent_data != NULL) {
        free(app->persistent_data);
    }

    /* 释放通道数据 */
    for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
        if (app->channel_data[ch].temp_dynamic_data != NULL) {
            free(app->channel_data[ch].temp_dynamic_data);
        }
        if (app->channel_data[ch].global_data_copy != NULL) {
            free(app->channel_data[ch].global_data_copy);
        }
    }

    /* 从应用管理器中移除 */
    app_mgr.apps[app_index] = NULL;
    app_mgr.app_count--;

    /* 从VM中移除 */
    for (u16 i = 0; i < vm->app_count; i++) {
        if (vm->apps[i] == app) {
            /* 移动后面的应用 */
            for (u16 j = i; j < vm->app_count - 1; j++) {
                vm->apps[j] = vm->apps[j + 1];
            }
            vm->app_count--;
            break;
        }
    }

    free(app);
    printf("App uninstalled: app_count=%u\n", vm->app_count);

    return 0;
}

/**
 * @brief 选择应用
 */
int vm_app_manager_select(VMContext *vm, u8 channel, const AID *app_aid) {
    if (vm == NULL || app_aid == NULL) {
        return -1;
    }

    /* 检查通道号 */
    if (channel >= MAX_CHANNELS) {
        printf("Error: Invalid channel %u\n", channel);
        return -2;
    }

    /* 查找应用 */
    AppInstance *app = find_app_by_aid(app_aid);
    if (app == NULL) {
        printf("Error: App not found\n");
        return -3;
    }

    /* 检查应用生命周期 */
    if (!check_lifecycle_state(app, APP_LIFECYCLE_SELECTABLE)) {
        printf("Error: App not selectable\n");
        return -4;
    }

    /* 检查模块是否存在 */
    if (app->module_index >= vm->module_count) {
        printf("Error: Module %u not loaded\n", app->module_index);
        return -5;
    }

    Module *module = &vm->modules[app->module_index];

    /* 检查是否支持多通道选择 */
    bool supports_multi_channel = true; /* TODO: 从模块信息中读取 */

    /* 检查应用是否已在其他通道被选择 */
    for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
        if (ch != channel && app_mgr.selected_apps[ch] == (u8)(app - app_mgr.apps)) {
            if (!supports_multi_channel) {
                printf("Error: App already selected on channel %u\n", ch);
                return -6;
            }
        }
    }

    /* 分配临时动态数据 */
    if (app->channel_data[channel].temp_dynamic_data == NULL) {
        app->channel_data[channel].temp_dynamic_data = (u8*)malloc(VM_GLOBAL_DATA_SIZE);
        if (app->channel_data[channel].temp_dynamic_data == NULL) {
            printf("Error: Failed to allocate temp dynamic data\n");
            return -7;
        }
        memset(app->channel_data[channel].temp_dynamic_data, 0, VM_GLOBAL_DATA_SIZE);
    }

    /* 分配模块全局数据副本 */
    if (app->channel_data[channel].global_data_copy == NULL) {
        app->channel_data[channel].global_data_copy = (u8*)malloc(module->global_data_size);
        if (app->channel_data[channel].global_data_copy == NULL) {
            free(app->channel_data[channel].temp_dynamic_data);
            printf("Error: Failed to allocate global data copy\n");
            return -8;
        }
        memcpy(app->channel_data[channel].global_data_copy, 
               module->global_data, module->global_data_size);
    }

    /* 设置当前应用和模块 */
    vm->current_app = app;
    vm->current_module = module;
    vm->current_channel = channel;

    /* 调用选择方法 */
    int ret = call_lifecycle_method(vm, app, supports_multi_channel ? 3 : 1);
    if (ret != 0) {
        printf("Warning: Select method returned error\n");
    }

    /* 标记为已选择 */
    app->lifecycle = APP_LIFECYCLE_SELECTED;
    app->channel_data[channel].selected = true;
    app_mgr.selected_apps[channel] = (u8)(app - app_mgr.apps);
    app_mgr.channels_active[channel] = true;

    printf("App selected: channel=%u\n", channel);

    return 0;
}

/**
 * @brief 取消选择应用
 */
int vm_app_manager_deselect(VMContext *vm, u8 channel) {
    if (vm == NULL) {
        return -1;
    }

    /* 检查通道号 */
    if (channel >= MAX_CHANNELS) {
        printf("Error: Invalid channel %u\n", channel);
        return -2;
    }

    /* 检查是否有应用被选择 */
    if (app_mgr.selected_apps[channel] == 0xFF) {
        printf("Warning: No app selected on channel %u\n", channel);
        return 0;
    }

    AppInstance *app = app_mgr.apps[app_mgr.selected_apps[channel]];
    if (app == NULL || !app->installed) {
        printf("Error: Invalid app on channel %u\n", channel);
        return -3;
    }

    /* 调用取消选择方法 */
    int ret = call_lifecycle_method(vm, app, 2);
    if (ret != 0) {
        printf("Warning: Deselect method returned error\n");
    }

    /* 更新应用生命周期 */
    app->lifecycle = APP_LIFECYCLE_INSTALLED;
    app->channel_data[channel].selected = false;
    app_mgr.selected_apps[channel] = 0xFF;

    /* 清除当前应用 */
    vm->current_app = NULL;
    vm->current_module = NULL;

    printf("App deselected: channel=%u\n", channel);

    return 0;
}

/**
 * @brief 执行APDU命令
 */
int vm_app_manager_execute_apdu(VMContext *vm, u8 channel, 
                                const u8 *apdu, u32 apdu_len,
                                u8 *response, u32 *response_len) {
    if (vm == NULL) {
        return -1;
    }

    /* 检查通道号 */
    if (channel >= MAX_CHANNELS) {
        printf("Error: Invalid channel %u\n", channel);
        return -2;
    }

    /* 检查是否有应用被选择 */
    if (app_mgr.selected_apps[channel] == 0xFF) {
        printf("Error: No app selected on channel %u\n", channel);
        return -3;
    }

    AppInstance *app = app_mgr.apps[app_mgr.selected_apps[channel]];
    if (app == NULL || !app->installed) {
        printf("Error: Invalid app on channel %u\n", channel);
        return -4;
    }

    /* 设置当前应用和模块 */
    vm->current_app = app;
    vm->current_channel = channel;
    if (app->module_index < vm->module_count) {
        vm->current_module = &vm->modules[app->module_index];
    }

    /* TODO: 执行APDU命令 */
    printf("Executing APDU on channel %u: len=%u\n", channel, apdu_len);

    /* 简化实现：返回成功状态 */
    if (response != NULL && response_len != NULL && *response_len >= 2) {
        response[0] = 0x90; /* SW1: 命令成功 */
        response[1] = 0x00; /* SW2: 无额外信息 */
        *response_len = 2;
    }

    return 0;
}

/**
 * @brief 清理应用管理器
 */
void vm_app_manager_cleanup(VMContext *vm) {
    /* 释放所有应用实例 */
    for (u16 i = 0; i < MAX_APPS_PER_MODULE * MAX_MODULES; i++) {
        AppInstance *app = app_mgr.apps[i];
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
            for (u8 ch = 0; ch < MAX_CHANNELS; ch++) {
                if (app->channel_data[ch].temp_dynamic_data != NULL) {
                    free(app->channel_data[ch].temp_dynamic_data);
                }
                if (app->channel_data[ch].global_data_copy != NULL) {
                    free(app->channel_data[ch].global_data_copy);
                }
            }
            free(app);
            app_mgr.apps[i] = NULL;
        }
    }

    app_mgr.app_count = 0;
    memset(app_mgr.selected_apps, 0xFF, sizeof(app_mgr.selected_apps));
    memset(app_mgr.channels_active, 0, sizeof(app_mgr.channels_active));

    if (vm != NULL) {
        vm->app_count = 0;
        for (u16 i = 0; i < MAX_APPS_PER_MODULE * MAX_MODULES; i++) {
            vm->apps[i] = NULL;
        }
    }
}
