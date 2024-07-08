#pragma once
#include "kernel/ps2_keyboard.h"
#include "kernel/pic.h"
#include "kernel/asm.cpp"
#include "include/key_event.h"
#include "kernel/circular_buffer.h"

// based on code and info from
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Time/PIT.cpp

extern PIC g_pic;

PS2Keyboard g_ps2_keyboard;

/***************************************************************************/
CircularBuffer<KeyEvent, 256> g_key_events;
/***************************************************************************/
u8 prefix_state = 0;
enum PrefixState
{
    NO_PREFIX,
    PREFIX_0XF0,
    PREFIX_0XE0,
    PREFIX_0XE0_0XF0,
};

kb_modifier_t g_active_modifiers = 0;

// this is identical for no prefix and 0xf0 prefix, only
// * no prefix means key is pressed
// * 0xf0 prefix means key is released
KeyCode scancode_to_key_no_prefix[256] =
{
    KEY_INVALID, KEY_F9,  KEY_INVALID, KEY_F5,      // 0x00, 0x01, 0x02, 0x03,
    KEY_F3,      KEY_F1,  KEY_F2,      KEY_F12,     // 0x04, 0x05, 0x06, 0x07,
    KEY_INVALID, KEY_F10, KEY_F8,      KEY_F6,      // 0x08, 0x09, 0x0a, 0x0b,
    KEY_F4,      KEY_TAB, KEY_TILDE,   KEY_INVALID, // 0x0c, 0x0d, 0x0e, 0x0f,

    KEY_INVALID,   KEY_LEFT_ALT, KEY_LEFT_SHIFT, KEY_INVALID, // 0x10, 0x11, 0x12, 0x13,
    KEY_LEFT_CTRL, KEY_Q,        KEY_1,          KEY_INVALID, // 0x14, 0x15, 0x16, 0x17,
    KEY_INVALID,   KEY_INVALID,  KEY_Z,          KEY_S,       // 0x18, 0x19, 0x1a, 0x1b,
    KEY_A,         KEY_W,        KEY_2,          KEY_INVALID, // 0x1c, 0x1d, 0x1e, 0x1f,

    KEY_INVALID,  KEY_C,     KEY_X, KEY_D,       // 0x20, 0x21, 0x22, 0x23,
    KEY_E,        KEY_4,     KEY_3, KEY_INVALID, // 0x24, 0x25, 0x26, 0x27,
    KEY_INVALID,  KEY_SPACE, KEY_V, KEY_F,       // 0x28, 0x29, 0x2a, 0x2b,
    KEY_T,        KEY_R,     KEY_5, KEY_INVALID, // 0x2c, 0x2d, 0x2e, 0x2f,

    KEY_INVALID,  KEY_N,       KEY_B, KEY_H,       // 0x30, 0x31, 0x32, 0x33,
    KEY_G,        KEY_Y,       KEY_6, KEY_INVALID, // 0x34, 0x35, 0x36, 0x37,
    KEY_INVALID,  KEY_INVALID, KEY_M, KEY_J,       // 0x38, 0x39, 0x3a, 0x3b,
    KEY_U,        KEY_7,       KEY_8, KEY_INVALID, // 0x3c, 0x3d, 0x3e, 0x3f,

    KEY_INVALID,   KEY_COMMA, KEY_K,     KEY_I,       // 0x40, 0x41, 0x42, 0x43,
    KEY_O,         KEY_0,     KEY_9,     KEY_INVALID, // 0x44, 0x45, 0x46, 0x47,
    KEY_INVALID,   KEY_DOT,   KEY_SLASH, KEY_L,       // 0x48, 0x49, 0x4a, 0x4b,
    KEY_SEMICOLON, KEY_P,     KEY_MINUS, KEY_INVALID, // 0x4c, 0x4d, 0x4e, 0x4f,

    KEY_INVALID,             KEY_INVALID,     KEY_APOSTROPHE, KEY_INVALID,              // 0x50, 0x51, 0x52, 0x53,
    KEY_LEFT_SQAURE_BRACKET, KEY_EQUAL,       KEY_INVALID,    KEY_INVALID,              // 0x54, 0x55, 0x56, 0x57,
    KEY_CAPSLOCK,            KEY_RIGHT_SHIFT, KEY_ENTER,      KEY_RIGHT_SQUARE_BRACKET, // 0x58, 0x59, 0x5a, 0x5b,
    KEY_INVALID,             KEY_BACKSLASH,   KEY_INVALID,    KEY_INVALID,              // 0x5c, 0x5d, 0x5e, 0x5f,

    KEY_INVALID, KEY_INVALID, KEY_INVALID,   KEY_INVALID, // 0x60, 0x61, 0x62, 0x63,
    KEY_INVALID, KEY_INVALID, KEY_BACKSPACE, KEY_INVALID, // 0x64, 0x65, 0x66, 0x67,
    KEY_INVALID, KEY_KEYPAD,  KEY_INVALID,   KEY_KEYPAD,  // 0x68, 0x69, 0x6a, 0x6b,
    KEY_KEYPAD,  KEY_INVALID, KEY_INVALID,   KEY_INVALID, // 0x6c, 0x6d, 0x6e, 0x6f,

    KEY_KEYPAD, KEY_KEYPAD, KEY_KEYPAD,     KEY_KEYPAD,  // 0x70, 0x71, 0x72, 0x73,
    KEY_KEYPAD, KEY_KEYPAD, KEY_ESCAPE,     KEY_NUMLOCK, // 0x74, 0x75, 0x76, 0x77,
    KEY_F11,    KEY_KEYPAD, KEY_KEYPAD,     KEY_KEYPAD,  // 0x78, 0x79, 0x7a, 0x7b,
    KEY_KEYPAD, KEY_KEYPAD, KEY_SCROLLLOCK, KEY_INVALID, // 0x7c, 0x7d, 0x7e, 0x7f,

    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_7, // 0x80, 0x81, 0x82, 0x83,

    // [0x84, 0x8f]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0x90, 0x9f]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xa0, 0xaf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xb0, 0xbf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xc0, 0xcf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xd0, 0xdf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xe0, 0xef]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xf0, 0xff]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
};

// TODO keys unaccounted for in this code
// * print screen pressed
// * print screen released
// * pause pressed

// this is identical for 0xe0 prefix and 0xe0, 0xf0 prefix, only
// * 0xe0 prefix means key is pressed
// * 0xe0, 0xf0 prefix means key is released
KeyCode scancode_to_key_0xe0_prefix[256] =
{
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x00, 0x01, 0x02, 0x03
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x04, 0x05, 0x06, 0x07
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x08, 0x09, 0x0a, 0x0b
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x0c, 0x0d, 0x0e, 0x0f

    KEY_MULTIMEDIA, KEY_RIGHT_ALT, KEY_INVALID, KEY_INVALID, // 0x10, 0x11, 0x12, 0x13
    KEY_RIGHT_CTRL, KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, // 0x14, 0x15, 0x16, 0x17
    KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x18, 0x19, 0x1a, 0x1b
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_LEFT_WIN, // 0x1c, 0x1d, 0x1e, 0x1f

    KEY_MULTIMEDIA, KEY_MULTIMEDIA, KEY_INVALID, KEY_MULTIMEDIA, // 0x20, 0x21, 0x22, 0x23
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_RIGHT_WIN, // 0x24, 0x25, 0x26, 0x27
    KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, KEY_MULTIMEDIA, // 0x28, 0x29, 0x2a, 0x2b
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_MULTIMEDIA, // 0x2c, 0x2d, 0x2e, 0x2f

    KEY_MULTIMEDIA, KEY_INVALID, KEY_MULTIMEDIA, KEY_INVALID, // 0x30, 0x31, 0x32, 0x33
    KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, KEY_ACPI, // 0x34, 0x35, 0x36, 0x37
    KEY_MULTIMEDIA, KEY_INVALID, KEY_MULTIMEDIA, KEY_MULTIMEDIA, // 0x38, 0x39, 0x3a, 0x3b
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_ACPI, // 0x3c, 0x3d, 0x3e, 0x3f

    KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x40, 0x41, 0x42, 0x43
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x44, 0x45, 0x46, 0x47
    KEY_MULTIMEDIA, KEY_INVALID, KEY_KEYPAD, KEY_INVALID, // 0x48, 0x49, 0x4a, 0x4b
    KEY_INVALID, KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, // 0x4c, 0x4d, 0x4e, 0x4f

    KEY_MULTIMEDIA, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x50, 0x51, 0x52, 0x53
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x54, 0x55, 0x56, 0x57
    KEY_INVALID, KEY_INVALID, KEY_KEYPAD, KEY_INVALID, // 0x58, 0x59, 0x5a, 0x5b
    KEY_INVALID, KEY_INVALID, KEY_ACPI, KEY_INVALID, // 0x5c, 0x5d, 0x5e, 0x5f

    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x60, 0x61, 0x62, 0x63
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x64, 0x65, 0x66, 0x67
    KEY_INVALID, KEY_END, KEY_INVALID, KEY_LEFT, // 0x68, 0x69, 0x6a, 0x6b
    KEY_HOME, KEY_INVALID, KEY_INVALID, KEY_INVALID, // 0x6c, 0x6d, 0x6e, 0x6f

    KEY_INSERT, KEY_DELETE, KEY_DOWN, KEY_INVALID, // 0x70, 0x71, 0x72, 0x73
    KEY_RIGHT, KEY_UP, KEY_INVALID, KEY_INVALID, // 0x74, 0x75, 0x76, 0x77
    KEY_INVALID, KEY_INVALID, KEY_PAGE_DOWN, KEY_INVALID, // 0x78, 0x79, 0x7a, 0x7b
    KEY_INVALID, KEY_PAGE_UP, KEY_INVALID, KEY_INVALID, // 0x7c, 0x7d, 0x7e, 0x7f

    // [0x80, 0x8f]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0x90, 0x9f]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xa0, 0xaf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xb0, 0xbf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xc0, 0xcf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xd0, 0xdf]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xe0, 0xef]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,

    // [0xf0, 0xff]
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
    KEY_INVALID, KEY_INVALID, KEY_INVALID, KEY_INVALID,
};

struct ProcessScancodeResult
{
    bool has_event;
    KeyEvent event;
};
ProcessScancodeResult process_scancode(u8 scancode)
{
    KeyEvent event;

    // scancodes fall into 2 categories:
    // * no prefix:
    //      * no-prefix: key pressed
    //      * 0xf0 prefix: key released
    //      * modifier keys: left alt, left ctrl, left shift, right shift
    // * 0xe0 prefix:
    //      * 0xe0 prefix: key pressed
    //      * 0xe0, 0xf0 prefix: key released
    //      * modifier keys: right alt, right ctrl, left win, right win
    switch(scancode) {
        case 0xe0: {
            ASSERT(prefix_state == PrefixState::NO_PREFIX);
            prefix_state = PrefixState::PREFIX_0XE0;

            return {false, {}};
        } break;

        case 0xf0: {
            if(prefix_state == PrefixState::NO_PREFIX)
                prefix_state = PrefixState::PREFIX_0XF0;
            else if(prefix_state == PrefixState::PREFIX_0XE0)
                prefix_state = PrefixState::PREFIX_0XE0_0XF0;
            else
                UNREACHABLE();

            return {false, {}};
        } break;

        default: {
            KeyCode key = KEY_INVALID;
            bool is_pressed = false;

            if(prefix_state == PrefixState::NO_PREFIX || prefix_state == PrefixState::PREFIX_0XF0) {
                is_pressed = prefix_state == PrefixState::NO_PREFIX;
                key = scancode_to_key_no_prefix[scancode];
            } else if(prefix_state == PrefixState::PREFIX_0XE0 || prefix_state == PrefixState::PREFIX_0XE0_0XF0) {
                is_pressed = prefix_state == PrefixState::PREFIX_0XE0;
                key = scancode_to_key_0xe0_prefix[scancode];
            }

            event = {
                key,
                is_pressed,
                g_active_modifiers
            };

            dbg_str("MODIFIERS: "); dbg_uint((u32)event.modifiers); dbg_str("\n");
            // update modifiers
            if(key < KEY_HOLD_MODIFIERS_END) {
                kb_modifier_t bit = (1 << (u8)key);
                if(is_pressed)
                    g_active_modifiers |= bit;
                else
                    g_active_modifiers &= ~bit;
            } else if(key < KEY_LOCK_MODIFIERS_END) {
                kb_modifier_t bit = (1 << (u8)(key-1));
                if(is_pressed)
                    g_active_modifiers ^= bit;
            }

            prefix_state = PrefixState::NO_PREFIX;

            return {true, event};
        } break;
    }
}

/***************************************************************************/

void PS2Keyboard::initialize()
{
    // https://wiki.osdev.org/%228042%22_PS/2_Controller
    // disable ps2 ports
    // disable devices
    wait_write(CMD_STATUS_PORT, COMMAND_DISABLE_PORT1);
    wait_write(CMD_STATUS_PORT, COMMAND_DISABLE_PORT2);

    // disable interrupts and translation
    wait_write(CMD_STATUS_PORT, COMMAND_READ_CONFIG);
    u8 config = wait_read(DATA_PORT);
    config &= ~CONFIG_PORT1_INTERRUPT;
    config &= ~CONFIG_PORT2_INTERRUPT;
    config &= ~CONFIG_PORT1_TRANSLATION; // TODO should this be enabled
    is_dual_channel = (config & CONFIG_DISABLE_PORT2_CLOCK) != 0;

    wait_write(CMD_STATUS_PORT, COMMAND_WRITE_CONFIG);
    wait_write(DATA_PORT, config);

    // send 0xaa, check for 0x55, restore config byte
    wait_write(CMD_STATUS_PORT, COMMAND_CONTROLLER_TEST);
    u8 result = wait_read(DATA_PORT);
    ASSERT(result == CONTROLLER_TEST_SUCCESS);

    wait_write(CMD_STATUS_PORT, COMMAND_WRITE_CONFIG);
    wait_write(DATA_PORT, config);

    // use command 0xab to test port1
    wait_write(CMD_STATUS_PORT, COMMAND_TEST_PORT1);
    u8 port1_result = wait_read(DATA_PORT);
    ASSERT(port1_result == 0);

    // enable port1 with command 0xae and enable interrupts with config byte bit 0
    wait_write(CMD_STATUS_PORT, COMMAND_ENABLE_PORT1);
    config |= CONFIG_PORT1_INTERRUPT;
    config &= ~CONFIG_DISABLE_PORT1_CLOCK;
    wait_write(CMD_STATUS_PORT, COMMAND_WRITE_CONFIG);
    wait_write(DATA_PORT, config);

    // send reset command to port1 device (0xff) ->response 0xfa means success
    reset_device(0x60);
    g_pic.unmask_irq(1);
}

void PS2Keyboard::reset_device(u16 port)
{
    out8(port, DEVICE_RESET);
    u8 result1 = wait_read(DATA_PORT);
    u8 result2 = wait_read(DATA_PORT);
    ASSERT(result1 == DEVICE_TEST_SUCCESS_BYTE1);
    ASSERT(result2 == DEVICE_TEST_SUCCESS_BYTE2);
}

void PS2Keyboard::handle_irq()
{
    u8 byte = in8(DATA_PORT);
    dbg_str("KEYBOARD IRQ "); dbg_uint(byte); dbg_str("\n");

// TODO implement processing of scan code set 2 
// https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Set_2
// NOTE: if key K has scancode N
//       * K pressed fires irq with scancode N
//       * K released fires irq with scancode 0xf0, followed by irq with scancode N
// NOTE: if capslock or shift is pressed, keycode K will not change, so
//       driver must keep track of shift key, capslock, alt key, etc.
// NOTE: if you hold K, the keyboard will repeatedly trigger irqs for K

    ProcessScancodeResult res = process_scancode(byte);
    if(res.has_event) {
        g_key_events.push_end(res.event);
        /*
        KeyEvent event = res.event;
        if(is_ascii_event(event)) {
            char c = keyevent_to_ascii(event);
            ASSERT(c != 0);
            const char s[] = { c, '\n', 0 };
            dbg_str("ASCII: "); dbg_str(s);
            vga_print("ASCII: "); vga_print(s);
        }
        */
    }

    //UNREACHABLE();  // TODO send KeyEvent to process
                    //      KeyEvent can be converted to ascii value in process (make lib file for this)
                    //      to simplify, process should poll for keys, not be interrupted like how Windows does it

}

void PS2Keyboard::wait_write(u16 port, u8 val)
{
    while(1) {
        u8 status = in8(CMD_STATUS_PORT);
        if(!(status & STATUS_INPUT_FULL))
            break;
    }
    out8(port, val);
}

u8 PS2Keyboard::wait_read(u16 port)
{
    while(1) {
        u8 status = in8(CMD_STATUS_PORT);
        if(!(status & STATUS_OUTPUT_FULL))
            break;
    }
    u8 byte = in8(port);
    return byte;
}
