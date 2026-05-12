/**
 * @file gcos_jcshell.h
 * @brief JCShell server API (cref-compatible)
 * 
 * Provides a TCP server that listens on ports 9000 and 9900,
 * exactly like cref's jcshell implementation.
 */

#ifndef GCOS_JCSHELL_H
#define GCOS_JCSHELL_H

#include "gcos_vm.h"

/* ============================================================================
 * JCShell Server API
 * ============================================================================ */

/**
 * @brief Initialize JCShell server
 * 
 * Creates and binds TCP sockets on ports 9000 (contacted) and 9900 (contactless).
 * Must be called before gcos_jcshell_start().
 * 
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_jcshell_init(void);

/**
 * @brief Start JCShell server threads
 * 
 * Spawns two threads to accept client connections on both ports.
 * This function returns immediately; the server runs in background threads.
 * 
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_jcshell_start(void);

/**
 * @brief Cleanup JCShell server resources
 * 
 * Closes listen sockets and releases resources.
 */
void gcos_jcshell_cleanup(void);

#endif /* GCOS_JCSHELL_H */
