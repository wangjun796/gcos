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
    TRANSPORT_MODE_TCP_SERVER = 0,      /**< TCP server (accept client connections) */
    TRANSPORT_MODE_SERIAL = 1,          /**< Serial port (for real card hardware) */
    TRANSPORT_MODE_JCSHELL = 2,         /**< JCShell server (TLP224 protocol, ports 9000/9900) */
    TRANSPORT_MODE_TLP_SERVER = 3       /**< TLP Server for JCRE (port 9025, cref-compatible) */
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

/* ============================================================================
 * Low-Level Byte I/O (for TLP224 encoding/decoding)
 * ============================================================================ */

/**
 * @brief Send a single byte via transport layer
 * 
 * Used by TLP224 encoding to send ASCII hex characters.
 * 
 * @param byte  Byte to send
 * @return      0 on success, -1 on error
 */
s8 gcos_transport_send_byte(u8 byte);

/**
 * @brief Receive a single byte from transport layer
 * 
 * Used by TLP224 decoding to receive ASCII hex characters.
 * 
 * @param byte  Pointer to store received byte
 * @return      0 on success, -1 on error
 */
s8 gcos_transport_receive_byte(u8 *byte);

#endif /* GCOS_TRANSPORT_H */
