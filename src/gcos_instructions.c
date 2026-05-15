/**
 * @file gcos_instructions.c
 * @brief GCOS VM Instruction Set Implementation
 * 
 * Implements the COS3 specification instruction set:
 * - Control instructions (jump, call, return)
 * - Numeric instructions (constants, arithmetic, comparison, bitwise)
 * - Variable instructions (local variable read/write)
 * - Memory instructions (load, store)
 * - Exception handling instructions
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_instructions.h"
#include "gcos_platform.h"
#include "gcos_flash_exec.h"     /* NEW: Flash execution support */
#include <string.h>

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

#define POP_STACK(vm, value) \
    do { \
        GCOSResult _ret = gcos_vm_stack_pop((vm), &(value)); \
        if (_ret != GCOS_OK) return _ret; \
    } while(0)

#define PUSH_STACK(vm, value) \
    do { \
        GCOSResult _ret = gcos_vm_stack_push((vm), (value)); \
        if (_ret != GCOS_OK) return _ret; \
    } while(0)

#define READ_BYTE(vm, byte) \
    do { \
        if (vm->runtime.program_counter >= vm->runtime.code_size) { \
            vm->runtime.exception = EXCEPTION_ACCESS_VIOLATION; \
            return GCOS_ERROR_MEMORY_ACCESS; \
        } \
        /* SMART CARD: Read from Flash (XIP - Execute In Place) */ \
        u32 flash_addr = vm->runtime.code_flash_offset + vm->runtime.program_counter; \
        (byte) = FLASH_FETCH_BYTE(flash_addr); \
        vm->runtime.program_counter++; \
    } while(0)

#define READ_S8(vm, val) \
    do { \
        u8 _b; \
        READ_BYTE(vm, _b); \
        (val) = (s8)_b; \
    } while(0)

#define READ_U16(vm, val) \
    do { \
        u8 _b1, _b2; \
        READ_BYTE(vm, _b1); \
        READ_BYTE(vm, _b2); \
        (val) = ((u16)_b1 << 8) | (u16)_b2; \
    } while(0)

#define READ_S16(vm, val) \
    do { \
        u16 _u16; \
        READ_U16(vm, _u16); \
        (val) = (s16)_u16; \
    } while(0)

/* ============================================================================
 * Control Instructions (0x00-0x1F)
 * ============================================================================ */

static GCOSResult exec_trap(GCOSVM *vm) {
    u8 trap_code;
    READ_BYTE(vm, trap_code);
    
    GCOS_PRINTF("[Instruction] TRAP code=%u\n", trap_code);
    
    /* Set exception based on trap code */
    vm->runtime.exception = (GCOSExceptionType)trap_code;
    vm->state = GCOS_STATE_EXCEPTION;
    
    return GCOS_SUCCESS;
}

static GCOSResult exec_nop(GCOSVM *vm) {
    GCOS_PRINTF("[Instruction] NOP\n");
    return GCOS_SUCCESS;
}

static GCOSResult exec_pop(GCOSVM *vm) {
    u32 value;
    POP_STACK(vm, value);
    GCOS_PRINTF("[Instruction] POP value=%u\n", value);
    return GCOS_SUCCESS;
}

static GCOSResult exec_pop_is(GCOSVM *vm) {
    /* Pop indirect stack */
    if (vm->runtime.indirect_stack_pointer == 0) {
        vm->runtime.exception = EXCEPTION_STACK_UNDERFLOW;
        return GCOS_ERROR_STACK_UNDERFLOW;
    }
    vm->runtime.indirect_stack_pointer--;
    GCOS_PRINTF("[Instruction] POP.IS\n");
    return GCOS_SUCCESS;
}

static GCOSResult exec_br_s8(GCOSVM *vm) {
    s8 offset;
    READ_S8(vm, offset);
    
    vm->runtime.program_counter += offset;
    GCOS_PRINTF("[Instruction] BR.S8 offset=%d\n", offset);
    return GCOS_SUCCESS;
}

static GCOSResult exec_br_s16(GCOSVM *vm) {
    s16 offset;
    READ_S16(vm, offset);
    
    vm->runtime.program_counter += offset;
    GCOS_PRINTF("[Instruction] BR.S16 offset=%d\n", offset);
    return GCOS_SUCCESS;
}

static GCOSResult exec_beqz_s8(GCOSVM *vm) {
    s8 offset;
    u32 value;
    
    READ_S8(vm, offset);
    POP_STACK(vm, value);
    
    if (value == 0) {
        vm->runtime.program_counter += offset;
    }
    
    GCOS_PRINTF("[Instruction] BEQZ.S8 offset=%d, value=%u\n", offset, value);
    return GCOS_SUCCESS;
}

static GCOSResult exec_bnez_s8(GCOSVM *vm) {
    s8 offset;
    u32 value;
    
    READ_S8(vm, offset);
    POP_STACK(vm, value);
    
    if (value != 0) {
        vm->runtime.program_counter += offset;
    }
    
    GCOS_PRINTF("[Instruction] BNEZ.S8 offset=%d, value=%u\n", offset, value);
    return GCOS_SUCCESS;
}

static GCOSResult exec_ret(GCOSVM *vm) {
    if (vm->runtime.frame_top == 0) {
        vm->runtime.exception = EXCEPTION_STACK_UNDERFLOW;
        return GCOS_ERROR_STACK_UNDERFLOW;
    }
    
    /* Pop frame and restore PC */
    vm->runtime.frame_top--;
    GCOSStackFrame *frame = &vm->runtime.frame_stack[vm->runtime.frame_top];
    vm->runtime.program_counter = frame->return_address;
    
    GCOS_PRINTF("[Instruction] RET pc=%u\n", vm->runtime.program_counter);
    return GCOS_SUCCESS;
}

static GCOSResult exec_call(GCOSVM *vm) {
    u16 target_addr;
    READ_U16(vm, target_addr);
    
    if (vm->runtime.frame_top >= GCOS_MAX_FRAME_DEPTH) {
        vm->runtime.exception = EXCEPTION_STACK_OVERFLOW;
        return GCOS_ERROR_STACK_OVERFLOW;
    }
    
    /* Push new frame */
    GCOSStackFrame *frame = &vm->runtime.frame_stack[vm->runtime.frame_top];
    frame->return_address = vm->runtime.program_counter;
    frame->base_pointer = vm->runtime.stack_pointer;
    frame->frame_size = 0;
    vm->runtime.frame_top++;
    
    /* Jump to target */
    vm->runtime.program_counter = target_addr;
    
    GCOS_PRINTF("[Instruction] CALL addr=%u\n", target_addr);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Numeric Instructions - Constants (0x18-0x22)
 * ============================================================================ */

static GCOSResult exec_ldc_i32(GCOSVM *vm, u32 value) {
    PUSH_STACK(vm, value);
    GCOS_PRINTF("[Instruction] LDC value=%u\n", value);
    return GCOS_SUCCESS;
}

static GCOSResult exec_ldc_0(GCOSVM *vm) { return exec_ldc_i32(vm, 0); }
static GCOSResult exec_ldc_1(GCOSVM *vm) { return exec_ldc_i32(vm, 1); }
static GCOSResult exec_ldc_2(GCOSVM *vm) { return exec_ldc_i32(vm, 2); }
static GCOSResult exec_ldc_3(GCOSVM *vm) { return exec_ldc_i32(vm, 3); }
static GCOSResult exec_ldc_4(GCOSVM *vm) { return exec_ldc_i32(vm, 4); }
static GCOSResult exec_ldc_5(GCOSVM *vm) { return exec_ldc_i32(vm, 5); }
static GCOSResult exec_ldc_6(GCOSVM *vm) { return exec_ldc_i32(vm, 6); }
static GCOSResult exec_ldc_7(GCOSVM *vm) { return exec_ldc_i32(vm, 7); }
static GCOSResult exec_ldc_8(GCOSVM *vm) { return exec_ldc_i32(vm, 8); }

static GCOSResult exec_ldc_m1(GCOSVM *vm) {
    PUSH_STACK(vm, 0xFFFFFFFF); /* -1 in two's complement */
    GCOS_PRINTF("[Instruction] LDC.M1 value=-1\n");
    return GCOS_SUCCESS;
}

static GCOSResult exec_ldc_i32_full(GCOSVM *vm) {
    u32 value;
    READ_U16(vm, value); /* Read upper 16 bits */
    u32 value2;
    READ_U16(vm, value2); /* Read lower 16 bits */
    value = (value << 16) | value2;
    
    PUSH_STACK(vm, value);
    GCOS_PRINTF("[Instruction] LDC.I32 value=%u\n", value);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Numeric Instructions - Arithmetic (0x27-0x2D)
 * ============================================================================ */

static GCOSResult exec_add(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a + b);
    GCOS_PRINTF("[Instruction] ADD %u + %u = %u\n", a, b, a + b);
    return GCOS_SUCCESS;
}

static GCOSResult exec_sub(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a - b);
    GCOS_PRINTF("[Instruction] SUB %u - %u = %u\n", a, b, a - b);
    return GCOS_SUCCESS;
}

static GCOSResult exec_mul(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a * b);
    GCOS_PRINTF("[Instruction] MUL %u * %u = %u\n", a, b, a * b);
    return GCOS_SUCCESS;
}

static GCOSResult exec_divs(GCOSVM *vm) {
    s32 a, b;
    POP_STACK(vm, *(u32*)&b);
    POP_STACK(vm, *(u32*)&a);
    
    if (b == 0) {
        vm->runtime.exception = EXCEPTION_DIVISION_BY_ZERO;
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    s32 result = a / b;
    PUSH_STACK(vm, *(u32*)&result);
    GCOS_PRINTF("[Instruction] DIVS %d / %d = %d\n", a, b, result);
    return GCOS_SUCCESS;
}

static GCOSResult exec_divu(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    
    if (b == 0) {
        vm->runtime.exception = EXCEPTION_DIVISION_BY_ZERO;
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    u32 result = a / b;
    PUSH_STACK(vm, result);
    GCOS_PRINTF("[Instruction] DIVU %u / %u = %u\n", a, b, result);
    return GCOS_SUCCESS;
}

static GCOSResult exec_rems(GCOSVM *vm) {
    s32 a, b;
    POP_STACK(vm, *(u32*)&b);
    POP_STACK(vm, *(u32*)&a);
    
    if (b == 0) {
        vm->runtime.exception = EXCEPTION_DIVISION_BY_ZERO;
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    s32 result = a % b;
    PUSH_STACK(vm, *(u32*)&result);
    GCOS_PRINTF("[Instruction] REMS %d %% %d = %d\n", a, b, result);
    return GCOS_SUCCESS;
}

static GCOSResult exec_remu(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    
    if (b == 0) {
        vm->runtime.exception = EXCEPTION_DIVISION_BY_ZERO;
        return GCOS_ERROR_INVALID_PARAM;
    }
    
    u32 result = a % b;
    PUSH_STACK(vm, result);
    GCOS_PRINTF("[Instruction] REMU %u %% %u = %u\n", a, b, result);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Numeric Instructions - Bitwise (0x2E-0x35)
 * ============================================================================ */

static GCOSResult exec_and(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a & b);
    return GCOS_SUCCESS;
}

static GCOSResult exec_or(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a | b);
    return GCOS_SUCCESS;
}

static GCOSResult exec_xor(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a ^ b);
    return GCOS_SUCCESS;
}

static GCOSResult exec_shl(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a << (b & 0x1F));
    return GCOS_SUCCESS;
}

static GCOSResult exec_shrs(GCOSVM *vm) {
    s32 a;
    u32 b;
    POP_STACK(vm, b);
    POP_STACK(vm, *(u32*)&a);
    s32 result = a >> (b & 0x1F);
    PUSH_STACK(vm, *(u32*)&result);
    return GCOS_SUCCESS;
}

static GCOSResult exec_shru(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a >> (b & 0x1F));
    return GCOS_SUCCESS;
}

static GCOSResult exec_rotl(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    u32 shift = b & 0x1F;
    u32 result = (a << shift) | (a >> (32 - shift));
    PUSH_STACK(vm, result);
    return GCOS_SUCCESS;
}

static GCOSResult exec_rotr(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    u32 shift = b & 0x1F;
    u32 result = (a >> shift) | (a << (32 - shift));
    PUSH_STACK(vm, result);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Numeric Instructions - Comparison (0x38-0x42)
 * ============================================================================ */

static GCOSResult exec_eqz(GCOSVM *vm) {
    u32 a;
    POP_STACK(vm, a);
    PUSH_STACK(vm, a == 0 ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_eq(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a == b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_ne(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a != b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_lts(GCOSVM *vm) {
    s32 a, b;
    POP_STACK(vm, *(u32*)&b);
    POP_STACK(vm, *(u32*)&a);
    PUSH_STACK(vm, a < b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_ltu(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a < b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_gts(GCOSVM *vm) {
    s32 a, b;
    POP_STACK(vm, *(u32*)&b);
    POP_STACK(vm, *(u32*)&a);
    PUSH_STACK(vm, a > b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_gtu(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a > b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_les(GCOSVM *vm) {
    s32 a, b;
    POP_STACK(vm, *(u32*)&b);
    POP_STACK(vm, *(u32*)&a);
    PUSH_STACK(vm, a <= b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_leu(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a <= b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_ges(GCOSVM *vm) {
    s32 a, b;
    POP_STACK(vm, *(u32*)&b);
    POP_STACK(vm, *(u32*)&a);
    PUSH_STACK(vm, a >= b ? 1 : 0);
    return GCOS_SUCCESS;
}

static GCOSResult exec_geu(GCOSVM *vm) {
    u32 a, b;
    POP_STACK(vm, b);
    POP_STACK(vm, a);
    PUSH_STACK(vm, a >= b ? 1 : 0);
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Main Instruction Dispatcher
 * ============================================================================ */

/**
 * @brief Execute a single instruction
 * @param vm VM instance
 * @return GCOSResult Success or error code
 */
GCOSResult gcos_instruction_execute(GCOSVM *vm) {
    if (vm == NULL || vm->state != GCOS_STATE_RUNNING) {
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Check program counter bounds */
    if (vm->runtime.program_counter >= vm->runtime.code_size) {
        GCOS_PRINTF("[Instruction] PC out of bounds: %u >= %u\n", 
                   vm->runtime.program_counter, vm->runtime.code_size);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Fetch opcode from Flash (XIP - Execute In Place) */
    u32 flash_addr = vm->runtime.code_flash_offset + vm->runtime.program_counter;
    u8 opcode = FLASH_FETCH_BYTE(flash_addr);
    vm->runtime.program_counter++;
    
    /* Dispatch based on opcode */
    switch (opcode) {
        /* Control Instructions */
        case OP_TRAP:          return exec_trap(vm);
        case OP_NOP:           return exec_nop(vm);
        case OP_POP:           return exec_pop(vm);
        case OP_POP_IS:        return exec_pop_is(vm);
        case OP_BR_S8:         return exec_br_s8(vm);
        case OP_BR_S16:        return exec_br_s16(vm);
        case OP_BEQZ_S8:       return exec_beqz_s8(vm);
        case OP_BNEZ_S8:       return exec_bnez_s8(vm);
        case OP_RET:           return exec_ret(vm);
        case OP_CALL:          return exec_call(vm);
        
        /* Constant Instructions */
        case OP_LDC_0:         return exec_ldc_0(vm);
        case OP_LDC_1:         return exec_ldc_1(vm);
        case OP_LDC_2:         return exec_ldc_2(vm);
        case OP_LDC_3:         return exec_ldc_3(vm);
        case OP_LDC_4:         return exec_ldc_4(vm);
        case OP_LDC_5:         return exec_ldc_5(vm);
        case OP_LDC_6:         return exec_ldc_6(vm);
        case OP_LDC_7:         return exec_ldc_7(vm);
        case OP_LDC_8:         return exec_ldc_8(vm);
        case OP_LDC_I32:       return exec_ldc_i32_full(vm);
        case OP_LDC_M1:        return exec_ldc_m1(vm);
        
        /* Arithmetic Instructions */
        case OP_ADD:           return exec_add(vm);
        case OP_SUB:           return exec_sub(vm);
        case OP_MUL:           return exec_mul(vm);
        case OP_DIVS:          return exec_divs(vm);
        case OP_DIVU:          return exec_divu(vm);
        case OP_REMS:          return exec_rems(vm);
        case OP_REMU:          return exec_remu(vm);
        
        /* Bitwise Instructions */
        case OP_AND:           return exec_and(vm);
        case OP_OR:            return exec_or(vm);
        case OP_XOR:           return exec_xor(vm);
        case OP_SHL:           return exec_shl(vm);
        case OP_SHRS:          return exec_shrs(vm);
        case OP_SHRU:          return exec_shru(vm);
        case OP_ROTL:          return exec_rotl(vm);
        case OP_ROTR:          return exec_rotr(vm);
        
        /* Comparison Instructions */
        case OP_EQZ:           return exec_eqz(vm);
        case OP_EQ:            return exec_eq(vm);
        case OP_NE:            return exec_ne(vm);
        case OP_LTS:           return exec_lts(vm);
        case OP_LTU:           return exec_ltu(vm);
        case OP_GTS:           return exec_gts(vm);
        case OP_GTU:           return exec_gtu(vm);
        case OP_LES:           return exec_les(vm);
        case OP_LEU:           return exec_leu(vm);
        case OP_GES:           return exec_ges(vm);
        case OP_GEU:           return exec_geu(vm);
        
        default:
            GCOS_PRINTF("[Instruction] Unknown opcode: 0x%02X\n", opcode);
            vm->runtime.exception = EXCEPTION_INVALID_OPCODE;
            vm->state = GCOS_STATE_EXCEPTION;
            return GCOS_ERR_INVALID_OPCODE;
    }
}
