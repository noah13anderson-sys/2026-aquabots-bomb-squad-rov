#include "display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>
#include <drm_mode.h>

#include "lvgl.h"

#define DRM_DEVICE "/dev/dri/card1"
#define DRAW_BUFFER_LINES 40

typedef struct {
    int fd;

    uint32_t connector_id;
    uint32_t crtc_id;

    uint32_t width;
    uint32_t height;

    uint32_t handle;
    uint32_t pitch;
    uint32_t fb_id;

    uint64_t size;

    uint32_t *framebuffer;

    lv_display_t *display;

    lv_color_t *draw_buffer1;
    lv_color_t *draw_buffer2;

    bool ready;
} DisplayDevice;

static DisplayDevice display = {
    .fd = -1
};

static int drm_ioctl(int fd, unsigned long request, void *arg)
{
    int result;

    do {
        result = ioctl(fd, request, arg);
    } while (result < 0 && errno == EINTR);

    return result;
}

static int find_crtc(
    drmModeRes *resources,
    drmModeConnector *connector,
    drmModeEncoder *encoder,
    uint32_t *crtc_id)
{
    if (encoder && encoder->crtc_id) {
        *crtc_id = encoder->crtc_id;
        return 0;
    }

    if (connector->encoder_id) {
        drmModeEncoder *current =
            drmModeGetEncoder(display.fd, connector->encoder_id);

        if (current) {
            if (current->crtc_id) {
                *crtc_id = current->crtc_id;
                drmModeFreeEncoder(current);
                return 0;
            }

            drmModeFreeEncoder(current);
        }
    }

    if (encoder) {
        for (int i = 0; i < resources->count_crtcs; i++) {
            if (encoder->possible_crtcs & (1U << i)) {
                *crtc_id = resources->crtcs[i];
                return 0;
            }
        }
    }

    return -1;
}

static int select_display(void)
{
    drmModeRes *resources =
        drmModeGetResources(display.fd);

    if (!resources)
        return -1;

    int result = -1;

    for (int i = 0; i < resources->count_connectors; i++) {

        drmModeConnector *connector =
            drmModeGetConnector(
                display.fd,
                resources->connectors[i]
            );

        if (!connector)
            continue;

        if (connector->connection != DRM_MODE_CONNECTED ||
            connector->count_modes == 0) {
            drmModeFreeConnector(connector);
            continue;
        }

        drmModeModeInfo *mode = &connector->modes[0];

        drmModeEncoder *encoder = NULL;

        if (connector->encoder_id)
            encoder = drmModeGetEncoder(
                display.fd,
                connector->encoder_id
            );

        uint32_t crtc_id = 0;

        if (find_crtc(resources, connector, encoder, &crtc_id) == 0) {

            display.connector_id = connector->connector_id;
            display.crtc_id = crtc_id;

            display.width = mode->hdisplay;
            display.height = mode->vdisplay;

            display.saved_crtc =
                drmModeGetCrtc(display.fd, crtc_id);

            display.mode = *mode;

            result = 0;
        }

        if (encoder)
            drmModeFreeEncoder(encoder);

        drmModeFreeConnector(connector);

        if (result == 0)
            break;
    }

    drmModeFreeResources(resources);

    return result;
}

static int create_framebuffer(void)
{
    struct drm_mode_create_dumb create = {0};

    create.width = display.width;
    create.height = display.height;
    create.bpp = 32;

    if (drm_ioctl(
            display.fd,
            DRM_IOCTL_MODE_CREATE_DUMB,
            &create) < 0) {
        perror("DRM create dumb buffer");
        return -1;
    }

    display.handle = create.handle;
    display.pitch = create.pitch;
    display.size = create.size;

    if (drmModeAddFB(
            display.fd,
            display.width,
            display.height,
            24,
            32,
            display.pitch,
            display.handle,
            &display.fb_id) != 0) {
        perror("DRM add framebuffer");
        return -1;
    }

    struct drm_mode_map_dumb map = {0};

    map.handle = display.handle;

    if (drm_ioctl(
            display.fd,
            DRM_IOCTL_MODE_MAP_DUMB,
            &map) < 0) {
        perror("DRM map dumb buffer");
        return -1;
    }

    display.framebuffer = mmap(
        NULL,
        display.size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        display.fd,
        map.offset
    );

    if (display.framebuffer == MAP_FAILED) {
        display.framebuffer = NULL;
        perror("DRM mmap");
        return -1;
    }

    memset(
        display.framebuffer,
        0,
        display.size
    );

    if (drmModeSetCrtc(
            display.fd,
            display.crtc_id,
            display.fb_id,
            0,
            0,
            &display.connector_id,
            1,
            &display.mode) != 0) {
        perror("DRM set CRTC");
        return -1;
    }

    return 0;
}

static void display_flush(
    lv_display_t *lv_display,
    const lv_area_t *area,
    uint8_t *px_map)
{
    (void)lv_display;

    if (!display.ready || !px_map) {
        lv_display_flush_ready(display.display);
        return;
    }

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;

    if (x2 >= (int32_t)display.width)
        x2 = display.width - 1;

    if (y2 >= (int32_t)display.height)
        y2 = display.height - 1;

    uint32_t copy_width = x2 - x1 + 1;

    const uint16_t *source = (const uint16_t *)px_map;

    for (int32_t y = y1; y <= y2; y++) {

        uint32_t *destination =
            (uint32_t *)((uint8_t *)display.framebuffer +
                         (size_t)y * display.pitch);

        const uint16_t *source_row =
            source + (size_t)(y - area->y1) *
                     (area->x2 - area->x1 + 1) +
                     (x1 - area->x1);

        for (uint32_t x = 0; x < copy_width; x++) {

            uint16_t pixel = source_row[x];

            uint32_t r = (pixel >> 11) & 0x1F;
            uint32_t g = (pixel >> 5) & 0x3F;
            uint32_t b = pixel & 0x1F;

            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);

            destination[x1 + x] =
                (r << 16) | (g << 8) | b;
        }
    }

    lv_display_flush_ready(display.display);
}

int display_init(void)
{
    display.fd = open(DRM_DEVICE, O_RDWR | O_CLOEXEC);

    if (display.fd < 0) {
        perror("DRM open");
        return -1;
    }

    if (drmSetMaster(display.fd) != 0) {
        perror("DRM set master");
        goto error;
    }

    if (select_display() != 0) {
        fprintf(stderr, "No connected DRM display found\n");
        goto error;
    }

    printf(
        "Display: %ux%u\n",
        display.width,
        display.height
    );

    if (create_framebuffer() != 0)
        goto error;

    display.display = lv_display_create(
        display.width,
        display.height
    );

    if (!display.display)
        goto error;

    lv_display_set_color_format(
        display.display,
        LV_COLOR_FORMAT_RGB565
    );

    size_t buffer_size =
        (size_t)display.width *
        DRAW_BUFFER_LINES *
        sizeof(uint16_t);

    display.draw_buffer1 = lv_malloc(buffer_size);
    display.draw_buffer2 = lv_malloc(buffer_size);

    if (!display.draw_buffer1 || !display.draw_buffer2)
        goto error;

    lv_display_set_buffers(
        display.display,
        display.draw_buffer1,
        display.draw_buffer2,
        buffer_size,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_display_set_flush_cb(
        display.display,
        display_flush
    );

    display.ready = true;

    return 0;

error:
    display_deinit();
    return -1;
}

void display_poll(void)
{
    /* LVGL handles rendering through its timer handler. */
}

void display_deinit(void)
{
    display.ready = false;

    if (display.display) {
        lv_display_delete(display.display);
        display.display = NULL;
    }

    lv_free(display.draw_buffer1);
    lv_free(display.draw_buffer2);

    display.draw_buffer1 = NULL;
    display.draw_buffer2 = NULL;

    if (display.framebuffer) {
        munmap(display.framebuffer, display.size);
        display.framebuffer = NULL;
    }

    if (display.fd >= 0) {

        if (display.fb_id)
            drmModeRmFB(display.fd, display.fb_id);

        if (display.handle) {
            struct drm_mode_destroy_dumb destroy = {0};
            destroy.handle = display.handle;

            drm_ioctl(
                display.fd,
                DRM_IOCTL_MODE_DESTROY_DUMB,
                &destroy
            );
        }

        drmDropMaster(display.fd);
        close(display.fd);
    }

    memset(&display, 0, sizeof(display));
    display.fd = -1;
}

uint32_t display_get_width(void)
{
    return display.width;
}

uint32_t display_get_height(void)
{
    return display.height;
}

bool display_is_ready(void)
{
    return display.ready;
}