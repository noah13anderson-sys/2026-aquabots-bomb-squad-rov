#include "ui.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "../camera/camera.h"
#include "lvgl.h"

#define UI_BACKGROUND       0x101820
#define UI_PANEL            0x1B2836
#define UI_PANEL_SELECTED   0x263B50
#define UI_ACCENT           0x00C8FF
#define UI_TEXT             0xFFFFFF
#define UI_MUTED            0x8295A8
#define UI_SUCCESS          0x35D07F
#define UI_WARNING          0xFFB547

#define UI_MENU_WIDTH       300
#define UI_MENU_PADDING     20

static lv_obj_t *screen;

static lv_obj_t *menu_panel;
static lv_obj_t *menu_title;

static lv_obj_t *bluetooth_button;
static lv_obj_t *video_button;

static lv_obj_t *bluetooth_info_label;
static lv_obj_t *camera_info_label;

static lv_obj_t *status_label;
static lv_obj_t *content_panel;
static lv_obj_t *content_title;
static lv_obj_t *content_description;

static bool bluetooth_connected = false;
static bool camera_connected = false;

static CameraInfo camera_info;
static const char *bluetooth_info = "No controller connected";

typedef enum {
    MENU_MAIN,
    MENU_BLUETOOTH,
    MENU_VIDEO
} MenuPage;

static MenuPage current_page = MENU_MAIN;

static void style_button(
    lv_obj_t *button,
    bool selected
)
{
    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(
            selected ? UI_PANEL_SELECTED : UI_PANEL
        ),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        button,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        button,
        8,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        button,
        selected ? 2 : 0,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        button,
        lv_color_hex(UI_ACCENT),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        button,
        lv_color_hex(UI_TEXT),
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        button,
        14,
        LV_PART_MAIN
    );
}

static void show_main_page(void)
{
    current_page = MENU_MAIN;

    lv_label_set_text(content_title, "Bomb OS");

    lv_label_set_text(
        content_description,
        "Underwater ROV Control System\n\n"
        "Select an option from the menu.\n\n"
        "Use the arrow keys or numpad to navigate."
    );
}

static void show_bluetooth_page(void)
{
    current_page = MENU_BLUETOOTH;

    lv_label_set_text(
        content_title,
        "Bluetooth Controller"
    );

    lv_label_set_text_fmt(
        content_description,
        "Controller Status: %s\n\n"
        "Device: %s\n\n"
        "Supported controllers:\n"
        "- PlayStation 4 DualShock\n"
        "- PlayStation 5 DualSense\n"
        "- Compatible Bluetooth controllers\n\n"
        "Only one controller can be connected at a time.",
        bluetooth_connected ? "CONNECTED" : "DISCONNECTED",
        bluetooth_info
    );
}

static void show_video_page(void)
{
    current_page = MENU_VIDEO;

    lv_label_set_text(
        content_title,
        "Video Information"
    );

    lv_label_set_text_fmt(
        content_description,
        "Camera Status: %s\n\n"
        "Camera FPS: %s\n\n"
        "Video source: USB Camera\n\n"
        "Display modes:\n"
        "- Fit\n"
        "- Scale\n\n"
        "Only one USB camera is supported.",
        camera_connected ? "CONNECTED" : "DISCONNECTED",
        camera_info.fps
    );
}

static void bluetooth_button_callback(
    lv_event_t *event
)
{
    (void)event;

    style_button(bluetooth_button, true);
    style_button(video_button, false);

    show_bluetooth_page();
}

static void video_button_callback(
    lv_event_t *event
)
{
    (void)event;

    style_button(bluetooth_button, false);
    style_button(video_button, true);

    show_video_page();
}

static lv_obj_t *create_menu_button(
    lv_obj_t *parent,
    const char *text,
    lv_event_cb_t callback
)
{
    lv_obj_t *button = lv_button_create(parent);

    lv_obj_set_width(button, LV_PCT(100));
    lv_obj_set_height(button, 55);

    style_button(button, false);

    lv_obj_t *label = lv_label_create(button);

    lv_label_set_text(label, text);

    lv_obj_center(label);

    lv_obj_add_event_cb(
        button,
        callback,
        LV_EVENT_CLICKED,
        NULL
    );

    return button;
}

void ui_init(void)
{
    screen = lv_screen_active();

    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(UI_BACKGROUND),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    menu_panel = lv_obj_create(screen);

    lv_obj_set_size(
        menu_panel,
        UI_MENU_WIDTH,
        LV_PCT(100)
    );

    lv_obj_align(
        menu_panel,
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );

    lv_obj_set_style_bg_color(
        menu_panel,
        lv_color_hex(UI_PANEL),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        menu_panel,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        menu_panel,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        menu_panel,
        UI_MENU_PADDING,
        LV_PART_MAIN
    );

    lv_obj_set_flex_flow(
        menu_panel,
        LV_FLEX_FLOW_COLUMN
    );

    lv_obj_set_flex_align(
        menu_panel,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_START
    );

    menu_title = lv_label_create(menu_panel);

    lv_label_set_text(
        menu_title,
        "BOMB OS"
    );

    lv_obj_set_style_text_color(
        menu_title,
        lv_color_hex(UI_ACCENT),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        menu_title,
        LV_FONT_DEFAULT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_letter_space(
        menu_title,
        3,
        LV_PART_MAIN
    );

    lv_obj_set_width(
        menu_title,
        LV_PCT(100)
    );

    lv_obj_set_style_pad_bottom(
        menu_title,
        20,
        LV_PART_MAIN
    );

    bluetooth_button = create_menu_button(
        menu_panel,
        "Bluetooth",
        bluetooth_button_callback
    );

    video_button = create_menu_button(
        menu_panel,
        "Video",
        video_button_callback
    );

    status_label = lv_label_create(menu_panel);

    lv_label_set_text(
        status_label,
        "SYSTEM READY"
    );

    lv_obj_set_style_text_color(
        status_label,
        lv_color_hex(UI_SUCCESS),
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_top(
        status_label,
        20,
        LV_PART_MAIN
    );

    content_panel = lv_obj_create(screen);

    lv_obj_set_size(
        content_panel,
        LV_PCT(100),
        LV_PCT(100)
    );

    lv_obj_set_style_margin_left(
        content_panel,
        UI_MENU_WIDTH,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        content_panel,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        content_panel,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        content_panel,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        content_panel,
        35,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        content_panel,
        LV_OBJ_FLAG_SCROLLABLE
    );

    content_title = lv_label_create(content_panel);

    lv_obj_align(
        content_title,
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );

    lv_obj_set_style_text_color(
        content_title,
        lv_color_hex(UI_TEXT),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        content_title,
        LV_FONT_DEFAULT,
        LV_PART_MAIN
    );

    content_description = lv_label_create(content_panel);

    lv_obj_set_width(
        content_description,
        LV_PCT(100)
    );

    lv_obj_align(
        content_description,
        LV_ALIGN_TOP_LEFT,
        0,
        65
    );

    lv_obj_set_style_text_color(
        content_description,
        lv_color_hex(UI_MUTED),
        LV_PART_MAIN
    );

    lv_obj_set_style_text_line_space(
        content_description,
        10,
        LV_PART_MAIN
    );

    show_main_page();

    style_button(bluetooth_button, false);
    style_button(video_button, false);

    lv_obj_move_foreground(menu_panel);
}

void ui_set_bluetooth_info(const char *info)
{
    if (info == NULL)
        return;

    bluetooth_info = info;

    bluetooth_connected = true;

    if (current_page == MENU_BLUETOOTH)
        show_bluetooth_page();

    lv_label_set_text(
        status_label,
        "CONTROLLER CONNECTED"
    );

    lv_obj_set_style_text_color(
        status_label,
        lv_color_hex(UI_SUCCESS),
        LV_PART_MAIN
    );
}

void ui_set_camera_info(CameraInfo info)
{
    camera_info = info;

    camera_connected = true;

    if (current_page == MENU_VIDEO)
        show_video_page();
}

void ui_keyboard_input(uint32_t key)
{
    switch (key)
    {
        case LV_KEY_UP:
        case LV_KEY_LEFT:
        case LV_KEY_HOME:
            show_main_page();
            style_button(bluetooth_button, false);
            style_button(video_button, false);
            break;

        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:
        case LV_KEY_END:
            show_video_page();
            style_button(bluetooth_button, false);
            style_button(video_button, true);
            break;

        case LV_KEY_ENTER:
            if (current_page == MENU_MAIN)
            {
                show_bluetooth_page();
                style_button(bluetooth_button, true);
            }
            else
            {
                show_main_page();
                style_button(bluetooth_button, false);
                style_button(video_button, false);
            }
            break;

        default:
            break;
    }
}

void ui_deinit(void)
{
    if (screen != NULL)
    {
        lv_obj_clean(screen);
    }

    screen = NULL;
    menu_panel = NULL;
    content_panel = NULL;
    content_title = NULL;
    content_description = NULL;

    bluetooth_button = NULL;
    video_button = NULL;
    status_label = NULL;

    bluetooth_connected = false;
    camera_connected = false;
}