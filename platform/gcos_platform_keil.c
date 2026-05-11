/**
 * @file gcos_platform_keil.c
 * @brief Keil Cortex-M平台适配实现
 * 
 * 此文件提供Keil MDK-ARM环境下的平台特定功能实现：
 * - 调试输出（ITM/UART）
 * - 平台初始化/清理钩子
 * - 时间获取函数
 * 
 * @note 将此文件添加到Keil工程中
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_platform.h"

/* ============================================================================
 * 配置选项
 * ============================================================================ */

/* 选择调试输出方式 */
#define GCOS_DEBUG_OUTPUT_ITM       1   /* 使用ITM输出（推荐） */
#define GCOS_DEBUG_OUTPUT_UART      0   /* 使用UART输出 */
#define GCOS_DEBUG_OUTPUT_SEMIHOST  0   /* 使用半主机模式（仅调试） */

/* 启用/禁用调试输出 */
#ifndef GCOS_ENABLE_DEBUG
    #define GCOS_ENABLE_DEBUG       1   /* 1=启用, 0=禁用 */
#endif

/* ============================================================================
 * 头文件包含
 * ============================================================================ */

#if GCOS_ENABLE_DEBUG
    #include <stdio.h>
    #include <stdarg.h>
    #include <string.h>
    
    /* STM32 HAL库（如果使用UART） */
    #if GCOS_DEBUG_OUTPUT_UART
        #include "stm32f4xx_hal.h"  /* 根据实际芯片修改 */
        extern UART_HandleTypeDef huart1;  /* 根据实际配置修改 */
    #endif
    
    /* CMSIS ITM支持 */
    #if GCOS_DEBUG_OUTPUT_ITM
        #include "core_cm4.h"  /* 根据Cortex-M版本修改 */
    #endif
#endif

/* ============================================================================
 * 调试输出实现
 * ============================================================================ */

#if GCOS_ENABLE_DEBUG && !GCOS_HAS_STDIO

/**
 * @brief Keil环境下的调试输出实现
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * 
 * @note 根据配置选择不同的输出方式
 */
void gcos_debug_printf(const char *fmt, ...) {
    if (fmt == NULL) {
        return;
    }
    
#if GCOS_DEBUG_OUTPUT_ITM
    /* 方法1: 使用ITM (Instrumentation Trace Macrocell) */
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    /* 通过ITM逐个字符发送 */
    for (int i = 0; buffer[i] != '\0'; i++) {
        /* 检查ITM是否可用 */
        if (ITM->TCR & ITM_TCR_ITMENA_Msk) {
            ITM_SendChar((uint32_t)buffer[i]);
        }
    }
    
#elif GCOS_DEBUG_OUTPUT_UART
    /* 方法2: 使用UART */
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    /* 通过UART发送（阻塞方式） */
    HAL_UART_Transmit(&huart1, (uint8_t*)buffer, len, 1000);
    
#elif GCOS_DEBUG_OUTPUT_SEMIHOST
    /* 方法3: 使用半主机模式（仅调试时） */
    /* 注意：生产环境必须禁用，否则会崩溃 */
    #pragma import(__use_no_semihosting)
    
    /* 简化实现：什么都不做 */
    (void)fmt;
    
#else
    /* 未配置输出方式 */
    (void)fmt;
#endif
}

#endif /* GCOS_ENABLE_DEBUG && !GCOS_HAS_STDIO */

/* ============================================================================
 * 平台初始化/清理钩子
 * ============================================================================ */

/**
 * @brief Cortex-M平台初始化
 * @note 在VM初始化前调用
 */
void gcos_platform_init(void) {
#if GCOS_DEBUG_OUTPUT_ITM
    /* 初始化ITM */
    /* ITM默认已启用，无需额外配置 */
    
    /* 可选：配置ITM端口 */
    // ITM->TER |= (1 << 0);  /* 启用端口0 */
    
#elif GCOS_DEBUG_OUTPUT_UART
    /* 初始化UART */
    /* UART通常在SystemInit()中已初始化 */
    // MX_USART1_UART_Init();
#endif
    
    /* 其他平台初始化 */
    /* - 系统时钟已在启动代码中配置 */
    /* - GPIO、DMA等根据需要初始化 */
}

/**
 * @brief Cortex-M平台清理
 * @note 在VM销毁后调用
 */
void gcos_platform_cleanup(void) {
    /* 关闭外设 */
    /* - 关闭UART */
    /* - 关闭DMA */
    /* - 进入低功耗模式 */
    
    /* 简化实现：什么都不做 */
}

/* ============================================================================
 * 时间获取函数（用于性能统计）
 * ============================================================================ */

#if GCOS_ENABLE_DEBUG

/**
 * @brief 获取当前时间（微秒）
 * @return 当前时间戳（微秒）
 * 
 * @note Cortex-M通常使用SysTick或DWT计数器
 */
uint64_t get_current_time_us(void) {
    /* 方法1: 使用DWT计数器（如果可用） */
    #if defined(__CORTEX_M) && (__CORTEX_M >= 3)
        static uint32_t dwt_initialized = 0;
        
        if (!dwt_initialized) {
            /* 启用DWT计数器 */
            CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
            DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
            dwt_initialized = 1;
        }
        
        /* DWT计数器是32位，需要处理溢出 */
        static uint32_t last_count = 0;
        static uint64_t overflow_count = 0;
        
        uint32_t current_count = DWT->CYCCNT;
        if (current_count < last_count) {
            overflow_count++;
        }
        last_count = current_count;
        
        /* 假设系统时钟为168MHz（STM32F4） */
        uint64_t total_cycles = (overflow_count << 32) | current_count;
        return (total_cycles * 1000000ULL) / SystemCoreClock;
        
    #else
        /* 方法2: 使用SysTick（精度较低） */
        /* SysTick通常是1ms中断，不适合微秒级计时 */
        return 0;  /* 返回0表示不支持 */
    #endif
}

#else

/* 调试禁用时，提供空实现 */
uint64_t get_current_time_us(void) {
    return 0;
}

#endif /* GCOS_ENABLE_DEBUG */

/* ============================================================================
 * 内存操作函数（如果string.h不可用）
 * ============================================================================ */

#if !GCOS_HAS_STRING

/**
 * @brief 内存复制
 */
void gcos_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/**
 * @brief 内存设置
 */
void gcos_memset(void *ptr, int val, size_t n) {
    uint8_t *p = (uint8_t *)ptr;
    
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)val;
    }
}

/**
 * @brief 内存比较
 */
int gcos_memcmp(const void *p1, const void *p2, size_t n) {
    const uint8_t *a = (const uint8_t *)p1;
    const uint8_t *b = (const uint8_t *)p2;
    
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return a[i] - b[i];
        }
    }
    
    return 0;
}

#endif /* !GCOS_HAS_STRING */
