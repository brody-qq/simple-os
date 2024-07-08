// based off code from https://wiki.osdev.org/Bare_Bones
#pragma once
#include "types.h"

// TODO are these numbers guaranteed?
const u32 VGA_WIDTH = 80;
const u32 VGA_HEIGHT = 25;
const u32 VGA_CELLS = VGA_WIDTH * VGA_HEIGHT;
u16 *s_vga_buffer = (u16 *)0xB8000; // TODO should this be a volatile * ?
static u32 s_vga_col = 0;
static u32 s_vga_row = 0;

enum vga_color
{
    VGA_BLACK = 0,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LIGHT_GREY,
    VGA_DARK_GREY,
    VGA_LIGHT_BLUE,
    VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,
    VGA_LIGHT_MAGENTA,
    VGA_LIGHT_BROWN,
    VGA_WHITE,
};

inline u16 vga_entry(u8 color, u8 ch)
{
    u16 val = (color << 8) | ch;
    return val;
}

inline u16 vga_entry(u8 fg_color, u8 bg_color, u8 ch)
{
    u16 val = (((bg_color << 4) | fg_color) << 8) | ch;
    return val;
}

void vga_initialize()
{
    // TODO add color
    for (u32 ind = 0; ind < VGA_CELLS; ++ind)
        s_vga_buffer[ind] = vga_entry(VGA_BLACK, ' ');
}

static inline void inc_row()
{
    s_vga_row++;
    if (s_vga_row >= VGA_HEIGHT)
        s_vga_row = 0;
    return;
}

static inline void inc_col()
{
    s_vga_col++;
    if (s_vga_col >= VGA_WIDTH)
        s_vga_col = 0;
    return;
}

static inline void inc_cursor()
{
    s_vga_col++;
    if (s_vga_col >= VGA_WIDTH) {
        s_vga_col = 0;
        inc_row();
    }
    return;
}

void clear_row(int row)
{
    u64 row_start = row*VGA_WIDTH;
    for(u32 i = 0; i < VGA_WIDTH; ++i) {
        u32 index = row_start + i;
        s_vga_buffer[index] = vga_entry(VGA_WHITE, ' ');
    }
}

void vga_print(const char *str)
{
    for (; *str != 0; str++) {
        if (*str == '\n') {
            s_vga_col = 0;
            inc_row();
            clear_row(s_vga_row);
            continue;
        }

        u32 index = s_vga_row * VGA_WIDTH + s_vga_col;
        s_vga_buffer[index] = vga_entry(VGA_WHITE, *str);
        inc_cursor();
    }
}
