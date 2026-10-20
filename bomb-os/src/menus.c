#include "menu.h"
#include "menus.h"

extern const Menu main_menu;
extern const Menu settings_menu;

static const MenuOption settings_options[] = {
    { "Back", MENU_ACTION_SUBMENU, NULL, NULL }
};

const Menu settings_menu = {
    "Settings",
    settings_options,
    sizeof(settings_options) / sizeof(settings_options[0])
};

static const MenuOption main_options[] = {
    { "Settings", MENU_ACTION_SUBMENU, &settings_menu, NULL }
};

const Menu main_menu = {
    "Bomb OS",
    main_options,
    sizeof(main_options) / sizeof(main_options[0])
};