#include "clock.h"
#include "proc.h"
#include "printk.h"
#include "syscall.h"

#define INTERRUPT_SIG 0x8000000000000000
#define TIMER_INTERRUPT_SIG 0x5
#define ECALL_FROM_USER 0x8

#define SYS_WRITE 64
#define SYS_GETPID  172

/* struct pt_regs defined in proc.h */
void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs) {
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略

    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    if (scause & INTERRUPT_SIG) {
        // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
        if (scause % 16 == TIMER_INTERRUPT_SIG) {
            printk("S Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
        }
    } else if (scause & ECALL_FROM_USER) {
        /* 系统调用的返回参数放置在 a0 中 (不可以直接修改寄存器， 应该修改 regs 中保存的内容)。 */

        /* 64 号系统调用 sys_write(unsigned int fd, const char* buf, size_t count)
         * 该调用将用户态传递的字符串打印到屏幕上，此处fd为标准输出（1），buf为用户需要打印的起始地址，
         * count为字符串长度，返回打印的字符数。( 具体见 user/printf.c )*/
        if (regs->gpr[13] == SYS_WRITE) // a7
            regs->gpr[6] = sys_write(regs->gpr[6], (const char *) regs->gpr[7], regs->gpr[8]);
            //  a0                       a0            a1            a2

            /* 172 号系统调用 sys_getpid() 该调用从current中获取当前的pid放入a0中返回，无参数。
             *（ 具体见 user/getpid.c ） */
        else if (regs->gpr[13] == SYS_GETPID)   // a7
            regs->gpr[6] = sys_getpid();        // a0

        /* 针对系统调用这一类异常， 我们需要手动将 sepc + 4 ：sepc 记录的是触发异常的指令地址，
         * 由于系统调用这类异常处理完成之后， 我们应该继续执行后续的指令，因此需要我们手动修改 sepc 的地址，
         * 使得 sret 之后 程序继续执行。 */
        regs->sepc += 4;
    }
}