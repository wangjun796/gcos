/**
 * @file main_test.c
 * @brief GCOS VM 主测试程序
 * 
 * 测试虚拟机的核心功能：
 * - 虚拟机创建和初始化
 * - 模块加载
 * - 应用安装和选择
 * - 基本指令执行
 * - 事务管理
 */

#include "gcos_vm_full.h"
#include "gcos_vm_full_part2.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * 测试辅助函数
 * ============================================================================ */

/**
 * @brief 打印测试结果
 */
static void print_test_result(const char *test_name, int result) {
    printf("[%s] ", test_name);
    if (result == GCOS_OK) {
        printf("PASSED\n");
    } else {
        printf("FAILED (error code: %d)\n", result);
    }
}

/**
 * @brief 打印分隔线
 */
static void print_separator(void) {
    printf("========================================\n");
}

/* ============================================================================
 * 测试函数
 * ============================================================================ */

/**
 * @brief 测试虚拟机创建和初始化
 */
static int test_vm_creation(void) {
    print_separator();
    printf("Test: VM Creation and Initialization\n");
    print_separator();

    /* 创建虚拟机 */
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("ERROR: Failed to create VM\n");
        return -1;
    }

    /* 初始化虚拟机 */
    GCOSResult ret = gcos_vm_init(vm);
    if (ret != GCOS_OK) {
        printf("ERROR: Failed to initialize VM\n");
        gcos_vm_destroy(vm);
        return -2;
    }

    print_test_result("VM Creation and Initialization", 0);
    print_separator();

    /* 打印虚拟机信息 */
    gcos_vm_print_info(vm);

    /* 清理 */
    gcos_vm_destroy(vm);
    return 0;
}

/**
 * @brief 测试模块加载
 */
static int test_module_loading(void) {
    print_separator();
    printf("Test: Module Loading\n");
    print_separator();

    /* 创建虚拟机 */
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("ERROR: Failed to create VM\n");
        return -1;
    }

    if (gcos_vm_init(vm) != GCOS_OK) {
        printf("ERROR: Failed to initialize VM\n");
        gcos_vm_destroy(vm);
        return -2;
    }

    /* 模拟一个简单的SEF文件 */
    u8 sef_data[] = {
        /* SEF文件头 */
        0x66, 0x65, 0x66, 0x00,  /* "sef" */
        0x01, 0x00, 0x00, 0x00,  /* 版本号 */

        /* 首段 */
        0x01, 0x00, 0x00, 0x00,  /* 段ID */
        0x1A, 0x00, 0x00, 0x00,  /* 段大小 */

        /* 首段内容 */
        0x01, 0x00, 0x00, 0x00,  /* SEF版本 */
        0x05, 0x00, 0x00, 0x00,  /* SEF AID长度 */
        0xA0, 0x00, 0x01, 0x00,  /* SEF AID */

        /* 段信息 */
        0x00, 0x00, 0x00, 0x00,  /* 导入模块数 */
        0x00, 0x00, 0x00, 0x00,  /* 导入函数数 */
        0x01, 0x00, 0x00, 0x00,  /* 应用数 */

        /* 函数段长度 */
        0x00, 0x10, 0x00, 0x00,

        /* 元素段长度 */
        0x00, 0x00, 0x00, 0x00,

        /* 数据段长度 */
        0x00, 0x00, 0x00, 0x00,

        /* 代码段长度 */
        0x00, 0x00, 0x10, 0x00
    };

    /* 加载模块 */
    u8 module_index;
    GCOSResult ret = gcos_vm_load_module(vm, sef_data, sizeof(sef_data), &module_index);

    print_test_result("Module Loading", ret);
    print_separator();

    if (ret == GCOS_OK) {
        printf("Module loaded successfully: index=%u\n", module_index);
        gcos_vm_print_module_info(vm, module_index);
    }

    gcos_vm_destroy(vm);
    return (ret == GCOS_OK) ? 0 : -1;
}

/**
 * @brief 测试应用管理
 */
static int test_app_management(void) {
    print_separator();
    printf("Test: Application Management\n");
    print_separator();

    /* 创建虚拟机 */
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("ERROR: Failed to create VM\n");
        return -1;
    }

    if (gcos_vm_init(vm) != GCOS_OK) {
        printf("ERROR: Failed to initialize VM\n");
        gcos_vm_destroy(vm);
        return -2;
    }

    /* 模拟加载模块 */
    u8 sef_data[] = {
        0x66, 0x65, 0x66, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00,
        0xA0, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x10, 0x00
    };

    u8 module_index;
    GCOSResult ret = gcos_vm_load_module(vm, sef_data, sizeof(sef_data), &module_index);
    if (ret != GCOS_OK) {
        printf("ERROR: Failed to load module\n");
        gcos_vm_destroy(vm);
        return -1;
    }

    /* 安装应用 */
    GCOSAID app_aid = {.length = 5, .aid = {0xA0, 0x00, 0x01, 0x00, 0x00, 0x00}};
    ret = gcos_vm_install_app(vm, module_index, &app_aid);
    print_test_result("App Installation", ret);

    if (ret == GCOS_OK) {
        /* 选择应用 */
        ret = gcos_vm_select_app(vm, 0, &app_aid);
        print_test_result("App Selection", ret);
    }

    /* 取消选择应用 */
    if (ret == GCOS_OK) {
        ret = gcos_vm_deselect_app(vm, 0);
        print_test_result("App Deselection", ret);
    }

    gcos_vm_destroy(vm);
    return 0;
}

/**
 * @brief 测试事务管理
 */
static int test_transaction_management(void) {
    print_separator();
    printf("Test: Transaction Management\n");
    print_separator();

    /* 创建虚拟机 */
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("ERROR: Failed to create VM\n");
        return -1;
    }

    if (gcos_vm_init(vm) != GCOS_OK) {
        printf("ERROR: Failed to initialize VM\n");
        gcos_vm_destroy(vm);
        return -2;
    }

    /* 测试事务操作 */
    GCOSResult ret;

    /* 开始事务 */
    ret = gcos_vm_transaction_begin(vm);
    print_test_result("Transaction Begin", ret);

    if (ret == GCOS_OK) {
        /* 提交事务 */
        ret = gcos_vm_transaction_commit(vm);
        print_test_result("Transaction Commit", ret);

        /* 测试嵌套事务 */
        ret = gcos_vm_transaction_begin(vm);
        print_test_result("Nested Transaction Begin", ret);

        if (ret == GCOS_OK) {
            /* 回滚嵌套事务 */
            ret = gcos_vm_transaction_rollback(vm);
            print_test_result("Nested Transaction Rollback", ret);

            /* 提交外层事务 */
            ret = gcos_vm_transaction_commit(vm);
            print_test_result("Outer Transaction Commit", ret);
        }
    }

    gcos_vm_destroy(vm);
    return 0;
}

/**
 * @brief 测试指令执行
 */
static int test_instruction_execution(void) {
    print_separator();
    printf("Test: Instruction Execution\n");
    print_separator();

    /* 创建虚拟机 */
    GCOSVM *vm = gcos_vm_create();
    if (vm == NULL) {
        printf("ERROR: Failed to create VM\n");
        return -1;
    }

    if (gcos_vm_init(vm) != GCOS_OK) {
        printf("ERROR: Failed to initialize VM\n");
        gcos_vm_destroy(vm);
        return -2;
    }

    /* 加载简单模块 */
    u8 sef_data[] = {
        0x66, 0x65, 0x66, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00,
        0xA0, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x10, 0x00
    };

    u8 module_index;
    GCOSResult ret = gcos_vm_load_module(vm, sef_data, sizeof(sef_data), &module_index);
    if (ret != GCOS_OK) {
        printf("ERROR: Failed to load module\n");
        gcos_vm_destroy(vm);
        return -1;
    }

    /* 执行一些基本指令 */
    printf("Executing basic instructions...\n");

    /* 测试NOP指令 */
    u8 opcode = 0x01; /* OP_NOP */
    ret = gcos_execute_instruction(vm, opcode, NULL, 0);
    printf("NOP: %d\n", ret);

    /* 测试常量加载指令 */
    opcode = 0x40; /* OP_LDC_0 */
    u32 operands[1] = {0};
    ret = gcos_execute_instruction(vm, opcode, operands, 1);
    printf("LDC.0: %d\n", ret);

    /* 测试算术指令 */
    opcode = 0x60; /* OP_ADD */
    u32 operands2[2];
    ret = gcos_execute_instruction(vm, opcode, operands2, 2);
    printf("ADD: %d\n", ret);

    /* 测试跳转指令 */
    opcode = 0x10; /* OP_BR_S8 */
    u32 operands3[1] = {0x02}; /* 向前跳转2字节 */
    ret = gcos_execute_instruction(vm, opcode, operands3, 1);
    printf("BR.S8: %d\n", ret);

    print_test_result("Instruction Execution", ret);
    print_separator();

    /* 打印虚拟机状态 */
    gcos_vm_print_info(vm);

    gcos_vm_destroy(vm);
    return 0;
}

/* ============================================================================
 * 主函数
 * ============================================================================ */

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("GCOS VM Test Suite\n");
    printf("Version: " GCOS_VM_VERSION "\n");
    printf("========================================\n\n");

    int total_tests = 0;
    int passed_tests = 0;

    /* 运行所有测试 */
    if (test_vm_creation() == 0) {
        total_tests++;
        passed_tests++;
    }

    if (test_module_loading() == 0) {
        total_tests++;
        passed_tests++;
    }

    if (test_app_management() == 0) {
        total_tests++;
        passed_tests++;
    }

    if (test_transaction_management() == 0) {
        total_tests++;
        passed_tests++;
    }

    if (test_instruction_execution() == 0) {
        total_tests++;
        passed_tests++;
    }

    /* 打印测试总结 */
    print_separator();
    printf("Test Summary\n");
    print_separator();
    printf("Total Tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Success Rate: %.2f%%\n", (passed_tests * 100.0) / total_tests);
    print_separator();

    printf("\nPress any key to exit...\n");
    getchar();

    return (passed_tests == total_tests) ? 0 : 1;
}
