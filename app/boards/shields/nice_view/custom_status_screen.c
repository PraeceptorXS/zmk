/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include "custom_status_screen.h"
#include "widgets/bongo_cat_wpm.h"
#include "widgets/battery_level.h"
#include "widgets/output_status.h"
#include "widgets/modifiers_info.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_bongo_cat_wpm bongo_cat_wpm_widget;
static struct zmk_widget_battery_level battery_level_widget;
static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_modifiers modifiers_widget;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

    zmk_widget_battery_level_init(&battery_level_widget, screen);
    lv_obj_align(zmk_widget_battery_level_obj(&battery_level_widget), LV_ALIGN_TOP_RIGHT, 0, 0);

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_RIGHT,
                 OUTPUT_OFFSET, BATTERY_WIDTH);

    zmk_widget_bongo_cat_wpm_init(&bongo_cat_wpm_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_wpm_obj(&bongo_cat_wpm_widget), LV_ALIGN_TOP_RIGHT,
                 WPM_OFFSET, 0);

    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    return screen;
}
