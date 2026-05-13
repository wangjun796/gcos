#ifndef GCOS_TRANSPORT_V2_H
#define GCOS_TRANSPORT_V2_H

#include "gcos_vm.h"
#include "gcos_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Transport Protocol Definitions
 * ============================================================================ */

/**
 * @brief Transport protocol type
 */
typedef enum {
    TRANSPORT_PROTOCOL_T0 = 0,        /**< T=0 protocol (ISO 7816-3) */
    TRANSPORT_PROTOCOL_TCL = 2        /**< T=CL protocol (ISO 14443-4) */
} TransportProtocol;

/* ============================================================================
 * Transport API - Version 2 (Refactored)
 * ============================================================================ */

/**
 * @brief Initialize transport layer
 * 
 * Sets up the transport layer with specified protocol.
 * 
 * @param protocol  Transport protocol (T0 or TCL)
 * @param port      Port number (for TCP mode)
 * @return GCOSResult
 */
GCOSResult transport_init_v2(TransportProtocol protocol, u16 port);

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
s16 transport_receive_apdu_v2(u8 *apdu_buffer, u16 max_len);

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
GCOSResult transport_send_response_v2(const u8 *data, u16 data_len, u16 sw);

/**
 * @brief Cleanup transport layer
 * 
 * Releases all transport resources.
 */
void transport_cleanup_v2(void);

/**
 * @brief Get current transport protocol
 * 
 * @return Current protocol type
 */
TransportProtocol transport_get_protocol_v2(void);

/**
 * @brief Check if transport is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool transport_is_initialized_v2(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_TRANSPORT_V2_H */
