/**
 * @file gcos_jcshell.c
 * @brief JCShell server implementation (cref-compatible)
 * 
 * Implements a TCP server that listens on ports 9000 (contacted) and 9900 (contactless),
 * exactly like cref's jcshell.c. Handles TLP224 protocol messages and integrates
 * with the T=0 protocol layer.
 * 
 * Based on cref's jcshell.c, translated to English and adapted for GCOS VM.
 */

#include "gcos_jcshell.h"
#include "gcos_tlp.h"
#include "gcos_t0_protocol.h"
#include "gcos_apdu.h"
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
#include <pthread.h>
#endif

/* ============================================================================
 * Configuration Constants (from cref jcshell.c)
 * ============================================================================ */

#define CONTACTED_PORT      9000    /**< Port for contacted cards (T=0) */
#define CONTACTLESS_PORT    9900    /**< Port for contactless cards (T=CL) */
#define MAX_CLIENTS         10      /**< Maximum concurrent clients */
#define BUFFER_SIZE         1024    /**< Message buffer size */

/* ============================================================================
 * Internal State
 * ============================================================================ */

static bool server_initialized = false;
static int listen_sock_contacted = -1;
static int listen_sock_contactless = -1;

#ifdef GCOS_PLATFORM_WIN32
static WSADATA wsa_data;
#endif

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Process a single client connection
 * 
 * Handles the complete TLP224 message exchange:
 * 1. Receive TLP224 message
 * 2. Decode to binary APDU
 * 3. Process through T=0 protocol
 * 4. Encode response as TLP224
 * 5. Send back to client
 * 
 * @param client_sock  Client socket file descriptor
 * @param port         Port number (9000 or 9900)
 * @return             0 on success, -1 on error
 */
/**
 * @brief Process a single client connection (cref-compatible binary protocol)
 * 
 * This follows cref's jcshell.c processConnect() function:
 * 1. Receive 4-byte header: [type][cmd][size_hi][size_lo]
 * 2. Receive data payload
 * 3. If POWER_UP (type=0, cmd=0x21), send ATR response
 * 4. Otherwise, forward APDU to VM and return response
 * 
 * Protocol: Binary (not TLP224 ASCII hex)
 */
static int process_client_connection(int client_sock, u16 port) {
    u8 header[4];
    u8 apdu_buffer[APDU_BUFFER_SIZE];
    u8 response_buffer[RESPONSE_BUFFER_SIZE];
    u16 data_size;
    u8 type;
    u8 cmd;
    
    /* Determine connection type based on port */
    GCOSConnType conn_type;
    if (port == 9000) {
        conn_type = GCOS_CONN_TYPE_T0;
        printf("[JCShell] Connection type: T=0 (contacted, port %u)\n", port);
    } else if (port == 9900) {
        conn_type = GCOS_CONN_TYPE_T5;
        printf("[JCShell] Connection type: T=CL (contactless, port %u)\n", port);
    } else {
        printf("[JCShell] ERROR: Unknown port %u\n", port);
        return -1;
    }
    
    printf("[JCShell] Client connected on port %u\n", port);
    
    /* Main message processing loop */
    while (1) {
        /* Step 1: Receive 4-byte header */
#ifdef GCOS_PLATFORM_WIN32
        if (recv(client_sock, (char*)header, 4, 0) != 4) {
#else
        if (read(client_sock, header, 4) != 4) {
#endif
            printf("[JCShell] ERROR: Failed to receive header\n");
            break;
        }
        
        type = header[0];
        cmd = header[1];
        data_size = ((u16)header[2] << 8) | header[3];
        
        printf("[JCShell] Received header: type=%u, cmd=0x%02X, size=%u\n",
               type, cmd, data_size);
        printf("[JCShell] Header bytes: %02X %02X %02X %02X\n",
               header[0], header[1], header[2], header[3]);
        
        /* Step 2: Receive data payload */
        if (data_size > 0) {
            if (data_size >= APDU_BUFFER_SIZE) {
                printf("[JCShell] ERROR: Data too large (%u bytes)\n", data_size);
                break;
            }
            
            u16 received = 0;
            while (received < data_size) {
#ifdef GCOS_PLATFORM_WIN32
                int n = recv(client_sock, (char*)&apdu_buffer[received], 
                            data_size - received, 0);
#else
                int n = read(client_sock, &apdu_buffer[received], 
                            data_size - received);
#endif
                if (n <= 0) {
                    printf("[JCShell] ERROR: Failed to receive data\n");
                    goto close_connection;
                }
                received += n;
            }
            
            printf("[JCShell] Received %u bytes of data\n", received);
        }
        
        /* Step 3: Check for POWER_UP command (type=0, cmd=0x21) */
        if (type == 0 && cmd == 0x21) {
            printf("[JCShell] POWER_UP command received\n");
            
            /* Build ATR response */
            static const u8 atr[] = { 0x3B, 0xF4, 0x11, 0x00, 0xFF, 0x00 };
            static const u8 hist[] = { 0x11, 0x22, 0x33, 0x44 };
            u8 atr_len = sizeof(atr);
            u8 hist_len = sizeof(hist);
            u8 total_len = atr_len + hist_len;
            
            /* Response format: [type][cmd][size_hi][size_lo][atr_data...][hist_data...] */
            u8 resp_header[4];
            resp_header[0] = type;
            resp_header[1] = 0;  /* cmd=0 for response */
            resp_header[2] = 0;  /* size high byte */
            resp_header[3] = total_len;  /* size low byte */
            
            /* Send header */
#ifdef GCOS_PLATFORM_WIN32
            if (send(client_sock, (const char*)resp_header, 4, 0) != 4) {
#else
            if (write(client_sock, resp_header, 4) != 4) {
#endif
                printf("[JCShell] ERROR: Failed to send response header\n");
                break;
            }
            
            /* Send ATR data */
#ifdef GCOS_PLATFORM_WIN32
            if (send(client_sock, (const char*)atr, atr_len, 0) != (int)atr_len) {
#else
            if (write(client_sock, atr, atr_len) != (int)atr_len) {
#endif
                printf("[JCShell] ERROR: Failed to send ATR\n");
                break;
            }
            
            /* Send historical bytes */
#ifdef GCOS_PLATFORM_WIN32
            if (send(client_sock, (const char*)hist, hist_len, 0) != (int)hist_len) {
#else
            if (write(client_sock, hist, hist_len) != (int)hist_len) {
#endif
                printf("[JCShell] ERROR: Failed to send historical bytes\n");
                break;
            }
            
            printf("[JCShell] Sent ATR response (%u bytes)\n", total_len);
            continue;
        }
        
        /* Step 4: Regular APDU command - forward to VM with connection type */
        printf("[JCShell] Processing APDU command (%u bytes, conn_type=%d)\n", 
               data_size, conn_type);
        
        /* Get VM instance */
        extern GCOSVM* gcos_vm_get_instance(void);
        GCOSVM* vm = gcos_vm_get_instance();
        
        if (vm == NULL) {
            printf("[JCShell] ERROR: VM not initialized\n");
            /* Send error SW 0x6F00 */
            u8 resp_header[4];
            resp_header[0] = type;
            resp_header[1] = 0;
            resp_header[2] = 0;
            resp_header[3] = 2;  /* SW is 2 bytes */
            
#ifdef GCOS_PLATFORM_WIN32
            send(client_sock, (const char*)resp_header, 4, 0);
#else
            write(client_sock, resp_header, 4);
#endif
            u8 sw_error[2] = { 0x6F, 0x00 };
#ifdef GCOS_PLATFORM_WIN32
            send(client_sock, (const char*)sw_error, 2, 0);
#else
            write(client_sock, sw_error, 2);
#endif
            continue;
        }
        
        /* Process APDU through VM with connection type */
        memset(response_buffer, 0, RESPONSE_BUFFER_SIZE);  /* Clear buffer */
        u16 response_length = 0;  /* Initialize to 0, VM will set actual length */
        u16 sw = gcos_vm_process_apdu_with_conn_type(
            vm, apdu_buffer, data_size,
            response_buffer, &response_length,
            conn_type
        );
        
        printf("[JCShell] VM returned SW=0x%04X, Response=%u bytes\n", 
               sw, response_length);
        
        /* Build binary response: [type][cmd][size_hi][size_lo][data...][SW1][SW2] */
        u16 total_data_len = response_length + 2;  /* data + SW */
        u8 resp_header[4];
        resp_header[0] = type;
        resp_header[1] = 0;
        resp_header[2] = (u8)(total_data_len >> 8);
        resp_header[3] = (u8)(total_data_len & 0xFF);
        
        /* Send header */
#ifdef GCOS_PLATFORM_WIN32
        if (send(client_sock, (const char*)resp_header, 4, 0) != 4) {
#else
        if (write(client_sock, resp_header, 4) != 4) {
#endif
            printf("[JCShell] ERROR: Failed to send response header\n");
            break;
        }
        
        /* Send response data */
        if (response_length > 0) {
#ifdef GCOS_PLATFORM_WIN32
            if (send(client_sock, (const char*)response_buffer, response_length, 0) != (int)response_length) {
#else
            if (write(client_sock, response_buffer, response_length) != (int)response_length) {
#endif
                printf("[JCShell] ERROR: Failed to send response data\n");
                break;
            }
        }
        
        /* Send SW */
        u8 sw_bytes[2] = { (u8)(sw >> 8), (u8)(sw & 0xFF) };
#ifdef GCOS_PLATFORM_WIN32
        if (send(client_sock, (const char*)sw_bytes, 2, 0) != 2) {
#else
        if (write(client_sock, sw_bytes, 2) != 2) {
#endif
            printf("[JCShell] ERROR: Failed to send SW\n");
            break;
        }
        
        printf("[JCShell] Response sent successfully (SW=0x%04X)\n", sw);
    }
    
close_connection:
    printf("[JCShell] Client disconnected\n");
#ifdef GCOS_PLATFORM_WIN32
    closesocket(client_sock);
#else
    close(client_sock);
#endif
    return 0;
}

/**
 * @brief Server thread function
 * 
 * Accepts client connections and spawns handler threads.
 * 
 * @param arg  Port number (cast to void*)
 * @return     NULL
 */
#ifdef GCOS_PLATFORM_WIN32
static DWORD WINAPI server_thread_func(LPVOID arg)
#else
static void* server_thread_func(void* arg)
#endif
{
    u16 port = (u16)(uintptr_t)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf("[JCShell] Server thread started on port %u\n", port);
    
    while (1) {
        int client_sock = accept(port == CONTACTED_PORT ? 
                                 listen_sock_contacted : listen_sock_contactless,
                                 (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) {
            printf("[JCShell] ERROR: accept() failed\n");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[JCShell] New connection from %s:%d\n", 
               client_ip, ntohs(client_addr.sin_port));
        
        /* Process client in current thread (could be moved to separate thread) */
        process_client_connection(client_sock, port);
        
        /* Close client socket */
#ifdef GCOS_PLATFORM_WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
    }
    
    return NULL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_jcshell_init(void) {
    if (server_initialized) {
        return GCOS_SUCCESS;
    }
    
    printf("[JCShell] Initializing server...\n");
    
#ifdef GCOS_PLATFORM_WIN32
    /* Initialize Winsock */
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        printf("[JCShell] ERROR: WSAStartup failed (%d)\n", result);
        return GCOS_ERR_INVALID_PARAM;
    }
#endif
    
    /* Create contacted socket (port 9000) */
    listen_sock_contacted = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_contacted < 0) {
        printf("[JCShell] ERROR: Failed to create contacted socket\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    int opt = 1;
    setsockopt(listen_sock_contacted, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr_contacted;
    memset(&addr_contacted, 0, sizeof(addr_contacted));
    addr_contacted.sin_family = AF_INET;
    addr_contacted.sin_addr.s_addr = INADDR_ANY;
    addr_contacted.sin_port = htons(CONTACTED_PORT);
    
    if (bind(listen_sock_contacted, (struct sockaddr*)&addr_contacted, 
             sizeof(addr_contacted)) < 0) {
        printf("[JCShell] ERROR: Failed to bind contacted socket to port %u\n", 
               CONTACTED_PORT);
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (listen(listen_sock_contacted, MAX_CLIENTS) < 0) {
        printf("[JCShell] ERROR: Failed to listen on contacted socket\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    printf("[JCShell] Contacted server listening on port %u\n", CONTACTED_PORT);
    
    /* Create contactless socket (port 9900) */
    listen_sock_contactless = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_contactless < 0) {
        printf("[JCShell] ERROR: Failed to create contactless socket\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    setsockopt(listen_sock_contactless, SOL_SOCKET, SO_REUSEADDR, 
               (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr_contactless;
    memset(&addr_contactless, 0, sizeof(addr_contactless));
    addr_contactless.sin_family = AF_INET;
    addr_contactless.sin_addr.s_addr = INADDR_ANY;
    addr_contactless.sin_port = htons(CONTACTLESS_PORT);
    
    if (bind(listen_sock_contactless, (struct sockaddr*)&addr_contactless, 
             sizeof(addr_contactless)) < 0) {
        printf("[JCShell] ERROR: Failed to bind contactless socket to port %u\n", 
               CONTACTLESS_PORT);
        return GCOS_ERR_INVALID_PARAM;
    }
    
    if (listen(listen_sock_contactless, MAX_CLIENTS) < 0) {
        printf("[JCShell] ERROR: Failed to listen on contactless socket\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    printf("[JCShell] Contactless server listening on port %u\n", CONTACTLESS_PORT);
    
    server_initialized = true;
    return GCOS_SUCCESS;
}

GCOSResult gcos_jcshell_start(void) {
    if (!server_initialized) {
        printf("[JCShell] ERROR: Server not initialized\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    printf("[JCShell] Starting server threads...\n");
    
#ifdef GCOS_PLATFORM_WIN32
    /* Start contacted server thread */
    HANDLE h_thread_contacted = CreateThread(NULL, 0, server_thread_func, 
                                             (LPVOID)(uintptr_t)CONTACTED_PORT, 
                                             0, NULL);
    if (h_thread_contacted == NULL) {
        printf("[JCShell] ERROR: Failed to create contacted thread\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Start contactless server thread */
    HANDLE h_thread_contactless = CreateThread(NULL, 0, server_thread_func, 
                                               (LPVOID)(uintptr_t)CONTACTLESS_PORT, 
                                               0, NULL);
    if (h_thread_contactless == NULL) {
        printf("[JCShell] ERROR: Failed to create contactless thread\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    printf("[JCShell] Server threads started\n");
#else
    /* POSIX threads */
    pthread_t thread_contacted, thread_contactless;
    
    if (pthread_create(&thread_contacted, NULL, server_thread_func, 
                       (void*)(uintptr_t)CONTACTED_PORT) != 0) {
        printf("[JCShell] ERROR: Failed to create contacted thread\n");
        return GCOS_ERR_INVALID_STATE;
    }
    
    if (pthread_create(&thread_contactless, NULL, server_thread_func, 
                       (void*)(uintptr_t)CONTACTLESS_PORT) != 0) {
        printf("[JCShell] ERROR: Failed to create contactless thread\n");
        return GCOS_ERR_INVALID_STATE;
    }
    
    /* Detach threads */
    pthread_detach(thread_contacted);
    pthread_detach(thread_contactless);
    
    printf("[JCShell] Server threads started\n");
#endif
    
    return GCOS_SUCCESS;
}

void gcos_jcshell_cleanup(void) {
    if (!server_initialized) {
        return;
    }
    
    printf("[JCShell] Cleaning up...\n");
    
    /* Close listen sockets */
    if (listen_sock_contacted >= 0) {
#ifdef GCOS_PLATFORM_WIN32
        closesocket(listen_sock_contacted);
#else
        close(listen_sock_contacted);
#endif
        listen_sock_contacted = -1;
    }
    
    if (listen_sock_contactless >= 0) {
#ifdef GCOS_PLATFORM_WIN32
        closesocket(listen_sock_contactless);
#else
        close(listen_sock_contactless);
#endif
        listen_sock_contactless = -1;
    }
    
#ifdef GCOS_PLATFORM_WIN32
    WSACleanup();
#endif
    
    server_initialized = false;
    printf("[JCShell] Cleanup complete\n");
}
