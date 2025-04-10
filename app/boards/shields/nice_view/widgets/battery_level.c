/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_level.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

typedef enum { CENTRAL, PERIPHERAL } device_e;

struct battery_state {
    device_e source;
    uint8_t level;
    bool charging;
};

static lv_color_t battery_image_buffer[2][BATTERY_HEIGHT * BATTERY_WIDTH];

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool charging) {
    // Canvas is of dimensions BATTERY_HEIGHT x BATTERY_WIDTH
    // Empty the canvas first
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_100);

    // Battery base
    lv_draw_rect_dsc_t battery_base;
    lv_draw_rect_dsc_init(&battery_base);
    battery_base.bg_opa = LV_OPA_TRANSP;          // Don't fill center
    battery_base.border_color = lv_color_black(); // Draw border
    battery_base.border_width = 1;
    lv_canvas_draw_rect(canvas, 0, 0, BATTERY_HEIGHT - 2, BATTERY_WIDTH, &battery_base);

    // Battery cap
    lv_draw_rect_dsc_t battery_top;
    lv_draw_rect_dsc_init(&battery_top);
    battery_top.bg_opa = LV_OPA_100;
    battery_top.bg_color = lv_color_black();
    lv_canvas_draw_rect(canvas, BATTERY_HEIGHT - 2, 2, 2, BATTERY_WIDTH - 4, &battery_top);

    // Draw bolt if charging
    if (charging) {
        char charging_arr[5] = {};
        strcat(charging_arr, LV_SYMBOL_PLUS);
        lv_draw_label_dsc_t label_dsc;
        init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
        lv_canvas_draw_text(canvas, 0, -3, BATTERY_HEIGHT, &label_dsc, charging_arr);
    } else {
        // Battery settings
        lv_draw_rect_dsc_t battery_fill;
        lv_draw_rect_dsc_init(&battery_fill);
        battery_fill.bg_opa = LV_OPA_100;
        battery_fill.bg_color = lv_color_black();
        // Determine level of charge
        uint8_t step = 100 / (BATTERY_HEIGHT - 6); // 2 each for upper and lower padding, 2 for cap
        lv_canvas_draw_rect(canvas, 2, 2, level / step, BATTERY_WIDTH - 4, &battery_fill);
    }
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    lv_obj_t *symbol = lv_obj_get_child(widget, state.source == CENTRAL ? 0 : 1);

    draw_battery(symbol, state.level, state.charging);
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_battery_level *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_symbol(widget->canvas, state);
    }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){
        .source = PERIPHERAL,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state){
        .source = CENTRAL,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
        .charging = zmk_usb_is_powered(),
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) {
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_level, struct battery_state, battery_status_update_cb,
                            battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_level, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_level, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_level, zmk_usb_conn_state_changed);

int zmk_widget_battery_level_init(struct zmk_widget_battery_level *widget, lv_obj_t *parent) {
    // Battery Widget area
    widget->canvas = lv_obj_create(parent);
    lv_obj_set_size(widget->canvas, BATTERY_HEIGHT, DISP_WIDTH);
    lv_obj_t *image_canvas_0 = lv_canvas_create(widget->canvas);
    lv_obj_t *image_canvas_1 = lv_canvas_create(widget->canvas);

    // LEFT battery
    lv_canvas_set_buffer(image_canvas_0, battery_image_buffer[0], BATTERY_HEIGHT, BATTERY_WIDTH,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(image_canvas_0, LV_ALIGN_TOP_RIGHT, 0, 0);

    // RIGHT battery
    lv_canvas_set_buffer(image_canvas_1, battery_image_buffer[1], BATTERY_HEIGHT, BATTERY_WIDTH,
                         LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(image_canvas_1, LV_ALIGN_TOP_RIGHT, 0, DISP_WIDTH - BATTERY_WIDTH);

    sys_slist_append(&widgets, &widget->node);

    widget_battery_level_init();

    return 0;
}

lv_obj_t *zmk_widget_battery_level_obj(struct zmk_widget_battery_level *widget) {
    return widget->canvas;
}