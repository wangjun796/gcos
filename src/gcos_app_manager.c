/**
 * @file gcos_app_manager.c
 * @brief GCOS VM Application Manager Implementation
 * 
 * Implements application lifecycle management based on cref architecture.
 */

#include "gcos_app_manager.h"
#include "gcos_apdu.h"  // For status word constants
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Find a free application slot
 * 
 * @param vm VM instance
 * @return Free app ID, or 0xFF if no free slot
 */
static u8 find_free_app_slot(GCOSVM *vm) {
    for (u8 i = 0; i < MAX_APPS; i++) {
        if (!vm->apps[i].installed) {
            return i;
        }
    }
    return 0xFF;  /* No free slot */
}

/* ============================================================================
 * ISD (Initial Security Domain) Implementation
 * ============================================================================ */

/**
 * @brief SELECT command handler
 * 
 * Handles SELECT APDU (INS=0xA4) to select applications by AID.
 * Implements ISO 7816-4 SELECT command semantics.
 */
static u16 isd_handler_select(GCOSAppInstance *app,
                              const u8 *apdu,
                              u16 apdu_len,
                              u8 *response,
                              u16 *resp_len) {
    // Parse APDU header
    u8 p1 = apdu[2];  // P1: Selection mode
    u8 p2 = apdu[3];  // P2: Selection control
    u8 lc = (apdu_len > 4) ? apdu[4] : 0;  // Lc: Length of AID data
    
    printf("[ISD] SELECT command: P1=0x%02X, P2=0x%02X, Lc=%u\n", p1, p2, lc);
    
    // Find VM instance by searching for the app pointer
    GCOSVM *vm = NULL;
    extern GCOSVM* gcos_vm_get_instance(void);  // Forward declaration
    vm = gcos_vm_get_instance();
    
    if (vm == NULL) {
        printf("[ISD] ERROR: Cannot get VM instance\n");
        return 0x6F00;  // SW_NO_PRECISE_DIAGNOSIS
    }
    
    // Case 1: Implicit selection (no AID provided)
    if (lc == 0 || apdu_len <= 5) {
        printf("[ISD] SELECT: Implicit selection (no AID)\n");
        
        // Select ISD itself or default application
        GCOSAppInstance *isd = app_find_by_id(vm, APP_FIRST);
        
        if (isd == NULL) {
            return 0x6A82;  // SW_FILE_NOT_FOUND
        }
        
        // Update selection state
        vm->selected_app = isd;
        isd->is_selected = true;
        isd->selected_channel = vm->current_channel;
        
        // Generate FCP (File Control Parameters) response
        // For now, return empty response with success
        if (resp_len) {
            *resp_len = 0;
        }
        
        printf("[ISD] SELECT: ISD selected successfully\n");
        return 0x9000;  // SW_SUCCESS
    }
    
    // Case 2: Explicit selection by AID
    const u8 *aid_data = &apdu[5];  // AID starts after CLA INS P1 P2 Lc
    u8 aid_length = lc;
    
    if (aid_length < 5 || aid_length > 16) {
        printf("[ISD] SELECT: Invalid AID length %u\n", aid_length);
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    printf("[ISD] SELECT: Looking for AID: ");
    for (int i = 0; i < aid_length; i++) {
        printf("%02X", aid_data[i]);
    }
    printf("\n");
    
    // Find application by AID
    GCOSAppInstance *target_app = app_find_by_aid(vm, aid_data, aid_length);
    
    if (target_app == NULL) {
        printf("[ISD] SELECT: Application not found\n");
        return 0x6A82;  // SW_FILE_NOT_FOUND
    }
    
    // Verify application state
    if (target_app->lifecycle != APPLICATION_SELECTABLE &&
        target_app->lifecycle != APPLICATION_PERSONALIZED) {
        printf("[ISD] SELECT: Application not selectable (state=0x%02X)\n",
               target_app->lifecycle);
        return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
    }
    
    // Deselect current application if different
    if (vm->selected_app != NULL && vm->selected_app != target_app) {
        app_deselect(vm, vm->current_channel);
    }
    
    // Call on_select callback if exists
    if (target_app->on_select != NULL) {
        GCOSResult result = target_app->on_select(target_app);
        if (result != GCOS_SUCCESS) {
            printf("[ISD] SELECT: on_select() failed: %d\n", result);
            return 0x6F00;  // SW_EXECUTION_ERROR
        }
    }
    
    // Update selection state
    vm->selected_app = target_app;
    target_app->is_selected = true;
    target_app->selected_channel = vm->current_channel;
    
    vm->channels[vm->current_channel].selected_app = target_app;
    vm->channels[vm->current_channel].active = true;
    
    printf("[ISD] SELECT: Application selected successfully. AID: ");
    for (int i = 0; i < target_app->app_aid.length; i++) {
        printf("%02X", target_app->app_aid.aid[i]);
    }
    printf("\n");
    
    // Generate FCP response
    // For now, return empty response
    if (resp_len) {
        *resp_len = 0;
    }
    
    return SW_SUCCESS;
}

/**
 * @brief ISD's process() method
 * 
 * Handles all GP management commands.
 */
static u16 isd_process(GCOSAppInstance *app,
                       const u8 *apdu,
                       u16 apdu_len,
                       u8 *response,
                       u16 *resp_len) {
    u8 ins = apdu[1];
    
    printf("[ISD] Processing command INS=0x%02X\n", ins);
    
    // Dispatch to specific handlers based on INS
    switch (ins) {
        case 0xA4:  // SELECT
            return isd_handler_select(app, apdu, apdu_len, response, resp_len);
        
        case 0xE4:  // LOAD
        case 0xE6:  // INSTALL
        case 0xE2:  // DELETE
        case 0xF2:  // GET STATUS
        case 0x50:  // INITIALIZE UPDATE
        case 0x82:  // EXTERNAL AUTHENTICATE
        case 0xCA:  // GET DATA
        case 0xDA:  // PUT DATA
            // TODO: Implement other GP commands
            printf("[ISD] Command 0x%02X not yet implemented\n", ins);
            
            // For now, return echo response for testing
            if (resp_len != NULL && response != NULL) {
                if (apdu_len > 4 && apdu_len <= 256) {
                    memcpy(response, &apdu[4], apdu_len - 4);
                    *resp_len = apdu_len - 4;
                } else {
                    *resp_len = 0;
                }
            }
            return 0x9000;  // SW_SUCCESS
        
        default:
            printf("[ISD] Unsupported GP command: INS=0x%02X\n", ins);
            return 0x6D00;  // SW_INS_NOT_SUPPORTED
    }
}

/**
 * @brief Create ISD application
 * 
 * ISD is pre-installed during system initialization.
 */
static GCOSResult create_isd_application(GCOSVM *vm) {
    GCOSAppInstance *isd = &vm->apps[APP_FIRST];
    
    // Set ISD AID (GlobalPlatform ISD AID)
    u8 isd_aid[] = {0xA0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x00};
    memcpy(isd->app_aid.aid, isd_aid, 8);
    isd->app_aid.length = 8;
    
    // Set application ID
    isd->app_id = APP_FIRST;
    
    // Set lifecycle state to SELECTABLE
    isd->lifecycle = APPLICATION_SELECTABLE;
    
    // ⭐ Set ISD's process() method
    isd->process = isd_process;
    isd->on_select = NULL;   // ISD doesn't need select/deselect callbacks
    isd->on_deselect = NULL;
    
    // Initialize state
    isd->is_selected = false;
    isd->selected_channel = 0xFF;
    isd->installed = true;
    isd->install_time = 0;
    
    // Initialize data pointers to NULL
    isd->app_domain_data = NULL;
    isd->app_domain_data_size = 0;
    isd->ref_domain_data = NULL;
    isd->ref_domain_data_size = 0;
    isd->persistent_data = NULL;
    isd->persistent_data_size = 0;
    
    // Initialize channel data
    for (int i = 0; i < MAX_CHANNELS; i++) {
        isd->channel_data[i].temp_dynamic_data = NULL;
        isd->channel_data[i].temp_dynamic_data_size = 0;
        isd->channel_data[i].global_data_copy = NULL;
        isd->channel_data[i].global_data_copy_size = 0;
        isd->channel_data[i].active = false;
        isd->channel_data[i].selected = false;
    }
    isd->current_channel = 0;
    
    vm->app_count = 1;
    
    printf("[ISD] Created with AID: ");
    for (int i = 0; i < isd->app_aid.length; i++) {
        printf("%02X", isd->app_aid.aid[i]);
    }
    printf("\n");
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Application Manager API Implementation
 * ============================================================================ */

GCOSResult app_manager_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    printf("[APP_MANAGER] Initializing application manager...\n");
    
    // Clear all application slots
    memset(vm->apps, 0, sizeof(GCOSAppInstance) * MAX_APPS);
    
    // Initialize application count
    vm->app_count = 0;
    
    // Clear selected application
    vm->selected_app = NULL;
    
    // Initialize channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
        vm->channels[i].selected_app = NULL;
        vm->channels[i].active = false;
    }
    vm->current_channel = 0;
    
    // ⭐ Create ISD (Initial Security Domain)
    GCOSResult result = create_isd_application(vm);
    if (result != GCOS_SUCCESS) {
        printf("[APP_MANAGER] ERROR: Failed to create ISD\n");
        return result;
    }
    
    printf("[APP_MANAGER] Application manager initialized successfully\n");
    return GCOS_SUCCESS;
}

GCOSResult app_register(GCOSVM *vm, 
                        const GCOSAID *app_aid,
                        u16 (*process_func)(GCOSAppInstance *, const u8 *, u16, u8 *, u16 *),
                        GCOSResult (*on_select)(GCOSAppInstance *),
                        void (*on_deselect)(GCOSAppInstance *),
                        u16 module_index,
                        u8 *app_id) {
    if (vm == NULL || app_aid == NULL || process_func == NULL) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    // Find free application slot
    u8 new_app_id = find_free_app_slot(vm);
    
    if (new_app_id == 0xFF) {
        printf("[APP_REGISTER] ERROR: Application table full\n");
        return GCOS_ERROR_APP_TABLE_FULL;
    }
    
    GCOSAppInstance *app = &vm->apps[new_app_id];
    
    // Set application information
    memcpy(&app->app_aid, app_aid, sizeof(GCOSAID));
    app->app_id = new_app_id;
    app->module_index = module_index;
    app->lifecycle = APPLICATION_INSTALLED;
    
    // ⭐ Set method pointers
    app->process = process_func;
    app->on_select = on_select;
    app->on_deselect = on_deselect;
    
    // Initialize state
    app->is_selected = false;
    app->selected_channel = 0xFF;
    app->installed = true;
    app->install_time = 0;  // TODO: Get current timestamp
    
    // Initialize data pointers to NULL
    app->app_domain_data = NULL;
    app->app_domain_data_size = 0;
    app->ref_domain_data = NULL;
    app->ref_domain_data_size = 0;
    app->persistent_data = NULL;
    app->persistent_data_size = 0;
    
    // Initialize channel data
    for (int i = 0; i < MAX_CHANNELS; i++) {
        app->channel_data[i].temp_dynamic_data = NULL;
        app->channel_data[i].temp_dynamic_data_size = 0;
        app->channel_data[i].global_data_copy = NULL;
        app->channel_data[i].global_data_copy_size = 0;
        app->channel_data[i].active = false;
        app->channel_data[i].selected = false;
    }
    app->current_channel = 0;
    
    vm->app_count++;
    
    if (app_id != NULL) {
        *app_id = new_app_id;
    }
    
    printf("[APP_REGISTER] Registered. ID=%u AID=", new_app_id);
    for (int i = 0; i < app_aid->length; i++) {
        printf("%02X", app_aid->aid[i]);
    }
    printf("\n");
    
    return GCOS_SUCCESS;
}

GCOSAppInstance* app_find_by_aid(GCOSVM *vm, const u8 *aid, u8 aid_len) {
    if (vm == NULL || aid == NULL || aid_len == 0 || aid_len > AID_MAX_LENGTH) {
        return NULL;
    }
    
    for (u8 i = 0; i < MAX_APPS; i++) {
        if (!vm->apps[i].installed) {
            continue;
        }
        
        u8 stored_aid_len = vm->apps[i].app_aid.length;
        
        // ⭐ ISO 7816-4 prefix matching:
        // Stored AID must be >= incoming AID length
        if (stored_aid_len < aid_len) {
            continue;
        }
        
        // Compare only the first aid_len bytes (prefix match)
        if (memcmp(vm->apps[i].app_aid.aid, aid, aid_len) == 0) {
            return &vm->apps[i];
        }
    }
    
    return NULL;
}

GCOSAppInstance* app_find_by_id(GCOSVM *vm, u8 app_id) {
    if (vm == NULL || app_id >= MAX_APPS) {
        return NULL;
    }
    
    if (!vm->apps[app_id].installed) {
        return NULL;
    }
    
    return &vm->apps[app_id];
}

GCOSResult app_select(GCOSVM *vm, u8 app_id, u8 channel) {
    if (vm == NULL || channel >= MAX_CHANNELS) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    // Step 1: Find application
    GCOSAppInstance *app = app_find_by_id(vm, app_id);
    
    if (app == NULL) {
        printf("[APP_SELECT] ERROR: Application %u not found\n", app_id);
        return GCOS_ERROR_APP_NOT_FOUND;
    }
    
    // Step 2: Validate application state
    if (app->lifecycle != APPLICATION_SELECTABLE &&
        app->lifecycle != APPLICATION_PERSONALIZED) {
        printf("[APP_SELECT] ERROR: Application %u not selectable (state=0x%02X)\n",
               app_id, app->lifecycle);
        return GCOS_ERROR_APP_NOT_SELECTABLE;
    }
    
    // Step 3: Deselect currently selected application (if different)
    if (vm->selected_app != NULL && vm->selected_app != app) {
        app_deselect(vm, vm->current_channel);
    }
    
    // Step 4: Call application's on_select() callback (if exists)
    if (app->on_select != NULL) {
        GCOSResult result = app->on_select(app);
        if (result != GCOS_SUCCESS) {
            printf("[APP_SELECT] ERROR: on_select() failed: %d\n", result);
            return result;
        }
    }
    
    // Step 5: Update selection state
    vm->selected_app = app;
    app->is_selected = true;
    app->selected_channel = channel;
    
    vm->channels[channel].selected_app = app;
    vm->channels[channel].active = true;
    vm->current_channel = channel;
    
    printf("[APP_SELECT] Application selected. AID: ");
    for (int i = 0; i < app->app_aid.length; i++) {
        printf("%02X", app->app_aid.aid[i]);
    }
    printf("\n");
    
    return GCOS_SUCCESS;
}

GCOSResult app_deselect(GCOSVM *vm, u8 channel) {
    if (vm == NULL || channel >= MAX_CHANNELS) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = vm->channels[channel].selected_app;
    
    if (app == NULL) {
        return GCOS_SUCCESS;  // No application selected
    }
    
    // Step 1: Call application's on_deselect() callback (if exists)
    if (app->on_deselect != NULL) {
        app->on_deselect(app);
    }
    
    // Step 2: Clear selection state
    app->is_selected = false;
    app->selected_channel = 0xFF;
    
    vm->channels[channel].selected_app = NULL;
    vm->channels[channel].active = false;
    
    if (vm->selected_app == app) {
        vm->selected_app = NULL;
    }
    
    printf("[APP_DESELECT] Application deselected\n");
    
    return GCOS_SUCCESS;
}

GCOSResult app_delete(GCOSVM *vm, u8 app_id) {
    if (vm == NULL || app_id >= MAX_APPS) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    GCOSAppInstance *app = &vm->apps[app_id];
    
    if (!app->installed) {
        return GCOS_ERROR_APP_NOT_FOUND;
    }
    
    // Cannot delete ISD
    if (app_id == APP_FIRST) {
        printf("[APP_DELETE] ERROR: Cannot delete ISD\n");
        return GCOS_ERROR_CANNOT_DELETE_ISD;
    }
    
    // Deselect if currently selected
    if (vm->selected_app == app) {
        app_deselect(vm, vm->current_channel);
    }
    
    // Clear application slot
    memset(app, 0, sizeof(GCOSAppInstance));
    
    vm->app_count--;
    
    printf("[APP_DELETE] Application %u deleted\n", app_id);
    
    return GCOS_SUCCESS;
}

GCOSAppInstance* app_get_selected(GCOSVM *vm) {
    if (vm == NULL) {
        return NULL;
    }
    
    return vm->selected_app;
}

bool app_is_selected(GCOSVM *vm) {
    if (vm == NULL) {
        return false;
    }
    
    return (vm->selected_app != NULL);
}
