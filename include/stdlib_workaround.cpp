#include "include/stdlib_workaround.h"

void memmove_workaround(void *dest, void *src, size_t len)
{
    if(dest == src)
        return;

    u8 *src_ptr = (u8 *)src;
    u8 *dest_ptr = (u8 *)dest;

    // copy left to right
    if(dest_ptr < src_ptr) {
        while(len--)
            *dest_ptr++ = *src_ptr++;
        return;
    }

    // copy right to left
    src_ptr = src_ptr + (len-1);
    dest_ptr = dest_ptr + (len-1);
    while(len--)
        *dest_ptr-- = *src_ptr--;
}

void memset_workaround(void *dest, int val, size_t len)
{
    u8 *dest_ptr = (u8 *)dest;
    while(len--)
        *dest_ptr++ = val;
}

int strncmp_workaround(const char *str1, const char *str2, size_t len)
{
    if(len <= 0)
        return 0;

    while(len && *str1 && *str2) {
        if(*str1 < *str2)
            return -1;
        if(*str1 > *str2)
            return 1;

        ++str1;
        ++str2;
        --len;
    }

    if(len == 0)
        return 0;
    if(*str1 == 0)
        return -1;
    //if(*str2 == 0)
    return 1;
}

size_t strlen_workaround(const char *str)
{
    int i = 0;
    while(str[i]) { ++i; }
    return i;
}

const char *strchr_workaround(const char *str, int c)
{
    const char *ptr = str;
    while(*ptr && *ptr != c) { ++ptr; }
    return ptr;
}
