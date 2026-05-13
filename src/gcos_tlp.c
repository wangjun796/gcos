/**
 * @file gcos_tlp.c
 * @brief TLP224 (Transport Layer Protocol) message encoding/decoding
 * 
 * Implements TLP224 message framing with ASCII hex encoding.
 * Based on cref's jcshell.c sendTLP224Message() and receiveTLP224Message().
 * 
 * TLP224 Protocol:
 * - Each binary byte is encoded as 2 ASCII hex characters
 * - Messages are terminated with EOT (0x03)
 * - Last byte before EOT is LRC (XOR of all preceding bytes)
 * - Message format: [ACK/NACK][LenHi][LenLo][CmdType][Payload...][LRC][EOT]
 */

#include "gcos_tlp.h"
#include "gcos_transport.h"  /* For low-level byte I/O */
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global TLP Message Instance
 * ============================================================================ */

TLP_MSG g_tlp_msg;

/* ============================================================================
 * TLP Message Management
 * ============================================================================ */

void tlp_msg_init(TLP_MSG *msg) {
    if (msg == NULL) {
        return;
    }
    
    msg->ioFlag = TLP_INPUT;
    msg->len = 0;
    msg->fd = -1;
    msg->ioState = TLP_STATE_CLOSED;
    msg->ioOffset = 0;
    memset(msg->buf, 0, TLP_BUFFER_SIZE);
}

void tlp_msg_reset(TLP_MSG *msg) {
    if (msg == NULL) {
        return;
    }
    
    msg->ioFlag = TLP_INPUT;
    msg->len = 0;
    msg->ioState = TLP_STATE_OPEN;
    msg->ioOffset = 0;
    memset(msg->buf, 0, TLP_BUFFER_SIZE);
}

/* ============================================================================
 * LRC (Longitudinal Redundancy Check) Functions
 * ============================================================================ */

u8 tlp_compute_lrc(const u8 *buf, u16 length) {
    u8 lrc = 0;
    u16 i;
    
    if (buf == NULL || length == 0) {
        return 0;
    }
    
    /* XOR all bytes to compute LRC */
    for (i = 0; i < length; i++) {
        lrc ^= buf[i];
    }
    
    return lrc;
}

bool tlp_validate_lrc(const TLP_MSG *msg) {
    u8 computed_lrc;
    u8 received_lrc;
    
    if (msg == NULL || msg->len < 2) {
        return false;
    }
    
    /* LRC is the last byte of the message (before EOT in transmission) */
    received_lrc = msg->buf[msg->len - 1];
    
    /* Compute LRC over all bytes except the LRC itself */
    computed_lrc = tlp_compute_lrc(msg->buf, msg->len - 1);
    
    return (computed_lrc == received_lrc);
}

/* ============================================================================
 * TLP224 ASCII Hex Encoding/Decoding (from cref jcshell.c)
 * ============================================================================ */

/**
 * @brief Convert a single byte to two ASCII hex characters
 * 
 * Example: 0xAB -> 'A' (0x41), 'B' (0x42)
 * 
 * @param byte     Input byte
 * @param hi_nibble Output high nibble ASCII character
 * @param lo_nibble Output low nibble ASCII character
 */
static void byte_to_ascii_hex(u8 byte, char *hi_nibble, char *lo_nibble) {
    u8 hi = (byte >> 4) & 0x0F;
    u8 lo = byte & 0x0F;
    
    /* Convert high nibble to ASCII */
    if (hi < 10) {
        *hi_nibble = (char)(hi + 0x30);  /* '0' - '9' */
    } else {
        *hi_nibble = (char)(hi + 0x37);  /* 'A' - 'F' */
    }
    
    /* Convert low nibble to ASCII */
    if (lo < 10) {
        *lo_nibble = (char)(lo + 0x30);  /* '0' - '9' */
    } else {
        *lo_nibble = (char)(lo + 0x37);  /* 'A' - 'F' */
    }
}

/**
 * @brief Convert two ASCII hex characters back to a byte
 * 
 * Example: 'A' (0x41), 'B' (0x42) -> 0xAB
 * 
 * @param hi_nibble High nibble ASCII character
 * @param lo_nibble Low nibble ASCII character
 * @return          Decoded byte, or -1 on error
 */
static int ascii_hex_to_byte(char hi_nibble, char lo_nibble) {
    int hi = (int)(unsigned char)hi_nibble;
    int lo = (int)(unsigned char)lo_nibble;
    
    /* Convert high nibble from ASCII */
    hi -= 0x30;
    if (hi > 9) {
        hi -= 7;  /* Convert 'A'-'F' to 10-15 */
    }
    if (hi < 0 || hi > 0xF) {
        return -1;  /* Invalid character */
    }
    
    /* Convert low nibble from ASCII */
    lo -= 0x30;
    if (lo > 9) {
        lo -= 7;  /* Convert 'A'-'F' to 10-15 */
    }
    if (lo < 0 || lo > 0xF) {
        return -1;  /* Invalid character */
    }
    
    return (hi << 4) | lo;
}

/**
 * @brief Send a TLP224 encoded message via transport layer
 * 
 * Encodes binary message as ASCII hex and sends it with EOT terminator.
 * This matches cref's sendTLP224Message() function.
 * 
 * Message format on wire:
 *   [Hex(ACK)][Hex(LenHi)][Hex(LenLo)][Hex(Cmd)]...[Hex(LRC)][EOT]
 * 
 * @param msg   TLP message to send
 * @return      0 on success, -1 on error
 */
s8 tlp_send_message(TLP_MSG *msg) {
    char hi, lo;
    u16 i;
    
    if (msg == NULL || msg->len == 0) {
        return -1;
    }
    
    printf("[TLP] Sending %u bytes (encoded as %u ASCII chars + EOT)\n", 
           msg->len, msg->len * 2 + 1);
    
    /* Check if using direct socket mode (JCShell) or transport layer mode */
    if (msg->fd >= 0) {
        /* Direct socket mode - use send()/write() directly */
        for (i = 0; i < msg->len; i++) {
            byte_to_ascii_hex(msg->buf[i], &hi, &lo);
            
            /* Send high nibble */
#ifdef GCOS_PLATFORM_WIN32
            if (send(msg->fd, &hi, 1, 0) != 1) {
#else
            if (write(msg->fd, &hi, 1) != 1) {
#endif
                printf("[TLP] ERROR: Failed to send high nibble at byte %u\n", i);
                return -1;
            }
            
            /* Send low nibble */
#ifdef GCOS_PLATFORM_WIN32
            if (send(msg->fd, &lo, 1, 0) != 1) {
#else
            if (write(msg->fd, &lo, 1) != 1) {
#endif
                printf("[TLP] ERROR: Failed to send low nibble at byte %u\n", i);
                return -1;
            }
        }
        
        /* Send EOT terminator */
        char eot = TLP_EOT;
#ifdef GCOS_PLATFORM_WIN32
        if (send(msg->fd, &eot, 1, 0) != 1) {
#else
        if (write(msg->fd, &eot, 1) != 1) {
#endif
            printf("[TLP] ERROR: Failed to send EOT\n");
            return -1;
        }
    } else {
        /* Transport layer mode - use gcos_transport_send_byte() */
        /* Encode each byte as 2 ASCII hex characters */
        for (i = 0; i < msg->len; i++) {
            byte_to_ascii_hex(msg->buf[i], &hi, &lo);
            
            /* Send high nibble */
            if (gcos_transport_send_byte((u8)hi) != 0) {
                printf("[TLP] ERROR: Failed to send high nibble at byte %u\n", i);
                return -1;
            }
            
            /* Send low nibble */
            if (gcos_transport_send_byte((u8)lo) != 0) {
                printf("[TLP] ERROR: Failed to send low nibble at byte %u\n", i);
                return -1;
            }
        }
        
        /* Send EOT terminator */
        if (gcos_transport_send_byte(TLP_EOT) != 0) {
            printf("[TLP] ERROR: Failed to send EOT\n");
            return -1;
        }
    }
    
    printf("[TLP] Message sent successfully\n");
    return 0;
}

/**
 * @brief Receive a TLP224 encoded message from transport layer
 * 
 * Receives ASCII hex characters until EOT, decodes to binary,
 * validates LRC and message length.
 * This matches cref's receiveTLP224Message() function.
 * 
 * Retry logic:
 * - Up to 5 retries on transmission errors
 * - Sends NACK on LRC failure
 * - Sends protocol error status on invalid format
 * 
 * @param msg   TLP message structure to fill
 * @return      Number of bytes received, or -1 on error
 */
s16 tlp_receive_message(TLP_MSG *msg) {
    int tries = 0;
    int got_len = 0;
    u16 expected_len;
    
    if (msg == NULL) {
        return -1;
    }
    
#ifdef TLP_DEBUG
    printf("[TLP DEBUG] ========================================\n");
    printf("[TLP DEBUG] Starting message reception...\n");
    printf("[TLP DEBUG] ========================================\n");
#endif
    
    printf("[TLP] Waiting for TLP224 message...\n");
    
    while (1) {
        int message_too_long = 0;
        int xmit_error = 0;
        int got = 0;
        u8 byte;
        
        /* Only retry link level errors 5 times before giving up */
        if (tries++ > 5) {
            printf("[TLP] ERROR: Max retries exceeded (5)\n");
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Giving up after 5 retries\n");
#endif
            return -1;
        }
        
        printf("[TLP] Receiving attempt #%d...\n", tries);
#ifdef TLP_DEBUG
        printf("[TLP DEBUG] Starting to read ASCII hex pairs...\n");
#endif
        
        /* Loop reading ASCII hex pairs until EOT is received */
        while (1) {
            u8 hi_nibble, lo_nibble;
            int decoded_byte;
            
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Reading high nibble (byte %d)...\n", got);
#endif
            
            /* Read high nibble ASCII character */
            if (gcos_transport_receive_byte(&hi_nibble) != 0) {
                printf("[TLP] ERROR: Failed to read high nibble\n");
#ifdef TLP_DEBUG
                printf("[TLP DEBUG] Transport receive failed for high nibble\n");
#endif
                return -1;
            }
            
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Received high nibble: 0x%02X ('%c')\n", hi_nibble, 
                   (hi_nibble >= 32 && hi_nibble < 127) ? hi_nibble : '.');
#endif
            
            /* Check for EOT */
            if (hi_nibble == TLP_EOT) {
#ifdef TLP_DEBUG
                printf("[TLP DEBUG] Received EOT after %d bytes\n", got);
#endif
                break;  /* End of message */
            }
            
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Reading low nibble...\n");
#endif
            
            /* Read low nibble ASCII character */
            if (gcos_transport_receive_byte(&lo_nibble) != 0) {
                printf("[TLP] ERROR: Failed to read low nibble\n");
#ifdef TLP_DEBUG
                printf("[TLP DEBUG] Transport receive failed for low nibble\n");
#endif
                return -1;
            }
            
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Received low nibble: 0x%02X ('%c')\n", lo_nibble,
                   (lo_nibble >= 32 && lo_nibble < 127) ? lo_nibble : '.');
#endif
            
            /* Check for EOT (should not appear in middle of byte) */
            if (lo_nibble == TLP_EOT) {
                xmit_error = 1;
                printf("[TLP] ERROR: Unexpected EOT in middle of byte\n");
                break;
            }
            
            /* Decode ASCII hex pair to byte */
            decoded_byte = ascii_hex_to_byte((char)hi_nibble, (char)lo_nibble);
            if (decoded_byte < 0) {
                xmit_error = 1;
#ifdef TLP_DEBUG
                printf("[TLP DEBUG] Failed to decode hex pair: 0x%02X 0x%02X\n", hi_nibble, lo_nibble);
#endif
                printf("[TLP] ERROR: Invalid hex character: 0x%02X 0x%02X\n", 
                       hi_nibble, lo_nibble);
                continue;
            }
            
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Decoded byte %d: 0x%02X\n", got, decoded_byte);
#endif
            
            if (message_too_long || xmit_error) {
#ifdef TLP_DEBUG
                printf("[TLP DEBUG] Skipping storage (error=%d, too_long=%d)\n", 
                       xmit_error, message_too_long);
#endif
                continue;  /* Skip storing data if error detected */
            }
            
            /* Store decoded byte */
            if (got >= TLP_BUFFER_SIZE) {
                message_too_long = 1;
#ifdef TLP_DEBUG
                printf("[TLP DEBUG] Buffer full at %d bytes (max=%d)\n", got, TLP_BUFFER_SIZE);
#endif
                printf("[TLP] WARNING: Message too long (%d bytes)\n", got);
                continue;
            }
            
            msg->buf[got++] = (u8)decoded_byte;
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Stored byte %d: 0x%02X (total=%d)\n", got-1, decoded_byte, got);
#endif
        }
        
        /* Handle transmission errors */
        if (xmit_error) {
            printf("[TLP] Transmission error, sending NACK...\n");
            
            /* Send NACK response */
            msg->buf[0] = TLP_NACK;
            msg->buf[1] = 0;
            msg->buf[2] = 0;
            msg->buf[3] = tlp_compute_lrc(msg->buf, 3);
            msg->len = 4;
            
            if (tlp_send_message(msg) != 0) {
                return -1;
            }
            continue;  /* Retry */
        }
        
        /* Handle message too long */
        if (message_too_long) {
            printf("[TLP] Message too long, sending error status...\n");
            
            /* Send STATUS_MESSAGE_TOO_LONG */
            msg->buf[0] = TLP_ACK;
            msg->buf[1] = 0;
            msg->buf[2] = 1;
            msg->buf[3] = STATUS_MESSAGE_TOO_LONG;
            msg->buf[4] = tlp_compute_lrc(msg->buf, 4);
            msg->len = 5;
            
            if (tlp_send_message(msg) != 0) {
                return -1;
            }
            continue;  /* Retry */
        }
        
        got_len = got;
        
#ifdef TLP_DEBUG
        printf("[TLP DEBUG] ========================================\n");
        printf("[TLP DEBUG] Received %d bytes total\n", got_len);
        printf("[TLP DEBUG] Raw received data:\n  ");
        for (int i = 0; i < got_len && i < 32; i++) {
            printf("%02X ", msg->buf[i]);
            if ((i + 1) % 16 == 0) printf("\n  ");
        }
        if (got_len > 32) printf("... (%d more bytes)", got_len - 32);
        printf("\n");
        printf("[TLP DEBUG] ========================================\n");
#endif
        
        /* Validate LRC */
        if (got_len < 1) {
            printf("[TLP] ERROR: Message too short for LRC validation\n");
#ifdef TLP_DEBUG
            printf("[TLP DEBUG] Message length %d is too short\n", got_len);
#endif
            continue;
        }
        
        u8 received_lrc = msg->buf[got_len - 1];
        u8 computed_lrc = tlp_compute_lrc(msg->buf, got_len - 1);
        
#ifdef TLP_DEBUG
        printf("[TLP DEBUG] LRC validation:\n");
        printf("[TLP DEBUG]   Received LRC:  0x%02X\n", received_lrc);
        printf("[TLP DEBUG]   Computed LRC:  0x%02X\n", computed_lrc);
        printf("[TLP DEBUG]   Match: %s\n", (received_lrc == computed_lrc) ? "YES" : "NO");
#endif
        
        if (received_lrc != computed_lrc) {
            printf("[TLP] ERROR: LRC mismatch (received=0x%02X, computed=0x%02X)\n",
                   received_lrc, computed_lrc);
            
            /* Send NACK */
            msg->buf[0] = TLP_NACK;
            msg->buf[1] = 0;
            msg->buf[2] = 0;
            msg->buf[3] = tlp_compute_lrc(msg->buf, 3);
            msg->len = 4;
            
            if (tlp_send_message(msg) != 0) {
                return -1;
            }
            continue;  /* Retry */
        }
        
        /* Validate message length field */
        expected_len = ((u16)msg->buf[1] << 8) | msg->buf[2];
        
#ifdef TLP_DEBUG
        printf("[TLP DEBUG] Length validation:\n");
        printf("[TLP DEBUG]   Header length: %u (0x%02X 0x%02X)\n", 
               expected_len, msg->buf[1], msg->buf[2]);
        printf("[TLP DEBUG]   Actual length: %u (received - 4 header bytes)\n", 
               got_len - 4);
        printf("[TLP DEBUG]   Match: %s\n", (expected_len == (u16)(got_len - 4)) ? "YES" : "NO");
#endif
        if (expected_len != (u16)(got_len - 4)) {
            printf("[TLP] ERROR: Length mismatch (header=%u, actual=%u)\n",
                   expected_len, got_len - 4);
            
            /* Send NACK */
            msg->buf[0] = TLP_NACK;
            msg->buf[1] = 0;
            msg->buf[2] = 0;
            msg->buf[3] = tlp_compute_lrc(msg->buf, 3);
            msg->len = 4;
            
            if (tlp_send_message(msg) != 0) {
                return -1;
            }
            continue;  /* Retry */
        }
        
        /* Validate first byte is ACK or NACK */
        if (msg->buf[0] != TLP_ACK && msg->buf[0] != TLP_NACK) {
            printf("[TLP] ERROR: Invalid message type (0x%02X), expected ACK/NACK\n",
                   msg->buf[0]);
            
            /* Send protocol error status */
            msg->buf[0] = TLP_ACK;
            msg->buf[1] = 0;
            msg->buf[2] = 1;
            msg->buf[3] = STATUS_PROTOCOL_ERROR;
            msg->buf[4] = tlp_compute_lrc(msg->buf, 4);
            msg->len = 5;
            
            if (tlp_send_message(msg) != 0) {
                return -1;
            }
            continue;  /* Retry */
        }
        
        /* All validations passed */
        msg->len = (u16)got_len;
        printf("[TLP] Message received successfully (%u bytes)\n", msg->len);
        
        break;  /* Success */
    }
    
    return (s16)got_len;
}
