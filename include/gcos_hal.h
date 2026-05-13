#ifndef GCOS_HAL_H
#define GCOS_HAL_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * HAL Interface Type Definitions
 * ============================================================================ */

/**
 * @brief HAL interface type
 */
typedef enum {
    HAL_INTERFACE_CONTACTED = 0,      /**< Contacted interface (ISO 7816) */
    HAL_INTERFACE_CONTACTLESS = 1     /**< Contactless interface (ISO 14443) */
} HalInterfaceType;

/**
 * @brief HAL configuration structure
 */
typedef struct {
    u16 port;                         /**< Port number (for TCP mode) */
    HalInterfaceType interface_type;  /**< Interface type */
    const char *device_path;          /**< Device path (for serial/SPI mode) */
    u32 baudrate;                     /**< Baud rate (for serial mode) */
} HalConfig;

/* ============================================================================
 * HAL API - Hardware Abstraction Layer
 * ============================================================================ */

/**
 * @brief Initialize HAL
 * 
 * Platform-specific initialization for hardware communication.
 * 
 * @param config  HAL configuration parameters
 * @return GCOSResult
 */
GCOSResult hal_init(const HalConfig *config);

/**
 * @brief Read data from hardware
 * 
 * Blocking read operation. Returns when data is available or timeout occurs.
 * 
 * @param buffer   Receive buffer
 * @param max_len  Maximum bytes to read
 * @return Actual bytes read, -1 on error, 0 on connection closed
 */
s16 hal_read(u8 *buffer, u16 max_len);

/**
 * @brief Write data to hardware
 * 
 * Blocking write operation.
 * 
 * @param buffer  Transmit buffer
 * @param len     Bytes to write
 * @return Actual bytes written, -1 on error
 */
s16 hal_write(const u8 *buffer, u16 len);

/**
 * @brief Cleanup HAL resources
 * 
 * Release all hardware resources and close connections.
 */
void hal_cleanup(void);

/**
 * @brief Get current interface type
 * 
 * @return Current interface type (CONTACTED or CONTACTLESS)
 */
HalInterfaceType hal_get_interface_type(void);

/**
 * @brief Check if HAL is initialized
 * 
 * @return true if initialized, false otherwise
 */
bool hal_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_HAL_H */
