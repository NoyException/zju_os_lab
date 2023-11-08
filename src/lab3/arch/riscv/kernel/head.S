.extern start_kernel

    .section .text.init
    .globl _start
_start:
    # ----------------------
    # -  initialize stack  -
    # ----------------------

    la sp, boot_stack_top

    jal ra, mm_init # initialize physical memory

    jal ra, task_init # initialize task threads

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
    jal ra, clock_set_next_event

    # ----------------------

    # set sstatus[SIE] = 1
    csrr t0, sstatus
    ori t0, t0, 0x2
    csrw sstatus, t0

    # ----------------------
    # Call the kernel
    jal x0, start_kernel

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096

    .globl boot_stack_top
boot_stack_top: