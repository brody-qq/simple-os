#pragma once
#include "kernel/types.h"
#include "include/string.h"

void out8(u16, u8);
u8 in8(u16);
void vga_print(const char *str);

// code from https://wiki.osdev.org/Serial_Ports#Initialization and https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/DebugOutput.cpp
#define COM1 0x3f8
void init_serial()
{
    out8(COM1 + 1, 0x00); // Disable all interrupts
    out8(COM1 + 3, 0x80); // Enable DLAB (set baud rate divisor)
    out8(COM1 + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    out8(COM1 + 1, 0x00); //                  (hi byte)
    out8(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
    out8(COM1 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    out8(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void dbg_putch(char ch)
{
    while((in8(COM1 + 5) & 0x20) == 0) {}
    out8(COM1, ch);
}

// TODO make proper format string print function
void dbg_str(const char *str)
{
    while(*str != 0)
        dbg_putch(*str++);
}

void dbg_uint(u64 num)
{
    const u32 max_u64_digits = 20;
    char numstr[max_u64_digits+1] = {0};
    char *str = uint_to_str(num, numstr, max_u64_digits);

    dbg_str(str);
}

void dbg_int(s64 n)
{
    if(n < 0) {
        dbg_putch('-');
        n = -n;
    }
    dbg_uint(n);
}

// based on code from https://github.com/SerenityOS/serenity/blob/master/Kernel/Library/Assertions.h

[[noreturn]] void __kernel_panic()
{
    dbg_str("KERNEL PANIC\n");
    //asm volatile("ud2");
    asm volatile(
        "cli\n"
        "loop:\n"
        "   hlt\n"
        "   jmp loop\n"
    );
    __builtin_unreachable();
}

void print_file_line_func(char const* file, unsigned line, char const* func)
{
    dbg_str("file: ");
    dbg_str(file);
    dbg_str("\n");

    dbg_str("line: ");
    dbg_uint(line);
    dbg_str("\n");

    dbg_str("func: ");
    dbg_str(func);
    dbg_str("\n");
}

[[noreturn]] void __assertion_failed(char const* expr, char const* file, unsigned line, char const* func)
{
    // TODO print backtrace
    asm volatile("cli");
    vga_print("ASSERTION FAILED\n");
    dbg_str("ASSERTION FAILED:\n");
    dbg_str("expr: ");
    dbg_str(expr);
    dbg_str("\n");

    print_file_line_func(file, line, func);

    __kernel_panic();
}
#define ASSERT(expr)                                                            \
    do {                                                                        \
        if (!static_cast<bool>(expr)) [[unlikely]]                              \
            __assertion_failed(#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    } while (0)

[[noreturn]] void __unreachable_code(char const* file, unsigned line, char const* func)
{
    // TODO print backtrace
    asm volatile("cli");
    vga_print("CODE REACHED AN UNREACHABLE() STATEMENT\n");
    dbg_str("CODE REACHED AN UNREACHABLE() STATEMENT:\n");

    print_file_line_func(file, line, func);

    __kernel_panic();
}
#define UNREACHABLE()                                                           \
    do {                                                                        \
        __unreachable_code(__FILE__, __LINE__, __PRETTY_FUNCTION__);            \
    } while (0)
