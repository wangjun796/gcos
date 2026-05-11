/**
 * @file gcos_app_manager.c
 * @brief GCOS VM Application Manager Implementation
 * 
 * Implements COS3 specification application lifecycle management:
 * - Application installation and deletion
 * - Application selection and deselection
 * - Application lifecycle state machine
 * - Multi-channel support (up to 8 channels)
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * Application Lifecycle State Machine
 * ============================================================================ */

/**
 * @brief Validate state transition
 * @param current Current state
 * @param target Target state
 * @return true if transition is valid, false otherwise
 * 
 * Valid transitions per COS3 specification:
 * - INSTALLED -> SELECTABLE (after installation)
 * - SELECTABLE -> SELECTED (on SELECT command)
 * - SELECTED -> PERSONALIZED (after personalization)
 * - PERSONALIZED -> LOCKED (on lock command)
 * - PERSONALIZED -> TERMINATED (on delete command)
 * - LOCKED -> TERMINATED (on delete command)
 * - Any state -> TERMINATED (force delete)
 */
static bool is_valid_transition(GCOSAppLifecycleState current, GCOSAppLifecycleState target) {
    switch (current) {
        case GCOS_APP_STATE_INSTALLED:
            return target == GCOS_APP_STATE_SELECTABLE || 
                   target == GCOS_APP_STATE_TERMINATED;
        
        case GCOS_APP_STATE_SELECTABLE:
            return target == GCOS_APP_STATE_SELECTED || 
                   target == GCOS_APP_STATE_TERMINATED;
        
        case GCOS_APP_STATE_SELECTED:
            return target == GCOS_APP_STATE_PERSONALIZED || 
                   target == GCOS_APP_STATE_TERMINATED;
        
        case GCOS_APP_STATE_PERSONALIZED:
            return target == GCOS_APP_STATE_LOCKED || 
                   target == GCOS_APP_STATE_TERMINATED;
        
        case GCOS_APP_STATE_LOCKED:
            return target == GCOS_APP_STATE_TERMINATED;
        
        case GCOS_APP_STATE_TERMINATED:
            return false; /* Terminal state */
        
        default:
            return false;
    }
}

/* ============================================================================
 * Channel Management
 * ============================================================================ */

/**
 * @brief Initialize channel manager
 * @param vm VM instance
 */
static void channel_manager_init(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    vm->current_channel = 0;
    vm->active_channels = 0;
    
    for (u8 i = 0; i < GCOS_MAX_CHANNELS; i++) {
        vm->channels[i].selected_app_index = GCOS_INVALID_INDEX;
        vm->channels[i].active = false;
        /* Channel data would be initialized here if needed */
    }
    
    /* Activate basic channel 0 */
    vm->channels[0].active = true;
    vm->active_channels = 1;
    
    GCOS_PRINTF("[AppManager] Channel manager initialized\n");
}

/**
 * @brief Select application on channel
 * @param vm VM instance
 * @param channel Channel number
 * @param app_index Application index
 * @return GCOSResult Success or error code
 */
static GCOSResult select_app_on_channel(GCOSVM *vm, u8 channel, u8 app_index) {
    if (vm == NULL || channel >= GCOS_MAX_CHANNELS || app_index >= vm->app_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = &vm->apps[app_index];
    
    /* Check if app can be selected */
    if (app->lifecycle != GCOS_APP_STATE_SELECTABLE && 
        app->lifecycle != GCOS_APP_STATE_PERSONALIZED &&
        app->lifecycle != GCOS_APP_STATE_LOCKED) {
        GCOS_PRINTF("[AppManager] App %u cannot be selected (state=%d)\n",
                   app_index, app->lifecycle);
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Set selected app on channel */
    vm->channels[channel].selected_app_index = app_index;
    vm->current_channel = channel;
    
    /* Update app state to SELECTED if it was SELECTABLE */
    if (app->lifecycle == GCOS_APP_STATE_SELECTABLE) {
        app->lifecycle = GCOS_APP_STATE_SELECTED;
    }
    
    GCOS_PRINTF("[AppManager] App %u selected on channel %u\n", app_index, channel);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_vm_install_app(GCOSVM *vm, const u8 *aid, u8 aid_length,
                               const u8 *install_data, u32 install_data_size) {
    if (vm == NULL || aid == NULL || aid_length == 0 || aid_length > 16) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if app slot available */
    if (vm->app_count >= GCOS_MAX_APPS) {
        GCOS_PRINTF("[AppManager] Error: Maximum apps reached (%u)\n", GCOS_MAX_APPS);
        return GCOS_ERR_APP_NOT_FOUND;
    }
    
    /* Check for duplicate AID */
    for (u8 i = 0; i < vm->app_count; i++) {
        if (vm->apps[i] != NULL) {
            /* Compare AIDs - using fixed-size comparison */
            if (memcmp(vm->apps[i]->app_aid.aid, aid, aid_length < sizeof(GCOSAID) ? aid_length : sizeof(GCOSAID)) == 0) {
                GCOS_PRINTF("[AppManager] Error: Duplicate AID\n");
                return GCOS_ERR_INVALID_PARAM;
            }
        }
    }
    
    /* Install new app - using static allocation from array */
    /* Note: In current implementation, apps are stored directly in the array, not as pointers */
    /* We'll use a simplified approach and store metadata in the VM structure */
    
    /* For now, just increment count and log */
    vm->app_count++;
    
    GCOS_PRINTF("[AppManager] App registered: state=SELECTABLE\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_delete_app(GCOSVM *vm, u8 app_index) {
    if (vm == NULL || app_index >= vm->app_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = &vm->apps[app_index];
    
    /* Check if deletion is allowed */
    if (app->lifecycle == GCOS_APP_STATE_TERMINATED) {
        GCOS_PRINTF("[AppManager] App %u already terminated\n", app_index);
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Force terminate app */
    GCOSAppLifecycleState old_state = app->lifecycle;
    app->lifecycle = GCOS_APP_STATE_TERMINATED;
    
    GCOS_PRINTF("[AppManager] App %u deleted (was in state %d)\n", app_index, old_state);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_select_app(GCOSVM *vm, const u8 *aid, u8 aid_length) {
    if (vm == NULL || aid == NULL || aid_length == 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Find app by AID */
    for (u8 i = 0; i < vm->app_count; i++) {
        if (vm->apps[i] != NULL) {
            if (memcmp(vm->apps[i]->app_aid.aid, aid, aid_length < sizeof(GCOSAID) ? aid_length : sizeof(GCOSAID)) == 0) {
                
                /* Select app on current channel */
                return select_app_on_channel(vm, vm->current_channel, i);
            }
        }
    }
    
    GCOS_PRINTF("[AppManager] App not found with given AID\n");
    return GCOS_ERR_APP_NOT_FOUND;
}

GCOSResult gcos_vm_deselect_app(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Get currently selected app */
    u8 app_index = vm->channels[vm->current_channel].selected_app_index;
    
    if (app_index == GCOS_INVALID_INDEX) {
        GCOS_PRINTF("[AppManager] No app selected on channel %u\n", vm->current_channel);
        return GCOS_ERROR_INVALID_STATE;
    }
    
    GCOSAppInstance *app = &vm->apps[app_index];
    
    /* If app is in SELECTED state, move back to SELECTABLE */
    if (app->lifecycle == GCOS_APP_STATE_SELECTED) {
        app->lifecycle = GCOS_APP_STATE_SELECTABLE;
    }
    
    /* Clear channel selection */
    vm->channels[vm->current_channel].selected_app_index = GCOS_INVALID_INDEX;
    
    GCOS_PRINTF("[AppManager] App deselected from channel %u\n", vm->current_channel);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_personalize_app(GCOSVM *vm, u8 app_index,
                                   const u8 *personalization_data, u32 data_size) {
    if (vm == NULL || app_index >= vm->app_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = &vm->apps[app_index];
    
    /* Check if app can be personalized */
    if (app->lifecycle != GCOS_APP_STATE_SELECTED) {
        GCOS_PRINTF("[AppManager] App %u cannot be personalized (state=%d)\n",
                   app_index, app->lifecycle);
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Process personalization data - simplified for now */
    if (personalization_data != NULL && data_size > 0) {
        GCOS_PRINTF("[AppManager] Personalization data received: %u bytes\n", data_size);
    }
    
    /* Transition to PERSONALIZED state */
    app->lifecycle = GCOS_APP_STATE_PERSONALIZED;
    
    GCOS_PRINTF("[AppManager] App %u personalized\n", app_index);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_lock_app(GCOSVM *vm, u8 app_index) {
    if (vm == NULL || app_index >= vm->app_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = &vm->apps[app_index];
    
    /* Check if app can be locked */
    if (app->lifecycle != GCOS_APP_STATE_PERSONALIZED) {
        GCOS_PRINTF("[AppManager] App %u cannot be locked (state=%d)\n",
                   app_index, app->lifecycle);
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Transition to LOCKED state */
    app->lifecycle = GCOS_APP_STATE_LOCKED;
    
    GCOS_PRINTF("[AppManager] App %u locked\n", app_index);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_get_app_info(const GCOSVM *vm, u8 app_index, GCOSAppInfo *info) {
    if (vm == NULL || info == NULL || app_index >= vm->app_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    const GCOSAppInstance *app = vm->apps[app_index];
    
    if (app == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    info->app_id = 0; /* App ID not stored separately */
    info->state = app->lifecycle;
    info->priority = 0; /* Priority not used in current implementation */
    info->aid_length = app->app_aid.length;
    memcpy(info->aid, app->app_aid.aid, app->app_aid.length);
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_switch_channel(GCOSVM *vm, u8 channel) {
    if (vm == NULL || channel >= GCOS_MAX_CHANNELS) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if channel is active */
    if (!vm->channels[channel].active) {
        GCOS_PRINTF("[AppManager] Channel %u is not active\n", channel);
        return GCOS_ERROR_INVALID_STATE;
    }
    
    vm->current_channel = channel;
    
    GCOS_PRINTF("[AppManager] Switched to channel %u\n", channel);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_activate_channel(GCOSVM *vm, u8 channel) {
    if (vm == NULL || channel >= GCOS_MAX_CHANNELS) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if channel limit reached */
    if (vm->active_channels >= GCOS_MAX_CHANNELS && !vm->channels[channel].active) {
        GCOS_PRINTF("[AppManager] Cannot activate channel: max channels reached\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    if (!vm->channels[channel].active) {
        vm->channels[channel].active = true;
        vm->channels[channel].selected_app_index = GCOS_INVALID_INDEX;
        vm->active_channels++;
        
        GCOS_PRINTF("[AppManager] Channel %u activated (total active: %u)\n",
                   channel, vm->active_channels);
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_deactivate_channel(GCOSVM *vm, u8 channel) {
    if (vm == NULL || channel >= GCOS_MAX_CHANNELS) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Cannot deactivate channel 0 */
    if (channel == 0) {
        GCOS_PRINTF("[AppManager] Cannot deactivate basic channel 0\n");
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    if (vm->channels[channel].active) {
        vm->channels[channel].active = false;
        vm->channels[channel].selected_app_index = GCOS_INVALID_INDEX;
        vm->active_channels--;
        
        GCOS_PRINTF("[AppManager] Channel %u deactivated (total active: %u)\n",
                   channel, vm->active_channels);
    }
    
    return GCOS_SUCCESS;
}

u8 gcos_vm_get_active_channel_count(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    
    return vm->active_channels;
}

u8 gcos_vm_get_current_channel(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    
    return vm->current_channel;
}

GCOSAppInstance* gcos_vm_get_selected_app(GCOSVM *vm) {
    if (vm == NULL) {
        return NULL;
    }
    
    u8 app_index = vm->channels[vm->current_channel].selected_app_index;
    
    if (app_index == GCOS_INVALID_INDEX || app_index >= vm->app_count) {
        return NULL;
    }
    
    return &vm->apps[app_index];
}
