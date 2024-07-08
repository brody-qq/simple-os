#pragma once
#include "kernel/pic.h"

// based on info and code from
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Interrupts/PIC.cpp
//  https://wiki.osdev.org/8259_PIC

PIC g_pic;

void PIC::initialize(u8 pic1_base_vector, u8 pic2_base_vector)
{
    const u8 ICW1_ICW4_INIT = 0x11;
    const u8 ICW4_8086 = 0x1;

    out8(PIC1_CMD, ICW1_ICW4_INIT);
    out8(PIC2_CMD, ICW1_ICW4_INIT);

    out8(PIC1_DATA, pic1_base_vector);
    out8(PIC2_DATA, pic2_base_vector);

    out8(PIC1_DATA, (1 << PIC2_LINE));
    out8(PIC2_DATA, PIC2_LINE);

    out8(PIC1_DATA, ICW4_8086);
    out8(PIC2_DATA, ICW4_8086);

    // initially all interrupts are masked except for the line connecting PIC2 to PIC1
    out8(PIC1_DATA, ~(1 << PIC2_LINE));
    out8(PIC2_DATA, 0xff);
}

void PIC::mask_irq(u8 irq)
{
    u8 port = 0;
    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    u8 mask = in8(port) | (1 << irq);
    out8(port, mask);
}

void PIC::unmask_irq(u8 irq)
{
    u8 port = 0;
    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    u8 mask = in8(port) & ~(1 << irq);
    out8(port, mask);
}

void PIC::eoi(u8 irq)
{
    if(irq >= 8) {
        out8(PIC2_CMD, EOI);
    }
    out8(PIC1_CMD, EOI);
}

u16 PIC::isr()
{
    out8(PIC1_CMD, READ_ISR);
    out8(PIC2_CMD, READ_ISR);
    u16 isr = (in8(PIC2_CMD) << 8) | in8(PIC1_CMD);
    return isr;
}

u16 PIC::irr()
{
    out8(PIC1_CMD, READ_IRR);
    out8(PIC2_CMD, READ_IRR);
    u16 irr = (in8(PIC2_CMD) << 8) | in8(PIC1_CMD);
    return irr;
}

// NOTE:
//  spurious interrupts can be:
//      - 7 if they come from PIC1
//      - 15 if they come from PIC2
//  in the case the spurious interrupt comes from PIC2, an EOI must be sent to PIC1 since
//  the PIC1 is not aware that it's IRQ2 line was set by a spurious interrupt
// 
// returns true if it was a spurious, else returns false
bool PIC::handle_spurious_interrupt(u8 irq)
{
    if(irq != 7 && irq != 15)
        return false;
    
    if(isr() & (1 << irq))
        return false;

    if(irq == 15)
        eoi(PIC2_LINE);
    
    return true;
}
