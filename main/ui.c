/*
 * ui.c: het scherm in GitHub-stijl, met puur zwarte achtergrond.
 *
 *               ((·))         wifi-symbool
 *            studeerzone
 *      /                   \
 *     |        30%          |     streepjes lopen over 270 graden,
 *     |       ● aan         |     van linksonder naar rechtsonder
 *      \                   /
 *        1             100
 */
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include "app_config.h"
#include "lvgl.h"

/* Kleuren */
#define C_BACKGROUND  0x000000
#define C_BORDER      0x30363d
#define C_FG          0xe6edf3
#define C_FG_MUTED    0x9198a1
#define C_OFF_TEXT    0x6e7681
#define C_OFF_DOT     0x484f58
#define C_SUCCESS     0x3fb950
#define C_ATTENTION   0xd29922
#define C_DANGER      0xf85149
#define C_WIFI_ON     0x238636  /* gedempt groen */
#define C_WIFI_OFF    0x8e1519  /* donkerrood */

/* Wifi-symbool: een stip met drie boogjes eromheen */
#define WIFI_BARS     3
#define WIFI_Y        -62       /* midden van de stip, ten opzichte van het schermmidden */
#define WIFI_RADIUS_1 5         /* straal van het kleinste boogje, de volgende steeds +4 */
#define WIFI_RSSI_3   -60       /* dBm vanaf waar 3, 2 of 1 streepje oplicht */
#define WIFI_RSSI_2   -70
#define WIFI_RSSI_1   -80

/* Schaal */
#define TICK_COUNT    61
#define TICK_MAJOR    10      /* elk tiende streepje is langer */
#define ARC_START     135.0f  /* graden, 0 is rechts, met de klok mee */
#define ARC_SWEEP     270.0f
#define CENTER        120
#define TICK_OUTER    112
#define TICK_INNER    102
#define TICK_INNER_MAJOR 98

/* Verloop van donker- naar lichtgroen, met de groentinten van GitHub */
static const uint32_t GRADIENT[] = { 0x116329, 0x1a7f37, 0x2ea043, C_SUCCESS };
#define GRADIENT_STOPS 4

static lv_obj_t *s_ticks[TICK_COUNT];
static lv_point_precise_t s_points[TICK_COUNT][2];
static lv_color_t s_tick_color[TICK_COUNT];
static int s_lit_count = -1;
static lv_obj_t *s_percent, *s_dot, *s_status;
static lv_obj_t *s_wifi_dot, *s_wifi_bars[WIFI_BARS];
static int s_wifi_shown = -2;   /* -1 = geen verbinding, anders 0 t/m 3 streepjes */

static lv_color_t gradient_color(float t)
{
    float position = t * (GRADIENT_STOPS - 1);
    int index = (int)position;
    if (index >= GRADIENT_STOPS - 1) {
        index = GRADIENT_STOPS - 2;
    }
    uint8_t mix = (uint8_t)((position - index) * 255.0f);
    return lv_color_mix(lv_color_hex(GRADIENT[index + 1]), lv_color_hex(GRADIENT[index]), mix);
}

static void create_ticks(lv_obj_t *screen)
{
    for (int i = 0; i < TICK_COUNT; i++) {
        float t = (float)i / (TICK_COUNT - 1);
        float angle = (ARC_START + ARC_SWEEP * t) * (float)M_PI / 180.0f;
        int inner = (i % TICK_MAJOR == 0) ? TICK_INNER_MAJOR : TICK_INNER;
        int x1 = CENTER + lroundf(inner * cosf(angle));
        int y1 = CENTER + lroundf(inner * sinf(angle));
        int x2 = CENTER + lroundf(TICK_OUTER * cosf(angle));
        int y2 = CENTER + lroundf(TICK_OUTER * sinf(angle));

        /* Elk streepje is een klein eigen object. Daardoor hertekent LVGL alleen
         * de streepjes die echt veranderen. */
        int left = LV_MIN(x1, x2);
        int top = LV_MIN(y1, y2);
        s_points[i][0].x = x1 - left;
        s_points[i][0].y = y1 - top;
        s_points[i][1].x = x2 - left;
        s_points[i][1].y = y2 - top;

        lv_obj_t *line = lv_line_create(screen);
        lv_line_set_points(line, s_points[i], 2);
        lv_obj_set_pos(line, left, top);
        lv_obj_set_style_line_width(line, (i % TICK_MAJOR == 0) ? 2 : 1, 0);
        lv_obj_set_style_line_color(line, lv_color_hex(C_BORDER), 0);
        s_ticks[i] = line;
        s_tick_color[i] = gradient_color(t);
    }
}

static void create_wifi(lv_obj_t *screen)
{
    s_wifi_dot = lv_obj_create(screen);
    lv_obj_remove_style_all(s_wifi_dot);
    lv_obj_set_size(s_wifi_dot, 4, 4);
    lv_obj_set_style_radius(s_wifi_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_wifi_dot, LV_OPA_COVER, 0);
    lv_obj_align(s_wifi_dot, LV_ALIGN_CENTER, 0, WIFI_Y);

    /* Elk boogje is een lv_arc met hetzelfde middelpunt als de stip.
     * Alleen het achtergronddeel (LV_PART_MAIN) is zichtbaar, van 225 tot 315 graden:
     * het kwart boven het middelpunt. */
    for (int i = 0; i < WIFI_BARS; i++) {
        int radius = WIFI_RADIUS_1 + 4 * i;
        lv_obj_t *arc = lv_arc_create(screen);
        lv_obj_remove_style_all(arc);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(arc, 2 * radius, 2 * radius);
        lv_obj_align(arc, LV_ALIGN_CENTER, 0, WIFI_Y);
        lv_arc_set_bg_angles(arc, 225, 315);
        lv_obj_set_style_arc_width(arc, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
        s_wifi_bars[i] = arc;
    }
}

static lv_obj_t *create_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

void ui_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(C_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* Dunne grijze ring langs de rand */
    lv_obj_t *ring = lv_obj_create(screen);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 236, 236);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(C_BORDER), 0);

    create_ticks(screen);
    create_wifi(screen);

    lv_obj_t *room = create_label(screen, &lv_font_montserrat_14, C_FG_MUTED, UI_ROOM_LABEL);
    lv_obj_align(room, LV_ALIGN_CENTER, 0, -44);

    s_percent = create_label(screen, &lv_font_montserrat_48, C_OFF_TEXT, "uit");
    lv_obj_align(s_percent, LV_ALIGN_CENTER, 0, -2);

    /* Bolletje en status naast elkaar, samen gecentreerd */
    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 42);

    s_dot = lv_obj_create(row);
    lv_obj_remove_style_all(s_dot);
    lv_obj_set_size(s_dot, 8, 8);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    s_status = create_label(row, &lv_font_montserrat_14, C_FG_MUTED, "");

    lv_obj_t *low = create_label(screen, &lv_font_montserrat_12, C_OFF_TEXT, "1");
    lv_obj_align(low, LV_ALIGN_CENTER, -54, 92);
    lv_obj_t *high = create_label(screen, &lv_font_montserrat_12, C_OFF_TEXT, "100");
    lv_obj_align(high, LV_ALIGN_CENTER, 54, 92);

    dimmer_state_t start;
    dimmer_init(&start);
    ui_update(&start, false, NULL);
    ui_update_wifi(false, 0);
}

void ui_update_wifi(bool connected, int rssi)
{
    int bars = -1;
    if (connected) {
        bars = (rssi >= WIFI_RSSI_3) ? 3 : (rssi >= WIFI_RSSI_2) ? 2 : (rssi >= WIFI_RSSI_1) ? 1 : 0;
    }
    if (bars == s_wifi_shown) {
        return;
    }
    s_wifi_shown = bars;

    lv_color_t main = lv_color_hex(connected ? C_WIFI_ON : C_WIFI_OFF);
    lv_obj_set_style_bg_color(s_wifi_dot, main, 0);
    for (int i = 0; i < WIFI_BARS; i++) {
        /* Zonder verbinding is het hele symbool rood; anders licht een boogje op als het signaal sterk genoeg is. */
        bool lit = !connected || i < bars;
        lv_obj_set_style_arc_color(s_wifi_bars[i], lit ? main : lv_color_hex(C_BORDER), LV_PART_MAIN);
    }
}

static void update_ticks(int lit_count)
{
    if (lit_count == s_lit_count) {
        return;
    }
    for (int i = 0; i < TICK_COUNT; i++) {
        bool lit = i < lit_count;
        bool was_lit = i < s_lit_count;
        if (lit != was_lit || s_lit_count < 0) {
            lv_obj_set_style_line_color(s_ticks[i], lit ? s_tick_color[i] : lv_color_hex(C_BORDER), 0);
        }
    }
    s_lit_count = lit_count;
}

static void set_status(uint32_t dot_color, const char *text)
{
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(dot_color), 0);
    lv_label_set_text(s_status, text);
}

void ui_update(const dimmer_state_t *state, bool connected, const char *error)
{
    /* Stand 1% licht het eerste streepje op, 100% allemaal. */
    int lit = 0;
    if (connected && state->on) {
        lit = 1 + (state->brightness - DIM_MIN_PERCENT) * (TICK_COUNT - 1)
                  / (DIM_MAX_PERCENT - DIM_MIN_PERCENT);
    }
    update_ticks(lit);

    if (lit > 0) {
        char text[8];
        snprintf(text, sizeof(text), "%d%%", state->brightness);
        lv_label_set_text(s_percent, text);
        lv_obj_set_style_text_color(s_percent, lv_color_hex(C_FG), 0);
    } else {
        lv_label_set_text(s_percent, connected ? "uit" : "--");
        lv_obj_set_style_text_color(s_percent, lv_color_hex(C_OFF_TEXT), 0);
    }

    if (!connected) {
        set_status(error ? C_DANGER : C_ATTENTION, error ? error : "verbinden...");
    } else if (state->on) {
        set_status(C_SUCCESS, "aan");
    } else {
        set_status(C_OFF_DOT, "uit");
    }
}
