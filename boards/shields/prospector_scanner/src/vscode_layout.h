/*
 * VSCode layout for Scanner Mode (Prospector Display screen)
 *
 * Keeps the information layout of the original YADS main screen
 * (name / WPM / connection / layer / modifiers / battery / signal) and
 * restyles each part with VS Code vocabulary:
 *   layer row  -> editor tabs
 *   connection -> status bar "remote" chip
 *   modifiers  -> keybinding labels
 *   battery    -> progress bar (severity colours)
 *   signal     -> status bar strip
 * Palettes: Dark Modern / COBOL Green (double-tap to switch).
 * Design notes: Keyboard/ZMK/Prospector-VSCode配色設計.md in the owner's vault.
 */
#pragma once

#include <lvgl.h>
#include <stdint.h>
#include "prospector_layouts.h"

lv_obj_t *vscode_layout_create(lv_obj_t *parent);
void vscode_layout_update(const struct prospector_keyboard_data *d);
void vscode_layout_update_signal(int8_t rssi, int32_t rate_x100);
void vscode_layout_set_waiting(void);
void vscode_layout_update_scanner_battery(int level);
void vscode_layout_push_keys(uint8_t n);   /* key presses since the last call (typing wave) */
void vscode_layout_destroy(void);

/* Dark Modern <-> COBOL Green */
void vscode_layout_cycle_palette(void);
uint8_t vscode_layout_get_palette(void);
const char *vscode_layout_get_palette_name(void);
