    .section .text.entry
    .align 2
    .globl _traps
_traps:
    # 1. switch sp
    csrr t0, sstatus
    andi t0, t0, 0x100
    # skip switching stack if the previous mode is S-mode
    bne t0, x0, _trap_recovery  # SPP != USER

    csrr t0, sscratch
    csrw sscratch, sp
    addi sp, t0, 0

_trap_recovery:
    # 2. save 32 registers and sepc to stack
    addi sp, sp, -280
    sd x1, 0(sp)
    sd x2, 8(sp)
    sd x3, 16(sp)
    sd x4, 24(sp)
    sd x5, 32(sp)
    sd x6, 40(sp)
    sd x7, 48(sp)
    sd x8, 56(sp)
    sd x9, 64(sp)
    sd x10, 72(sp)
    sd x11, 80(sp)
    sd x12, 88(sp)
    sd x13, 96(sp)
    sd x14, 104(sp)
    sd x15, 112(sp)
    sd x16, 120(sp)
    sd x17, 128(sp)
    sd x18, 136(sp)
    sd x19, 144(sp)
    sd x20, 152(sp)
    sd x21, 160(sp)
    sd x22, 168(sp)
    sd x23, 176(sp)
    sd x24, 184(sp)
    sd x25, 192(sp)
    sd x26, 200(sp)
    sd x27, 208(sp)
    sd x28, 216(sp)
    sd x29, 224(sp)
    sd x30, 232(sp)
    sd x31, 240(sp)
    csrr t0, sepc
    sd t0, 248(sp)
    # lab4 addition: stores of sstatus sscratch
    csrr t0, sstatus
    sd t0, 256(sp)
    csrr t0, sscratch
    sd t0, 264(sp)
    csrr t0, stval
    sd t0, 272(sp)

    # -----------

    # 3. call trap_handler
    csrr a0, scause
    csrr a1, sepc
    add a2, sp, x0
    jal ra, trap_handler

    # -----------

# Lab6：for child program
    .global __ret_from_fork
__ret_from_fork:

    # 4. restore sepc and 32 registers (x2(sp) should be restore last) from stack

    # lab4 addition: loads of sepc sstatus sscratch
    ld t0, 272(sp)
    csrw stval, t0
    ld t0, 264(sp)
    csrw sscratch, t0
    ld t0, 256(sp)
    csrw sstatus, t0
    ld t0, 248(sp)
    csrw sepc, t0

    ld x1, 0(sp)
    ld x3, 16(sp)
    ld x4, 24(sp)
    ld x5, 32(sp)
    ld x6, 40(sp)
    ld x7, 48(sp)
    ld x8, 56(sp)
    ld x9, 64(sp)
    ld x10, 72(sp)
    ld x11, 80(sp)
    ld x12, 88(sp)
    ld x13, 96(sp)
    ld x14, 104(sp)
    ld x15, 112(sp)
    ld x16, 120(sp)
    ld x17, 128(sp)
    ld x18, 136(sp)
    ld x19, 144(sp)
    ld x20, 152(sp)
    ld x21, 160(sp)
    ld x22, 168(sp)
    ld x23, 176(sp)
    ld x24, 184(sp)
    ld x25, 192(sp)
    ld x26, 200(sp)
    ld x27, 208(sp)
    ld x28, 216(sp)
    ld x29, 224(sp)
    ld x30, 232(sp)
    ld x31, 240(sp)
    ld x2, 8(sp)
    addi sp, sp, 280

    # -----------

    # 5. return from trap

    # lab4 addition: when S-mode -> U-mode, change sp

    csrr t0, sstatus             # t0 changed here
    andi t0, t0, 0x100
    bne t0, x0, SRET_FROM_TRAP   # SPP != USER-MODE

    # csrrw sp, sscratch, sp       # swapping values(this operation meets the requirements)
    csrr t0, sscratch
    csrw sscratch, sp
    addi sp, t0, 0

    sret

SRET_FROM_TRAP:
    sret


    .extern dummy
    .globl __dummy
__dummy:
    # la t0, dummy
    # csrw sepc, t0

    # lab4: S-mode -> U-mode, change sp
    csrr t0, sscratch   # get u-mode sp
    csrw sscratch, sp   # save s-mode sp
    addi sp, t0, 0      # change sp
    sret


    .globl __switch_to
__switch_to:
    # save state to prev process
    # YOUR CODE HERE

    # t0 -> &currentTask.thread_struct
    add t0, a0, 32
    sd ra, 0(t0)
    sd sp, 8(t0)
    sd s0, 16(t0)
    sd s1, 24(t0)
    sd s2, 32(t0)
    sd s3, 40(t0)
    sd s4, 48(t0)
    sd s5, 56(t0)
    sd s6, 64(t0)
    sd s7, 72(t0)
    sd s8, 80(t0)
    sd s9, 88(t0)
    sd s10, 96(t0)
    sd s11, 104(t0)
    # lab4 addition: stores of sepc sstatus sscratch
    csrr t1, sepc
    sd t1, 112(t0)
    csrr t1, sstatus
    sd t1, 120(t0)
    csrr t1, sscratch
    sd t1, 128(t0)

    # restore state from next process
    # t0 -> &nextTask.thread_struct
    add t0, a1, 32
    ld ra, 0(t0)
    ld sp, 8(t0)
    ld s0, 16(t0)
    ld s1, 24(t0)
    ld s2, 32(t0)
    ld s3, 40(t0)
    ld s4, 48(t0)
    ld s5, 56(t0)
    ld s6, 64(t0)
    ld s7, 72(t0)
    ld s8, 80(t0)
    ld s9, 88(t0)
    ld s10, 96(t0)
    ld s11, 104(t0)
    # lab4 addition: loads of sepc sstatus sscratch
    ld t1, 112(t0)
    csrw sepc, t1
    ld t1, 120(t0)
    csrw sstatus, t1
    ld t1, 128(t0)
    csrw sscratch, t1

    # lab4: swap page table
    la t1, PA2VA_OFFSET

    ld t2, 136(t0)  # t2 == nextTask.pgd
    sub t2, t2, t1
    srli t2, t2, 12 # t2 == nextTask's page table PPN
    
    li t3, 8
    slli t3, t3, 60 # t3 == satp Page-based 39 bit virtual addressing mode
    add t2, t2, t3   # t2 == satp value

    csrw satp, t2

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret