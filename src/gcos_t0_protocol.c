/**
 * @file gcos_t0_protocol.c
 * @brief ISO7816-3 T=0 Protocol implementation
 * 
 * Implements the complete T=0 byte-level protocol for smart card communication.
 * Based on cref's t0.c, t0.h, t0_ll.c and t0_ll.h, translated to English and 
 * adapted for GCOS VM.
 * 
 * This module handles:
 * - TLP224 message framing and LRC validation
 * - POWER_UP/POWER_DOWN command processing
 * - ISO_INPUT/ISO_OUTPUT APDU reception
 * - Procedure byte validation per ISO7816-3
 * - Status word transmission with next command reception
 */

#include "gcos_t0_protocol.h"
#include "gcos_transport.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal State Variables
 * ============================================================================ */

static T0State current_state = T0_STATE_INIT;  /**< Current T=0 protocol state */
static u8 backed_up_header[APDU_HEADER_LENGTH]; /**< Backed up APDU header for 6Cxx handling */

/* Default ATR (Answer To Reset) - can be customized */
static const u8 default_atr[] = { 0x3B, 0xF4, 0x11, 0x00, 0xFF, 0x00 };
static const u8 default_hist[] = { 0x11, 0x22, 0x33, 0x44 };
static const u8 default_hist_len = 4;

/* ============================================================================
 * Helper Functions - Transport Layer Integration
 * ============================================================================ */

/**
 * @brief Receive a single byte from transport layer
 * 
 * @param byte  Pointer to store received byte
 * @return      0 on success, -1 on error
 */
static s8 receive_byte(u8 *byte) {
    u8 buffer[1];
    u16 len;
    
    if (byte == NULL) {
        return -1;
    }
    
    len = gcos_transport_receive_apdu(buffer, 1);
    if (len != 1) {
        return -1;
    }
    
    *byte = buffer[0];
    return 0;
}

/**
 * @brief Send a single byte via transport layer
 * 
 * @param byte  Byte to send
 * @return      0 on success, -1 on error
 */
static s8 send_byte(u8 byte) {
    u8 buffer[1] = { byte };
    
    gcos_transport_send_response(buffer, 1, 0x9000);
    return 0;
}

/**
 * @brief Receive N bytes from transport layer
 * 
 * @param msg     TLP message structure
 * @param data    Buffer to store received data
 * @param offset  Offset in buffer
 * @param length  Number of bytes to receive
 * @return        Number of bytes received, -1 on error
 */
static s16 get_n_bytes(TLP_MSG *msg, u8 *data, u16 offset, u16 length) {
    u16 i;
    u8 byte;
    
    if (msg == NULL || data == NULL || length == 0) {
        return -1;
    }
    
    for (i = 0; i < length; i++) {
        if (receive_byte(&byte) != 0) {
            return -1;
        }
        data[offset + i] = byte;
    }
    
    return (s16)length;
}

/**
 * @brief Send N bytes via transport layer
 * 
 * @param msg     TLP message structure
 * @param data    Buffer containing data to send
 * @param offset  Offset in buffer
 * @param length  Number of bytes to send
 * @return        0 on success, -1 on error
 */
static s8 put_n_bytes(TLP_MSG *msg, const u8 *data, u16 offset, u16 length) {
    if (msg == NULL || data == NULL || length == 0) {
        return -1;
    }
    
    /* For simplicity, send all data at once */
    gcos_transport_send_response(&data[offset], length, 0x9000);
    return 0;
}

/**
 * @brief Send status response message
 * 
 * Sends an ACK/NACK response with status code.
 * 
 * @param msg   TLP message structure
 * @param code  Status code
 * @return      0 on success, -1 on error
 */
static s8 status_response(TLP_MSG *msg, u8 code) {
    if (msg == NULL) {
        return -1;
    }
    
    msg->buf[0] = TLP_ACK;
    msg->buf[1] = 0;  /* Length high byte */
    msg->buf[2] = 1;  /* Length low byte (1 byte payload) */
    msg->buf[3] = code;
    msg->buf[4] = tlp_compute_lrc(msg->buf, 4);
    msg->len = 5;
    
    /* Send via transport layer */
    gcos_transport_send_response(msg->buf, msg->len, 0x9000);
    
    return 0;
}

/* ============================================================================
 * T=0 Protocol Initialization
 * ============================================================================ */

void t0_protocol_init(TLP_MSG *msg) {
    if (msg == NULL) {
        return;
    }
    
    tlp_msg_init(msg);
    current_state = T0_STATE_INIT;
    memset(backed_up_header, 0, APDU_HEADER_LENGTH);
    
    printf("[T=0] Protocol initialized, state: INIT\n");
}

/* ============================================================================
 * ATR (Answer To Reset) Handling
 * ============================================================================ */

s8 t0_send_atr(TLP_MSG *msg, u8 hist_len, const u8 *atr, const u8 *hist, bool need_recv) {
    u8 atr_len = TLP_ATR_LEN;
    u16 total_len;
    
    if (msg == NULL || atr == NULL) {
        return -1;
    }
    
    /* If need_recv is true, wait for POWER_UP command first */
    if (need_recv) {
        while (1) {
            u16 recv_len;
            
            recv_len = gcos_transport_receive_apdu(msg->buf, TLP_BUFFER_SIZE);
            if (recv_len == 0) {
                printf("[T=0] Error: Failed to receive POWER_UP\n");
                return -1;
            }
            
            msg->len = recv_len;
            
            /* Check if this is a POWER_UP command */
            if (msg->buf[3] != TLP_POWER_UP) {
                printf("[T=0] Warning: Expected POWER_UP, got 0x%02X\n", msg->buf[3]);
                status_response(msg, STATUS_CARD_TURNED_OFF);
                continue;
            }
            
            /* Validate POWER_UP message length (should be 4 bytes) */
            total_len = ((u16)msg->buf[1] << 8) | msg->buf[2];
            if (total_len != 4) {
                printf("[T=0] Error: Invalid POWER_UP length: %u\n", total_len);
                status_response(msg, STATUS_INCORRECT_NUMBER_OF_ARGS);
                continue;
            }
            
            break;  /* Valid POWER_UP received */
        }
    }
    
    /* Build ATR response message */
    msg->buf[0] = TLP_ACK;
    msg->buf[1] = 0;  /* Length high byte */
    msg->buf[2] = (u8)(4 + atr_len + hist_len);  /* Length low byte */
    msg->buf[3] = STATUS_SUCCESS;
    msg->buf[4] = 0x28;  /* ATR indicator */
    msg->buf[5] = 2;     /* Protocol type (T=0) */
    msg->buf[6] = (u8)(atr_len + hist_len);
    
    /* Copy ATR bytes */
    memcpy(&msg->buf[7], atr, atr_len);
    
    /* Copy historical bytes (if any) */
    if (hist_len > 0 && hist != NULL) {
        memcpy(&msg->buf[7 + atr_len], hist, hist_len);
    }
    
    /* Compute and append LRC */
    total_len = 7 + atr_len + hist_len;
    msg->buf[total_len] = tlp_compute_lrc(msg->buf, total_len);
    msg->len = total_len + 1;
    
    /* Send ATR message via TLP (uses socket directly if msg->fd is valid) */
    if (msg->fd >= 0) {
        /* Direct socket mode (JCShell) */
        if (tlp_send_message(msg) != 0) {
            printf("[T=0] ERROR: Failed to send ATR via TLP\n");
            return -1;
        }
    } else {
        /* Transport layer mode (TCP Server) */
        gcos_transport_send_response(msg->buf, msg->len, 0x9000);
    }
    
    /* Update state */
    current_state = T0_STATE_HEADER_RECEIVED;
    msg->ioState = TLP_STATE_OPEN;
    
    printf("[T=0] ATR sent successfully (%u bytes)\n", msg->len);
    
    return 0;
}

/* ============================================================================
 * Command Reception (_t0RcvCommand from cref t0_ll.c)
 * ============================================================================ */

s8 t0_receive_command(TLP_MSG *msg, u8 *command) {
    u16 recv_len;
    
    if (msg == NULL || command == NULL) {
        return -1;
    }
    
    printf("[T=0] Waiting for APDU command...\n");
    
    /* Receive TLP224 message */
    recv_len = gcos_transport_receive_apdu(msg->buf, TLP_BUFFER_SIZE);
    if (recv_len == 0) {
        printf("[T=0] Error: Failed to receive message\n");
        return -1;
    }
    
    msg->len = recv_len;
    
    /* Process based on command type (msg->buf[3]) */
    switch (msg->buf[3] & 0xFF) {
        
    case TLP_POWER_UP:
        /* Handle reset/power-up command */
        printf("[T=0] Received POWER_UP - resetting\n");
        
        /* Reset message state */
        tlp_msg_reset(msg);
        
        /* Send ATR without waiting for POWER_UP (already received) */
        t0_send_atr(msg, default_hist_len, default_atr, default_hist, false);
        
        /* Continue waiting for actual APDU */
        return t0_receive_command(msg, command);
        
    case TLP_POWER_DOWN:
        /* Handle power-down command */
        printf("[T=0] Received POWER_DOWN - closing connection\n");
        
        status_response(msg, STATUS_SUCCESS);
        
        /* Update state to closed */
        msg->ioState = TLP_STATE_CLOSED;
        current_state = T0_STATE_INIT;
        
        return 1;  /* Signal that session should end */
        
    case TLP_ISO_INPUT:
        /* Handle APDU with data (Case 3 or Case 4) */
        printf("[T=0] Received ISO_INPUT (APDU with data)\n");
        
        /* Validate INS byte (cannot be 0x6X or 0x9X - those are SW1 values) */
        if ((msg->buf[TLP_OFFSET_INS] & 0xF0) == 0x60 ||
            (msg->buf[TLP_OFFSET_INS] & 0xF0) == 0x90) {
            printf("[T=0] Error: Invalid INS byte 0x%02X\n", msg->buf[TLP_OFFSET_INS]);
            status_response(msg, STATUS_ISO_CMD_ERROR);
            return t0_receive_command(msg, command);  /* Retry */
        }
        
        /* Validate P3 matches data length */
        if (msg->buf[TLP_OFFSET_P3] != (u8)(msg->len - 10)) {
            printf("[T=0] Error: P3 mismatch - P3=%u, actual=%u\n", 
                   msg->buf[TLP_OFFSET_P3], msg->len - 10);
            status_response(msg, STATUS_ISO_LC_ERROR);
            return t0_receive_command(msg, command);  /* Retry */
        }
        
        /* Copy APDU header (5 bytes) to command buffer */
        memcpy(command, &msg->buf[TLP_OFFSET_CLA], APDU_HEADER_LENGTH);
        
        /* Set I/O flag for data reception */
        if (msg->buf[TLP_OFFSET_P3] > 0) {
            msg->ioFlag = TLP_INPUT;
            msg->ioOffset = TLP_OFFSET_DATA_IN;
        }
        
        break;
        
    case TLP_ISO_OUTPUT:
        /* Handle APDU without data (Case 1 or Case 2) */
        printf("[T=0] Received ISO_OUTPUT (APDU without data)\n");
        
        /* Validate INS byte */
        if ((msg->buf[TLP_OFFSET_INS] & 0xF0) == 0x60 ||
            (msg->buf[TLP_OFFSET_INS] & 0xF0) == 0x90) {
            printf("[T=0] Error: Invalid INS byte 0x%02X\n", msg->buf[TLP_OFFSET_INS]);
            status_response(msg, STATUS_ISO_CMD_ERROR);
            return t0_receive_command(msg, command);  /* Retry */
        }
        
        /* Validate message length (must be exactly 10 bytes for 5-byte APDU + TLP overhead) */
        if (msg->len != 10) {
            printf("[T=0] Error: Invalid message length %u (expected 10)\n", msg->len);
            status_response(msg, STATUS_INCORRECT_NUMBER_OF_ARGS);
            return t0_receive_command(msg, command);  /* Retry */
        }
        
        /* Copy APDU header to command buffer */
        memcpy(command, &msg->buf[TLP_OFFSET_CLA], APDU_HEADER_LENGTH);
        
        /* Set I/O flag for data transmission */
        msg->ioFlag = TLP_OUTPUT;
        msg->ioOffset = TLP_OFFSET_CLA;
        msg->len = TLP_OFFSET_CLA;
        
        break;
        
    default:
        /* Unknown command type */
        printf("[T=0] Error: Unknown command type 0x%02X\n", msg->buf[3]);
        status_response(msg, STATUS_COMMAND_UNKNOWN);
        return t0_receive_command(msg, command);  /* Retry */
    }
    
    /* Update state to active */
    msg->ioState = TLP_STATE_ACTIVE;
    current_state = T0_STATE_HEADER_READ;
    
    printf("[T=0] Command received: CLA=0x%02X INS=0x%02X P1=0x%02X P2=0x%02X P3=0x%02X\n",
           command[0], command[1], command[2], command[3], command[4]);
    
    return 0;
}

/* ============================================================================
 * Data Reception (_t0RcvData from cref t0_ll.c)
 * ============================================================================ */

s16 t0_receive_data(TLP_MSG *msg, u8 *data, u16 offset, u16 length, u8 proc_byte) {
    u8 ack = proc_byte & 0xFF;
    s16 bytes_received;
    
    if (msg == NULL || data == NULL) {
        return -1;
    }
    
    printf("[T=0] Receiving %u bytes of data (proc_byte=0x%02X)\n", length, proc_byte);
    
    /* Handle NULL procedure byte (0x60) - used to extend waiting time */
    if (length == 0 && ack == 0x60) {
        printf("[T=0] NULL procedure byte received - extending wait time\n");
        return 0;
    }
    
    /* Validate I/O direction */
    if (msg->ioFlag != TLP_INPUT) {
        printf("[T=0] Error: t0_receive_data called but not in INPUT mode\n");
        return -1;
    }
    
    /* Get INS and P3 from command header */
    u8 ins = msg->buf[TLP_OFFSET_INS] & 0xFF;
    u8 p3 = msg->buf[TLP_OFFSET_P3] & 0xFF;
    
    /* Validate procedure byte per ISO7816-3 Section 8.2.2.1 */
    if (T0_VALIDATE_PROC_BYTE_RECV(ack, ins, length, p3)) {
        /* Procedure byte is valid - receive data */
        bytes_received = get_n_bytes(msg, data, offset, length);
        return bytes_received;
    } else {
        /* Procedure byte validation failed */
        printf("[T=0] Error: Invalid procedure byte - ACK=0x%02X, INS=0x%02X\n", ack, ins);
        return -2;
    }
}

/* ============================================================================
 * Data Transmission (_t0SndDataProc from cref t0_ll.c)
 * ============================================================================ */

s8 t0_send_data_proc(TLP_MSG *msg, const u8 *data, u16 offset, u16 length, 
                     u8 proc_byte, bool send_proc_byte) {
    u8 ack = proc_byte & 0xFF;
    
    if (msg == NULL || data == NULL) {
        return -1;
    }
    
    printf("[T=0] Sending %u bytes of data (proc_byte=0x%02X, burst=%d)\n", 
           length, proc_byte, !send_proc_byte);
    
    /* Handle NULL procedure byte (0x60) */
    if (send_proc_byte && length == 0 && ack == 0x60) {
        printf("[T=0] NULL procedure byte - extending wait time\n");
        return 0;
    }
    
    /* Validate I/O direction */
    if (msg->ioFlag != TLP_OUTPUT) {
        printf("[T=0] Error: t0_send_data_proc called but not in OUTPUT mode\n");
        return 1;
    }
    
    /* If not validating procedure bytes (burst mode), send data directly */
    if (!send_proc_byte) {
        put_n_bytes(msg, data, offset, length);
        return 0;
    }
    
    /* Get INS and P3 from command header */
    u8 ins = msg->buf[TLP_OFFSET_INS] & 0xFF;
    u8 p3 = msg->buf[TLP_OFFSET_P3] & 0xFF;
    
    /* Handle P3=0 as 256 per ISO7816-3 */
    if (p3 == 0) {
        p3 = 256;
    }
    
    /* Validate procedure byte per ISO7816-3 Section 8.2.2.1 */
    if (T0_VALIDATE_PROC_BYTE_SEND(ack, ins, length, p3)) {
        /* Procedure byte is valid - send data */
        put_n_bytes(msg, data, offset, length);
        return 0;
    } else {
        /* Procedure byte validation failed */
        printf("[T=0] Error: Invalid procedure byte - ACK=0x%02X, INS=0x%02X, LEN=%u, P3=%u\n",
               ack, ins, length, p3);
        return 1;
    }
}

/* ============================================================================
 * Status Word Transmission (_t0SndStatusRcvCommand from cref t0_ll.c)
 * ============================================================================ */

s8 t0_send_status_recv_command(TLP_MSG *msg, u8 *command, u16 sw1sw2) {
    u8 le = 0;
    u16 size;
    s8 result;
    
    if (msg == NULL || command == NULL) {
        return -1;
    }
    
    printf("[T=0] Sending status word: 0x%04X\n", sw1sw2);
    
    /* Build status response message */
    msg->buf[0] = TLP_ACK;
    msg->buf[1] = 0;  /* Length high byte */
    msg->buf[2] = 3;  /* Length low byte (3 bytes: status + SW1 + SW2) */
    size = 3;
    
    /* Determine status code based on I/O mode and SW */
    if (msg->ioFlag == TLP_OUTPUT) {
        /* Case 2 or 2S - we were asked to send data */
        le = (u8)(msg->len - TLP_OFFSET_CLA);
        
        if (le == 0) {
            /* We declined to send data */
            msg->buf[3] = STATUS_INTERRUPTED_EXCHANGE;
        } else {
            /* Set status based on SW */
            if (sw1sw2 == 0x9000) {
                msg->buf[3] = STATUS_SUCCESS;
            } else {
                msg->buf[3] = STATUS_CARD_ERROR;
            }
            
            size += le;
            msg->buf[1] = (u8)(size >> 8);
            msg->buf[2] = (u8)(size & 0xFF);
        }
        
    } else if (msg->ioFlag == TLP_INPUT) {
        /* Case 3 or 3S - we didn't read all sent data */
        if (le > 0) {
            msg->buf[3] = STATUS_INTERRUPTED_EXCHANGE;
        } else if (sw1sw2 == 0x9000) {
            msg->buf[3] = STATUS_SUCCESS;
        } else {
            msg->buf[3] = STATUS_CARD_ERROR;
        }
        
    } else {
        /* Case 1 - no data transfer */
        if (sw1sw2 == 0x9000) {
            msg->buf[3] = STATUS_SUCCESS;
        } else {
            msg->buf[3] = STATUS_CARD_ERROR;
        }
    }
    
    /* Append SW1SW2 */
    msg->buf[4 + le] = (u8)(sw1sw2 >> 8);
    msg->buf[5 + le] = (u8)(sw1sw2 & 0xFF);
    
    /* Compute and append LRC */
    msg->buf[6 + le] = tlp_compute_lrc(msg->buf, 6 + le);
    msg->len = 7 + le;
    
    /* Send status message */
    gcos_transport_send_response(msg->buf, msg->len, 0x9000);
    
    /* Immediately receive next command */
    printf("[T=0] Waiting for next command...\n");
    
    result = t0_receive_command(msg, command);
    
    /* Backup command header for potential 6Cxx handling */
    if (result == 0) {
        t0_backup_cmd_header(command);
    }
    
    return result;
}

/* ============================================================================
 * Status Word Only (_t0SndStatus from cref t0_ll.c)
 * ============================================================================ */

s8 t0_send_status_only(TLP_MSG *msg, u16 sw1sw2) {
    u8 le = 0;
    u16 size;
    
    if (msg == NULL) {
        return -1;
    }
    
    printf("[T=0] Sending status word only: 0x%04X\n", sw1sw2);
    
    /* Build status response message */
    msg->buf[0] = TLP_ACK;
    msg->buf[1] = 0;
    msg->buf[2] = 3;
    
    /* Determine status code */
    if (msg->ioFlag == TLP_OUTPUT) {
        le = (u8)(msg->len - TLP_OFFSET_CLA);
        
        if (le == 0) {
            msg->buf[3] = STATUS_INTERRUPTED_EXCHANGE;
        } else {
            if (sw1sw2 == 0x9000) {
                msg->buf[3] = STATUS_SUCCESS;
            } else {
                msg->buf[3] = STATUS_CARD_ERROR;
            }
            
            size = 3 + le;
            msg->buf[1] = (u8)(size >> 8);
            msg->buf[2] = (u8)(size & 0xFF);
        }
        
    } else if (msg->ioFlag == TLP_INPUT) {
        if (le > 0) {
            msg->buf[3] = STATUS_INTERRUPTED_EXCHANGE;
        } else if (sw1sw2 == 0x9000) {
            msg->buf[3] = STATUS_SUCCESS;
        } else {
            msg->buf[3] = STATUS_CARD_ERROR;
        }
        
    } else {
        if (sw1sw2 == 0x9000) {
            msg->buf[3] = STATUS_SUCCESS;
        } else {
            msg->buf[3] = STATUS_CARD_ERROR;
        }
    }
    
    /* Append SW1SW2 and LRC */
    msg->buf[4 + le] = (u8)(sw1sw2 >> 8);
    msg->buf[5 + le] = (u8)(sw1sw2 & 0xFF);
    msg->buf[6 + le] = tlp_compute_lrc(msg->buf, 6 + le);
    msg->len = 7 + le;
    
    /* Send status message */
    gcos_transport_send_response(msg->buf, msg->len, 0x9000);
    
    return 0;
}

/* ============================================================================
 * Command Header Management
 * ============================================================================ */

void t0_backup_cmd_header(const u8 *command) {
    if (command == NULL) {
        return;
    }
    
    memcpy(backed_up_header, command, APDU_HEADER_LENGTH);
}

void t0_restore_cmd_header(u8 *command) {
    if (command == NULL) {
        return;
    }
    
    memcpy(command, backed_up_header, APDU_HEADER_LENGTH);
}

u8 t0_get_cmd_header_byte(u16 position) {
    if (position >= APDU_HEADER_LENGTH) {
        return 0;
    }
    
    return backed_up_header[position];
}

/* ============================================================================
 * Wait Extension (NULL Procedure Byte)
 * ============================================================================ */

s8 t0_send_wait(void) {
    /* Send NULL procedure byte (0x60) to extend waiting time */
    printf("[T=0] Sending NULL procedure byte (0x60) to extend wait time\n");
    
    /* In a real implementation, this would send 0x60 to the terminal */
    /* For now, we just log it since our transport layer doesn't support this */
    
    return 0;
}
