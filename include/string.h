#pragma once
#include "include/types.h"
#include "include/stdlib_workaround.h"
#include "include/syscall.h"
#include "include/math.h"

char *uint_to_str(u64 num, char *buf, int len)
{
    if(len <= 0)
        return buf;

    if(num == 0) {
        buf[len-1] = '0';
        return buf + len-1;
    }

    //const u32 max_u64_digits = 20;

    int i = len;
    while(num > 0 && i > 0) {
        --i;
        u32 digit = num % 10;
        buf[i] = '0' + digit;
        num /= 10;
    }

    return buf + i;
}

// TODO make this account for + or - at start of string
bool str_is_num(const char *str)
{
    int len = strlen_workaround(str);
    for(int i=0; i<len; ++i) {
        if(str[i] < '0' || str[i] > '9')
            return false;
    }
    return true;
}

// TODO make this account for + or - at start of string
// TODO make sure the number in str doesn't overflow an int
int str_to_uint(const char *str)
{
    int len = strlen_workaround(str);

    int i = len;
    int pow10 = 1;
    int val = 0;
    while(i-- > 0) {
        char c = str[i];
        int digit = c - '0';
        val += pow10 * digit;

        pow10++;
    }
    return val;
}

void concat(char *dest, const char *src1, const char *src2, int buf_len)
{
    int src1_len = strlen_workaround(src1);
    int copy1_len = min(buf_len, src1_len);
    memmove_workaround(dest, (char *)src1, copy1_len);

    int src2_len = strlen_workaround(src2);
    int copy2_len = min(buf_len - copy1_len, src2_len);
    memmove_workaround(dest + copy1_len, (char *)src2, copy2_len);

    dest[copy1_len + copy2_len] = 0;
}

bool contains(const char *s, char c)
{
    while(*s) {
        if(*s == c)
            return true;
        ++s;
    }
    return false;
}

// TODO this doesn't handle it properly if pwd and/or path overflow buf_len
void resolve_path(const char *pwd, const char *path, char *buf, int buf_len)
{
    bool abs_path = path[0] == '/';
    int pathlen = strlen_workaround(path);
    if(pathlen +1 > buf_len) {
        return;
    }

    if(abs_path) {
        memmove_workaround(buf, (char *)path, pathlen);
        buf[pathlen] = 0;
        return;
    }

    int pwdlen = strlen_workaround(pwd);
    if(pwdlen +1 > buf_len) {
        return;
    }
    memmove_workaround(buf, (char *)pwd, pwdlen);
    if(buf[pwdlen-1] != '/')
        buf[pwdlen++] = '/';
    if(pathlen + pwdlen +1 > buf_len) {
        return;
    }
    memmove_workaround(buf + pwdlen, (char *)path, pathlen);
    buf[pathlen + pwdlen] = 0;
}

size_t abs_path_len(const char *pwd, const char *path)
{
    bool is_abs_path = path[0] == '/';
    size_t pathlen = strlen_workaround(path);
    if(is_abs_path)
        return pathlen;

    int pwdlen = strlen_workaround(pwd);
    if(pwd[pwdlen-1] != '/')
        pwdlen++;
    return pwdlen + pathlen;
}

void write_uint(u64 num)
{
    int max_digits = 20;

    char s[max_digits+1] = {0};
    char *ptr = uint_to_str(num, s, max_digits);
    sys_tty_write(ptr, max_digits - (ptr-s));
    sys_tty_flush();
}

void usage_error(const char *prog_name, const char *args)
{
    sys_tty_write("usage is: ");
    sys_tty_write(prog_name);
    sys_tty_write(" ");
    sys_tty_write(args);
    sys_tty_write("\n");
    sys_tty_flush();
}

void cmd_error(const char *error_str)
{
    sys_tty_write("error: ");
    sys_tty_write(error_str);
    sys_tty_write("\n");
    sys_tty_flush();
}

void prog_error(const char *prog_name, const char *error_str)
{
    sys_tty_write(prog_name);
    sys_tty_write(": ");
    sys_tty_write(error_str);
    sys_tty_write("\n");
    sys_tty_flush();
}
