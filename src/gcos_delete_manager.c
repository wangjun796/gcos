/**
 * @file gcos_delete_manager.c
 * @brief GCOS VM DELETE Command Implementation
 * 
 * Implements GlobalPlatform DELETE command (INS=0xE6) for removing
 * applications and modules from the card.
 * 
 * Reference: GlobalPlatform Card Specification v2.3.1 - DELETE Command
 */

#include "gcos_vm.h"
#include "gcos_delete_manager.h"
#include "gcos_app_manager.h"
#include "gcos_module_registry.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Parse DELETE command data (TLV format)
 * 
 * Expected TLV structure:
 *   Tag 0x4F: AID(s) to delete
 *   Can contain multiple AIDs in sequence
 * 
 * @param data DELETE command data
 * @param data_len Data length
 * @param aids Output array of AIDs
 * @param aid_count Output number of AIDs found
 * @param max_aids Maximum number of AIDs that can be stored
 * @return true if parsing successful, false otherwise
 */
static bool parse_delete_data(const u8 *data, u16 data_len,
                              GCOSAID *aids, u8 *aid_count, u8 max_aids) {
    u16 offset = 0;
    *aid_count = 0;
    
    while (offset < data_len && *aid_count < max_aids) {
        if (offset + 2 > data_len) {
            printf("[DELETE] ERROR: Invalid TLV structure\n");
            return false;
        }
        
        u8 tag = data[offset++];
        u8 len = data[offset++];
        
        if (tag != 0x4F) {
            printf("[DELETE] WARNING: Unknown tag 0x%02X, skipping\n", tag);
            offset += len;
            continue;
        }
        
        if (len < 5 || len > 16) {
            printf("[DELETE] ERROR: Invalid AID length %u\n", len);
            return false;
        }
        
        if (offset + len > data_len) {
            printf("[DELETE] ERROR: AID extends beyond data\n");
            return false;
        }
        
        // Store AID
        aids[*aid_count].length = len;
        memcpy(aids[*aid_count].aid, &data[offset], len);
        (*aid_count)++;
        
        printf("[DELETE] Found AID to delete: ");
        for (int i = 0; i < len; i++) {
            printf("%02X", data[offset + i]);
        }
        printf("\n");
        
        offset += len;
    }
    
    if (*aid_count == 0) {
        printf("[DELETE] ERROR: No AIDs found in data\n");
        return false;
    }
    
    return true;
}

/**
 * @brief Check if AID corresponds to a module or application
 * 
 * Priority: If both app and module exist with same AID prefix, prefer app.
 * But for exact match, check both.
 * 
 * @param vm VM instance
 * @param aid AID to check
 * @return 0 = not found, 1 = application, 2 = module
 */
static int identify_aid_type(GCOSVM *vm, const GCOSAID *aid) {
    // Check if it's a module (exact match first)
    GCOSModuleRegistry *reg = NULL;
    for (u8 i = 0; i < MAX_MODULES; i++) {
        if (vm->module_registry[i].is_loaded &&
            vm->module_registry[i].module_aid.length == aid->length &&
            memcmp(vm->module_registry[i].module_aid.aid, aid->aid, aid->length) == 0) {
            reg = &vm->module_registry[i];
            break;
        }
    }
    
    // If exact module match found, return module
    if (reg != NULL) {
        return 2;  // Module
    }
    
    // Otherwise check for application (prefix match is OK for apps)
    GCOSAppInstance *app = app_find_by_aid(vm, aid->aid, aid->length);
    if (app != NULL) {
        return 1;  // Application
    }
    
    return 0;  // Not found
}

/* ============================================================================
 * DELETE Command Handlers
 * ============================================================================ */

u16 delete_app_by_aid(GCOSVM *vm, const u8 *aid, u8 aid_length, bool delete_related) {
    if (vm == NULL || aid == NULL || aid_length == 0) {
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    printf("[DELETE] Deleting application by AID...\n");
    
    // Find application
    GCOSAppInstance *app = app_find_by_aid(vm, aid, aid_length);
    
    if (app == NULL) {
        printf("[DELETE] ERROR: Application not found\n");
        return 0x6A82;  // SW_FILE_NOT_FOUND
    }
    
    u8 app_id = app->app_id;
    
    // Cannot delete ISD
    if (app_id == APP_FIRST) {
        printf("[DELETE] ERROR: Cannot delete ISD\n");
        return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
    }
    
    // Delete application (this will trigger GRT cleanup automatically)
    GCOSResult result = app_delete(vm, app_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[DELETE] ERROR: Failed to delete application: %d\n", result);
        return 0x6F00;  // SW_EXECUTION_ERROR
    }
    
    printf("[DELETE] Application deleted successfully\n");
    return 0x9000;  // SW_SUCCESS
}

u16 delete_module_by_aid(GCOSVM *vm, const u8 *aid, u8 aid_length, bool delete_related) {
    if (vm == NULL || aid == NULL || aid_length == 0) {
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    printf("[DELETE] Deleting module by AID...\n");
    
    // Find module
    GCOSModuleRegistry *reg = module_registry_find_by_aid(vm, &(GCOSAID){.length = aid_length});
    memcpy(reg ? reg->module_aid.aid : aid, aid, aid_length);
    
    // Better approach: search manually
    u8 module_id = 0xFF;
    for (u8 i = 0; i < MAX_MODULES; i++) {
        if (vm->module_registry[i].is_loaded &&
            vm->module_registry[i].module_aid.length == aid_length &&
            memcmp(vm->module_registry[i].module_aid.aid, aid, aid_length) == 0) {
            module_id = i;
            reg = &vm->module_registry[i];
            break;
        }
    }
    
    if (module_id == 0xFF || reg == NULL) {
        printf("[DELETE] ERROR: Module not found\n");
        return 0x6A82;  // SW_FILE_NOT_FOUND
    }
    
    printf("[DELETE] Found module ID=%u with %u instances\n",
           module_id, reg->instance_count);
    
    // Check if module has active instances
    if (reg->instance_count > 0) {
        if (!delete_related) {
            printf("[DELETE] ERROR: Module has %u active instances. Use delete_related flag.\n",
                   reg->instance_count);
            return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
        }
        
        // Delete all app instances first
        printf("[DELETE] Deleting %u app instances...\n", reg->instance_count);
        
        // Copy instance IDs since we'll be modifying the array
        u8 instance_ids[MAX_APPS_PER_MODULE];
        u8 count = reg->instance_count;
        memcpy(instance_ids, reg->instance_ids, count);
        
        for (u8 i = 0; i < count; i++) {
            if (instance_ids[i] != 0xFF) {
                printf("[DELETE] Deleting app instance %u...\n", instance_ids[i]);
                GCOSResult result = app_delete(vm, instance_ids[i]);
                
                if (result != GCOS_SUCCESS) {
                    printf("[DELETE] WARNING: Failed to delete app %u: %d\n",
                           instance_ids[i], result);
                }
            }
        }
        
        // Verify all instances are deleted
        if (reg->instance_count > 0) {
            printf("[DELETE] WARNING: Some instances still remain: %u\n", reg->instance_count);
        }
    }
    
    // Now unload the module
    printf("[DELETE] Unloading module...\n");
    GCOSResult result = module_registry_unload(vm, module_id);
    
    if (result != GCOS_SUCCESS) {
        printf("[DELETE] ERROR: Failed to unload module: %d\n", result);
        return 0x6F00;  // SW_EXECUTION_ERROR
    }
    
    // Decrement module count
    if (vm->module_count > 0) {
        vm->module_count--;
    }
    
    printf("[DELETE] Module unloaded successfully\n");
    return 0x9000;  // SW_SUCCESS
}

/* ============================================================================
 * Main DELETE Command Handler
 * ============================================================================ */

u16 handle_delete_command(const u8 *apdu, u16 apdu_len,
                          u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    if (vm == NULL) {
        return 0x6F00;  // SW_NO_PRECISE_DIAGNOSIS
    }
    
    printf("[DELETE] === DELETE COMMAND ===\n");
    
    // Parse APDU header
    u8 p1 = apdu[2];  // Deletion options
    u8 p2 = apdu[3];  // Reserved
    u8 lc = (apdu_len > 4) ? apdu[4] : 0;
    
    printf("[DELETE] Options: P1=0x%02X, P2=0x%02X, Lc=%u\n", p1, p2, lc);
    
    // Parse deletion flags from P1
    bool delete_package = (p1 & 0x04) != 0;      // Bit 2: Delete package (module)
    bool delete_from_package = (p1 & 0x02) != 0; // Bit 1: Delete from package (app instances)
    bool delete_related = (p1 & 0x01) != 0;      // Bit 0: Delete related objects
    
    printf("[DELETE] Flags: package=%d, from_package=%d, related=%d\n",
           delete_package, delete_from_package, delete_related);
    
    // Validate flags
    if (!delete_package && !delete_from_package) {
        printf("[DELETE] ERROR: No deletion type specified\n");
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    // Parse DELETE data
    if (lc == 0 || apdu_len < 5) {
        printf("[DELETE] ERROR: No data provided\n");
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    const u8 *delete_data = &apdu[5];
    u16 delete_data_len = lc;
    
    GCOSAID aids[16];  // Support up to 16 AIDs in one command
    u8 aid_count = 0;
    
    if (!parse_delete_data(delete_data, delete_data_len, aids, &aid_count, 16)) {
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    printf("[DELETE] Processing %u AID(s)...\n", aid_count);
    
    // Process each AID
    u16 last_sw = 0x9000;
    for (u8 i = 0; i < aid_count; i++) {
        printf("\n[DELETE] --- Processing AID %u/%u ---\n", i + 1, aid_count);
        
        // Identify what this AID refers to
        int aid_type = identify_aid_type(vm, &aids[i]);
        
        if (aid_type == 0) {
            printf("[DELETE] WARNING: AID not found, skipping\n");
            last_sw = 0x6A82;  // SW_FILE_NOT_FOUND
            continue;
        }
        
        if (aid_type == 1) {
            // It's an application
            if (!delete_from_package && !delete_related) {
                printf("[DELETE] WARNING: Skipping app (flags don't match)\n");
                last_sw = 0x6985;
                continue;
            }
            
            last_sw = delete_app_by_aid(vm, aids[i].aid, aids[i].length, delete_related);
            
        } else if (aid_type == 2) {
            // It's a module
            if (!delete_package) {
                printf("[DELETE] WARNING: Skipping module (flags don't match)\n");
                last_sw = 0x6985;
                continue;
            }
            
            last_sw = delete_module_by_aid(vm, aids[i].aid, aids[i].length, delete_related);
        }
        
        if (last_sw != 0x9000) {
            printf("[DELETE] ERROR: Failed at AID %u, stopping\n", i + 1);
            break;
        }
    }
    
    printf("\n[DELETE] DELETE command completed. SW=0x%04X\n", last_sw);
    
    if (resp_len) {
        *resp_len = 0;
    }
    
    return last_sw;
}
