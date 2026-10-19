/*
 * VSCode layout for Scanner Mode - see vscode_layout.h
 *
 * Screen: 280x240 (landscape). Positions follow the original YADS main screen
 * (custom_status_screen.c): name y=25 / WPM left / connection right /
 * layer centre / modifiers y~145 / battery bar near the bottom / signal row y=219.
 *
 * Colours: VS Code "Dark Modern" (microsoft/vscode
 * extensions/theme-defaults/themes/*.json). A few values that the theme files do
 * not define (selection tint, ...) are estimates. "COBOL Green" is a classic green-screen terminal theme.
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

/* Typing equalizer: between the WPM block and the connection chip */
#define VS_EQ_N      16     /* bands */
#define VS_EQ_X      72     /* left edge */
#define VS_EQ_W      6      /* bar width */
#define VS_EQ_PITCH  8      /* bar width + 2px gap -> right edge x=198, left of the chip */
#define VS_EQ_BASE   76     /* baseline y (the "Layer" heading starts at y=80) */
#define VS_EQ_MAXH   28     /* tallest bar: top at y=48, below the keyboard name */

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
    uint32_t on_accent;   /* text on an accent-coloured chip */
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
        .accent = 0x0078D4, .sel = 0x264F78, .on_sel = 0xFFFFFF, .on_accent = 0xFFFFFF,
        .number = 0xB5CEA8, .comment = 0x6A9955,
        .warn = 0xCCA700, .err = 0xF85149, .warn_bg = 0x7A6400, .err_bg = 0xC72E0F,
        .offline = 0x6C1717, .neutral = 0x3A3D40, .band_fg = 0xCCCCCC,
        .name = "Dark Modern",
    },
    {   /* COBOL Green: black screen, phosphor green (classic mainframe terminal).
         * Green = #3BBF3B, the slightly calmer than the heatmap red; heatmap captions keep #4DF14D tones. */
        .editor = 0x000000, .chrome = 0x030A03, .border = 0x0C260C,
        .fg = 0x3BBF3B, .desc = 0x277E27, .disabled = 0x184C18, .tab_dim = 0x226F22,
        .accent = 0x3BBF3B, .sel = 0x113511, .on_sel = 0xB1E5B1, .on_accent = 0x000000,
        .number = 0x89D989, .comment = 0x2E952E,
        .warn = 0xFFC800, .err = 0xFF4B3A, .warn_bg = 0x7A6400, .err_bg = 0xC72E0F,
        .offline = 0x7A1A1A, .neutral = 0x184C18, .band_fg = 0x277E27,
        .name = "COBOL Green",
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
    uint32_t total;        /* key presses seen since boot (this session) */
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
static lv_obj_t *s_eq[VS_EQ_N];     /* bars */
static lv_obj_t *s_cap[VS_EQ_N];    /* peak-hold caps */
static lv_timer_t *s_wave_timer;
static uint8_t s_lvl[VS_EQ_N];      /* bar height (px) */
static uint8_t s_peak[VS_EQ_N];     /* peak cap height (px) */
static uint8_t s_hold[VS_EQ_N];     /* ticks the cap stays before it falls */
static uint16_t s_wave_acc;         /* key presses since the last tick */
static bool s_wave_dirty;           /* bars need a redraw */
static uint32_t s_rng = 2463534242u;

/* Modifier keycaps linger after release; the spot below them shows layer changes and an idle
 * comment line. */
#define VS_MOD_LINGER_MS   800
#define VS_TOAST_LAYER_MS  1000    /* layer change (not shown when returning to layer 0) */
#define VS_IDLE_CYCLE_MS   10000   /* idle line: shown once per cycle ... */
#define VS_IDLE_SHOW_MS    3000    /* ... for this long; keys / best wpm alternate per cycle */
static uint32_t s_mod_until[4];     /* tick when a released modifier's keycap disappears (0 = none) */
static bool s_mod_prev[4];          /* modifier was held at the last update */
static int s_mod_n;                 /* keycaps currently drawn */
static lv_obj_t *s_info;
static uint8_t s_info_kind = 0xFF;  /* 0 hidden, 1 layer toast, 3 idle line */
static char s_info_txt[32];
static char s_toast[24];
static uint8_t s_toast_kind;        /* 1 layer, 2 milestone */
static uint32_t s_toast_t0;
static uint32_t s_toast_ms;

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

/* The scanner-side name list for the keyboard (CONFIG_PROSPECTOR_LAYER_NAMES).
 * Lists are separated by ';'. "Prefix:Name,Name,..." is used for keyboards whose
 * name starts with Prefix; a list without "Prefix:" applies to any keyboard (this
 * is also the old single-list format). Returns NULL if nothing applies. */
static const char *layer_names_for(const char *kb, size_t *out_len) {
    const char *s = CONFIG_PROSPECTOR_LAYER_NAMES;
    const char *any = NULL;
    size_t any_len = 0;
    while (*s) {
        const char *end = strchr(s, ';');
        size_t len = end ? (size_t)(end - s) : strlen(s);
        const char *colon = memchr(s, ':', len);
        if (colon == NULL) {
            if (any == NULL) {
                any = s;
                any_len = len;
            }
        } else {
            size_t plen = (size_t)(colon - s);
            if (plen > 0 && strncmp(kb, s, plen) == 0) {
                *out_len = len - plen - 1;
                return colon + 1;
            }
        }
        if (!end) break;
        s = end + 1;
    }
    *out_len = any_len;
    return any;
}

/* Layer name for the label under the tabs: the scanner-side list for this
 * keyboard wins; otherwise the 4-char name that arrived in the advertisement. */
static const char *layer_label(int idx, const char *fallback, char *buf, size_t n) {
    size_t total;
    const char *s = layer_names_for(st.name, &total);
    if (s == NULL) return fallback;
    const char *stop = s + total;
    int cur = 0;
    while (s < stop) {
        const char *e = memchr(s, ',', (size_t)(stop - s));
        size_t len = e ? (size_t)(e - s) : (size_t)(stop - s);
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
    lv_obj_set_style_text_color(s_conn_lbl, lv_color_hex(col == p->accent ? p->on_accent : 0xFFFFFF),
                                LV_PART_MAIN);
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

/* Modifiers: keybinding labels. Shown while pressed and, dimmed, for a moment after release. */
static void upd_mods(void) {
    const vs_palette_t *p = &PAL[cur_pal];
    bool held[4] = {
        (st.mods & 0x11) != 0,  /* Ctrl */
        (st.mods & 0x22) != 0,  /* Shift */
        (st.mods & 0x44) != 0,  /* Alt */
        (st.mods & 0x88) != 0,  /* Win/GUI */
    };
    bool on[4], dim[4];
    uint32_t now = lv_tick_get();
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (s_mod_prev[i] && !held[i]) {
            s_mod_until[i] = now + VS_MOD_LINGER_MS;
            if (s_mod_until[i] == 0) s_mod_until[i] = 1;
        }
        if (held[i]) s_mod_until[i] = 0;
        s_mod_prev[i] = held[i];
        bool linger = !held[i] && s_mod_until[i] != 0 && (int32_t)(now - s_mod_until[i]) < 0;
        if (!held[i] && !linger) s_mod_until[i] = 0;
        on[i] = held[i] || linger;
        dim[i] = linger;
        if (on[i]) n++;
    }
    s_mod_n = n;
    const int cw = 58, gap = 6;
    int x = (VS_W - (n * cw + (n > 0 ? (n - 1) * gap : 0))) / 2;
    for (int i = 0; i < 4; i++) {
        if (!s_mod[i]) continue;
        if (on[i]) {
            lv_obj_set_pos(s_mod[i], x, 144);
            lv_obj_set_style_bg_color(s_mod[i], lv_color_hex(dim[i] ? p->chrome : p->sel), LV_PART_MAIN);
            lv_obj_set_style_border_color(s_mod[i], lv_color_hex(dim[i] ? p->disabled : p->accent),
                                          LV_PART_MAIN);
            lv_obj_set_style_text_color(s_mod_lbl[i], lv_color_hex(dim[i] ? p->desc : p->on_sel),
                                        LV_PART_MAIN);
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

/* Typing equalizer. Every key press kicks one pseudo-random band (and its neighbours); the
 * band is NOT derived from the key, so nothing about what was typed leaves the keyboard.
 * Bars fall back smoothly and a peak cap lingers for a moment, like a spectrum analyzer. */
static uint32_t eq_rand(void) {
    s_rng ^= k_uptime_get_32();
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static void draw_eq(void) {
    const vs_palette_t *p = &PAL[cur_pal];
    for (int i = 0; i < VS_EQ_N; i++) {
        if (!s_eq[i]) continue;
        int h = (s_lvl[i] < 2) ? 2 : s_lvl[i];
        lv_obj_set_size(s_eq[i], VS_EQ_W, h);
        lv_obj_set_pos(s_eq[i], VS_EQ_X + i * VS_EQ_PITCH, VS_EQ_BASE - h);
        lv_obj_set_style_bg_color(s_eq[i], lv_color_hex(s_lvl[i] > 2 ? p->accent : p->disabled), LV_PART_MAIN);
        if (s_cap[i]) {
            bool show = (int)s_peak[i] > (int)s_lvl[i] + 2;
            vs_show(s_cap[i], show);
            if (show) {
                lv_obj_set_pos(s_cap[i], VS_EQ_X + i * VS_EQ_PITCH, VS_EQ_BASE - s_peak[i] - 3);
            }
        }
    }
}

/* Best WPM over the last ~10 minutes (same span as the heatmap): 20 slots of 30 s, each holding the
 * highest WPM seen in that slot. The oldest slot is cleared as time moves on. */
#define VS_BEST_SLOTS   20
#define VS_BEST_SLOT_MS 30000
static uint8_t s_best_b[VS_BEST_SLOTS];
static uint32_t s_best_slot;
static bool s_best_init;

static void best_roll(void) {
    uint32_t slot = lv_tick_get() / VS_BEST_SLOT_MS;
    if (!s_best_init) {
        s_best_init = true;
        s_best_slot = slot;
        return;
    }
    uint32_t d = slot - s_best_slot;
    if (d == 0) return;
    if (d >= VS_BEST_SLOTS) {
        memset(s_best_b, 0, sizeof(s_best_b));
    } else {
        for (uint32_t k = 1; k <= d; k++) {
            s_best_b[(s_best_slot + k) % VS_BEST_SLOTS] = 0;
        }
    }
    s_best_slot = slot;
}

static void best_note(uint8_t wpm) {
    best_roll();
    uint8_t *b = &s_best_b[s_best_slot % VS_BEST_SLOTS];
    if (wpm > *b) *b = wpm;
}

static uint8_t best_get(void) {
    uint8_t m = 0;
    for (int i = 0; i < VS_BEST_SLOTS; i++) {
        if (s_best_b[i] > m) m = s_best_b[i];
    }
    return m;
}

/* 1234 -> "1,234" */
static void fmt_num(char *out, size_t n, uint32_t v) {
    if (v >= 1000000) {
        snprintf(out, n, "%u,%03u,%03u", (unsigned)(v / 1000000), (unsigned)((v / 1000) % 1000),
                 (unsigned)(v % 1000));
    } else if (v >= 1000) {
        snprintf(out, n, "%u,%03u", (unsigned)(v / 1000), (unsigned)(v % 1000));
    } else {
        snprintf(out, n, "%u", (unsigned)v);
    }
}

static void start_toast(uint8_t kind, const char *text, uint32_t ms) {
    strncpy(s_toast, text, sizeof(s_toast) - 1);
    s_toast[sizeof(s_toast) - 1] = 0;
    s_toast_kind = kind;
    s_toast_t0 = lv_tick_get();
    s_toast_ms = ms;
}

/* The spot below the layer tabs (where modifier keycaps appear). Priority: keycaps > toast > idle
 * comment line. Called every 50 ms; touches LVGL only when something changes. */
static void upd_info(void) {
    if (!s_info) return;
    const vs_palette_t *p = &PAL[cur_pal];
    uint32_t now = lv_tick_get();
    uint8_t kind = 0;
    char txt[32] = "";
    best_roll();
    uint8_t best = best_get();

    if (st.valid && s_mod_n == 0) {
        if (s_toast_kind != 0 && (now - s_toast_t0) < s_toast_ms) {
            kind = s_toast_kind;
            snprintf(txt, sizeof(txt), "%s", s_toast);
        } else if (st.total > 0 && (now % VS_IDLE_CYCLE_MS) < VS_IDLE_SHOW_MS) {
            char num[16];
            bool second = ((now / VS_IDLE_CYCLE_MS) & 1) != 0 && best > 0;
            kind = 3;
            if (second) {
                snprintf(txt, sizeof(txt), "// 10m best %u wpm", (unsigned)best);
            } else {
                fmt_num(num, sizeof(num), st.total);
                snprintf(txt, sizeof(txt), "// %s keys", num);
            }
        }
    }

    if (kind != s_info_kind) {
        s_info_kind = kind;
        if (kind == 3) {
            lv_obj_set_style_text_font(s_info, &JetBrainsMono_Regular_12, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_info, lv_color_hex(p->comment), LV_PART_MAIN);
            lv_obj_set_y(s_info, 154);
        } else if (kind != 0) {
            lv_obj_set_style_text_font(s_info, &JetBrainsMono_Bold_16, LV_PART_MAIN);
            lv_obj_set_style_text_color(s_info, lv_color_hex(p->fg), LV_PART_MAIN);
            lv_obj_set_y(s_info, 152);
        }
        vs_show(s_info, kind != 0);
        s_info_txt[0] = 0;
    }
    if (kind != 0 && strcmp(txt, s_info_txt) != 0) {
        strncpy(s_info_txt, txt, sizeof(s_info_txt) - 1);
        s_info_txt[sizeof(s_info_txt) - 1] = 0;
        lv_label_set_text(s_info, txt);
    }
}

static void wave_tick(lv_timer_t *t) {
    ARG_UNUSED(t);

    /* 1. key presses since the last tick kick bands.
     * Soft limit: when 11 or more bands are already up, a new kick is smaller (60 %) and does
     * not spill into the neighbours, so even a long burst of fast typing (WPM 120-140 is about
     * 10-12 presses/s) never pins every bar at the top. Below that limit the display is kept
     * lively: about half of the bars are lit at WPM 140. */
    int n = (s_wave_acc > 4) ? 4 : (int)s_wave_acc;
    s_wave_acc = 0;
    for (int k = 0; k < n; k++) {
        int lit = 0;
        for (int i = 0; i < VS_EQ_N; i++) {
            if (s_lvl[i] > 6) lit++;
        }
        bool crowded = (lit >= 11);
        uint32_t r = eq_rand();
        int b = (int)(r % VS_EQ_N);
        int jump = 14 + (int)((r >> 8) % 15);          /* 14..28 px */
        if (crowded) jump = (jump * 6) / 10;
        if (jump > VS_EQ_MAXH) jump = VS_EQ_MAXH;
        if (s_lvl[b] < jump) s_lvl[b] = (uint8_t)jump;
        if (!crowded) {
            /* The neighbours resonate unevenly: one side (chosen at random) rings loudly
             * (75-100 % of the kick), the other more faintly (30-55 %), and the loud side
             * carries on to the next band (40 %). */
            int loud = (jump * (75 + (int)((r >> 20) % 26))) / 100;
            int soft = (jump * (30 + (int)((r >> 26) % 26))) / 100;
            bool left_loud = ((r >> 30) & 1) != 0;
            int left = left_loud ? loud : soft;
            int right = left_loud ? soft : loud;
            int far_idx = left_loud ? b - 2 : b + 2;
            int far_lvl = (jump * 40) / 100;
            if (b > 0 && s_lvl[b - 1] < left) s_lvl[b - 1] = (uint8_t)left;
            if (b < VS_EQ_N - 1 && s_lvl[b + 1] < right) s_lvl[b + 1] = (uint8_t)right;
            if (far_idx >= 0 && far_idx < VS_EQ_N && s_lvl[far_idx] < far_lvl) {
                s_lvl[far_idx] = (uint8_t)far_lvl;
            }
        }
    }

    /* 2. peak hold, then fall */
    bool any = false;
    for (int i = 0; i < VS_EQ_N; i++) {
        if (s_lvl[i] > s_peak[i]) {
            s_peak[i] = s_lvl[i];
            s_hold[i] = 8;                       /* 8 ticks x 50 ms */
        } else if (s_hold[i] > 0) {
            s_hold[i]--;
        } else if (s_peak[i] > s_lvl[i]) {
            s_peak[i]--;
        }
        if (s_lvl[i] > 2) {
            s_lvl[i] = (uint8_t)((s_lvl[i] * 22) / 25);   /* x0.88 per 50 ms tick */
        } else {
            s_lvl[i] = 0;
        }
        if (s_lvl[i] || s_peak[i]) any = true;
    }

    if (any || s_wave_dirty) {
        draw_eq();
        s_wave_dirty = any;   /* one last redraw once everything is flat */
    }

    /* 3. released modifier keycaps expire; refresh the info line */
    uint32_t now = lv_tick_get();
    for (int i = 0; i < 4; i++) {
        if (s_mod_until[i] != 0 && (int32_t)(now - s_mod_until[i]) >= 0) {
            upd_mods();
            break;
        }
    }
    upd_info();
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
    draw_eq();
    upd_info();
}

/* ========== Create / destroy ========== */

static void clear_pointers(void) {
    s_root = NULL;
    s_sbat_lbl = NULL;
    s_info = NULL;
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
    for (int i = 0; i < VS_EQ_N; i++) {
        s_eq[i] = NULL;
        s_cap[i] = NULL;
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

    /* Typing equalizer: 16 bands that jump on key presses and fall back */
    for (int i = 0; i < VS_EQ_N; i++) {
        s_eq[i] = vs_rect(s_root, VS_EQ_X + i * VS_EQ_PITCH, VS_EQ_BASE - 2, VS_EQ_W, 2, p->disabled);
        s_cap[i] = vs_rect(s_root, VS_EQ_X + i * VS_EQ_PITCH, VS_EQ_BASE - 5, VS_EQ_W, 2, p->number);
        vs_show(s_cap[i], false);
    }
    s_wave_timer = lv_timer_create(wave_tick, 50, NULL);

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

    /* Info line in the same spot (layer toast / idle comment). Hidden at first. */
    s_info = vs_label(s_root, &JetBrainsMono_Regular_12, p->comment, "", 20, 154, 240, LV_TEXT_ALIGN_CENTER);
    vs_show(s_info, false);
    s_info_kind = 0xFF;
    s_info_txt[0] = 0;

    /* Battery: % above a full-width bar (YADS: y=-42 / -33 from the bottom) */
    s_bat_lbl = vs_label(s_root, &JetBrainsMono_Bold_16, p->accent, "--", 90, 181, 100, LV_TEXT_ALIGN_CENTER);
    s_bat_track = vs_rect(s_root, (VS_W - VS_BAR_W) / 2, 203, VS_BAR_W, 6, p->chrome);
    s_bat_fill = vs_rect(s_root, (VS_W - VS_BAR_W) / 2, 203, 1, 6, p->accent);

    /* Waiting cover (no keyboard yet / timed out) */
    s_wait_cover = vs_rect(s_root, 0, 0, VS_W, VS_BAND_Y, p->editor);
    s_wait_name = vs_label(s_wait_cover, &JetBrainsMono_Bold_16, p->desc, "Scanning...", 40, 22, 200,
                           LV_TEXT_ALIGN_CENTER);
    s_wait_l1 = vs_label(s_wait_cover, &JetBrainsMono_Bold_12, p->comment, "// waiting for keyboard...",
                         20, 100, 240, LV_TEXT_ALIGN_CENTER);
    s_wait_l2 = vs_label(s_wait_cover, &JetBrainsMono_Bold_12, p->comment, "// scanning", 20, 120, 240,
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
    memset(s_lvl, 0, sizeof(s_lvl));
    memset(s_peak, 0, sizeof(s_peak));
    memset(s_hold, 0, sizeof(s_hold));
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
        upd_layer();  // the layer name list depends on the keyboard
    }
    if (first || st.wpm != d->wpm_value) {
        st.wpm = d->wpm_value;
        best_note(st.wpm);
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
        uint8_t prev_layer = st.layer;
        st.layer = d->active_layer;
        strncpy(st.lname, d->current_layer_name, sizeof(st.lname) - 1);
        st.lname[sizeof(st.lname) - 1] = '\0';
        upd_layer();
        if (!first && st.layer != prev_layer && st.layer != 0) {
            char nb[20], tb[24];
            snprintf(tb, sizeof(tb), "-> %s", layer_label(st.layer, st.lname, nb, sizeof(nb)));
            start_toast(1, tb, VS_TOAST_LAYER_MS);
        }
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
    if (n == 0) return;
    st.total += n;
    if (s_wave_timer == NULL) return;   /* not on screen */
    s_wave_acc += n;
    if (s_wave_acc > 100) {
        s_wave_acc = 100;
    }
}
