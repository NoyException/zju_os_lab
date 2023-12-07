//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "string.h"
#include "vm.h"

//arch/riscv/kernel/proc.c

extern void __dummy();
extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

void task_init() {
    test_init(NR_TASKS);
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;

    for (int i = 1; i < NR_TASKS; ++i) {
        /** LAB2 */
        // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
        // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
        //      task[i].counter  = task_test_counter[i];
        //      task[i].priority = task_test_priority[i];
        // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
        // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址
        task[i] = (struct task_struct*)kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = task_test_counter[i];
        task[i]->priority = task_test_priority[i];
        task[i]->pid = i;
        task[i]->thread.ra = (uint64)__dummy;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;

        /** LAB4：在defs.h中添加了一些相关宏定义 */
        // 1. 对于每个进程，初始化刚刚在 thread_struct 中添加的三个变量：
        //    1.1. 将 sepc 设置为 USER_START
        //    1.2. 配置 sstatus 中的 SPP（使得 sret 返回至 U-Mode）,
        //         SPIE （sret 之后开启中断）, SUM（S-Mode 可以访问 User 页面）
        //    1.3. 将 sscratch 设置为 U-Mode 的 sp，其值为 USER_END
        //        （即，用户态栈被放置在 user space 的最后一个页面）
        // 2. 对于每个进程，创建属于它自己的页表
        // 3. 为了避免 U-Mode 和 S-Mode 切换的时候切换页表,
        //    将内核页表 （ swapper_pg_dir ） 复制到每个进程的页表中
        /** TODO: */
        // 4. 将 uapp 所在的页面映射到每个进行的页表中。
        //    注意，在程序运行过程中，有部分数据不在栈上，而在初始化的过程中就已经被分配了空间
        //    （比如我们的 uapp 中的 counter 变量）。所以，二进制文件需要先被拷贝到一块某个进程专用的内存之后
        //    再进行映射，防止所有的进程共享数据，造成预期外的进程间相互影响
        // 5. 设置用户态栈。对每个用户态进程，其拥有两个栈：
        //    用户态栈和内核态栈
        //    其中，内核态栈在 lab3 中我们已经设置好了, 可以通过 alloc_page 接口申请一个空的页面来作为用户态栈，
        //    并映射到进程的页表中
        task[i]->thread.sepc = USER_START;
        task[i]->thread.sstatus = SPP(SPP_USER) | SPIE(1) | SUM(1);
        task[i]->thread.sscratch = USER_END;

        task[i]->pgd = (pagetable_t)alloc_page();

        memcpy(task[i]->pgd, &swapper_pg_dir, PGSIZE);

        /** TODO: */
        // 4. 将 uapp 所在的页面映射到每个进行的页表中。
//        create_mapping(task[i]->pgd, USER_START, , USER_END - USER_START, 0B1111);
    }

    printk("...proc_init done!\n");
}

// arch/riscv/kernel/proc.c
void dummy() {
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d, counter = %d\n", current->pid, auto_inc_local_var, current->counter);
        }
    }
}

extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next) {
    if(current == next) return;

    printk("switch from [PID = %d] to [PID = %d]\n", (int)current->pid, (int)next->pid);

    struct task_struct* prev = current;
    current = next;
    __switch_to(prev, next);
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    printk("current pid: %d, counter: %d\n", (int)current->pid, (int)current->counter);
    if(current == idle)
        schedule();
    else {
        if(current->counter>0)
            current->counter--;
        if(current->counter<=0)
            schedule();
    }
}

#ifdef SJF
void schedule(){
    printk("SJF schedule\n");

    struct task_struct* next = idle;
    int counter = 0x7fffffff;
    int zero = current->counter==0;
    for (int i = 1; i < NR_TASKS; ++i) {
        if(task[i]->counter > 0 && task[i]->counter < counter){
            next = task[i];
            counter = task[i]->counter;
        }
        if(zero && task[i]->counter != 0)
            zero = 0;
    }

    if(zero){
        for (int i = 1; i < NR_TASKS; ++i) {
            task[i]->counter = rand() % 10 + 1;
        }
        schedule();
        return;
    }

    switch_to(next);
}
#endif

#ifdef PRIORITY
void schedule(){
    printk("PRIORITY schedule\n");

    uint64 i,next,c;
	struct task_struct ** p;

	while (1) {
		c = 0;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (int)(*p)->counter, next = i;
		}
		if (c) break;
        for(p = &task[NR_TASKS-1] ; p > &task[0] ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(task[next]);
}

#endif

#ifndef SJF
#ifndef PRIORITY
void schedule(){
    printk("not defined");
}
#endif
#endif
