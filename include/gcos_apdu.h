/**
 * @file gcos_apdu.h
 * @brief GCOS VM APDU Processing
 * 
 * Handles APDU command dispatching based on cref architecture.
 * GP management commands are handled by ISD, other commands by selected applet.
 */

#ifndef GCOS_APDU_H
#define GCOS_APDU_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Status Words (Based on ISO 7816-4 and GP Specification)
 * ============================================================================ */

#define SW_SUCCESS                      0x9000  /* Success */
#define SW_NO_PRECISE_DIAGNOSIS         0x6F00  /* No precise diagnosis */
#define SW_WRONG_LENGTH                 0x6700  /* Wrong length */
#define SW_INS_NOT_SUPPORTED            0x6D00  /* INS not supported */
#define SW_CLA_NOT_SUPPORTED            0x6E00  /* CLA not supported */
#define SW_FILE_NOT_FOUND               0x6A82  /* File not found */
#define SW_WRONG_DATA                   0x6A80  /* Wrong data */
#define SW_CONDITIONS_NOT_SATISFIED     0x6985  /* Conditions not satisfied */
#define SW_INCORRECT_P1P2              0x6A86  /* Incorrect P1-P2 */
#define SW_EXECUTION_ERROR              0x6F00  /* Execution error */

/* ============================================================================
 * APDU Processing API
 * ============================================================================ */

/**
 * @brief Process APDU command
 * 
 * ⭐ Core function based on cref architecture:
 * 1. Parse APDU
 * 2. If GP management command → call ISD's process()
 * 3. Else if selected_app exists → call selected_app->process()
 * 4. Else → return 0x6F00
 * 
 * @param vm VM instance
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length (output)
 * @return Status word (SW)
 */
u16 gcos_process_apdu(GCOSVM *vm, 
                      const u8 *apdu, 
                      u16 apdu_len,
                      u8 *response, 
                      u16 *resp_len);

/**
 * @brief Check if command is a GP management command
 * 
 * @param cla CLA byte
 * @param ins INS byte
 * @return true if GP management command, false otherwise
 */
bool is_gp_management_command(u8 cla, u8 ins);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_APDU_H */
