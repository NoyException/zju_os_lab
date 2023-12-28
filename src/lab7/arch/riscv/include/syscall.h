//
// Created by Noy on 2023/12/11.
//

#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H

#include "types.h"
#include "stddef.h"
#include "proc.h"

int64_t sys_lseek(unsigned int fd, int64_t offset, int whence);
int64_t sys_openat(int dfd, const char* filename, int flags);
int64_t sys_close(unsigned int fd);
int64_t sys_write(unsigned int fd, const char* buf, size_t count);
int64_t sys_read(unsigned int fd, char* buf, uint64_t count);
uint64 sys_getpid();
uint64 sys_clone(struct pt_regs *regs);

#endif //OS_SYSCALL_H
