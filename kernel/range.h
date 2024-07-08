#pragma once
#include "kernel/types.h"
#include "kernel/debug.cpp"


struct PRange
{
    paddr addr = 0;
    u64 length = 0;
    u64 one_past_end() const;
    void print();

    // static subtract(const PRange& a, const PRange& b) {}

    PRange subtract(const PRange& other) const;
    PRange subtract(u64 size);
};

void print_prange(const PRange& range);

// TODO VRange and PRange are the exact same thing
struct VRange
{
    vaddr addr = 0;
    u64 length = 0;
    u64 one_past_end() const;
    void print();

    VRange subtract(const VRange& other) const;
    VRange subtract(u64 size);
};

void print_vrange(const VRange& range);
