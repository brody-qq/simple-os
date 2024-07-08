#pragma once
#include "kernel/types.h"

vaddr kmalloc(u64 size, u64 alignment);

void kfree(vaddr ptr);
