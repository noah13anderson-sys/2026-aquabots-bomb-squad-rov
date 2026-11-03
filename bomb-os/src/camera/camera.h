
#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CAMERA_SCALE_FIT,
    CAMERA_SCALE_FILL,
    CAMERA_SCALE_STRETCH
} CameraScaleMode;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t pixel_format;
    bool connected;
} CameraInfo;

int camera_init(const char *device,
                uint32_t output_width,
                uint32_t output_height,
                CameraScaleMode mode);

int camera_update(void);

const uint16_t *camera_get_pixels(void);

CameraInfo camera_get_info(void);

void camera_set_scale_mode(CameraScaleMode mode);

void camera_close(void);

#endif
