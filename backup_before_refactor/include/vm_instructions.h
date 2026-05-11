#ifndef VM_INSTRUCTIONS_H
#define VM_INSTRUCTIONS_H

#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 指令操作码定义
 * 基于 GB/T 44901.3 标准附录A
 * ============================================================================ */

/* --- 控制流指令 (0x00-0x1F) --- */
#define OP_NOP          0x00    /* 空操作 */
#define OP_TRAP         0x01    /* 陷阱指令 */
#define OP_RET          0x02    /* 返回 */
#define OP_IRET         0x03    /* 中断返回 */

#define OP_BR_S8        0x10    /* 无条件跳转（8位偏移） */
#define OP_BR_S16       0x11    /* 无条件跳转（16位偏移） */

#define OP_BEQZ_S8      0x12    /* 为零则跳转（8位） */
#define OP_BEQZ_S16     0x13    /* 为零则跳转（16位） */
#define OP_BNEZ_S8      0x14    /* 非零则跳转（8位） */
#define OP_BNEZ_S16     0x15    /* 非零则跳转（16位） */

#define OP_BEQ_S8       0x16    /* 相等则跳转（8位） */
#define OP_BNE_S8       0x17    /* 不等则跳转（8位） */
#define OP_BLTU_S8      0x18    /* 无符号小于则跳转 */
#define OP_BGTU_S8      0x19    /* 无符号大于则跳转 */
#define OP_BLEU_S8      0x1A    /* 无符号小于等于则跳转 */
#define OP_BGEU_S8      0x1B    /* 无符号大于等于则跳转 */

#define OP_BRTB_S8      0x1C    /* 真值表跳转（8位） */
#define OP_BRTB_S16     0x1D    /* 真值表跳转（16位） */

/* --- 调用指令 (0x20-0x2F) --- */
#define OP_CALL_U16     0x20    /* 调用函数（16位地址） */
#define OP_CALLIND      0x21    /* 间接调用 */
#define OP_CALLEX_U8    0x22    /* 扩展调用（8位） */
#define OP_CALLIN_U8    0x23    /* 内部调用（8位） */

/* --- 栈操作指令 (0x30-0x3F) --- */
#define OP_POP          0x30    /* 弹出栈顶 */
#define OP_POP_IS       0x31    /* 弹出到间接栈 */
#define OP_PICK         0x32    /* 复制栈中元素 */

/* --- 常量加载指令 (0x40-0x5F) --- */
#define OP_LDC_0        0x40    /* 加载常数0 */
#define OP_LDC_1        0x41    /* 加载常数1 */
#define OP_LDC_2        0x42    /* 加载常数2 */
#define OP_LDC_3        0x43    /* 加载常数3 */
#define OP_LDC_4        0x44    /* 加载常数4 */
#define OP_LDC_5        0x45    /* 加载常数5 */
#define OP_LDC_6        0x46    /* 加载常数6 */
#define OP_LDC_7        0x47    /* 加载常数7 */
#define OP_LDC_8        0x48    /* 加载常数8 */
#define OP_LDC_M1       0x49    /* 加载常数-1 */

#define OP_LDC_U8       0x4A    /* 加载8位无符号常数 */
#define OP_LDC_U16      0x4B    /* 加载16位无符号常数 */
#define OP_LDC_I32      0x4C    /* 加载32位整数 */
#define OP_LDC_A8       0x4D    /* 加载8位地址 */
#define OP_LDC_A16      0x4E    /* 加载16位地址 */

/* --- 算术运算指令 (0x60-0x7F) --- */
#define OP_ADD          0x60    /* 加法 */
#define OP_SUB          0x61    /* 减法 */
#define OP_MUL          0x62    /* 乘法 */
#define OP_DIVS         0x63    /* 有符号除法 */
#define OP_DIVU         0x64    /* 无符号除法 */
#define OP_REMS         0x65    /* 有符号取余 */
#define OP_REMU         0x66    /* 无符号取余 */

#define OP_ADD_U8       0x68    /* 8位加法 */
#define OP_ADD_U16      0x69    /* 16位加法 */
#define OP_SUB_U8       0x6A    /* 8位减法 */
#define OP_SUB_U16      0x6B    /* 16位减法 */

/* 紧凑格式算术指令 */
#define OP_ADD_T2_C6    0x70    /* 加法（紧凑格式） */
#define OP_ADD_T3_C5    0x71
#define OP_ADD_T4_C12   0x72
#define OP_SUB_T2_C6    0x73
#define OP_SUB_T3_C5    0x74
#define OP_SUB_T4_C12   0x75

/* --- 逻辑运算指令 (0x80-0x9F) --- */
#define OP_AND          0x80    /* 按位与 */
#define OP_OR           0x81    /* 按位或 */
#define OP_XOR          0x82    /* 按位异或 */
#define OP_SHL          0x83    /* 左移 */
#define OP_SHRS         0x84    /* 算术右移 */
#define OP_SHRU         0x85    /* 逻辑右移 */
#define OP_ROTL         0x86    /* 左旋转 */
#define OP_ROTR         0x87    /* 右旋转 */

#define OP_AND_U8       0x88    /* 8位按位与 */
#define OP_AND_U16      0x89    /* 16位按位与 */
#define OP_OR_U8        0x8A    /* 8位按位或 */
#define OP_OR_U16       0x8B    /* 16位按位或 */

/* 紧凑格式逻辑指令 */
#define OP_AND_T3_C5    0x90    /* 按位与（紧凑格式） */
#define OP_SHL_T3_C5    0x91
#define OP_SHRU_T3_C5   0x92

/* --- 类型转换指令 (0xA0-0xAF) --- */
#define OP_CVT_U8       0xA0    /* 转换为8位无符号 */
#define OP_CVT_U16      0xA1    /* 转换为16位无符号 */

/* --- 比较指令 (0xB0-0xBF) --- */
#define OP_EQZ          0xB0    /* 等于零 */
#define OP_EQ           0xB1    /* 相等 */
#define OP_NE           0xB2    /* 不等 */
#define OP_LTS          0xB3    /* 有符号小于 */
#define OP_LTU          0xB4    /* 无符号小于 */
#define OP_GTS          0xB5    /* 有符号大于 */
#define OP_GTU          0xB6    /* 无符号大于 */
#define OP_LES          0xB7    /* 有符号小于等于 */
#define OP_LEU          0xB8    /* 无符号小于等于 */
#define OP_GES          0xB9    /* 有符号大于等于 */
#define OP_GEU          0xBA    /* 无符号大于等于 */

#define OP_EQ_U8        0xC0    /* 8位相等 */
#define OP_EQ_U16       0xC1    /* 16位相等 */
#define OP_NE_U8        0xC2    /* 8位不等 */
#define OP_NE_U16       0xC3    /* 16位不等 */

/* --- 直接数据访问指令 (0xD0-0xDF) --- */
#define OP_LDT_0        0xD0    /* 加载局部变量0 */
#define OP_LDT_1        0xD1    /* 加载局部变量1 */
#define OP_LDT_2        0xD2    /* 加载局部变量2 */
#define OP_LDT_3        0xD3    /* 加载局部变量3 */
#define OP_LDT_4        0xD4    /* 加载局部变量4 */
#define OP_LDT_5        0xD5    /* 加载局部变量5 */
#define OP_LDT_6        0xD6    /* 加载局部变量6 */
#define OP_LDT_7        0xD7    /* 加载局部变量7 */
#define OP_LDT_8        0xD8    /* 加载局部变量8 */
#define OP_LDT          0xD9    /* 加载局部变量（索引） */

#define OP_STT_0        0xDA    /* 存储到局部变量0 */
#define OP_STT_1        0xDB    /* 存储到局部变量1 */
#define OP_STT_2        0xDC    /* 存储到局部变量2 */
#define OP_STT_3        0xDD    /* 存储到局部变量3 */
#define OP_STT_4        0xDE    /* 存储到局部变量4 */
#define OP_STT_5        0xDF    /* 存储到局部变量5 */
#define OP_STT_6        0xE0    /* 存储到局部变量6 */
#define OP_STT_7        0xE1    /* 存储到局部变量7 */
#define OP_STT_8        0xE2    /* 存储到局部变量8 */
#define OP_STT          0xE3    /* 存储到局部变量（索引） */
#define OP_STT_IS       0xE4    /* 从间接栈存储 */

/* --- 全局/域数据访问指令 (0xE5-0xEF) --- */
#define OP_STLDT_0      0xE5    /* 加载全局/域变量0 */
#define OP_STLDT_1      0xE6    /* 加载全局/域变量1 */
#define OP_STLDT_2      0xE7    /* 加载全局/域变量2 */
#define OP_STLDT_3      0xE8    /* 加载全局/域变量3 */
#define OP_STLDT_4      0xE9    /* 加载全局/域变量4 */
#define OP_STLDT_5      0xEA    /* 加载全局/域变量5 */
#define OP_STLDT_6      0xEB    /* 加载全局/域变量6 */
#define OP_STLDT_7      0xEC    /* 加载全局/域变量7 */
#define OP_STLDT_8      0xED    /* 加载全局/域变量8 */
#define OP_STLDT        0xEE    /* 加载全局/域变量（索引） */
#define OP_STLDT_IS     0xEF    /* 从间接栈存储到全局/域 */

/* --- 内存块操作指令 (0xF0-0xFF) --- */
#define OP_LDMS8        0xF0    /* 加载有符号8位内存 */
#define OP_LDMU8        0xF1    /* 加载无符号8位内存 */
#define OP_LDMS16       0xF2    /* 加载有符号16位内存 */
#define OP_LDMU16       0xF3    /* 加载无符号16位内存 */
#define OP_LDM32        0xF4    /* 加载32位内存 */

#define OP_STM8         0xF5    /* 存储8位内存 */
#define OP_STM16        0xF6    /* 存储16位内存 */
#define OP_STM32        0xF7    /* 存储32位内存 */

#define OP_MCOPY        0xF8    /* 内存拷贝 */
#define OP_MFILL        0xF9    /* 内存填充 */

/* --- 异常处理指令 --- */
#define OP_TRY          0xFA    /* 尝试块开始 */
#define OP_CATCH        0xFB    /* 捕获块开始 */

/* --- 紧凑格式指令扩展 --- */
#define OP_ADD_T4_T4    0xFC    /* 加法（双4位操作数） */
#define OP_SUB_T4_T4    0xFD    /* 减法（双4位操作数） */
#define OP_TINC_T8_U8   0xFE    /* 自增 */
#define OP_TDEC_T8_U8   0xFF    /* 自减 */

/* ============================================================================
 * 指令结构
 * ============================================================================ */

/**
 * @brief 指令信息
 */
typedef struct {
    u8 opcode;                  /* 操作码 */
    const char *mnemonic;       /* 助记符 */
    OpCodeCategory category;    /* 指令分类 */
    u8 operand_count;           /* 操作数数量 */
    u8 operand_types[4];        /* 操作数类型 */
    const char *description;    /* 指令描述 */
} InstructionInfo;

/* ============================================================================
 * API 函数声明
 * ============================================================================ */

/**
 * @brief 获取指令信息
 * @param opcode 操作码
 * @return 指令信息指针，无效操作码返回NULL
 */
const InstructionInfo* vm_get_instruction_info(u8 opcode);

/**
 * @brief 解码指令
 * @param code 字节码指针
 * @param code_size 代码大小
 * @param offset 当前偏移
 * @param opcode 输出：操作码
 * @param operands 输出：操作数数组
 * @param operand_count 输出：操作数数量
 * @return 指令长度（字节），失败返回0
 */
int vm_decode_instruction(const u8 *code, u32 code_size, u32 offset,
                          u8 *opcode, u32 *operands, u8 *operand_count);

/**
 * @brief 执行单条指令
 * @param vm 虚拟机上下文
 * @param opcode 操作码
 * @param operands 操作数数组
 * @param operand_count 操作数数量
 * @return 0成功，非0失败
 */
int vm_execute_instruction(VMContext *vm, u8 opcode, 
                           const u32 *operands, u8 operand_count);

/**
 * @brief 打印指令（调试用）
 * @param opcode 操作码
 * @param operands 操作数数组
 * @param operand_count 操作数数量
 */
void vm_print_instruction(u8 opcode, const u32 *operands, u8 operand_count);

/**
 * @brief 获取操作码分类
 * @param opcode 操作码
 * @return 操作码分类
 */
OpCodeCategory vm_get_opcode_category(u8 opcode);

/**
 * @brief 检查操作码是否有效
 * @param opcode 操作码
 * @return true有效，false无效
 */
bool vm_is_valid_opcode(u8 opcode);

#ifdef __cplusplus
}
#endif

#endif /* VM_INSTRUCTIONS_H */
