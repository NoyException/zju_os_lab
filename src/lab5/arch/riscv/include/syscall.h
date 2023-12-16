//
// Created by Noy on 2023/12/11.
//

#ifndef OS_SYSCALL_H
#define OS_SYSCALL_H

#include "types.h"
#include "stddef.h"

uint64 sys_write(unsigned int fd, const char* buf, size_t count);
uint64 sys_getpid();


#endif //OS_SYSCALL_H
