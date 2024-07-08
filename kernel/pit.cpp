#pragma once
#include "kernel/pit.h"
#include "kernel/asm.cpp"
#include "kernel/pic.h"

// based on code and info from
//  https://wiki.osdev.org/Programmable_Interval_Timer
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Time/PIT.cpp

PIT g_pit;
extern PIC g_pic;

void PIT::initialize()
{
    out8(PIT_CMD, SELECT0 | MODE_SQUARE_WAVE | ACCESS_MODE_WORD);
    out8(TIMER0_DATA, FREQUENCY & 0xff);
    out8(TIMER0_DATA, (FREQUENCY >> 8) & 0xff);
    g_pic.unmask_irq(0);
}
