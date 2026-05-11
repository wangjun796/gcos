#include "vm_instructions.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * 指令信息表
 * ============================================================================ */

static const InstructionInfo instruction_table[] = {
    /* 控制流指令 */
    {OP_NOP,        "NOP",      OP_CATEGORY_CONTROL,    0, {}, "空操作"},
    {OP_TRAP,       "TRAP",     OP_CATEGORY_CONTROL,    1, {0}, "陷阱指令"},
    {OP_RET,        "RET",      OP_CATEGORY_CONTROL,    0, {}, "返回"},
    {OP_IRET,       "IRET",     OP_CATEGORY_CONTROL,    0, {}, "中断返回"},
    
    {OP_BR_S8,      "BR.S8",    OP_CATEGORY_CONTROL,    1, {0}, "无条件跳转（8位）"},
    {OP_BR_S16,     "BR.S16",   OP_CATEGORY_CONTROL,    1, {0}, "无条件跳转（16位）"},
    
    {OP_BEQZ_S8,    "BEQZ.S8",  OP_CATEGORY_CONTROL,    2, {0, 0}, "为零则跳转（8位）"},
    {OP_BEQZ_S16,   "BEQZ.S16", OP_CATEGORY_CONTROL,    2, {0, 0}, "为零则跳转（16位）"},
    {OP_BNEZ_S8,    "BNEZ.S8",  OP_CATEGORY_CONTROL,    2, {0, 0}, "非零则跳转（8位）"},
    {OP_BNEZ_S16,   "BNEZ.S16", OP_CATEGORY_CONTROL,    2, {0, 0}, "非零则跳转（16位）"},
    
    /* 调用指令 */
    {OP_CALL_U16,   "CALL.U16", OP_CATEGORY_CONTROL,    1, {0}, "调用函数"},
    {OP_CALLIND,    "CALLIND",  OP_CATEGORY_CONTROL,    0, {}, "间接调用"},
    
    /* 栈操作 */
    {OP_POP,        "POP",      OP_CATEGORY_CONTROL,    0, {}, "弹出栈顶"},
    {OP_POP_IS,     "POP.IS",   OP_CATEGORY_CONTROL,    0, {}, "弹出到间接栈"},
    
    /* 常量加载 */
    {OP_LDC_0,      "LDC.0",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数0"},
    {OP_LDC_1,      "LDC.1",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数1"},
    {OP_LDC_M1,     "LDC.M1",   OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数-1"},
    {OP_LDC_U8,     "LDC.U8",   OP_CATEGORY_ARITHMETIC, 1, {0}, "加载8位无符号数"},
    {OP_LDC_U16,    "LDC.U16",  OP_CATEGORY_ARITHMETIC, 1, {0}, "加载16位无符号数"},
    {OP_LDC_I32,    "LDC.I32",  OP_CATEGORY_ARITHMETIC, 1, {0}, "加载32位整数"},
    
    /* 算术运算 */
    {OP_ADD,        "ADD",      OP_CATEGORY_ARITHMETIC, 0, {}, "加法"},
    {OP_SUB,        "SUB",      OP_CATEGORY_ARITHMETIC, 0, {}, "减法"},
    {OP_MUL,        "MUL",      OP_CATEGORY_ARITHMETIC, 0, {}, "乘法"},
    {OP_DIVS,       "DIVS",     OP_CATEGORY_ARITHMETIC, 0, {}, "有符号除法"},
    {OP_DIVU,       "DIVU",     OP_CATEGORY_ARITHMETIC, 0, {}, "无符号除法"},
    
    /* 逻辑运算 */
    {OP_AND,        "AND",      OP_CATEGORY_LOGIC,      0, {}, "按位与"},
    {OP_OR,         "OR",       OP_CATEGORY_LOGIC,      0, {}, "按位或"},
    {OP_XOR,        "XOR",      OP_CATEGORY_LOGIC,      0, {}, "按位异或"},
    {OP_SHL,        "SHL",      OP_CATEGORY_LOGIC,      0, {}, "左移"},
    {OP_SHRS,       "SHRS",     OP_CATEGORY_LOGIC,      0, {}, "算术右移"},
    {OP_SHRU,       "SHRU",     OP_CATEGORY_LOGIC,      0, {}, "逻辑右移"},
    
    /* 比较指令 */
    {OP_EQZ,        "EQZ",      OP_CATEGORY_COMPARISON, 0, {}, "等于零"},
    {OP_EQ,         "EQ",       OP_CATEGORY_COMPARISON, 0, {}, "相等"},
    {OP_NE,         "NE",       OP_CATEGORY_COMPARISON, 0, {}, "不等"},
    {OP_LTS,        "LTS",      OP_CATEGORY_COMPARISON, 0, {}, "有符号小于"},
    {OP_LTU,        "LTU",      OP_CATEGORY_COMPARISON, 0, {}, "无符号小于"},
    
    /* 数据访问 */
    {OP_LDT_0,      "LDT.0",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量0"},
    {OP_LDT_1,      "LDT.1",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量1"},
    {OP_STT_0,      "STT.0",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量0"},
    {OP_STT_1,      "STT.1",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量1"},
    
    {OP_LDMU8,      "LDMU8",    OP_CATEGORY_MEMORY,     0, {}, "加载无符号8位内存"},
    {OP_LDMU16,     "LDMU16",   OP_CATEGORY_MEMORY,     0, {}, "加载无符号16位内存"},
    {OP_LDM32,      "LDM32",    OP_CATEGORY_MEMORY,     0, {}, "加载32位内存"},
    {OP_STM8,       "STM8",     OP_CATEGORY_MEMORY,     0, {}, "存储8位内存"},
    {OP_STM16,      "STM16",    OP_CATEGORY_MEMORY,     0, {}, "存储16位内存"},
    {OP_STM32,      "STM32",    OP_CATEGORY_MEMORY,     0, {}, "存储32位内存"},
};

static const int instruction_table_size = sizeof(instruction_table) / sizeof(InstructionInfo);

/* ============================================================================
 * API 实现
 * ============================================================================ */

const InstructionInfo* vm_get_instruction_info(u8 opcode) {
    for (int i = 0; i < instruction_table_size; i++) {
        if (instruction_table[i].opcode == opcode) {
            return &instruction_table[i];
        }
    }
    return NULL;
}

int vm_decode_instruction(const u8 *code, u32 code_size, u32 offset,
                          u8 *opcode, u32 *operands, u8 *operand_count) {
    if (code == NULL || opcode == NULL || operands == NULL || operand_count == NULL) {
        return 0;
    }
    
    if (offset >= code_size) {
        return 0;
    }
    
    *opcode = code[offset];
    *operand_count = 0;
    
    const InstructionInfo *info = vm_get_instruction_info(*opcode);
    if (info == NULL) {
        return 1; /* 未知指令，长度为1 */
    }
    
    /* 根据指令类型解码操作数 */
    int instr_len = 1; /* 操作码长度 */
    
    /* 简化实现：根据不同指令格式解析操作数 */
    /* 实际应该根据标准详细实现 */
    
    switch (*opcode) {
        case OP_BR_S8:
        case OP_BEQZ_S8:
        case OP_BNEZ_S8:
        case OP_BEQ_S8:
        case OP_BNE_S8:
        case OP_BLTU_S8:
        case OP_BGTU_S8:
        case OP_BLEU_S8:
        case OP_BGEU_S8:
        case OP_BRTB_S8:
            /* 8位偏移 */
            if (offset + 1 < code_size) {
                operands[0] = (s8)code[offset + 1];
                *operand_count = 1;
                instr_len = 2;
            }
            break;
            
        case OP_BR_S16:
        case OP_BEQZ_S16:
        case OP_BNEZ_S16:
        case OP_BRTB_S16:
            /* 16位偏移 */
            if (offset + 2 < code_size) {
                operands[0] = (s16)(code[offset + 1] | (code[offset + 2] << 8));
                *operand_count = 1;
                instr_len = 3;
            }
            break;
            
        case OP_LDC_U8:
            if (offset + 1 < code_size) {
                operands[0] = code[offset + 1];
                *operand_count = 1;
                instr_len = 2;
            }
            break;
            
        case OP_LDC_U16:
            if (offset + 2 < code_size) {
                operands[0] = code[offset + 1] | (code[offset + 2] << 8);
                *operand_count = 1;
                instr_len = 3;
            }
            break;
            
        case OP_LDC_I32:
            if (offset + 4 < code_size) {
                operands[0] = code[offset + 1] | 
                             (code[offset + 2] << 8) |
                             (code[offset + 3] << 16) |
                             (code[offset + 4] << 24);
                *operand_count = 1;
                instr_len = 5;
            }
            break;
            
        case OP_CALL_U16:
            if (offset + 2 < code_size) {
                operands[0] = code[offset + 1] | (code[offset + 2] << 8);
                *operand_count = 1;
                instr_len = 3;
            }
            break;
            
        default:
            /* 无操作数指令 */
            break;
    }
    
    return instr_len;
}

int vm_execute_instruction(VMContext *vm, u8 opcode,
                           const u32 *operands, u8 operand_count) {
    if (vm == NULL) {
        return -1;
    }
    
    u32 val1, val2, result;
    
    switch (opcode) {
        /* --- 栈操作 --- */
        case OP_POP:
            return vm_stack_pop(vm, &val1);
        
        /* --- 常量加载 --- */
        case OP_LDC_0:
            return vm_stack_push(vm, 0);
        case OP_LDC_1:
            return vm_stack_push(vm, 1);
        case OP_LDC_M1:
            return vm_stack_push(vm, 0xFFFFFFFF); /* -1的补码 */
        case OP_LDC_U8:
            if (operand_count < 1) return -1;
            return vm_stack_push(vm, operands[0]);
        case OP_LDC_U16:
            if (operand_count < 1) return -1;
            return vm_stack_push(vm, operands[0]);
        case OP_LDC_I32:
            if (operand_count < 1) return -1;
            return vm_stack_push(vm, operands[0]);
        
        /* --- 算术运算 --- */
        case OP_ADD:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 + val2;
            return vm_stack_push(vm, result);
            
        case OP_SUB:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 - val2;
            return vm_stack_push(vm, result);
            
        case OP_MUL:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 * val2;
            return vm_stack_push(vm, result);
            
        case OP_DIVS:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (val2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = (s32)val1 / (s32)val2;
            return vm_stack_push(vm, result);
            
        case OP_DIVU:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (val2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = val1 / val2;
            return vm_stack_push(vm, result);
        
        /* --- 逻辑运算 --- */
        case OP_AND:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 & val2;
            return vm_stack_push(vm, result);
            
        case OP_OR:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 | val2;
            return vm_stack_push(vm, result);
            
        case OP_XOR:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 ^ val2;
            return vm_stack_push(vm, result);
            
        case OP_SHL:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 << (val2 & 0x1F);
            return vm_stack_push(vm, result);
            
        case OP_SHRU:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = val1 >> (val2 & 0x1F);
            return vm_stack_push(vm, result);
            
        case OP_SHRS:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = (s32)val1 >> (val2 & 0x1F);
            return vm_stack_push(vm, result);
        
        /* --- 比较指令 --- */
        case OP_EQZ:
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = (val1 == 0) ? 1 : 0;
            return vm_stack_push(vm, result);
            
        case OP_EQ:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = (val1 == val2) ? 1 : 0;
            return vm_stack_push(vm, result);
            
        case OP_NE:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = (val1 != val2) ? 1 : 0;
            return vm_stack_push(vm, result);
            
        case OP_LTS:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = ((s32)val1 < (s32)val2) ? 1 : 0;
            return vm_stack_push(vm, result);
            
        case OP_LTU:
            if (vm_stack_pop(vm, &val2) != 0) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            result = (val1 < val2) ? 1 : 0;
            return vm_stack_push(vm, result);
        
        /* --- 控制流 --- */
        case OP_NOP:
            return 0;
            
        case OP_RET:
            /* 从当前函数返回 */
            {
                u32 return_addr;
                int ret = vm_executor_pop_frame(vm, &return_addr);
                if (ret != 0) return ret;
                vm->program_counter = return_addr;
            }
            return 0;
            
        case OP_BR_S8:
            if (operand_count < 1) return -1;
            vm->program_counter += (s8)operands[0];
            return 0;
            
        case OP_BEQZ_S8:
            if (operand_count < 1) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (val1 == 0) {
                vm->program_counter += (s8)operands[0];
            }
            return 0;
            
        case OP_BNEZ_S8:
            if (operand_count < 1) return -1;
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            if (val1 != 0) {
                vm->program_counter += (s8)operands[0];
            }
            return 0;
        
        /* --- 数据访问 --- */
        case OP_LDT_0:
        case OP_LDT_1:
            /* 简化实现：从局部变量加载 */
            /* 实际应该根据栈帧计算地址 */
            return vm_stack_push(vm, 0);
            
        case OP_STT_0:
        case OP_STT_1:
            /* 简化实现：存储到局部变量 */
            if (vm_stack_pop(vm, &val1) != 0) return -1;
            return 0;
        
        /* --- 未实现的指令 --- */
        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            printf("Warning: Unimplemented opcode 0x%02X\n", opcode);
            return -1;
    }
}

void vm_print_instruction(u8 opcode, const u32 *operands, u8 operand_count) {
    const InstructionInfo *info = vm_get_instruction_info(opcode);
    
    if (info == NULL) {
        printf("UNKNOWN 0x%02X", opcode);
        return;
    }
    
    printf("%s", info->mnemonic);
    
    for (u8 i = 0; i < operand_count; i++) {
        if (i == 0) {
            printf(" ");
        }
        printf("0x%08X", operands[i]);
        if (i < operand_count - 1) {
            printf(", ");
        }
    }
}

OpCodeCategory vm_get_opcode_category(u8 opcode) {
    const InstructionInfo *info = vm_get_instruction_info(opcode);
    if (info == NULL) {
        return OP_CATEGORY_CONTROL;
    }
    return info->category;
}

bool vm_is_valid_opcode(u8 opcode) {
    return vm_get_instruction_info(opcode) != NULL;
}
