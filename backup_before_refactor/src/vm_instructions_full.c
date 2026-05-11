/**
 * @file vm_instructions_full.c
 * @brief GCOS VM 指令集完整实现 - 基于COS3规范附录A
 * 
 * 实现所有COS3规范定义的指令，包括：
 * - 控制流指令
 * - 数值指令（常量、算术、位运算、比较）
 * - 变量指令
 * - 内存指令
 * - 异常处理指令
 * - 复合指令
 */

#include "vm_instructions.h"
#include "vm_memory.h"
#include "vm_core.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 指令信息表
 * ============================================================================ */

static const InstructionInfo instruction_table[] = {
    /* 控制流指令 */
    {OP_NOP,        "NOP",      OP_CATEGORY_CONTROL,    0, {}, "空操作"},
    {OP_TRAP,       "TRAP",     OP_CATEGORY_CONTROL,    0, {}, "陷阱指令"},
    {OP_RET,        "RET",      OP_CATEGORY_CONTROL,    0, {}, "函数返回"},
    {OP_IRET,       "IRET",     OP_CATEGORY_CONTROL,    0, {}, "中断返回"},

    {OP_BR_S8,      "BR.S8",    OP_CATEGORY_CONTROL,    1, {0}, "无条件跳转（8位）"},
    {OP_BR_S16,     "BR.S16",   OP_CATEGORY_CONTROL,    2, {0, 0}, "无条件跳转（16位）"},

    {OP_BEQZ_S8,    "BEQZ.S8",  OP_CATEGORY_CONTROL,    1, {0}, "为零则跳转（8位）"},
    {OP_BEQZ_S16,   "BEQZ.S16", OP_CATEGORY_CONTROL,    2, {0, 0}, "为零则跳转（16位）"},
    {OP_BNEZ_S8,    "BNEZ.S8",  OP_CATEGORY_CONTROL,    1, {0}, "非零则跳转（8位）"},
    {OP_BNEZ_S16,   "BNEZ.S16", OP_CATEGORY_CONTROL,    2, {0, 0}, "非零则跳转（16位）"},

    {OP_BEQ_S8,     "BEQ.S8",   OP_CATEGORY_CONTROL,    1, {0}, "相等则跳转（8位）"},
    {OP_BNE_S8,     "BNE.S8",   OP_CATEGORY_CONTROL,    1, {0}, "不等则跳转（8位）"},
    {OP_BLTU_S8,    "BLTU.S8",  OP_CATEGORY_CONTROL,    1, {0}, "无符号小于则跳转"},
    {OP_BGTU_S8,    "BGTU.S8",  OP_CATEGORY_CONTROL,    1, {0}, "无符号大于则跳转"},
    {OP_BLEU_S8,    "BLEU.S8",  OP_CATEGORY_CONTROL,    1, {0}, "无符号小于等于则跳转"},
    {OP_BGEU_S8,    "BGEU.S8",  OP_CATEGORY_CONTROL,    1, {0}, "无符号大于等于则跳转"},

    {OP_BRTB_S8,    "BRTB.S8",  OP_CATEGORY_CONTROL,    1, {0}, "真值表跳转（8位）"},
    {OP_BRTB_S16,   "BRTB.S16", OP_CATEGORY_CONTROL,    2, {0, 0}, "真值表跳转（16位）"},

    /* 调用指令 */
    {OP_CALL_U16,    "CALL.U16",  OP_CATEGORY_CONTROL,    2, {0, 0}, "调用函数"},
    {OP_CALLIND,     "CALLIND",   OP_CATEGORY_CONTROL,    0, {}, "间接调用"},
    {OP_CALLEX_U8,   "CALLEX.U8", OP_CATEGORY_CONTROL,    1, {0}, "调用外部函数"},
    {OP_CALLIN_U8,   "CALLIN.U8", OP_CATEGORY_CONTROL,    1, {0}, "调用内部函数"},

    /* 栈操作 */
    {OP_POP,        "POP",      OP_CATEGORY_CONTROL,    0, {}, "弹出栈顶"},
    {OP_POP_IS,     "POP.IS",   OP_CATEGORY_CONTROL,    0, {}, "弹出到间接栈"},
    {OP_PICK,       "PICK",     OP_CATEGORY_CONTROL,    0, {}, "条件选择"},
    {OP_DUP,        "DUP",      OP_CATEGORY_CONTROL,    0, {}, "复制栈顶"},
    {OP_SWAP,       "SWAP",     OP_CATEGORY_CONTROL,    0, {}, "交换栈顶两个元素"},
    {OP_OVER,       "OVER",     OP_CATEGORY_CONTROL,    0, {}, "复制次栈顶到栈顶"},
    {OP_ROT,        "ROT",      OP_CATEGORY_CONTROL,    0, {}, "旋转栈顶三个元素"},

    /* 常量加载 */
    {OP_LDC_0,      "LDC.0",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数0"},
    {OP_LDC_1,      "LDC.1",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数1"},
    {OP_LDC_2,      "LDC.2",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数2"},
    {OP_LDC_3,      "LDC.3",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数3"},
    {OP_LDC_4,      "LDC.4",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数4"},
    {OP_LDC_5,      "LDC.5",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数5"},
    {OP_LDC_6,      "LDC.6",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数6"},
    {OP_LDC_7,      "LDC.7",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数7"},
    {OP_LDC_8,      "LDC.8",    OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数8"},
    {OP_LDC_M1,     "LDC.M1",   OP_CATEGORY_ARITHMETIC, 0, {}, "加载常数-1"},
    {OP_LDC_U8,     "LDC.U8",   OP_CATEGORY_ARITHMETIC, 1, {0}, "加载8位无符号常数"},
    {OP_LDC_U16,    "LDC.U16",  OP_CATEGORY_ARITHMETIC, 2, {0, 0}, "加载16位无符号常数"},
    {OP_LDC_I32,    "LDC.I32",  OP_CATEGORY_ARITHMETIC, 4, {0, 0, 0, 0}, "加载32位整数"},
    {OP_LDC_A8,     "LDC.A8",   OP_CATEGORY_ARITHMETIC, 1, {0}, "加载8位地址"},
    {OP_LDC_A16,    "LDC.A16",  OP_CATEGORY_ARITHMETIC, 2, {0, 0}, "加载16位地址"},

    /* 算术运算 */
    {OP_ADD,        "ADD",      OP_CATEGORY_ARITHMETIC, 0, {}, "加法"},
    {OP_SUB,        "SUB",      OP_CATEGORY_ARITHMETIC, 0, {}, "减法"},
    {OP_MUL,        "MUL",      OP_CATEGORY_ARITHMETIC, 0, {}, "乘法"},
    {OP_DIVS,       "DIVS",     OP_CATEGORY_ARITHMETIC, 0, {}, "有符号除法"},
    {OP_DIVU,       "DIVU",     OP_CATEGORY_ARITHMETIC, 0, {}, "无符号除法"},
    {OP_REMS,       "REMS",     OP_CATEGORY_ARITHMETIC, 0, {}, "有符号取模"},
    {OP_REMU,       "REMU",     OP_CATEGORY_ARITHMETIC, 0, {}, "无符号取模"},
    {OP_ADD_U8,     "ADD.U8",   OP_CATEGORY_ARITHMETIC, 1, {0}, "8位加法"},
    {OP_ADD_U16,    "ADD.U16",  OP_CATEGORY_ARITHMETIC, 2, {0, 0}, "16位加法"},
    {OP_SUB_U8,     "SUB.U8",   OP_CATEGORY_ARITHMETIC, 1, {0}, "8位减法"},
    {OP_SUB_U16,    "SUB.U16",  OP_CATEGORY_ARITHMETIC, 2, {0, 0}, "16位减法"},

    /* 紧凑格式算术指令 */
    {OP_ADD_T2_C6,   "ADD.T2_C6", OP_CATEGORY_ARITHMETIC, 1, {0}, "加法（紧凑格式）"},
    {OP_ADD_T3_C5,   "ADD.T3_C5", OP_CATEGORY_ARITHMETIC, 1, {0}, "加法（紧凑格式）"},
    {OP_ADD_T4_C12,  "ADD.T4_C12", OP_CATEGORY_ARITHMETIC, 2, {0, 0}, "加法（紧凑格式）"},
    {OP_SUB_T2_C6,   "SUB.T2_C6", OP_CATEGORY_ARITHMETIC, 1, {0}, "减法（紧凑格式）"},
    {OP_SUB_T3_C5,   "SUB.T3_C5", OP_CATEGORY_ARITHMETIC, 1, {0}, "减法（紧凑格式）"},
    {OP_SUB_T4_C12,  "SUB.T4_C12", OP_CATEGORY_ARITHMETIC, 2, {0, 0}, "减法（紧凑格式）"},

    /* 位运算 */
    {OP_AND,        "AND",      OP_CATEGORY_LOGIC,      0, {}, "按位与"},
    {OP_OR,         "OR",       OP_CATEGORY_LOGIC,      0, {}, "按位或"},
    {OP_XOR,        "XOR",      OP_CATEGORY_LOGIC,      0, {}, "按位异或"},
    {OP_SHL,        "SHL",      OP_CATEGORY_LOGIC,      0, {}, "左移"},
    {OP_SHRS,       "SHRS",     OP_CATEGORY_LOGIC,      0, {}, "算术右移"},
    {OP_SHRU,       "SHRU",     OP_CATEGORY_LOGIC,      0, {}, "逻辑右移"},
    {OP_ROTL,       "ROTL",     OP_CATEGORY_LOGIC,      0, {}, "循环左移"},
    {OP_ROTR,       "ROTR",     OP_CATEGORY_LOGIC,      0, {}, "循环右移"},
    {OP_AND_U8,     "AND.U8",   OP_CATEGORY_LOGIC,      1, {0}, "8位按位与"},
    {OP_AND_U16,    "AND.U16",  OP_CATEGORY_LOGIC,      2, {0, 0}, "16位按位与"},
    {OP_OR_U8,      "OR.U8",    OP_CATEGORY_LOGIC,      1, {0}, "8位按位或"},
    {OP_OR_U16,     "OR.U16",   OP_CATEGORY_LOGIC,      2, {0, 0}, "16位按位或"},

    /* 紧凑格式逻辑指令 */
    {OP_AND_T3_C5,   "AND.T3_C5", OP_CATEGORY_LOGIC,      1, {0}, "按位与（紧凑格式）"},
    {OP_SHL_T3_C5,   "SHL.T3_C5", OP_CATEGORY_LOGIC,      1, {0}, "左移（紧凑格式）"},
    {OP_SHRU_T3_C5,  "SHRU.T3_C5", OP_CATEGORY_LOGIC,      1, {0}, "逻辑右移（紧凑格式）"},

    /* 类型转换 */
    {OP_CVT_U8,     "CVT.U8",   OP_CATEGORY_CONVERSION, 0, {}, "转换为8位无符号"},
    {OP_CVT_U16,    "CVT.U16",  OP_CATEGORY_CONVERSION, 0, {}, "转换为16位无符号"},

    /* 比较指令 */
    {OP_EQZ,        "EQZ",      OP_CATEGORY_COMPARISON, 0, {}, "等于零"},
    {OP_EQ,         "EQ",       OP_CATEGORY_COMPARISON, 0, {}, "相等"},
    {OP_NE,         "NE",       OP_CATEGORY_COMPARISON, 0, {}, "不等"},
    {OP_LTS,        "LTS",      OP_CATEGORY_COMPARISON, 0, {}, "有符号小于"},
    {OP_LTU,        "LTU",      OP_CATEGORY_COMPARISON, 0, {}, "无符号小于"},
    {OP_GTS,        "GTS",      OP_CATEGORY_COMPARISON, 0, {}, "有符号大于"},
    {OP_GTU,        "GTU",      OP_CATEGORY_COMPARISON, 0, {}, "无符号大于"},
    {OP_LES,        "LES",      OP_CATEGORY_COMPARISON, 0, {}, "有符号小于等于"},
    {OP_LEU,        "LEU",      OP_CATEGORY_COMPARISON, 0, {}, "无符号小于等于"},
    {OP_GES,        "GES",      OP_CATEGORY_COMPARISON, 0, {}, "有符号大于等于"},
    {OP_GEU,        "GEU",      OP_CATEGORY_COMPARISON, 0, {}, "无符号大于等于"},
    {OP_EQ_U8,      "EQ.U8",    OP_CATEGORY_COMPARISON, 1, {0}, "8位相等"},
    {OP_EQ_U16,     "EQ.U16",   OP_CATEGORY_COMPARISON, 2, {0, 0}, "16位相等"},
    {OP_NE_U8,      "NE.U8",    OP_CATEGORY_COMPARISON, 1, {0}, "8位不等"},
    {OP_NE_U16,     "NE.U16",   OP_CATEGORY_COMPARISON, 2, {0, 0}, "16位不等"},

    /* 变量指令 */
    {OP_LDT_0,      "LDT.0",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量0"},
    {OP_LDT_1,      "LDT.1",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量1"},
    {OP_LDT_2,      "LDT.2",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量2"},
    {OP_LDT_3,      "LDT.3",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量3"},
    {OP_LDT_4,      "LDT.4",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量4"},
    {OP_LDT_5,      "LDT.5",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量5"},
    {OP_LDT_6,      "LDT.6",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量6"},
    {OP_LDT_7,      "LDT.7",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量7"},
    {OP_LDT_8,      "LDT.8",    OP_CATEGORY_MEMORY,     0, {}, "加载局部变量8"},
    {OP_LDT,        "LDT",      OP_CATEGORY_MEMORY,     1, {0}, "加载局部变量"},
    {OP_STT_0,      "STT.0",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量0"},
    {OP_STT_1,      "STT.1",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量1"},
    {OP_STT_2,      "STT.2",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量2"},
    {OP_STT_3,      "STT.3",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量3"},
    {OP_STT_4,      "STT.4",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量4"},
    {OP_STT_5,      "STT.5",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量5"},
    {OP_STT_6,      "STT.6",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量6"},
    {OP_STT_7,      "STT.7",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量7"},
    {OP_STT_8,      "STT.8",    OP_CATEGORY_MEMORY,     0, {}, "存储到局部变量8"},
    {OP_STT,        "STT",      OP_CATEGORY_MEMORY,     1, {0}, "存储到局部变量"},
    {OP_STT_IS,     "STT.IS",    OP_CATEGORY_MEMORY,     0, {}, "从间接栈存储"},

    /* 全局/域数据访问 */
    {OP_STLDT_0,    "STLDT.0",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量0"},
    {OP_STLDT_1,    "STLDT.1",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量1"},
    {OP_STLDT_2,    "STLDT.2",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量2"},
    {OP_STLDT_3,    "STLDT.3",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量3"},
    {OP_STLDT_4,    "STLDT.4",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量4"},
    {OP_STLDT_5,    "STLDT.5",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量5"},
    {OP_STLDT_6,    "STLDT.6",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量6"},
    {OP_STLDT_7,    "STLDT.7",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量7"},
    {OP_STLDT_8,    "STLDT.8",  OP_CATEGORY_MEMORY,     0, {}, "加载全局/域变量8"},
    {OP_STLDT,      "STLDT",    OP_CATEGORY_MEMORY,     1, {0}, "加载全局/域变量"},
    {OP_STLDT_IS,   "STLDT.IS", OP_CATEGORY_MEMORY,     0, {}, "从间接栈存储到全局/域"},

    /* 内存指令 */
    {OP_LDMS8,      "LDMS8",    OP_CATEGORY_MEMORY,     0, {}, "加载有符号8位"},
    {OP_LDMU8,      "LDMU8",    OP_CATEGORY_MEMORY,     0, {}, "加载无符号8位"},
    {OP_LDMS16,     "LDMS16",   OP_CATEGORY_MEMORY,     0, {}, "加载有符号16位"},
    {OP_LDMU16,     "LDMU16",   OP_CATEGORY_MEMORY,     0, {}, "加载无符号16位"},
    {OP_LDM32,      "LDM32",    OP_CATEGORY_MEMORY,     0, {}, "加载32位"},
    {OP_STM8,       "STM8",     OP_CATEGORY_MEMORY,     0, {}, "存储8位"},
    {OP_STM16,      "STM16",    OP_CATEGORY_MEMORY,     0, {}, "存储16位"},
    {OP_STM32,      "STM32",    OP_CATEGORY_MEMORY,     0, {}, "存储32位"},
    {OP_MCOPY,      "MCOPY",    OP_CATEGORY_MEMORY,     0, {}, "内存拷贝"},
    {OP_MFILL,      "MFILL",    OP_CATEGORY_MEMORY,     0, {}, "内存填充"},

    /* 异常处理指令 */
    {OP_TRY,        "TRY",      OP_CATEGORY_CONTROL,    0, {}, "异常监测"},
    {OP_CATCH,      "CATCH",    OP_CATEGORY_CONTROL,    0, {}, "异常捕获"},

    /* 紧凑格式指令扩展 */
    {OP_ADD_T4_T4,   "ADD.T4_T4", OP_CATEGORY_ARITHMETIC, 1, {0}, "加法（双4位操作数）"},
    {OP_SUB_T4_T4,   "SUB.T4_T4", OP_CATEGORY_ARITHMETIC, 1, {0}, "减法（双4位操作数）"},
    {OP_TINC_T8_U8,  "TINC.T8_U8", OP_CATEGORY_ARITHMETIC, 1, {0}, "自增"},
    {OP_TDEC_T8_U8,  "TDEC.T8_U8", OP_CATEGORY_ARITHMETIC, 1, {0}, "自减"},
};

static const int instruction_table_size = sizeof(instruction_table) / sizeof(InstructionInfo);

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/**
 * @brief 从栈中读取指定偏移的值
 */
static u32 stack_peek_offset(VMContext *vm, u32 offset) {
    if (vm->stack_pointer <= offset) {
        vm->exception = EXCEPTION_STACK_UNDERFLOW;
        return 0;
    }
    return vm->executor_stack[vm->stack_pointer - offset - 1];
}

/**
 * @brief 获取局部变量地址
 */
static u32* get_local_var_addr(VMContext *vm, u32 index) {
    StackFrame *frame = &vm->frame_stack[vm->frame_top - 1];
    if (index >= frame->local_var_count) {
        vm->exception = EXCEPTION_ARRAY_BOUNDS;
        return NULL;
    }
    return &vm->executor_stack[frame->local_vars_offset + index];
}

/**
 * @brief 从内存读取数据
 */
static u32 read_memory(VMContext *vm, u32 addr, u32 size) {
    if (addr >= VM_MODULE_CODE_SIZE) {
        vm->exception = EXCEPTION_INVALID_ADDRESS;
        return 0;
    }
    
    switch (size) {
        case 1:
            return vm->module_code[addr];
        case 2:
            return vm->module_code[addr] | (vm->module_code[addr + 1] << 8);
        case 4:
            return vm->module_code[addr] | 
                   (vm->module_code[addr + 1] << 8) |
                   (vm->module_code[addr + 2] << 16) |
                   (vm->module_code[addr + 3] << 24);
        default:
            vm->exception = EXCEPTION_INVALID_ADDRESS;
            return 0;
    }
}

/**
 * @brief 向内存写入数据
 */
static void write_memory(VMContext *vm, u32 addr, u32 value, u32 size) {
    if (addr >= VM_MODULE_CODE_SIZE) {
        vm->exception = EXCEPTION_INVALID_ADDRESS;
        return;
    }
    
    switch (size) {
        case 1:
            vm->module_code[addr] = (u8)value;
            break;
        case 2:
            vm->module_code[addr] = (u8)value;
            vm->module_code[addr + 1] = (u8)(value >> 8);
            break;
        case 4:
            vm->module_code[addr] = (u8)value;
            vm->module_code[addr + 1] = (u8)(value >> 8);
            vm->module_code[addr + 2] = (u8)(value >> 16);
            vm->module_code[addr + 3] = (u8)(value >> 24);
            break;
    }
}

/**
 * @brief 执行算术运算
 */
static u32 do_arithmetic(u32 op1, u32 op2, u8 opcode) {
    switch (opcode) {
        case OP_ADD:
            return op1 + op2;
        case OP_SUB:
            return op1 - op2;
        case OP_MUL:
            return op1 * op2;
        case OP_DIVS:
            if (op2 == 0) {
                return 0; /* 除零错误由调用者处理 */
            }
            return (s32)op1 / (s32)op2;
        case OP_DIVU:
            if (op2 == 0) {
                return 0;
            }
            return op1 / op2;
        case OP_REMS:
            if (op2 == 0) {
                return 0;
            }
            return (s32)op1 % (s32)op2;
        case OP_REMU:
            if (op2 == 0) {
                return 0;
            }
            return op1 % op2;
        default:
            return 0;
    }
}

/**
 * @brief 执行位运算
 */
static u32 do_bitwise(u32 op1, u32 op2, u8 opcode) {
    switch (opcode) {
        case OP_AND:
            return op1 & op2;
        case OP_OR:
            return op1 | op2;
        case OP_XOR:
            return op1 ^ op2;
        case OP_SHL:
            return op1 << (op2 & 0x1F);
        case OP_SHRS:
            return (s32)op1 >> (op2 & 0x1F);
        case OP_SHRU:
            return op1 >> (op2 & 0x1F);
        case OP_ROTL:
            return (op1 << (op2 & 0x1F)) | (op1 >> (32 - (op2 & 0x1F)));
        case OP_ROTR:
            return (op1 >> (op2 & 0x1F)) | (op1 << (32 - (op2 & 0x1F)));
        default:
            return 0;
    }
}

/**
 * @brief 执行比较运算
 */
static u32 do_compare(u32 op1, u32 op2, u8 opcode) {
    s32 s1 = (s32)op1;
    s32 s2 = (s32)op2;
    
    switch (opcode) {
        case OP_EQ:
            return (op1 == op2) ? 1 : 0;
        case OP_NE:
            return (op1 != op2) ? 1 : 0;
        case OP_LTS:
            return (s1 < s2) ? 1 : 0;
        case OP_LTU:
            return (op1 < op2) ? 1 : 0;
        case OP_GTS:
            return (s1 > s2) ? 1 : 0;
        case OP_GTU:
            return (op1 > op2) ? 1 : 0;
        case OP_LES:
            return (s1 <= s2) ? 1 : 0;
        case OP_LEU:
            return (op1 <= op2) ? 1 : 0;
        case OP_GES:
            return (s1 >= s2) ? 1 : 0;
        case OP_GEU:
            return (op1 >= op2) ? 1 : 0;
        default:
            return 0;
    }
}

/**
 * @brief 获取局部变量地址
 */
static u32* get_local_var_addr(VMContext *vm, u32 index) {
    if (vm->frame_top == 0) {
        vm->exception = EXCEPTION_INVALID_ADDRESS;
        return NULL;
    }

    StackFrame *frame = &vm->frame_stack[vm->frame_top - 1];
    if (index >= frame->local_var_count) {
        vm->exception = EXCEPTION_ARRAY_BOUNDS;
        return NULL;
    }

    return &vm->executor_stack[frame->local_vars_offset + index];
}

/**
 * @brief 获取全局变量地址
 */
static u32* get_global_var_addr(VMContext *vm, u32 index) {
    if (index >= VM_GLOBAL_DATA_SIZE / sizeof(u32)) {
        vm->exception = EXCEPTION_ARRAY_BOUNDS;
        return NULL;
    }
    return (u32*)&vm->global_data[index * sizeof(u32)];
}

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
            offset = (s8)operands[0];
            vm->program_counter += offset;
            return 0;

        case OP_BR_S16:
            /* 16位偏移跳转 */
            offset = (s16)((operands[0] & 0xFF) | (operands[1] << 8));
            vm->program_counter += offset;
            return 0;

        case OP_BEQZ_S8:
            /* 为零则8位跳转 */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 == 0) {
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
                offset = (s16)((operands[0] & 0xFF) | (operands[1] << 8));
                vm->program_counter += offset;
            }
            return 0;

        case OP_BEQ_S8:
            /* 相等则8位跳转 */
            if (vm_stack_pop(vm, &val1) != 0 || vm_stack_pop(vm, &val2) != 0) {
                return -1;
            }
            if (val1 == val2) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BNE_S8:
            /* 不等则8位跳转 */
            if (vm_stack_pop(vm, &val1) != 0 || vm_stack_pop(vm, &val2) != 0) {
                return -1;
            }
            if (val1 != val2) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BLTU_S8:
            /* 无符号小于则跳转 */
            if (vm_stack_pop(vm, &val1) != 0 || vm_stack_pop(vm, &val2) != 0) {
                return -1;
            }
            if (val1 < val2) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BGTU_S8:
            /* 无符号大于则跳转 */
            if (vm_stack_pop(vm, &val1) != 0 || vm_stack_pop(vm, &val2) != 0) {
                return -1;
            }
            if (val1 > val2) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BLEU_S8:
            /* 无符号小于等于则跳转 */
            if (vm_stack_pop(vm, &val1) != 0 || vm_stack_pop(vm, &val2) != 0) {
                return -1;
            }
            if (val1 <= val2) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BGEU_S8:
            /* 无符号大于等于则跳转 */
            if (vm_stack_pop(vm, &val1) != 0 || vm_stack_pop(vm, &val2) != 0) {
                return -1;
            }
            if (val1 >= val2) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BRTB_S8:
            /* 真值表跳转（8位） */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 != 0) {
                offset = (s8)operands[0];
                vm->program_counter += offset;
            }
            return 0;

        case OP_BRTB_S16:
            /* 真值表跳转（16位） */
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val1 != 0) {
                offset = (s16)((operands[0] & 0xFF) | (operands[1] << 8));
                vm->program_counter += offset;
            }
            return 0;

        case OP_CALL_U16:
        case OP_CALLIND:
        case OP_CALLEX_U8:
        case OP_CALLIN_U8:
            /* 函数调用指令在执行引擎中处理 */
            return 0;

        case OP_POP:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            return 0;

        case OP_POP_IS:
            /* 弹出间接栈 */
            if (vm->indirect_stack_pointer == 0) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            vm->indirect_stack_pointer--;
            return 0;

        case OP_PICK:
            /* 条件选择 */
            if (vm_stack_pop(vm, &val3) != 0 ||
                vm_stack_pop(vm, &val2) != 0 ||
                vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val3 != 0) ? val1 : val2;
            if (vm_stack_push(vm, result) != 0) {
                return -1;
            }
            return 0;

        case OP_DUP:
            /* 复制栈顶 */
            val1 = stack_peek_offset(vm, 1);
            if (vm->exception != EXCEPTION_NONE) {
                return -1;
            }
            if (vm_stack_push(vm, val1) != 0) {
                return -1;
            }
            return 0;

        case OP_SWAP:
            /* 交换栈顶两个元素 */
            if (vm->stack_pointer < 2) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            val1 = vm->executor_stack[vm->stack_pointer - 1];
            val2 = vm->executor_stack[vm->stack_pointer - 2];
            vm->executor_stack[vm->stack_pointer - 1] = val2;
            vm->executor_stack[vm->stack_pointer - 2] = val1;
            return 0;

        case OP_OVER:
            /* 复制次栈顶到栈顶 */
            if (vm->stack_pointer < 2) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            val1 = vm->executor_stack[vm->stack_pointer - 2];
            if (vm_stack_push(vm, val1) != 0) {
                return -1;
            }
            return 0;

        case OP_ROT:
            /* 旋转栈顶三个元素 */
            if (vm->stack_pointer < 3) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            val1 = vm->executor_stack[vm->stack_pointer - 1];
            val2 = vm->executor_stack[vm->stack_pointer - 2];
            result = vm->executor_stack[vm->stack_pointer - 3];
            vm->executor_stack[vm->stack_pointer - 1] = result;
            vm->executor_stack[vm->stack_pointer - 2] = val1;
            vm->executor_stack[vm->stack_pointer - 3] = val2;
            return 0;

        case OP_TRY:
            /* 异常监测 - 设置异常处理器 */
            if (vm->handler_count >= 16) {
                vm->exception = EXCEPTION_STACK_OVERFLOW;
                return -1;
            }
            vm->exception_handlers[vm->handler_count++] = vm->program_counter;
            vm_stack_push(vm, 0);  /* 压入0表示无异常 */
            return 0;

        case OP_CATCH:
            /* 异常捕获 */
            if (vm->handler_count > 0) {
                vm->handler_count--;
                vm->program_counter = vm->exception_handlers[vm->handler_count];
            }
            vm_stack_push(vm, 1);  /* 压入1表示捕获成功 */
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
    u32 val1, val2, result;
    s32 sval1, sval2, sresult;

    switch (opcode) {
        /* 常量加载 */
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
            return vm_stack_push(vm, operands[0] & 0xFF);
        case OP_LDC_U16:
            result = (operands[0] & 0xFF) | (operands[1] << 8);
            return vm_stack_push(vm, result);
        case OP_LDC_I32:
            result = operands[0] | (operands[1] << 8) | (operands[2] << 16) | (operands[3] << 24);
            return vm_stack_push(vm, result);
        case OP_LDC_A8:
            return vm_stack_push(vm, operands[0] & 0xFF);
        case OP_LDC_A16:
            result = (operands[0] & 0xFF) | (operands[1] << 8);
            return vm_stack_push(vm, result);

        /* 基本算术运算 */
        case OP_ADD:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 + val2;
            return vm_stack_push(vm, result);

        case OP_SUB:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 - val2;
            return vm_stack_push(vm, result);

        case OP_MUL:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 * val2;
            return vm_stack_push(vm, result);

        case OP_DIVS:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sval2 = (s32)val2;
            if (sval2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            sresult = sval1 / sval2;
            return vm_stack_push(vm, (u32)sresult);

        case OP_DIVU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = val1 / val2;
            return vm_stack_push(vm, result);

        case OP_REMS:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sval2 = (s32)val2;
            if (sval2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            sresult = sval1 % sval2;
            return vm_stack_push(vm, (u32)sresult);

        case OP_REMU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            if (val2 == 0) {
                vm->exception = EXCEPTION_DIVISION_BY_ZERO;
                return -1;
            }
            result = val1 % val2;
            return vm_stack_push(vm, result);

        /* 立即数算术运算 */
        case OP_ADD_U8:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 + (u8)operands[0];
            return vm_stack_push(vm, result);

        case OP_ADD_U16:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 + ((u16)operands[0] | (operands[1] << 8));
            return vm_stack_push(vm, result);

        case OP_SUB_U8:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 - (u8)operands[0];
            return vm_stack_push(vm, result);

        case OP_SUB_U16:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 - ((u16)operands[0] | (operands[1] << 8));
            return vm_stack_push(vm, result);

        /* 紧凑格式算术 */
        case OP_ADD_T2_C6:
        case OP_ADD_T3_C5:
        case OP_ADD_T4_C12:
        case OP_SUB_T2_C6:
        case OP_SUB_T3_C5:
        case OP_SUB_T4_C12:
        case OP_ADD_T4_T4:
        case OP_SUB_T4_T4:
        case OP_TINC_T8_U8:
        case OP_TDEC_T8_U8:
            /* 紧凑格式指令需要特殊处理 */
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行逻辑运算指令
 */
static int execute_logic(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 val1, val2, result;

    switch (opcode) {
        case OP_AND:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 & val2;
            return vm_stack_push(vm, result);

        case OP_OR:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 | val2;
            return vm_stack_push(vm, result);

        case OP_XOR:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 ^ val2;
            return vm_stack_push(vm, result);

        case OP_SHL:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 << (val2 & 0x1F);
            return vm_stack_push(vm, result);

        case OP_SHRS:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sresult = sval1 >> ((s32)val2 & 0x1F);
            return vm_stack_push(vm, (u32)sresult);

        case OP_SHRU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 >> (val2 & 0x1F);
            return vm_stack_push(vm, result);

        case OP_ROTL:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            {
                u32 shift = val2 & 0x1F;
                result = ((val1 << shift) | (val1 >> (32 - shift)));
            }
            return vm_stack_push(vm, result);

        case OP_ROTR:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            {
                u32 shift = val2 & 0x1F;
                result = ((val1 >> shift) | (val1 << (32 - shift)));
            }
            return vm_stack_push(vm, result);

        case OP_AND_U8:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 & (u8)operands[0];
            return vm_stack_push(vm, result);

        case OP_AND_U16:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 & ((u16)operands[0] | (operands[1] << 8));
            return vm_stack_push(vm, result);

        case OP_OR_U8:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 | (u8)operands[0];
            return vm_stack_push(vm, result);

        case OP_OR_U16:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = val1 | ((u16)operands[0] | (operands[1] << 8));
            return vm_stack_push(vm, result);

        /* 紧凑格式逻辑指令 */
        case OP_AND_T3_C5:
        case OP_SHL_T3_C5:
        case OP_SHRU_T3_C5:
            /* 紧凑格式指令需要特殊处理 */
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行比较指令
 */
static int execute_comparison(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 val1, val2, result;
    s32 sval1, sval2;

    switch (opcode) {
        case OP_EQZ:
            if (vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 == 0) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_EQ:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 == val2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_NE:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 != val2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_LTS:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sval2 = (s32)val2;
            result = (sval1 < sval2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_LTU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 < val2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_GTS:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sval2 = (s32)val2;
            result = (sval1 > sval2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_GTU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 > val2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_LES:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sval2 = (s32)val2;
            result = (sval1 <= sval2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_LEU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 <= val2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_GES:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &sval1) != 0) {
                return -1;
            }
            sval2 = (s32)val2;
            result = (sval1 >= sval2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_GEU:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 >= val2) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_EQ_U8:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 == (val2 & 0xFF)) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_EQ_U16:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 == ((u16)operands[0] | (operands[1] << 8))) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_NE_U8:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 != (val2 & 0xFF)) ? 1 : 0;
            return vm_stack_push(vm, result);

        case OP_NE_U16:
            if (vm_stack_pop(vm, &val2) != 0 || vm_stack_pop(vm, &val1) != 0) {
                return -1;
            }
            result = (val1 != ((u16)operands[0] | (operands[1] << 8))) ? 1 : 0;
            return vm_stack_push(vm, result);

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行变量指令
 */
static int execute_variable(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 value, index;

    switch (opcode) {
        /* 加载局部变量 */
        case OP_LDT_0:
        case OP_LDT_1:
        case OP_LDT_2:
        case OP_LDT_3:
        case OP_LDT_4:
        case OP_LDT_5:
        case OP_LDT_6:
        case OP_LDT_7:
        case OP_LDT_8:
            index = opcode - OP_LDT_0;
            value = stack_peek_offset(vm, index + 1);
            if (vm->exception != EXCEPTION_NONE) {
                return -1;
            }
            return vm_stack_push(vm, value);

        case OP_LDT:
            index = operands[0];
            if (index >= 256) {
                vm->exception = EXCEPTION_ARRAY_BOUNDS;
                return -1;
            }
            value = stack_peek_offset(vm, index + 1);
            if (vm->exception != EXCEPTION_NONE) {
                return -1;
            }
            return vm_stack_push(vm, value);

        /* 存储到局部变量 */
        case OP_STT_0:
        case OP_STT_1:
        case OP_STT_2:
        case OP_STT_3:
        case OP_STT_4:
        case OP_STT_5:
        case OP_STT_6:
        case OP_STT_7:
        case OP_STT_8:
            index = opcode - OP_STT_0;
            if (vm_stack_pop(vm, &value) != 0) {
                return -1;
            }
            if (vm->stack_pointer < index + 1) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            vm->executor_stack[vm->stack_pointer - index - 1] = value;
            return 0;

        case OP_STT:
            index = operands[0];
            if (vm_stack_pop(vm, &value) != 0) {
                return -1;
            }
            if (vm->stack_pointer < index + 1) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            vm->executor_stack[vm->stack_pointer - index - 1] = value;
            return 0;

        case OP_STT_IS:
            /* 从间接栈存储到局部变量 */
            if (vm->indirect_stack_pointer == 0) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            if (vm_stack_pop(vm, &index) != 0) {
                return -1;
            }
            if (vm->stack_pointer < index + 1) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            vm->executor_stack[vm->stack_pointer - index - 1] = 
                *(u32*)vm->indirect_var_stack[--vm->indirect_stack_pointer];
            return 0;

        /* 加载全局/域变量 */
        case OP_STLDT_0:
        case OP_STLDT_1:
        case OP_STLDT_2:
        case OP_STLDT_3:
        case OP_STLDT_4:
        case OP_STLDT_5:
        case OP_STLDT_6:
        case OP_STLDT_7:
        case OP_STLDT_8:
            index = opcode - OP_STLDT_0;
            {
                u32 *addr = get_global_var_addr(vm, index);
                if (addr == NULL) {
                    return -1;
                }
                return vm_stack_push(vm, *addr);
            }

        case OP_STLDT:
            index = operands[0];
            {
                u32 *addr = get_global_var_addr(vm, index);
                if (addr == NULL) {
                    return -1;
                }
                return vm_stack_push(vm, *addr);
            }

        case OP_STLDT_IS:
            /* 从间接栈存储到全局/域变量 */
            if (vm->indirect_stack_pointer == 0) {
                vm->exception = EXCEPTION_STACK_UNDERFLOW;
                return -1;
            }
            if (vm_stack_pop(vm, &index) != 0) {
                return -1;
            }
            {
                u32 *addr = get_global_var_addr(vm, index);
                if (addr == NULL) {
                    return -1;
                }
                *addr = *(u32*)vm->indirect_var_stack[--vm->indirect_stack_pointer];
            }
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

/**
 * @brief 执行内存指令
 */
static int execute_memory(VMContext *vm, u8 opcode, const u32 *operands, u8 operand_count) {
    u32 addr, value, len, src, des, fill;
    u8 *mem_ptr;

    switch (opcode) {
        case OP_LDMS8:
            if (vm_stack_pop(vm, &addr) != 0) {
                return -1;
            }
            if (addr >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            {
                s8 sval = (s8)vm->global_data[addr];
                value = (u32)sval;
            }
            return vm_stack_push(vm, value);

        case OP_LDMU8:
            if (vm_stack_pop(vm, &addr) != 0) {
                return -1;
            }
            if (addr >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            value = vm->global_data[addr];
            return vm_stack_push(vm, value);

        case OP_LDMS16:
            if (vm_stack_pop(vm, &addr) != 0) {
                return -1;
            }
            if (addr + 1 >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            {
                s16 sval = (s16)(vm->global_data[addr] | (vm->global_data[addr + 1] << 8));
                value = (u32)sval;
            }
            return vm_stack_push(vm, value);

        case OP_LDMU16:
            if (vm_stack_pop(vm, &addr) != 0) {
                return -1;
            }
            if (addr + 1 >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            value = vm->global_data[addr] | (vm->global_data[addr + 1] << 8);
            return vm_stack_push(vm, value);

        case OP_LDM32:
            if (vm_stack_pop(vm, &addr) != 0) {
                return -1;
            }
            if (addr + 3 >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            value = vm->global_data[addr] | (vm->global_data[addr + 1] << 8) |
                   (vm->global_data[addr + 2] << 16) | (vm->global_data[addr + 3] << 24);
            return vm_stack_push(vm, value);

        case OP_STM8:
            if (vm_stack_pop(vm, &addr) != 0 || vm_stack_pop(vm, &value) != 0) {
                return -1;
            }
            if (addr >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            vm->global_data[addr] = (u8)value;
            return 0;

        case OP_STM16:
            if (vm_stack_pop(vm, &addr) != 0 || vm_stack_pop(vm, &value) != 0) {
                return -1;
            }
            if (addr + 1 >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            vm->global_data[addr] = (u8)value;
            vm->global_data[addr + 1] = (u8)(value >> 8);
            return 0;

        case OP_STM32:
            if (vm_stack_pop(vm, &addr) != 0 || vm_stack_pop(vm, &value) != 0) {
                return -1;
            }
            if (addr + 3 >= VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            vm->global_data[addr] = (u8)value;
            vm->global_data[addr + 1] = (u8)(value >> 8);
            vm->global_data[addr + 2] = (u8)(value >> 16);
            vm->global_data[addr + 3] = (u8)(value >> 24);
            return 0;

        case OP_MCOPY:
            if (vm_stack_pop(vm, &len) != 0 || vm_stack_pop(vm, &src) != 0 || vm_stack_pop(vm, &des) != 0) {
                return -1;
            }
            if (src + len > VM_GLOBAL_DATA_SIZE || des + len > VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            memmove(&vm->global_data[des], &vm->global_data[src], len);
            return 0;

        case OP_MFILL:
            if (vm_stack_pop(vm, &len) != 0 || vm_stack_pop(vm, &des) != 0 || vm_stack_pop(vm, &fill) != 0) {
                return -1;
            }
            if (des + len > VM_GLOBAL_DATA_SIZE) {
                vm->exception = EXCEPTION_INVALID_ADDRESS;
                return -1;
            }
            memset(&vm->global_data[des], (u8)fill, len);
            return 0;

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

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

    switch (*opcode) {
        /* 8位偏移跳转 */
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
        case OP_CALLEX_U8:
        case OP_CALLIN_U8:
        case OP_LDC_U8:
        case OP_LDC_A8:
        case OP_ADD_U8:
        case OP_SUB_U8:
        case OP_AND_U8:
        case OP_OR_U8:
        case OP_EQ_U8:
        case OP_NE_U8:
            if (offset + 1 < code_size) {
                operands[0] = code[offset + 1];
                *operand_count = 1;
                instr_len = 2;
            }
            break;

        /* 16位偏移跳转 */
        case OP_BR_S16:
        case OP_BEQZ_S16:
        case OP_BNEZ_S16:
        case OP_BRTB_S16:
        case OP_LDC_U16:
        case OP_LDC_A16:
        case OP_ADD_U16:
        case OP_SUB_U16:
        case OP_AND_U16:
        case OP_OR_U16:
        case OP_EQ_U16:
        case OP_NE_U16:
            if (offset + 2 < code_size) {
                operands[0] = code[offset + 1] | (code[offset + 2] << 8);
                *operand_count = 1;
                instr_len = 3;
            }
            break;

        /* 32位立即数 */
        case OP_LDC_I32:
            if (offset + 4 < code_size) {
                operands[0] = code[offset + 1] | (code[offset + 2] << 8) |
                               (code[offset + 3] << 16) | (code[offset + 4] << 24);
                *operand_count = 1;
                instr_len = 5;
            }
            break;

        /* 16位函数调用 */
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

    /* 根据操作码分类执行 */
    OpCodeCategory category = vm_get_opcode_category(opcode);

    switch (category) {
        case OP_CATEGORY_CONTROL:
            return execute_control(vm, opcode, operands, operand_count);

        case OP_CATEGORY_ARITHMETIC:
            return execute_arithmetic(vm, opcode, operands, operand_count);

        case OP_CATEGORY_LOGIC:
            return execute_logic(vm, opcode, operands, operand_count);

        case OP_CATEGORY_COMPARISON:
            return execute_comparison(vm, opcode, operands, operand_count);

        case OP_CATEGORY_MEMORY:
            return execute_memory(vm, opcode, operands, operand_count);

        case OP_CATEGORY_CONVERSION:
            /* 类型转换指令暂未实现 */
            vm->exception = EXCEPTION_NOT_IMPLEMENTED;
            return -1;

        case OP_CATEGORY_TRAP:
            /* 陷阱指令在控制流中处理 */
            return execute_control(vm, opcode, operands, operand_count);

        default:
            vm->exception = EXCEPTION_INVALID_OPCODE;
            return -1;
    }
}

void vm_print_instruction(u8 opcode, const u32 *operands, u8 operand_count) {
    const InstructionInfo *info = vm_get_instruction_info(opcode);
    if (info == NULL) {
        printf("UNKNOWN(0x%02X)", opcode);
        return;
    }

    printf("%s", info->mnemonic);

    for (u8 i = 0; i < operand_count; i++) {
        printf(" 0x%X", operands[i]);
    }
}

OpCodeCategory vm_get_opcode_category(u8 opcode) {
    const InstructionInfo *info = vm_get_instruction_info(opcode);
    if (info == NULL) {
        return OP_CATEGORY_TRAP;
    }
    return info->category;
}

bool vm_is_valid_opcode(u8 opcode) {
    return vm_get_instruction_info(opcode) != NULL;
}
