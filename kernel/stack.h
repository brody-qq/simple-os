#pragma once
#include "kernel/types.h"
#include "include/math.h"
#include "kernel/asm.cpp"

struct Stack
{
    u64 bottom = 0;
    u64 top = 0;

    void set_stack_top(u64 size, u64 alignment)
    {
        u64 t = bottom + size-1;
        top = round_down_align(t, alignment);
    }

    bool is_ptr_in_stack(u64 ptr)
    {
        return ptr >= bottom && ptr <= top;
    }

    bool currently_using_stack()
    {
        return is_ptr_in_stack(rsp());
    }
};
