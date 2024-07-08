#include "include/syscall.h"
#include "include/string.h"

int main(int argc, char **argv)
{
    int len = sys_pwd_length();
    char *buf = (char *)sys_alloc(len+1, 64);
    sys_get_pwd(buf, len+1);
    buf[len] = 0;

    sys_tty_write(buf, len);
    sys_tty_write("\n", 1);
    sys_tty_flush();

    sys_free(buf);
}
