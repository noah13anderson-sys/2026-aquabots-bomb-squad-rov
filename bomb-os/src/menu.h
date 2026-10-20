#ifndef MENU_H
#define MENU_H

#include <stddef.h>

typedef enum {
    MENU_ACTION_NONE,
    MENU_ACTION_SUBMENU,
    MENU_ACTION_EXIT,
    MENU_ACTION_EXEC,
} MenuAction;

typedef struct Menu Menu;

typedef struct {
    const char *label;
    MenuAction action;
    const Menu *submenu;
    const void *exec;
} MenuOption;

struct Menu {
    const char *title;
    const MenuOption *options;
    size_t option_count;
};

#endif