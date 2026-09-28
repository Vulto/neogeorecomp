/*
 * palette.c — Neo Geo dual-bank palette system implementation.
 *
 * Maintains two banks of palette RAM and a pre-converted ARGB lookup
 * table that's updated on every palette write for fast rendering.
 */

#include <neogeorecomp/palette.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ----- Internal State ----- */

/* Raw Neo Geo palette data (2 banks x 256 palettes x 16 colors) */
static uint16_t s_palram[NEOGEO_PALETTE_BANKS][NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL];

/* Pre-converted ARGB8888 lookup table for the active bank */
static uint32_t s_argb_table[NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL];
static uint32_t s_shadow_argb_table[NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL];

static uint8_t s_active_bank = 0;

/* ----- Color Conversion ----- */

/*
 * Neo Geo video output uses five weighted bits per RGB component.
 * The common dark bit selects an 8.2K ohm pulldown and the global
 * shadow output adds a 150 ohm pulldown. These are the same resistor
 * networks used by the hardware model in MAME.
 *
 * The four columns are:
 *   0 = normal
 *   1 = dark
 *   2 = shadow
 *   3 = dark + shadow
 */
static const uint8_t s_rgb_lookup[32][4] = {
    {  0,   0,   0,   0}, {  8,   8,   4,   4}, { 14,  14,   8,   8}, { 22,  21,  12,  12},
    { 30,  30,  17,  17}, { 38,  38,  21,  21}, { 44,  44,  25,  25}, { 52,  51,  29,  29},
    { 64,  64,  36,  36}, { 72,  71,  40,  40}, { 78,  78,  44,  44}, { 86,  85,  48,  48},
    { 94,  94,  53,  53}, {102, 102,  57,  57}, {108, 108,  61,  61}, {116, 116,  65,  65},
    {126, 126,  71,  71}, {134, 134,  75,  75}, {140, 140,  79,  79}, {148, 147,  83,  83},
    {156, 156,  88,  88}, {164, 164,  92,  92}, {170, 170,  96,  96}, {178, 177, 100, 100},
    {191, 190, 107, 107}, {199, 198, 111, 111}, {205, 205, 116, 115}, {213, 212, 120, 119},
    {221, 221, 124, 124}, {229, 229, 129, 128}, {235, 235, 133, 132}, {255, 251, 142, 141}
};

static uint32_t palette_convert(uint16_t neo_color, bool shadow) {
    uint8_t dark = (uint8_t)((neo_color >> 15) & 1u);
    uint8_t r5 = (uint8_t)(((neo_color >> 14) & 1u) | ((neo_color >> 7) & 0x1Eu));
    uint8_t g5 = (uint8_t)(((neo_color >> 13) & 1u) | ((neo_color >> 3) & 0x1Eu));
    uint8_t b5 = (uint8_t)(((neo_color >> 12) & 1u) | ((neo_color << 1) & 0x1Eu));
    uint8_t mode = (uint8_t)(dark + (shadow ? 2u : 0u));

    uint8_t r8 = s_rgb_lookup[r5][mode];
    uint8_t g8 = s_rgb_lookup[g5][mode];
    uint8_t b8 = s_rgb_lookup[b5][mode];

    return 0xFF000000u | ((uint32_t)r8 << 16) | ((uint32_t)g8 << 8) | b8;
}

uint32_t palette_neo_to_argb(uint16_t neo_color) {
    return palette_convert(neo_color, false);
}

/* Rebuild the ARGB table for the active bank */
static void rebuild_argb_table(void) {
    int total = NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL;
    for (int i = 0; i < total; i++) {
        /* Color index 0 of every palette is always transparent. */
        if ((i & 0xF) == 0) {
            s_argb_table[i] = 0x00000000;
            s_shadow_argb_table[i] = 0x00000000;
        } else {
            uint16_t neo_color = s_palram[s_active_bank][i];
            s_argb_table[i] = palette_convert(neo_color, false);
            s_shadow_argb_table[i] = palette_convert(neo_color, true);
        }
    }
}

/* ----- Initialization ----- */

int palette_init(void) {
    memset(s_palram, 0, sizeof(s_palram));
    memset(s_argb_table, 0, sizeof(s_argb_table));
    memset(s_shadow_argb_table, 0, sizeof(s_shadow_argb_table));
    s_active_bank = 0;
    return 0;
}

void palette_shutdown(void) {
    /* Nothing to free — all static */
}

/* ----- Palette RAM Access ----- */

uint16_t palette_read(uint32_t offset) {
    if (offset >= NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL) return 0;
    return s_palram[s_active_bank][offset];
}

void palette_write(uint32_t offset, uint16_t val) {
    if (offset >= NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL) return;
    s_palram[s_active_bank][offset] = val;
    /* Update the ARGB cache for this entry */
    if ((offset & 0xF) == 0) {
        s_argb_table[offset] = 0x00000000;
        s_shadow_argb_table[offset] = 0x00000000;
    } else {
        s_argb_table[offset] = palette_convert(val, false);
        s_shadow_argb_table[offset] = palette_convert(val, true);
    }
}

/* ----- Bank Switching ----- */

void palette_set_bank(uint8_t bank) {
    if (bank > 1) return;
    if (bank != s_active_bank) {
        s_active_bank = bank;
        rebuild_argb_table();
    }
}

uint8_t palette_get_bank(void) {
    return s_active_bank;
}

/* ----- Fast Access ----- */

const uint32_t *palette_get_argb_table(void) {
    return s_argb_table;
}

const uint32_t *palette_get_shadow_argb_table(void) {
    return s_shadow_argb_table;
}

uint32_t palette_get_backdrop(void) {
    /* Last color in the active bank */
    return s_argb_table[NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL - 1];
}
