
#pragma once
#include "kernel/types.h"

// based on code and info from https://wiki.osdev.org/Programmable_Interval_Timer

const u16 MS_PER_TICK = 4;
const u16 TICKS_PER_SECOND = 1000 / MS_PER_TICK;

struct PIT
{
    static const u16 TIMER0_DATA = 0x40;
    static const u16 PIT_CMD = 0x43;

    static const u16 SELECT0 = 0b00'000000;
    static const u16 READ_BACK = 0b11'000000;

    static const u16 ACCESS_MODE_WORD = 0b11'0000;

    static const u16 MODE_TERMINAL_COUNT = 0b000'0;
    static const u16 MODE_SQUARE_WAVE = 0b011'0;

    static const u32 BASE_FREQUENCY = 1193182;
    static const u16 FREQUENCY = BASE_FREQUENCY / TICKS_PER_SECOND;

    void initialize();
};
