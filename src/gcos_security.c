/**
 * @file gcos_security.c
 * @brief GCOS VM Security Management Implementation
 * 
 * Implements COS3 specification security features:
 * - Domain isolation
 * - Interface authorization table
 * - Access control checks
 * - Security context management
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * Security Constants
 * ============================================================================ */

#define MAX_DOMAINS             16      /* Maximum number of domains */
#define AUTH_TABLE_SIZE         256     /* Authorization table size */

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Check if operation is authorized
 * @param vm VM instance
 * @param interface_id Interface identifier
 * @return true if authorized, false otherwise
 */
static bool check_authorization(const GCOSVM *vm, u8 interface_id) {
    if (vm == NULL || interface_id >= AUTH_TABLE_SIZE) {
        return false;
    }
    
    /* Check authorization table */
    return vm->security.authorization_table[interface_id] != 0;
}

/**
 * @brief Validate domain access
 * @param vm VM instance
 * @param target_domain Target domain ID
 * @return GCOSResult Success or error code
 */
static GCOSResult validate_domain_access(const GCOSVM *vm, u8 target_domain) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if target domain exists */
    if (target_domain >= MAX_DOMAINS) {
        GCOS_PRINTF("[Security] Invalid domain ID: %u\n", target_domain);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Check if current domain can access target domain */
    /* In COS3, higher privilege domains can access lower privilege domains */
    if (vm->security.current_domain > target_domain) {
        GCOS_PRINTF("[Security] Domain access violation: current=%u, target=%u\n",
                   vm->security.current_domain, target_domain);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    return GCOS_SUCCESS;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_security_init(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Initialize security context */
    vm->security.current_domain = 0;  /* Start in most privileged domain */
    vm->security.authorization_table_size = AUTH_TABLE_SIZE;
    
    /* Clear authorization table */
    memset(vm->security.authorization_table, 0, AUTH_TABLE_SIZE);
    
    /* Set default authorizations for system interfaces */
    /* Domain 0 (system) has full access */
    for (u8 i = 0; i < AUTH_TABLE_SIZE; i++) {
        vm->security.authorization_table[i] = 0x01;  /* Authorized for domain 0 */
    }
    
    GCOS_PRINTF("[Security] Security manager initialized\n");
    return GCOS_SUCCESS;
}

GCOSResult gcos_security_set_domain(GCOSVM *vm, u8 domain_id) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Validate domain transition */
    GCOSResult result = validate_domain_access(vm, domain_id);
    if (result != GCOS_SUCCESS) {
        return result;
    }
    
    u8 old_domain = vm->security.current_domain;
    vm->security.current_domain = domain_id;
    
    GCOS_PRINTF("[Security] Domain changed: %u -> %u\n", old_domain, domain_id);
    return GCOS_SUCCESS;
}

u8 gcos_security_get_current_domain(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    
    return vm->security.current_domain;
}

GCOSResult gcos_security_grant_access(GCOSVM *vm, u8 interface_id, u8 domain_mask) {
    if (vm == NULL || interface_id >= AUTH_TABLE_SIZE) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Only privileged domains can grant access */
    if (vm->security.current_domain != 0) {
        GCOS_PRINTF("[Security] Access denied: insufficient privileges to grant access\n");
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Set authorization bits */
    vm->security.authorization_table[interface_id] = domain_mask;
    
    GCOS_PRINTF("[Security] Access granted: interface=%u, domains=0x%02X\n",
               interface_id, domain_mask);
    return GCOS_SUCCESS;
}

GCOSResult gcos_security_revoke_access(GCOSVM *vm, u8 interface_id, u8 domain_mask) {
    if (vm == NULL || interface_id >= AUTH_TABLE_SIZE) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Only privileged domains can revoke access */
    if (vm->security.current_domain != 0) {
        GCOS_PRINTF("[Security] Access denied: insufficient privileges to revoke access\n");
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Clear authorization bits */
    vm->security.authorization_table[interface_id] &= ~domain_mask;
    
    GCOS_PRINTF("[Security] Access revoked: interface=%u, domains=0x%02X\n",
               interface_id, domain_mask);
    return GCOS_SUCCESS;
}

bool gcos_security_check_permission(const GCOSVM *vm, u8 interface_id) {
    if (vm == NULL || interface_id >= AUTH_TABLE_SIZE) {
        return false;
    }
    
    u8 domain_bit = 1 << vm->security.current_domain;
    bool authorized = (vm->security.authorization_table[interface_id] & domain_bit) != 0;
    
    if (!authorized) {
        GCOS_PRINTF("[Security] Permission denied: interface=%u, domain=%u\n",
                   interface_id, vm->security.current_domain);
    }
    
    return authorized;
}

GCOSResult gcos_security_validate_memory_access(const GCOSVM *vm, u32 address, u32 size,
                                                GCOSMemoryAccessType access_type) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check bounds based on memory region */
    switch (access_type) {
        case GCOS_MEMORY_ACCESS_STACK:
            if (address + size > GCOS_EXECUTOR_STACK_SIZE * sizeof(u32)) {
                GCOS_PRINTF("[Security] Stack access out of bounds: addr=%u, size=%u\n",
                           address, size);
                return GCOS_ERROR_MEMORY_ACCESS;
            }
            break;
            
        case GCOS_MEMORY_ACCESS_GLOBAL:
            if (address + size > GCOS_GLOBAL_DATA_SIZE) {
                GCOS_PRINTF("[Security] Global data access out of bounds: addr=%u, size=%u\n",
                           address, size);
                return GCOS_ERROR_MEMORY_ACCESS;
            }
            break;
            
        case GCOS_MEMORY_ACCESS_HEAP:
            if (address + size > GCOS_HEAP_SIZE) {
                GCOS_PRINTF("[Security] Heap access out of bounds: addr=%u, size=%u\n",
                           address, size);
                return GCOS_ERROR_MEMORY_ACCESS;
            }
            break;
            
        case GCOS_MEMORY_ACCESS_CODE:
            if (address + size > GCOS_MODULE_CODE_SIZE) {
                GCOS_PRINTF("[Security] Code access out of bounds: addr=%u, size=%u\n",
                           address, size);
                return GCOS_ERROR_MEMORY_ACCESS;
            }
            break;
            
        default:
            GCOS_PRINTF("[Security] Unknown memory access type: %d\n", access_type);
            return GCOS_ERROR_INVALID_PARAM;
    }
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_security_verify_caller(const GCOSVM *vm, u16 caller_address,
                                       u16 target_address) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Verify caller is within valid code range */
    if (caller_address >= vm->runtime.code_size) {
        GCOS_PRINTF("[Security] Invalid caller address: %u\n", caller_address);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Verify target is within valid code range */
    if (target_address >= vm->runtime.code_size) {
        GCOS_PRINTF("[Security] Invalid target address: %u\n", target_address);
        return GCOS_ERROR_MEMORY_ACCESS;
    }
    
    /* Check if cross-domain call is authorized */
    /* This is a simplified check - real implementation would track function domains */
    
    return GCOS_SUCCESS;
}

GCOSResult gcos_security_setup_app_isolation(GCOSVM *vm, u8 app_index) {
    if (vm == NULL || app_index >= vm->app_count) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    const GCOSAppInstance *app = &vm->apps[app_index];
    
    /* Assign app to its own domain based on app index */
    u8 app_domain = app_index + 1;  /* Domain 0 is reserved for system */
    
    if (app_domain >= MAX_DOMAINS) {
        GCOS_PRINTF("[Security] Cannot create domain: max domains reached\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Set current domain to app's domain */
    vm->security.current_domain = app_domain;
    
    /* Configure authorization for app domain */
    /* Apps can only access their own resources by default */
    for (u8 i = 0; i < AUTH_TABLE_SIZE; i++) {
        /* Clear all bits except the app's own domain bit */
        vm->security.authorization_table[i] = (1 << app_domain);
    }
    
    GCOS_PRINTF("[Security] App %u isolated in domain %u\n", app_index, app_domain);
    return GCOS_SUCCESS;
}

void gcos_security_dump_authorization_table(const GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    GCOS_PRINTF("\n=== Security Authorization Table ===\n");
    GCOS_PRINTF("Current Domain: %u\n", vm->security.current_domain);
    GCOS_PRINTF("Table Size: %u bytes\n", vm->security.authorization_table_size);
    
    /* Print non-zero entries */
    u32 printed = 0;
    for (u32 i = 0; i < vm->security.authorization_table_size && printed < 32; i++) {
        if (vm->security.authorization_table[i] != 0) {
            GCOS_PRINTF("  Interface %3u: domains=0x%02X\n",
                       i, vm->security.authorization_table[i]);
            printed++;
        }
    }
    
    GCOS_PRINTF("===================================\n\n");
}
