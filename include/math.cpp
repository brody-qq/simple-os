#include "include/math.h"

bool is_power_of_2(u64 val)
{
    // TODO this was causing a linker error
    //return __builtin_popcount(val) == 1;
    u64 count = 0;
    for(int i = 0; i < 64; ++i) {
        if((val & ((u64)1 << i)) != 0)
            count++;
    }
    return count == 1;
}

u64 round_up_align(u64 val, u64 alignment)
{
    //ASSERT(is_power_of_2(alignment));
    //u64 mask = ~(alignment-1);
    //val = (val + (alignment-1)) & mask;
    if(alignment == 0)
        return val;

    val = (val + (alignment-1));
    val -= val % alignment;
    return val;
}

u64 round_down_align(u64 val, u64 alignment)
{
    //ASSERT(is_power_of_2(alignment));
    //u64 mask = ~(alignment-1);
    //val &= mask;
    if(alignment == 0)
        return val;

    val -= val % alignment;
    return val;
}

u64 round_up_divide(u64 num, u64 denominator)
{
    u64 res = (num + (denominator -1)) / denominator;
    return res;
}

bool is_aligned(u64 val, u64 alignment)
{
    if(alignment == 0)
        return true;

    return (val & (alignment-1)) == 0;
}

bool xor_(bool a, bool b)
{
    return (!a && b) || (a && !b);
}
