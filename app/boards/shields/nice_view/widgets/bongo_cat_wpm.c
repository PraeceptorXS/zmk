/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>

#include "bongo_cat_wpm.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Cat Animation frames
LV_IMG_DECLARE(cat_idle)
LV_IMG_DECLARE(cat_slap_left)
LV_IMG_DECLARE(cat_slap_right)
LV_IMG_DECLARE(cat_both_up)
LV_IMG_DECLARE(cat_both_down)

const lv_img_dsc_t *cats_imgs[] = {&cat_idle, &cat_slap_left, &cat_slap_right, &cat_both_up,
                                   &cat_both_down};
struct wpm_status_state {
    uint8_t wpm;
};

static uint8_t frame_num;
static lv_color_t bongo_cat_wpm_buf[DISP_WIDTH * WPM_HEIGHT];

static void draw_wpm_update(lv_obj_t *canvas, struct wpm_status_state state) {
    // Initialize canvas for WPM display
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    // Draw text for WPM
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, WPM_HEIGHT, &rect_black_dsc);

    // Draw WPM
    uint8_t wpm = state.wpm;
    // Cat idle, no animation
    if (wpm == 0) {
        lv_canvas_draw_img(canvas, 0, 0, cats_imgs[0], &img_dsc);
    }
    // Low WPM, animate loop of LEFT, UP, RIGHT, UP
    else if (wpm < 40) {
        switch (frame_num++ & 0x3) {
        case 0:
            lv_canvas_draw_img(canvas, 0, 0, &cat_slap_left, &img_dsc);
            break;
        case 2:
            lv_canvas_draw_img(canvas, 0, 0, &cat_slap_right, &img_dsc);
            break;
        default:
            lv_canvas_draw_img(canvas, 0, 0, &cat_both_up, &img_dsc);
        }
    }
    // Medium WPM, animate loop of LEFT, RIGHT
    else if (wpm < 80) {
        if (frame_num++ & 0x1) {
            lv_canvas_draw_img(canvas, 0, 0, &cat_slap_left, &img_dsc);
        } else {
            lv_canvas_draw_img(canvas, 0, 0, &cat_slap_right, &img_dsc);
        }
    }
    // High WPM, animate loop of UP, DOWN
    else {
        if (frame_num++ & 0x1) {
            lv_canvas_draw_img(canvas, 0, 0, &cat_both_down, &img_dsc);
        } else {
            lv_canvas_draw_img(canvas, 0, 0, &cat_both_up, &img_dsc);
        }
    }

    // Draw current WPM
    char wpm_text[4] = {};
    snprintf(wpm_text, sizeof(wpm_text), "%d", state.wpm);
    lv_canvas_draw_text(canvas, -4, 28, 50, &label_dsc, wpm_text);

    // Rotate canvas
    rotate_canvas(canvas);
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct wpm_status_state){.wpm = ev->state};
};

void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_bongo_cat_wpm *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { draw_wpm_update(widget->canvas, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat_wpm, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_bongo_cat_wpm, zmk_wpm_state_changed);

int zmk_widget_bongo_cat_wpm_init(struct zmk_widget_bongo_cat_wpm *widget, lv_obj_t *parent) {
    // WPM Widget area
    widget->canvas = lv_canvas_create(parent);
    lv_obj_set_size(widget->canvas, DISP_WIDTH, WPM_HEIGHT);
    lv_canvas_set_buffer(widget->canvas, bongo_cat_wpm_buf, DISP_WIDTH, WPM_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    sys_slist_append(&widgets, &widget->node);

    widget_bongo_cat_wpm_init();
    frame_num = 0;
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_wpm_obj(struct zmk_widget_bongo_cat_wpm *widget) {
    return widget->canvas;
}
