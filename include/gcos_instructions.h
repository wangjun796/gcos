/**
 * @file gcos_instructions.h
 * @brief GCOS VM Instruction Set Definition - Based on COS3 Specification Appendix A
 * 
 * Instruction Categories:
 * - Control Instructions: Jump, call, return
 * - Numeric Instructions: Constants, arithmetic, comparison, bitwise operations
 * - Variable Instructions: Local variable read/write
 * - Memory Instructions: Load, store
 * - Exception Handling Instructions: Trap, catch
 * - Compound Instructions: Composite functionality
 */

#ifndef GCOS_INSTRUCTIONS_H
#define GCOS_INSTRUCTIONS_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opcode Definitions - Based on COS3 Specification Appendix A
 * ============================================================================ */

/* --- Control Instructions (0x00-0x1F) --- */
#define OP_TRAP             0x00    /* trap - Trap instruction */
#define OP_NOP              0x01    /* nop - No operation */
#define OP_POP              0x02    /* pop - Pop from stack */
#define OP_POP_IS           0x03    /* pop.is - Reclaim indirect stack */

#define OP_BR_S8            0x04    /* br.s8 - Unconditional jump (8-bit offset) */
#define OP_BR_S16           0x05    /* br.s16 - Unconditional jump (16-bit offset) */

#define OP_BEQZ_S8          0x06    /* beqz.s8 - Branch if equal to zero (8-bit) */
#define OP_BEQZ_S16         0x07    /* beqz.s16 - Branch if equal to zero (16-bit) */
#define OP_BNEZ_S8          0x08    /* bnez.s8 - Branch if not equal to zero (8-bit) */
#define OP_BNEZ_S16         0x09    /* bnez.s16 - Branch if not equal to zero (16-bit) */

#define OP_RET              0x0A    /* ret - Function return */
#define OP_IRET             0x0B    /* iret - Interrupt return */
#define OP_CALL             0x0C    /* call - Function call */
#define OP_CALL_INDIRECT    0x0D    /* call.indirect - Indirect call */

/* --- Numeric Instructions - Constants (0x18-0x22) --- */
#define OP_LDC_0            0x18    /* ldc.0 - load constant 0 */
#define OP_LDC_1            0x19    /* ldc.1 - load constant 1 */
#define OP_LDC_2            0x1A    /* ldc.2 - load constant 2 */
#define OP_LDC_3            0x1B    /* ldc.3 - load constant 3 */
#define OP_LDC_4            0x1C    /* ldc.4 - load constant 4 */
#define OP_LDC_5            0x1D    /* ldc.5 - load constant 5 */
#define OP_LDC_6            0x1E    /* ldc.6 - load constant 6 */
#define OP_LDC_7            0x1F    /* ldc.7 - load constant 7 */
#define OP_LDC_8            0x20    /* ldc.8 - load constant 8 */
#define OP_LDC_I32          0x21    /* ldc.i32 - load 32-bit integer constant */
#define OP_LDC_M1           0x22    /* ldc.m1 - load constant -1 */

/* --- Numeric Instructions - Arithmetic Operations (0x27-0x35) --- */
#define OP_ADD              0x27    /* add - Addition */
#define OP_SUB              0x28    /* sub - Subtraction */
#define OP_MUL              0x29    /* mul - Multiplication */
#define OP_DIVS             0x2A    /* divs - Signed division */
#define OP_DIVU             0x2B    /* divu - Unsigned division */
#define OP_REMS             0x2C    /* rems - Signed modulo */
#define OP_REMU             0x2D    /* remu - Unsigned modulo */

/* --- Numeric Instructions - Bitwise Operations (0x2E-0x35) --- */
#define OP_AND              0x2E    /* and - Bitwise AND */
#define OP_OR               0x2F    /* or - Bitwise OR */
#define OP_XOR              0x30    /* xor - Bitwise XOR */
#define OP_SHL              0x31    /* shl - Shift left */
#define OP_SHRS             0x32    /* shrs - Arithmetic shift right */
#define OP_SHRU             0x33    /* shru - Logical shift right */
#define OP_ROTL             0x34    /* rotl - Rotate left */
#define OP_ROTR             0x35    /* rotr - Rotate right */

/* --- Numeric Instructions - Comparison (0x38-0x42) --- */
#define OP_EQZ              0x38    /* eqz - Equal to zero */
#define OP_EQ               0x39    /* eq - Equal */
#define OP_NE               0x3A    /* ne - Not equal */
#define OP_LTS              0x3B    /* lts - Signed less than */
#define OP_LTU              0x3C    /* ltu - Unsigned less than */
#define OP_GTS              0x3D    /* gts - Signed greater than */
#define OP_GTU              0x3E    /* gtu - Unsigned greater than */
#define OP_LES              0x3F    /* les - Signed less than or equal */
#define OP_LEU              0x40    /* leu - Unsigned less than or equal */
#define OP_GES              0x41    /* ges - Signed greater than or equal */
#define OP_GEU              0x42    /* geu - Unsigned greater than or equal */

/* --- Variable Instructions (0x43-0x61) --- */
#define OP_LDT_0            0x43    /* ldt.0 - Load local variable 0 */
#define OP_LDT_1            0x44    /* ldt.1 - Load local variable 1 */
#define OP_LDT_2            0x45    /* ldt.2 - Load local variable 2 */
#define OP_LDT_3            0x46    /* ldt.3 - Load local variable 3 */
#define OP_LDT_4            0x47    /* ldt.4 - Load local variable 4 */
#define OP_LDT_5            0x48    /* ldt.5 - Load local variable 5 */
#define OP_LDT_6            0x49    /* ldt.6 - Load local variable 6 */
#define OP_LDT_7            0x4A    /* ldt.7 - Load local variable 7 */
#define OP_LDT_8            0x4B    /* ldt.8 - Load local variable 8 */
#define OP_LDT              0x4C    /* ldt - Load local variable */
#define OP_STT_0            0x4D    /* stt.0 - Store to local variable 0 */
#define OP_STT_1            0x4E    /* stt.1 - Store to local variable 1 */
#define OP_STT_2            0x4F    /* stt.2 - Store to local variable 2 */
#define OP_STT_3            0x50    /* stt.3 - Store to local variable 3 */
#define OP_STT_4            0x51    /* stt.4 - Store to local variable 4 */
#define OP_STT_5            0x52    /* stt.5 - Store to local variable 5 */
#define OP_STT_6            0x53    /* stt.6 - Store to local variable 6 */
#define OP_STT_7            0x54    /* stt.7 - Store to local variable 7 */
#define OP_STT_8            0x55    /* stt.8 - Store to local variable 8 */
#define OP_STT              0x56    /* stt - Store to local variable */
#define OP_STLDT_0          0x58    /* stldt.0 - Copy to local variable 0 */
#define OP_STLDT_1          0x59    /* stldt.1 - Copy to local variable 1 */
#define OP_STLDT_2          0x5A    /* stldt.2 - Copy to local variable 2 */
#define OP_STLDT_3          0x5B    /* stldt.3 - Copy to local variable 3 */
#define OP_STLDT_4          0x5C    /* stldt.4 - Copy to local variable 4 */
#define OP_STLDT_5          0x5D    /* stldt.5 - Copy to local variable 5 */
#define OP_STLDT_6          0x5E    /* stldt.6 - Copy to local variable 6 */
#define OP_STLDT_7          0x5F    /* stldt.7 - Copy to local variable 7 */
#define OP_STLDT_8          0x60    /* stldt.8 - Copy to local variable 8 */
#define OP_STLDT            0x61    /* stldt - Copy to local variable */

/* --- Memory Instructions (0x63-0x6C) --- */
#define OP_LDMS8            0x63    /* ldms8 - Load signed 8-bit */
#define OP_LDMU8            0x64    /* ldmu8 - Load unsigned 8-bit */
#define OP_LDMS16           0x65    /* ldms16 - Load signed 16-bit */
#define OP_LDMU16           0x66    /* ldmu16 - Load unsigned 16-bit */
#define OP_LDM32            0x67    /* ldm32 - Load 32-bit */
#define OP_STM8             0x68    /* stm8 - Store 8-bit */
#define OP_STM16            0x69    /* stm16 - Store 16-bit */
#define OP_STM32            0x6A    /* stm32 - Store 32-bit */
#define OP_MCOPY            0x6B    /* mcopy - Memory copy */
#define OP_MFILL            0x6C    /* mfill - Memory fill */

/* --- Exception Handling Instructions (0x6D-0x6E) --- */
#define OP_TRY              0x6D    /* try - Exception monitoring */
#define OP_CATCH            0x6E    /* catch - Exception catching */

/* --- Compound Instructions (0x6F-0x7D) --- */
#define OP_PICK             0x6F    /* pick - Conditional selection */
#define OP_DUP              0x70    /* dup - Duplicate stack top */
#define OP_SWAP             0x71    /* swap - Swap top two stack elements */
#define OP_OVER             0x72    /* over - Copy second-to-top to top */
#define OP_ROT              0x73    /* rot - Rotate top three stack elements */
#define OP_ADD_U8           0x76    /* add.u8 - Add with 8-bit immediate */
#define OP_ADD_U16          0x77    /* add.u16 - Add with 16-bit immediate */
#define OP_SUB_U8           0x78    /* sub.u8 - Subtract with 8-bit immediate */
#define OP_SUB_U16          0x79    /* sub.u16 - Subtract with 16-bit immediate */
#define OP_AND_U8           0x7A    /* and.u8 - Bitwise AND with 8-bit immediate */
#define OP_AND_U16          0x7B    /* and.u16 - Bitwise AND with 16-bit immediate */
#define OP_OR_U8            0x7C    /* or.u8 - Bitwise OR with 8-bit immediate */
#define OP_OR_U16           0x7D    /* or.u16 - Bitwise OR with 16-bit immediate */

/* --- Function Call Instructions (0x14-0x17) --- */
#define OP_CALLEX_U8         0x14    /* callex.u8 - Call external function */
#define OP_CALLIN_U8         0x15    /* callin.u8 - Call internal function */
#define OP_CALL_U16          0x16    /* call.u16 - Call function */
#define OP_CALLIND          0x17    /* callind - Indirect call */

/* ============================================================================
 * Additional Instructions (Reserved for future use)
 * ============================================================================ */
#define OP_DUP              0xF0    /* dup - Duplicate stack top */
#define OP_SWAP             0xF1    /* swap - Swap top two stack elements */
#define OP_OVER             0xF2    /* over - Copy second-to-top to top */
#define OP_ROT              0xF3    /* rot - Rotate top three stack elements */

/* --- Double-byte Opcode Prefixes (0xFC-0xFE) --- */
#define OP_PREFIX_FC        0xFC    /* Double-byte opcode prefix */
#define OP_PREFIX_FD        0xFD    /* Double-byte opcode prefix */
#define OP_PREFIX_FE        0xFE    /* Double-byte opcode prefix */

/* ============================================================================
 * Instruction Information Structure
 * ============================================================================ */

/**
 * @brief Instruction metadata
 */
typedef struct {
    u16 opcode;                 /* Opcode */
    const char *mnemonic;       /* Mnemonic */
    GCOSOpCodeCategory category;/* Instruction category */
    u8 operand_size;            /* Operand size (bytes) */
    const char *description;    /* Description */
} GCOSInstructionInfo;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get instruction information
 * @param opcode Opcode
 * @return Pointer to instruction info, NULL if invalid opcode
 */
const GCOSInstructionInfo* gcos_instruction_get_info(u16 opcode);

/**
 * @brief Get opcode category
 * @param opcode Opcode
 * @return Instruction category
 */
GCOSOpCodeCategory gcos_instruction_get_category(u16 opcode);

/**
 * @brief Get opcode mnemonic
 * @param opcode Opcode
 * @return Mnemonic string
 */
const char* gcos_instruction_get_mnemonic(u16 opcode);

/**
 * @brief Get operand size
 * @param opcode Opcode
 * @return Operand size (bytes)
 */
u8 gcos_instruction_get_operand_size(u16 opcode);

/**
 * @brief Check if opcode is valid
 * @param opcode Opcode
 * @return true if valid, false if invalid
 */
bool gcos_instruction_is_valid(u16 opcode);

/**
 * @brief Check if opcode is double-byte
 * @param opcode Opcode
 * @return true if double-byte, false if single-byte
 */
bool gcos_instruction_is_double_byte(u16 opcode);

/**
 * @brief Print instruction information
 * @param opcode Opcode
 */
void gcos_instruction_print_info(u16 opcode);

/**
 * @brief Get total number of instructions
 * @return Total instruction count
 */
u32 gcos_instruction_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_INSTRUCTIONS_H */
