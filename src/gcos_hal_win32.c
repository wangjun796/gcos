/**
 * @file gcos_hal_win32.c
 * @brief HAL implementation for Win32 platform (TCP Socket based)
 * 
 * This module provides hardware abstraction for Windows platform using
 * TCP sockets. It can be easily replaced with MCU-specific implementations
 * (SPI/I2C/UART) for real card hardware.
 */

#include "gcos_hal.h"
#include <stdio.h>
#include <string.h>

#ifdef GCOS_PLATFORM_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* ============================================================================
 * Internal State
 * ============================================================================ */

static bool hal_initialized = false;
static int hal_socket = -1;              // Listen socket
static int hal_client_socket = -1;       // Accepted client socket
static HalInterfaceType current_interface = HAL_INTERFACE_CONTACTED;

#ifdef GCOS_PLATFORM_WIN32
static WSADATA wsa_data;
#endif

/* ============================================================================
 * HAL Implementation
 * ============================================================================ */

GCOSResult hal_init(const HalConfig *config) {
    if (config == NULL) {
        printf("[HAL] ERROR: NULL configuration\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (hal_initialized) {
        printf("[HAL] WARNING: HAL already initialized, cleaning up first\n");
        hal_cleanup();
    }
    
    printf("[HAL] Initializing HAL (port=%u, interface=%s)...\n",
           config->port,
           config->interface_type == HAL_INTERFACE_CONTACTED ? "CONTACTED" : "CONTACTLESS");
    
#ifdef GCOS_PLATFORM_WIN32
    /* Initialize Winsock */
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        printf("[HAL] ERROR: WSAStartup failed (%d)\n", result);
        return GCOS_ERR_INVALID_PARAM;
    }
#endif
    
    /* Create socket */
    hal_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (hal_socket < 0) {
        printf("[HAL] ERROR: Failed to create socket\n");
#ifdef GCOS_PLATFORM_WIN32
        WSACleanup();
#endif
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(hal_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    /* Bind to address */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config->port);
    
    if (bind(hal_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[HAL] ERROR: Failed to bind to port %u\n", config->port);
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_socket);
        WSACleanup();
#else
        close(hal_socket);
#endif
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Listen for connections */
    if (listen(hal_socket, 1) < 0) {
        printf("[HAL] ERROR: Failed to listen\n");
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_socket);
        WSACleanup();
#else
        close(hal_socket);
#endif
        return GCOS_ERR_INVALID_PARAM;
    }
    
    current_interface = config->interface_type;
    hal_initialized = true;
    
    printf("[HAL] HAL initialized successfully on port %u\n", config->port);
    return GCOS_SUCCESS;
}

s16 hal_read(u8 *buffer, u16 max_len) {
    if (!hal_initialized || buffer == NULL || max_len == 0) {
        return -1;
    }
    
    /* If no client connected, accept one */
    if (hal_client_socket < 0) {
        if (hal_socket < 0) {
            printf("[HAL] ERROR: Listen socket not valid\n");
            return -1;
        }
        
        hal_client_socket = accept(hal_socket, NULL, NULL);
        if (hal_client_socket < 0) {
            printf("[HAL] ERROR: accept() failed\n");
            return -1;
        }
        
        printf("[HAL] Client connected for read\n");
    }
    
    /* Read data from client */
#ifdef GCOS_PLATFORM_WIN32
    int ret = recv(hal_client_socket, (char*)buffer, max_len, 0);
#else
    int ret = read(hal_client_socket, buffer, max_len);
#endif
    
    if (ret < 0) {
        printf("[HAL] ERROR: recv/read failed\n");
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_client_socket);
#else
        close(hal_client_socket);
#endif
        hal_client_socket = -1;
        return -1;
    }
    
    if (ret == 0) {
        printf("[HAL] Connection closed by peer\n");
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_client_socket);
#else
        close(hal_client_socket);
#endif
        hal_client_socket = -1;
        return 0;
    }
    
    printf("[HAL] Received %d bytes\n", ret);
    return (s16)ret;
}

s16 hal_write(const u8 *buffer, u16 len) {
    if (!hal_initialized || buffer == NULL || len == 0) {
        return -1;
    }
    
    /* If no client connected, accept one */
    if (hal_client_socket < 0) {
        if (hal_socket < 0) {
            printf("[HAL] ERROR: Listen socket not valid\n");
            return -1;
        }
        
        hal_client_socket = accept(hal_socket, NULL, NULL);
        if (hal_client_socket < 0) {
            printf("[HAL] ERROR: accept() failed for write\n");
            return -1;
        }
        
        printf("[HAL] Client connected for write\n");
    }
    
    /* Write data to client */
#ifdef GCOS_PLATFORM_WIN32
    int ret = send(hal_client_socket, (const char*)buffer, len, 0);
#else
    int ret = write(hal_client_socket, buffer, len);
#endif
    
    if (ret < 0) {
        printf("[HAL] ERROR: send/write failed\n");
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_client_socket);
#else
        close(hal_client_socket);
#endif
        hal_client_socket = -1;
        return -1;
    }
    
    printf("[HAL] Sent %d bytes\n", ret);
    return (s16)ret;
}

void hal_cleanup(void) {
    if (!hal_initialized) {
        return;
    }
    
    printf("[HAL] Cleaning up HAL...\n");
    
    /* Close client socket first */
    if (hal_client_socket >= 0) {
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_client_socket);
#else
        close(hal_client_socket);
#endif
        hal_client_socket = -1;
    }
    
    /* Then close listen socket */
    if (hal_socket >= 0) {
#ifdef GCOS_PLATFORM_WIN32
        closesocket(hal_socket);
#else
        close(hal_socket);
#endif
        hal_socket = -1;
    }
    
#ifdef GCOS_PLATFORM_WIN32
    WSACleanup();
#endif
    
    hal_initialized = false;
    printf("[HAL] HAL cleanup complete\n");
}

HalInterfaceType hal_get_interface_type(void) {
    return current_interface;
}

bool hal_is_initialized(void) {
    return hal_initialized;
}
