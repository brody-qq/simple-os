#pragma once
#include "kernel/tty.h"
#include "kernel/debug.cpp"
#include "include/math.h"
#include "kernel/vector.h"

TTY g_tty;

struct VgaStrInfo
{
    u32 end_i;
    u32 strlen;
    u32 blanks;
    u32 remainder;
    u32 lines;
};

// TODO this can probably be vectorized
void TTY::flush_to_vga()
{
    // NOTE: this is non-trivial because each str can take multiple lines
    //       in the terminal (because it can hit the edge of the vga row and 
    //       wrap multiple times), so just counting the number of newlines
    //       in the char_buffer is not sufficient

    // NOTE: the rest of code assumes strings are delimited by newlines
    //       but the last string may not have a newline
    ASSERT(CMD_LEN % VGA_WIDTH == 0);

    auto res = char_buffer.peek_end();
    bool should_pop_at_function_end = false;
    if(res.has_obj && res.obj != '\n') {
        char_buffer.push_end('\n');
        should_pop_at_function_end = true;
    }

    // TODO this loop could be much simplified if TTY stored the end_i of each
    //      string every time a '\n' character is pushed to char_buffer
    u32 count = char_buffer.count();
    u32 total_lines = 0;
    Vector<VgaStrInfo> strs{VGA_HEIGHT + scroll_amount};
    u32 cmdline_lines = CMD_LEN / VGA_WIDTH;
    for(u32 i = 0; i < count-1 && total_lines < (VGA_HEIGHT + scroll_amount - cmdline_lines);) {
        u32 end_i = count -1 -i;
        ASSERT(end_i == count -1 || char_buffer[end_i] == '\n');

        u32 start_i = end_i;
        while(start_i > 0 && char_buffer[start_i-1] != '\n') {--start_i;};
        u32 strlen = end_i - start_i;

        u32 lines = max((u64)1, round_up_divide(strlen, VGA_WIDTH));
        u32 remainder = (strlen % VGA_WIDTH);
        u32 blanks = VGA_WIDTH * lines - strlen;
        total_lines += lines;
        strs.append({end_i, strlen, blanks, remainder, lines});

        i += strlen + 1;
    }

    u32 scroll_offset = scroll_amount;
    u32 first_i = 0;
    for(u64 i = 0; i < strs.length && scroll_offset > 0; ++i) {
        auto *info = &strs[i];
        if(scroll_offset >= info->lines) {
            scroll_offset -= info->lines;
            first_i++;
            continue;
        }

        u32 adjusted_lines = info->lines - scroll_offset;
        u32 newlen = adjusted_lines * VGA_WIDTH;

        info->end_i -= info->strlen - newlen;
        info->strlen = newlen;
        info->remainder = 0;
        info->blanks = 0;
        break;
    }

// TODO do this with memcpy and memset (this is complicated because
//      you have to account for the fact that char_buffer is stored
//      as chars, not u16, and must account for the memcpy wrapping
//      the end of the circular buffer)
// TODO instread of blitting vga_entry(...), can the color values
//      and the char values be blitted in separate loops?
    u16 *ptr = s_vga_buffer + VGA_CELLS -1;
    for(u32 i = 0; i < CMD_LEN; ++i)
        *ptr-- = vga_entry(VGA_WHITE, cmdline[CMD_LEN -1 -i]);
    if(cmd_cursor < CMD_LEN)
        *(ptr+1 + cmd_cursor) = vga_entry(VGA_BLACK, VGA_WHITE, cmdline[cmd_cursor]);

    u16 blank = vga_entry(VGA_WHITE, ' ');
    for(u64 i = first_i; i < strs.length && ptr >= s_vga_buffer; ++i) {
        auto info = strs[i];

        // NOTE: this blank_space loop should never overflow the vga buffer
        for(u32 j = 0; j < info.blanks; ++j)
            *ptr-- = blank;

        u32 remaining = ptr - s_vga_buffer +1;
        u32 count = min(remaining, info.strlen);
        for(u32 j = 0; j < count; ++j)
            *ptr-- = vga_entry(VGA_WHITE, char_buffer[info.end_i -1 -j]);
    }

    while(ptr >= s_vga_buffer)
        *ptr-- = blank; 

    if(should_pop_at_function_end)
        char_buffer.pop_end();
}

void TTY::init_tty()
{
    g_tty.scroll_amount = 0;
    g_tty.set_cmdline("_UNINITIALIZED PROMPT_ ");
    g_tty.set_cursor(0);
}

void TTY::write_str(const char *s, int len)
{
    // TODO see the commented out push_buffer() in CircularBuffer if this is a bottleneck
    // TODO check strlen, skip first parts of string if string overflows entire buffer
    for(int i = 0; i < len && s[i]; ++i) {
        char_buffer.push_end(s[i]);
    }
}

void TTY::set_cmdline(const char *s)
{
    int len = min(CMD_LEN, (u32)strlen_workaround(s));
    memmove_workaround(cmdline, (void *)s, len);
    memset_workaround(cmdline + len, ' ', CMD_LEN - len);
}

void TTY::flush_cmdline()
{
    for(u32 i = 0; i < CMD_LEN; ++i)
        char_buffer.push_end(cmdline[i]);
    char_buffer.push_end('\n');
}

void TTY::set_cursor(u32 i)
{
    cmd_cursor = min(CMD_LEN, i);
}

void TTY::scroll_up(u32 amount)
{
    // TODO this should be capped to the current number of lines available in the
    //      char buffer
    amount = min(amount, VGA_HEIGHT * 5);
    scroll_amount += amount;
}

void TTY::scroll_down(u32 amount)
{
    amount = min(scroll_amount, amount);
    scroll_amount -= amount;
}
