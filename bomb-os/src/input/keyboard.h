#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>

typedef enum {
    KEYBOARD_NONE,
    KEYBOARD_UP,
    KEYBOARD_DOWN,
    KEYBOARD_LEFT,
    KEYBOARD_RIGHT,
    KEYBOARD_ENTER,
    KEYBOARD_BACK
} KeyboardKey;

int keyboard_init(void);

KeyboardKey keyboard_poll(void);

bool keyboard_is_connected(void);

void keyboard_close(void);

#endif