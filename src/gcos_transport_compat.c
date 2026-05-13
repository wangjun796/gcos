/**
 * @file gcos_transport_compat.c
 * @brief Backward compatibility layer for old transport API
 * 
 * This module provides compatibility wrappers so that existing code
 * using the old gcos_transport.h API continues to work without changes.
 * 
 * It maps old API calls to the new v2 implementation.
 */

#include "gcos_transport.h"
#include "gcos_transport_v2.h"
#include <stdio.h>

/* ============================================================================
 * Internal State Mapping
 * ============================================================================ */

static TransportMode compat_mode = TRANSPORT_MODE_TCP_SERVER;
static u16 compat_port = 9000;

/* ============================================================================
 * Compatibility Implementation
 * ============================================================================ */

GCOSResult gcos_transport_init(TransportMode mode, u16 port) {
    printf("[Transport-Compat] Initializing (mode=%u, port=%u)\n", mode, port);
    
    compat_mode = mode;
    compat_port = port;
    
    /* Map old mode to new protocol */
    TransportProtocol protocol;
    switch (mode) {
        case TRANSPORT_MODE_TCP_SERVER:
        case TRANSPORT_MODE_JCSHELL:
            /* Default to T=0 for contacted interface */
            protocol = TRANSPORT_PROTOCOL_T0;
            break;
            
        case TRANSPORT_MODE_SERIAL:
            /* For serial, assume T=0 */
            protocol = TRANSPORT_PROTOCOL_T0;
            break;
            
        default:
            protocol = TRANSPORT_PROTOCOL_T0;
            break;
    }
    
    /* Call new v2 API */
    return transport_init_v2(protocol, port);
}

u16 gcos_transport_receive_apdu(u8 *buffer, u16 max_len) {
    if (buffer == NULL || max_len == 0) {
        return 0;
    }
    
    /* Call new v2 API */
    s16 len = transport_receive_apdu_v2(buffer, max_len);
    
    /* Convert to unsigned (old API returns u16) */
    if (len > 0) {
        return (u16)len;
    }
    
    return 0;
}

void gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw) {
    /* Call new v2 API */
    GCOSResult result = transport_send_response_v2(data, data_len, sw);
    
    if (result != GCOS_SUCCESS) {
        printf("[Transport-Compat] WARNING: Send response failed\n");
    }
}

void gcos_transport_cleanup(void) {
    printf("[Transport-Compat] Cleaning up\n");
    
    /* Call new v2 API */
    transport_cleanup_v2();
}

TransportMode gcos_transport_get_mode(void) {
    return compat_mode;
}

s8 gcos_transport_send_byte(u8 byte) {
    /* For backward compatibility, send single byte via HAL */
    s16 sent = hal_write(&byte, 1);
    return (sent == 1) ? 0 : -1;
}

s8 gcos_transport_receive_byte(u8 *byte) {
    if (byte == NULL) {
        return -1;
    }
    
    /* For backward compatibility, receive single byte via HAL */
    s16 received = hal_read(byte, 1);
    return (received == 1) ? 0 : -1;
}
