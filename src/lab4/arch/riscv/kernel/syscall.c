//
// Created by Noy on 2023/12/11.
//

#include "syscall.h"
#include "proc.h"
#include "printk.h"

extern struct task_struct* current;

uint64 sys_write(unsigned int fd, const char* buf, size_t count){
    if(fd == 1){
        char str[count+1];
        for(int i = 0; i < count; i++){
            str[i] = buf[i];
        }
        str[count] = '\0';
        printk("%s",str);
    }
    return count;
}

uint64 sys_getpid(){
    return current->pid;
}