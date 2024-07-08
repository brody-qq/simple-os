#include "include/syscall.h"
#include "include/stdlib_workaround.h"

void sys_yield()
{
    asm volatile(
        "movq %0, %%rcx\n"
        "int $0xff\n"
        :
        : "i"(SYSCALL_YIELD)
        : "rcx"
    );
}

void sys_exit()
{
    asm volatile(
        "movq %0, %%rcx\n"
        "int $0xff\n"
        :
        : "i"(SYSCALL_EXIT)
        : "rcx"
    );
}

void sys_print(const char *str, u64 len)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "movq %2, %%r8\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_PRINT),
            "g"((u64)str),
            "g"(len)
        : "rcx", "rdx", "r8"
    );
}

void *sys_alloc(u64 size, u64 align)
{
    u64 addr = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "int $0xff\n"
        :   "=a"(addr)
        :   "i"(SYSCALL_ALLOC),
            "g"(size),
            "g"(align)
        : "rcx", "rdx", "r8", "memory"
    );
    return (void *)addr;
}

void sys_free(void *ptr)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_FREE),
            "g"((u64)ptr)
        : "rcx", "rdx", "memory"
    );
}

void sys_poll_keyboard(PollKeyboardResult *result)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_POLL_KEYBOARD),
            "g"((u64)result)
        : "rcx", "rdx", "memory"
    );
}

int sys_exec(const char *bin_path, char **argv, u64 flags)
{
    u64 result;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "movq %4, %%r9\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_EXEC),
            "g"((u64)bin_path),
            "g"((u64)argv),
            "g"(flags)
        : "rcx", "rdx", "r8", "r9"
    );
    return result;
}

void sys_tty_write(const char *str)
{
    sys_tty_write(str, strlen_workaround(str));
}

void sys_tty_write(const char *str, int len)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "movq %2, %%r8\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_WRITE),
            "g"((u64)str),
            "g"((u64)len)
        : "rcx", "rdx", "r8"
    );
}

void sys_tty_info(TTYInfo *info)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_INFO),
            "g"((u64)info)
        : "rcx", "rdx"
    );
}

void sys_tty_set_cursor(u32 i)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_SET_CURSOR),
            "g"((u64)i)
        : "rcx", "rdx"
    );
}

void sys_tty_set_cmdline(char *cmdline)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_SET_CMDLINE),
            "g"((u64)cmdline)
        : "rcx", "rdx"
    );
}

void sys_tty_flush()
{
    asm volatile(
        "movq %0, %%rcx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_FLUSH)
        : "rcx"
    );
}

void sys_tty_scroll(int amount)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_SCROLL),
            "g"((u64)amount)
        : "rcx", "rdx"
    );
}

void sys_tty_flush_cmdline()
{
    asm volatile(
        "movq %0, %%rcx\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_TTY_FLUSH_CMDLINE)
        : "rcx"
    );
}

int sys_pwd_length()
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_PWD_LENGTH)
        : "rcx"
    );
    return result;
}

void sys_get_pwd(char *dest, int len)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "movq %2, %%r8\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_GET_PWD),
            "r"((u64)dest),
            "r"((u64)len)
        : "rcx", "rdx", "r8", "memory"
    );
}

int sys_set_pwd(char *pwd)
{
    int result;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_SET_PWD),
            "r"((u64)pwd)
        : "rcx", "rdx"
    );
    return result;
}

FileStatResult sys_stat(const char *path)
{
    FileStatResult result = {};
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "movq %2, %%r8\n"
        "int $0xff\n"
        :   
        :   "i"(SYSCALL_STAT),
            "r"((u64)path),
            "r"((u64)&result)
        : "rcx", "rdx", "r8", "memory"
    );
    return result;
}

int sys_list_dir_buf_size(const char *path)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_LIST_DIR_BUF_SIZE),
            "r"((u64)path)
        : "rcx", "rdx"
    );
    return result;
}

void sys_list_dir(const char *path, char *buf, int buf_size, char **buf_one_past_end)
{
    asm volatile(
        "movq %0, %%rcx\n"
        "movq %1, %%rdx\n"
        "movq %2, %%r8\n"
        "movq %3, %%r9\n"
        "movq %4, %%r10\n"
        "int $0xff\n"
        :
        :   "i"(SYSCALL_LIST_DIR),
            "r"((u64)path),
            "r"((u64)buf),
            "r"((u64)buf_size),
            "r"((u64)buf_one_past_end)
        : "rcx", "rdx", "r8", "r9", "r10"
    );
}

u16 sys_fs_read(const char *path, char *buf, int offset, int size)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "movq %4, %%r9\n"
        "movq %5, %%r10\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_READ),
            "r"((u64)path),
            "r"((u64)buf),
            "r"((u64)offset),
            "r"((u64)size)
        : "rcx", "rdx", "r8", "r9", "r10"
    );
    return result;
}

u16 sys_fs_write(const char *path, char *buf, int offset, int size)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "movq %4, %%r9\n"
        "movq %5, %%r10\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_WRITE),
            "r"((u64)path),
            "r"((u64)buf),
            "r"((u64)offset),
            "r"((u64)size)
        : "rcx", "rdx", "r8", "r9", "r10"
    );
    return result;
}

u16 sys_fs_create_file(const char *path)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_CREATE_FILE),
            "r"((u64)path)
        : "rcx", "rdx"
    );
    return result;
}

u16 sys_fs_create_dir(const char *path)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_CREATE_DIR),
            "r"((u64)path)
        : "rcx", "rdx"
    );
    return result;
}

u16 sys_fs_rm_file(const char *path)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_RM_FILE),
            "r"((u64)path)
        : "rcx", "rdx"
    );
    return result;
}

u16 sys_fs_rm_dir(const char *path)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_RM_DIR),
            "r"((u64)path)
        : "rcx", "rdx"
    );
    return result;
}

u16 sys_fs_mv(const char *src, const char *dst)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_MV),
            "r"((u64)src),
            "r"((u64)dst)
        : "rcx", "rdx", "r8"
    );
    return result;
}

u16 sys_fs_truncate(const char *path, int new_size)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_TRUNC),
            "r"((u64)path),
            "r"((u64)new_size)
        : "rcx", "rdx", "r8"
    );
    return result;
}

// TODO this doesn't have to be done in kernelspace behind a syscall
bool sys_is_same_path(const char *path1, const char *path2)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "movq %3, %%r8\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_IS_SAME_PATH),
            "r"((u64)path1),
            "r"((u64)path2)
        : "rcx", "rdx", "r8"
    );
    return result == 0;
}

// TODO this doesn't have to be done in kernelspace behind a syscall
bool sys_is_dir_path(const char *path)
{
    int result = 0;
    asm volatile(
        "movq %1, %%rcx\n"
        "movq %2, %%rdx\n"
        "int $0xff\n"
        :   "=a"(result)
        :   "i"(SYSCALL_FS_IS_DIR_PATH),
            "r"((u64)path)
        : "rcx", "rdx"
    );
    return result == 0;
}
