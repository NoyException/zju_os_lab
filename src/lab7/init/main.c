#include "printk.h"
//#include "../../lab4/arch/riscv/include/proc.h"

extern void test();
extern void schedule();

int start_kernel() {
    printk("[S-MODE] Hello RISC-V\n");

//    printk("sstatus=%llx\n", csr_read(sstatus));
//    csr_write(sstatus, 0x0000000000000000);
//    printk("updated sstatus=%llx\n", csr_read(sstatus));

    schedule();
    test(); // DO NOT DELETE !!!

    return 0;
}
