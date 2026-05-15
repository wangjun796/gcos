/**
 * @file gcos_install_manager.c
 * @brief GCOS VM INSTALL Command Implementation
 * 
 * Implements the INSTALL command for creating application instances from loaded modules.
 * Similar to cref's applet installation process.
 * 
 * Reference: GlobalPlatform Card Specification - INSTALL Command
 */

#include "gcos_vm.h"
#include "gcos_module_registry.h"
#include "gcos_app_manager.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Dummy process function for newly installed applications
 * 
 * This is a placeholder. In real implementation, the process function
 * would be resolved from the module's function table.
 */
static u16 dummy_process(GCOSAppInstance *app, const u8 *apdu, u16 apdu_len,
                         u8 *response, u16 *resp_len) {
    printf("[APP] Dummy process() called\n");
    return 0x9000;
}

/**
 * @brief Parse INSTALL command data (TLV format)
 * 
 * Expected TLV structure:
 *   Tag 0x4F: Application AID
 *   Tag 0xC4: Installation parameters (optional)
 *   Tag 0xCB: Module AID (identifies which module to instantiate)
 * 
 * @param data INSTALL command data
 * @param data_len Data length
 * @param app_aid Output: Application AID
 * @param module_aid Output: Module AID
 * @param install_params Output: Installation parameters
 * @param install_params_len Output: Installation parameters length
 * @return true if parsing successful, false otherwise
 */
static bool parse_install_data(const u8 *data, u16 data_len,
                               GCOSAID *app_aid,
                               GCOSAID *module_aid,
                               const u8 **install_params,
                               u16 *install_params_len) {
    u16 offset = 0;
    
    // Initialize outputs
    memset(app_aid, 0, sizeof(GCOSAID));
    memset(module_aid, 0, sizeof(GCOSAID));
    *install_params = NULL;
    *install_params_len = 0;
    
    while (offset < data_len) {
        if (offset + 2 > data_len) {
            printf("[INSTALL] ERROR: Invalid TLV structure\n");
            return false;
        }
        
        u8 tag = data[offset++];
        u8 len = data[offset++];
        
        if (offset + len > data_len) {
            printf("[INSTALL] ERROR: TLV length exceeds data\n");
            return false;
        }
        
        switch (tag) {
            case 0x4F:  // Application AID
                if (len < 5 || len > 16) {
                    printf("[INSTALL] ERROR: Invalid App AID length %u\n", len);
                    return false;
                }
                app_aid->length = len;
                memcpy(app_aid->aid, &data[offset], len);
                
                printf("[INSTALL] App AID: ");
                for (int i = 0; i < len; i++) {
                    printf("%02X", data[offset + i]);
                }
                printf("\n");
                break;
                
            case 0xCB:  // Module AID (Package AID)
                if (len < 5 || len > 16) {
                    printf("[INSTALL] ERROR: Invalid Module AID length %u\n", len);
                    return false;
                }
                module_aid->length = len;
                memcpy(module_aid->aid, &data[offset], len);
                
                printf("[INSTALL] Module AID: ");
                for (int i = 0; i < len; i++) {
                    printf("%02X", data[offset + i]);
                }
                printf("\n");
                break;
                
            case 0xC4:  // Installation parameters
                *install_params = &data[offset];
                *install_params_len = len;
                
                printf("[INSTALL] Install params: %u bytes\n", len);
                break;
                
            default:
                printf("[INSTALL] WARNING: Unknown tag 0x%02X\n", tag);
                break;
        }
        
        offset += len;
    }
    
    // Validate required fields
    if (app_aid->length == 0) {
        printf("[INSTALL] ERROR: Application AID not provided\n");
        return false;
    }
    
    if (module_aid->length == 0) {
        printf("[INSTALL] ERROR: Module AID not provided\n");
        return false;
    }
    
    return true;
}

/**
 * @brief Allocate global data space for application instance
 * 
 * Copies global data template from module to application instance.
 * Each instance gets its own copy for isolation.
 * 
 * @param vm VM instance
 * @param app Application instance
 * @param reg Module registry entry
 * @return GCOS_SUCCESS on success, error code otherwise
 */
static GCOSResult allocate_instance_global_data(GCOSVM *vm,
                                                GCOSAppInstance *app,
                                                GCOSModuleRegistry *reg) {
    if (reg->global_data_size == 0) {
        // No global data template, nothing to allocate
        app->app_domain_data = NULL;
        app->app_domain_data_size = 0;
        return GCOS_SUCCESS;
    }
    
    // Check if we have space in static allocation
    // For now, use a simple approach: store pointer to module's global data
    // In a real implementation, each instance would have its own copy
    
    // ⭐ Simplified: Point to module's global data template
    // TODO: Implement per-instance global data copying
    app->app_domain_data = (u8 *)reg->global_data_template;
    app->app_domain_data_size = reg->global_data_size;
    
    printf("[INSTALL] Global data allocated: %u bytes\n", reg->global_data_size);
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * INSTALL Command Handler
 * ============================================================================ */

/**
 * @brief Handle INSTALL command (INS=0xE2)
 * 
 * Creates an application instance from a loaded module.
 * 
 * APDU Format:
 *   CLA INS P1 P2 Lc [Data]
 *   80  E2 xx yy zz  [TLV data]
 * 
 * P1: Installation mode
 *   0x00 = INSTALL FOR MAKE SELECTABLE (create and make selectable)
 *   0x02 = INSTALL FOR INSTALL (create with initialization)
 *   0x04 = INSTALL FOR LOAD (not used here, handled by LOAD command)
 * 
 * P2: Installation parameters
 * 
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 handle_install_command(const u8 *apdu, u16 apdu_len,
                           u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    if (vm == NULL) {
        return 0x6F00;  // SW_NO_PRECISE_DIAGNOSIS
    }
    
    printf("[INSTALL] === INSTALL COMMAND ===\n");
    
    // Parse APDU header
    u8 p1 = apdu[2];  // Installation mode
    u8 p2 = apdu[3];  // Installation parameters
    u8 lc = (apdu_len > 4) ? apdu[4] : 0;
    
    printf("[INSTALL] Mode: P1=0x%02X, P2=0x%02X, Lc=%u\n", p1, p2, lc);
    
    // Validate installation mode
    if (p1 != 0x00 && p1 != 0x02) {
        printf("[INSTALL] ERROR: Invalid installation mode 0x%02X\n", p1);
        return 0x6A86;  // SW_INCORRECT_P1P2
    }
    
    // Parse INSTALL data
    if (lc == 0 || apdu_len < 5) {
        printf("[INSTALL] ERROR: No installation data\n");
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    const u8 *install_data = &apdu[5];
    u16 install_data_len = lc;
    
    GCOSAID app_aid, module_aid;
    const u8 *install_params = NULL;
    u16 install_params_len = 0;
    
    if (!parse_install_data(install_data, install_data_len,
                            &app_aid, &module_aid,
                            &install_params, &install_params_len)) {
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    // Step 1: Find module by AID
    printf("[INSTALL] Step 1: Finding module...\n");
    GCOSModuleRegistry *reg = module_registry_find_by_aid(vm, &module_aid);
    
    if (reg == NULL) {
        printf("[INSTALL] ERROR: Module not found\n");
        return 0x6A88;  // SW_REFERENCED_DATA_NOT_FOUND
    }
    
    if (!reg->is_loaded) {
        printf("[INSTALL] ERROR: Module not loaded\n");
        return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
    }
    
    printf("[INSTALL] Module found: ID=%u\n", reg->module_id);
    
    // Step 2: Check if application AID already exists
    printf("[INSTALL] Step 2: Checking for duplicate AID...\n");
    GCOSAppInstance *existing_app = app_find_by_aid(vm, app_aid.aid, app_aid.length);
    
    if (existing_app != NULL) {
        printf("[INSTALL] ERROR: Application AID already exists\n");
        return 0x6A89;  // SW_FILE_ALREADY_EXISTS
    }
    
    // Step 3: Create application instance
    printf("[INSTALL] Step 3: Creating application instance...\n");
    
    u8 new_app_id = 0xFF;
    GCOSResult result = app_register_ex(vm,
                                        &app_aid,
                                        dummy_process,
                                        NULL,  // on_select
                                        NULL,  // on_deselect
                                        NULL,  // on_install (will be called later)
                                        reg->module_id,
                                        APP_TYPE_REGULAR,
                                        0xFF,  // security_domain_id (ISD)
                                        0x00,  // privileges
                                        &new_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[INSTALL] ERROR: Failed to register application: %d\n", result);
        return 0x6F00;  // SW_EXECUTION_ERROR
    }
    
    GCOSAppInstance *app = app_find_by_id(vm, new_app_id);
    if (app == NULL) {
        printf("[INSTALL] ERROR: Application registration failed\n");
        return 0x6F00;
    }
    
    // Link application to module
    app->module = reg;
    
    printf("[INSTALL] Application created: ID=%u\n", new_app_id);
    
    // Step 4: Add instance to module registry
    printf("[INSTALL] Step 4: Adding instance to module registry...\n");
    result = module_registry_add_instance(vm, reg->module_id, new_app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[INSTALL] ERROR: Failed to add instance to registry: %d\n", result);
        // Cleanup: delete the application
        app_delete(vm, new_app_id);
        return 0x6F00;
    }
    
    printf("[INSTALL] Instance added. Total instances: %u\n",
           module_registry_get_instance_count(vm, reg->module_id));
    
    // Step 5: Allocate global data for instance
    printf("[INSTALL] Step 5: Allocating global data...\n");
    result = allocate_instance_global_data(vm, app, reg);
    
    if (result != GCOS_SUCCESS) {
        printf("[INSTALL] ERROR: Failed to allocate global data: %d\n", result);
        module_registry_remove_instance(vm, reg->module_id, new_app_id);
        app_delete(vm, new_app_id);
        return 0x6F00;
    }
    
    // Step 6: Call module's install method (if exists)
    if (p1 == 0x02 && install_params != NULL && install_params_len > 0) {
        printf("[INSTALL] Step 6: Calling install method...\n");
        
        // TODO: Resolve and call the module's install function
        // For now, just set lifecycle state
        printf("[INSTALL] Install method call (placeholder)\n");
    }
    
    // Step 7: Set lifecycle state
    if (p1 == 0x00) {
        // INSTALL FOR MAKE SELECTABLE
        app->lifecycle = APPLICATION_SELECTABLE;
        printf("[INSTALL] Lifecycle: SELECTABLE\n");
    } else if (p1 == 0x02) {
        // INSTALL FOR INSTALL
        app->lifecycle = APPLICATION_INSTALLED;
        printf("[INSTALL] Lifecycle: INSTALLED\n");
    }
    
    // Success
    printf("[INSTALL] Installation completed successfully\n");
    
    if (resp_len) {
        *resp_len = 0;
    }
    
    return 0x9000;  // SW_SUCCESS
}
