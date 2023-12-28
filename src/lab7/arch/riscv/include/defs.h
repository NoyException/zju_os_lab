#ifndef _DEFS_H
#define _DEFS_H

#include "types.h"

/** PAGE DEFS */
#define PGSIZE 0x1000 // 4KB
#define PGROUNDUP(addr) ((addr + PGSIZE - 1) & (~(PGSIZE - 1)))
#define PGROUNDDOWN(addr) (addr & (~(PGSIZE - 1)))

/** OPENSBI DEFS */
#define OPENSBI_SIZE (0x200000)

/** PHYSICAL SPACE DEFS */
#define PHY_START 0x0000000080000000
#define PHY_SIZE  128 * 1024 * 1024 // 128MB，QEMU 默认内存大小
#define PHY_END   (PHY_START + PHY_SIZE)

/** USER SPACE DEFS */
#define USER_START (0x0000000000000000) // user space start virtual address
#define USER_END   (0x0000004000000000) // user space end virtual address

/** VM SPACE DEFS */
#define VM_START (0xffffffe000000000)
#define VM_END   (0xffffffff00000000)
#define VM_SIZE  (VM_END - VM_START)

#define PA2VA_OFFSET (uint64)(VM_START - PHY_START)

/** SSTATUS BITS DEFS */
#define SPP(value) value << 8
#define SPP_USER 0
#define SPP_NONUSER 1
#define SPIE(value) value << 5
#define SUM(value) value << 18

/** Page Table Entry DEFS */
#define PTE_VALID   1
#define PTE_READ    1 << 1
#define PTE_WRITE   1 << 2
#define PTE_EXECUTE 1 << 3
#define PTE_USER    1 << 4

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) < (b) ? (b) : (a))

/** CSR DEFS */
#define csr_read(csr)                       \
({                                          \
    register uint64 __v;                    \
    asm volatile ("csrr %[v], " #csr        \
    : [v] "=r" (__v)::);                    \
    __v;                                    \
})


#define csr_write(csr, val)                         \
({                                                  \
    uint64 __v = (uint64)(val);                     \
    asm volatile ("csrw " #csr ", %0"               \
                    : : "r" (__v)                   \
                    : "memory");                    \
})

#endif