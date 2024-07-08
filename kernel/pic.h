#pragma once
#include "kernel/types.h"
#include "kernel/asm.cpp"

// based on info and code from
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Interrupts/PIC.cpp
//  https://wiki.osdev.org/8259_PIC

struct PIC
{
    static const u16 PIC1_CMD = 0x20;
    static const u16 PIC1_DATA = 0x21;

    static const u16 PIC2_CMD = 0xa0;
    static const u16 PIC2_DATA = 0xa1;

    static const u8 READ_IRR = 0xa;
    static const u8 READ_ISR = 0xb;

    static const u8 PIC2_LINE = 0x2; // the vector number for PIC2 on the PIC1 chip
    static const u8 EOI = 0x20;

    static const u8 PIC1_BASE_VECTOR = 0x20;
    static const u8 PIC2_BASE_VECTOR = 0x28;

    // used by interrupt handlers to see if a vector is in the vector range for the pic
    static const u64 BASE_VECTOR = PIC1_BASE_VECTOR;
    static const u64 VECTORS_ONE_PAST_END = PIC2_BASE_VECTOR + 0x8;

    void initialize(u8, u8);
    void mask_irq(u8);
    void unmask_irq(u8);
    void eoi(u8);
    u16 isr();
    u16 irr();
    bool handle_spurious_interrupt(u8);
};
