/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_bongo_cat_wpm {
    sys_snode_t node;
    lv_obj_t *canvas;
};

int zmk_widget_bongo_cat_wpm_init(struct zmk_widget_bongo_cat_wpm *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_bongo_cat_wpm_obj(struct zmk_widget_bongo_cat_wpm *widget);