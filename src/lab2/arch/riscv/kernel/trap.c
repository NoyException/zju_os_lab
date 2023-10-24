#include "clock.h"
#include "proc.h"
#include "printk.h"

#define INTERRUPT_SIG 0x8000000000000000
#define TIMER_INTERRUPT_SIG 0x5

void trap_handler(unsigned long scause, unsigned long sepc) {
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略

    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    if (scause & INTERRUPT_SIG){
        // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
        if (scause%16 == TIMER_INTERRUPT_SIG){
            printk("S Mode Timer Interrupt\n");
            clock_set_next_event();
            do_timer();
        }
    }
}