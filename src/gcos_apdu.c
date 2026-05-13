/**
 * @file gcos_apdu.c
 * @brief APDU command processing implementation
 * 
 * Implements the APDU protocol layer for GCOS VM, providing:
 * - APDU parsing and validation
 * - Command dispatch table
 * - Main processing loop
 * - Status word generation
 * 
 * This module is modeled after JavaCard/cref's T=0 protocol implementation
 * but adapted for COS3 specification requirements.
 */

#include "gcos_apdu.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MAX_STREAM_LOAD_CONTEXTS    4   /**< Maximum concurrent stream loads */
#define STREAM_LOAD_BUFFER_SIZE     4096 /**< Buffer size per context */

/* ============================================================================
 * Forward Declarations - APDU Command Handlers
 * ============================================================================ */

static u16 apdu_handler_load(GCOSVM *vm, const GCOSSApdu *apdu, 
                             u8 *response, u16 *resp_len);
static u16 apdu_handler_install(GCOSVM *vm, const GCOSSApdu *apdu, 
                                u8 *response, u16 *resp_len);
static u16 apdu_handler_delete(GCOSVM *vm, const GCOSSApdu *apdu, 
                               u8 *response, u16 *resp_len);
static u16 apdu_handler_select(GCOSVM *vm, const GCOSSApdu *apdu, 
                               u8 *response, u16 *resp_len);
static u16 apdu_handler_deselect(GCOSVM *vm, const GCOSSApdu *apdu, 
                                 u8 *response, u16 *resp_len);
static u16 apdu_handler_get_status(GCOSVM *vm, const GCOSSApdu *apdu, 
                                   u8 *response, u16 *resp_len);
static u16 apdu_handler_manage_channel(GCOSVM *vm, const GCOSSApdu *apdu, 
                                       u8 *response, u16 *resp_len);

/* ============================================================================
 * APDU Command Table
 * ============================================================================ */

/**
 * @brief Static APDU command dispatch table
 * 
 * Maps INS bytes to handler functions. This table is searched linearly,
 * so frequently used commands should be placed first for performance.
 */
static const ApduCommandEntry apdu_command_table[] = {
    { INS_SELECT,         apdu_handler_select,        "SELECT" },
    { INS_DESELECT,       apdu_handler_deselect,      "DESELECT" },
    { INS_LOAD,           apdu_handler_load,          "LOAD" },
    { INS_INSTALL,        apdu_handler_install,       "INSTALL" },
    { INS_DELETE,         apdu_handler_delete,        "DELETE" },
    { INS_GET_STATUS,     apdu_handler_get_status,    "GET STATUS" },
    { INS_MANAGE_CHANNEL, apdu_handler_manage_channel,"MANAGE CHANNEL" },
    { 0x00,               NULL,                       NULL }  /* Terminator */
};

/* ============================================================================
 * Stream Load Context Management
 * ============================================================================ */

/**
 * @brief Stream load context pool
 * 
 * Statically allocated contexts to avoid dynamic memory allocation.
 */
static StreamLoadContext stream_load_contexts[MAX_STREAM_LOAD_CONTEXTS];

/**
 * @brief Initialize stream load subsystem
 */
static void stream_load_init(void) {
    for (int i = 0; i < MAX_STREAM_LOAD_CONTEXTS; i++) {
        stream_load_contexts[i].context_id = 0;
        stream_load_contexts[i].state = STREAM_LOAD_IDLE;
        stream_load_contexts[i].total_size = 0;
        stream_load_contexts[i].received_size = 0;
        stream_load_contexts[i].buffer = NULL;
        stream_load_contexts[i].buffer_size = 0;
        stream_load_contexts[i].checksum = 0;
        stream_load_contexts[i].final_block = 0;
    }
}

/**
 * @brief Find free stream load context
 * 
 * @return Context index, or -1 if none available
 */
static int find_free_context(void) {
    for (int i = 0; i < MAX_STREAM_LOAD_CONTEXTS; i++) {
        if (stream_load_contexts[i].state == STREAM_LOAD_IDLE) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Find context by ID
 * 
 * @param context_id  Context ID to search for
 * @return Context index, or -1 if not found
 */
static int find_context_by_id(u8 context_id) {
    for (int i = 0; i < MAX_STREAM_LOAD_CONTEXTS; i++) {
        if (stream_load_contexts[i].context_id == context_id &&
            stream_load_contexts[i].state != STREAM_LOAD_IDLE) {
            return i;
        }
    }
    return -1;
}

/* ============================================================================
 * APDU Parsing Functions
 * ============================================================================ */

GCOSResult gcos_apdu_parse(const u8 *apdu_buffer, u8 apdu_length, 
                           GCOSSApdu *apdu) {
    if (apdu_buffer == NULL || apdu == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Validate minimum length */
    if (apdu_length < APDU_HEADER_MIN_LENGTH) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Parse header fields */
    apdu->cla = apdu_buffer[0];
    apdu->ins = apdu_buffer[1];
    apdu->p1 = apdu_buffer[2];
    apdu->p2 = apdu_buffer[3];
    
    /* Determine if this is a case 1, 2, 3, or 4 APDU */
    if (apdu_length == APDU_HEADER_LENGTH) {
        /* Case 1 or 2: No data field */
        apdu->lc = 0;
        apdu->data = NULL;
        apdu->le = apdu_buffer[4];
        apdu->has_data = 0;
    } else {
        /* Case 3 or 4: Has data field */
        apdu->lc = apdu_buffer[4];
        
        /* Validate data length */
        if (apdu_length < APDU_HEADER_LENGTH + apdu->lc) {
            return GCOS_ERR_INVALID_PARAM;
        }
        
        apdu->data = &apdu_buffer[APDU_HEADER_LENGTH];
        apdu->has_data = 1;
        
        /* Check for Le byte (case 4) */
        if (apdu_length > APDU_HEADER_LENGTH + apdu->lc) {
            apdu->le = apdu_buffer[APDU_HEADER_LENGTH + apdu->lc];
        } else {
            apdu->le = 0;
        }
    }
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * APDU Handler Dispatch
 * ============================================================================ */

ApduHandler gcos_apdu_find_handler(GCOSVM *vm, u8 ins) {
    (void)vm; /* VM parameter reserved for future use */
    
    const ApduCommandEntry *entry = apdu_command_table;
    while (entry->handler != NULL) {
        if (entry->ins == ins) {
            return entry->handler;
        }
        entry++;
    }
    
    return NULL; /* Not found */
}

/* ============================================================================
 * APDU Command Handlers (Stubs - to be implemented)
 * ============================================================================ */

/**
 * @brief LOAD command handler
 * 
 * Handles streaming module loading via state machine.
 * P1 indicates load phase:
 *   0x00 = Initialize
 *   0x01 = Receive data block
 *   0x02 = Finalize and install
 */
static u16 apdu_handler_load(GCOSVM *vm, const GCOSSApdu *apdu, 
                             u8 *response, u16 *resp_len) {
    (void)response;
    
    /* Set response length to 0 */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    switch (apdu->p1) {
        case 0x00: /* Initialize load */
            if (!apdu->has_data || apdu->lc < 4) {
                return SW_WRONG_LENGTH;
            }
            /* TODO: Implement stream load initialization */
            return SW_FUNCTION_NOT_SUPPORTED;
            
        case 0x01: /* Receive data block */
            if (!apdu->has_data || apdu->lc == 0) {
                return SW_WRONG_LENGTH;
            }
            /* TODO: Implement data block reception */
            return SW_FUNCTION_NOT_SUPPORTED;
            
        case 0x02: /* Finalize load */
            /* TODO: Implement load finalization */
            return SW_FUNCTION_NOT_SUPPORTED;
            
        default:
            return SW_INCORRECT_P1P2;
    }
}

/**
 * @brief INSTALL command handler
 * 
 * Installs a loaded module as an application instance.
 */
static u16 apdu_handler_install(GCOSVM *vm, const GCOSSApdu *apdu, 
                                u8 *response, u16 *resp_len) {
    (void)vm;
    (void)apdu;
    (void)response;
    
    /* Set response length to 0 */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    /* TODO: Implement application installation */
    return SW_FUNCTION_NOT_SUPPORTED;
}

/**
 * @brief DELETE command handler
 * 
 * Deletes an installed application or module.
 */
static u16 apdu_handler_delete(GCOSVM *vm, const GCOSSApdu *apdu, 
                               u8 *response, u16 *resp_len) {
    (void)vm;
    (void)apdu;
    (void)response;
    
    /* Set response length to 0 */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    /* TODO: Implement application deletion */
    return SW_FUNCTION_NOT_SUPPORTED;
}

/**
 * @brief SELECT command handler
 * 
 * Selects an application by AID for subsequent operations.
 */
static u16 apdu_handler_select(GCOSVM *vm, const GCOSSApdu *apdu, 
                               u8 *response, u16 *resp_len) {
    (void)vm;
    (void)apdu;
    (void)response;
    
    /* Set response length to 0 (no data returned for unsupported command) */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    /* TODO: Implement application selection */
    return SW_FUNCTION_NOT_SUPPORTED;
}

/**
 * @brief DESELECT command handler
 * 
 * Deselects the currently selected application.
 */
static u16 apdu_handler_deselect(GCOSVM *vm, const GCOSSApdu *apdu, 
                                 u8 *response, u16 *resp_len) {
    (void)vm;
    (void)apdu;
    (void)response;
    
    /* Set response length to 0 */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    /* TODO: Implement application deselection */
    return SW_FUNCTION_NOT_SUPPORTED;
}

/**
 * @brief GET STATUS command handler
 * 
 * Returns status information about applications/modules.
 */
static u16 apdu_handler_get_status(GCOSVM *vm, const GCOSSApdu *apdu, 
                                   u8 *response, u16 *resp_len) {
    (void)vm;
    (void)apdu;
    (void)response;
    
    /* Set response length to 0 */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    /* TODO: Implement status query */
    return SW_FUNCTION_NOT_SUPPORTED;
}

/**
 * @brief MANAGE CHANNEL command handler
 * 
 * Opens or closes logical channels.
 */
static u16 apdu_handler_manage_channel(GCOSVM *vm, const GCOSSApdu *apdu, 
                                       u8 *response, u16 *resp_len) {
    (void)vm;
    (void)apdu;
    (void)response;
    
    /* Set response length to 0 */
    if (resp_len != NULL) {
        *resp_len = 0;
    }
    
    /* TODO: Implement channel management */
    return SW_FUNCTION_NOT_SUPPORTED;
}

/* ============================================================================
 * Stream Load API Implementation
 * ============================================================================ */

GCOSResult gcos_stream_load_init(GCOSVM *vm, u32 total_size, u8 *context_id) {
    (void)vm;
    
    /* Find free context */
    int idx = find_free_context();
    if (idx < 0) {
        return GCOS_ERR_OUT_OF_MEMORY; /* No free contexts */
    }
    
    /* Initialize context */
    stream_load_contexts[idx].context_id = (u8)(idx + 1); /* Non-zero IDs */
    stream_load_contexts[idx].state = STREAM_LOAD_INIT;
    stream_load_contexts[idx].total_size = total_size;
    stream_load_contexts[idx].received_size = 0;
    stream_load_contexts[idx].checksum = 0;
    stream_load_contexts[idx].final_block = 0;
    
    /* Note: Buffer allocation would happen here in real implementation */
    /* For now, we use static buffer */
    
    if (context_id != NULL) {
        *context_id = stream_load_contexts[idx].context_id;
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_stream_load_receive(GCOSVM *vm, u8 context_id, 
                                    const u8 *data, u32 data_len, u8 is_final) {
    (void)vm;
    
    /* Find context */
    int idx = find_context_by_id(context_id);
    if (idx < 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Validate state */
    if (stream_load_contexts[idx].state != STREAM_LOAD_RECEIVING &&
        stream_load_contexts[idx].state != STREAM_LOAD_INIT) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Update state */
    stream_load_contexts[idx].state = STREAM_LOAD_RECEIVING;
    stream_load_contexts[idx].received_size += data_len;
    stream_load_contexts[idx].final_block = is_final;
    
    /* Update checksum (simple sum for now) */
    for (u32 i = 0; i < data_len; i++) {
        stream_load_contexts[idx].checksum += data[i];
    }
    
    /* Check if complete */
    if (is_final || stream_load_contexts[idx].received_size >= 
        stream_load_contexts[idx].total_size) {
        stream_load_contexts[idx].state = STREAM_LOAD_VERIFYING;
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_stream_load_complete(GCOSVM *vm, u8 context_id) {
    (void)vm;
    
    /* Find context */
    int idx = find_context_by_id(context_id);
    if (idx < 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Validate state */
    if (stream_load_contexts[idx].state != STREAM_LOAD_VERIFYING) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* TODO: Perform verification, linking, and installation */
    
    /* Mark as complete */
    stream_load_contexts[idx].state = STREAM_LOAD_COMPLETE;
    
    /* Reset context for reuse */
    stream_load_contexts[idx].state = STREAM_LOAD_IDLE;
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_stream_load_abort(GCOSVM *vm, u8 context_id) {
    (void)vm;
    
    /* Find context */
    int idx = find_context_by_id(context_id);
    if (idx < 0) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Reset context */
    stream_load_contexts[idx].state = STREAM_LOAD_IDLE;
    stream_load_contexts[idx].received_size = 0;
    stream_load_contexts[idx].checksum = 0;
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Main APDU Processing Function
 * ============================================================================ */

GCOSResult gcos_apdu_init(GCOSVM *vm) {
    (void)vm;
    
    /* Initialize stream load contexts */
    stream_load_init();
    
    return GCOS_SUCCESS;
}

u16 gcos_vm_process_apdu(GCOSVM *vm, const u8 *apdu_buffer, u8 apdu_length,
                         u8 *response_buffer, u16 *response_length) {
    /* Step 1: Validate input parameters */
    if (vm == NULL || apdu_buffer == NULL || response_buffer == NULL || 
        response_length == NULL) {
        return SW_NO_PRECISE_DIAGNOSIS;
    }
    
    /* Step 2: Validate APDU length */
    if (apdu_length < APDU_HEADER_MIN_LENGTH) {
        return SW_WRONG_LENGTH;
    }
    
    if (apdu_length > APDU_BUFFER_SIZE) {
        return SW_WRONG_LENGTH;
    }
    
    /* Step 3: Parse APDU */
    GCOSSApdu apdu;
    GCOSResult result = gcos_apdu_parse(apdu_buffer, apdu_length, &apdu);
    if (result != GCOS_SUCCESS) {
        return SW_WRONG_LENGTH;
    }
    
    /* Step 4: Extract logical channel from CLA byte */
    u8 channel = apdu.cla & APDU_CHANNEL_MASK;
    vm->current_channel = channel;
    
    /* Step 5: Find handler for this INS */
    ApduHandler handler = gcos_apdu_find_handler(vm, apdu.ins);
    if (handler == NULL) {
        return SW_INS_NOT_SUPPORTED;
    }
    
    /* Step 6: Execute handler */
    u16 sw = handler(vm, &apdu, response_buffer, response_length);
    
    /* Step 7: Return status word */
    return sw;
}

u16 gcos_vm_process_apdu_with_conn_type(GCOSVM *vm, const u8 *apdu_buffer, u8 apdu_length,
                                        u8 *response_buffer, u16 *response_length,
                                        GCOSConnType conn_type) {
    /* Step 0: Set connection type in VM context */
    if (vm != NULL) {
        vm->current_conn_type = conn_type;
        printf("[VM] Connection type set to: %s\n", 
               conn_type == GCOS_CONN_TYPE_T0 ? "T=0 (contacted)" : "T=CL (contactless)");
    }
    
    /* Step 1-7: Delegate to standard APDU processing */
    return gcos_vm_process_apdu(vm, apdu_buffer, apdu_length, 
                                response_buffer, response_length);
}
