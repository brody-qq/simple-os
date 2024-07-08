#pragma once
#include "include/types.h"
#include "kernel/vga.cpp"
#include "kernel/circular_buffer.h"

struct TTY
{
    static const u32 SCROLLBACK_BUFFER_SIZE = VGA_CELLS * 5;
    CircularBuffer<u16, SCROLLBACK_BUFFER_SIZE> char_buffer; // TODO make vga_char type
    u32 scroll_amount = 0; // 0 means bottom of buffer, 1 means bottom of buffer + 1 line, etc.

    static const u32 CMD_LEN = VGA_WIDTH;
    u32 cmd_cursor = 0;
    char cmdline[CMD_LEN];

    void flush_to_vga();
    static void init_tty();
    void write_str(const char *, int);
    void scroll_up(u32);
    void scroll_down(u32);

    void set_cmdline(const char *s);
    void flush_cmdline();
    void set_cursor(u32);
};
