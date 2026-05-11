/**
 * @file vm_transaction_full.c
 * @brief GCOS VM 事务管理器完整实现
 * 
 * 实现基于COS3规范的事务管理机制，包括：
 * - 事务开始/提交/回滚
 * - 数据备份和恢复
 * - 嵌套事务支持
 * - 原子性保证
 */

#include "vm_core.h"
#include "vm_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 事务管理器状态
 * ============================================================================ */

typedef struct {
    bool active;
    u8 nesting_level;
    u8 *backup_data;
    u32 backup_size;
    u32 checkpoint_count;
    u32 max_checkpoint_count;
} TransactionManager;

static TransactionManager trans_mgr = {
    .active = false,
    .nesting_level = 0,
    .backup_data = NULL,
    .backup_size = 0,
    .checkpoint_count = 0,
    .max_checkpoint_count = 16
};

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/**
 * @brief 创建数据备份
 */
static int create_backup(VMContext *vm) {
    /* 计算需要备份的数据大小 */
    u32 backup_size = 0;

    /* 备份应用域数据 */
    if (vm->current_app != NULL) {
        if (vm->current_app->app_domain_data != NULL) {
            backup_size += VM_HEAP_SIZE; /* 最大可能大小 */
        }
        if (vm->current_app->ref_domain_data != NULL) {
            backup_size += VM_HEAP_SIZE;
        }
        if (vm->current_app->persistent_data != NULL) {
            backup_size += VM_HEAP_SIZE;
        }
    }

    /* 备份模块域数据 */
    if (vm->current_module != NULL) {
        if (vm->current_module->domain_data != NULL) {
            backup_size += VM_HEAP_SIZE;
        }
    }

    if (backup_size == 0) {
        return 0;
    }

    /* 分配备份缓冲区 */
    if (trans_mgr.backup_data != NULL) {
        trans_mgr.backup_data = (u8*)malloc(backup_size);
        if (trans_mgr.backup_data == NULL) {
            return -1;
        }
        trans_mgr.backup_size = backup_size;
    } else if (backup_size > trans_mgr.backup_size) {
        /* 需要更大的备份空间 */
        u8 *new_backup = (u8*)realloc(trans_mgr.backup_data, backup_size);
        if (new_backup == NULL) {
            return -2;
        }
        trans_mgr.backup_data = new_backup;
        trans_mgr.backup_size = backup_size;
    }

    /* 执行备份 */
    u32 offset = 0;

    if (vm->current_app != NULL) {
        if (vm->current_app->app_domain_data != NULL) {
            memcpy(trans_mgr.backup_data + offset, vm->current_app->app_domain_data, 
                   VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
        if (vm->current_app->ref_domain_data != NULL) {
            memcpy(trans_mgr.backup_data + offset, vm->current_app->ref_domain_data,
                   VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
        if (vm->current_app->persistent_data != NULL) {
            memcpy(trans_mgr.backup_data + offset, vm->current_app->persistent_data,
                   VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
    }

    if (vm->current_module != NULL) {
        if (vm->current_module->domain_data != NULL) {
            memcpy(trans_mgr.backup_data + offset, vm->current_module->domain_data,
                   VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
    }

    trans_mgr.checkpoint_count++;
    return 0;
}

/**
 * @brief 恢复数据备份
 */
static int restore_backup(VMContext *vm) {
    if (trans_mgr.backup_data == NULL || trans_mgr.checkpoint_count == 0) {
        return -1;
    }

    u32 offset = 0;

    /* 恢复应用域数据 */
    if (vm->current_app != NULL) {
        if (vm->current_app->app_domain_data != NULL) {
            memcpy(vm->current_app->app_domain_data,
                   trans_mgr.backup_data + offset, VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
        if (vm->current_app->ref_domain_data != NULL) {
            memcpy(vm->current_app->ref_domain_data,
                   trans_mgr.backup_data + offset, VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
        if (vm->current_app->persistent_data != NULL) {
            memcpy(vm->current_app->persistent_data,
                   trans_mgr.backup_data + offset, VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
    }

    /* 恢复模块域数据 */
    if (vm->current_module != NULL) {
        if (vm->current_module->domain_data != NULL) {
            memcpy(vm->current_module->domain_data,
                   trans_mgr.backup_data + offset, VM_HEAP_SIZE);
            offset += VM_HEAP_SIZE;
        }
    }

    trans_mgr.checkpoint_count--;
    return 0;
}

/**
 * @brief 验证事务状态
 */
static int validate_transaction(VMContext *vm) {
    if (!trans_mgr.active) {
        printf("Error: No active transaction\n");
        return -1;
    }

    if (trans_mgr.nesting_level == 0) {
        printf("Error: Transaction nesting level is 0\n");
        return -2;
    }

    return 0;
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

/**
 * @brief 初始化事务管理器
 */
int vm_transaction_init(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    memset(&trans_mgr, 0, sizeof(TransactionManager));
    trans_mgr.active = false;
    trans_mgr.nesting_level = 0;
    trans_mgr.backup_data = NULL;
    trans_mgr.backup_size = 0;
    trans_mgr.checkpoint_count = 0;
    trans_mgr.max_checkpoint_count = 16;

    /* 初始化VM中的事务上下文 */
    vm->transaction.active = false;
    vm->transaction.backup_data = NULL;
    vm->transaction.backup_size = 0;
    vm->transaction.checkpoint_count = 0;

    return 0;
}

/**
 * @brief 开始事务
 */
int vm_transaction_begin(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    /* 检查嵌套层级 */
    if (trans_mgr.nesting_level >= trans_mgr.max_checkpoint_count) {
        printf("Error: Maximum transaction nesting level reached\n");
        vm->exception = EXCEPTION_TRANSACTION_ABORT;
        return -2;
    }

    /* 创建检查点 */
    int ret = create_backup(vm);
    if (ret != 0) {
        printf("Error: Failed to create transaction backup\n");
        vm->exception = EXCEPTION_OUT_OF_MEMORY;
        return -3;
    }

    /* 增加嵌套层级 */
    trans_mgr.nesting_level++;
    trans_mgr.active = true;

    /* 更新VM中的事务上下文 */
    vm->transaction.active = true;
    vm->transaction.backup_data = trans_mgr.backup_data;
    vm->transaction.backup_size = trans_mgr.backup_size;
    vm->transaction.checkpoint_count = trans_mgr.checkpoint_count;

    printf("Transaction begun: level=%u, checkpoints=%u\n",
           trans_mgr.nesting_level, trans_mgr.checkpoint_count);

    return 0;
}

/**
 * @brief 提交事务
 */
int vm_transaction_commit(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    /* 验证事务状态 */
    int ret = validate_transaction(vm);
    if (ret != 0) {
        return ret;
    }

    printf("Transaction commit: level=%u, checkpoints=%u\n",
           trans_mgr.nesting_level, trans_mgr.checkpoint_count);

    /* 减少嵌套层级 */
    trans_mgr.nesting_level--;

    /* 检查是否还有嵌套事务 */
    if (trans_mgr.nesting_level == 0) {
        /* 最外层事务提交，释放备份 */
        if (trans_mgr.backup_data != NULL) {
            free(trans_mgr.backup_data);
            trans_mgr.backup_data = NULL;
            trans_mgr.backup_size = 0;
        }
        trans_mgr.active = false;
        trans_mgr.checkpoint_count = 0;
    }

    /* 更新VM中的事务上下文 */
    vm->transaction.active = trans_mgr.active;
    vm->transaction.backup_data = trans_mgr.backup_data;
    vm->transaction.backup_size = trans_mgr.backup_size;
    vm->transaction.checkpoint_count = trans_mgr.checkpoint_count;

    return 0;
}

/**
 * @brief 回滚事务
 */
int vm_transaction_rollback(VMContext *vm) {
    if (vm == NULL) {
        return -1;
    }

    /* 验证事务状态 */
    int ret = validate_transaction(vm);
    if (ret != 0) {
        return ret;
    }

    printf("Transaction rollback: level=%u, checkpoints=%u\n",
           trans_mgr.nesting_level, trans_mgr.checkpoint_count);

    /* 恢复到上一个检查点 */
    ret = restore_backup(vm);
    if (ret != 0) {
        printf("Error: Failed to restore transaction backup\n");
        return -2;
    }

    /* 减少嵌套层级 */
    trans_mgr.nesting_level--;

    /* 检查是否还有嵌套事务 */
    if (trans_mgr.nesting_level == 0) {
        /* 最外层事务回滚，释放备份 */
        if (trans_mgr.backup_data != NULL) {
            free(trans_mgr.backup_data);
            trans_mgr.backup_data = NULL;
            trans_mgr.backup_size = 0;
        }
        trans_mgr.active = false;
        trans_mgr.checkpoint_count = 0;
    }

    /* 更新VM中的事务上下文 */
    vm->transaction.active = trans_mgr.active;
    vm->transaction.backup_data = trans_mgr.backup_data;
    vm->transaction.backup_size = trans_mgr.backup_size;
    vm->transaction.checkpoint_count = trans_mgr.checkpoint_count;

    return 0;
}

/**
 * @brief 获取事务状态
 */
bool vm_transaction_is_active(const VMContext *vm) {
    if (vm == NULL) {
        return false;
    }
    return vm->transaction.active;
}

/**
 * @brief 获取嵌套层级
 */
u8 vm_transaction_get_nesting_level(const VMContext *vm) {
    if (vm == NULL) {
        return 0;
    }
    return trans_mgr.nesting_level;
}

/**
 * @brief 获取检查点数量
 */
u32 vm_transaction_get_checkpoint_count(const VMContext *vm) {
    if (vm == NULL) {
        return 0;
    }
    return trans_mgr.checkpoint_count;
}

/**
 * @brief 清理事务管理器
 */
void vm_transaction_cleanup(VMContext *vm) {
    if (trans_mgr.backup_data != NULL) {
        free(trans_mgr.backup_data);
        trans_mgr.backup_data = NULL;
        trans_mgr.backup_size = 0;
    }

    trans_mgr.active = false;
    trans_mgr.nesting_level = 0;
    trans_mgr.checkpoint_count = 0;

    if (vm != NULL) {
        vm->transaction.active = false;
        vm->transaction.backup_data = NULL;
        vm->transaction.backup_size = 0;
        vm->transaction.checkpoint_count = 0;
    }
}
