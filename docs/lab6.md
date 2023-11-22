# Lab6: VFS & FAT32 文件系统

## 实验目的

* 认识 VFS 及 VFS 中的文件，利用 `read`, `write` 和 `lseek` 对抽象文件进行操作。
* 实现 FAT32 文件系统的基本功能，并对其中的文件进行读写。
* 认识 MBR 主引导记录。

## 实验环境

与先前的实验中使用的环境相同。

## 背景知识

### VirtIO

## 实验步骤

### 利用 VirtIO 为 QEMU 添加虚拟存储

我们为大家构建好了磁盘镜像，其中包含了一个 MBR 分区表以及一个存储有一些文件的 FAT32 分区。可以使用如下的命令来启动 QEMU，并将该磁盘连接到 QEMU 的一个 VirtIO 接口上，构成一个 `virtio-blk-device`。

```bash
run: all
    @echo Launch the qemu ......
    @qemu-system-riscv64 \
        -machine virt \
        -nographic \
        -bios default \
        -kernel vmlinux \
        -global virtio-mmio.force-legacy=false \
        -drive file=disk.img,if=none,format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0
```

### 为实验工程添加 Shell

我们为大家提供了 `nish` 来与我们在实验中完成的 kernel 进行交互。`nish` (Not Implemented SHell) 提供了简单的用户交互和文件读写功能，有如下的命令。

```bash
echo [string] # 将 string 输出到 stdout
cat  [path]   # 将路径为 path 的文件的内容输出到 stdout
edit [path] [offset] [string] # 将路径为 path 的文件，
            # 偏移量为 offset 的部分开始，写为 string
```

同步 `os23fall-stu` 中的 `user` 文件夹，替换原有的用户态程序为 `nish`。

```plaintext
lab6
└── user
    ├── forktest.c
    ├── link.lds
    ├── Makefile
    ├── printf.c
    ├── ramdisk.S
    ├── shell.c
    ├── start.S
    ├── stddef.h
    ├── stdio.h
    ├── string.h
    ├── syscall.h
    ├── unistd.c
    └── unistd.h
```

我们在启动一个用户态程序时默认打开了三个文件，`stdin`，`stdout` 和 `stdout`，他们对应的 file descriptor 分别为 `0`，`1`，`2`。在 `nish` 启动时，会首先向 `stdout` 和 `stdout` 分别写入一段内容，用户态的代码如下所示。

```c
write(1, "hello, stdout!\n", 15);
write(2, "hello, stderr!\n", 15);
```

### 处理 `stdout` 和 `stderr` 的写入

我们在用户态已经实现好了 `write` 函数来向内核发起 syscall，我们先在内核态完成真实的写入过程，也即将写入的字符输出到串口。

```c
int64_t sys_write(unsigned int fd, const char* buf, uint64_t count);

void trap_handler(uint64_t scause, uint64_t sepc, struct pt_regs *regs) {
    ...
    if (scause == 0x8) { // syscalls
        uint64_t sys_call_num = regs->a7;
        ...
        if (sys_call_num == SYS_WRITE) {
            regs->a0 = sys_write(regs->a0, (const char*)(regs->a1), regs->a2);
            regs->sepc = regs->sepc + 4;
        } else {
            printk("Unhandled Syscall: 0x%lx\n", regs->a7);
            while (1);
        }
    }
    ...
}
```

注意到我们使用的是 `fd` 来索引打开的文件，所以在该进程的内核态需要维护当前进程打开的文件，将这些文件的信息储存在一个表中，并在 `task_struct` 中指向这个表。

```c
struct file {
    uint32_t opened;
    uint32_t perms;
    int64_t cfo;
    uint32_t fs_type;

    union {
        struct fat32_file fat32_file;
    };

    int64_t (*lseek) (struct file* file, int64_t offset, uint64_t whence);
    int64_t (*write) (struct file* file, const void* buf, uint64_t len);
    int64_t (*read)  (struct file* file, void* buf, uint64_t len);

    char path[MAX_PATH_LENGTH];
};

struct task_struct {
    ...
    struct file *files;
    ...
};
```

可以看到每一个被打开的文件对应三个函数指针，这三个函数指针抽象出了每个被打开的文件的操作。也对应了 `SYS_LSEEK`，`SYS_WRITE`，和 `SYS_READ` 这三种 syscall。