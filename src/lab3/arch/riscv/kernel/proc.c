//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"

//arch/riscv/kernel/proc.c

extern void __dummy();

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

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    for (int i = 1; i < NR_TASKS; ++i) {
        task[i] = (struct task_struct*)kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = task_test_counter[i];
        task[i]->priority = task_test_priority[i];
        task[i]->pid = i;
        task[i]->thread.ra = (uint64)__dummy;
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;
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
