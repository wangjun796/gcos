/**
 * @file gcos_transport_v2.c
 * @brief Refactored transport layer with protocol separation
 * 
 * This module implements the transport layer with clear separation between:
 * - Protocol handling (T=0, T=CL)
 * - Hardware communication (via HAL)
 * 
 * It replaces the old gcos_transport.c with a cleaner architecture.
 */

#include "gcos_transport_v2.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

static bool transport_initialized = false;
static TransportProtocol current_protocol = TRANSPORT_PROTOCOL_T0;

/* ============================================================================
 * T=0 Protocol Implementation
 * ============================================================================ */

/**
 * @brief T=0 APDU reception
 * 
 * T=0 protocol receives:
 * - 5 bytes header: CLA INS P1 P2 P3(Lc)
 * - Lc bytes data (if Lc > 0)
 * 
 * @param buffer   Receive buffer
 * @param max_len  Maximum length
 * @return APDU length, -1 on error
 */
static s16 t0_receive_apdu(u8 *buffer, u16 max_len) {
    if (buffer == NULL || max_len < 5) {
        return -1;
    }
    
    /* Step 1: Receive 5-byte header */
    s16 header_len = hal_read(buffer, 5);
    if (header_len < 5) {
        printf("[Transport-T0] ERROR: Failed to receive header\n");
        return -1;
    }
    
    printf("[Transport-T0] Received header: %02X %02X %02X %02X %02X\n",
           buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    
    /* Step 2: Check if there's data */
    u8 lc = buffer[4];
    if (lc > 0) {
        if ((u16)(5 + lc) > max_len) {
            printf("[Transport-T0] ERROR: APDU too large (%u bytes)\n", 5 + lc);
            return -1;
        }
        
        /* Receive data bytes */
        s16 data_len = hal_read(&buffer[5], lc);
        if (data_len < lc) {
            printf("[Transport-T0] ERROR: Failed to receive data\n");
            return -1;
        }
        
        printf("[Transport-T0] Received %u bytes of data\n", lc);
        return 5 + lc;
    }
    
    /* No data, just header */
    return 5;
}

/**
 * @brief T=0 response transmission
 * 
 * T=0 protocol sends:
 * - Response data (if any)
 * - SW1 SW2 (2 bytes)
 * 
 * @param data      Response data
 * @param data_len  Data length
 * @param sw        Status word
 * @return GCOSResult
 */
static GCOSResult t0_send_response(const u8 *data, u16 data_len, u16 sw) {
    u8 response_buffer[260];
    u16 total_len = 0;
    
    /* Copy response data */
    if (data != NULL && data_len > 0) {
        if (data_len > sizeof(response_buffer) - 2) {
            printf("[Transport-T0] ERROR: Response too large\n");
            return GCOS_ERR_INVALID_PARAM;
        }
        memcpy(response_buffer, data, data_len);
        total_len = data_len;
    }
    
    /* Append status word */
    response_buffer[total_len++] = (u8)(sw >> 8);
    response_buffer[total_len++] = (u8)(sw & 0xFF);
    
    /* Send via HAL */
    s16 sent = hal_write(response_buffer, total_len);
    if (sent != (s16)total_len) {
        printf("[Transport-T0] ERROR: Failed to send response\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    printf("[Transport-T0] Sent response (%u bytes, SW=0x%04X)\n", total_len, sw);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * T=CL Protocol Implementation
 * ============================================================================ */

/**
 * @brief T=CL APDU reception
 * 
 * T=CL protocol receives complete APDU frame in one shot.
 * 
 * @param buffer   Receive buffer
 * @param max_len  Maximum length
 * @return APDU length, -1 on error
 */
static s16 tcl_receive_apdu(u8 *buffer, u16 max_len) {
    if (buffer == NULL || max_len == 0) {
        return -1;
    }
    
    /* Receive complete APDU */
    s16 len = hal_read(buffer, max_len);
    if (len < 0) {
        printf("[Transport-TCL] ERROR: Failed to receive APDU\n");
        return -1;
    }
    
    if (len == 0) {
        printf("[Transport-TCL] Connection closed\n");
        return 0;
    }
    
    printf("[Transport-TCL] Received APDU (%d bytes)\n", len);
    return len;
}

/**
 * @brief T=CL response transmission
 * 
 * T=CL protocol sends complete response frame.
 * 
 * @param data      Response data
 * @param data_len  Data length
 * @param sw        Status word
 * @return GCOSResult
 */
static GCOSResult tcl_send_response(const u8 *data, u16 data_len, u16 sw) {
    u8 response_buffer[260];
    u16 total_len = 0;
    
    /* Copy response data */
    if (data != NULL && data_len > 0) {
        if (data_len > sizeof(response_buffer) - 2) {
            printf("[Transport-TCL] ERROR: Response too large\n");
            return GCOS_ERR_INVALID_PARAM;
        }
        memcpy(response_buffer, data, data_len);
        total_len = data_len;
    }
    
    /* Append status word */
    response_buffer[total_len++] = (u8)(sw >> 8);
    response_buffer[total_len++] = (u8)(sw & 0xFF);
    
    /* Send via HAL */
    s16 sent = hal_write(response_buffer, total_len);
    if (sent != (s16)total_len) {
        printf("[Transport-TCL] ERROR: Failed to send response\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    printf("[Transport-TCL] Sent response (%u bytes, SW=0x%04X)\n", total_len, sw);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Transport API Implementation
 * ============================================================================ */

GCOSResult transport_init_v2(TransportProtocol protocol, u16 port) {
    if (transport_initialized) {
        printf("[Transport] WARNING: Already initialized, cleaning up first\n");
        transport_cleanup_v2();
    }
    
    printf("[Transport] Initializing transport (protocol=%s, port=%u)...\n",
           protocol == TRANSPORT_PROTOCOL_T0 ? "T=0" : "T=CL",
           port);
    
    /* Initialize HAL */
    HalConfig hal_config;
    hal_config.port = port;
    hal_config.interface_type = (protocol == TRANSPORT_PROTOCOL_T0) ?
        HAL_INTERFACE_CONTACTED : HAL_INTERFACE_CONTACTLESS;
    hal_config.device_path = NULL;
    hal_config.baudrate = 0;
    
    GCOSResult result = hal_init(&hal_config);
    if (result != GCOS_SUCCESS) {
        printf("[Transport] ERROR: HAL initialization failed\n");
        return result;
    }
    
    current_protocol = protocol;
    transport_initialized = true;
    
    printf("[Transport] Transport initialized successfully\n");
    return GCOS_SUCCESS;
}

s16 transport_receive_apdu_v2(u8 *apdu_buffer, u16 max_len) {
    if (!transport_initialized || apdu_buffer == NULL) {
        return -1;
    }
    
    /* Dispatch to protocol-specific handler */
    switch (current_protocol) {
        case TRANSPORT_PROTOCOL_T0:
            return t0_receive_apdu(apdu_buffer, max_len);
            
        case TRANSPORT_PROTOCOL_TCL:
            return tcl_receive_apdu(apdu_buffer, max_len);
            
        default:
            printf("[Transport] ERROR: Unknown protocol\n");
            return -1;
    }
}

GCOSResult transport_send_response_v2(const u8 *data, u16 data_len, u16 sw) {
    if (!transport_initialized) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Dispatch to protocol-specific handler */
    switch (current_protocol) {
        case TRANSPORT_PROTOCOL_T0:
            return t0_send_response(data, data_len, sw);
            
        case TRANSPORT_PROTOCOL_TCL:
            return tcl_send_response(data, data_len, sw);
            
        default:
            printf("[Transport] ERROR: Unknown protocol\n");
            return GCOS_ERR_INVALID_PARAM;
    }
}

void transport_cleanup_v2(void) {
    if (!transport_initialized) {
        return;
    }
    
    printf("[Transport] Cleaning up transport layer...\n");
    
    /* Cleanup HAL */
    hal_cleanup();
    
    transport_initialized = false;
    printf("[Transport] Transport cleanup complete\n");
}

TransportProtocol transport_get_protocol_v2(void) {
    return current_protocol;
}

bool transport_is_initialized_v2(void) {
    return transport_initialized;
}
