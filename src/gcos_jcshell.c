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
static int process_client_connection(int client_sock, u16 port) {
    TLP_MSG msg;
    u8 apdu_buffer[APDU_BUFFER_SIZE];
    u8 response_buffer[RESPONSE_BUFFER_SIZE];
    u16 response_length = 0;
    u16 sw = 0x9000;
    
    printf("[JCShell] Client connected on port %u\n", port);
    
    /* Initialize TLP message */
    tlp_msg_init(&msg);
    msg.fd = client_sock;
    
    /* Main message processing loop */
    while (1) {
        s16 recv_len;
        
        /* Step 1: Receive TLP224 message */
        recv_len = tlp_receive_message(&msg);
        if (recv_len < 0) {
            printf("[JCShell] ERROR: Failed to receive TLP224 message\n");
            break;
        }
        
        printf("[JCShell] Received TLP224 message (%d bytes)\n", recv_len);
        printf("  Binary: ");
        for (int i = 0; i < msg.len && i < 20; i++) {
            printf("%02X", msg.buf[i]);
        }
        if (msg.len > 20) printf("...");
        printf("\n");
        
        /* Step 2: Check for POWER_UP command */
        if (msg.buf[3] == TLP_POWER_UP) {
            printf("[JCShell] POWER_UP command received\n");
            
            /* Send ATR */
            static const u8 atr[] = { 0x3B, 0xF4, 0x11, 0x00, 0xFF, 0x00 };
            static const u8 hist[] = { 0x11, 0x22, 0x33, 0x44 };
            
            s8 result = t0_send_atr(&msg, 4, atr, hist, false);
            if (result != 0) {
                printf("[JCShell] ERROR: Failed to send ATR\n");
                break;
            }
            
            continue;
        }
        
        /* Step 3: Check for POWER_DOWN command */
        if (msg.buf[3] == TLP_POWER_DOWN) {
            printf("[JCShell] POWER_DOWN command received\n");
            
            /* Send acknowledgment */
            msg.buf[0] = TLP_ACK;
            msg.buf[1] = 0;
            msg.buf[2] = 1;
            msg.buf[3] = STATUS_SUCCESS;
            msg.buf[4] = tlp_compute_lrc(msg.buf, 4);
            msg.len = 5;
            
            tlp_send_message(&msg);
            break;
        }
        
        /* Step 4: Process ISO_INPUT/ISO_OUTPUT commands */
        if (msg.buf[3] == TLP_ISO_INPUT || msg.buf[3] == TLP_ISO_OUTPUT) {
            /* Extract APDU from TLP message */
            u8 apdu_len = msg.len - 10;  /* Subtract TLP header (4) + LRC (1) + overhead */
            if (apdu_len > APDU_BUFFER_SIZE) {
                printf("[JCShell] ERROR: APDU too long (%u bytes)\n", apdu_len);
                continue;
            }
            
            memcpy(apdu_buffer, &msg.buf[TLP_OFFSET_CLA], apdu_len);
            
            printf("[JCShell] Processing APDU (%u bytes): ", apdu_len);
            for (int i = 0; i < apdu_len && i < 20; i++) {
                printf("%02X", apdu_buffer[i]);
            }
            printf("\n");
            
            /* Step 5: Process APDU through VM */
            extern GCOSVM* gcos_vm_get_instance(void);
            GCOSVM* vm = gcos_vm_get_instance();
            
            if (vm == NULL) {
                printf("[JCShell] ERROR: VM not initialized\n");
                sw = 0x6F00;
            } else {
                response_length = RESPONSE_BUFFER_SIZE;
                sw = gcos_vm_process_apdu(vm, apdu_buffer, apdu_len,
                                         response_buffer, &response_length);
                
                printf("[JCShell] VM returned SW=0x%04X, Response=%u bytes\n", 
                       sw, response_length);
            }
            
            /* Step 6: Build TLP224 response */
            msg.buf[0] = TLP_ACK;
            msg.buf[1] = (u8)((response_length + 3) >> 8);
            msg.buf[2] = (u8)((response_length + 3) & 0xFF);
            msg.buf[3] = (sw == 0x9000) ? STATUS_SUCCESS : STATUS_CARD_ERROR;
            
            /* Copy response data */
            if (response_length > 0) {
                memcpy(&msg.buf[4], response_buffer, response_length);
            }
            
            /* Append SW1SW2 */
            msg.buf[4 + response_length] = (u8)(sw >> 8);
            msg.buf[5 + response_length] = (u8)(sw & 0xFF);
            
            /* Compute and append LRC */
            msg.len = 6 + response_length;
            msg.buf[msg.len] = tlp_compute_lrc(msg.buf, msg.len);
            msg.len++;
            
            /* Step 7: Send TLP224 response */
            if (tlp_send_message(&msg) != 0) {
                printf("[JCShell] ERROR: Failed to send response\n");
                break;
            }
            
            printf("[JCShell] Response sent (SW=0x%04X)\n", sw);
            continue;
        }
        
        /* Unknown command type */
        printf("[JCShell] WARNING: Unknown command type 0x%02X\n", msg.buf[3]);
        
        /* Send protocol error */
        msg.buf[0] = TLP_ACK;
        msg.buf[1] = 0;
        msg.buf[2] = 1;
        msg.buf[3] = STATUS_COMMAND_UNKNOWN;
        msg.buf[4] = tlp_compute_lrc(msg.buf, 4);
        msg.len = 5;
        
        tlp_send_message(&msg);
    }
    
    printf("[JCShell] Client disconnected\n");
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
        return GCOS_ERR_INVALID_STATE;
    }
    
    printf("[JCShell] Starting server threads...\n");
    
#ifdef GCOS_PLATFORM_WIN32
    /* Start contacted server thread */
    HANDLE h_thread_contacted = CreateThread(NULL, 0, server_thread_func, 
                                             (LPVOID)(uintptr_t)CONTACTED_PORT, 
                                             0, NULL);
    if (h_thread_contacted == NULL) {
        printf("[JCShell] ERROR: Failed to create contacted thread\n");
        return GCOS_ERR_INVALID_STATE;
    }
    
    /* Start contactless server thread */
    HANDLE h_thread_contactless = CreateThread(NULL, 0, server_thread_func, 
                                               (LPVOID)(uintptr_t)CONTACTLESS_PORT, 
                                               0, NULL);
    if (h_thread_contactless == NULL) {
        printf("[JCShell] ERROR: Failed to create contactless thread\n");
        return GCOS_ERR_INVALID_STATE;
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
