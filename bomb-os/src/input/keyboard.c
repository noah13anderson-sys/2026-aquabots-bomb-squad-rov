#include "keyboard.h"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#define INPUT_DIRECTORY "/dev/input"
#define MAX_EVENTS 32

static int keyboard_fd = -1;

static bool has_key(int fd, unsigned int key)
{
    unsigned long bits[NBITS(KEY_MAX + 1)];

    memset(bits, 0, sizeof(bits));

    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0)
        return false;

    return (bits[BIT_WORD(key)] & BIT_MASK(key)) != 0;
}

static bool is_keyboard(int fd)
{
    return has_key(fd, KEY_UP) &&
           has_key(fd, KEY_DOWN) &&
           has_key(fd, KEY_ENTER);
}

int keyboard_init(void)
{
    DIR *directory = opendir(INPUT_DIRECTORY);

    if (!directory) {
        perror("keyboard: opendir");
        return -1;
    }

    struct dirent *entry;

    while ((entry = readdir(directory))) {

        if (strncmp(entry->d_name, "event", 5) != 0)
            continue;

        char path[128];

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            INPUT_DIRECTORY,
            entry->d_name
        );

        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);

        if (fd < 0)
            continue;

        if (is_keyboard(fd)) {
            keyboard_fd = fd;

            char name[256] = {0};

            ioctl(fd, EVIOCGNAME(sizeof(name)), name);

            printf("Keyboard connected: %s\n", name);

            break;
        }

        close(fd);
    }

    closedir(directory);

    if (keyboard_fd < 0) {
        fprintf(stderr, "No keyboard found\n");
        return -1;
    }

    return 0;
}

KeyboardKey keyboard_poll(void)
{
    if (keyboard_fd < 0)
        return KEYBOARD_NONE;

    struct input_event event;

    while (read(keyboard_fd, &event, sizeof(event)) ==
           sizeof(event)) {

        if (event.type != EV_KEY || event.value != 1)
            continue;

        switch (event.code) {

            case KEY_UP:
            case KEY_KP8:
                return KEYBOARD_UP;

            case KEY_DOWN:
            case KEY_KP2:
                return KEYBOARD_DOWN;

            case KEY_LEFT:
            case KEY_KP4:
                return KEYBOARD_LEFT;

            case KEY_RIGHT:
            case KEY_KP6:
                return KEYBOARD_RIGHT;

            case KEY_ENTER:
            case KEY_KPENTER:
            case KEY_KP5:
                return KEYBOARD_ENTER;

            case KEY_ESC:
            case KEY_KP0:
                return KEYBOARD_BACK;

            default:
                break;
        }
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK &&
        errno != EINTR) {
        close(keyboard_fd);
        keyboard_fd = -1;
    }

    return KEYBOARD_NONE;
}

bool keyboard_is_connected(void)
{
    return keyboard_fd >= 0;
}

void keyboard_close(void)
{
    if (keyboard_fd >= 0)
        close(keyboard_fd);

    keyboard_fd = -1;
}