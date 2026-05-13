/**
 * @file gcos_tlp_server.c
 * @brief TLP Server for GCOS (equivalent to JCRE in cref)
 * 
 * This implements the server side that listens on port 9025 and receives
 * APDU commands from JCShell via TLP protocol.
 * 
 * Architecture:
 * - JCShell (ports 9000/9900) <-> TLP Protocol <-> TLP Server (port 9025) <-> GCOS VM
 * 
 * Reference: cref/adapter/win32/server.c, io_cad.c
 */

#include "gcos_vm.h"
#include "gcos_apdu.h"
#include <stdio.h>
#include <string.h>

#ifdef GCOS_PLATFORM_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#endif

#define TLP_SERVER_PORT     9025
#define APDU_BUFFER_SIZE    261
#define RESPONSE_BUFFER_SIZE 261

/**
 * @brief ConnectInfo structure for handshake (cref-compatible)
 */
typedef struct {
    u32 magic;          /* Magic number: 0x5a5a1234 */
    u32 connect_type;   /* Connection type: 0=contacted, 2=contactless */
} ConnectInfo;

#define CONNECT_MAGIC_NUMBER 0x5a5a1234

static bool server_initialized = false;
static int listen_sock = -1;
static GCOSVM* vm_instance = NULL;

#ifdef GCOS_PLATFORM_WIN32
static WSADATA wsa_data;
#endif

/**
 * @brief Receive ConnectInfo handshake from client
 */
static int receive_handshake(int client_sock) {
    ConnectInfo info;
    
    printf("[TLP_Server] Waiting for handshake...\n");
    
    int received = recv(client_sock, (char*)&info, sizeof(ConnectInfo), 0);
    if (received != sizeof(ConnectInfo)) {
        printf("[TLP_Server] ERROR: Failed to receive handshake (%d bytes)\n", received);
        return -1;
    }
    
    printf("[TLP_Server] Handshake received: magic=0x%08X, type=%u\n",
           info.magic, info.connect_type);
    
    if (info.magic != CONNECT_MAGIC_NUMBER) {
        printf("[TLP_Server] ERROR: Invalid magic number\n");
        return -1;
    }
    
    printf("[TLP_Server] Handshake OK! Type: %s\n",
           info.connect_type == 0 ? "CONTACTED" : 
           info.connect_type == 2 ? "CONTACTLESS" : "UNKNOWN");
    
    return 0;
}

/**
 * @brief Accept a client connection (single-threaded, cref-compatible)
 * 
 * This function blocks waiting for a client connection, similar to cref's
 * getConnection() in server.c. cref uses single-threaded architecture where
 * the main thread accepts one connection at a time.
 * 
 * @param listen_sock Listening socket
 * @return Client socket fd, or -1 on error
 */
static int accept_client_connection(int listen_sock) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf("[TLP_Server] Waiting for client connection on port %d...\n", TLP_SERVER_PORT);
    
    int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        printf("[TLP_Server] ERROR: Failed to accept connection\n");
        return -1;
    }
    
    printf("[TLP_Server] Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    return client_sock;
}

/**
 * @brief Process a single client connection (single-threaded, cref-compatible)
 * 
 * Note: cref uses single-threaded architecture. The main thread accepts
 * one connection at a time via getConnection() in send_ATR().
 * This implementation follows the same pattern.
 */
static int process_client_connection(int client_sock) {
    u8 apdu_buffer[APDU_BUFFER_SIZE];
    u8 response_buffer[RESPONSE_BUFFER_SIZE + 2];
    
    /* Step 1: Receive handshake */
    if (receive_handshake(client_sock) != 0) {
        printf("[TLP_Server] Handshake failed\n");
        return -1;
    }
    
    printf("[TLP_Server] Ready to process APDUs\n");
    
    /* Step 2: Main loop - receive and process APDUs */
    while (1) {
        /* Receive 4-byte header: [type][cmd][length_hi][length_lo] */
        u8 header[4];
        int received = recv(client_sock, (char*)header, 4, 0);
        if (received != 4) {
            printf("[TLP_Server] Client disconnected (header recv failed)\n");
            break;
        }
        
        u8 type = header[0];
        u8 cmd = header[1];
        u16 size = ((u16)header[2] << 8) | (u16)header[3];
        
        printf("[TLP_Server] Received header: type=0x%02X, cmd=0x%02X, size=%u\n",
               type, cmd, size);
        
        /* Check for POWER_UP command (type=0, cmd=0x21) */
        if (type == 0 && cmd == 0x21) {
            printf("[TLP_Server] POWER_UP command received\n");
            
            /* For now, send a simple ATR response */
            /* In real implementation, this should initialize the card */
            u8 atr[] = { 0x3B, 0xF4, 0x11, 0x00, 0xFF, 0x00 };
            u8 atr_len = sizeof(atr);
            
            /* Build response: [type][cmd][data_length_hi][data_length_lo][data...] */
            u8 resp_header[4];
            resp_header[0] = type;
            resp_header[1] = cmd;
            resp_header[2] = 0;
            resp_header[3] = atr_len;
            
            send(client_sock, (const char*)resp_header, 4, 0);
            send(client_sock, (const char*)atr, atr_len, 0);
            
            printf("[TLP_Server] Sent ATR response\n");
            continue;
        }
        
        /* Regular APDU command */
        if (size == 0 || size > APDU_BUFFER_SIZE) {
            printf("[TLP_Server] Invalid APDU size: %u\n", size);
            break;
        }
        
        /* Receive APDU data */
        received = recv(client_sock, (char*)apdu_buffer, size, 0);
        if (received != size) {
            printf("[TLP_Server] Failed to receive APDU data\n");
            break;
        }
        
        printf("[TLP_Server] APDU (%u bytes): ", size);
        for (int i = 0; i < size && i < 20; i++) {
            printf("%02X", apdu_buffer[i]);
        }
        if (size > 20) printf("...");
        printf("\n");
        
        /* Process APDU through VM */
        u16 response_len = RESPONSE_BUFFER_SIZE;
        u16 sw = gcos_vm_process_apdu(vm_instance, apdu_buffer, size,
                                      response_buffer, &response_len);
        
        printf("[TLP_Server] VM returned SW: 0x%04X, Data length: %u\n", sw, response_len);
        
        /* Build response: [type][cmd][data_length_hi][data_length_lo][data...][SW_hi][SW_lo] */
        u16 total_data_len = response_len + 2;
        u8 resp_header[4];
        resp_header[0] = type;
        resp_header[1] = cmd;
        resp_header[2] = (u8)(total_data_len >> 8);
        resp_header[3] = (u8)(total_data_len & 0xFF);
        
        /* Send response header */
        send(client_sock, (const char*)resp_header, 4, 0);
        
        /* Send response data */
        if (response_len > 0) {
            send(client_sock, (const char*)response_buffer, response_len, 0);
        }
        
        /* Send SW */
        u8 sw_buf[2];
        sw_buf[0] = (u8)(sw >> 8);
        sw_buf[1] = (u8)(sw & 0xFF);
        send(client_sock, (const char*)sw_buf, 2, 0);
        
        printf("[TLP_Server] Response sent\n");
    }
    
    return 0;
}

/**
 * @brief Initialize TLP server
 */
GCOSResult gcos_tlp_server_init(GCOSVM* vm) {
    if (server_initialized) {
        return GCOS_SUCCESS;
    }
    
    vm_instance = vm;
    
    printf("[TLP_Server] Initializing server on port %d...\n", TLP_SERVER_PORT);
    
#ifdef GCOS_PLATFORM_WIN32
    /* Initialize Winsock */
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        printf("[TLP_Server] ERROR: WSAStartup failed (%d)\n", result);
        return GCOS_ERR_INVALID_PARAM;
    }
#endif
    
    /* Create socket */
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        printf("[TLP_Server] ERROR: Failed to create socket\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Allow address reuse */
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    /* Bind to port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(TLP_SERVER_PORT);
    
    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[TLP_Server] ERROR: Failed to bind to port %d\n", TLP_SERVER_PORT);
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Listen */
    if (listen(listen_sock, 5) < 0) {
        printf("[TLP_Server] ERROR: Failed to listen\n");
        return GCOS_ERR_INVALID_PARAM;
    }
    
    server_initialized = true;
    printf("[TLP_Server] Server listening on port %d\n", TLP_SERVER_PORT);
    
    return GCOS_SUCCESS;
}

/**
 * @brief Start TLP server (blocking, single-threaded like cref)
 * 
 * This follows cref's single-threaded architecture where the main thread
 * accepts and processes one connection at a time, similar to how send_ATR()
 * calls getConnection() in t0_ll.c
 */
GCOSResult gcos_tlp_server_start(void) {
    if (!server_initialized) {
        printf("[TLP_Server] ERROR: Server not initialized\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    printf("[TLP_Server] Starting server loop (single-threaded, cref-compatible)...\n");
    
    while (1) {
        /* Accept a client connection (blocking) */
        int client_sock = accept_client_connection(listen_sock);
        if (client_sock < 0) {
            printf("[TLP_Server] ERROR: Failed to accept connection\n");
            continue;
        }
        
        /* Process the connection (blocking) - single-threaded like cref */
        process_client_connection(client_sock);
        
        /* Close client socket */
#ifdef GCOS_PLATFORM_WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
        
        printf("[TLP_Server] Connection processed, ready for next connection\n");
    }
    
    return GCOS_SUCCESS;
}

/**
 * @brief Cleanup TLP server
 */
void gcos_tlp_server_cleanup(void) {
    if (listen_sock >= 0) {
#ifdef GCOS_PLATFORM_WIN32
        closesocket(listen_sock);
        WSACleanup();
#else
        close(listen_sock);
#endif
        listen_sock = -1;
    }
    server_initialized = false;
    printf("[TLP_Server] Cleanup complete\n");
}
