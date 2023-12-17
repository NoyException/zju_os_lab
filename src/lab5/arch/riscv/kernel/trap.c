#include "clock.h"
#include "proc.h"
#include "printk.h"
#include "syscall.h"
#include "defs.h"
#include "mm.h"
#include "vm.h"
#include "string.h"

#define INTERRUPT_SIG 0x8000000000000000
#define TIMER_INTERRUPT 0x5
#define ECALL_FROM_USER 0x8
#define INSTRUCTION_PAGE_FAULT 0xc
#define LOAD_PAGE_FAULT 0xd
#define STORE_PAGE_FAULT 0xf

#define SYS_WRITE 64
#define SYS_GETPID  172

/* In vmlinux.lds */
extern char _sramdisk[];
extern char _eramdisk[];
/* 定位uapp段 */
static char *uapp_start = _sramdisk;
static char *uapp_end = _eramdisk;

/* struct pt_regs defined in proc.h */
void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs) {
//    printk("[S] scause: %lx, sepc: %lx\n", scause, sepc);

    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    if (scause & INTERRUPT_SIG) {
        scause -= INTERRUPT_SIG;
        // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
        if (scause == TIMER_INTERRUPT) {
            // printk("S Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
        } else {
            printk("[S] Unhandled interrupt\n");
            printk("[S] scause: %lx, sepc: %lx\n", scause, sepc);
        }
    } else {
        if (scause == ECALL_FROM_USER) {
            /* 系统调用的返回参数放置在 a0 中 (不可以直接修改寄存器， 应该修改 regs 中保存的内容)。 */

            /* 64 号系统调用 sys_write(unsigned int fd, const char* buf, size_t count)
             * 该调用将用户态传递的字符串打印到屏幕上，此处fd为标准输出（1），buf为用户需要打印的起始地址，
             * count为字符串长度，返回打印的字符数。( 具体见 user/printf.c )*/
            if (regs->gpr[16] == SYS_WRITE) // a7
                //  a0                       a0            a1                          a2
                regs->gpr[9] = sys_write(regs->gpr[9], (const char *) regs->gpr[10], regs->gpr[11]);
            else if (regs->gpr[16] == SYS_GETPID)   // a7
                regs->gpr[9] = sys_getpid();        // a0

            /* 针对系统调用这一类异常， 我们需要手动将 sepc + 4 ：sepc 记录的是触发异常的指令地址，
             * 由于系统调用这类异常处理完成之后， 我们应该继续执行后续的指令，因此需要我们手动修改 sepc 的地址，
             * 使得 sret 之后 程序继续执行。 */
            regs->sepc += 4;
        } else if (scause == INSTRUCTION_PAGE_FAULT || scause == LOAD_PAGE_FAULT || scause == STORE_PAGE_FAULT) {
            //1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
            uint64 bad_addr = regs->stval;
            uint64 bad_addr_floor = PGROUNDDOWN(bad_addr);
            printk("[S]1 Page Fault: scause=%lx, addr=%lx, floor=%lx\n", scause, bad_addr, bad_addr_floor);

            //2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
            struct vm_area_struct *vma = find_vma(get_current_task(), bad_addr);
            if (vma == NULL) return;

            //3. 分配一个页，将这个页映射到对应的用户地址空间
            uint64 page_addr = alloc_page();
            create_mapping(get_current_task()->pgd, bad_addr_floor, page_addr - PA2VA_OFFSET, PGSIZE,
                           (vma->vm_flags & (~(uint64_t) VM_ANONYM)));
            uint64 pa = get_mapping(get_current_task()->pgd, bad_addr);
            printk("[S]2 page_addr: %lx, pa: %lx\n", page_addr - PA2VA_OFFSET, pa);

            //4. 通过 (vma->vm_flags & VM_ANONYM) 获得当前的 VMA 是否是匿名空间
            //5. 根据 VMA 匿名与否决定将新的页清零或是拷贝 uapp 中的内容
            if (vma->vm_flags & VM_ANONYM) {
                printk("[S]3 Anonymous page\n");
                memset((void *) page_addr, 0, PGSIZE);
            } else {
                printk("[S]3 File page\n");
                printk("[S]4 vm_start: %lx, vm_end: %lx\n", vma->vm_start, vma->vm_end);
                if (bad_addr - vma->vm_start < PGSIZE) {
                    uint64 start_offset = vma->vm_start - PGROUNDDOWN(vma->vm_start);
                    uint64 src_addr = vma->vm_content_offset_in_file + (uint64) uapp_start;
                    uint64 size = min(PGSIZE - start_offset, vma->vm_end - vma->vm_start);
                    printk("[S]5 start: %lx, src_addr: %lx, size: %lx\n", (page_addr + start_offset), src_addr, size);
                    memcpy((void *) (page_addr + start_offset), (void *) src_addr, size);
                } else {
                    uint64 src_addr =
                            vma->vm_content_offset_in_file + (uint64) uapp_start + bad_addr_floor - vma->vm_start;
                    uint64 size = min(PGSIZE, vma->vm_end - bad_addr_floor);
                    memcpy((void *) page_addr, (void *) src_addr, size);
                }
                flush_tlb();
                printk("[S]6 flush\n");
                printk("[S] value=%lx\n\n", *(uint64*)bad_addr);
            }
        } else {
            printk("[S] Unhandled exception\n");
            printk("[S] scause: %lx, sepc: %lx\n", scause, sepc);
            while (1);
        }
    }
}