#pragma once
#include "kernel/types.h"

// based on code and info from
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Time/PIT.cpp

struct PS2Keyboard
{
    void initialize();
    void handle_irq();
    void reset_device(u16);
    void wait_write(u16, u8);
    u8 wait_read(u16);

    bool is_dual_channel = false;

    const u64 DATA_PORT = 0x60;
    const u64 CMD_STATUS_PORT = 0x64;

    const u64 STATUS_OUTPUT_FULL = 0x0; // must be clear before attempting read
    const u64 STATUS_INPUT_FULL = 1 << 1; // must be clear before attempting write
    const u64 STATUS_SYS_FLAG = 1 << 2;
    const u64 STATUS_CONTROLLER_DEVICE_SELECT = 1 << 3; // 0 = data writes go to PS2 device, 1 = data writes go to PS2 controller command
    const u64 STATUS_TIMEOUT_ERROR = 1 << 6;
    const u64 STATUS_PARITY_ERROR = 1 << 7;

    const u64 CONFIG_PORT1_INTERRUPT = 0;
    const u64 CONFIG_PORT2_INTERRUPT = 1 << 1;
    const u64 CONFIG_POST_TEST_PASSED = 1 << 2;
    const u64 CONFIG_DISABLE_PORT1_CLOCK = 1 << 4;
    const u64 CONFIG_DISABLE_PORT2_CLOCK = 1 << 5;
    const u64 CONFIG_PORT1_TRANSLATION = 1 << 6;

    const u64 COMMAND_READ_CONFIG = 0x20;
    const u64 COMMAND_WRITE_CONFIG = 0x60;
    const u64 COMMAND_DISABLE_PORT1 = 0xad;
    const u64 COMMAND_DISABLE_PORT2 = 0xa7;
    const u64 COMMAND_CONTROLLER_TEST = 0xaa;
    const u64 COMMAND_TEST_PORT1 = 0xab;
    const u64 COMMAND_ENABLE_PORT1 = 0xae;

    const u64 DEVICE_RESET = 0xff;

    const u8 CONTROLLER_TEST_SUCCESS = 0x55;

    const u8 DEVICE_TEST_SUCCESS_BYTE1 = 0xfa;
    const u8 DEVICE_TEST_SUCCESS_BYTE2 = 0xaa;
};
