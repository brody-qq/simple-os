#pragma once
#include "include/types.h"
#include "include/key_event.h"
#include "include/filesystem_defs.h"

const u64 SYSCALL_YIELD = 0x0;
const u64 SYSCALL_EXIT = 0x1;
const u64 SYSCALL_EXEC = 0x2;
const u64 SYSCALL_PRINT = 0x3;
const u64 SYSCALL_ALLOC = 0x4;
const u64 SYSCALL_FREE = 0x5;
const u64 SYSCALL_POLL_KEYBOARD = 0x6;
const u64 SYSCALL_TTY_WRITE = 0x7;
const u64 SYSCALL_TTY_INFO = 0x8;
const u64 SYSCALL_TTY_SET_CURSOR = 0x9;
const u64 SYSCALL_TTY_SET_CMDLINE = 0xa;
const u64 SYSCALL_TTY_FLUSH = 0xb;
const u64 SYSCALL_TTY_SCROLL = 0xc;
const u64 SYSCALL_TTY_FLUSH_CMDLINE = 0xd;
const u64 SYSCALL_PWD_LENGTH = 0xe;
const u64 SYSCALL_GET_PWD = 0xf;
const u64 SYSCALL_SET_PWD = 0x10;
const u64 SYSCALL_STAT = 0x11;
const u64 SYSCALL_LIST_DIR_BUF_SIZE = 0x12;
const u64 SYSCALL_LIST_DIR = 0x13;
const u64 SYSCALL_FS_READ = 0x14;
const u64 SYSCALL_FS_CREATE_FILE = 0x15;
const u64 SYSCALL_FS_CREATE_DIR = 0x16;
const u64 SYSCALL_FS_RM_FILE = 0x17;
const u64 SYSCALL_FS_RM_DIR = 0x18;
const u64 SYSCALL_FS_MV = 0x19;
const u64 SYSCALL_FS_WRITE = 0x1a;
const u64 SYSCALL_FS_TRUNC = 0x1b;
const u64 SYSCALL_FS_IS_SAME_PATH = 0x1c;
const u64 SYSCALL_FS_IS_DIR_PATH = 0x1d;

// NOTE: if these started from 0 then they can't be combined like EXEC_IS_BLOCKING | EXEC_CAN_BE_ORPHANED
const u64 EXEC_IS_BLOCKING = 0x1;
const u64 EXEC_CAN_BE_ORPHANED = 0x2;

const u64 SYS_SUCCESS = 0x0;
const u64 SYS_BAD_PATH = 0X1;
const u64 SYS_FILE_NOT_FOUND = 0x2;
const u64 SYS_FILE_ALREADY_EXISTS = 0x3;

void sys_yield();

void sys_exit();

// NOTE: len does not include the null terminator
void sys_print(const char *str, u64 len);

void *sys_alloc(u64 size, u64 align);

void sys_free(void *ptr);

struct PollKeyboardResult
{
    bool has_result = false;
    KeyEvent event;
};
void sys_poll_keyboard(PollKeyboardResult *result);

    //new ((void *)g_init_process) Process("/userspace/test.elf", false, false, "/");
int sys_exec(const char *bin_path, char **argv, u64 flags);

void sys_tty_write(const char *str, int len);
void sys_tty_write(const char *str);

struct TTYInfo
{
    int cmdline_size = 0;
    int scrollback_buffer_size = 0;
};
void sys_tty_info(TTYInfo *info);

void sys_tty_set_cursor(u32 i);

void sys_tty_set_cmdline(char *cmdline);

void sys_tty_flush();

void sys_tty_scroll(int amount);

void sys_tty_flush_cmdline();

int sys_pwd_length();

void sys_get_pwd(char *dest, int len);

int sys_set_pwd(char *pwd);

FileStatResult sys_stat(const char *path);

int sys_list_dir_buf_size(const char *path);

void sys_list_dir(const char *path, char *buf, int buf_size, char **buf_one_past_end);

// TODO switch the u8, u64 params to standard C types
u16 sys_fs_read(const char *path, char *buffer, int offset, int size);
u16 sys_fs_write(const char *path, char *buffer, int offset, int size);

u16 sys_fs_create_file(const char *path);
u16 sys_fs_create_dir(const char *path);

u16 sys_fs_rm_file(const char *path);
u16 sys_fs_rm_dir(const char *path);

u16 sys_fs_mv(const char *src, const char *dst);

u16 sys_fs_truncate(const char *path, int new_size);

bool sys_is_same_path(const char *path1, const char *path2);
bool sys_is_dir_path(const char *path);
