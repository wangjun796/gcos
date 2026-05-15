/**
 * @file gcos_install_manager.h
 * @brief GCOS VM INSTALL Command API
 * 
 * Provides functions for creating application instances from loaded modules.
 */

#ifndef GCOS_INSTALL_MANAGER_H
#define GCOS_INSTALL_MANAGER_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle INSTALL command (INS=0xE2)
 * 
 * Creates an application instance from a loaded module.
 * 
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 handle_install_command(const u8 *apdu, u16 apdu_len,
                           u8 *response, u16 *resp_len);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_INSTALL_MANAGER_H */
