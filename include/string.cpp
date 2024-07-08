#include "include/string.h"

char *uint_to_str(u64 num, char *buf, int len)
{
    if(len <= 0)
        return buf;

    //const u32 max_u64_digits = 20;

    int i = len-1;
    buf[i] = '0'; // handles case where num == 0
    while(num > 0 && i >= 0) {
        u32 digit = num % 10;
        buf[i] = '0' + digit;
        num /= 10;
        --i;
    }

    return buf + i+1;
}
