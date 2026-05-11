#include "vm_core.h"
#include <stdio.h>

/**
 * GCOS虚拟机示例程序
 * 演示如何创建、配置和使用虚拟机
 */

int main(void) {
    printf("========================================\n");
    printf("GCOS VM Example Application\n");
    printf("========================================\n\n");
    
    /* 1. 创建虚拟机实例 */
    printf("[1] Creating VM instance...\n");
    VMContext *vm = vm_create();
    if (vm == NULL) {
        printf("Error: Failed to create VM\n");
        return -1;
    }
    printf("    VM created successfully\n\n");
    
    /* 2. 初始化虚拟机 */
    printf("[2] Initializing VM...\n");
    int ret = vm_init(vm);
    if (ret != 0) {
        printf("Error: Failed to initialize VM (code=%d)\n", ret);
        vm_destroy(vm);
        return -1;
    }
    printf("    VM initialized successfully\n\n");
    
    /* 3. 打印虚拟机信息 */
    printf("[3] VM Information:\n");
    vm_print_info(vm);
    printf("\n");
    
    /* 4. 打印内存映射 */
    printf("[4] Memory Map:\n");
    vm_print_memory_map(vm);
    printf("\n");
    
    /* 5. 模拟加载一个简单程序并执行 */
    printf("[5] Loading sample bytecode...\n");
    
    /* 创建一个简单的字节码程序：计算 1+2+3+...+10 */
    /* 这里只是示例，实际应该从SEF文件加载 */
    u8 sample_code[] = {
        OP_LDC_U8, 0,      /* 加载常数0（累加器） */
        OP_LDC_U8, 1,      /* 加载常数1（计数器） */
        OP_LDC_U8, 10,     /* 加载常数10（上限） */
        
        /* 循环开始 */
        OP_LDT_0,          /* 加载计数器 */
        OP_LDT_1,          /* 加载上限 */
        OP_GTU,            /* 比较：计数器 > 上限？ */
        OP_BNEZ_S8, 20,    /* 如果大于，跳转到结束 */
        
        OP_LDT_0,          /* 加载累加器 */
        OP_LDT_2,          /* 加载计数器 */
        OP_ADD,            /* 累加器 += 计数器 */
        OP_STT_0,          /* 存储累加器 */
        
        OP_LDT_2,          /* 加载计数器 */
        OP_LDC_U8, 1,      /* 加载常数1 */
        OP_ADD,            /* 计数器++ */
        OP_STT_2,          /* 存储计数器 */
        
        OP_BR_S8, -18,     /* 跳转回循环开始 */
        
        /* 循环结束 */
        OP_LDT_0,          /* 加载最终结果 */
        OP_RET             /* 返回 */
    };
    
    /* 将代码复制到模块代码区 */
    if (sizeof(sample_code) <= VM_MODULE_CODE_SIZE) {
        memcpy(vm->module_code, sample_code, sizeof(sample_code));
        printf("    Sample code loaded (%zu bytes)\n\n", sizeof(sample_code));
    } else {
        printf("    Error: Sample code too large\n\n");
    }
    
    /* 6. 执行虚拟机 */
    printf("[6] Executing VM...\n");
    ret = vm_execute(vm);
    if (ret != 0) {
        printf("    Execution failed (code=%d)\n", ret);
        printf("    Exception: %d\n", vm_get_exception(vm));
    } else {
        printf("    Execution completed successfully\n");
    }
    
    /* 7. 获取执行统计 */
    printf("\n[7] Execution Statistics:\n");
    u64 instr_count, exec_time;
    vm_get_stats(vm, &instr_count, &exec_time);
    printf("    Instructions executed: %llu\n", (unsigned long long)instr_count);
    printf("    Execution time: %llu us\n", (unsigned long long)exec_time);
    printf("\n");
    
    /* 8. 打印最终栈状态 */
    printf("[8] Final Stack State:\n");
    vm_print_stack(vm);
    printf("\n");
    
    /* 9. 清理资源 */
    printf("[9] Destroying VM...\n");
    vm_destroy(vm);
    printf("    VM destroyed\n\n");
    
    printf("========================================\n");
    printf("Example completed successfully!\n");
    printf("========================================\n");
    
    return 0;
}
