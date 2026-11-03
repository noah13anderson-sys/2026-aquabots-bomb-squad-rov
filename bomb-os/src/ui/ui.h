#ifndef UI_H #define UI_H

#include <stdint.h> #include <stdbool.h> #include "lvgl.h"

int ui_init(int width, int height);

void ui_deinit(void);

void ui_set_camera_connected(bool connected);

void ui_keyboard_input(uint32_t key);

void ui_set_camera_frame(const void *pixels);

void ui_set_camera_info(const char *info);

void ui_set_bluetooth_info(const char *info);

void ui_set_status(const char *status);

#endif