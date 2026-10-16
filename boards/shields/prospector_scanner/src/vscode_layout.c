/*
 * VSCode layout for Scanner Mode - see vscode_layout.h
 *
 * Screen: 280x240 (landscape). Positions follow the original YADS main screen
 * (custom_status_screen.c): name y=25 / WPM left / connection right /
 * layer centre / modifiers y~145 / battery bar near the bottom / signal row y=219.
 *
 * Colours: VS Code "Dark Modern" and "Light 2026" (microsoft/vscode
 * extensions/theme-defaults/themes/*.json). A few values that the theme files do
 * not define (selection tint, warning text in light theme, ...) are estimates.
 */
#include "vscode_layout.h"
#include "display_settings.h"
#include "fonts.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(vscode_layout, CONFIG_ZMK_LOG_LEVEL);

#define VS_W        280
#define VS_H        240
#define VS_BAND_Y   218
#define VS_MAX_TABS 8
#define VS_BAR_W    165

/* Typing activity wave: between the WPM block and the connection chip */
#define VS_WAVE_N    24     /* bars */
#define VS_WAVE_X    74     /* left edge */
#define VS_WAVE_BASE 86     /* baseline y */
#define VS_WAVE_STEP 5      /* px per level */
#define VS_WAVE_MAX  6      /* levels */

/* ========== Palettes ========== */

typedef struct {
    uint32_t editor;      /* editor.background */
    uint32_t chrome;      /* tab.inactiveBackground / statusBar.background */
    uint32_t border;      /* statusBar.border */
    uint32_t fg;          /* foreground */
    uint32_t desc;        /* descriptionForeground */
    uint32_t disabled;    /* disabledForeground */
    uint32_t tab_dim;     /* inactive tab number / editorLineNumber.foreground */
    uint32_t accent;      /* focusBorder / statusBarItem.remoteBackground */
    uint32_t sel;         /* editor.selectionBackground */
    uint32_t on_sel;      /* text on selection */
    uint32_t number;      /* numeric literal */
    uint32_t comment;     /* comment */
    uint32_t warn;        /* warning text */
    uint32_t err;         /* error text */
    uint32_t warn_bg;     /* statusBarItem.warningBackground */
    uint32_t err_bg;      /* statusBarItem.errorBackground */
    uint32_t offline;     /* statusBarItem.offlineBackground */
    uint32_t neutral;     /* neutral chip */
    uint32_t band_fg;     /* status bar text */
    const char *name;
} vs_palette_t;

static const vs_palette_t PAL[2] = {
    {   /* Dark Modern */
        .editor = 0x1F1F1F, .chrome = 0x181818, .border = 0x2B2B2B,
        .fg = 0xCCCCCC, .desc = 0x9D9D9D, .disabled = 0x767676, .tab_dim = 0x6E7681,
        .accent = 0x0078D4, .sel = 0x264F78, .on_sel = 0xFFFFFF,
        .number = 0xB5CEA8, .comment = 0x6A9955,
        .warn = 0xCCA700, .err = 0xF85149, .warn_bg = 0x7A6400, .err_bg = 0xC72E0F,
        .offline = 0x6C1717, .neutral = 0x3A3D40, .band_fg = 0xCCCCCC,
        .name = "Dark Modern",
    },
    {   /* Light 2026 */
        .editor = 0xFFFFFF, .chrome = 0xFAFAFD, .border = 0xF0F1F2,
        .fg = 0x202020, .desc = 0x606060, .disabled = 0xBBBBBB, .tab_dim = 0x606060,
        .accent = 0x0069CC, .sel = 0xB3D0F0, .on_sel = 0x202020,
        .number = 0x0550AE, .comment = 0x6E7781,
        .warn = 0x8A6D00, .err = 0xAD0707, .warn_bg = 0x7A6400, .err_bg = 0xC72E0F,
        .offline = 0xC93C3C, .neutral = 0x8C8C8C, .band_fg = 0x606060,
        .name = "Light 2026",
    },
};

static uint8_t cur_pal = 0;

/* ========== State and widgets ========== */

static struct {
    bool valid;            /* data received at least once since create */
    char name[24];
    uint8_t layer;
    char lname[8];
    uint8_t mods;
    uint8_t wpm;
    uint8_t bat;
    bool usb;
    bool ble_conn;
    bool ble_bond;
    uint8_t profile;
    int8_t rssi;
    int32_t rate_x100;
    bool has_signal;
    int sbat;              /* scanner's own battery %, -1 = unknown */
} st = {.sbat = -1};   /* kept across destroy/create so the screen redraws at once */

static lv_obj_t *s_parent;
static lv_obj_t *s_root;
static lv_obj_t *s_name;
static lv_obj_t *s_sbat_lbl;
static lv_obj_t *s_wpm_t, *s_wpm_v;
static lv_obj_t *s_conn_chip, *s_conn_lbl;
static lv_obj_t *s_layer_t, *s_layer_name;
static lv_obj_t *s_tab[VS_MAX_TABS], *s_tab_lbl[VS_MAX_TABS], *s_tab_line[VS_MAX_TABS];
static lv_obj_t *s_mod[4], *s_mod_lbl[4];
static lv_obj_t *s_bat_lbl, *s_bat_track, *s_bat_fill;
static lv_obj_t *s_wait_cover, *s_wait_name, *s_wait_l1, *s_wait_l2;
static lv_obj_t *s_band, *s_ch_lbl, *s_rx_lbl, *s_sig[5], *s_dbm_lbl, *s_hz_lbl;
static uint8_t s_ntabs;
static lv_obj_t *s_wave[VS_WAVE_N];
static lv_timer_t *s_wave_timer;
static uint8_t s_hist[VS_WAVE_N];   /* oldest first */
static uint16_t s_wave_acc;         /* key presses since the last tick */
static bool s_wave_dirty;           /* bars need a redraw */

/* ========== Helpers ========== */

static lv_obj_t *vs_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* Label with a fixed width. align: LV_TEXT_ALIGN_LEFT / CENTER / RIGHT */
static lv_obj_t *vs_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                          const char *text, int x, int y, int w, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
    lv_obj_set_width(l, w);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void vs_show(lv_obj_t *o, bool show) {
    if (!o) return;
    if (show) {
        lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    }
}

static uint8_t vs_rssi_bars(int8_t r) {
    if (r >= -50) return 5;
    if (r >= -60) return 4;
    if (r >= -70) return 3;
    if (r >= -80) return 2;
    if (r >= -90) return 1;
    return 0;
}

/* Layer name for the label under the tabs: the scanner-side table
 * (CONFIG_PROSPECTOR_LAYER_NAMES, comma separated) wins; otherwise the 4-char
 * name that arrived in the advertisement. */
static const char *layer_label(int idx, const char *fallback, char *buf, size_t n) {
    const char *s = CONFIG_PROSPECTOR_LAYER_NAMES;
    int cur = 0;
    while (*s) {
        const char *e = strchr(s, ',');
        size_t len = e ? (size_t)(e - s) : strlen(s);
        if (cur == idx) {
            if (len == 0) break;
            if (len >= n) len = n - 1;
            memcpy(buf, s, len);
            buf[len] = '\0';
            return buf;
        }
        cur++;
        if (!e) break;
        s = e + 1;
    }
    return fallback;
}

/* ========== Dynamic updates (each is idempotent) ========== */

static void upd_name(void) {
    if (!s_name) return;
    lv_label_set_text(s_name, st.name[0] ? st.name : "Scanning...");
}

static void upd_wpm(void) {
    if (!s_wpm_v) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)st.wpm);
    lv_label_set_text(s_wpm_v, buf);
}

/* Connection: status bar "remote" chip. connected = accent, paired but
 * disconnected = offline (dark red), unpaired = neutral grey. USB = accent. */
static void upd_conn(void) {
    if (!s_conn_chip) return;
    const vs_palette_t *p = &PAL[cur_pal];
    char buf[12];
    uint32_t col;
    if (st.usb) {
        snprintf(buf, sizeof(buf), "USB");
        col = p->accent;
    } else {
        snprintf(buf, sizeof(buf), "BLE %u", (unsigned)st.profile);
        col = st.ble_conn ? p->accent : (st.ble_bond ? p->offline : p->neutral);
    }
    lv_label_set_text(s_conn_lbl, buf);
    lv_obj_set_style_bg_color(s_conn_chip, lv_color_hex(col), LV_PART_MAIN);
}

/* Layer: editor tabs. The active tab gets the editor background, an accent top
 * line and a larger bold number. */
static void upd_layer(void) {
    const vs_palette_t *p = &PAL[cur_pal];
    for (int i = 0; i < s_ntabs; i++) {
        if (!s_tab[i]) continue;
        bool act = (i == st.layer);
        lv_obj_set_style_bg_color(s_tab[i], lv_color_hex(act ? p->editor : p->chrome), LV_PART_MAIN);
        vs_show(s_tab_line[i], act);
        lv_obj_set_style_text_font(s_tab_lbl[i],
                                   act ? &JetBrainsMono_Bold_20 : &JetBrainsMono_Regular_16,
                                   LV_PART_MAIN);
        lv_obj_set_style_text_color(s_tab_lbl[i], lv_color_hex(act ? p->fg : p->tab_dim), LV_PART_MAIN);
        lv_obj_set_y(s_tab_lbl[i], act ? 5 : 8);
    }
    if (s_layer_name) {
        char nb[20];
        lv_label_set_text(s_layer_name, layer_label(st.layer, st.lname, nb, sizeof(nb)));
    }
}

/* Modifiers: keybinding labels, shown only while pressed (as in YADS). */
static void upd_mods(void) {
    const vs_palette_t *p = &PAL[cur_pal];
    bool on[4] = {
        (st.mods & 0x11) != 0,  /* Ctrl */
        (st.mods & 0x22) != 0,  /* Shift */
        (st.mods & 0x44) != 0,  /* Alt */
        (st.mods & 0x88) != 0,  /* Win/GUI */
    };
    const int cw = 58, gap = 6;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (on[i]) n++;
    }
    int x = (VS_W - (n * cw + (n > 0 ? (n - 1) * gap : 0))) / 2;
    for (int i = 0; i < 4; i++) {
        if (!s_mod[i]) continue;
        if (on[i]) {
            lv_obj_set_pos(s_mod[i], x, 144);
            lv_obj_set_style_bg_color(s_mod[i], lv_color_hex(p->sel), LV_PART_MAIN);
            lv_obj_set_style_border_color(s_mod[i], lv_color_hex(p->accent), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_mod_lbl[i], lv_color_hex(p->on_sel), LV_PART_MAIN);
            x += cw + gap;
            vs_show(s_mod[i], true);
        } else {
            vs_show(s_mod[i], false);
        }
    }
}

/* Battery: progress bar. >=40 accent, 20-39 warning, <20 error. */
static void upd_battery(void) {
    if (!s_bat_lbl) return;
    const vs_palette_t *p = &PAL[cur_pal];
    char buf[8];
    uint32_t col;
    int w;
    if (st.bat == 0) {
        snprintf(buf, sizeof(buf), "--");
        col = p->disabled;
        w = 0;
    } else {
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)st.bat);
        col = (st.bat >= 40) ? p->accent : (st.bat >= 20 ? p->warn : p->err);
        w = (VS_BAR_W * (int)st.bat) / 100;
        if (w < 2) w = 2;
    }
    lv_label_set_text(s_bat_lbl, buf);
    lv_obj_set_style_text_color(s_bat_lbl, lv_color_hex(col), LV_PART_MAIN);
    lv_obj_set_width(s_bat_fill, w > 0 ? w : 1);
    lv_obj_set_style_bg_color(s_bat_fill, lv_color_hex(col), LV_PART_MAIN);
    vs_show(s_bat_fill, w > 0);
}

static void upd_channel(void) {
    if (!s_ch_lbl) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "ch %u", (unsigned)display_settings_get_channel());
    lv_label_set_text(s_ch_lbl, buf);
}

/* Signal: status bar strip. Weak signal (<= -80 dBm) turns warning colour. */
static void upd_signal(void) {
    if (!s_dbm_lbl) return;
    const vs_palette_t *p = &PAL[cur_pal];
    uint8_t bars = st.has_signal ? vs_rssi_bars(st.rssi) : 0;
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_bg_color(s_sig[i], lv_color_hex(i < bars ? p->band_fg : p->disabled),
                                  LV_PART_MAIN);
    }
    char buf[16];
    if (st.has_signal) {
        snprintf(buf, sizeof(buf), "%ddBm", (int)st.rssi);
    } else {
        snprintf(buf, sizeof(buf), "--dBm");
    }
    lv_label_set_text(s_dbm_lbl, buf);
    lv_obj_set_style_text_color(s_dbm_lbl,
                                lv_color_hex((st.has_signal && st.rssi <= -80) ? p->warn : p->band_fg),
                                LV_PART_MAIN);
    if (st.has_signal && st.rate_x100 >= 0) {
        snprintf(buf, sizeof(buf), "%d.%dHz", (int)(st.rate_x100 / 100), (int)((st.rate_x100 % 100) / 10));
    } else {
        snprintf(buf, sizeof(buf), "-.-Hz");
    }
    lv_label_set_text(s_hz_lbl, buf);
}

/* Scanner's own battery: small text at the top right (YADS: (216,4)). Shown only
 * when the battery widget is enabled in the display settings. */
static void upd_scanner_battery(void) {
    if (!s_sbat_lbl) return;
    const vs_palette_t *p = &PAL[cur_pal];
    if (st.sbat < 0 || !display_settings_get_battery_visible()) {
        vs_show(s_sbat_lbl, false);
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", st.sbat);
    lv_label_set_text(s_sbat_lbl, buf);
    lv_obj_set_style_text_color(s_sbat_lbl,
                                lv_color_hex(st.sbat < 20 ? p->err : (st.sbat < 40 ? p->warn : p->desc)),
                                LV_PART_MAIN);
    vs_show(s_sbat_lbl, true);
}

static void upd_waiting(void) {
    vs_show(s_wait_cover, !st.valid);
}

/* Typing activity wave: one sample per 100 ms tick. A sample is the number of key
 * presses seen since the last tick (x2, capped), or the previous sample minus one,
 * whichever is larger, so a keystroke leaves a short decaying bump. */
static void draw_wave(void) {
    const vs_palette_t *p = &PAL[cur_pal];
    for (int i = 0; i < VS_WAVE_N; i++) {
        if (!s_wave[i]) continue;
        int h = 2 + s_hist[i] * VS_WAVE_STEP;
        lv_obj_set_size(s_wave[i], 4, h);
        lv_obj_set_pos(s_wave[i], VS_WAVE_X + i * 5, VS_WAVE_BASE - h);
        lv_obj_set_style_bg_color(s_wave[i], lv_color_hex(s_hist[i] ? p->accent : p->disabled), LV_PART_MAIN);
    }
}

static void wave_tick(lv_timer_t *t) {
    ARG_UNUSED(t);
    int lv = (int)s_wave_acc * 2;
    s_wave_acc = 0;
    int decay = (int)s_hist[VS_WAVE_N - 1] - 1;
    if (lv < decay) lv = decay;
    if (lv > VS_WAVE_MAX) lv = VS_WAVE_MAX;
    if (lv < 0) lv = 0;

    bool any = (lv > 0);
    for (int i = 0; i < VS_WAVE_N - 1; i++) {
        s_hist[i] = s_hist[i + 1];
        if (s_hist[i]) any = true;
    }
    s_hist[VS_WAVE_N - 1] = (uint8_t)lv;

    if (any || s_wave_dirty) {
        draw_wave();
        s_wave_dirty = any;   /* one last redraw once everything is flat */
    }
}

static void refresh_all(void) {
    upd_name();
    upd_wpm();
    upd_conn();
    upd_layer();
    upd_mods();
    upd_battery();
    upd_channel();
    upd_signal();
    upd_scanner_battery();
    upd_waiting();
    draw_wave();
}

/* ========== Create / destroy ========== */

static void clear_pointers(void) {
    s_root = NULL;
    s_sbat_lbl = NULL;
    s_name = s_wpm_t = s_wpm_v = s_conn_chip = s_conn_lbl = s_layer_t = s_layer_name = NULL;
    for (int i = 0; i < VS_MAX_TABS; i++) {
        s_tab[i] = s_tab_lbl[i] = s_tab_line[i] = NULL;
    }
    for (int i = 0; i < 4; i++) {
        s_mod[i] = s_mod_lbl[i] = NULL;
    }
    s_bat_lbl = s_bat_track = s_bat_fill = NULL;
    s_wait_cover = s_wait_name = s_wait_l1 = s_wait_l2 = NULL;
    s_band = s_ch_lbl = s_rx_lbl = s_dbm_lbl = s_hz_lbl = NULL;
    for (int i = 0; i < 5; i++) {
        s_sig[i] = NULL;
    }
    for (int i = 0; i < VS_WAVE_N; i++) {
        s_wave[i] = NULL;
    }
}

static void teardown(void) {
    if (s_wave_timer) {
        lv_timer_del(s_wave_timer);
        s_wave_timer = NULL;
    }
    if (s_root != NULL) {
        lv_obj_del(s_root);
    }
    clear_pointers();
}

static void build(lv_obj_t *parent) {
    const vs_palette_t *p = &PAL[cur_pal];
    static const char *mod_names[4] = {"Ctrl", "Shift", "Alt", "Win"};

    s_ntabs = display_settings_get_max_layers();
    if (s_ntabs < 1) s_ntabs = 7;
    if (s_ntabs > VS_MAX_TABS) s_ntabs = VS_MAX_TABS;

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, VS_W, VS_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(p->editor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    /* Keyboard name: top centre (YADS: y=25) */
    s_name = vs_label(s_root, &JetBrainsMono_Bold_16, p->fg, "Scanning...", 40, 22, 200, LV_TEXT_ALIGN_CENTER);

    /* Scanner's own battery: top right */
    s_sbat_lbl = vs_label(s_root, &JetBrainsMono_Regular_10, p->desc, "", VS_W - 66, 6, 56, LV_TEXT_ALIGN_RIGHT);
    vs_show(s_sbat_lbl, false);

    /* WPM: left corner (YADS: x=8..56, y=53). Value = numeric literal colour. */
    s_wpm_t = vs_label(s_root, &JetBrainsMono_Regular_10, p->desc, "wpm", 14, 54, 50, LV_TEXT_ALIGN_LEFT);
    s_wpm_v = vs_label(s_root, &JetBrainsMono_Bold_16, p->number, "0", 14, 66, 50, LV_TEXT_ALIGN_LEFT);

    /* Connection: right corner (YADS: top right, y=53). Remote-indicator style chip. */
    s_conn_chip = vs_rect(s_root, VS_W - 12 - 62, 52, 62, 22, p->neutral);
    lv_obj_set_style_radius(s_conn_chip, 3, LV_PART_MAIN);
    s_conn_lbl = vs_label(s_conn_chip, &JetBrainsMono_Bold_12, 0xFFFFFF, "BLE 0",
                          0, 3, 62, LV_TEXT_ALIGN_CENTER);

    /* Typing activity wave (new): bars scroll left every 100 ms */
    for (int i = 0; i < VS_WAVE_N; i++) {
        s_wave[i] = vs_rect(s_root, VS_WAVE_X + i * 5, VS_WAVE_BASE - 2, 4, 2, p->disabled);
    }
    s_wave_timer = lv_timer_create(wave_tick, 100, NULL);

    /* Layer: heading + editor tabs (YADS: y=82 heading, numbers below) */
    s_layer_t = vs_label(s_root, &JetBrainsMono_Regular_12, p->desc, "Layer", 90, 80, 100, LV_TEXT_ALIGN_CENTER);
    {
        int tw = 232 / s_ntabs;
        int x0 = (VS_W - tw * s_ntabs) / 2;
        for (int i = 0; i < s_ntabs; i++) {
            s_tab[i] = vs_rect(s_root, x0 + i * tw, 96, tw - 2, 30, p->chrome);
            s_tab_lbl[i] = vs_label(s_tab[i], &JetBrainsMono_Regular_16, p->tab_dim, "", 0, 8, tw - 2,
                                    LV_TEXT_ALIGN_CENTER);
            char num[4];
            snprintf(num, sizeof(num), "%d", i);
            lv_label_set_text(s_tab_lbl[i], num);
            s_tab_line[i] = vs_rect(s_tab[i], 0, 0, tw - 2, 2, p->accent);
            vs_show(s_tab_line[i], false);
        }
    }
    s_layer_name = vs_label(s_root, &JetBrainsMono_Regular_12, p->desc, "", 40, 129, 200, LV_TEXT_ALIGN_CENTER);

    /* Modifiers: keybinding labels (YADS: y=145). Created hidden. */
    for (int i = 0; i < 4; i++) {
        s_mod[i] = vs_rect(s_root, 0, 144, 58, 32, p->sel);
        lv_obj_set_style_radius(s_mod[i], 4, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_mod[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_mod[i], lv_color_hex(p->accent), LV_PART_MAIN);
        s_mod_lbl[i] = vs_label(s_mod[i], &JetBrainsMono_Bold_16, p->on_sel, mod_names[i], 0, 6, 56,
                                LV_TEXT_ALIGN_CENTER);
        vs_show(s_mod[i], false);
    }

    /* Battery: % above a full-width bar (YADS: y=-42 / -33 from the bottom) */
    s_bat_lbl = vs_label(s_root, &JetBrainsMono_Bold_16, p->accent, "--", 90, 181, 100, LV_TEXT_ALIGN_CENTER);
    s_bat_track = vs_rect(s_root, (VS_W - VS_BAR_W) / 2, 203, VS_BAR_W, 6, p->chrome);
    s_bat_fill = vs_rect(s_root, (VS_W - VS_BAR_W) / 2, 203, 1, 6, p->accent);

    /* Waiting cover (no keyboard yet / timed out) */
    s_wait_cover = vs_rect(s_root, 0, 0, VS_W, VS_BAND_Y, p->editor);
    s_wait_name = vs_label(s_wait_cover, &JetBrainsMono_Bold_16, p->desc, "Scanning...", 40, 22, 200,
                           LV_TEXT_ALIGN_CENTER);
    s_wait_l1 = vs_label(s_wait_cover, &JetBrainsMono_Regular_12, p->comment, "// waiting for keyboard...",
                         20, 100, 240, LV_TEXT_ALIGN_CENTER);
    s_wait_l2 = vs_label(s_wait_cover, &JetBrainsMono_Regular_12, p->comment, "// scanning", 20, 120, 240,
                         LV_TEXT_ALIGN_CENTER);

    /* Status bar strip (YADS: signal row y=219: Ch x=62, RX x=102, bars x=130, dBm x=167, Hz x=222) */
    s_band = vs_rect(s_root, 0, VS_BAND_Y, VS_W, VS_H - VS_BAND_Y, p->chrome);
    {
        lv_obj_t *line = vs_rect(s_band, 0, 0, VS_W, 1, p->border);
        (void)line;
    }
    s_ch_lbl = vs_label(s_band, &JetBrainsMono_Regular_10, p->band_fg, "ch 0", 62, 5, 40, LV_TEXT_ALIGN_LEFT);
    s_rx_lbl = vs_label(s_band, &JetBrainsMono_Regular_10, p->band_fg, "RX", 104, 5, 22, LV_TEXT_ALIGN_LEFT);
    for (int i = 0; i < 5; i++) {
        int h = 3 + i * 2;
        s_sig[i] = vs_rect(s_band, 126 + i * 5, 17 - h + 1, 3, h, p->disabled);
    }
    s_dbm_lbl = vs_label(s_band, &JetBrainsMono_Regular_10, p->band_fg, "--dBm", 156, 5, 62, LV_TEXT_ALIGN_LEFT);
    s_hz_lbl = vs_label(s_band, &JetBrainsMono_Regular_10, p->band_fg, "-.-Hz", 222, 5, 56, LV_TEXT_ALIGN_LEFT);
}

lv_obj_t *vscode_layout_create(lv_obj_t *parent) {
    if (s_root != NULL) {
        LOG_WRN("VSCode layout already created");
        return s_root;
    }
    memset(s_hist, 0, sizeof(s_hist));
    s_wave_acc = 0;
    s_wave_dirty = false;
    s_parent = parent;
    build(parent);
    refresh_all();
    LOG_INF("VSCode layout created (%s)", PAL[cur_pal].name);
    return s_root;
}

void vscode_layout_destroy(void) {
    teardown();
    s_parent = NULL;
}

void vscode_layout_cycle_palette(void) {
    if (s_root == NULL || s_parent == NULL) return;
    cur_pal = (cur_pal + 1) % 2;
    teardown();
    build(s_parent);
    refresh_all();
    LOG_INF("VSCode palette: %s", PAL[cur_pal].name);
}

uint8_t vscode_layout_get_palette(void) {
    return cur_pal;
}

const char *vscode_layout_get_palette_name(void) {
    return PAL[cur_pal].name;
}

/* ========== Data entry points ========== */

void vscode_layout_update(const struct prospector_keyboard_data *d) {
    if (d == NULL) return;
    /* The layout framework replays cached data right after create(); ignore it until real data arrives */
    if (!d->has_dynamic_data) return;

    bool first = !st.valid;
    st.valid = true;

    if (first || strncmp(st.name, d->keyboard_name, sizeof(st.name)) != 0) {
        strncpy(st.name, d->keyboard_name, sizeof(st.name) - 1);
        st.name[sizeof(st.name) - 1] = '\0';
        upd_name();
    }
    if (first || st.wpm != d->wpm_value) {
        st.wpm = d->wpm_value;
        upd_wpm();
    }
    if (first || st.usb != d->usb_connected || st.ble_conn != d->ble_connected ||
        st.ble_bond != d->ble_bonded || st.profile != d->profile_slot) {
        st.usb = d->usb_connected;
        st.ble_conn = d->ble_connected;
        st.ble_bond = d->ble_bonded;
        st.profile = d->profile_slot;
        upd_conn();
    }
    if (first || st.layer != d->active_layer || strncmp(st.lname, d->current_layer_name, sizeof(st.lname)) != 0) {
        st.layer = d->active_layer;
        strncpy(st.lname, d->current_layer_name, sizeof(st.lname) - 1);
        st.lname[sizeof(st.lname) - 1] = '\0';
        upd_layer();
    }
    if (first || st.mods != d->modifier_flags) {
        st.mods = d->modifier_flags;
        upd_mods();
    }
    if (first || st.bat != d->battery_level) {
        st.bat = d->battery_level;
        upd_battery();
    }
    if (first) {
        upd_channel();
        upd_waiting();
    }
}

void vscode_layout_update_signal(int8_t rssi, int32_t rate_x100) {
    st.rssi = rssi;
    st.rate_x100 = rate_x100;
    st.has_signal = true;
    upd_signal();
    upd_channel();
}

void vscode_layout_set_waiting(void) {
    st.valid = false;
    st.has_signal = false;
    upd_signal();
    upd_waiting();
}

void vscode_layout_update_scanner_battery(int level) {
    st.sbat = level;
    upd_scanner_battery();
}

/* Called every 100 ms with the key presses counted since the last call */
void vscode_layout_push_keys(uint8_t n) {
    if (s_wave_timer == NULL || n == 0) return;   /* not on screen */
    s_wave_acc += n;
    if (s_wave_acc > 100) {
        s_wave_acc = 100;
    }
}
