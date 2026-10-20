
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "menus.h"

int main(void)
{
    printf("%s\n", main_menu.title);
    return 0;
}
