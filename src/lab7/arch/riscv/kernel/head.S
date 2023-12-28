.extern start_kernel

    .section .text.init
    .globl _start
_start:

    # ----------------------
    # -  initialize stack  -
    # ----------------------

    la t0, boot_stack
    la t1, PA2VA_OFFSET
    sub t0, t0, t1
    addi sp, t0, 0 # physical addr of stack

    # ---------------------
    # - set up v/m memory -
    # ---------------------

    call setup_vm

    call relocate

    call mm_init

    call setup_vm_final

    call task_init

    # ----------------------

    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0

    # ----------------------

    # set sie[STIE] = 1
    csrr t0, sie
    ori t0, t0, 0x20
    csrw sie, t0

    # ----------------------

    # set first time interrupt
    call clock_set_next_event

    # ----------------------

    # set sstatus[SIE] = 1
    # csrr t0, sstatus
    # ori t0, t0, 0x2
    # csrw sstatus, t0

    # ----------------------
    # Call the kernel
    jal x0, start_kernel

relocate:
    ###################### 
    #   YOUR CODE HERE   #
    ######################

    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)

    la t0, PA2VA_OFFSET
    add ra, ra, t0
    add sp, sp, t0

    # set satp with early_pgtbl

    la t1, early_pgtbl
    sub t1, t1, t0
    srli t1, t1, 12

    li t2, 8
    slli t2, t2, 60
    or t1, t1, t2

    csrw satp, t1

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret


    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096

    .globl boot_stack_top
boot_stack_top: