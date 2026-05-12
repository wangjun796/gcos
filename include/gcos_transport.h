#ifndef GCOS_TRANSPORT_H
#define GCOS_TRANSPORT_H

#include "gcos_vm.h"

/* ============================================================================
 * Transport Mode Definitions
 * ============================================================================ */

/**
 * @brief Transport mode for APDU communication
 */
typedef enum {
    TRANSPORT_MODE_STDIO = 0,       /**< Standard input/output (interactive) */
    TRANSPORT_MODE_TCP_SERVER = 1,  /**< TCP server (accept client connections) */
    TRANSPORT_MODE_SERIAL = 2       /**< Serial port (for real card hardware) */
} TransportMode;

/* ============================================================================
 * Transport API
 * ============================================================================ */

/**
 * @brief Initialize transport layer
 * 
 * @param mode  Transport mode (STDIO, TCP_SERVER, SERIAL)
 * @param port  Port number (only used for TCP_SERVER mode)
 * @return      GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_transport_init(TransportMode mode, u16 port);

/**
 * @brief Receive APDU from terminal/card reader
 * 
 * Blocks until an APDU is received or connection is closed.
 * 
 * @param buffer    Buffer to store received APDU
 * @param max_len   Maximum buffer length
 * @return          Actual APDU length, or 0 if connection closed/EOF
 */
u16 gcos_transport_receive_apdu(u8 *buffer, u16 max_len);

/**
 * @brief Send response to terminal/card reader
 * 
 * @param data      Response data (can be NULL if no data)
 * @param data_len  Length of response data
 * @param sw        Status word (SW1SW2)
 */
void gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw);

/**
 * @brief Cleanup transport resources
 * 
 * Closes sockets and releases resources.
 */
void gcos_transport_cleanup(void);

/**
 * @brief Get current transport mode
 * 
 * @return Current transport mode
 */
TransportMode gcos_transport_get_mode(void);

#endif /* GCOS_TRANSPORT_H */
