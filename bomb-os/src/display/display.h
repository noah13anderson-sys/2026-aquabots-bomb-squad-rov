#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

int display_init(void);

void display_poll(void);

void display_deinit(void);

uint32_t display_get_width(void);

uint32_t display_get_height(void);

bool display_is_ready(void);

#endif