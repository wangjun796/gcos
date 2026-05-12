/**
 * @file gcos_t0_protocol.h
 * @brief ISO7816-3 T=0 Protocol implementation
 * 
 * Implements the complete T=0 byte-level protocol for smart card communication.
 * Based on cref's t0.c, t0.h, t0_ll.c and t0_ll.h, translated to English and 
 * adapted for GCOS VM.
 * 
 * Key features:
 * - Complete T=0 state machine (INIT → HEADER_RECEIVED → HEADER_READ → INCOMING/OUTGOING)
 * - Procedure byte handling per ISO7816-3 specification
 * - APDU header and data transfer
 * - Status word (SW1SW2) transmission with immediate next command reception
 * - Support for Case 1, 2, 3, and 4 APDUs
 */

#ifndef GCOS_T0_PROTOCOL_H
#define GCOS_T0_PROTOCOL_H

#include "gcos_vm.h"
#include "gcos_tlp.h"

/* ============================================================================
 * T=0 Protocol States
 * ============================================================================ */

/**
 * @brief T=0 I/O line states
 * 
 * Represents the state of the communication channel during T=0 protocol.
 * Modeled after cref's t0.c state machine.
 */
typedef enum {
    T0_STATE_INIT = 0,              /**< No header received yet */
    T0_STATE_HEADER_RECEIVED = 1,   /**< Header received but not passed to upper layer */
    T0_STATE_HEADER_READ = 2,       /**< Header received and passed to application */
    T0_STATE_INCOMING = 3,          /**< Application called receive_data (Case 3/4) */
    T0_STATE_OUTGOING = 4,          /**< Application called send_data (Case 2/4) */
    T0_STATE_OUTGOING_BURST = 5     /**< Burst mode sending in progress */
} T0State;

/* ============================================================================
 * APDU Header Constants
 * ============================================================================ */

#define APDU_HEADER_LENGTH      5       /**< APDU header length (CLA INS P1 P2 P3) */
#define APDU_CLA_OFFSET         0       /**< CLA byte offset in header */
#define APDU_INS_OFFSET         1       /**< INS byte offset in header */
#define APDU_P1_OFFSET          2       /**< P1 byte offset in header */
#define APDU_P2_OFFSET          3       /**< P2 byte offset in header */
#define APDU_P3_OFFSET          4       /**< P3 byte offset in header */

/* ============================================================================
 * Procedure Byte Validation Macros
 * ============================================================================ */

/**
 * @brief Validate procedure byte for data reception
 * 
 * Per ISO7816-3 Section 8.2.2.1:
 * - ACK ^ INS == 0x00: Transfer all remaining bytes (VPP idle)
 * - ACK ^ INS == 0x01: Transfer all remaining bytes (VPP active)
 * - ACK ^ INS == 0xFE: Transfer next byte only (VPP active)
 * - ACK ^ INS == 0xFF: Transfer next byte only (VPP idle)
 */
#define T0_VALIDATE_PROC_BYTE_RECV(ack, ins, length, p3) \
    ((((ack ^ ins) & 0xFF) == 0x00 && (length) <= (p3)) || \
     (((ack ^ ins) & 0xFF) == 0x01 && (length) <= (p3)) || \
     (((ack ^ ins) & 0xFF) == 0xFE && (length) == 1) || \
     (((ack ^ ins) & 0xFF) == 0xFF && (length) == 1))

/**
 * @brief Validate procedure byte for data transmission
 * 
 * Similar to reception validation but accounts for burst mode.
 */
#define T0_VALIDATE_PROC_BYTE_SEND(ack, ins, length, p3) \
    ((((ack & 0xFE) == ((ins) & 0xFE)) && (length) <= (p3)) || \
     (((ack ^ ins) & 0xFF) == 0x01 && (length) <= (p3)) || \
     (((ack ^ ins) & 0xFF) == 0xFE && (length) == 1) || \
     (((ack ^ ins) & 0xFF) == 0xFF && (length) == 1))

/* ============================================================================
 * T=0 Protocol API
 * ============================================================================ */

/**
 * @brief Initialize T=0 protocol layer
 * 
 * Initializes the global TLP message and sets initial state.
 * Must be called before any T=0 operations.
 * 
 * @param msg  Pointer to TLP message structure
 */
void t0_protocol_init(TLP_MSG *msg);

/**
 * @brief Send Answer To Reset (ATR)
 * 
 * Sends the ATR to the terminal after power-up or reset.
 * This must be called within 40000 clock cycles after reset per ISO7816-3.
 * 
 * @param msg        Pointer to TLP message structure
 * @param hist_len   Length of historical bytes
 * @param atr        ATR bytes array
 * @param hist       Historical bytes array
 * @param need_recv  true if waiting for POWER_UP command, false otherwise
 * @return           0 on success, -1 on error
 */
s8 t0_send_atr(TLP_MSG *msg, u8 hist_len, const u8 *atr, const u8 *hist, bool need_recv);

/**
 * @brief Receive APDU command header
 * 
 * Receives a 5-byte APDU header (CLA INS P1 P2 P3) from the terminal.
 * Handles POWER_UP, POWER_DOWN, ISO_INPUT, and ISO_OUTPUT commands.
 * 
 * State transition: INIT/ACTIVE → HEADER_READ
 * 
 * @param msg      Pointer to TLP message structure
 * @param command  Buffer to store 5-byte APDU header
 * @return         0 on success, -1 on error, 1 if POWER_DOWN received
 */
s8 t0_receive_command(TLP_MSG *msg, u8 *command);

/**
 * @brief Receive command data from terminal
 * 
 * Receives data bytes for Case 3 or Case 4 APDUs.
 * Validates procedure bytes per ISO7816-3 specification.
 * 
 * State requirement: Must be in INCOMING state
 * 
 * @param msg       Pointer to TLP message structure
 * @param data      Buffer to store received data
 * @param offset    Offset in buffer where data should be stored
 * @param length    Number of bytes to receive
 * @param proc_byte Procedure byte from INS field
 * @return          Number of bytes received, -1 on protocol error, -2 on procedure byte error
 */
s16 t0_receive_data(TLP_MSG *msg, u8 *data, u16 offset, u16 length, u8 proc_byte);

/**
 * @brief Send response data to terminal
 * 
 * Sends data bytes for Case 2 or Case 4 APDUs.
 * Supports burst mode (all bytes at once) or byte-by-byte with procedure bytes.
 * 
 * State requirement: Must be in OUTGOING state
 * 
 * @param msg            Pointer to TLP message structure
 * @param data           Buffer containing data to send
 * @param offset         Offset in data buffer
 * @param length         Number of bytes to send
 * @param proc_byte      Procedure byte (INS or ~INS depending on mode)
 * @param send_proc_byte true to validate procedure bytes, false for burst mode
 * @return               0 on success, 1 on protocol error
 */
s8 t0_send_data_proc(TLP_MSG *msg, const u8 *data, u16 offset, u16 length, 
                     u8 proc_byte, bool send_proc_byte);

/**
 * @brief Send status word and receive next command
 * 
 * Sends SW1SW2 status bytes and immediately receives the next APDU header.
 * This is the standard end-of-command sequence in T=0 protocol.
 * 
 * State transition: ACTIVE → HEADER_READ (for next command)
 * 
 * @param msg      Pointer to TLP message structure
 * @param command  Buffer to store next APDU header
 * @param sw1sw2   Status word (SW1SW2)
 * @return         0 on success, -1 on error, 1 if POWER_DOWN received
 */
s8 t0_send_status_recv_command(TLP_MSG *msg, u8 *command, u16 sw1sw2);

/**
 * @brief Send status word only (without receiving next command)
 * 
 * Sends SW1SW2 status bytes without immediately receiving next command.
 * Used for special cases like chaining or extended APDUs.
 * 
 * @param msg      Pointer to TLP message structure
 * @param sw1sw2   Status word (SW1SW2)
 * @return         0 on success, -1 on error
 */
s8 t0_send_status_only(TLP_MSG *msg, u16 sw1sw2);

/**
 * @brief Backup current APDU command header
 * 
 * Saves the current command header for later restoration.
 * Used for 6Cxx status handling and GET RESPONSE sequences.
 * 
 * @param command  5-byte APDU header to backup
 */
void t0_backup_cmd_header(const u8 *command);

/**
 * @brief Restore backed up APDU command header
 * 
 * Restores a previously saved command header.
 * 
 * @param command  Buffer to restore header into
 */
void t0_restore_cmd_header(u8 *command);

/**
 * @brief Get command header byte by position
 * 
 * @param position  Position in command header (0-4)
 * @return          Command header byte at specified position
 */
u8 t0_get_cmd_header_byte(u16 position);

/**
 * @brief Send NULL procedure byte (0x60) to extend waiting time
 * 
 * Per ISO7816-3 Section 8.2.2.2, sends a NULL byte to reset the work
 * waiting time when processing takes longer than expected.
 * 
 * @return  0 on success
 */
s8 t0_send_wait(void);

#endif /* GCOS_T0_PROTOCOL_H */
