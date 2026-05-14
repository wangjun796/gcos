/**
 * @file gcos_app_manager.h
 * @brief GCOS VM Application Manager
 * 
 * Manages application lifecycle, selection, and APDU dispatching.
 * Based on cref architecture: each applet has a single process() method.
 */

#ifndef GCOS_APP_MANAGER_H
#define GCOS_APP_MANAGER_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Application Manager API
 * ========================================================================== */

/**
 * @brief Initialize application manager (create ISD)
 * 
 * Called during VM initialization to create the Initial Security Domain.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult app_manager_init(GCOSVM *vm);

/**
 * @brief Register an application to the application table
 * 
 * Called during INSTALL command to register a new application instance.
 * 
 * @param vm VM instance
 * @param app_aid Application AID
 * @param process_func Application's process() method pointer
 * @param on_select Application's select() callback (optional, can be NULL)
 * @param on_deselect Application's deselect() callback (optional, can be NULL)
 * @param module_index Associated module index
 * @param[out] app_id Output application ID
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult app_register(GCOSVM *vm, 
                        const GCOSAID *app_aid,
                        u16 (*process_func)(GCOSAppInstance *, const u8 *, u16, u8 *, u16 *),
                        GCOSResult (*on_select)(GCOSAppInstance *),
                        void (*on_deselect)(GCOSAppInstance *),
                        u16 module_index,
                        u8 *app_id);

/**
 * @brief Register a new application (extended version with full parameters)
 * 
 * Creates a new application instance with specified type, security domain, and privileges.
 * 
 * @param vm VM instance
 * @param app_aid Application AID
 * @param process_func Process method pointer (required)
 * @param on_select Select callback (optional, can be NULL)
 * @param on_deselect Deselect callback (optional, can be NULL)
 * @param on_install Install callback (optional, can be NULL)
 * @param module_index Module index
 * @param app_type Application type (APP_TYPE_REGULAR, APP_TYPE_ISD, etc.)
 * @param security_domain_id Security domain ID (0xFF = ISD)
 * @param privilege_byte1 Privilege byte 1
 * @param[out] app_id Output: assigned application ID
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult app_register_ex(GCOSVM *vm,
                           const GCOSAID *app_aid,
                           u16 (*process_func)(GCOSAppInstance *, const u8 *, u16, u8 *, u16 *),
                           GCOSResult (*on_select)(GCOSAppInstance *),
                           void (*on_deselect)(GCOSAppInstance *),
                           GCOSResult (*on_install)(GCOSAppInstance *, const u8 *, u16),
                           u16 module_index,
                           GCOSAppType app_type,
                           u8 security_domain_id,
                           u8 privilege_byte1,
                           u8 *app_id);

/**
 * @brief Find application by AID
 * 
 * @param vm VM instance
 * @param aid AID data
 * @param aid_len AID length
 * @return Application instance pointer, NULL if not found
 */
GCOSAppInstance* app_find_by_aid(GCOSVM *vm, const u8 *aid, u8 aid_len);

/**
 * @brief Find application by ID
 * 
 * @param vm VM instance
 * @param app_id Application ID
 * @return Application instance pointer, NULL if not found
 */
GCOSAppInstance* app_find_by_id(GCOSVM *vm, u8 app_id);

/**
 * @brief Select an application
 * 
 * 1. Find application
 * 2. Validate state
 * 3. Call on_select() callback if exists
 * 4. Update vm->selected_app
 * 
 * @param vm VM instance
 * @param app_id Application ID
 * @param channel Channel number
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult app_select(GCOSVM *vm, u8 app_id, u8 channel);

/**
 * @brief Deselect an application
 * 
 * 1. Call on_deselect() callback if exists
 * 2. Clear vm->selected_app
 * 
 * @param vm VM instance
 * @param channel Channel number
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult app_deselect(GCOSVM *vm, u8 channel);

/**
 * @brief Delete an application
 * 
 * @param vm VM instance
 * @param app_id Application ID
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult app_delete(GCOSVM *vm, u8 app_id);

/**
 * @brief Get currently selected application
 * 
 * @param vm VM instance
 * @return Selected application instance, NULL if none selected
 */
GCOSAppInstance* app_get_selected(GCOSVM *vm);

/**
 * @brief Check if an application is selected
 * 
 * @param vm VM instance
 * @return true if an application is selected, false otherwise
 */
bool app_is_selected(GCOSVM *vm);

/**
 * @brief LOAD command handler (INS=0xE4)
 * 
 * Implements three-phase loading:
 * - P1=0x00: INSTALL FOR LOAD
 * - P1=0x01: LOAD BLOCKS
 * - P1=0x02: FINALIZE
 * 
 * @param app ISD application instance
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 isd_handler_load(GCOSAppInstance *app,
                     const u8 *apdu,
                     u16 apdu_len,
                     u8 *response,
                     u16 *resp_len);

/**
 * @brief Reset LOAD context (for testing)
 * 
 * @param vm VM instance
 */
void reset_load_context(GCOSVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_APP_MANAGER_H */
