/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include "status.h"
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>
#include <zmk/hid.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Cat Animation Frames
LV_IMG_DECLARE(cat_idle)
LV_IMG_DECLARE(cat_slap_left)
LV_IMG_DECLARE(cat_slap_right)
LV_IMG_DECLARE(cat_both_up)
LV_IMG_DECLARE(cat_both_down)

const lv_img_dsc_t *cats_imgs[5] = {&cat_idle, &cat_slap_left, &cat_slap_right, &cat_both_up,
                                    &cat_both_down};
struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

struct wpm_status_state {
    uint8_t wpm;
};

struct mods_status_state {
    uint8_t mods;
};

static uint8_t frame;
static void draw_battery_status(lv_obj_t *widget, lv_color_t cbuf[], struct status_state state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, BATTERY_IDX);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, BATTERY_HEIGHT, &rect_black_dsc);

    // Draw battery
    draw_battery(canvas, state);

    // Draw output status
    char output_text[10] = {};

    switch (state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        strcat(output_text, LV_SYMBOL_USB);
        break;
    case ZMK_TRANSPORT_BLE:
        if (state.active_profile_bonded) {
            if (state.active_profile_connected) {
                strcat(output_text, LV_SYMBOL_WIFI);
            } else {
                strcat(output_text, LV_SYMBOL_CLOSE);
            }
        } else {
            strcat(output_text, LV_SYMBOL_SETTINGS);
        }
        break;
    }
    lv_canvas_draw_text(canvas, 0, 0, DISP_WIDTH, &label_dsc, output_text);

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}
static void draw_wpm(lv_obj_t *widget, lv_color_t cbuf[], struct status_state state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, WPM_IDX);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, WPM_HEIGHT, &rect_black_dsc);

    // Draw WPM
    uint8_t wpm = state.wpm;
    if (wpm == 0) {
        lv_canvas_draw_img(canvas, 0, 0, cats_imgs[0], &img_dsc);
    } else if (wpm < 40) {
        switch (frame++ & 0x3) {
        case 0:
            lv_canvas_draw_img(canvas, 0, 0, &cat_slap_left, &img_dsc);
            break;
        case 2:
            lv_canvas_draw_img(canvas, 0, 0, &cat_slap_right, &img_dsc);
            break;
        default:
            lv_canvas_draw_img(canvas, 0, 0, &cat_both_up, &img_dsc);
        }
    } else if (wpm < 80) {
        if (frame++ & 0x1) {
            lv_canvas_draw_img(canvas, 0, 0, cats_imgs[1], &img_dsc);
        } else {
            lv_canvas_draw_img(canvas, 0, 0, cats_imgs[2], &img_dsc);
        }
    } else {
        if (frame++ & 0x1) {
            lv_canvas_draw_img(canvas, 0, 0, cats_imgs[3], &img_dsc);
        } else {
            lv_canvas_draw_img(canvas, 0, 0, cats_imgs[4], &img_dsc);
        }
    }
    char wpm_text[6] = {};
    snprintf(wpm_text, sizeof(wpm_text), "%d", state.wpm);
    lv_canvas_draw_text(canvas, -4, 28, 50, &label_dsc, wpm_text);

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}
static void draw_bt_prof(lv_obj_t *widget, lv_color_t cbuf[], struct status_state state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, BT_PROF_IDX);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    lv_draw_arc_dsc_t arc_dsc;
    init_arc_dsc(&arc_dsc, LVGL_FOREGROUND, 2);
    lv_draw_arc_dsc_t arc_dsc_filled;
    init_arc_dsc(&arc_dsc_filled, LVGL_FOREGROUND, 9);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t label_dsc_black;
    init_label_dsc(&label_dsc_black, LVGL_BACKGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, BT_PROF_HEIGHT, &rect_black_dsc);

    // Draw circles
    int circle_offsets[3][2] = {{11, 11}, {34, 11}, {57, 11}};

    for (int i = 0; i < 3; i++) {
        bool selected = state.active_profile_index == i;

        lv_canvas_draw_arc(canvas, circle_offsets[i][0], circle_offsets[i][1], 11, 0, 359,
                           &arc_dsc);

        if (selected) {
            lv_canvas_draw_arc(canvas, circle_offsets[i][0], circle_offsets[i][1], 7, 0, 359,
                               &arc_dsc_filled);
        }

        // Writing the bt profile label
        char label[2];
        label[0] = 0x31 + i;
        label[1] = '\0';
        lv_canvas_draw_text(canvas, circle_offsets[i][0] - 7, circle_offsets[i][1] - 8, 14,
                            (selected ? &label_dsc_black : &label_dsc), label);
    }

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void draw_mods(lv_obj_t *widget, lv_color_t cbuf[], struct status_state state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, MODS_IDX);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_label_dsc_t mod_dsc;
    init_label_dsc(&mod_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t mod_dsc_black;
    init_label_dsc(&mod_dsc_black, LVGL_BACKGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, MODS_HEIGHT, &rect_black_dsc);

    // Mod Labels
    char names[4][4] = {"CTL", "SFT", "ALT", "CMD"};

    // Drawing Mod Boxes
    int mod_offsets[4][2] = {{2, 2}, {35, 2}, {2, 22}, {35, 22}};
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, 42, &rect_white_dsc);
    for (int i = 0; i < 4; i++) {
        bool selected = (state.mod_state >> i) & 0x11;
        lv_canvas_draw_rect(canvas, mod_offsets[i][0], mod_offsets[i][1], 31, 18, &rect_black_dsc);
        if (selected)
            lv_canvas_draw_rect(canvas, mod_offsets[i][0] + 2, mod_offsets[i][1] + 2, 27, 14,
                                &rect_white_dsc);
        lv_canvas_draw_text(canvas, mod_offsets[i][0], mod_offsets[i][1] + 4, 32,
                            (selected ? &mod_dsc_black : &mod_dsc), names[i]);
    }

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}

static void draw_layer(lv_obj_t *widget, lv_color_t cbuf[], struct status_state state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, LAYER_IDX);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_unscii_16, LV_TEXT_ALIGN_CENTER);

    // Fill background
    lv_canvas_draw_rect(canvas, 0, 0, DISP_WIDTH, LAYER_HEIGHT, &rect_black_dsc);

    // Draw layer
    if (state.layer_label == NULL) {
        char text[4] = {};
        sprintf(text, "L%i", state.layer_index);
        lv_canvas_draw_text(canvas, 0, 5, 68, &label_dsc, text);
    } else {
        lv_canvas_draw_text(canvas, 0, 5, 68, &label_dsc, state.layer_label);
    }

    // Rotate canvas
    rotate_canvas(canvas, cbuf);
}
struct mods_status_state mods_status_get_state(const zmk_event_t *eh) {
    // Intentionally useless
    return (struct mods_status_state){.mods = 0};
};

void set_mods_state(struct zmk_widget_status *widget, struct mods_status_state state) {
    // Had to make call here since status_get_state didn't reliably update the mod state
    widget->state.mod_state = zmk_hid_get_explicit_mods();
    draw_mods(widget->obj, widget->mods_buf, widget->state);
}

void mods_status_update_cb(struct mods_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_mods_state(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_mods_status, struct mods_status_state, mods_status_update_cb,
                            mods_status_get_state)
ZMK_SUBSCRIPTION(widget_mods_status, zmk_keycode_state_changed);

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    widget->state.battery = state.level;

    draw_battery_status(widget->obj, widget->battery_buf, widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

static void set_output_status(struct zmk_widget_status *widget, struct output_status_state state) {
    widget->state.selected_endpoint = state.selected_endpoint;
    widget->state.active_profile_connected = state.active_profile_connected;
    widget->state.active_profile_bonded = state.active_profile_bonded;
    widget->state.active_profile_index = state.active_profile_index;

    draw_battery_status(widget->obj, widget->battery_buf, widget->state);
    draw_bt_prof(widget->obj, widget->bt_prof_buf, widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){.selected_endpoint = zmk_endpoints_selected(),
                                        .active_profile_connected =
                                            zmk_ble_active_profile_is_connected(),
                                        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
                                        .active_profile_index = zmk_ble_active_profile_index()};
    ;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

static void set_layer_status(struct zmk_widget_status *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_layer(widget->obj, widget->layer_buf, widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index, .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index))};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

static void set_wpm_status(struct zmk_widget_status *widget, struct wpm_status_state state) {
    widget->state.wpm = state.wpm;
    draw_wpm(widget->obj, widget->wpm_buf, widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    // Bar for battery and output indication
    lv_obj_t *battery_area = lv_canvas_create(widget->obj);
    lv_obj_align(battery_area, LV_ALIGN_TOP_RIGHT, BATTERY_OFFSET, 0);
    lv_canvas_set_buffer(battery_area, widget->battery_buf, DISP_WIDTH, BATTERY_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);

    // WPM Widget area
    lv_obj_t *wpm_area = lv_canvas_create(widget->obj);
    lv_obj_align(wpm_area, LV_ALIGN_TOP_RIGHT, WPM_OFFSET, 0);
    lv_canvas_set_buffer(wpm_area, widget->wpm_buf, DISP_WIDTH, WPM_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    // BT Profile area
    lv_obj_t *bt_prof_area = lv_canvas_create(widget->obj);
    lv_obj_align(bt_prof_area, LV_ALIGN_TOP_RIGHT, BT_PROF_OFFSET, 0);
    lv_canvas_set_buffer(bt_prof_area, widget->bt_prof_buf, DISP_WIDTH, BT_PROF_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);

    // Modifier Status Area
    lv_obj_t *mods_area = lv_canvas_create(widget->obj);
    lv_obj_align(mods_area, LV_ALIGN_TOP_RIGHT, MODS_OFFSET, 0);
    lv_canvas_set_buffer(mods_area, widget->mods_buf, DISP_WIDTH, MODS_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    // Layer Area
    lv_obj_t *layer_area = lv_canvas_create(widget->obj);
    lv_obj_align(layer_area, LV_ALIGN_TOP_RIGHT, LAYER_OFFSET, 0);
    lv_canvas_set_buffer(layer_area, widget->layer_buf, DISP_WIDTH, LAYER_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_wpm_status_init();
    widget_output_status_init();
    widget_mods_status_init();
    widget_layer_status_init();

    frame = 0;
    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
