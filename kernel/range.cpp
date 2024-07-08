#pragma once
#include "kernel/range.h"

void print_prange(const PRange& range)
{
    dbg_str("range.addr: ");
    dbg_uint((u64)range.addr);
    dbg_str("\nrange.length: ");
    dbg_uint(range.length);
    dbg_str("\n\n");
}

u64 PRange::one_past_end() const { return addr + length; }

void PRange::print() { print_prange(*this); }

PRange PRange::subtract(const PRange& other) const
{
    if(other.one_past_end() <= addr)
        return {addr, length};
    u64 lost_space = other.one_past_end() - addr;
    paddr new_addr = other.one_past_end();
    u64 new_length = length - lost_space;
    return {new_addr, new_length};
}

PRange PRange::subtract(u64 size)
{
    u64 new_addr = addr + size;
    u64 new_length = length - size;
    return {new_addr, new_length};
}

// ---------------------------------------------------------------------------------------

u64 VRange::one_past_end() const { return addr + length; }

VRange VRange::subtract(const VRange& other) const
{
    if(other.one_past_end() <= addr)
        return {addr, length};
    u64 lost_space = other.one_past_end() - addr;
    paddr new_addr = other.one_past_end();
    u64 new_length = length - lost_space;
    return {new_addr, new_length};
}

VRange VRange::subtract(u64 size)
{
    u64 new_addr = addr + size;
    u64 new_length = length - size;
    return {new_addr, new_length};
}

void print_vrange(const VRange& range)
{
    dbg_str("range.addr: ");
    dbg_uint((u64)range.addr);
    dbg_str("\nrange.length: ");
    dbg_uint(range.length);
    dbg_str("\n\n");
}

void VRange::print() { print_vrange(*this); }
