/**
 * @file gcos_flash_exec.h
 * @brief GCOS Flash Execution Interface (XIP - Execute In Place)
 * 
 * Provides interfaces for executing bytecode directly from Flash:
 * - Fetch instructions from Flash (no RAM copy)
 * - Read data from Flash
 * - Minimal RAM usage for runtime state
 * 
 * This is critical for smart card environments with limited RAM (8-64KB).
 * Code stays in Flash, only runtime state (PC, stack) is in RAM.
 * 
 * @version 1.0.0
 * @date 2026-05-12
 */

#ifndef GCOS_FLASH_EXEC_H
#define GCOS_FLASH_EXEC_H

#include "gcos_vm.h"
#include "eflash_ftl.h"  /* eflash library */

/* ============================================================================
 * Flash Access Macros (XIP - Execute In Place)
 * ============================================================================ */

/**
 * @brief Fetch single byte from Flash
 * @param flash_offset Absolute Flash offset
 * @return Byte value
 * 
 * Usage:
 *   u8 opcode = FLASH_FETCH_BYTE(vm->runtime.code_flash_offset + pc);
 */
static inline u8 FLASH_FETCH_BYTE(u32 flash_offset) {
    return eflash_read_byte(flash_offset);
}

/**
 * @brief Fetch 16-bit value from Flash (little-endian)
 * @param flash_offset Absolute Flash offset
 * @return 16-bit value
 * 
 * Usage:
 *   u16 operand = FLASH_FETCH_U16(vm->runtime.code_flash_offset + pc);
 */
static inline u16 FLASH_FETCH_U16(u32 flash_offset) {
    u8 b0 = eflash_read_byte(flash_offset);
    u8 b1 = eflash_read_byte(flash_offset + 1);
    return (u16)b0 | ((u16)b1 << 8);
}

/**
 * @brief Fetch 32-bit value from Flash (little-endian)
 * @param flash_offset Absolute Flash offset
 * @return 32-bit value
 * 
 * Usage:
 *   u32 operand = FLASH_FETCH_U32(vm->runtime.code_flash_offset + pc);
 */
static inline u32 FLASH_FETCH_U32(u32 flash_offset) {
    u8 b0 = eflash_read_byte(flash_offset);
    u8 b1 = eflash_read_byte(flash_offset + 1);
    u8 b2 = eflash_read_byte(flash_offset + 2);
    u8 b3 = eflash_read_byte(flash_offset + 3);
    return (u32)b0 | ((u32)b1 << 8) | 
           ((u32)b2 << 16) | ((u32)b3 << 24);
}

/**
 * @brief Fetch bytecode instruction from current module
 * @param vm VM instance
 * @param pc_offset Program counter offset (relative to code start)
 * @return Opcode byte
 * 
 * This is the primary interface for instruction fetching.
 * Code is read directly from Flash - NO RAM copy.
 */
static inline u8 FETCH_OPCODE(GCOSVM *vm, u32 pc_offset) {
    return FLASH_FETCH_BYTE(vm->runtime.code_flash_offset + pc_offset);
}

/**
 * @brief Fetch 16-bit operand from current module
 * @param vm VM instance
 * @param pc_offset Program counter offset
 * @return 16-bit operand
 */
static inline u16 FETCH_OPERAND_U16(GCOSVM *vm, u32 pc_offset) {
    return FLASH_FETCH_U16(vm->runtime.code_flash_offset + pc_offset);
}

/**
 * @brief Fetch 32-bit operand from current module
 * @param vm VM instance
 * @param pc_offset Program counter offset
 * @return 32-bit operand
 */
static inline u32 FETCH_OPERAND_U32(GCOSVM *vm, u32 pc_offset) {
    return FLASH_FETCH_U32(vm->runtime.code_flash_offset + pc_offset);
}

/* ============================================================================
 * Flash Execution Functions
 * ============================================================================ */

/**
 * @brief Execute single instruction from Flash
 * @param vm VM instance
 * @return GCOSResult Success or error code
 * 
 * This function:
 * 1. Fetches opcode from Flash (XIP)
 * 2. Decodes instruction
 * 3. Fetches operands from Flash if needed
 * 4. Executes instruction
 * 5. Updates program counter
 * 
 * All code access is from Flash - minimal RAM usage.
 */
GCOSResult flash_execute_instruction(GCOSVM *vm);

/**
 * @brief Execute module from Flash until completion or exception
 * @param vm VM instance
 * @return GCOSResult Success or error code
 * 
 * Main execution loop for XIP mode.
 */
GCOSResult flash_execute_module(GCOSVM *vm);

/**
 * @brief Verify code integrity in Flash
 * @param flash_offset Code offset in Flash
 * @param code_size Code size
 * @param expected_checksum Expected CRC32 checksum
 * @return true if valid, false otherwise
 */
bool flash_verify_code_integrity(u32 flash_offset, u32 code_size, u32 expected_checksum);

/* ============================================================================
 * Flash Storage Management
 * ============================================================================ */

/**
 * @brief Allocate Flash space for SEF file
 * @param sef_size SEF file size
 * @return Flash offset, or 0xFFFFFFFF if failed
 */
u32 flash_allocate_sef_storage(u32 sef_size);

/**
 * @brief Write SEF file to Flash
 * @param flash_offset Flash offset
 * @param sef_data SEF file data
 * @param sef_size SEF file size
 * @return GCOSResult Success or error code
 */
GCOSResult flash_write_sef(u32 flash_offset, const u8 *sef_data, u32 sef_size);

/**
 * @brief Read SEF section from Flash (streaming parse)
 * @param sef_flash_offset SEF file offset in Flash
 * @param section_offset Section offset within SEF
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return GCOSResult Success or error code
 */
GCOSResult flash_read_sef_section(u32 sef_flash_offset, u32 section_offset, 
                                  u8 *buffer, u32 size);

#endif /* GCOS_FLASH_EXEC_H */
