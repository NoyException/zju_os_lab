//
// Created by Noy on 2023/12/11.
//

#include "syscall.h"
#include "printk.h"
#include "mm.h"
#include "string.h"
#include "defs.h"
#include "vm.h"

/* Lab6 content */
extern void __ret_from_fork();                     // entry.S
extern uint64 swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

uint64 sys_write(unsigned int fd, const char *buf, size_t count) {
    if (fd == 1) {
        char str[count + 1];
        for (int i = 0; i < count; i++) {
            str[i] = buf[i];
        }
        str[count] = '\0';
        printk("%s", str);
    }
    return count;
}

uint64 sys_getpid() {
    return get_current_task()->pid;
}

uint64 sys_clone(struct pt_regs *regs) {
    /* 1. 参考 task_init 创建一个新的 task，将的 parent task 的整个页复制到新创建的
       task_struct 页上(这一步复制了哪些东西?）。将 thread.ra 设置为 __ret_from_fork */
    int i;
    for (i = 1; i < NR_TASKS; i++)
        if (get_task(i) == NULL) break;
    if (i == NR_TASKS) return 0;
    printk("[S] fork from current pid: %d, child pid: %d\n", get_current_task()->pid, i);

    struct task_struct *task_child = (struct task_struct *) kalloc();
    memcpy(task_child, get_current_task(), PGSIZE);
    task_child->pid = i;
    set_task(i, task_child);

    task_child->thread.ra = (uint64) __ret_from_fork;

    /* 2. 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
       并将其中的 a0, sp, sepc 设置成正确的值(为什么还要设置 sp?)，
       并正确设置 thread.sp（子进程内核态的sp）*/
    uint64 ofs_pt_regs = (uint64) regs - PGROUNDDOWN((uint64) regs);
    struct pt_regs *pt_regs_child = (struct pt_regs *) ((uint64) task_child + ofs_pt_regs);
    memcpy(pt_regs_child, regs, sizeof(struct pt_regs));
    pt_regs_child->gpr[9] = 0;                      // a0（子进程返回值）
    pt_regs_child->gpr[1] = (uint64) pt_regs_child;  // sp
    pt_regs_child->sepc = regs->sepc + 4;         // sepc（调用fork的下一条）

    task_child->thread.sp = (uint64) pt_regs_child;  // 需要通过sp来恢复所有寄存器的值（__ret_from_trap）

    /* 3. 为 child task 分配一个根页表，并仿照 setup_vm_final 来创建内核空间的映射 */
    task_child->pgd = (pagetable_t) kalloc();
    memcpy(task_child->pgd, swapper_pg_dir, PGSIZE);

    /* 4. 为 child task 申请 user stack，并将 parent task 的 user stack
       数据复制到其中。 (既然 user stack 也在 vma 中，这一步也可以直接在 5 中做，无需特殊处理) */
    uint64 task_child_user_stack = kalloc();
    memcpy((char *) task_child_user_stack, (char*)(USER_END - PGSIZE), PGSIZE);
    create_mapping(task_child->pgd, USER_END - PGSIZE, task_child_user_stack - PA2VA_OFFSET, PGSIZE,
                   PTE_USER | PTE_WRITE | PTE_READ | PTE_VALID);

    /* 5. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存 */
    for (i = 0; i < get_current_task()->vma_cnt; i++) {
        struct vm_area_struct vma = get_current_task()->vmas[i];
        uint64 vm_addr_curr = vma.vm_start;
        while (vm_addr_curr < vma.vm_end) {
            uint64 vm_addr_pg = PGROUNDDOWN(vm_addr_curr);
            /* 映射存在 */
            if (get_mapping(get_current_task()->pgd, vm_addr_curr)) {
                uint64 pg_copy = kalloc();

                memcpy((char *) pg_copy, (char *) vm_addr_pg, PGSIZE);
                create_mapping(task_child->pgd, vm_addr_pg, pg_copy - PA2VA_OFFSET,
                               PGSIZE, vma.vm_flags | PTE_USER | PTE_VALID);
            }
            vm_addr_curr = vm_addr_pg + PGSIZE;
        }
    }

    /* 6. 返回子 task 的 pid */
    return task_child->pid;
}