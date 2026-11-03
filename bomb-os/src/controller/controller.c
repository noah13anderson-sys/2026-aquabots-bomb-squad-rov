
#include "controller.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MAX_EVENT_DEVICES 64
#define NBITS(x) (((x) + (8 * sizeof(unsigned long)) - 1) / \
                  (8 * sizeof(unsigned long)))

typedef struct {
    int fd;

    int vertical_axis;
    int steering_axis;
    int reverse_axis;
    int forward_axis;

    struct input_absinfo abs_info[ABS_MAX + 1];
    bool axis_available[ABS_MAX + 1];

    char name[CONTROLLER_NAME_LENGTH];

    ControllerState state;
    ControllerConfig config;

} Controller;

static Controller controller = {
    .fd = -1
};

static bool has_bit(const unsigned long *bits, int bit)
{
    return (bits[bit / (8 * sizeof(unsigned long))] &
            (1UL << (bit % (8 * sizeof(unsigned long))))) != 0;
}

static float clamp_float(float value, float min, float max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static float normalize_stick(int value,
                             const struct input_absinfo *info,
                             float deadzone)
{
    float center = ((float)info->minimum + info->maximum) / 2.0f;

    float result;

    if (value < center) {
        float range = center - info->minimum;

        result = range > 0.0f
            ? ((float)value - center) / range
            : 0.0f;
    } else {
        float range = info->maximum - center;

        result = range > 0.0f
            ? ((float)value - center) / range
            : 0.0f;
    }

    result = clamp_float(result, -1.0f, 1.0f);

    if (result > -deadzone && result < deadzone)
        return 0.0f;

    if (result > 0.0f)
        result = (result - deadzone) / (1.0f - deadzone);
    else
        result = (result + deadzone) / (1.0f - deadzone);

    return clamp_float(result, -1.0f, 1.0f);
}

static float normalize_trigger(int value,
                               const struct input_absinfo *info)
{
    int range = info->maximum - info->minimum;

    if (range <= 0)
        return 0.0f;

    float result =
        ((float)value - info->minimum) / (float)range;

    return clamp_float(result, 0.0f, 1.0f);
}

static void reset_state(void)
{
    memset(&controller.state, 0, sizeof(controller.state));
}

static void disconnect_controller(void)
{
    if (controller.fd >= 0) {
        close(controller.fd);
        controller.fd = -1;
    }

    memset(controller.name, 0, sizeof(controller.name));

    reset_state();
}

static bool read_axis_info(int fd, int axis,
                           struct input_absinfo *info)
{
    if (axis < 0 || axis > ABS_MAX)
        return false;

    return ioctl(fd, EVIOCGABS(axis), info) >= 0;
}

static bool is_gamepad(int fd)
{
    unsigned long ev_bits[NBITS(EV_MAX + 1)] = {0};
    unsigned long abs_bits[NBITS(ABS_MAX + 1)] = {0};
    unsigned long key_bits[NBITS(KEY_MAX + 1)] = {0};

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0)
        return false;

    if (!has_bit(ev_bits, EV_ABS) ||
        !has_bit(ev_bits, EV_KEY))
        return false;

    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)),
              abs_bits) < 0)
        return false;

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)),
              key_bits) < 0)
        return false;

    int forward = controller.config.forward_axis;
    int reverse = controller.config.reverse_axis;

    if (!has_bit(abs_bits, forward) && forward == ABS_RZ)
        forward = ABS_GAS;

    if (!has_bit(abs_bits, reverse) && reverse == ABS_Z)
        reverse = ABS_BRAKE;

    if (!has_bit(abs_bits, controller.config.vertical_axis) ||
        !has_bit(abs_bits, controller.config.steering_axis) ||
        !has_bit(abs_bits, forward) ||
        !has_bit(abs_bits, reverse))
        return false;

    if (!has_bit(key_bits, BTN_GAMEPAD) &&
        !has_bit(key_bits, BTN_JOYSTICK) &&
        !has_bit(key_bits, BTN_SOUTH) &&
        !has_bit(key_bits, BTN_A))
        return false;

    controller.forward_axis = forward;
    controller.reverse_axis = reverse;

    return true;
}

static bool find_controller(void)
{
    char path[64];

    for (int i = 0; i < MAX_EVENT_DEVICES; i++) {

        snprintf(path, sizeof(path),
                 "/dev/input/event%d", i);

        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);

        if (fd < 0)
            continue;

        if (!is_gamepad(fd)) {
            close(fd);
            continue;
        }

        controller.fd = fd;

        controller.vertical_axis =
            controller.config.vertical_axis;

        controller.steering_axis =
            controller.config.steering_axis;

        struct {
            int axis;
            int *destination;
        } axes[] = {
            { controller.vertical_axis, NULL },
            { controller.steering_axis, NULL },
            { controller.reverse_axis, NULL },
            { controller.forward_axis, NULL }
        };

        (void)axes;

        memset(controller.axis_available, 0,
               sizeof(controller.axis_available));

        int required_axes[] = {
            controller.vertical_axis,
            controller.steering_axis,
            controller.reverse_axis,
            controller.forward_axis
        };

        for (unsigned int j = 0;
             j < sizeof(required_axes) / sizeof(required_axes[0]);
             j++) {

            int axis = required_axes[j];

            if (axis < 0 || axis > ABS_MAX ||
                !read_axis_info(fd, axis,
                                &controller.abs_info[axis])) {
                disconnect_controller();
                goto next_device;
            }

            controller.axis_available[axis] = true;
        }

        if (ioctl(fd, EVIOCGNAME(sizeof(controller.name)),
                  controller.name) < 0) {
            snprintf(controller.name,
                     sizeof(controller.name),
                     "Unknown gamepad");
        }

        reset_state();

        controller.state.connected = true;

        strncpy(controller.state.name,
                controller.name,
                sizeof(controller.state.name) - 1);

        return true;

next_device:
        continue;
    }

    return false;
}

int controller_init(const ControllerConfig *config)
{
    disconnect_controller();

    controller.config.vertical_axis = ABS_Y;
    controller.config.steering_axis = ABS_RX;

    controller.config.reverse_axis = ABS_Z;
    controller.config.forward_axis = ABS_RZ;

    controller.config.deadzone = 0.12f;

    if (config != NULL) {
        controller.config = *config;
    }

    if (controller.config.deadzone < 0.0f ||
        controller.config.deadzone >= 1.0f) {
        controller.config.deadzone = 0.12f;
    }

    return 0;
}

static void process_event(const struct input_event *event)
{
    if (event->type == EV_ABS) {

        int axis = event->code;

        if (axis < 0 || axis > ABS_MAX ||
            !controller.axis_available[axis])
            return;

        struct input_absinfo *info =
            &controller.abs_info[axis];

        switch (axis) {

            case ABS_Y:
                controller.state.vertical =
                    -normalize_stick(
                        event->value,
                        info,
                        controller.config.deadzone
                    );
                break;

            case ABS_RX:
                controller.state.steering =
                    normalize_stick(
                        event->value,
                        info,
                        controller.config.deadzone
                    );
                break;

            default:
                if (axis == controller.vertical_axis) {
                    controller.state.vertical =
                        -normalize_stick(
                            event->value, info,
                            controller.config.deadzone
                        );
                }

                if (axis == controller.steering_axis) {
                    controller.state.steering =
                        normalize_stick(
                            event->value, info,
                            controller.config.deadzone
                        );
                }

                if (axis == controller.forward_axis) {
                    controller.state.forward =
                        normalize_trigger(event->value, info);
                }

                if (axis == controller.reverse_axis) {
                    controller.state.reverse =
                        normalize_trigger(event->value, info);
                }

                break;
        }

        if (axis == controller.forward_axis) {
            controller.state.forward =
                normalize_trigger(event->value, info);
        }

        if (axis == controller.reverse_axis) {
            controller.state.reverse =
                normalize_trigger(event->value, info);
        }
    }

    if (event->type == EV_KEY) {

        bool pressed = event->value != 0;

        switch (event->code) {

            case BTN_START:
                controller.state.menu_pressed = pressed;
                break;

            case BTN_SOUTH:
                controller.state.select_pressed = pressed;
                break;

            case BTN_EAST:
                controller.state.back_pressed = pressed;
                break;

            default:
                break;
        }
    }
}

bool controller_poll(ControllerState *state)
{
    if (state == NULL)
        return false;

    if (controller.fd < 0) {
        if (!find_controller()) {
            reset_state();
            *state = controller.state;
            return false;
        }
    }

    struct input_event event;

    while (1) {

        ssize_t bytes = read(controller.fd,
                             &event,
                             sizeof(event));

        if (bytes == (ssize_t)sizeof(event)) {
            if (event.type == EV_SYN &&
                event.code == SYN_DROPPED) {
                disconnect_controller();
                break;
            }

            process_event(&event);
            continue;
        }

        if (bytes < 0 && errno == EINTR)
            continue;

        if (bytes < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

        disconnect_controller();
        break;
    }

    if (controller.fd < 0) {
        *state = controller.state;
        return false;
    }

    controller.state.connected = true;

    *state = controller.state;

    return true;
}

bool controller_is_connected(void)
{
    return controller.fd >= 0;
}

const char *controller_get_name(void)
{
    return controller.name;
}

void controller_close(void)
{
    disconnect_controller();
}
