# Lab7: VFS & FAT32 文件系统

本实验中不涉及 `fork` 的实现和缺页异常，只需要完成 Lab4 即可开始本实验。

## 实验目的

* 认识 VFS 及 VFS 中的文件，利用 `read`, `write` 和 `lseek` 对抽象文件进行操作。
* 实现 FAT32 文件系统的基本功能，并对其中的文件进行读写。
* 认识 MBR 主引导记录。

## 实验环境

与先前的实验中使用的环境相同。

## 背景知识

### VirtIO

### MBR

### FAT32

## 实验步骤

### Shell: 与内核进行交互

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
// user/shell.c

write(1, "hello, stdout!\n", 15);
write(2, "hello, stderr!\n", 15);
```

#### 处理 `stdout` 的写入

我们在用户态已经像上面这样实现好了 `write` 函数来向内核发起 syscall，我们先在内核态完成真实的写入过程，也即将写入的字符输出到串口。

```c
// arch/riscv/include/syscall.h

int64_t sys_write(unsigned int fd, const char* buf, uint64_t count);

// arch/riscv/include/syscall.c

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
// include/fs.h

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

首先要做的是在创建进程时为进程初始化文件，当初始化进程时，先完成打开的文件的列表的初始化，这里我们的方式是直接分配一个页，并用 `files` 指向这个页。

```c
// fs/vfs.c
int64_t stdout_write(struct file* file, const void* buf, uint64_t len);
int64_t stderr_write(struct file* file, const void* buf, uint64_t len);
int64_t stdin_read(struct file* file, void* buf, uint64_t len);

struct file* file_init() {
    struct file *ret = (struct file*)alloc_page();

    // stdin
    ret[0].opened = 1;
    ...

    // stdout
    ret[1].opened = 1;
    ret[1].perms = FILE_WRITABLE;
    ret[1].cfo = 0;
    ret[1].lseek = NULL;
    ret[1].write = /* todo */;
    ret[1].read = NULL;
    memcpy(ret[1].path, "stdout", 7);

    // stderr
    ret[2].opened = 1;
    ...

    return ret;
}

int64_t stdout_write(struct file* file, const void* buf, uint64_t len) {
    char to_print[len + 1];
    for (int i = 0; i < len; i++) {
        to_print[i] = ((const char*)buf)[i];
    }
    to_print[len] = 0;
    return printk(buf);
}

// arch/riscv/kernel/proc.c
void task_init() {
    ...
    // Initialize the stdin, stdout, and stderr.
    task[1]->files = file_init();
    printk("[S] proc_init done!\n");
    ...
}
```

可以看到每一个被打开的文件对应三个函数指针，这三个函数指针抽象出了每个被打开的文件的操作。也对应了 `SYS_LSEEK`，`SYS_WRITE`，和 `SYS_READ` 这三种 syscall. 最终由函数 `sys_write` 调用 `stdout` 对应的 `struct file` 中的函数指针 `write` 来执行对应的写串口操作。我们这里直接给出 `stdout_write` 的实现，只需要直接把这个函数指针赋值给 `stdout` 对应 `struct file` 中的 `write` 即可。

接着你需要实现 `sys_write` syscall，来间接调用我们赋值的 `stdout` 对应的函数指针。

```c

// arch/riscv/kernel/syscall.c

int64_t sys_write(unsigned int fd, const char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        /* todo: indirect call */
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}
```

至此，你已经能够打印出 `stdout` 的输出了。

```plaintext
2023 Hello RISC-V
hello, stdout!
```

#### 处理 `stderr` 的写入

仿照 `stdout` 的输出过程，完成 `stderr` 的写入，让 `nish` 可以正确打印出

```plaintext
2023 Hello RISC-V
hello, stdout!
hello, stderr!
SHELL >
```

#### 处理 `stdin` 的读取

此时 `nish` 已经打印出命令行等待输入命令以进行交互了，但是还需要读入从终端输入的命令才能够与人进行交互，所以我们要实现 `stdin` 以获取键盘键入的内容。

在终端中已经实现了不断读 `stdin` 文件来获取键入的内容，并解析出命令，你需要完成的只是响应如下的系统调用：

```c
// user/shell.c

read(0, read_buf, 1);
```

代码框架中已经实现了一个在内核态用于向终端读取一个字符的函数，你需要调用这个函数来实现你的 `stdin_read`.

```c
// fs/vfs.c

char uart_getchar() {
    /* already implemented in the file */
}

int64_t stdin_read(struct file* file, void* buf, uint64_t len) {
    /* todo: use uart_getchar() to get <len> chars */
}
```

接着参考 `syscall_write` 的实现，来实现 `syscall_read`.

```c
int64_t sys_read(unsigned int fd, char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        /* todo: indirect call */
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}
```


### FAT32: 持久存储

在本次实验中我们仅需实现 FAT32 文件系统中很小一部分功能，我们为实验中的测试做如下限制：

* 文件名长度小于等于 8 个字符，并且不包含后缀名和字符 `.` .
* 不包含目录的实现，所有文件都保存在磁盘根目录下。
* 不涉及磁盘上文件的创建和删除。

#### 利用 VirtIO 为 QEMU 添加虚拟存储

我们为大家构建好了[磁盘镜像](todo)，其中包含了一个 MBR 分区表以及一个存储有一些文件的 FAT32 分区。可以使用如下的命令来启动 QEMU，并将该磁盘连接到 QEMU 的一个 VirtIO 接口上，构成一个 `virtio-blk-device`。

```Makefile
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

`virtio` 所需的驱动我们已经为大家编写完成了，在 `fs/virtio.c` 中给出。

#### 初始化 MBR

我们为大家实现了读取 MBR 这一磁盘初始化过程。该过程会搜索磁盘中存在的分区，然后对分区进行初步的初始化。

对 VirtIO 和 MBR 进行初始化的逻辑可以被添加在初始化第一个进程的 `task_init` 中

```c
// arch/riscv/kernel/proc.c
void task_init() {
    ...
    printk("[S] proc_init done!\n");

    virtio_dev_init();
    mbr_init();
}
```

