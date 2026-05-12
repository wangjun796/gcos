/**
 * @file gcos_main.c
 * @brief GCOS VM main entry point with real APDU transport
 * 
 * This file implements the main processing loop for GCOS VM with
 * real APDU send/receive functionality via:
 * - STDIO mode (interactive command-line)
 * - TCP server mode (remote testing, similar to cref)
 * 
 * Modeled after cref's main.c and t0.c architecture.
 */

#include "gcos_vm.h"
#include "gcos_apdu.h"
#include "gcos_transport.h"
#include "gcos_tlp.h"
#include "gcos_t0_protocol.h"
#include "gcos_jcshell.h"  /* JCShell server (NEW) */
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

#define VM_MEMORY_SIZE          (64 * 1024)  /* 64KB VM memory */
#define DEFAULT_TCP_PORT        9028         /* Default TCP port for server mode */

/* ============================================================================
 * Static Memory Allocation (No dynamic allocation per COS3 spec)
 * ============================================================================ */

/** VM instance - statically allocated */
static GCOSVM vm_instance;

/** APDU input buffer */
static u8 apdu_input_buffer[APDU_BUFFER_SIZE];

/** Response output buffer */
static u8 response_buffer[RESPONSE_BUFFER_SIZE];

/** VM memory pool */
static u8 vm_memory_pool[VM_MEMORY_SIZE];

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize VM with default configuration
 * 
 * @return GCOS_SUCCESS on success, error code otherwise
 */
static GCOSResult initialize_vm(void) {
    printf("[GCOS] Initializing VM...\n");
    
    /* Initialize VM core */
    GCOSResult result = gcos_vm_init(&vm_instance);
    if (result != GCOS_SUCCESS) {
        printf("[GCOS] ERROR: VM initialization failed (result=%d)\n", result);
        return result;
    }
    
    /* Initialize APDU subsystem */
    result = gcos_apdu_init(&vm_instance);
    if (result != GCOS_SUCCESS) {
        printf("[GCOS] ERROR: APDU initialization failed (result=%d)\n", result);
        return result;
    }
    
    printf("[GCOS] VM initialized successfully\n");
    printf("[GCOS]   Memory: %u bytes\n", VM_MEMORY_SIZE);
    printf("[GCOS]   Max modules: %u\n", MAX_MODULES);
    printf("[GCOS]   Max apps per module: %u\n", MAX_APPS_PER_MODULE);
    
    return GCOS_SUCCESS;
}

/**
 * @brief Process a single APDU command using complete T=0 protocol
 * 
 * This function implements the full T=0 protocol flow:
 * 1. Receive APDU header (5 bytes: CLA INS P1 P2 P3)
 * 2. Receive command data (if Lc > 0)
 * 3. Process command via VM
 * 4. Send response data (if any)
 * 5. Send status word (SW1SW2)
 * 
 * Based on cref's t0.c processing loop.
 * 
 * @return 1 if processing should continue, 0 to exit
 */
static int process_single_apdu(void) {
    u8 apdu_buffer[APDU_BUFFER_SIZE];
    u16 apdu_length;
    u8 response_data[RESPONSE_BUFFER_SIZE];
    u16 response_length = 0;
    u16 sw = 0x9000;    /* Default success status */
    
    /* ========================================================================
     * Step 1: Receive APDU from transport layer
     * ======================================================================== */
    apdu_length = gcos_transport_receive_apdu(apdu_buffer, APDU_BUFFER_SIZE);
    if (apdu_length == 0) {
        printf("[GCOS] No more APDUs to process\n");
        return 0; /* Exit */
    }
    
    if (apdu_length < 4) {
        printf("[GCOS] ERROR: APDU too short (%u bytes)\n", apdu_length);
        return 1; /* Continue */
    }
    
    printf("[T=0] Received APDU (%u bytes): ", apdu_length);
    for (u16 i = 0; i < apdu_length && i < 20; i++) {
        printf("%02X", apdu_buffer[i]);
    }
    if (apdu_length > 20) printf("...");
    printf("\n");
    
    /* ========================================================================
     * Step 2: Process APDU via GCOS VM
     * ======================================================================== */
    response_length = RESPONSE_BUFFER_SIZE;
    sw = gcos_vm_process_apdu(&vm_instance, 
                               apdu_buffer, 
                               apdu_length,
                               response_data, 
                               &response_length);
    
    printf("[GCOS] VM returned SW=%04X, Response length=%u\n", sw, response_length);
    
    /* ========================================================================
     * Step 3: Send Response
     * ======================================================================== */
    gcos_transport_send_response(response_data, response_length, sw);
    
    printf("[T=0] Command processing complete\n\n");
    
    return 1; /* Continue processing */
}

/**
 * @brief Print usage information
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("\nOptions:\n");
    printf("  -s, --stdio       Use STDIO mode (interactive, default)\n");
    printf("  -t, --tcp [PORT]  Use TCP server mode (default port: %u)\n", DEFAULT_TCP_PORT);
    printf("  -h, --help        Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                  # Interactive mode\n", program_name);
    printf("  %s -s               # Interactive mode\n", program_name);
    printf("  %s -t               # TCP server on port %u\n", program_name, DEFAULT_TCP_PORT);
    printf("  %s -t 9028          # TCP server on port 9028\n", program_name);
    printf("\nSTDIO Mode:\n");
    printf("  Enter APDU as hex string, e.g.:\n");
    printf("  00A4040008A000000003000000\n");
    printf("  Type 'quit' or 'exit' to terminate\n");
    printf("\nTCP Mode:\n");
    printf("  Connect using: nc localhost <PORT>\n");
    printf("  Or use a smart card terminal tool\n");
    printf("\n");
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

/**
 * @brief Main function - GCOS VM processing loop with real APDU transport
 * 
 * This function initializes the VM and enters the main processing loop.
 * Supports two transport modes:
 * 1. STDIO mode: Interactive command-line input
 * 2. TCP server mode: Accept remote connections (similar to cref)
 * 
 * The processing loop follows the cref pattern:
 * 1. Initialize VM and transport layer
 * 2. Enter infinite loop:
 *    a. Receive APDU via transport layer
 *    b. Parse and validate APDU
 *    c. Dispatch to appropriate handler
 *    d. Execute handler
 *    e. Send response (data + status word) via transport layer
 *    f. Repeat
 * 
 * @return Exit code (should never return in real card)
 */
int main(int argc, char *argv[]) {
    TransportMode mode = TRANSPORT_MODE_STDIO;
    u16 tcp_port = DEFAULT_TCP_PORT;
    
    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stdio") == 0) {
            mode = TRANSPORT_MODE_STDIO;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tcp") == 0) {
            mode = TRANSPORT_MODE_TCP_SERVER;
            if (i + 1 < argc) {
                tcp_port = (u16)atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("========================================\n");
    printf("  GCOS VM - COS3 Compliant Virtual Machine\n");
    printf("  Version 1.0.0\n");
    printf("========================================\n\n");
    
    /* Step 1: Initialize VM */
    GCOSResult result = initialize_vm();
    if (result != GCOS_SUCCESS) {
        printf("[GCOS] FATAL: Cannot initialize VM, exiting\n");
        return 1;
    }
    
    /* Step 2: Initialize transport layer */
    printf("\n[Transport] Initializing %s mode...\n", 
           mode == TRANSPORT_MODE_STDIO ? "STDIO" : "TCP Server");
    
    result = gcos_transport_init(mode, tcp_port);
    if (result != GCOS_SUCCESS) {
        printf("[GCOS] FATAL: Cannot initialize transport, exiting\n");
        gcos_vm_destroy(&vm_instance);
        return 1;
    }
    
    /* Step 2b: Initialize JCShell server (cref-compatible) */
    printf("\n[JCShell] Initializing JCShell server...\n");
    result = gcos_jcshell_init();
    if (result != GCOS_SUCCESS) {
        printf("[JCShell] WARNING: Failed to initialize JCShell server\n");
    } else {
        result = gcos_jcshell_start();
        if (result != GCOS_SUCCESS) {
            printf("[JCShell] WARNING: Failed to start JCShell server\n");
        } else {
            printf("[JCShell] Server started on ports 9000 (contacted) and 9900 (contactless)\n");
        }
    }
    
    /* Step 3: Initialize TLP and T=0 protocol layers */
    printf("\n[T=0] Initializing TLP and T=0 protocol layers...\n");
    t0_protocol_init(&g_tlp_msg);
    
    printf("\n[GCOS] Entering main processing loop...\n");
    printf("[GCOS] Waiting for APDU commands...\n\n");
    
    /* Step 3: Main processing loop */
    int continue_processing = 1;
    int apdu_count = 0;
    
    while (continue_processing) {
        apdu_count++;
        
        continue_processing = process_single_apdu();
    }
    
    /* Step 4: Cleanup */
    printf("\n[GCOS] Shutting down...\n");
    printf("[GCOS] Total APDUs processed: %d\n", apdu_count);
    
    gcos_transport_cleanup();
    gcos_vm_destroy(&vm_instance);
    
    printf("[GCOS] Shutdown complete\n");
    
    return 0;
}
