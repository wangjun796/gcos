#ifndef GCOS_TRANSPORT_H
#define GCOS_TRANSPORT_H

#include "gcos_vm.h"
#include "gcos_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Transport Protocol Definitions
 * ============================================================================ */

/**
 * @brief Transport protocol type (simplified from v2)
 */
typedef enum {
    TRANSPORT_PROTOCOL_T0 = 0,        /**< T=0 protocol (ISO 7816-3) */
    TRANSPORT_PROTOCOL_TCL = 2        /**< T=CL protocol (ISO 14443-4) */
} TransportProtocol;

/* ============================================================================
 * Backward Compatibility: Old Transport Mode Enum
 * ============================================================================ */

/**
 * @brief Legacy transport mode (deprecated, use TransportProtocol instead)
 * @note Kept for backward compatibility with gcos_main.c
 */
typedef enum {
    TRANSPORT_MODE_TCP_SERVER = 0,      /**< TCP server (accept client connections) */
    TRANSPORT_MODE_SERIAL = 1,          /**< Serial port (for real card hardware) */
    TRANSPORT_MODE_JCSHELL = 2,         /**< JCShell server (TLP224 protocol, ports 9000/9900) */
    TRANSPORT_MODE_TLP_SERVER = 3       /**< TLP Server for JCRE (port 9025, cref-compatible) */
} TransportMode;

/* ============================================================================
 * Transport API (Unified - based on v2 implementation)
 * ============================================================================ */

/**
 * @brief Initialize transport layer
 * 
 * Sets up the transport layer with specified protocol.
 * Uses HAL for hardware abstraction.
 * 
 * @param protocol  Transport protocol (T0 or TCL)
 * @param port      Port number (for TCP mode)
 * @return GCOSResult
 */
GCOSResult gcos_transport_init(TransportProtocol protocol, u16 port);

/**
 * @brief Receive complete APDU command
 * 
 * Receives and parses APDU according to protocol specifications.
 * For T=0: receives header + data bytes
 * For T=CL: receives complete APDU frame
 * 
 * @param apdu_buffer  APDU receive buffer
 * @param max_len      Maximum buffer length
 * @return APDU length (>0), 0 if no data, -1 on error
 */
s16 gcos_transport_receive_apdu(u8 *apdu_buffer, u16 max_len);

/**
 * @brief Send APDU response
 * 
 * Formats and sends response data with status word.
 * 
 * @param data       Response data (can be NULL)
 * @param data_len   Response data length
 * @param sw         Status word (SW1SW2)
 * @return GCOSResult
 */
GCOSResult gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw);

/**
 * @brief Cleanup transport layer
 * 
 * Releases all transport resources.
 */
void gcos_transport_cleanup(void);

/**
 * @brief Get current transport protocol
 * 
 * @return Current protocol type
 */
TransportProtocol gcos_transport_get_protocol(void);

/**
 * @brief Check if transport is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool gcos_transport_is_initialized(void);

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

#ifdef __cplusplus
}
#endif

#endif /* GCOS_TRANSPORT_H */
