/**
 * @file gcos_transport.c
 * @brief APDU transport layer implementation
 * 
 * Provides real APDU send/receive functionality:
 * - TCP socket mode (for remote testing, similar to cref)
 * - STDIN/STDOUT mode (for command-line interaction)
 * - Serial port mode (placeholder for real card hardware)
 * 
 * This module replaces the simulate_receive_apdu() in gcos_main.c
 * with actual communication channels.
 */

#include "gcos_transport.h"
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

static TransportMode current_mode = TRANSPORT_MODE_TCP_SERVER;
static int socket_fd = -1;
static bool transport_initialized = false;

#ifdef GCOS_PLATFORM_WIN32
static WSADATA wsa_data;
#endif

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Convert hex character to byte value
 */
static int hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/**
 * @brief Parse hex string to byte array
 * 
 * @param hex_str   Hex string (e.g., "00A40400")
 * @param buffer    Output buffer
 * @param max_len   Maximum buffer length
 * @return          Number of bytes parsed, or -1 on error
 */
static int parse_hex_string(const char *hex_str, u8 *buffer, u16 max_len) {
    int len = 0;
    const char *ptr = hex_str;
    
    while (*ptr && len < max_len) {
        /* Skip whitespace */
        while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') {
            ptr++;
        }
        
        if (!*ptr) break;
        
        /* Parse two hex characters */
        int high = hex_char_to_byte(*ptr++);
        if (high < 0) return -1;
        
        if (!*ptr) return -1; /* Need two hex chars */
        
        int low = hex_char_to_byte(*ptr++);
        if (low < 0) return -1;
        
        buffer[len++] = (u8)((high << 4) | low);
    }
    
    return len;
}

/**
 * @brief Convert byte array to hex string
 */
static void bytes_to_hex(const u8 *data, u16 len, char *hex_str, u16 max_len) {
    u16 pos = 0;
    for (u16 i = 0; i < len && pos < max_len - 2; i++) {
        sprintf(&hex_str[pos], "%02X", data[i]);
        pos += 2;
    }
    hex_str[pos] = '\0';
}

/* ============================================================================
 * TCP Socket Transport Implementation
 * ============================================================================ */

static int socket_init_server(u16 port) {
#ifdef GCOS_PLATFORM_WIN32
    /* Initialize Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("[Transport] ERROR: WSAStartup failed\n");
        return -1;
    }
#endif
    
    /* Create socket */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("[Transport] ERROR: Failed to create socket\n");
        return -1;
    }
    
    /* Allow address reuse */
    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    /* Bind to port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[Transport] ERROR: Failed to bind to port %u\n", port);
        return -1;
    }
    
    /* Listen */
    if (listen(socket_fd, 1) < 0) {
        printf("[Transport] ERROR: Failed to listen\n");
        return -1;
    }
    
    printf("[Transport] Server listening on port %u...\n", port);
    printf("[Transport] Waiting for client connection...\n");
    
    /* Accept connection */
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        printf("[Transport] ERROR: Failed to accept connection\n");
        return -1;
    }
    
    printf("[Transport] Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    /* Close server socket, keep client socket */
#ifdef GCOS_PLATFORM_WIN32
    closesocket(socket_fd);
#else
    close(socket_fd);
#endif
    socket_fd = client_fd;
    
    return 0;
}

static u16 socket_receive_apdu(u8 *buffer, u16 max_len) {
    if (socket_fd < 0) {
        printf("[Transport] ERROR: Socket not connected\n");
        return 0;
    }
    
    /* Receive length (2 bytes, big-endian) */
    u8 len_buf[2];
    int received = recv(socket_fd, (char*)len_buf, 2, 0);
    if (received != 2) {
        printf("[Transport] ERROR: Failed to receive length\n");
        return 0;
    }
    
    u16 apdu_len = ((u16)len_buf[0] << 8) | (u16)len_buf[1];
    
    if (apdu_len > max_len) {
        printf("[Transport] ERROR: APDU too long (%u > %u)\n", apdu_len, max_len);
        return 0;
    }
    
    /* Receive APDU data */
    received = recv(socket_fd, (char*)buffer, apdu_len, 0);
    if (received != apdu_len) {
        printf("[Transport] ERROR: Failed to receive APDU data\n");
        return 0;
    }
    
    return apdu_len;
}

static void socket_send_response(const u8 *data, u16 data_len, u16 sw) {
    if (socket_fd < 0) {
        printf("[Transport] ERROR: Socket not connected\n");
        return;
    }
    
    /* Send response length (2 bytes for data + 2 bytes for SW) */
    u16 total_len = data_len + 2;
    u8 len_buf[2];
    len_buf[0] = (u8)(total_len >> 8);
    len_buf[1] = (u8)(total_len & 0xFF);
    
    send(socket_fd, (const char*)len_buf, 2, 0);
    
    /* Send response data */
    if (data_len > 0) {
        send(socket_fd, (const char*)data, data_len, 0);
    }
    
    /* Send status word */
    u8 sw_buf[2];
    sw_buf[0] = (u8)(sw >> 8);
    sw_buf[1] = (u8)(sw & 0xFF);
    send(socket_fd, (const char*)sw_buf, 2, 0);
}

static void socket_cleanup(void) {
    if (socket_fd >= 0) {
#ifdef GCOS_PLATFORM_WIN32
        closesocket(socket_fd);
        WSACleanup();
#else
        close(socket_fd);
#endif
        socket_fd = -1;
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_transport_init(TransportMode mode, u16 port) {
    if (transport_initialized) {
        gcos_transport_cleanup();
    }
    
    current_mode = mode;
    
    switch (mode) {
        case TRANSPORT_MODE_TCP_SERVER:
            printf("[Transport] Initializing TCP server mode on port %u\n", port);
            if (socket_init_server(port) < 0) {
                return GCOS_ERR_INVALID_PARAM;
            }
            break;
            
        default:
            printf("[Transport] ERROR: Unsupported transport mode\n");
            return GCOS_ERR_INVALID_PARAM;
    }
    
    transport_initialized = true;
    return GCOS_SUCCESS;
}

u16 gcos_transport_receive_apdu(u8 *buffer, u16 max_len) {
    if (!transport_initialized) {
        printf("[Transport] ERROR: Transport not initialized\n");
        return 0;
    }
    
    switch (current_mode) {
        case TRANSPORT_MODE_TCP_SERVER:
            return socket_receive_apdu(buffer, max_len);
            
        default:
            printf("[Transport] ERROR: Unsupported mode for receive\n");
            return 0;
    }
}

void gcos_transport_send_response(const u8 *data, u16 data_len, u16 sw) {
    if (!transport_initialized) {
        printf("[Transport] ERROR: Transport not initialized\n");
        return;
    }
    
    switch (current_mode) {
        case TRANSPORT_MODE_TCP_SERVER:
            socket_send_response(data, data_len, sw);
            break;
            
        default:
            printf("[Transport] ERROR: Unsupported mode for send\n");
            break;
    }
}

void gcos_transport_cleanup(void) {
    if (!transport_initialized) {
        return;
    }
    
    switch (current_mode) {
        case TRANSPORT_MODE_TCP_SERVER:
            socket_cleanup();
            break;
            
        default:
            break;
    }
    
    transport_initialized = false;
    printf("[Transport] Cleanup complete\n");
}

TransportMode gcos_transport_get_mode(void) {
    return current_mode;
}

/* ============================================================================
 * Low-Level Byte I/O Implementation (for TLP224)
 * ============================================================================ */

s8 gcos_transport_send_byte(u8 byte) {
    switch (current_mode) {
        case TRANSPORT_MODE_TCP_SERVER: {
#ifdef GCOS_PLATFORM_WIN32
            if (socket_fd == -1) {
#ifdef TRANSPORT_DEBUG
                printf("[TRANSPORT DEBUG] ERROR: Socket not connected\n");
#endif
                return -1;
            }
            
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] Sending byte via TCP: 0x%02X ('%c')...\n", 
                   byte, (byte >= 32 && byte < 127) ? byte : '.');
#endif
            
            int n = send(socket_fd, (const char*)&byte, 1, 0);
#ifdef TRANSPORT_DEBUG
            if (n != 1) {
                printf("[TRANSPORT DEBUG] send() returned %d (expected 1)\n", n);
            }
#endif
            return (n == 1) ? 0 : -1;
#else
            if (socket_fd == -1) {
#ifdef TRANSPORT_DEBUG
                printf("[TRANSPORT DEBUG] ERROR: Socket not connected\n");
#endif
                return -1;
            }
            
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] Sending byte via TCP: 0x%02X ('%c')...\n", 
                   byte, (byte >= 32 && byte < 127) ? byte : '.');
#endif
            
            ssize_t n = write(socket_fd, &byte, 1);
#ifdef TRANSPORT_DEBUG
            if (n != 1) {
                printf("[TRANSPORT DEBUG] write() returned %ld (expected 1)\n", (long)n);
            }
#endif
            return (n == 1) ? 0 : -1;
#endif
        }
            
        default:
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] ERROR: Invalid transport mode\n");
#endif
            return -1;
    }
}

s8 gcos_transport_receive_byte(u8 *byte) {
    if (byte == NULL) {
        return -1;
    }
    
    switch (current_mode) {
        case TRANSPORT_MODE_TCP_SERVER: {
#ifdef GCOS_PLATFORM_WIN32
            if (socket_fd == -1) {
#ifdef TRANSPORT_DEBUG
                printf("[TRANSPORT DEBUG] ERROR: Socket not connected\n");
#endif
                return -1;
            }
            
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] Waiting to receive byte on socket %d...\n", socket_fd);
#endif
            
            char c;
            int n = recv(socket_fd, &c, 1, 0);
            if (n != 1) {
#ifdef TRANSPORT_DEBUG
                printf("[TRANSPORT DEBUG] recv() returned %d (expected 1)\n", n);
                if (n == 0) {
                    printf("[TRANSPORT DEBUG] Connection closed by peer\n");
                } else {
                    printf("[TRANSPORT DEBUG] recv() error: %d\n", WSAGetLastError());
                }
#endif
                return -1;
            }
            *byte = (u8)(c & 0xFF);
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] TCP received byte: 0x%02X ('%c')\n", 
                   *byte, (*byte >= 32 && *byte < 127) ? *byte : '.');
#endif
            return 0;
#else
            if (socket_fd == -1) {
#ifdef TRANSPORT_DEBUG
                printf("[TRANSPORT DEBUG] ERROR: Socket not connected\n");
#endif
                return -1;
            }
            
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] Waiting to receive byte on socket %d...\n", socket_fd);
#endif
            
            char c;
            ssize_t n = read(socket_fd, &c, 1);
            if (n != 1) {
#ifdef TRANSPORT_DEBUG
                printf("[TRANSPORT DEBUG] read() returned %ld (expected 1)\n", (long)n);
                if (n == 0) {
                    printf("[TRANSPORT DEBUG] Connection closed by peer\n");
                } else {
                    printf("[TRANSPORT DEBUG] read() error: %d (%s)\n", 
                           errno, strerror(errno));
                }
#endif
                return -1;
            }
            *byte = (u8)(c & 0xFF);
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] TCP received byte: 0x%02X ('%c')\n", 
                   *byte, (*byte >= 32 && *byte < 127) ? *byte : '.');
#endif
            return 0;
#endif
        }
            
        default:
#ifdef TRANSPORT_DEBUG
            printf("[TRANSPORT DEBUG] ERROR: Invalid transport mode\n");
#endif
            return -1;
    }
}
