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
 * @brief Process a single APDU command
 * 
 * This is the core processing function that receives, processes,
 * and responds to APDU commands using the real transport layer.
 * 
 * @return 1 if processing should continue, 0 to exit
 */
static int process_single_apdu(void) {
    /* Step 1: Receive APDU from terminal via transport layer */
    u16 apdu_length = gcos_transport_receive_apdu(apdu_input_buffer, APDU_BUFFER_SIZE);
    if (apdu_length == 0) {
        printf("[GCOS] Connection closed or EOF\n");
        return 0; /* Exit */
    }
    
    /* Log received APDU */
    printf("[GCOS] Received APDU (%u bytes): ", apdu_length);
    for (u16 i = 0; i < apdu_length && i < 32; i++) {
        printf("%02X ", apdu_input_buffer[i]);
    }
    if (apdu_length > 32) {
        printf("...");
    }
    printf("\n");
    
    /* Step 2: Process APDU through VM */
    u16 response_length = RESPONSE_BUFFER_SIZE;
    u16 sw = gcos_vm_process_apdu(&vm_instance, 
                                   apdu_input_buffer, 
                                   apdu_length,
                                   response_buffer, 
                                   &response_length);
    
    /* Step 3: Send response back via transport layer */
    gcos_transport_send_response(response_buffer, response_length, sw);
    
    /* Step 4: Log result */
    if (sw == 0x9000) {
        printf("[GCOS] ✓ Command executed successfully\n");
    } else {
        printf("[GCOS] ✗ Command failed with status %04X\n", sw);
    }
    
    printf("\n");
    
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
