/**
 * @file test_jcshell_server.c
 * @brief Simple test to verify JCShell server initialization
 */

#include <stdio.h>
#include "gcos_jcshell.h"

int main(void) {
    printf("Testing JCShell server initialization...\n\n");
    
    /* Initialize JCShell server */
    GCOSResult result = gcos_jcshell_init();
    if (result != 0) {
        printf("ERROR: gcos_jcshell_init() failed with code %d\n", result);
        return 1;
    }
    printf("✓ JCShell server initialized successfully\n\n");
    
    /* Start JCShell server */
    result = gcos_jcshell_start();
    if (result != 0) {
        printf("ERROR: gcos_jcshell_start() failed with code %d\n", result);
        return 1;
    }
    printf("✓ JCShell server started successfully\n\n");
    
    printf("Server should now be listening on ports 9000 and 9900\n");
    printf("Press Enter to exit...\n");
    getchar();
    
    /* Cleanup */
    gcos_jcshell_cleanup();
    printf("\n✓ JCShell server cleaned up\n");
    
    return 0;
}
