#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define NANOVG_GLES2_IMPLEMENTATION
#include "nanovg/nanovg.h"
#include "nanovg/nanovg_gl.h"

#include "menus.h"

int main(void)
{
    int fbfd = open("/dev/fb0", O_RDONLY);
    if (fbfd == -1) {
        return 1;
    }

    struct fb_var_screeninfo vinfo;

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        close(fbfd);
        return 1;
    }

    glClearColor(0.0f,0.0f,0.0f,255.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    NVGcontext *vg = nvgCreateGLES2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (vg == NULL) {
        return -1;
    }

    nvgBeginFrame(vg, vinfo.width, vinfo.height, 1.0f);

    nvgBeginPath(vg);
    nvgRect(vg, 50, 50, 150, 150); 
    nvgFillColor(vg, nvgRGBA(255, 128, 0, 255));
    nvgFill(vg);

    nvgFontSize(vg, 22.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 255)); // White text
    nvgText(vg, 50, 40, "Hello Buildroot!", NULL);

    nvgEndFrame(vg);
}
