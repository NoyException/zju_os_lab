//
// Created by Noy on 2023/11/28.
//

#ifndef OS_VM_H
#define OS_VM_H

#include "defs.h"

void flush_tlb();
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);
uint64 get_mapping(uint64 *pgtbl, uint64 va);

#endif //OS_VM_H
