/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include "output_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Store connection status state
static bool connected;

/*
 * OUTPUT STATUS CODE
 */
struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool usb_is_hid_ready;
};

static lv_color_t output_status_buf[OUTPUT_WIDTH * OUTPUT_HEIGHT];
static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){.selected_endpoint = zmk_endpoints_selected(),
                                        .active_profile_index = zmk_ble_active_profile_index(),
                                        .active_profile_connected =
                                            zmk_ble_active_profile_is_connected(),
                                        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
                                        .usb_is_hid_ready = zmk_usb_is_hid_ready()};
}

static void draw_output_status(lv_obj_t *canvas, struct output_status_state state) {
    // Fill background
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, OUTPUT_WIDTH, OUTPUT_HEIGHT, &rect_black_dsc);
    rect_black_dsc.bg_opa = LV_OPA_TRANSP;          // Don't fill center
    rect_black_dsc.border_color = lv_color_black(); // Draw border
    rect_black_dsc.border_width = 0;
    lv_canvas_draw_rect(canvas, 1, 2, OUTPUT_WIDTH - 1, OUTPUT_HEIGHT - 2, &rect_black_dsc);

    char output_txt[10] = {};
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    switch (state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        if (state.usb_is_hid_ready) {
            strcat(output_txt, LV_SYMBOL_LEFT);
        } else {
            strcat(output_txt, LV_SYMBOL_CLOSE);
        }
        strcat(output_txt, LV_SYMBOL_USB);
        break;
    case ZMK_TRANSPORT_BLE:
        if (state.active_profile_bonded) {
            if (state.active_profile_connected) {
                strcat(output_txt, LV_SYMBOL_LEFT);
            } else {
                strcat(output_txt, LV_SYMBOL_CLOSE);
            }
        } else {
            strcat(output_txt, LV_SYMBOL_SETTINGS);
        }
        strcat(output_txt, LV_SYMBOL_BLUETOOTH);
        break;
    }
    if (connected) {
        strcat(output_txt, LV_SYMBOL_RIGHT);
    } else {
        strcat(output_txt, LV_SYMBOL_CLOSE);
    }

    // Draw text
    lv_canvas_draw_text(canvas, 2, 5, OUTPUT_WIDTH - 2, &label_dsc, output_txt);
    rotate_canvas(canvas);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_output_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        draw_output_status(widget->canvas, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);

/*
 * PERIPHERAL CONNECTION STATUS CODE
 */
struct peripheral_state {
    bool connected;
};

static void update_peripheral_status(lv_obj_t *canvas, struct peripheral_state state) {
    // Update connected status
    connected = state.connected;
    // Faking an update status packet
    draw_output_status(canvas, output_status_get_state(NULL));
}

static void peripheral_state_cb(struct peripheral_state state) {
    struct zmk_widget_peripheral_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        update_peripheral_status(widget->obj, state);
    }
}

static struct peripheral_state peripheral_status_get_state(const zmk_event_t *eh) {
    struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    return (struct peripheral_state){.connected = (ev->state_of_charge > 0)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_state, struct peripheral_state, peripheral_state_cb,
                            peripheral_status_get_state)

ZMK_SUBSCRIPTION(widget_peripheral_state, zmk_peripheral_battery_state_changed);

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent) {
    connected = false;
    // WPM Widget area
    widget->canvas = lv_canvas_create(parent);
    lv_obj_set_size(widget->canvas, OUTPUT_WIDTH, OUTPUT_HEIGHT);
    lv_canvas_set_buffer(widget->canvas, output_status_buf, OUTPUT_WIDTH, OUTPUT_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
    sys_slist_append(&widgets, &widget->node);

    widget_output_status_init();
    widget_peripheral_state_init();

    return 0;
}

lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget) {
    return widget->canvas;
}