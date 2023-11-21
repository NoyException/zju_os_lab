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

在 `nish` 启动时，会首先向 `stdout` 和 `stdout` 分别写入一段内容，用户态的代码如下所示。

