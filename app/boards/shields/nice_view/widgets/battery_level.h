/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_battery_level {
    sys_snode_t node;
    lv_obj_t *canvas;
};

int zmk_widget_battery_level_init(struct zmk_widget_battery_level *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_battery_level_obj(struct zmk_widget_battery_level *widget);