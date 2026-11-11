/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Three-state layer/underglow toggle on a single key.
 *
 * Binding (2 cells): <&layer_ug LOWER_ID RAISE_ID>
 * Colors are node properties: lower-hsb = <h s b>; raise-hsb = <h s b>;
 *
 * State machine (default = layer 0):
 *   default + tap  -> LOWER (solid color)
 *   default + hold -> RAISE (solid color)
 *   LOWER/RAISE + any release -> default (restore color + effect)
 *
 * Pressing RGB_EFF on a sub-layer chooses the effect the default layer
 * resumes with (entry forces solid, so any other live effect is the
 * user's choice).
 *
 * Tap vs hold is decided on release using the press duration (200ms threshold).
 * On LOWER/RAISE a hold also returns to default: the spec only requires the
 * tap, but accepting any release means a slow tap can never strand the key
 * on a sub-layer.
 * The current layers are queried from the keymap (zmk_keymap_layer_active),
 * so external layer switches do not leave this behavior in a stale state.
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

static const struct zmk_led_hsb lower_color = {
	.h = DT_INST_PROP_BY_IDX(0, lower_hsb, 0),
	.s = DT_INST_PROP_BY_IDX(0, lower_hsb, 1),
	.b = DT_INST_PROP_BY_IDX(0, lower_hsb, 2),
};

static const struct zmk_led_hsb raise_color = {
	.h = DT_INST_PROP_BY_IDX(0, raise_hsb, 0),
	.s = DT_INST_PROP_BY_IDX(0, raise_hsb, 1),
	.b = DT_INST_PROP_BY_IDX(0, raise_hsb, 2),
};

struct layer_ug_saved {
	struct zmk_led_hsb color;
	uint8_t effect;
	bool valid;
};

static struct layer_ug_saved saved;
static int64_t press_start;

static void save_underglow(void) {
	if (!saved.valid) {
		saved.color = zmk_rgb_underglow_calc_hue(0);
		saved.effect = zmk_rgb_underglow_calc_effect(0);
		saved.valid = true;
	}
}

static void restore_underglow(void) {
	if (saved.valid) {
		/* Entry forces SOLID, so a live effect that is not SOLID means
		 * RGB_EFF was used on the sub-layer: that choice is the default
		 * layer's effect. Otherwise restore the saved one.
		 * shortcut: SOLID == 0 (first effect in rgb_underglow.c); cycling
		 * back to SOLID deliberately falls back to the saved effect. */
		uint8_t live = zmk_rgb_underglow_calc_effect(0);
		zmk_rgb_underglow_set_hsb(saved.color);
		zmk_rgb_underglow_select_effect(live == 0 ? saved.effect : live);
		saved.valid = false;
	}
}

static int layer_ug_binding_pressed(struct zmk_behavior_binding *binding,
				    struct zmk_behavior_binding_event event) {
	press_start = k_uptime_get();
	return ZMK_BEHAVIOR_OPAQUE;
}

static int layer_ug_binding_released(struct zmk_behavior_binding *binding,
				     struct zmk_behavior_binding_event event) {
	uint8_t lower_layer = binding->param1;
	uint8_t raise_layer = binding->param2;
	bool held = (k_uptime_get() - press_start) > LUG_TAP_MS;
	bool on_lower = zmk_keymap_layer_active(lower_layer);
	bool on_raise = zmk_keymap_layer_active(raise_layer);

	if (on_lower || on_raise) {
		/* On LOWER or RAISE: any release returns to the default layer */
		zmk_keymap_layer_deactivate(on_lower ? lower_layer : raise_layer);
		restore_underglow();
	} else {
		/* On the default layer: tap -> LOWER, hold -> RAISE */
		uint8_t target = held ? raise_layer : lower_layer;
		save_underglow();
		zmk_keymap_layer_activate(target);
		/* shortcut: 0 = UNDERGLOW_EFFECT_SOLID in rgb_underglow.c (not in
		 * the public header); stops any running animation so the solid
		 * layer color isn't overwritten by the next tick */
		zmk_rgb_underglow_select_effect(0);
		zmk_rgb_underglow_set_hsb(target == lower_layer ? lower_color : raise_color);
	}

	LOG_INF("layer_ug: held=%d on_lower=%d on_raise=%d", held, on_lower, on_raise);
	return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_layer_ug_driver_api = {
	.locality = BEHAVIOR_LOCALITY_GLOBAL,
	.binding_pressed = layer_ug_binding_pressed,
	.binding_released = layer_ug_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
			CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			&behavior_layer_ug_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
