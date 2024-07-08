#include "include/key_event.h"

bool is_shift_pressed(KeyEvent event)
{
    return (event.modifiers & (LEFT_SHIFT_HELD | RIGHT_SHIFT_HELD)) != 0;
}
bool is_ctrl_pressed(KeyEvent event)
{
    return (event.modifiers & (LEFT_CTRL_HELD | RIGHT_CTRL_HELD)) != 0;
}
bool is_alt_pressed(KeyEvent event)
{
    return (event.modifiers & (LEFT_ALT_HELD | RIGHT_ALT_HELD)) != 0;
}
bool is_win_pressed(KeyEvent event)
{
    return (event.modifiers & (LEFT_WIN_HELD | RIGHT_WIN_HELD)) != 0;
}
bool is_numlock_on(KeyEvent event)
{
    return (event.modifiers & NUMLOCK_HELD) != 0;
}
bool is_capslock_on(KeyEvent event)
{
    return (event.modifiers & CAPSLOCK_HELD) != 0;
}
bool is_scrolllock_on(KeyEvent event)
{
    return (event.modifiers & SCROLLLOCK_HELD) != 0;
}
bool should_be_uppercase(KeyEvent event)
{
    bool caps = is_capslock_on(event);
    bool shift = is_shift_pressed(event);
    return (!caps && shift) || (caps && !shift);
}

bool is_ascii_event(KeyEvent event)
{
    return event.pressed && (event.key > KEY_ASCII_START && event.key < KEY_ASCII_END);
}
char keyevent_to_ascii(KeyEvent event)
{
    // NOTE: there are 2 cases to consider:
    //       * alphabetical chars -> these are uppercase if (shift XOR capslock) is held
    //       * non-alphabetical chars -> these are uppercase if (shift) is held
    // NOTE: this should not assert that event.key is in ascii range, since it is callable
    //       by user processes

    char ascii = 0;
    u8 caps = is_capslock_on(event) ? 1 : 0;
    u8 shift = is_shift_pressed(event) ? 1 : 0;

    if(event.key < KEY_ALPHABETICAL_END) {
        ascii = keycode_to_ascii[caps ^ shift][event.key];
    } else {
        ascii = keycode_to_ascii[shift][event.key];
    }

    return ascii;
}
