// arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "elf.h"
#include "string.h"
#include "vm.h"

// arch/riscv/kernel/proc.c

extern void __dummy();

extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
 */
//extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
//extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

/**
 * new content for unit test of 2023 OS lab4
 */
/* In vmlinux.lds */
extern char _sramdisk[];
extern char _eramdisk[];
/* 定位uapp段 */
static char *uapp_start = _sramdisk;
static char *uapp_end = _eramdisk;

struct task_struct *get_current_task() {
    return current;
}

void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
             uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file){
    // 1. 为 task 分配一个新的 VMA
    struct vm_area_struct *vma = &task->vmas[task->vma_cnt++];
    // 2. 设置 VMA 的 vm_start, vm_end, vm_flags, vm_content_offset_in_file, vm_content_size_in_file
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_flags = flags;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
}

struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr){
    for(int i = 0; i < task->vma_cnt; i++){
        if(task->vmas[i].vm_start <= addr && addr < task->vmas[i].vm_end){
            return &task->vmas[i];
        }
    }
    return NULL;
}

void task_init() {
//    test_init(NR_TASKS);
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    idle = (struct task_struct *) kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    for (int i = 1; i < NR_TASKS; ++i) {
        /** LAB2 */
        task[i] = (struct task_struct *) kalloc();
        task[i]->state = TASK_RUNNING;
        // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
        //      task[i].counter  = task_test_counter[i];
        //      task[i].priority = task_test_priority[i];
        task[i]->counter = 1;//task_test_counter[i];
        task[i]->priority = 1;//task_test_priority[i];
        task[i]->pid = i;
        // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
        // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址
        task[i]->thread.ra = (uint64) __dummy;
        task[i]->thread.sp = (uint64) task[i] + PGSIZE;

        /** LAB4：在defs.h中添加了一些相关宏定义 */
        // 1. 对于每个进程，初始化刚刚在 thread_struct 中添加的三个变量：
        //    1.1. 将 sepc 设置为 USER_START
        //    （在elf里，sepc改为ehdr->e_entry）
        // task[i]->thread.sepc = USER_START;
        task[i]->thread.sepc = ((Elf64_Ehdr *) uapp_start)->e_entry;
        //    1.2. 配置 sstatus 中的 SPP（使得 sret 返回至 U-Mode）,
        //         SPIE （sret 之后开启中断）, SUM（S-Mode 可以访问 User 页面）
        task[i]->thread.sstatus = SPP(SPP_USER) | SPIE(1) | SUM(1);
        //    1.3. 将 sscratch 设置为 U-Mode 的 sp，其值为 USER_END
        //        （即，用户态栈被放置在 user space 的最后一个页面）
        task[i]->thread.sscratch = USER_END;

        // 1. 对于每个进程，创建属于它自己的页表
        task[i]->pgd = (pagetable_t) alloc_page();

        // 2. 为了避免 U-Mode 和 S-Mode 切换的时候切换页表,
        //    将内核页表 （ swapper_pg_dir ） 复制到每个进程的页表中
        memcpy((char *)task[i]->pgd, &swapper_pg_dir, PGSIZE);

        // 3. 将 uapp 所在的页面映射到每个进行的页表中
        // map_uapp_bin(task[i]);
        map_uapp_elf(task[i]);

        // 4. 设置用户态栈
        // 设置用户态栈。对每个用户态进程，其拥有两个栈：
        // 用户态栈和内核态栈
        // 其中，内核态栈在 lab3 中我们已经设置好了, 可以通过 alloc_page 接口申请一个空的页面来作为用户态栈，
        // 并映射到进程的页表中

        do_mmap(task[i], USER_END - PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, 0, 0);
//        uint64 user_stack_addr = alloc_page();
//        create_mapping((uint64 *) task[i]->pgd, USER_END - PGSIZE, user_stack_addr - PA2VA_OFFSET, PGSIZE,
//                       PTE_USER | PTE_WRITE | PTE_READ | PTE_VALID);
        // 对于每个用户进程来说，它的栈地址是一致的，但是在物理空间下，每个用户栈存储的实际物理地址是有区别的
    }

    printk("...proc_init done!\n");
}

void map_uapp_bin(struct task_struct *t){
    // 将 uapp 所在的页面映射到每个进行的页表中。
    // 注意，在程序运行过程中，有部分数据不在栈上，而在初始化的过程中就已经被分配了空间
    // 所以，二进制文件需要先被拷贝到一块某个进程专用的内存之后
    // 再进行映射，防止所有的进程共享数据，造成预期外的进程间相互影响

    /* 将二进制文件需要拷贝到一块某个进程专用的内存 */
//    uint64 num_pages_to_copy = (uapp_end - uapp_start) / PGSIZE + 1;
//    uint64 pages_dest_addr = alloc_pages(num_pages_to_copy);
//    uint64 pages_src_addr = (uint64) uapp_start;
//    // p_offset：段内容的开始位置相对于文件开头的偏移量
//    memcpy((uint64 *)pages_dest_addr, (uint64 *)pages_src_addr, num_pages_to_copy * PGSIZE);
//
//    /* 映射 */
//    uint64 pages_perms = PTE_USER | PTE_EXECUTE | PTE_WRITE | PTE_READ | PTE_VALID;
//    // page table entry: 4|3|2|1|0
//    //                   U|X|W|R|V
//
//    create_mapping((uint64 *) t->pgd, (uint64) USER_START,
//                   pages_dest_addr - PA2VA_OFFSET, num_pages_to_copy * PGSIZE, pages_perms);
}

void map_uapp_elf(struct task_struct *t) {
    // 将 uapp 所在的页面映射到每个进行的页表中。
    // 注意，在程序运行过程中，有部分数据不在栈上，而在初始化的过程中就已经被分配了空间
    // 所以，二进制文件需要先被拷贝到一块某个进程专用的内存之后
    // 再进行映射，防止所有的进程共享数据，造成预期外的进程间相互影响
    // ELF简要布局：https://zhuanlan.zhihu.com/p/286088470
    // ELF64_Phdr详解：https://zhuanlan.zhihu.com/p/389408697

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *) uapp_start; // 获取ELF文件头

    // 获取ELF进程文件头序列首元素（e_phoff：ELF程序文件头数组相对ELF文件头的偏移地址）
    uint64 phdr_start = (uint64) ehdr + ehdr->e_phoff;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr_curr = (Elf64_Phdr *)(phdr_start + i * sizeof(Elf64_Phdr));
        // Elf64_Phdr *phdr_curr = phdr_start + (i * sizeof(Elf64_Phdr));
        // ELF程序文件头数组下标为 i 的元素，每个元素大小为 ehdr->e_phentsize（56B）

        /* 拷贝已被分配空间的数据与原页中不属于程序文件头的部分：
         * 否则，新分配页中数据的ofs与原页中数据的ofs不匹配，会导致找不到需要的数据
         * 
         * 补充：PT_LOAD（Program Header Type-Loadable）：此类型表明本程序头指向一个可装载的段。
         * 段的内容会被从文件中拷贝到内存中。段在文件中的大小是 p_filesz，在内存中的大小是 p_memsz。
         * 如果 p_memsz 大于 p_filesz，在内存中多出的存储空间应填 0 补充 */
        if (phdr_curr->p_type == PT_LOAD) {
            do_mmap(t, phdr_curr->p_vaddr, phdr_curr->p_memsz, phdr_curr->p_flags << 1,
                    phdr_curr->p_offset, phdr_curr->p_filesz);
//            uint64 vaddr_round = (uint64) (phdr_curr->p_vaddr) - PGROUNDDOWN(phdr_curr->p_vaddr);

//            uint64 num_pages_to_copy = (vaddr_round + phdr_curr->p_memsz) / PGSIZE + 1;
//            uint64 pages_dest_addr = alloc_pages(num_pages_to_copy);
//            uint64 pages_src_addr = (uint64) (uapp_start) + phdr_curr->p_offset;
            // p_offset：段内容的开始位置相对于文件开头的偏移量
//            memcpy((uint64 *)(pages_dest_addr + vaddr_round), (uint64 *)pages_src_addr, phdr_curr->p_memsz);

//            uint64 perms = phdr_curr->p_flags;
//            uint64 perm_r = (perms & 4) >> 1, perms_w = (perms & 2) << 1, perm_x = (perms & 1) << 3;
//            uint64 pages_perms = PTE_USER | perm_x | perms_w | perm_r | PTE_VALID;
            // p_flags: 2|1|0   page table entry: 4|3|2|1|0
            //          R|W|X                     U|X|W|R|V

//            create_mapping((uint64 *) t->pgd, (uint64)PGROUNDDOWN(phdr_curr->p_vaddr),
//                           pages_dest_addr - PA2VA_OFFSET, num_pages_to_copy * PGSIZE, pages_perms);
        }
    }
}

void dummy() {
//    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while (1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if (current->counter == 1) {
                --(current->counter); // forced the counter to be zero if this thread is going to be scheduled
            }                         // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d, counter = %d\n", current->pid, auto_inc_local_var,
                   current->counter);
        }
    }
}

extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next) {
    if (current == next)
        return;

//    printk("switch from [PID = %d] to [PID = %d]\n", (int) current->pid, (int) next->pid);

    struct task_struct *prev = current;
    current = next;
    __switch_to(prev, next);
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度

    // printk("current pid: %d, counter: %d\n", (int) current->pid, (int) current->counter);
    if (current == idle)
        schedule();
    else {
        if (current->counter > 0)
            current->counter--;
        if (current->counter <= 0)
            schedule();
    }
}

#ifdef SJF
void schedule()
{
    printk("SJF schedule\n");

    struct task_struct *next = idle;
    int counter = 0x7fffffff;
    int zero = current->counter == 0;
    for (int i = 1; i < NR_TASKS; ++i)
    {
        if (task[i]->counter > 0 && task[i]->counter < counter)
        {
            next = task[i];
            counter = task[i]->counter;
        }
        if (zero && task[i]->counter != 0)
            zero = 0;
    }

    if (zero)
    {
        for (int i = 1; i < NR_TASKS; ++i)
        {
            task[i]->counter = rand() % 10 + 1;
        }
        schedule();
        return;
    }

    switch_to(next);
}
#endif

#ifdef PRIORITY

void schedule() {
    // printk("PRIORITY schedule\n");

    uint64 i, next, c;
    struct task_struct **p;

    while (1) {
        c = 0;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];
        while (--i) {
            if (!*--p)
                continue;
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (int) (*p)->counter, next = i;
        }
        if (c)
            break;
        for (p = &task[NR_TASKS - 1]; p > &task[0]; --p)
            if (*p)
                (*p)->counter = ((*p)->counter >> 1) +
                                (*p)->priority;
    }
    switch_to(task[next]);
}

#endif

#ifndef SJF
#ifndef PRIORITY
void schedule()
{
    printk("not defined");
}
#endif
#endif
