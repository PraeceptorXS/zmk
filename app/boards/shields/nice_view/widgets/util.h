/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <lvgl.h>
#include <zmk/endpoints_types.h>

#define DISP_WIDTH 68
#define ANIM_SIZE 64

#define PADDING 4
#define BATTERY_HEIGHT 17
#define BATTERY_IDX 0
#define BATTERY_OFFSET 68

#define WPM_HEIGHT 42
#define WPM_IDX 1
#define WPM_OFFSET (-BATTERY_HEIGHT + BATTERY_OFFSET - PADDING)

#define BT_PROF_HEIGHT 22
#define BT_PROF_IDX 2
#define BT_PROF_OFFSET (-WPM_HEIGHT + WPM_OFFSET - PADDING)

#define MODS_HEIGHT 42
#define MODS_IDX 3
#define MODS_OFFSET (-BT_PROF_HEIGHT + BT_PROF_OFFSET - PADDING)

#define LAYER_HEIGHT 29
#define LAYER_IDX 4
#define LAYER_OFFSET (-MODS_HEIGHT + MODS_OFFSET - PADDING)

#define LVGL_BACKGROUND                                                                            \
    IS_ENABLED(CONFIG_CUSTOM_WIDGET_INVERTED) ? lv_color_black() : lv_color_white()
#define LVGL_FOREGROUND                                                                            \
    IS_ENABLED(CONFIG_CUSTOM_WIDGET_INVERTED) ? lv_color_white() : lv_color_black()

struct status_state {
    uint8_t battery;
    bool charging;
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct zmk_endpoint_instance selected_endpoint;
    bool active_profile_connected;
    bool active_profile_bonded;
    uint8_t active_profile_index;
    uint8_t layer_index;
    const char *layer_label;
    uint8_t wpm;
    uint8_t mod_state;
#else
    bool connected;
#endif
};

struct battery_status_state {
    uint8_t level;
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool usb_present;
#endif
};

void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]);
void draw_battery(lv_obj_t *canvas, struct status_state state);
void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align);
void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color);
void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width);
void init_arc_dsc(lv_draw_arc_dsc_t *arc_dsc, lv_color_t color, uint8_t width);