/**
 * @file gcos_instructions.h
 * @brief GCOS VM 指令集定义 - 基于COS3规范附录A
 * 
 * 指令集分类:
 * - 控制指令 (Control): 跳转、调用、返回
 * - 数值指令 (Numeric): 常量、算术、比较、位运算
 * - 变量指令 (Variable): 局部变量读写
 * - 内存指令 (Memory): 加载、存储
 * - 异常处理指令 (Exception): 陷阱、捕获
 * - 复合指令 (Compound): 复合功能
 */

#ifndef GCOS_INSTRUCTIONS_H
#define GCOS_INSTRUCTIONS_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 操作码定义 - 基于COS3规范附录A
 * ============================================================================ */

/* --- 控制指令 (0x00-0x1F) --- */
#define OP_TRAP             0x00    /* trap - 陷阱指令 */
#define OP_NOP              0x01    /* nop - 空操作 */
#define OP_POP              0x02    /* pop - 弹栈 */
#define OP_POP_IS           0x03    /* pop.is - 回收间接栈 */

#define OP_BR_S8            0x04    /* br.s8 - 无条件跳转(8位偏移) */
#define OP_BR_S16           0x05    /* br.s16 - 无条件跳转(16位偏移) */

#define OP_BEQZ_S8          0x06    /* beqz.s8 - 为零则跳转(8位) */
#define OP_BEQZ_S16         0x07    /* beqz.s16 - 为零则跳转(16位) */
#define OP_BNEZ_S8          0x08    /* bnez.s8 - 非零则跳转(8位) */
#define OP_BNEZ_S16         0x09    /* bnez.s16 - 非零则跳转(16位) */

#define OP_RET              0x0A    /* ret - 函数返回 */
#define OP_IRET             0x0B    /* iret - 中断返回 */
#define OP_CALL             0x0C    /* call - 函数调用 */
#define OP_CALL_INDIRECT    0x0D    /* call.indirect - 间接调用 */

/* --- 数值指令 - 常量 (0x18-0x22) --- */
#define OP_LDC_0            0x18    /* ldc.0 - 加载常量0 */
#define OP_LDC_1            0x19    /* ldc.1 - 加载常量1 */
#define OP_LDC_2            0x1A    /* ldc.2 - 加载常量2 */
#define OP_LDC_3            0x1B    /* ldc.3 - 加载常量3 */
#define OP_LDC_4            0x1C    /* ldc.4 - 加载常量4 */
#define OP_LDC_5            0x1D    /* ldc.5 - 加载常量5 */
#define OP_LDC_6            0x1E    /* ldc.6 - 加载常量6 */
#define OP_LDC_7            0x1F    /* ldc.7 - 加载常量7 */
#define OP_LDC_8            0x20    /* ldc.8 - 加载常量8 */
#define OP_LDC_I32          0x21    /* ldc.i32 - 加载32位整数常量 */
#define OP_LDC_M1           0x22    /* ldc.m1 - 加载常量-1 */

/* --- 数值指令 - 算术运算 (0x27-0x35) --- */
#define OP_ADD              0x27    /* add - 加法 */
#define OP_SUB              0x28    /* sub - 减法 */
#define OP_MUL              0x29    /* mul - 乘法 */
#define OP_DIVS             0x2A    /* divs - 有符号除法 */
#define OP_DIVU             0x2B    /* divu - 无符号除法 */
#define OP_REMS             0x2C    /* rems - 有符号取模 */
#define OP_REMU             0x2D    /* remu - 无符号取模 */

/* --- 数值指令 - 位运算 (0x2E-0x35) --- */
#define OP_AND              0x2E    /* and - 按位与 */
#define OP_OR               0x2F    /* or - 按位或 */
#define OP_XOR              0x30    /* xor - 按位异或 */
#define OP_SHL              0x31    /* shl - 左移 */
#define OP_SHRS             0x32    /* shrs - 算术右移 */
#define OP_SHRU             0x33    /* shru - 逻辑右移 */
#define OP_ROTL             0x34    /* rotl - 循环左移 */
#define OP_ROTR             0x35    /* rotr - 循环右移 */

/* --- 数值指令 - 比较 (0x38-0x42) --- */
#define OP_EQZ              0x38    /* eqz - 等于零 */
#define OP_EQ               0x39    /* eq - 相等 */
#define OP_NE               0x3A    /* ne - 不相等 */
#define OP_LTS              0x3B    /* lts - 有符号小于 */
#define OP_LTU              0x3C    /* ltu - 无符号小于 */
#define OP_GTS              0x3D    /* gts - 有符号大于 */
#define OP_GTU              0x3E    /* gtu - 无符号大于 */
#define OP_LES              0x3F    /* les - 有符号小于等于 */
#define OP_LEU              0x40    /* leu - 无符号小于等于 */
#define OP_GES              0x41    /* ges - 有符号大于等于 */
#define OP_GEU              0x42    /* geu - 无符号大于等于 */

/* --- 变量指令 (0x43-0x61) --- */
#define OP_LDT_0            0x43    /* ldt.0 - 加载局部变量0 */
#define OP_LDT_1            0x44    /* ldt.1 - 加载局部变量1 */
#define OP_LDT_2            0x45    /* ldt.2 - 加载局部变量2 */
#define OP_LDT_3            0x46    /* ldt.3 - 加载局部变量3 */
#define OP_LDT_4            0x47    /* ldt.4 - 加载局部变量4 */
#define OP_LDT_5            0x48    /* ldt.5 - 加载局部变量5 */
#define OP_LDT_6            0x49    /* ldt.6 - 加载局部变量6 */
#define OP_LDT_7            0x4A    /* ldt.7 - 加载局部变量7 */
#define OP_LDT_8            0x4B    /* ldt.8 - 加载局部变量8 */
#define OP_LDT              0x4C    /* ldt - 加载局部变量 */
#define OP_STT_0            0x4D    /* stt.0 - 存储到局部变量0 */
#define OP_STT_1            0x4E    /* stt.1 - 存储到局部变量1 */
#define OP_STT_2            0x4F    /* stt.2 - 存储到局部变量2 */
#define OP_STT_3            0x50    /* stt.3 - 存储到局部变量3 */
#define OP_STT_4            0x51    /* stt.4 - 存储到局部变量4 */
#define OP_STT_5            0x52    /* stt.5 - 存储到局部变量5 */
#define OP_STT_6            0x53    /* stt.6 - 存储到局部变量6 */
#define OP_STT_7            0x54    /* stt.7 - 存储到局部变量7 */
#define OP_STT_8            0x55    /* stt.8 - 存储到局部变量8 */
#define OP_STT              0x56    /* stt - 存储到局部变量 */
#define OP_STLDT_0          0x58    /* stldt.0 - 拷贝到局部变量0 */
#define OP_STLDT_1          0x59    /* stldt.1 - 拷贝到局部变量1 */
#define OP_STLDT_2          0x5A    /* stldt.2 - 拷贝到局部变量2 */
#define OP_STLDT_3          0x5B    /* stldt.3 - 拷贝到局部变量3 */
#define OP_STLDT_4          0x5C    /* stldt.4 - 拷贝到局部变量4 */
#define OP_STLDT_5          0x5D    /* stldt.5 - 拷贝到局部变量5 */
#define OP_STLDT_6          0x5E    /* stldt.6 - 拷贝到局部变量6 */
#define OP_STLDT_7          0x5F    /* stldt.7 - 拷贝到局部变量7 */
#define OP_STLDT_8          0x60    /* stldt.8 - 拷贝到局部变量8 */
#define OP_STLDT            0x61    /* stldt - 拷贝到局部变量 */

/* --- 内存指令 (0x63-0x6C) --- */
#define OP_LDMS8            0x63    /* ldms8 - 加载有符号8位 */
#define OP_LDMU8            0x64    /* ldmu8 - 加载无符号8位 */
#define OP_LDMS16           0x65    /* ldms16 - 加载有符号16位 */
#define OP_LDMU16           0x66    /* ldmu16 - 加载无符号16位 */
#define OP_LDM32            0x67    /* ldm32 - 加载32位 */
#define OP_STM8             0x68    /* stm8 - 存储8位 */
#define OP_STM16            0x69    /* stm16 - 存储16位 */
#define OP_STM32            0x6A    /* stm32 - 存储32位 */
#define OP_MCOPY            0x6B    /* mcopy - 内存拷贝 */
#define OP_MFILL            0x6C    /* mfill - 内存填充 */

/* --- 异常处理指令 (0x6D-0x6E) --- */
#define OP_TRY              0x6D    /* try - 异常监测 */
#define OP_CATCH            0x6E    /* catch - 异常捕获 */

/* --- 复合指令 (0x6F-0x7D) --- */
#define OP_PICK             0x6F    /* pick - 条件选择 */
#define OP_DUP              0x70    /* dup - 复制栈顶 */
#define OP_SWAP             0x71    /* swap - 交换栈顶两个元素 */
#define OP_OVER             0x72    /* over - 复制次栈顶到栈顶 */
#define OP_ROT              0x73    /* rot - 旋转栈顶三个元素 */
#define OP_ADD_U8           0x76    /* add.u8 - 与8位立即数加 */
#define OP_ADD_U16          0x77    /* add.u16 - 与16位立即数加 */
#define OP_SUB_U8           0x78    /* sub.u8 - 与8位立即数减 */
#define OP_SUB_U16          0x79    /* sub.u16 - 与16位立即数减 */
#define OP_AND_U8           0x7A    /* and.u8 - 与8位立即数按位与 */
#define OP_AND_U16          0x7B    /* and.u16 - 与16位立即数按位与 */
#define OP_OR_U8            0x7C    /* or.u8 - 与8位立即数按位或 */
#define OP_OR_U16           0x7D    /* or.u16 - 与16位立即数按位或 */

/* --- 函数调用指令 (0x14-0x17) --- */
#define OP_CALLEX_U8         0x14    /* callex.u8 - 调用外部函数 */
#define OP_CALLIN_U8         0x15    /* callin.u8 - 调用内部函数 */
#define OP_CALL_U16          0x16    /* call.u16 - 调用函数 */
#define OP_CALLIND          0x17    /* callind - 间接调用 */

/* --- 数值指令 - 常量 (0x20-0x3F) --- */
#define OP_CONST_I32        0x20    /* const.i32 - 32位整数常量 */
#define OP_CONST_I16        0x21    /* const.i16 - 16位整数常量 */
#define OP_CONST_I8         0x22    /* const.i8 - 8位整数常量 */
#define OP_CONST_U32        0x23    /* const.u32 - 32位无符号常量 */
#define OP_CONST_U16        0x24    /* const.u16 - 16位无符号常量 */
#define OP_CONST_U8         0x25    /* const.u8 - 8位无符号常量 */

/* --- 数值指令 - 算术运算 (0x40-0x5F) --- */
#define OP_ADD              0x40    /* add - 加法 */
#define OP_SUB              0x41    /* sub - 减法 */
#define OP_MUL              0x42    /* mul - 乘法 */
#define OP_DIV              0x43    /* div - 除法 */
#define OP_MOD              0x44    /* mod - 取模 */
#define OP_NEG              0x45    /* neg - 取负 */

#define OP_ADD_OVF          0x46    /* add.ovf - 带溢出检查加法 */
#define OP_SUB_OVF          0x47    /* sub.ovf - 带溢出检查减法 */
#define OP_MUL_OVF          0x48    /* mul.ovf - 带溢出检查乘法 */

/* --- 数值指令 - 位运算 (0x60-0x7F) --- */
#define OP_AND              0x60    /* and - 按位与 */
#define OP_OR               0x61    /* or - 按位或 */
#define OP_XOR              0x62    /* xor - 按位异或 */
#define OP_NOT              0x63    /* not - 按位取反 */
#define OP_SHL              0x64    /* shl - 左移 */
#define OP_SHR              0x65    /* shr - 右移(算术) */
#define OP_SHRU             0x66    /* shr.u - 右移(逻辑) */

/* --- 数值指令 - 比较 (0x80-0x9F) --- */
#define OP_EQ               0x80    /* eq - 等于 */
#define OP_NE               0x81    /* ne - 不等于 */
#define OP_LT               0x82    /* lt - 小于 */
#define OP_GT               0x83    /* gt - 大于 */
#define OP_LE               0x84    /* le - 小于等于 */
#define OP_GE               0x85    /* ge - 大于等于 */
#define OP_LTU              0x86    /* lt.u - 小于(无符号) */
#define OP_GTU              0x87    /* gt.u - 大于(无符号) */
#define OP_LEU              0x88    /* le.u - 小于等于(无符号) */
#define OP_GEU              0x89    /* ge.u - 大于等于(无符号) */

/* --- 变量指令 (0xA0-0xBF) --- */
#define OP_LOAD_T2_C6       0xA0    /* load.t2_c6 - 加载局部变量(2位索引+6位常量) */
#define OP_LOAD_T4_C12      0xA1    /* load.t4_c12 - 加载局部变量(4位索引+12位常量) */
#define OP_LOAD_T8          0xA2    /* load.t8 - 加载局部变量(8位索引) */
#define OP_LOAD_T16         0xA3    /* load.t16 - 加载局部变量(16位索引) */

#define OP_STORE_T2_C6      0xB0    /* store.t2_c6 - 存储局部变量 */
#define OP_STORE_T4_C12     0xB1    /* store.t4_c12 - 存储局部变量 */
#define OP_STORE_T8         0xB2    /* store.t8 - 存储局部变量 */
#define OP_STORE_T16        0xB3    /* store.t16 - 存储局部变量 */

/* --- 内存指令 (0xC0-0xDF) --- */
#define OP_LOAD_A8          0xC0    /* load.a8 - 从地址加载(8位地址) */
#define OP_LOAD_A16         0xC1    /* load.a16 - 从地址加载(16位地址) */
#define OP_LOAD_A32         0xC2    /* load.a32 - 从地址加载(32位地址) */

#define OP_STORE_A8         0xD0    /* store.a8 - 存储到地址(8位地址) */
#define OP_STORE_A16        0xD1    /* store.a16 - 存储到地址(16位地址) */
#define OP_STORE_A32        0xD2    /* store.a32 - 存储到地址(32位地址) */

#define OP_LOADBASE_A16     0xD3    /* loadbase.a16 - 基址+偏移加载 */
#define OP_STOREBASE_A16    0xD4    /* storebase.a16 - 基址+偏移存储 */

/* --- 异常处理指令 (0xE0-0xEF) --- */
#define OP_THROW            0xE0    /* throw - 抛出异常 */
#define OP_TRY              0xE1    /* try - 尝试块开始 */
#define OP_CATCH            0xE2    /* catch - 捕获异常 */
#define OP_ENDTRY           0xE3    /* endtry - 尝试块结束 */

/* --- 复合指令 (0xF0-0xFB) --- */
#define OP_DUP              0xF0    /* dup - 复制栈顶 */
#define OP_SWAP             0xF1    /* swap - 交换栈顶两个元素 */
#define OP_OVER             0xF2    /* over - 复制次栈顶到栈顶 */
#define OP_ROT              0xF3    /* rot - 旋转栈顶三个元素 */

/* --- 双字节操作码前缀 (0xFC-0xFE) --- */
#define OP_PREFIX_FC        0xFC    /* 双字节操作码前缀 */
#define OP_PREFIX_FD        0xFD    /* 双字节操作码前缀 */
#define OP_PREFIX_FE        0xFE    /* 双字节操作码前缀 */

/* ============================================================================
 * 指令信息结构
 * ============================================================================ */

/**
 * @brief 指令元数据
 */
typedef struct {
    u16 opcode;                 /* 操作码 */
    const char *mnemonic;       /* 助记符 */
    GCOSOpCodeCategory category;/* 指令分类 */
    u8 operand_size;            /* 操作数大小(字节) */
    const char *description;    /* 描述 */
} GCOSInstructionInfo;

/* ============================================================================
 * API 函数
 * ============================================================================ */

/**
 * @brief 获取指令信息
 * @param opcode 操作码
 * @return 指令信息指针, NULL表示无效操作码
 */
const GCOSInstructionInfo* gcos_instruction_get_info(u16 opcode);

/**
 * @brief 获取操作码分类
 * @param opcode 操作码
 * @return 指令分类
 */
GCOSOpCodeCategory gcos_instruction_get_category(u16 opcode);

/**
 * @brief 获取操作码助记符
 * @param opcode 操作码
 * @return 助记符字符串
 */
const char* gcos_instruction_get_mnemonic(u16 opcode);

/**
 * @brief 获取操作数大小
 * @param opcode 操作码
 * @return 操作数大小(字节)
 */
u8 gcos_instruction_get_operand_size(u16 opcode);

/**
 * @brief 判断是否为有效操作码
 * @param opcode 操作码
 * @return true 有效, false 无效
 */
bool gcos_instruction_is_valid(u16 opcode);

/**
 * @brief 判断是否为双字节操作码
 * @param opcode 操作码
 * @return true 是双字节, false 单字节
 */
bool gcos_instruction_is_double_byte(u16 opcode);

/**
 * @brief 打印指令信息
 * @param opcode 操作码
 */
void gcos_instruction_print_info(u16 opcode);

/**
 * @brief 获取所有指令数量
 * @return 指令总数
 */
u32 gcos_instruction_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_INSTRUCTIONS_H */
