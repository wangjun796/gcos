/**
 * @file gcos_platform.h
 * @brief GCOS VM 平台适配层
 * 
 * 支持的平台:
 * - Win32 (Windows桌面环境)
 * - Cortex-M (Keil MDK-ARM编译环境)
 * 
 * 使用方法:
 * 1. Win32: 定义 GCOS_PLATFORM_WIN32 或不定义任何平台宏（默认）
 * 2. Keil Cortex-M: 定义 GCOS_PLATFORM_KEIL_CM
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#ifndef GCOS_PLATFORM_H
#define GCOS_PLATFORM_H

/* ============================================================================
 * 平台检测与定义
 * ============================================================================ */

/* 自动检测编译器 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
    /* Keil MDK-ARM 编译器 */
    #ifndef GCOS_PLATFORM_KEIL_CM
        #define GCOS_PLATFORM_KEIL_CM
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    /* Windows 平台 */
    #ifndef GCOS_PLATFORM_WIN32
        #define GCOS_PLATFORM_WIN32
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    /* GCC/Clang 编译器（可能是Linux或嵌入式GCC） */
    #if defined(__arm__) || defined(__thumb__)
        /* ARM GCC */
        #ifndef GCOS_PLATFORM_ARM_GCC
            #define GCOS_PLATFORM_ARM_GCC
        #endif
    #else
        /* x86/x64 GCC */
        #ifndef GCOS_PLATFORM_LINUX
            #define GCOS_PLATFORM_LINUX
        #endif
    #endif
#endif

/* 如果没有检测到任何平台，默认使用Win32 */
#if !defined(GCOS_PLATFORM_WIN32) && \
    !defined(GCOS_PLATFORM_KEIL_CM) && \
    !defined(GCOS_PLATFORM_ARM_GCC) && \
    !defined(GCOS_PLATFORM_LINUX)
    #define GCOS_PLATFORM_WIN32
#endif

/* ============================================================================
 * 平台特性配置
 * ============================================================================ */

/* Win32 平台配置 */
#ifdef GCOS_PLATFORM_WIN32
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    
    /* 标准库函数可用 */
    #define GCOS_HAS_STDIO          1
    #define GCOS_HAS_STDLIB         1
    #define GCOS_HAS_STRING         1
    
    /* 内存分配可用（但GCOS VM不使用动态分配） */
    #define GCOS_HAS_MALLOC         1
    
    /* 调试输出启用 */
    #define GCOS_ENABLE_DEBUG       1

/* Keil Cortex-M 平台配置 */
#elif defined(GCOS_PLATFORM_KEIL_CM)
    /* Keil环境下没有标准printf */
    #define GCOS_HAS_STDIO          0
    #define GCOS_HAS_STDLIB         0
    #define GCOS_HAS_STRING         1  /* string.h通常可用 */
    
    /* 禁止动态内存分配 */
    #define GCOS_HAS_MALLOC         0
    
    /* 默认禁用调试输出（可通过外部实现） */
    #ifndef GCOS_ENABLE_DEBUG
        #define GCOS_ENABLE_DEBUG   0
    #endif

/* ARM GCC 平台配置 */
#elif defined(GCOS_PLATFORM_ARM_GCC)
    #include <string.h>
    
    #define GCOS_HAS_STDIO          0
    #define GCOS_HAS_STDLIB         0
    #define GCOS_HAS_STRING         1
    #define GCOS_HAS_MALLOC         0
    
    #ifndef GCOS_ENABLE_DEBUG
        #define GCOS_ENABLE_DEBUG   0
    #endif

/* Linux 平台配置 */
#elif defined(GCOS_PLATFORM_LINUX)
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    
    #define GCOS_HAS_STDIO          1
    #define GCOS_HAS_STDLIB         1
    #define GCOS_HAS_STRING         1
    #define GCOS_HAS_MALLOC         1
    #define GCOS_ENABLE_DEBUG       1
#endif

/* ============================================================================
 * 统一API定义
 * ============================================================================ */

/* 字符串操作 */
#ifdef GCOS_HAS_STRING
    #include <string.h>
    #define gcos_memcpy(dest, src, n)     memcpy((dest), (src), (n))
    #define gcos_memset(ptr, val, n)      memset((ptr), (val), (n))
    #define gcos_memcmp(p1, p2, n)        memcmp((p1), (p2), (n))
#else
    /* 需要用户提供这些函数的实现 */
    void gcos_memcpy(void *dest, const void *src, size_t n);
    void gcos_memset(void *ptr, int val, size_t n);
    int gcos_memcmp(const void *p1, const void *p2, size_t n);
#endif

/* 调试输出宏 */
#if GCOS_ENABLE_DEBUG && GCOS_HAS_STDIO
    /* 有stdio的平台，使用printf */
    #define GCOS_PRINTF(fmt, ...)     printf(fmt, ##__VA_ARGS__)
    #define GCOS_PRINT(str)           printf("%s", str)
#elif GCOS_ENABLE_DEBUG
    /* 启用调试但没有stdio，使用弱符号，用户可以自己实现 */
    extern void gcos_debug_printf(const char *fmt, ...);
    #define GCOS_PRINTF(fmt, ...)     gcos_debug_printf(fmt, ##__VA_ARGS__)
    #define GCOS_PRINT(str)           gcos_debug_printf("%s", str)
#else
    /* 禁用调试输出 */
    #define GCOS_PRINTF(fmt, ...)     ((void)0)
    #define GCOS_PRINT(str)           ((void)0)
#endif

/* 断言宏 */
#if GCOS_ENABLE_DEBUG
    #ifdef GCOS_HAS_STDIO
        #include <assert.h>
        #define GCOS_ASSERT(expr)     assert(expr)
    #else
        #define GCOS_ASSERT(expr)     do { if (!(expr)) { while(1); } } while(0)
    #endif
#else
    #define GCOS_ASSERT(expr)         ((void)0)
#endif

/* 内存分配（GCOS VM不使用，但提供接口） */
#if GCOS_HAS_MALLOC
    #define gcos_malloc(size)         malloc(size)
    #define gcos_free(ptr)            free(ptr)
#else
    #define gcos_malloc(size)         NULL
    #define gcos_free(ptr)            ((void)0)
#endif

/* ============================================================================
 * 平台特定优化
 * ============================================================================ */

/* 字节序检测 */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define GCOS_LITTLE_ENDIAN        1
#elif defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
    #define GCOS_LITTLE_ENDIAN        1
#else
    #define GCOS_LITTLE_ENDIAN        0
#endif

/* 内联提示 */
#ifdef _MSC_VER
    #define GCOS_INLINE               __inline
    #define GCOS_FORCE_INLINE         __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define GCOS_INLINE               inline
    #define GCOS_FORCE_INLINE         inline __attribute__((always_inline))
#else
    #define GCOS_INLINE               inline
    #define GCOS_FORCE_INLINE         inline
#endif

/* 未使用参数警告抑制 */
#ifdef _MSC_VER
    #define GCOS_UNUSED(x)            ((void)(x))
#elif defined(__GNUC__) || defined(__clang__)
    #define GCOS_UNUSED(x)            ((void)(x))
#else
    #define GCOS_UNUSED(x)            ((void)(x))
#endif

/* ============================================================================
 * 平台初始化/清理钩子（可选实现）
 * ============================================================================ */

/**
 * @brief 平台初始化钩子
 * @note 在VM初始化前调用，可用于初始化硬件、时钟等
 */
void gcos_platform_init(void);

/**
 * @brief 平台清理钩子
 * @note 在VM销毁后调用，可用于释放资源
 */
void gcos_platform_cleanup(void);

/* 默认空实现（弱符号） */
/* 注意：MSVC不支持__weak，需要在.c文件中提供实现 */
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((weak)) void gcos_platform_init(void) {}
    __attribute__((weak)) void gcos_platform_cleanup(void) {}
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
    /* Keil ARM Compiler支持__weak */
    __weak void gcos_platform_init(void) {}
    __weak void gcos_platform_cleanup(void) {}
#else
    /* MSVC或其他编译器：不提供默认实现，用户必须自己实现 */
    /* 或者使用宏定义空函数 */
    #ifndef GCOS_PLATFORM_INIT_DEFINED
        static inline void gcos_platform_init(void) {}
        static inline void gcos_platform_cleanup(void) {}
    #endif
#endif

#endif /* GCOS_PLATFORM_H */
