
#include "camera.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define CAMERA_BUFFER_COUNT 4

typedef struct {
    void *start;
    size_t length;
} CameraBuffer;

typedef struct {
    int fd;
    bool streaming;

    uint32_t input_width;
    uint32_t input_height;

    uint32_t output_width;
    uint32_t output_height;

    uint32_t pixel_format;

    CameraScaleMode scale_mode;

    CameraBuffer buffers[CAMERA_BUFFER_COUNT];
    unsigned int buffer_count;

    uint16_t *output;

    CameraInfo info;

} CameraDevice;

static CameraDevice camera = {
    .fd = -1
};

static int camera_ioctl(int fd, unsigned long request, void *arg)
{
    int result;

    do {
        result = ioctl(fd, request, arg);
    } while (result < 0 && errno == EINTR);

    return result;
}

static uint16_t rgb565(unsigned char y,
                       unsigned char u,
                       unsigned char v)
{
    int c = (int)y - 16;
    int d = (int)u - 128;
    int e = (int)v - 128;

    if (c < 0)
        c = 0;

    int r = (298 * c + 409 * e + 128) >> 8;
    int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    int b = (298 * c + 516 * d + 128) >> 8;

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    return (uint16_t)(
        ((r >> 3) << 11) |
        ((g >> 2) << 5) |
        (b >> 3)
    );
}

static void convert_yuyv_pixel(
    const unsigned char *source,
    uint32_t x,
    uint32_t y,
    uint16_t *destination)
{
    size_t offset =
        ((size_t)y * camera.input_width + (x & ~1U)) * 2;

    const unsigned char *p = source + offset;

    unsigned char y_value = p[(x & 1U) ? 2 : 0];
    unsigned char u_value = p[1];
    unsigned char v_value = p[3];

    *destination = rgb565(y_value, u_value, v_value);
}

static void convert_frame(const unsigned char *source)
{
    uint32_t sw = camera.input_width;
    uint32_t sh = camera.input_height;

    uint32_t dw = camera.output_width;
    uint32_t dh = camera.output_height;

    memset(camera.output, 0,
           (size_t)dw * dh * sizeof(uint16_t));

    if (camera.scale_mode == CAMERA_SCALE_STRETCH) {

        for (uint32_t y = 0; y < dh; y++) {
            uint32_t sy = (uint64_t)y * sh / dh;

            for (uint32_t x = 0; x < dw; x++) {
                uint32_t sx = (uint64_t)x * sw / dw;

                convert_yuyv_pixel(
                    source,
                    sx,
                    sy,
                    &camera.output[(size_t)y * dw + x]
                );
            }
        }

        return;
    }

    double scale_x = (double)dw / sw;
    double scale_y = (double)dh / sh;

    double scale;

    if (camera.scale_mode == CAMERA_SCALE_FIT) {
        scale = scale_x < scale_y ? scale_x : scale_y;
    } else {
        scale = scale_x > scale_y ? scale_x : scale_y;
    }

    uint32_t scaled_width = (uint32_t)(sw * scale);
    uint32_t scaled_height = (uint32_t)(sh * scale);

    if (!scaled_width || !scaled_height)
        return;

    int offset_x = ((int)dw - (int)scaled_width) / 2;
    int offset_y = ((int)dh - (int)scaled_height) / 2;

    for (uint32_t y = 0; y < dh; y++) {

        int py = (int)y - offset_y;

        if (py < 0 || py >= (int)scaled_height)
            continue;

        uint32_t sy = (uint64_t)py * sh / scaled_height;

        for (uint32_t x = 0; x < dw; x++) {

            int px = (int)x - offset_x;

            if (px < 0 || px >= (int)scaled_width)
                continue;

            uint32_t sx = (uint64_t)px * sw / scaled_width;

            convert_yuyv_pixel(
                source,
                sx,
                sy,
                &camera.output[(size_t)y * dw + x]
            );
        }
    }
}

void camera_close(void)
{
    if (camera.fd >= 0) {

        if (camera.streaming) {
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            camera_ioctl(camera.fd, VIDIOC_STREAMOFF, &type);
        }

        for (unsigned int i = 0; i < camera.buffer_count; i++) {
            if (camera.buffers[i].start &&
                camera.buffers[i].length) {

                munmap(camera.buffers[i].start,
                       camera.buffers[i].length);
            }
        }

        close(camera.fd);
    }

    free(camera.output);

    memset(&camera, 0, sizeof(camera));

    camera.fd = -1;
}

int camera_init(const char *device,
                uint32_t output_width,
                uint32_t output_height,
                CameraScaleMode mode)
{
    if (!device || !output_width || !output_height)
        return -1;

    camera_close();

    camera.fd = open(device, O_RDWR | O_NONBLOCK | O_CLOEXEC);

    if (camera.fd < 0) {
        perror("camera: open");
        return -1;
    }

    camera.output_width = output_width;
    camera.output_height = output_height;
    camera.scale_mode = mode;

    camera.output = calloc(
        (size_t)output_width * output_height,
        sizeof(uint16_t)
    );

    if (!camera.output)
        goto error;

    struct v4l2_capability capability;

    if (camera_ioctl(camera.fd, VIDIOC_QUERYCAP, &capability) < 0)
        goto error;

    uint32_t caps = capability.capabilities;

    if (caps & V4L2_CAP_DEVICE_CAPS)
        caps = capability.device_caps;

    if (!(caps & V4L2_CAP_VIDEO_CAPTURE) ||
        !(caps & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "camera: streaming capture unsupported\n");
        goto error;
    }

    struct v4l2_format format = {0};

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = 1280;
    format.fmt.pix.height = 720;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_ANY;

    if (camera_ioctl(camera.fd, VIDIOC_S_FMT, &format) < 0)
        goto error;

    if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
        fprintf(stderr,
                "camera: device did not accept YUYV format\n");
        goto error;
    }

    camera.input_width = format.fmt.pix.width;
    camera.input_height = format.fmt.pix.height;
    camera.pixel_format = format.fmt.pix.pixelformat;

    if (camera.input_width < 2 ||
        camera.input_width % 2 != 0 ||
        !camera.input_height ||
        format.fmt.pix.bytesperline < camera.input_width * 2) {
        fprintf(stderr, "camera: unsupported frame layout\n");
        goto error;
    }

    camera.info.width = camera.input_width;
    camera.info.height = camera.input_height;
    camera.info.pixel_format = camera.pixel_format;
    camera.info.connected = true;

    struct v4l2_streamparm parameters = {0};

    parameters.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parameters.parm.capture.timeperframe.numerator = 1;
    parameters.parm.capture.timeperframe.denominator = 30;

    camera_ioctl(camera.fd, VIDIOC_S_PARM, &parameters);

    if (parameters.parm.capture.timeperframe.numerator > 0) {
        camera.info.fps =
            parameters.parm.capture.timeperframe.denominator /
            parameters.parm.capture.timeperframe.numerator;
    }

    struct v4l2_requestbuffers request = {0};

    request.count = CAMERA_BUFFER_COUNT;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;

    if (camera_ioctl(camera.fd, VIDIOC_REQBUFS, &request) < 0)
        goto error;

    camera.buffer_count = request.count;

    if (camera.buffer_count < 2) {
        fprintf(stderr, "camera: insufficient capture buffers\n");
        goto error;
    }

    for (unsigned int i = 0; i < camera.buffer_count; i++) {

        struct v4l2_buffer buffer = {0};

        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (camera_ioctl(camera.fd, VIDIOC_QUERYBUF, &buffer) < 0)
            goto error;

        camera.buffers[i].length = buffer.length;

        camera.buffers[i].start = mmap(
            NULL,
            buffer.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            camera.fd,
            buffer.m.offset
        );

        if (camera.buffers[i].start == MAP_FAILED) {
            camera.buffers[i].start = NULL;
            goto error;
        }

        if (camera_ioctl(camera.fd, VIDIOC_QBUF, &buffer) < 0)
            goto error;
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (camera_ioctl(camera.fd, VIDIOC_STREAMON, &type) < 0)
        goto error;

    camera.streaming = true;

    printf("Camera initialized: %ux%u\n",
           camera.input_width,
           camera.input_height);

    return 0;

error:
    perror("camera: initialization failed");
    camera_close();
    return -1;
}

int camera_update(void)
{
    if (camera.fd < 0 || !camera.streaming)
        return -1;

    struct v4l2_buffer buffer = {0};

    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (camera_ioctl(camera.fd, VIDIOC_DQBUF, &buffer) < 0) {

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        perror("camera: dequeue");
        camera.info.connected = false;
        return -1;
    }

    if (buffer.index >= camera.buffer_count ||
        buffer.bytesused >
            camera.buffers[buffer.index].length ||
        buffer.bytesused <
            (size_t)camera.input_width *
            camera.input_height * 2) {

        fprintf(stderr, "camera: invalid frame\n");

        camera_ioctl(camera.fd, VIDIOC_QBUF, &buffer);
        return 0;
    }

    convert_frame(camera.buffers[buffer.index].start);

    if (camera_ioctl(camera.fd, VIDIOC_QBUF, &buffer) < 0) {
        perror("camera: requeue");
        camera.info.connected = false;
        return -1;
    }

    return 1;
}

const uint16_t *camera_get_pixels(void)
{
    return camera.output;
}

CameraInfo camera_get_info(void)
{
    return camera.info;
}

void camera_set_scale_mode(CameraScaleMode mode)
{
    camera.scale_mode = mode;
}
