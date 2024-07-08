#pragma once
#include "kernel/types.h"
#include "kernel/utils.h"
#include "kernel/debug.cpp"

extern "C" void reload_cr3();

// based on code from SerenityOS:
//      https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/IO.h
//      https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/ASM_wrapper.cpp
//      https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/ASM_wrapper.h

u8 in8(u16 port)
{
    u8 val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

u16 in16(u16 port)
{
    u16 val;
    asm volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

u32 in32(u16 port)
{
    u32 val;
    asm volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void ins16(u16 port, u32 count, void *buffer)
{
    // TODO assert cld
    asm volatile("rep insw" : "+c"(count), "+D"(buffer) : "d"(port) : "memory");
}

void out8(u16 port, u8 val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void out16(u16 port, u16 val)
{
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

void out32(u16 port, u32 val)
{
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

void outs16(u16 port, u32 count, void *buffer)
{
    // TODO assert cld
    asm volatile("rep outsw" : "+c"(count), "+S"(buffer) : "d"(port));
}

bool is_aligned(u64, u64);
// NOTE: writes to cr3, cr4 and cr0 are serializing
void write_cr3(u64 addr)
{
    ASSERT(is_aligned(addr, 4096));
    asm volatile("mov %%rax, %%cr3" : : "a"(addr) : "memory");
}

u64 read_cr3()
{
    u64 addr;
    asm volatile("mov %%cr3, %%rax" : "=a"(addr));
    return addr;
}

void sti()
{
    asm volatile("sti" : : : "memory");
}

void cli()
{
    asm volatile("cli" : : : "memory");
}

u64 rflags()
{
    u64 rflags;
    asm volatile(
        "pushfq\n"
        "popq %0\n"
        : "=rm"(rflags) : : "memory");
    return rflags;
}

bool are_interrupts_enabled()
{
    bool res = (rflags() & (1 << 9)) != 0;
    return res;
}

// TODO should rsp() and rbp() be always_inline since entering them as functions will modyify the rsp & rbp
u64 rsp()
{
    u64 rsp;
    asm volatile("movq %%rsp, %0" : "=g"(rsp));
    return rsp;
}

u64 rbp()
{
    u64 rbp;
    asm volatile("movq %%rbp, %0" : "=g"(rbp));
    return rbp;
}
