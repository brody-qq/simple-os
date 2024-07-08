#pragma once
#include "include/types.h"

enum KeyCode
{
    // TODO these must be at start of Key enum so their values match the
    //      left shift of the Modifier values
    KEY_LEFT_SHIFT = 0, KEY_RIGHT_SHIFT,
    KEY_LEFT_CTRL, KEY_RIGHT_CTRL,
    KEY_LEFT_ALT, KEY_RIGHT_ALT,
    KEY_LEFT_WIN, KEY_RIGHT_WIN,
    KEY_HOLD_MODIFIERS_END, 

    KEY_CAPSLOCK, KEY_SCROLLLOCK, KEY_NUMLOCK,
    KEY_LOCK_MODIFIERS_END, // used to check if(key < KEY_MODIFIERS_END) { ... }

    KEY_INVALID,
    KEY_KEYPAD, // keypad keys are all grouped under one value since they are unsupported
    KEY_MULTIMEDIA, // multimedia keys are all grouped under one value since they are unsupported
    KEY_ACPI, // ACPI keys are all grouped under one value since they are unsupported

    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_BACKSPACE,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4,
    KEY_F5, KEY_F6, KEY_F7, KEY_F8,
    KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_ESCAPE,
    KEY_END, KEY_HOME, KEY_INSERT, KEY_DELETE,
    KEY_PAGE_DOWN, KEY_PAGE_UP,

    KEY_ASCII_START,
    KEY_A, KEY_B, KEY_C, KEY_D,
    KEY_E, KEY_F, KEY_G, KEY_H,
    KEY_I, KEY_J, KEY_K, KEY_L,
    KEY_M, KEY_N, KEY_O, KEY_P,
    KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X,
    KEY_Y, KEY_Z,

    KEY_ALPHABETICAL_END,

    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_TILDE,
    KEY_TAB,
    KEY_ENTER,
    KEY_LEFT_SQAURE_BRACKET, KEY_RIGHT_SQUARE_BRACKET,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_DOT,
    KEY_COMMA,
    KEY_SLASH, KEY_BACKSLASH, 
    KEY_SPACE,
    KEY_ASCII_END,

    KEY_COUNT
};

// NOTE: format of this table is:
//       key_to_ascii[0][keycode] = lowercase char for keycode
//       key_to_ascii[1][keycode] = uppercase char for keycode
constexpr char keycode_to_ascii[2][KEY_COUNT] =
{
    // lowercase
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0,

        0, 0, 0,
        0,

        0,
        0,
        0,
        0,

        0, 0, 0, 0,
        0, 
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 
        0, 0, 0, 0,
        0, 0,

        0,
        'a', 'b', 'c', 'd',
        'e', 'f', 'g', 'h',
        'i', 'j', 'k', 'l',
        'm', 'n', 'o', 'p',
        'q', 'r', 's', 't',
        'u', 'v', 'w', 'x',
        'y', 'z',

        0,

        '0', '1', '2', '3', '4',
        '5', '6', '7', '8', '9',
        '-',
        '=',
        '`',
        '\t',
        '\n',
        '[', ']',
        ';',
        '\'',
        '.',
        ',',
        '/',
        '\\',
        ' ',
        0,
    },

    // uppercase
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0,

        0, 0, 0,
        0,

        0,
        0,
        0,
        0,

        0, 0, 0, 0,
        0, 
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 
        0, 0, 0, 0,
        0, 0,

        0,
        'A', 'B', 'C', 'D',
        'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L',
        'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X',
        'Y', 'Z',

        0,

        ')', '!', '@', '#', '$',
        '%', '^', '&', '*', '(',
        '_',
        '+',
        '~',
        '\t',
        '\n',
        '{', '}',
        ':',
        '"',
        '>',
        '<',
        '?',
        '|',
        ' ',
        0,
    }
};

typedef u16 kb_modifier_t;
enum KeyboardModifiers : kb_modifier_t
{
    LEFT_SHIFT_HELD  = (1 << 0),
    RIGHT_SHIFT_HELD = (1 << 1),

    LEFT_CTRL_HELD   = (1 << 2),
    RIGHT_CTRL_HELD  = (1 << 3),

    LEFT_ALT_HELD    = (1 << 4),
    RIGHT_ALT_HELD   = (1 << 5),

    LEFT_WIN_HELD    = (1 << 6),
    RIGHT_WIN_HELD   = (1 << 7),

    CAPSLOCK_HELD    = (1 << 8),
    SCROLLLOCK_HELD  = (1 << 9),
    NUMLOCK_HELD     = (1 << 10),
};

struct KeyEvent
{
    KeyCode key;
    bool pressed;
    kb_modifier_t modifiers;
};


bool is_shift_pressed(KeyEvent event);

bool is_ctrl_pressed(KeyEvent event);

bool is_alt_pressed(KeyEvent event);

bool is_win_pressed(KeyEvent event);

bool is_numlock_on(KeyEvent event);

bool is_capslock_on(KeyEvent event);

bool is_scrolllock_on(KeyEvent event);

bool should_be_uppercase(KeyEvent event);


bool is_ascii_event(KeyEvent event);

char keyevent_to_ascii(KeyEvent event);
