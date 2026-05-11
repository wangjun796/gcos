/**
 * @file vm_executor_part2.c
 * @brief GCOS VM 执行器指令执行部分
 * 
 * 实现所有指令的执行逻辑
 */

#include "vm_core.h"
#include "vm_instructions.h"
#include "vm_memory.h"
#include <stdio.h>

/* ============================================================================
 * 指令执行函数
 * ============================================================================ */

/**
 * @brief 执行控制流指令
 */
static int execute_control(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 val1, val2, result;
    s32 offset;

    switch (opcode) {
        case OP_NOP:
            /* 空操作，什么都不做 */
            return 0;

        case OP_TRAP:
            /* 触发安全异常 */
            vm->exception = EXCEPTION_SECURITY_VIOLATION;
            return -1;

        case OP_RET:
            /* 函数返回 */
            if (vm->frame_top == 0) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }

            StackFrame *frame = &vm->frame_stack[--vm->frame_top];
            vm->stack_pointer = frame->local_vars_offset;
            vm->base_pointer = frame->base_pointer;
            vm->program_counter = frame->return_address;
            return 0;

        case OP_IRET:
            /* 中断返回 - 类似RET但处理中断上下文 */
            if (vm->frame_top == 0) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }

            StackFrame *frame = &vm->frame_stack[--vm->frame_top];
            vm->stack_pointer = frame->local_vars_offset;
            vm->base_pointer = frame->base_pointer;
            vm->program_counter = frame->return_address;
            return 0;

        case OP_BR_S8:
            /* 8位偏移跳转 */
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            offset = (s8)operands[0];
            vm->program_counter += offset;
            return 0;

        case OP_BR_S16:
            /* 16位偏移跳转 */
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            offset = (s16)((operands[0] & 0xFF) | (operands[1] << 8));
            vm->program_counter += offset;
            return 0;

        case OP_BEQZ_S8:
            /* 为零则8位跳转 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 == 0) {
                if (operand_count < 1) {
                    vm->exception = EXCEPTION_INVALID_OPERAND;
                    return -1;
                }
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BEQZ_S16:
            /* 为零则16位跳转 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 == 0) {
                if (operand_count < 2) {
                    vm->exception = EXCEPTION_INVALID_OPERAND;
                    return -1;
                }
                offset = (s16)((operands[0] & 0xFF) | (operands[1] << 8));
                vm->program_counter += offset;
            }
            return 0;

        case OP_BNEZ_S8:
            /* 非零则8位跳转 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 != 0) {
                if (operand_count < 1) {
                    vm->exception = EXCEPTION_INVALID_OPERAND;
                    return -1;
                }
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BNEZ_S16:
            /* 非零则16位跳转 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 != 0) {
                if (operand_count < 2) {
                    vm->exception = EXCEPTION_INVALID_OPERAND;
                    return -1;
                }
                offset = (s16)((operands[0] & 0xFF) | (operands[1] << 8));
                vm->program_counter += offset;
            }
            return 0;

        case OP_CALL_U16:
            /* 调用函数 */
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            u16 func_id = (u16)((operands[0] & 0xFF) | (operands[1] << 8));
            return call_function(vm, func_id);

        case OP_CALLIND:
            /* 间接调用 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            return call_function(vm, (u16)val1);

        case OP_CALLEX_U8:
            /* 调用外部函数 */
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            u16 ext_func_id = (u16)operands[0];
            /* TODO: 实现外部函数调用 */
            printf("Warning: External function call not implemented: %u\n", ext_func_id);
            return 0;

        case OP_CALLIN_U8:
            /* 调用内部函数 */
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            u16 int_func_id = (u16)operands[0];
            return call_function(vm, int_func_id);

        case OP_POP:
            /* 弹出栈顶 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            return 0;

        case OP_POP_IS:
            /* 弹出到间接栈 */
            if (vm->indirect_stack_pointer == 0) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            vm->indirect_stack_pointer--;
            return 0;

        case OP_PICK:
            /* 条件选择 */
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &result) != 0) return -1;
            vm_stack_push(vm, (result != 0) ? val1 : val2);
            return 0;

        case OP_DUP:
            /* 复制栈顶 */
            if (vm_stack_peek(vm, &val1) != 0) return -1;
            return vm_stack_push(vm, val1);

        case OP_SWAP:
            /* 交换栈顶两个元素 */
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            vm_stack_push(vm, val1);
            vm_stack_push(vm, val2);
            return 0;

        case OP_OVER:
            /* 复制次栈顶到栈顶 */
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            vm_stack_push(vm, val1);
            vm_stack_push(vm, val2);
            vm_stack_push(vm, val1);
            return 0;

        case OP_ROT:
            /* 旋转栈顶三个元素 */
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &result) != 0) return -1;
            vm_stack_push(vm, val2);
            vm_stack_push(vm, val1);
            vm_stack_push(vm, result);
            return 0;

        case OP_TRY:
            /* 异常监测 */
            /* TODO: 实现异常处理 */
            vm_stack_push(vm, 0);
            return 0;

        case OP_CATCH:
            /* 异常捕获 */
            /* TODO: 实现异常处理 */
            vm_stack_push(vm, 1);
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行算术运算指令
 */
static int execute_arithmetic(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 op1, op2, result;

    switch (opcode) {
        case OP_ADD:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 + op2;
            return vm_stack_push(vm, result);

        case OP_SUB:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 - op2;
            return vm_stack_push(vm, result);

        case OP_MUL:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 * op2;
            return vm_stack_push(vm, result);

        case OP_DIVS:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (op2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = (s32)op1 / (s32)op2;
            return vm_stack_push(vm, result);

        case OP_DIVU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (op2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = op1 / op2;
            return vm_stack_push(vm, result);

        case OP_REMS:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (op2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = (s32)op1 % (s32)op2;
            return vm_stack_push(vm, result);

        case OP_REMU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (op2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = op1 % op2;
            return vm_stack_push(vm, result);

        case OP_ADD_U8:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 + (operands[0] & 0xFF);
            return vm_stack_push(vm, result);

        case OP_ADD_U16:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 + ((operands[0] & 0xFF) | (operands[1] << 8));
            return vm_stack_push(vm, result);

        case OP_SUB_U8:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 - (operands[0] & 0xFF);
            return vm_stack_push(vm, result);

        case OP_SUB_U16:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 - ((operands[0] & 0xFF) | (operands[1] << 8));
            return vm_stack_push(vm, result);

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行位运算指令
 */
static int execute_bitwise(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 op1, op2, result;

    switch (opcode) {
        case OP_AND:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 & op2;
            return vm_stack_push(vm, result);

        case OP_OR:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 | op2;
            return vm_stack_push(vm, result);

        case OP_XOR:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 ^ op2;
            return vm_stack_push(vm, result);

        case OP_SHL:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 << (op2 & 0x1F);
            return vm_stack_push(vm, result);

        case OP_SHRS:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = (s32)op1 >> (op2 & 0x1F);
            return vm_stack_push(vm, result);

        case OP_SHRU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = op1 >> (op2 & 0x1F);
            return vm_stack_push(vm, result);

        case OP_ROTL:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = (op1 << (op2 & 0x1F)) | (op1 >> (32 - (op2 & 0x1F)));
            return vm_stack_push(vm, result);

        case OP_ROTR:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            result = (op1 >> (op2 & 0x1F)) | (op1 << (32 - (op2 & 0x1F)));
            return vm_stack_push(vm, result);

        case OP_AND_U8:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 & (operands[0] & 0xFF);
            return vm_stack_push(vm, result);

        case OP_AND_U16:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 & ((operands[0] & 0xFF) | (operands[1] << 8));
            return vm_stack_push(vm, result);

        case OP_OR_U8:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 | (operands[0] & 0xFF);
            return vm_stack_push(vm, result);

        case OP_OR_U16:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            result = op1 | ((operands[0] & 0xFF) | (operands[1] << 8));
            return vm_stack_push(vm, result);

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行比较指令
 */
static int execute_compare(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 op1, op2;
    s32 s1, s2;

    switch (opcode) {
        case OP_EQZ:
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 == 0) ? 1 : 0);
            return 0;

        case OP_EQ:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 == op2) ? 1 : 0);
            return 0;

        case OP_NE:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 != op2) ? 1 : 0);
            return 0;

        case OP_LTS:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            s1 = (s32)op1;
            s2 = (s32)op2;
            vm_stack_push(vm, (s1 < s2) ? 1 : 0);
            return 0;

        case OP_LTU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 < op2) ? 1 : 0);
            return 0;

        case OP_GTS:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            s1 = (s32)op1;
            s2 = (s32)op2;
            vm_stack_push(vm, (s1 > s2) ? 1 : 0);
            return 0;

        case OP_GTU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 > op2) ? 1 : 0);
            return 0;

        case OP_LES:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            s1 = (s32)op1;
            s2 = (s32)op2;
            vm_stack_push(vm, (s1 <= s2) ? 1 : 0);
            return 0;

        case OP_LEU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 <= op2) ? 1 : 0);
            return 0;

        case OP_GES:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            s1 = (s32)op1;
            s2 = (s32)op2;
            vm_stack_push(vm, (s1 >= s2) ? 1 : 0);
            return 0;

        case OP_GEU:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            vm_stack_push(vm, (op1 >= op2) ? 1 : 0);
            return 0;

        case OP_EQ_U8:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            vm_stack_push(vm, ((op1 & 0xFF) == (op2 & 0xFF)) ? 1 : 0);
            return 0;

        case OP_EQ_U16:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            vm_stack_push(vm, (op1 == op2) ? 1 : 0);
            return 0;

        case OP_NE_U8:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            vm_stack_push(vm, ((op1 & 0xFF) != (op2 & 0xFF)) ? 1 : 0);
            return 0;

        case OP_NE_U16:
            if (vm_stack_pop(vm, &op2) != 0) return -1;
            if (vm_stack_pop(vm, &op1) != 0) return -1;
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            vm_stack_push(vm, (op1 != op2) ? 1 : 0);
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行内存和变量指令
 */
static int execute_memory(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 addr, value, offset;
    u32 *var_addr;

    switch (opcode) {
        /* 局部变量加载 */
        case OP_LDT_0:
        case OP_LDT_1:
        case OP_LDT_2:
        case OP_LDT_3:
        case OP_LDT_4:
        case OP_LDT_5:
        case OP_LDT_6:
        case OP_LDT_7:
        case OP_LDT_8:
            var_addr = get_local_var_addr(vm, opcode - OP_LDT_0);
            if (var_addr == NULL) return -1;
            return vm_stack_push(vm, *var_addr);

        case OP_LDT:
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            var_addr = get_local_var_addr(vm, operands[0]);
            if (var_addr == NULL) return -1;
            return vm_stack_push(vm, *var_addr);

        /* 局部变量存储 */
        case OP_STT_0:
        case OP_STT_1:
        case OP_STT_2:
        case OP_STT_3:
        case OP_STT_4:
        case OP_STT_5:
        case OP_STT_6:
        case OP_STT_7:
        case OP_STT_8:
            if (vm_stack_pop(vm, &value) != 0) return -1;
            var_addr = get_local_var_addr(vm, opcode - OP_STT_0);
            if (var_addr == NULL) return -1;
            *var_addr = value;
            return 0;

        case OP_STT:
            if (vm_stack_pop(vm, &value) != 0) return -1;
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            var_addr = get_local_var_addr(vm, operands[0]);
            if (var_addr == NULL) return -1;
            *var_addr = value;
            return 0;

        /* 全局/域变量加载 */
        case OP_STLDT_0:
        case OP_STLDT_1:
        case OP_STLDT_2:
        case OP_STLDT_3:
        case OP_STLDT_4:
        case OP_STLDT_5:
        case OP_STLDT_6:
        case OP_STLDT_7:
        case OP_STLDT_8:
            var_addr = get_global_var_addr(vm, opcode - OP_STLDT_0);
            if (var_addr == NULL) return -1;
            return vm_stack_push(vm, *var_addr);

        case OP_STLDT:
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            var_addr = get_global_var_addr(vm, operands[0]);
            if (var_addr == NULL) return -1;
            return vm_stack_push(vm, *var_addr);

        /* 内存加载 */
        case OP_LDMS8:
        case OP_LDMU8:
            if (vm_stack_pop(vm, &addr) != 0) return -1;
            if (addr >= VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            value = (opcode == OP_LDMS8) ? (s8)vm->module_code[addr] : vm->module_code[addr];
            return vm_stack_push(vm, value);

        case OP_LDMS16:
        case OP_LDMU16:
            if (vm_stack_pop(vm, &addr) != 0) return -1;
            if (addr + 1 >= VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            value = (opcode == OP_LDMS16) ? 
                (s16)(vm->module_code[addr] | (vm->module_code[addr + 1] << 8)) :
                (vm->module_code[addr] | (vm->module_code[addr + 1] << 8));
            return vm_stack_push(vm, value);

        case OP_LDM32:
            if (vm_stack_pop(vm, &addr) != 0) return -1;
            if (addr + 3 >= VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            value = vm->module_code[addr] | 
                   (vm->module_code[addr + 1] << 8) |
                   (vm->module_code[addr + 2] << 16) |
                   (vm->module_code[addr + 3] << 24);
            return vm_stack_push(vm, value);

        /* 内存存储 */
        case OP_STM8:
            if (vm_stack_pop(vm, &value) != 0) return -1;
            if (vm_stack_pop(vm, &addr) != 0) return -1;
            if (addr >= VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            vm->module_code[addr] = (u8)value;
            return 0;

        case OP_STM16:
            if (vm_stack_pop(vm, &value) != 0) return -1;
            if (vm_stack_pop(vm, &addr) != 0) return -1;
            if (addr + 1 >= VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            vm->module_code[addr] = (u8)value;
            vm->module_code[addr + 1] = (u8)(value >> 8);
            return 0;

        case OP_STM32:
            if (vm_stack_pop(vm, &value) != 0) return -1;
            if (vm_stack_pop(vm, &addr) != 0) return -1;
            if (addr + 3 >= VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            vm->module_code[addr] = (u8)value;
            vm->module_code[addr + 1] = (u8)(value >> 8);
            vm->module_code[addr + 2] = (u8)(value >> 16);
            vm->module_code[addr + 3] = (u8)(value >> 24);
            return 0;

        case OP_MCOPY:
            /* 内存拷贝 */
            if (vm_stack_pop(vm, &value) != 0) return -1; /* len */
            if (vm_stack_pop(vm, &addr) != 0) return -1; /* src */
            if (vm_stack_pop(vm, &offset) != 0) return -1; /* dest */
            if (addr + offset + value > VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            memmove(vm->module_code + offset, vm->module_code + addr, value);
            return 0;

        case OP_MFILL:
            /* 内存填充 */
            if (vm_stack_pop(vm, &value) != 0) return -1; /* fill byte */
            if (vm_stack_pop(vm, &offset) != 0) return -1; /* len */
            if (vm_stack_pop(vm, &addr) != 0) return -1; /* dest */
            if (addr + offset > VM_MODULE_CODE_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            memset(vm->module_code + addr, (u8)value, offset);
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行常量加载指令
 */
static int execute_constant(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 value;

    switch (opcode) {
        case OP_LDC_0:
            return vm_stack_push(vm, 0);

        case OP_LDC_1:
            return vm_stack_push(vm, 1);

        case OP_LDC_2:
            return vm_stack_push(vm, 2);

        case OP_LDC_3:
            return vm_stack_push(vm, 3);

        case OP_LDC_4:
            return vm_stack_push(vm, 4);

        case OP_LDC_5:
            return vm_stack_push(vm, 5);

        case OP_LDC_6:
            return vm_stack_push(vm, 6);

        case OP_LDC_7:
            return vm_stack_push(vm, 7);

        case OP_LDC_8:
            return vm_stack_push(vm, 8);

        case OP_LDC_M1:
            return vm_stack_push(vm, 0xFFFFFFFF);

        case OP_LDC_U8:
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            return vm_stack_push(vm, operands[0] & 0xFF);

        case OP_LDC_U16:
            if (operand_count < 2) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            value = (operands[0] & 0xFF) | (operands[1] << 8);
            return vm_stack_push(vm, value);

        case OP_LDC_I32:
            if (operand_count < 4) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            value = operands[0] | (operands[1] << 8) | (operands[2] << 16) | (operands[3] << 24);
            return vm_stack_push(vm, value);

        case OP_LDC_A8:
        case OP_LDC_A16:
            /* 地址加载 - 实现为常量 */
            if (operand_count < 1) {
                vm->exception = EXCEPTION_INVALID_OPERAND;
                return -1;
            }
            value = (opcode == OP_LDC_A8) ? operands[0] & 0xFF :
                     (operands[0] & 0xFF) | (operands[1] << 8);
            return vm_stack_push(vm, value);

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 主指令执行函数
 */
int vm_execute_instruction(VMContext *vm, u8 opcode,
                            const u32 *operands, u8 operand_count) {
    if (vm == NULL) {
        return -1;
    }

    /* 打印调试信息 */
    if (executor_state.enable_trace) {
        const InstructionInfo *info = vm_get_instruction_info(opcode);
        if (info != NULL) {
            printf("[PC=%04X] %s", vm->program_counter, info->mnemonic);
            for (u8 i = 0; i < operand_count; i++) {
                printf(" %08X", operands[i]);
            }
            printf("\n");
        }
    }

    /* 根据指令分类分发 */
    OpCodeCategory category = vm_get_opcode_category(opcode);

    switch (category) {
        case OP_CATEGORY_CONTROL:
            return execute_control(vm, opcode, operands, operand_count);

        case OP_CATEGORY_ARITHMETIC:
            return execute_arithmetic(vm, opcode, operands, operand_count);

        case OP_CATEGORY_LOGIC:
        case OP_CATEGORY_COMPARISON:
            return execute_bitwise(vm, opcode, operands, operand_count);

        case OP_CATEGORY_MEMORY:
            return execute_memory(vm, opcode, operands, operand_count);

        case OP_CATEGORY_CONVERSION:
        case OP_CATEGORY_TRAP:
            /* TODO: 实现类型转换和陷阱指令 */
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}
