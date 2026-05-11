/**
 * @file gcos_transaction.c
 * @brief GCOS VM Transaction Management Implementation
 * 
 * Implements COS3 specification transaction mechanism:
 * - Transaction begin/commit/abort
 * - Data backup and restore
 * - Nested transaction support
 * - Atomicity guarantees
 * 
 * @version 1.0.0
 * @date 2026-05-11
 */

#include "gcos_vm.h"
#include "gcos_platform.h"
#include <string.h>

/* ============================================================================
 * Transaction Constants
 * ============================================================================ */

#define MAX_TRANSACTION_DEPTH   4       /* Maximum nested transaction depth */
#define BACKUP_MAGIC            0x54584E00  /* "TXN" magic number */

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Transaction backup record
 */
typedef struct {
    u32 magic;                      /* Backup magic number */
    u32 heap_backup[GCOS_HEAP_SIZE / 4];    /* Heap backup (as u32 array) */
    u8 global_data_backup[GCOS_GLOBAL_DATA_SIZE];  /* Global data backup */
    u32 heap_used_backup;           /* Heap usage backup */
    u32 global_data_used_backup;    /* Global data usage backup */
} TransactionBackup;

/* ============================================================================
 * Internal State
 * ============================================================================ */

static TransactionBackup g_transaction_backups[MAX_TRANSACTION_DEPTH];
static u8 g_transaction_depth = 0;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Create backup of current state
 * @param vm VM instance
 * @param backup Backup structure pointer
 */
static void create_backup(const GCOSVM *vm, TransactionBackup *backup) {
    if (vm == NULL || backup == NULL) {
        return;
    }
    
    backup->magic = BACKUP_MAGIC;
    
    /* Backup heap */
    memcpy(backup->heap_backup, vm->runtime.heap, sizeof(vm->runtime.heap));
    backup->heap_used_backup = vm->runtime.heap_used;
    
    /* Backup global data */
    memcpy(backup->global_data_backup, vm->runtime.global_data, sizeof(vm->runtime.global_data));
    backup->global_data_used_backup = vm->runtime.global_data_used;
    
    GCOS_PRINTF("[Transaction] Backup created at depth %u\n", g_transaction_depth);
}

/**
 * @brief Restore state from backup
 * @param vm VM instance
 * @param backup Backup structure pointer
 */
static void restore_backup(GCOSVM *vm, const TransactionBackup *backup) {
    if (vm == NULL || backup == NULL || backup->magic != BACKUP_MAGIC) {
        return;
    }
    
    /* Restore heap */
    memcpy(vm->runtime.heap, backup->heap_backup, sizeof(vm->runtime.heap));
    vm->runtime.heap_used = backup->heap_used_backup;
    
    /* Restore global data */
    memcpy(vm->runtime.global_data, backup->global_data_backup, sizeof(vm->runtime.global_data));
    vm->runtime.global_data_used = backup->global_data_used_backup;
    
    GCOS_PRINTF("[Transaction] State restored from depth %u\n", g_transaction_depth);
}

/**
 * @brief Clear backup data
 * @param backup Backup structure pointer
 */
static void clear_backup(TransactionBackup *backup) {
    if (backup == NULL) {
        return;
    }
    
    memset(backup, 0, sizeof(TransactionBackup));
    backup->magic = 0;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

GCOSResult gcos_vm_transaction_begin(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check transaction depth limit */
    if (g_transaction_depth >= MAX_TRANSACTION_DEPTH) {
        GCOS_PRINTF("[Transaction] Error: Maximum transaction depth exceeded (%u)\n",
                   g_transaction_depth);
        vm->runtime.exception = EXCEPTION_TRANSACTION_OVERFLOW;
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Check if already in exception state */
    if (vm->state == GCOS_STATE_EXCEPTION) {
        GCOS_PRINTF("[Transaction] Error: Cannot begin transaction in exception state\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Create backup before starting transaction */
    create_backup(vm, &g_transaction_backups[g_transaction_depth]);
    g_transaction_depth++;
    
    /* Set transaction state */
    vm->transaction.in_transaction = true;
    vm->transaction.transaction_depth = g_transaction_depth;
    
    GCOS_PRINTF("[Transaction] Begin transaction (depth=%u)\n", g_transaction_depth);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_transaction_commit(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if in transaction */
    if (!vm->transaction.in_transaction || g_transaction_depth == 0) {
        GCOS_PRINTF("[Transaction] Error: No active transaction to commit\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Decrement transaction depth */
    g_transaction_depth--;
    
    /* Clear the backup for this level */
    clear_backup(&g_transaction_backups[g_transaction_depth]);
    
    /* Update transaction state */
    if (g_transaction_depth == 0) {
        vm->transaction.in_transaction = false;
    }
    vm->transaction.transaction_depth = g_transaction_depth;
    
    GCOS_PRINTF("[Transaction] Commit transaction (remaining depth=%u)\n", g_transaction_depth);
    return GCOS_SUCCESS;
}

GCOSResult gcos_vm_transaction_abort(GCOSVM *vm) {
    if (vm == NULL) {
        return GCOS_ERR_INVALID_PARAM;
    }
    
    /* Check if in transaction */
    if (!vm->transaction.in_transaction || g_transaction_depth == 0) {
        GCOS_PRINTF("[Transaction] Error: No active transaction to abort\n");
        return GCOS_ERROR_INVALID_STATE;
    }
    
    /* Restore from backup */
    restore_backup(vm, &g_transaction_backups[g_transaction_depth - 1]);
    
    /* Clear all backups up to current depth */
    for (u8 i = 0; i < g_transaction_depth; i++) {
        clear_backup(&g_transaction_backups[i]);
    }
    
    /* Reset transaction state */
    g_transaction_depth = 0;
    vm->transaction.in_transaction = false;
    vm->transaction.transaction_depth = 0;
    
    GCOS_PRINTF("[Transaction] Abort transaction - state rolled back\n");
    return GCOS_SUCCESS;
}

bool gcos_vm_in_transaction(const GCOSVM *vm) {
    if (vm == NULL) {
        return false;
    }
    
    return vm->transaction.in_transaction;
}

u8 gcos_vm_get_transaction_depth(const GCOSVM *vm) {
    if (vm == NULL) {
        return 0;
    }
    
    return vm->transaction.transaction_depth;
}

/**
 * @brief Auto-abort transaction on exception
 * @param vm VM instance
 * @note Called when exception occurs during transaction
 */
void gcos_vm_transaction_auto_abort(GCOSVM *vm) {
    if (vm == NULL) {
        return;
    }
    
    if (vm->transaction.in_transaction) {
        GCOS_PRINTF("[Transaction] Auto-abort due to exception\n");
        gcos_vm_transaction_abort(vm);
    }
}
