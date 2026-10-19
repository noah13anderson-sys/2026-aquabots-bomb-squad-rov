#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef enum {
    MENU_ACTION_NONE,
    MENU_ACTION_SUBMENU,
    MENU_ACTION_EXIT
} MenuAction;

typedef struct Menu Menu;

typedef struct {
    const char *label;
    MenuAction action;
    const Menu *submenu;
} MenuOption;

struct Menu {
    const char *title;
    const MenuOption *options;
    size_t option_count;
};
extern const Menu main_menu;
extern const Menu settings_menu;

static const MenuOption settings_options[] = {
    { "Back", MENU_ACTION_SUBMENU, NULL }
};

const Menu settings_menu = {
    "Settings",
    settings_options,
    sizeof(settings_options) / sizeof(settings_options[0])
};

static const MenuOption main_options[] = {
    { "Settings", MENU_ACTION_SUBMENU, &settings_menu }
};

const Menu main_menu = {
    "Bomb OS",
    main_options,
    sizeof(main_options) / sizeof(main_options[0])
};

int main() {
    
}