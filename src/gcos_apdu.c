/**
 * @file gcos_apdu.c
 * @brief GCOS VM APDU Processing Implementation
 * 
 * Implements APDU command dispatching based on cref architecture.
 */

#include "gcos_apdu.h"
#include "gcos_app_manager.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * GP Management Command Detection
 * ============================================================================ */

bool is_gp_management_command(u8 cla, u8 ins) {
    // GP management commands (handled by ISD)
    switch (ins) {
        case 0xA4:  // SELECT ⭐
        case 0x50:  // INITIALIZE UPDATE
        case 0x82:  // EXTERNAL AUTHENTICATE
        case 0xCA:  // GET DATA (specific tags)
        case 0xDA:  // PUT DATA (specific tags)
        case 0xE2:  // DELETE
        case 0xE4:  // LOAD
        case 0xE6:  // INSTALL
        case 0xF2:  // GET STATUS
            return true;
        default:
            return false;
    }
}

/* ============================================================================
 * APDU Processing Core Function
 * ============================================================================ */

u16 gcos_process_apdu(GCOSVM *vm, 
                      const u8 *apdu, 
                      u16 apdu_len,
                      u8 *response, 
                      u16 *resp_len) {
    // Step 1: Basic validation
    if (vm == NULL || apdu == NULL || apdu_len < 4) {
        printf("[APDU] ERROR: Invalid parameters\n");
        return SW_WRONG_LENGTH;
    }
    
    u8 cla = apdu[0];
    u8 ins = apdu[1];
    
    printf("[APDU] CLA=%02X INS=%02X P1=%02X P2=%02X\n", cla, ins, apdu[2], apdu[3]);
    
    // Step 2: Check if it's a GP management command
    if (is_gp_management_command(cla, ins)) {
        printf("[APDU] GP management command detected\n");
        
        // GP commands are always handled by ISD
        GCOSAppInstance *isd = app_find_by_id(vm, APP_FIRST);
        
        if (isd == NULL) {
            printf("[APDU] ERROR: ISD not found!\n");
            return SW_NO_PRECISE_DIAGNOSIS;
        }
        
        // Call ISD's process() method
        if (isd->process == NULL) {
            printf("[APDU] ERROR: ISD has no process handler!\n");
            return SW_NO_PRECISE_DIAGNOSIS;
        }
        
        printf("[APDU] Dispatching to ISD\n");
        return isd->process(isd, apdu, apdu_len, response, resp_len);
    }
    
    // Step 3: Non-GP command requires selected application
    if (vm->selected_app == NULL) {
        printf("[APDU] ERROR: No application selected\n");
        return SW_NO_PRECISE_DIAGNOSIS;  // 0x6F00
    }
    
    GCOSAppInstance *app = vm->selected_app;
    
    printf("[APDU] Dispatching to application. AID: ");
    for (int i = 0; i < app->app_aid.length; i++) {
        printf("%02X", app->app_aid.aid[i]);
    }
    printf("\n");
    
    // Step 4: Check if application has process method
    if (app->process == NULL) {
        printf("[APDU] ERROR: Application has no process handler!\n");
        return SW_NO_PRECISE_DIAGNOSIS;
    }
    
    // Step 5: ⭐ Call application's process() method
    // Similar to cref's run_app(process_method_offset)
    printf("[APDU] Calling app->process()\n");
    u16 sw = app->process(app, apdu, apdu_len, response, resp_len);
    
    printf("[APDU] process() returned SW=0x%04X\n", sw);
    return sw;
}

/* ============================================================================
 * Compatibility Layer (for backward compatibility)
 * ============================================================================ */

/**
 * @brief Initialize APDU processing (compatibility function)
 */
GCOSResult gcos_apdu_init(GCOSVM *vm) {
    // Nothing to initialize for now
    if (vm == NULL) {
        return GCOS_ERROR_INVALID_PARAM;
    }
    printf("[APDU] APDU processing initialized\n");
    return GCOS_SUCCESS;
}

/**
 * @brief Process APDU with connection type (compatibility function)
 */
u16 gcos_vm_process_apdu_with_conn_type(GCOSVM *vm,
                                        const u8 *apdu,
                                        u16 apdu_len,
                                        u8 *response,
                                        u16 *resp_len,
                                        int conn_type) {
    // Ignore conn_type for now, just call the main function
    return gcos_process_apdu(vm, apdu, apdu_len, response, resp_len);
}

/**
 * @brief Process APDU command (old API name for compatibility)
 */
u16 gcos_vm_process_apdu(GCOSVM *vm,
                         const u8 *apdu,
                         u16 apdu_len,
                         u8 *response,
                         u16 *resp_len) {
    return gcos_process_apdu(vm, apdu, apdu_len, response, resp_len);
}
