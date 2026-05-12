#ifndef GCOS_APDU_H
#define GCOS_APDU_H

#include "gcos_vm.h"

/* ============================================================================
 * APDU Protocol Definitions (ISO/IEC 7816-4)
 * ============================================================================ */

/* APDU header length: CLA + INS + P1 + P2 + P3(Lc/Le) */
#define APDU_HEADER_LENGTH      5
#define APDU_HEADER_MIN_LENGTH  4

/* Maximum APDU buffer size */
#define APDU_BUFFER_SIZE        260
#define RESPONSE_BUFFER_SIZE    260

/* Logical channel mask in CLA byte */
#define APDU_CHANNEL_MASK       0x03
#define APDU_CHANNEL_BASIC      0x00

/* ============================================================================
 * Standard APDU Instructions (INS)
 * ============================================================================ */

/* Global Platform Card Management Instructions */
#define INS_LOAD                0xE8    /* Load module (streaming) */
#define INS_INSTALL             0xE6    /* Install application */
#define INS_INSTALL_FOR_LOAD    0xE2    /* Install for load */
#define INS_INSTALL_FOR_INSTALL 0xE2    /* Install for install */
#define INS_DELETE              0xE4    /* Delete application/module */
#define INS_GET_STATUS          0xF2    /* Get status information */

/* Application Selection Instructions */
#define INS_SELECT              0xA4    /* Select application */
#define INS_DESELECT            0xAA    /* Deselect application */

/* Channel Management Instructions */
#define INS_MANAGE_CHANNEL      0x70    /* Manage logical channel */

/* Other Instructions */
#define INS_GET_DATA            0xCA    /* Get data */
#define INS_PUT_DATA            0xDA    /* Put data */
#define INS_GET_RESPONSE        0xC0    /* Get response (for extended APDU) */

/* ============================================================================
 * Status Word (SW1SW2) Definitions
 * ============================================================================ */

/* Success */
#define SW_SUCCESS                      0x9000

/* Warning conditions */
#define SW_NO_FURTHER_INFORMATION       0x6200
#define SW_PART_OF_RETURNED_DATA_MAY_BE_CORRUPTED 0x6281

/* Execution errors */
#define SW_WRONG_LENGTH                 0x6700
#define SW_SECURITY_STATUS_NOT_SATISFIED 0x6982
#define SW_AUTHENTICATION_METHOD_BLOCKED 0x6983
#define SW_REFERENCE_DATA_INVALIDATED   0x6984
#define SW_CONDITIONS_NOT_SATISFIED     0x6985
#define SW_COMMAND_NOT_ALLOWED          0x6986
#define SW_APPLET_SELECT_FAILED         0x6999

/* Wrong parameters */
#define SW_INCORRECT_PARAMETERS         0x6A80
#define SW_FUNCTION_NOT_SUPPORTED       0x6A81
#define SW_APP_NOT_FOUND                0x6A82
#define SW_RECORD_NOT_FOUND             0x6A83
#define SW_NOT_ENOUGH_MEMORY            0x6A84
#define SW_INCORRECT_P1P2               0x6A86
#define SW_INCORRECT_LC_LE              0x6A87

/* Instruction errors */
#define SW_INS_NOT_SUPPORTED            0x6D00
#define SW_CLA_NOT_SUPPORTED            0x6E00

/* Technical problems */
#define SW_NO_PRECISE_DIAGNOSIS         0x6F00

/* ============================================================================
 * APDU Command Structure
 * ============================================================================ */

/**
 * @brief APDU command structure
 * 
 * Represents a complete APDU command as defined in ISO/IEC 7816-4.
 * This structure is used to parse incoming APDU commands and dispatch
 * them to appropriate handlers.
 */
typedef struct {
    u8 cla;                     /**< Class byte - includes logical channel */
    u8 ins;                     /**< Instruction byte - command code */
    u8 p1;                      /**< Parameter 1 */
    u8 p2;                      /**< Parameter 2 */
    u8 lc;                      /**< Length of command data field */
    const u8 *data;             /**< Pointer to command data (if any) */
    u8 le;                      /**< Expected response length */
    u8 has_data;                /**< Flag: 1 if data field present, 0 otherwise */
} GCOSSApdu;

/**
 * @brief APDU response status word structure
 * 
 * Contains the two-byte status word returned after APDU processing.
 */
typedef struct {
    u8 sw1;                     /**< Status word byte 1 */
    u8 sw2;                     /**< Status word byte 2 */
} GCOSSwStatus;

/**
 * @brief APDU handler function signature
 * 
 * All APDU command handlers must conform to this signature.
 * 
 * @param vm          Pointer to GCOS VM instance
 * @param apdu        Pointer to parsed APDU command
 * @param response    Buffer for response data
 * @param resp_len    [in/out] Response length
 * @return            Status word (SW1SW2)
 */
typedef u16 (*ApduHandler)(GCOSVM *vm, const GCOSSApdu *apdu, 
                           u8 *response, u16 *resp_len);

/**
 * @brief APDU command table entry
 * 
 * Maps an INS byte to its handler function.
 */
typedef struct {
    u8 ins;                     /**< Instruction byte */
    ApduHandler handler;        /**< Handler function */
    const char *name;           /**< Command name (for debugging) */
} ApduCommandEntry;

/* ============================================================================
 * Stream Load State Machine States
 * ============================================================================ */

/**
 * @brief Stream load states for LOAD instruction
 * 
 * The LOAD instruction uses a state machine to handle streaming
 * installation of modules across multiple APDU commands.
 */
typedef enum {
    STREAM_LOAD_IDLE = 0,           /**< No load operation in progress */
    STREAM_LOAD_INIT = 1,           /**< Initialization phase */
    STREAM_LOAD_RECEIVING = 2,      /**< Receiving data blocks */
    STREAM_LOAD_VERIFYING = 3,      /**< Verifying integrity */
    STREAM_LOAD_LINKING = 4,        /**< Linking imports/exports */
    STREAM_LOAD_INSTALLING = 5,     /**< Installing application */
    STREAM_LOAD_COMPLETE = 6        /**< Load completed successfully */
} StreamLoadState;

/**
 * @brief Stream load context
 * 
 * Maintains state for ongoing stream load operations.
 * Multiple concurrent loads are supported via context_id.
 */
typedef struct {
    u8 context_id;                  /**< Unique context identifier */
    StreamLoadState state;          /**< Current state */
    u32 total_size;                 /**< Total expected size */
    u32 received_size;              /**< Bytes received so far */
    u8 *buffer;                     /**< Temporary buffer for data */
    u32 buffer_size;                /**< Buffer capacity */
    u32 checksum;                   /**< Running checksum */
    u8 final_block;                 /**< Flag: 1 if this is final block */
} StreamLoadContext;

/* ============================================================================
 * APDU Processing API
 * ============================================================================ */

/**
 * @brief Initialize APDU subsystem
 * 
 * Sets up APDU command table and initializes stream load contexts.
 * 
 * @param vm    Pointer to GCOS VM instance
 * @return      GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_apdu_init(GCOSVM *vm);

/**
 * @brief Process a single APDU command
 * 
 * Main entry point for APDU processing. Parses the APDU, validates it,
 * and dispatches to the appropriate handler.
 * 
 * @param vm              Pointer to GCOS VM instance
 * @param apdu_buffer     Raw APDU bytes from terminal
 * @param apdu_length     Length of APDU buffer
 * @param response_buffer Buffer for response data
 * @param response_length [out] Length of response data
 * @return                Status word (SW1SW2)
 */
u16 gcos_vm_process_apdu(GCOSVM *vm, const u8 *apdu_buffer, u8 apdu_length,
                         u8 *response_buffer, u16 *response_length);

/**
 * @brief Parse raw APDU bytes into GCOSSApdu structure
 * 
 * @param apdu_buffer   Raw APDU bytes
 * @param apdu_length   Length of APDU buffer
 * @param apdu          [out] Parsed APDU structure
 * @return              GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_apdu_parse(const u8 *apdu_buffer, u8 apdu_length, 
                           GCOSSApdu *apdu);

/**
 * @brief Create status word from two bytes
 * 
 * @param sw1   Status word byte 1
 * @param sw2   Status word byte 2
 * @return      Combined SW1SW2 value
 */
static inline u16 gcos_make_sw(u8 sw1, u8 sw2) {
    return ((u16)sw1 << 8) | (u16)sw2;
}

/**
 * @brief Find APDU handler for given INS byte
 * 
 * @param vm    Pointer to GCOS VM instance
 * @param ins   Instruction byte
 * @return      Handler function pointer, or NULL if not found
 */
ApduHandler gcos_apdu_find_handler(GCOSVM *vm, u8 ins);

/* ============================================================================
 * Stream Load API
 * ============================================================================ */

/**
 * @brief Initialize stream load context
 * 
 * Allocates and initializes a new stream load context for LOAD instruction.
 * 
 * @param vm          Pointer to GCOS VM instance
 * @param total_size  Total expected size of module
 * @param context_id  [out] Assigned context ID
 * @return            GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_stream_load_init(GCOSVM *vm, u32 total_size, u8 *context_id);

/**
 * @brief Receive data block in stream load
 * 
 * Receives a block of module data as part of streaming LOAD operation.
 * 
 * @param vm          Pointer to GCOS VM instance
 * @param context_id  Context ID from initialization
 * @param data        Data block bytes
 * @param data_len    Length of data block
 * @param is_final    Flag: 1 if this is the final block
 * @return            GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_stream_load_receive(GCOSVM *vm, u8 context_id, 
                                    const u8 *data, u32 data_len, u8 is_final);

/**
 * @brief Complete stream load operation
 * 
 * Finalizes the stream load, performs verification, linking, and installation.
 * 
 * @param vm          Pointer to GCOS VM instance
 * @param context_id  Context ID from initialization
 * @return            GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_stream_load_complete(GCOSVM *vm, u8 context_id);

/**
 * @brief Abort stream load operation
 * 
 * Cancels an ongoing stream load and releases resources.
 * 
 * @param vm          Pointer to GCOS VM instance
 * @param context_id  Context ID to abort
 * @return            GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult gcos_stream_load_abort(GCOSVM *vm, u8 context_id);

#endif /* GCOS_APDU_H */
