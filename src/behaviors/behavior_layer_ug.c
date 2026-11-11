/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Three-state layer/underglow toggle on a single key.
 *
 * Binding (4 cells): <&layer_ug LOWER_ID RAISE_ID HSB_LOWER HSB_RAISE>
 *   HSB packed as (h<<16)|(s<<8)|b  (see RGB_COLOR_HSB_VAL in dt-bindings/zmk/rgb.h)
 *
 * State machine (default = layer 0):
 *   default + tap  -> LOWER (solid color)
 *   default + hold -> RAISE (solid color)
 *   LOWER   + tap  -> default (restore normal underglow)
 *   RAISE   + tap  -> default (restore normal underglow)
 *
 * Tap vs hold is decided on release using the press duration (200ms threshold).
 */

#define DT_DRV_COMPAT zmk_behavior_layer_ug

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define LUG_TAP_MS 200

enum layer_ug_state { LUG_DEFAULT, LUG_LOWER, LUG_RAISE };

struct behavior_layer_ug_data {
    enum layer_ug_state state;
    int64_t press_start;
    struct zmk_led_hsb saved_color;
    bool saved_valid;
};

static struct behavior_layer_ug_data data;

static void set_solid(uint32_t hsb) {
    zmk_rgb_underglow_set_hsb((struct zmk_led_hsb){
        .h = (hsb >> 16) & 0xFFFF,
        .s = (hsb >> 8) & 0xFF,
        .b = hsb & 0xFF,
    });
}

static void save_current(struct behavior_layer_ug_data *d) {
    if (!d->saved_valid) {
        d->saved_color = zmk_rgb_underglow_calc_hue(0); /* direction 0 = unchanged */
        d->saved_valid = true;
    }
}

static void restore_current(struct behavior_layer_ug_data *d) {
    if (d->saved_valid) {
        zmk_rgb_underglow_set_hsb(d->saved_color);
        d->saved_valid = false;
    }
}

static int layer_ug_binding_pressed(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    data.press_start = k_uptime_get();
    return ZMK_BEHAVIOR_OPAQUE;
}

static int layer_ug_binding_released(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    int lower_layer = binding->param1;
    int raise_layer = binding->param2;
    uint32_t hsb_lower = binding->param3;
    uint32_t hsb_raise = binding->param4;
    bool held = (k_uptime_get() - data.press_start) > LUG_TAP_MS;

    switch (data.state) {
    case LUG_DEFAULT:
        if (held) {
            save_current(&data);
            zmk_keymap_layer_activate(raise_layer);
            set_solid(hsb_raise);
            data.state = LUG_RAISE;
        } else {
            save_current(&data);
            zmk_keymap_layer_activate(lower_layer);
            set_solid(hsb_lower);
            data.state = LUG_LOWER;
        }
        break;
    case LUG_LOWER:
        zmk_keymap_layer_deactivate(lower_layer);
        restore_current(&data);
        data.state = LUG_DEFAULT;
        break;
    case LUG_RAISE:
        zmk_keymap_layer_deactivate(raise_layer);
        restore_current(&data);
        data.state = LUG_DEFAULT;
        break;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_layer_ug_driver_api = {
    .binding_pressed = layer_ug_binding_pressed,
    .binding_released = layer_ug_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, &data, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_layer_ug_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
