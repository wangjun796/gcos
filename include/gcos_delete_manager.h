/**
 * @file gcos_delete_manager.h
 * @brief GCOS VM DELETE Command API
 * 
 * Implements GlobalPlatform DELETE command (INS=0xE6) for removing
 * applications and modules from the card.
 * 
 * Reference: GlobalPlatform Card Specification v2.3.1 - DELETE Command
 */

#ifndef GCOS_DELETE_MANAGER_H
#define GCOS_DELETE_MANAGER_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle DELETE command (INS=0xE6)
 * 
 * Deletes applications and/or modules based on AID.
 * Supports single and batch deletion.
 * 
 * APDU Format:
 *   CLA INS P1 P2 Lc [Data]
 *   80  E6 xx yy zz  [TLV data]
 * 
 * P1: Deletion options
 *   Bit 0 = 1: Delete related objects (child apps when deleting module)
 *   Bit 1 = 1: Delete from package (delete applet instances)
 *   Bit 2 = 1: Delete package (delete module itself)
 * 
 * Data (TLV):
 *   Tag 0x4F: AID(s) to delete (can be multiple)
 * 
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 handle_delete_command(const u8 *apdu, u16 apdu_len,
                          u8 *response, u16 *resp_len);

/**
 * @brief Delete application by AID
 * 
 * Helper function to delete a single application.
 * 
 * @param vm VM instance
 * @param aid Application AID
 * @param aid_length AID length
 * @param delete_related Also delete related objects
 * @return Status word
 */
u16 delete_app_by_aid(GCOSVM *vm, const u8 *aid, u8 aid_length, bool delete_related);

/**
 * @brief Delete module by AID
 * 
 * Helper function to delete a module (package).
 * Will fail if module has active instances unless delete_related is true.
 * 
 * @param vm VM instance
 * @param aid Module AID
 * @param aid_length AID length
 * @param delete_related Also delete all app instances of this module
 * @return Status word
 */
u16 delete_module_by_aid(GCOSVM *vm, const u8 *aid, u8 aid_length, bool delete_related);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_DELETE_MANAGER_H */
