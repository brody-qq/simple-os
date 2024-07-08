#pragma once
#include "include/types.h"

template<typename T>
T min(T a, T b)
{
    if(a < b)
        return a;
    return b;
}

template<typename T>
T max(T a, T b)
{
    if(a > b)
        return a;
    return b;
}

bool is_power_of_2(u64 val);

u64 round_up_align(u64 val, u64 alignment);

u64 round_down_align(u64 val, u64 alignment);

u64 round_up_divide(u64 num, u64 denominator);

bool is_aligned(u64 val, u64 alignment);

bool xor_(bool a, bool b);
