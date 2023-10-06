//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"

//arch/riscv/kernel/proc.c

extern void __dummy();

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/

char task_test_char[NR_TASKS];          // TEST_SCHEDULE 测试中，各个 TASK 输出的字符
uint64 task_test_priority[NR_TASKS];    // TEST_SCHEDULE 测试中，各个 TASK 的 priority 已经被事先定好
uint64 task_test_counter[NR_TASKS];     // TEST_SCHEDULE 测试中，各个 TASK 的 counter 已经被事先定好
uint64 task_test_index = 0;             // TEST_SCHEDULE 测试中，输出到 task_test_output 中的字符数量
char task_test_output[NR_TASKS + 1];    // TEST_SCHEDULE 测试中，各个 TASK 输出字符的全局 buffer

void task_init() {

#ifdef TEST_SCHEDULE
    test_init(NR_TASKS);
#endif
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    /* YOUR CODE HERE */

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该线程在线程数组中的下标。
    //      但如果 TEST_SCHEDULE 宏已经被 define，那么为了单元测试的需要，进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    /* YOUR CODE HERE */

    printk("...proc_init done!\n");
}


void dummy() {
#ifndef TEST_SCHEDULE
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if (last_counter == -1 || current->counter != last_counter) {
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
        }
    }
#else
    printk("%c\n", task_test_char[current->pid]);
    task_test_output[task_test_index++] = task_test_char[current->pid];
    
    if(task_test_index == NR_TASKS - 1){
        task_test_index++;
        printk("Test Done, output = %s\n", task_test_output);

        uint64 num_tasks = NR_TASKS;
        char priority_52[] = "GoHDpgvlQFCfBIxJTayLnkEeZKjiWqXUNrPSdYhtbuwzmVMORcs";
        char priority_32[] = "GHDQFCfBIJTaLEeZKWXUNPSdYbVMORc";
        char priority_16[] = "GHDFCBIJLEKNPMO";
        char priority_8[]  = "GHDFCBE";
        char priority_4[]  = "DCB";

        char SJF_52[] = "scROMVmzwubthYdSPrNUXqWijKZeEknLyaTJxIBfCFQlvgpDHoG";
        char SJF_32[] = "cROMVbYdSPNUXWKZeELaTJIBfCFQDHG";
        char SJF_16[] = "OMPNKELJIBCFDHG";
        char SJF_8[]  = "EBCFDHG";
        char SJF_4[]  = "BCD";

        char * priority_output;
        char * SJF_output;

        switch(num_tasks){
            case 52 :
                priority_output = priority_52;
                SJF_output = SJF_52;
                break;
            case 32 :
                priority_output = priority_32;
                SJF_output = SJF_32;
                break;
            case 16 :
                priority_output = priority_16;
                SJF_output = SJF_16;
                break;
            case 8 :
                priority_output = priority_8;
                SJF_output = SJF_8;
                break;
            case 4 :
                priority_output = priority_4;
                SJF_output = SJF_4;
                break;
            default :
                printk("unknown test case\n");
                goto stuck;
        }
        #ifdef SJF
            for(int i = 0; i < NR_TASKS - 1; i++){
                if(SJF_output[i] != task_test_output[i]){
                    printk("NR_TASKS = %d, SJF test failed!\n\n", NR_TASKS);
                    goto stuck;
                }
            }
            printk("NR_TASKS = %d, SJF test passed!\n\n", NR_TASKS);
            goto stuck;
        #else
            for(int i = 0; i < NR_TASKS - 1; i++){
                if(priority_output[i] != task_test_output[i]){
                    printk("NR_TASKS = %d, PRIORITY test failed!\n\n", NR_TASKS);
                    goto stuck;
                }
            }
            printk("NR_TASKS = %d, PRIORITY test passed!\n\n", NR_TASKS);
            goto stuck;
        #endif
    }
stuck:    
    current->counter = 0;
    while(1);
#endif
}


extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next) {
    /* YOUR CODE HERE */
}


void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    /* YOUR CODE HERE */
}


void schedule(void) {
    /* YOUR CODE HERE */
}

#ifdef TEST_SCHEDULE
void test_init(int num_tasks) {
    char init_char = 'A';
    uint64 priority = PRIORITY_SEED;
    uint64 counter = COUNTER_SEED;

    for (char i = 0; i < num_tasks; i++){
        if(i < 26){
            task_test_char[i] = init_char + i;
        }
        else{
            task_test_char[i] = init_char + (i - 26) + 0x20;
        }

        priority = (priority * 5) % 97;
        task_test_priority[i] = priority;

        counter = (counter * 5) % 193;
        task_test_counter[i] = counter;

        task_test_output[i] = '\0';
    }
    task_test_output[num_tasks]  = '\0';
}
#endif