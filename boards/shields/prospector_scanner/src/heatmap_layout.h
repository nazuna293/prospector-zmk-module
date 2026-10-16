/*
 * Heatmap layout for Scanner Mode (Prospector Display screen)
 *
 * Draws the physical key layout of the Enigma_01_Pro (layout_b, 41 keys) and shades each key by
 * how often it was pressed over the last ~10 minutes (4 levels). The keyboard sends only this
 * accumulated, quantised frequency - never which key was pressed just now.
 * Palettes: Dark Modern / COBOL Green (double-tap to switch).
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>
#include "prospector_layouts.h"

lv_obj_t *heatmap_layout_create(lv_obj_t *parent);
void heatmap_layout_update(const struct prospector_keyboard_data *d);
void heatmap_layout_destroy(void);
void heatmap_layout_cycle_palette(void);
