#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include <linux/input.h>

#define CONTROLLER_NAME_LENGTH 128

typedef struct {
    float vertical;
    float steering;

    float forward;
    float reverse;

    bool menu_pressed;
    bool select_pressed;
    bool back_pressed;

    bool connected;

    char name[CONTROLLER_NAME_LENGTH];

} ControllerState;

typedef struct {
    int vertical_axis;
    int steering_axis;

    int reverse_axis;
    int forward_axis;

    float deadzone;

} ControllerConfig;


int controller_init(const ControllerConfig *config);

bool controller_poll(ControllerState *state);

bool controller_is_connected(void);

const char *controller_get_name(void);

void controller_close(void);

#endif