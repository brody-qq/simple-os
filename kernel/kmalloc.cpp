#pragma once
#include "kernel/kmalloc.h"
#include "kernel/vspace.h"

extern VSpace g_kernel_vspace;

vaddr kmalloc(u64 size, u64 alignment)
{
    dbg_str("KMALLOC, SIZE: "); dbg_uint(size); dbg_str(" ALIGN: "); dbg_uint(alignment); dbg_str("\n");
    auto addr = g_kernel_vspace.allocate_size(size, alignment);
    return addr;
}

void kfree(vaddr ptr)
{
    dbg_str("KFREE, PTR: "); dbg_uint(ptr); dbg_str("\n");
    g_kernel_vspace.free_size(ptr);
}
