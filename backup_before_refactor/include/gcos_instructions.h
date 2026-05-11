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

/* --- 操作码定义 --- */
#define OP_TRAP            0x00
#define OP_NOP             0x01
#define OP_POP             0x02
#define OP_POP_IS          0x03
#define OP_BR_S8           0x04
#define OP_BR_S16          0x05
#define OP_BEQZ_S8         0x06
#define OP_BEQZ_S16        0x07
#define OP_BNEZ_S8         0x08
#define OP_BNEZ_S16        0x09
#define OP_BEQ_S8          0x0A
#define OP_BNE_S8          0x0B
#define OP_BLTU_S8         0x0C
#define OP_BGTU_S8         0x0D
#define OP_BLEU_S8         0x0E
#define OP_BGEU_S8         0x0F
#define OP_BRTB_S8         0x10
#define OP_BRTB_S16        0x11
#define OP_RET             0x12
#define OP_IRET            0x13
#define OP_CALLEX_U8       0x14
#define OP_CALLIN_U8       0x15
#define OP_CALL_U16        0x16
#define OP_CALLIND         0x17
#define OP_LDC_0           0x18
#define OP_LDC_1           0x19
#define OP_LDC_2           0x1A
#define OP_LDC_3           0x1B
#define OP_LDC_4           0x1C
#define OP_LDC_5           0x1D
#define OP_LDC_6           0x1E
#define OP_LDC_7           0x1F
#define OP_LDC_8           0x20
#define OP_LDC_LEB         0x21
#define OP_LDC_M1          0x22
#define OP_LDC_U8          0x23
#define OP_LDC_U16         0x24
#define OP_LDC_A8          0x25
#define OP_LDC_A16         0x26
#define OP_ADD             0x27
#define OP_SUB             0x28
#define OP_MUL             0x29
#define OP_DIVS            0x2A
#define OP_DIVU            0x2B
#define OP_REMS            0x2C
#define OP_REMU            0x2D
#define OP_AND             0x2E
#define OP_OR              0x2F
#define OP_XOR             0x30
#define OP_SHL             0x31
#define OP_SHRS            0x32
#define OP_SHRU            0x33
#define OP_ROTL            0x34
#define OP_ROTR            0x35
#define OP_CVT_U8          0x36
#define OP_CVT_U16         0x37
#define OP_EQZ             0x38
#define OP_EQ              0x39
#define OP_NE              0x3A
#define OP_LTS             0x3B
#define OP_LTU             0x3C
#define OP_GTS             0x3D
#define OP_GTU             0x3E
#define OP_LES             0x3F
#define OP_LEU             0x40
#define OP_GES             0x41
#define OP_GEU             0x42
#define OP_LDT_0           0x43
#define OP_LDT_1           0x44
#define OP_LDT_2           0x45
#define OP_LDT_3           0x46
#define OP_LDT_4           0x47
#define OP_LDT_5           0x48
#define OP_LDT_6           0x49
#define OP_LDT_7           0x4A
#define OP_LDT_8           0x4B
#define OP_LDT             0x4C
#define OP_STT_0           0x4D
#define OP_STT_1           0x4E
#define OP_STT_2           0x4F
#define OP_STT_3           0x50
#define OP_STT_4           0x51
#define OP_STT_5           0x52
#define OP_STT_6           0x53
#define OP_STT_7           0x54
#define OP_STT_8           0x55
#define OP_STT             0x56
#define OP_STT_IS          0x57
#define OP_STLDT_0         0x58
#define OP_STLDT_1         0x59
#define OP_STLDT_2         0x5A
#define OP_STLDT_3         0x5B
#define OP_STLDT_4         0x5C
#define OP_STLDT_5         0x5D
#define OP_STLDT_6         0x5E
#define OP_STLDT_7         0x5F
#define OP_STLDT_8         0x60
#define OP_STLDT           0x61
#define OP_STLDT_IS        0x62
#define OP_LDMS8           0x63
#define OP_LDMU8           0x64
#define OP_LDMS16          0x65
#define OP_LDMU16          0x66
#define OP_LDM32           0x67
#define OP_STM8            0x68
#define OP_STM16           0x69
#define OP_STM32           0x6A
#define OP_MCOPY           0x6B
#define OP_MFILL           0x6C
#define OP_TRY             0x6D
#define OP_CATCH           0x6E
#define OP_PICK            0x6F
#define OP_BEQU8_FW        0x70
#define OP_BEQU16_FW       0x71
#define OP_BNEU8_FW        0x72
#define OP_BNEU16_FW       0x73
#define OP_BLTU8_FW        0x74
#define OP_BGTU8_FW        0x75
#define OP_ADD_U8          0x76
#define OP_ADD_U16         0x77
#define OP_SUB_U8          0x78
#define OP_SUB_U16         0x79
#define OP_AND_U8          0x7A
#define OP_AND_U16         0x7B
#define OP_OR_U8           0x7C
#define OP_OR_U16          0x7D
#define OP_SHO_O2_C5       0x7E
#define OP_EQ_U8           0x7F
#define OP_EQ_U16          0x80
#define OP_NE_U8           0x81
#define OP_NE_U16          0x82
#define OP_ADD_T2_C6       0x83
#define OP_ADD_T3_C5       0x84
#define OP_ADD_T4_C12      0x85
#define OP_SUB_T2_C6       0x86
#define OP_SUB_T3_C5       0x87
#define OP_SUB_T4_C12      0x88
#define OP_AND_T3_C5       0x89
#define OP_SHL_T3_C5       0x8A
#define OP_SHRU_T3_C5      0x8B
#define OP_ANDO_O1_T7      0x8C
#define OP_ADD_T4_T4       0x8D
#define OP_SUB_T4_T4       0x8E
#define OP_TINC_T8_U8      0x8F
#define OP_TDEC_T8_U8      0x90
#define OP_STT_T8_U8       0x91
#define OP_STT_T8_U16      0x92
#define OP_LDMU8_T4_D4     0x93
#define OP_LDMU8_T4_D12    0x94
#define OP_LDMU16_T4_D12   0x95
#define OP_LDM32_T2_D6     0x96
#define OP_LDM32_T4_D12    0x97
#define OP_LDM32_T8_D16    0x98
#define OP_LDMO_O2_T6_D16  0x99
#define OP_LDMU8_A8        0x9A
#define OP_LDMU8_A16       0x9B
#define OP_LDMU16_A8       0x9C
#define OP_LDMU16_A16      0x9D
#define OP_LDM32_A8        0x9E
#define OP_LDM32_A16       0x9F
#define OP_LDMO_O1_A15     0xA0
#define OP_STM8_T4_D4      0xA1
#define OP_STM8_T4_D12     0xA2
#define OP_STM16_T4_D12    0xA3
#define OP_STM32_T2_D6     0xA4
#define OP_STM32_T4_D12    0xA5
#define OP_STMO_O2_T6_D16  0xA6
#define OP_STM8_A8         0xA7
#define OP_STM8_A16        0xA8
#define OP_STM16_A8        0xA9
#define OP_STM16_A16       0xAA
#define OP_STM32_A8        0xAB
#define OP_STM32_A16       0xAC

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
