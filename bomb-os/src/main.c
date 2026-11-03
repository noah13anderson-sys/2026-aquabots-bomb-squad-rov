
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include "lvgl.h"

#include "display/display.h"
#include "camera/camera.h"
#include "input/keyboard.h"
#include "ui/ui.h"

static volatile sig_atomic_t running = 1;

static void shutdown_handler(int signal_number)
{
    (void)signal_number;
    running = 0;
}

int main(void)
{
    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);

    printf("================================\n");
    printf("          BOMB OS\n");
    printf("================================\n");

    lv_init();

    if (display_init() != 0) {
        fprintf(stderr, "Display initialization failed\n");
        return 1;
    }

    uint32_t width = display_get_width();
    uint32_t height = display_get_height();

    printf("Display resolution: %ux%u\n", width, height);

    if (keyboard_init() != 0) {
        fprintf(stderr, "Keyboard unavailable\n");
    }

    bool camera_ready = camera_init(
        "/dev/video0",
        width,
        height,
        CAMERA_SCALE_FIT
    ) == 0;

    if (ui_init(width, height) != 0) {
        fprintf(stderr, "UI initialization failed\n");
        goto shutdown;
    }

    ui_set_camera_connected(camera_ready);

    printf("Bomb OS ready\n");

    while (running) {

        KeyboardKey key = keyboard_poll();

        if (key != KEYBOARD_NONE)
            ui_keyboard_input(key);

        if (camera_ready) {

            int result = camera_update();

            if (result == 1) {
                ui_set_camera_frame(camera_get_pixels());
                ui_set_camera_info(camera_get_info());
            }

            if (result < 0) {
                camera_ready = false;
                ui_set_camera_connected(false);
                fprintf(stderr, "Camera disconnected\n");
            }
        }

        lv_timer_handler();

        usleep(5000);
    }

shutdown:

    printf("Shutting down Bomb OS...\n");

    camera_close();
    keyboard_close();
    ui_deinit();
    display_deinit();

    return 0;
}
