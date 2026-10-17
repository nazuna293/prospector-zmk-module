/*
 * Heatmap layout for Scanner Mode - see heatmap_layout.h
 *
 * Key geometry per keyboard (chosen by the start of the keyboard name, see GEOMS):
 * entries in key position order, units of 1/100 key (x, y, w, h).
 * If a physical layout changes, regenerate its table from the keyboard's layouts file.
 */
#include "heatmap_layout.h"
#include "fonts.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(heatmap_layout, CONFIG_ZMK_LOG_LEVEL);

#define HM_W        280
#define HM_H        240
#define HM_MAX_KEYS 48      /* largest table below (heat levels exist for PROSPECTOR_HEAT_KEYS positions) */
#define HM_X0       20      /* left edge of the captions */
#define HM_AREA_Y   62      /* key area: 240 x 120 px from (20, 62), keys scaled to fit */
#define HM_AREA_W   240
#define HM_AREA_H   120

/* Enigma_01_Pro: layout_b of Enigma_01_Pro_layouts.dtsi (41 keys) */
static const uint16_t KEYS_ENIGMA[][4] = {
    {100,0,100,100}, {200,0,100,100},
    {0,100,100,100}, {100,100,100,100}, {200,100,100,100}, {300,100,100,100}, {400,100,100,100},
    {500,100,100,100}, {600,100,100,100}, {700,100,100,100}, {800,100,100,100}, {900,100,100,100},
    {0,200,100,100}, {100,200,100,100}, {200,200,100,100}, {300,200,100,100}, {400,200,100,100},
    {500,200,100,100}, {600,200,100,100}, {700,200,100,100}, {800,200,100,100}, {900,200,100,100},
    {0,300,100,100}, {100,300,100,100}, {200,300,100,100}, {300,300,100,100}, {400,300,100,100},
    {500,300,100,100}, {600,300,100,100}, {700,300,100,100}, {800,300,100,100}, {900,300,100,100},
    {0,400,100,100}, {100,400,100,100}, {200,400,100,100}, {300,400,150,100}, {450,400,100,100},
    {550,400,150,100}, {700,400,100,100}, {800,400,100,100}, {900,400,100,100},
};

/* NaZZ: layout_default of NaZZ-layouts.dtsi (43 keys; drawn without rotation) */
static const uint16_t KEYS_NAZZ_DEFAULT[][4] = {
    {0,0,100,100}, {100,0,100,100}, {200,0,100,100}, {300,0,100,100}, {400,0,100,100}, {900,0,100,100}, {1000,0,100,100}, {1100,0,100,100}, {1200,0,100,100}, {1300,0,100,100},
    {0,100,100,100}, {100,100,100,100}, {200,100,100,100}, {300,100,100,100}, {400,100,100,100}, {500,125,100,100}, {800,125,100,100}, {900,100,100,100}, {1000,100,100,100}, {1100,100,100,100}, {1200,100,100,100}, {1300,100,100,100},
    {0,200,100,100}, {100,200,100,100}, {200,200,100,100}, {300,200,100,100}, {400,200,100,100}, {500,225,100,100}, {800,225,100,100}, {900,200,100,100}, {1000,200,100,100}, {1100,200,100,100}, {1200,200,100,100}, {1300,200,100,100},
    {0,300,100,100}, {100,300,100,100}, {200,300,100,100}, {320,320,100,100}, {435,330,100,100}, {545,360,100,100}, {755,360,100,100}, {865,330,100,100}, {1300,300,100,100},
};
/* NaZZ Lefty: layout_lefty of NaZZ-layouts.dtsi (43 keys; drawn without rotation) */
static const uint16_t KEYS_NAZZ_LEFTY[][4] = {
    {0,0,100,100}, {100,0,100,100}, {200,0,100,100}, {300,0,100,100}, {400,0,100,100}, {900,0,100,100}, {1000,0,100,100}, {1100,0,100,100}, {1200,0,100,100}, {1300,0,100,100},
    {0,100,100,100}, {100,100,100,100}, {200,100,100,100}, {300,100,100,100}, {400,100,100,100}, {500,125,100,100}, {800,125,100,100}, {900,100,100,100}, {1000,100,100,100}, {1100,100,100,100}, {1200,100,100,100}, {1300,100,100,100},
    {0,200,100,100}, {100,200,100,100}, {200,200,100,100}, {300,200,100,100}, {400,200,100,100}, {500,225,100,100}, {800,225,100,100}, {900,200,100,100}, {1000,200,100,100}, {1100,200,100,100}, {1200,200,100,100}, {1300,200,100,100},
    {0,300,100,100}, {435,330,100,100}, {545,360,100,100}, {755,360,100,100}, {865,330,100,100}, {980,320,100,100}, {1100,300,100,100}, {1200,300,100,100}, {1300,300,100,100},
};
/* NaZZ Twin: layout_twin of NaZZ-layouts.dtsi (40 keys; drawn without rotation) */
static const uint16_t KEYS_NAZZ_TWIN[][4] = {
    {0,0,100,100}, {100,0,100,100}, {200,0,100,100}, {300,0,100,100}, {400,0,100,100}, {900,0,100,100}, {1000,0,100,100}, {1100,0,100,100}, {1200,0,100,100}, {1300,0,100,100},
    {0,100,100,100}, {100,100,100,100}, {200,100,100,100}, {300,100,100,100}, {400,100,100,100}, {500,125,100,100}, {800,125,100,100}, {900,100,100,100}, {1000,100,100,100}, {1100,100,100,100}, {1200,100,100,100}, {1300,100,100,100},
    {0,200,100,100}, {100,200,100,100}, {200,200,100,100}, {300,200,100,100}, {400,200,100,100}, {500,225,100,100}, {800,225,100,100}, {900,200,100,100}, {1000,200,100,100}, {1100,200,100,100}, {1200,200,100,100}, {1300,200,100,100},
    {0,300,100,100}, {435,330,100,100}, {545,360,100,100}, {755,360,100,100}, {865,330,100,100}, {1300,300,100,100},
};

#define HM_N(t) ((int)(sizeof(t) / sizeof(t[0])))

typedef struct {
    const char *prefix;          /* start of the keyboard name ("" = any keyboard) */
    const uint16_t (*keys)[4];
    int n;
} hm_geom_t;

/* NaZZ's Bluetooth name is "NaZZ" (right trackball), "NaZZ Lefty" or "NaZZ Twin".
 * Matched by the start of the name in this order, so the longer names come first;
 * the last entry is the default */
static const hm_geom_t GEOMS[] = {
    {"NaZZ Lefty", KEYS_NAZZ_LEFTY, HM_N(KEYS_NAZZ_LEFTY)},
    {"NaZZ Twin", KEYS_NAZZ_TWIN, HM_N(KEYS_NAZZ_TWIN)},
    {"NaZZ", KEYS_NAZZ_DEFAULT, HM_N(KEYS_NAZZ_DEFAULT)},
    {"", KEYS_ENIGMA, HM_N(KEYS_ENIGMA)},
};

static const hm_geom_t *geom_for(const char *name) {
    for (int i = 0; i < HM_N(GEOMS); i++) {
        size_t len = strlen(GEOMS[i].prefix);
        if (len == 0 || strncmp(name, GEOMS[i].prefix, len) == 0) {
            return &GEOMS[i];
        }
    }
    return &GEOMS[HM_N(GEOMS) - 1];
}

typedef struct {
    uint32_t bg;
    uint32_t cell[4];     /* level 0 (never / rarely) .. 3 (hottest) */
    uint32_t text;
    uint32_t comment;
    const char *name;
} hm_palette_t;

static const hm_palette_t PAL[2] = {
    {   /* Dark Modern */
        .bg = 0x1F1F1F,
        /* one hue: none -> dark red -> mid red -> red (VS Code editorError.foreground) */
        .cell = {0x2B2B2B, 0x823A3A, 0xBF4444, 0xF14C4C},
        .text = 0x9D9D9D, .comment = 0x6A9955,
        .name = "Dark Modern",
    },
    {   /* COBOL Green: black screen, green captions, red heat */
        .bg = 0x000000,
        /* the heat stays red; only the screen and the captions are green */
        .cell = {0x1F1F1F, 0x823A3A, 0xBF4444, 0xF14C4C},
        .text = 0x339F33, .comment = 0x3CBC3C,
        .name = "COBOL Green",
    },
};

static uint8_t cur_pal = 0;

static lv_obj_t *s_parent;
static lv_obj_t *s_root;
static const hm_geom_t *s_geom = &GEOMS[HM_N(GEOMS) - 1];  /* key geometry of the shown keyboard */
static lv_obj_t *s_key[HM_MAX_KEYS];
static lv_obj_t *s_note;
static lv_obj_t *s_map;   /* keys + legend; shown only while a keyboard sends heatmap data */
static uint8_t s_shown[HM_MAX_KEYS];         /* level currently drawn per key */
static uint8_t s_heat[PROSPECTOR_HEAT_KEYS]; /* latest levels */
static bool s_heat_valid;

static lv_obj_t *hm_rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *hm_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text,
                          int x, int y, int w, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
    lv_obj_set_width(l, w);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    return l;
}

static void draw_levels(bool force) {
    const hm_palette_t *p = &PAL[cur_pal];
    for (int i = 0; i < s_geom->n && i < HM_MAX_KEYS; i++) {
        if (!s_key[i]) continue;
        uint8_t lv = (s_heat_valid && i < PROSPECTOR_HEAT_KEYS) ? (s_heat[i] & 0x3) : 0;
        if (!force && lv == s_shown[i]) continue;
        s_shown[i] = lv;
        lv_obj_set_style_bg_color(s_key[i], lv_color_hex(p->cell[lv]), LV_PART_MAIN);
    }
    if (s_map) {
        if (s_heat_valid) {
            lv_obj_clear_flag(s_map, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_map, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_note) {
        if (s_heat_valid) {
            lv_obj_add_flag(s_note, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_note, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void build(lv_obj_t *parent) {
    const hm_palette_t *p = &PAL[cur_pal];

    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, HM_W, HM_H);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(p->bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    hm_label(s_root, &JetBrainsMono_Bold_16, p->text, "typing heatmap", HM_X0, 14, 240, LV_TEXT_ALIGN_LEFT);
    hm_label(s_root, &JetBrainsMono_Regular_10, p->text, "recent ~10 min, busiest key = reddest", HM_X0, 36, 240,
             LV_TEXT_ALIGN_LEFT);

    s_map = lv_obj_create(s_root);
    lv_obj_set_size(s_map, HM_W, HM_H);
    lv_obj_set_pos(s_map, 0, 0);
    lv_obj_set_style_bg_opa(s_map, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_map, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_map, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_map, LV_OBJ_FLAG_SCROLLABLE);

    /* Scale the keys to fit the key area (Enigma: 10 x 5 keys -> 24 px, NaZZ: 14 x 4.6 -> 17 px) */
    int cols = 0, rows = 0;
    for (int i = 0; i < s_geom->n; i++) {
        int r = s_geom->keys[i][0] + s_geom->keys[i][2];
        int b = s_geom->keys[i][1] + s_geom->keys[i][3];
        if (r > cols) cols = r;
        if (b > rows) rows = b;
    }
    int unit = (cols > 0 && rows > 0) ? MIN(HM_AREA_W * 100 / cols, HM_AREA_H * 100 / rows) : 24;
    int x0 = (HM_W - (cols * unit) / 100) / 2;
    int y0 = HM_AREA_Y + (HM_AREA_H - (rows * unit) / 100) / 2;

    for (int i = 0; i < s_geom->n && i < HM_MAX_KEYS; i++) {
        int x = x0 + (s_geom->keys[i][0] * unit) / 100;
        int y = y0 + (s_geom->keys[i][1] * unit) / 100;
        int w = (s_geom->keys[i][2] * unit) / 100 - 2;
        int h = (s_geom->keys[i][3] * unit) / 100 - 2;
        s_key[i] = hm_rect(s_map, x, y, w, h, p->cell[0], 3);
        s_shown[i] = 0;
    }

    /* legend */
    hm_label(s_map, &JetBrainsMono_Regular_10, p->text, "less", HM_X0 + 56, 200, 30, LV_TEXT_ALIGN_LEFT);
    for (int k = 0; k < 4; k++) {
        hm_rect(s_map, HM_X0 + 92 + k * 18, 200, 14, 12, p->cell[k], 2);
    }
    hm_label(s_map, &JetBrainsMono_Regular_10, p->text, "more", HM_X0 + 168, 200, 30, LV_TEXT_ALIGN_LEFT);

    s_note = hm_label(s_root, &JetBrainsMono_Bold_12, p->comment, "// waiting for keyboard...", HM_X0, 120,
                      240, LV_TEXT_ALIGN_LEFT);
}

lv_obj_t *heatmap_layout_create(lv_obj_t *parent) {
    if (s_root != NULL) {
        return s_root;
    }
    s_parent = parent;
    build(parent);
    draw_levels(true);
    LOG_INF("Heatmap layout created (%s)", PAL[cur_pal].name);
    return s_root;
}

static void teardown(void) {
    if (s_root != NULL) {
        lv_obj_del(s_root);
    }
    s_root = NULL;
    s_note = NULL;
    s_map = NULL;
    for (int i = 0; i < HM_MAX_KEYS; i++) {
        s_key[i] = NULL;
    }
}

void heatmap_layout_destroy(void) {
    teardown();
    s_parent = NULL;
}

void heatmap_layout_cycle_palette(void) {
    if (s_root == NULL || s_parent == NULL) return;
    cur_pal = (cur_pal + 1) % 2;
    teardown();
    build(s_parent);
    draw_levels(true);
}

void heatmap_layout_update(const struct prospector_keyboard_data *d) {
    if (d == NULL) return;
    const hm_geom_t *g = geom_for(d->keyboard_name);
    if (g != s_geom) {
        s_geom = g;
        if (s_root != NULL && s_parent != NULL) {  /* another keyboard: redraw its key layout */
            teardown();
            build(s_parent);
            draw_levels(true);
        }
    }
    s_heat_valid = d->heat_valid;
    if (d->heat_valid) {
        memcpy(s_heat, d->heat, sizeof(s_heat));
    }
    if (s_root != NULL) {
        draw_levels(false);
    }
}
