/**
 * @file gcos_tlp.h
 * @brief TLP (Transport Layer Protocol) message definitions
 * 
 * Defines the TLP message structure and constants for APDU transport.
 * Based on cref's tlp.h, translated to English and adapted for GCOS VM.
 * 
 * TLP224 is a simple protocol that wraps APDU commands in a framed message
 * with error checking (LRC). It supports:
 * - Power up/down commands
 * - ISO input/output (APDU commands)
 * - Status responses
 */

#ifndef GCOS_TLP_H
#define GCOS_TLP_H

#include "gcos_vm.h"

/* ============================================================================
 * TLP Protocol Constants
 * ============================================================================ */

/* TLP Control Characters */
#define TLP_EOT                 0x03    /**< End of Transmission */
#define TLP_ACK                 0x60    /**< Acknowledge */
#define TLP_NACK                0xE0    /**< Not Acknowledge */

/* TLP Command Types */
#define TLP_POWER_DOWN          0x4D    /**< Power down command */
#define TLP_POWER_UP            0x6E    /**< Power up command */
#define TLP_ISO_INPUT           0xDA    /**< ISO input (APDU with data) */
#define TLP_ISO_OUTPUT          0xDB    /**< ISO output (APDU without data) */

/* TLP Message Offsets (within TLP payload) */
#define TLP_OFFSET_CLA          4       /**< CLA byte offset */
#define TLP_OFFSET_INS          5       /**< INS byte offset */
#define TLP_OFFSET_P1           6       /**< P1 byte offset */
#define TLP_OFFSET_P2           7       /**< P2 byte offset */
#define TLP_OFFSET_P3           8       /**< P3 byte offset */
#define TLP_OFFSET_DATA_IN      9       /**< Data field start offset */

/* TLP I/O Flags */
#define TLP_UNKNOWN             (-1)    /**< Unknown I/O direction */
#define TLP_INPUT               0       /**< Input direction (card receives data) */
#define TLP_OUTPUT              1       /**< Output direction (card sends data) */

/* TLP I/O States */
#define TLP_STATE_CLOSED        0       /**< Connection closed */
#define TLP_STATE_OPEN          1       /**< Connection open (after ATR) */
#define TLP_STATE_ACTIVE        2       /**< Active communication */

/* TLP Status Codes */
#define STATUS_SUCCESS                      0x00
#define STATUS_INCORRECT_NUMBER_OF_ARGS     0x03
#define STATUS_COMMAND_UNKNOWN              0x04
#define STATUS_PROTOCOL_ERROR               0x09
#define STATUS_ISO_CMD_ERROR                0x11
#define STATUS_MESSAGE_TOO_LONG             0x12
#define STATUS_CARD_TURNED_OFF              0x15
#define STATUS_UNKNOWN_PROTOCOL             0x17
#define STATUS_ISO_LC_ERROR                 0x1A
#define STATUS_CARD_PROTOCOL_ERROR          0xA1
#define STATUS_CARD_MALFUNCTION             0xA2
#define STATUS_CARD_ABORTED_CHAINING        0xA4
#define STATUS_READER_ABORTED_CHAINING      0xA5
#define STATUS_INVALID_PROCEDURE_BYTE       0xE4
#define STATUS_INTERRUPTED_EXCHANGE         0xE5
#define STATUS_CARD_ERROR                   0xE7
#define STATUS_CARD_REMOVED                 0xF7
#define STATUS_CARD_MISSING                 0xFB

/* TLP Message Size Limits */
#define TLP_MAX_LINE            0x800D  /**< Maximum line size (from cref) */
#define TLP_MAX_MESSAGE_LEN     258     /**< Maximum message length */
#define TLP_BUFFER_SIZE         260     /**< TLP buffer size (aligned with APDU) */

/* ATR Configuration */
#define TLP_ATR_LEN             6       /**< ATR length */

/* ============================================================================
 * TLP Message Structure
 * ============================================================================ */

/**
 * @brief TLP message structure
 * 
 * Represents a complete TLP224 message including header, payload, and LRC.
 * The message format is:
 *   [ACK/NACK][Length High][Length Low][Command/Status][Payload...][LRC]
 */
typedef struct {
    s8 ioFlag;                  /**< I/O flag: TLP_INPUT or TLP_OUTPUT */
    u16 len;                    /**< Total message length */
    s16 fd;                     /**< File descriptor (socket handle) */
    s8 ioState;                 /**< I/O state: CLOSED, OPEN, or ACTIVE */
    u16 ioOffset;               /**< Current I/O offset within message */
    u8 buf[TLP_BUFFER_SIZE];    /**< Message buffer */
} TLP_MSG;

/* ============================================================================
 * Global TLP Message Instance
 * ============================================================================ */

extern TLP_MSG g_tlp_msg;       /**< Global TLP message instance */

/* ============================================================================
 * TLP Utility Functions
 * ============================================================================ */

/**
 * @brief Initialize TLP message structure
 * 
 * @param msg  Pointer to TLP message to initialize
 */
void tlp_msg_init(TLP_MSG *msg);

/**
 * @brief Reset TLP message to initial state
 * 
 * @param msg  Pointer to TLP message to reset
 */
void tlp_msg_reset(TLP_MSG *msg);

/**
 * @brief Compute LRC (Longitudinal Redundancy Check) for TLP message
 * 
 * LRC is computed by XORing all bytes from index 0 to length-1.
 * 
 * @param buf     Buffer containing message data
 * @param length  Number of bytes to include in LRC calculation
 * @return        Computed LRC value
 */
u8 tlp_compute_lrc(const u8 *buf, u16 length);

/**
 * @brief Validate TLP message LRC
 * 
 * @param msg  Pointer to TLP message to validate
 * @return     true if LRC is valid, false otherwise
 */
bool tlp_validate_lrc(const TLP_MSG *msg);

/**
 * @brief Send a TLP224 encoded message
 * 
 * Encodes binary message as ASCII hex and sends it with EOT terminator.
 * Matches cref's sendTLP224Message() function.
 * 
 * Message format on wire:
 *   [Hex(ACK)][Hex(LenHi)][Hex(LenLo)][Hex(Cmd)]...[Hex(LRC)][EOT]
 * 
 * @param msg   TLP message to send
 * @return      0 on success, -1 on error
 */
s8 tlp_send_message(TLP_MSG *msg);

/**
 * @brief Receive a TLP224 encoded message
 * 
 * Receives ASCII hex characters until EOT, decodes to binary,
 * validates LRC and message length.
 * Matches cref's receiveTLP224Message() function.
 * 
 * Retry logic:
 * - Up to 5 retries on transmission errors
 * - Sends NACK on LRC failure
 * - Sends protocol error status on invalid format
 * 
 * @param msg   TLP message structure to fill
 * @return      Number of bytes received, or -1 on error
 */
s16 tlp_receive_message(TLP_MSG *msg);

#endif /* GCOS_TLP_H */
