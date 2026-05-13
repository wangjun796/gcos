/**
 * @file gcos_tlp_server.h
 * @brief TLP Server header (equivalent to JCRE server in cref)
 */

#ifndef GCOS_TLP_SERVER_H
#define GCOS_TLP_SERVER_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize TLP server (listens on port 9025)
 * @param vm Pointer to VM instance
 * @return GCOS_SUCCESS on success
 */
GCOSResult gcos_tlp_server_init(GCOSVM* vm);

/**
 * @brief Start TLP server (blocking)
 * @return GCOS_SUCCESS on success
 */
GCOSResult gcos_tlp_server_start(void);

/**
 * @brief Cleanup TLP server
 */
void gcos_tlp_server_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_TLP_SERVER_H */
