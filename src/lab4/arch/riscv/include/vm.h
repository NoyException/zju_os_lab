//
// Created by Noy on 2023/11/28.
//

#ifndef OS_VM_H
#define OS_VM_H

#include "defs.h"

void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm);

#endif //OS_VM_H
