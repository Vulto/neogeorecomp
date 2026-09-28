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

uint32_t palette_neo_to_argb(uint16_t neo_color) {
    uint8_t r5 = (uint8_t)(((neo_color >> 14) & 0x01u) | ((neo_color >> 7) & 0x1Eu));
    uint8_t g5 = (uint8_t)(((neo_color >> 13) & 0x01u) | ((neo_color >> 3) & 0x1Eu));
    uint8_t b5 = (uint8_t)(((neo_color >> 12) & 0x01u) | ((neo_color << 1) & 0x1Eu));

    /* Bit 15 selects the hardware dark/reference encoding; the normal
     * RGB conversion uses the five component bits below. */
    uint8_t r8 = (uint8_t)((r5 << 3) | (r5 >> 2));
    uint8_t g8 = (uint8_t)((g5 << 3) | (g5 >> 2));
    uint8_t b8 = (uint8_t)((b5 << 3) | (b5 >> 2));


    return 0xFF000000u | ((uint32_t)r8 << 16) | ((uint32_t)g8 << 8) | b8;
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
            uint32_t color = palette_neo_to_argb(s_palram[s_active_bank][i]);
            s_argb_table[i] = color;
            s_shadow_argb_table[i] =
                (color & 0xFF000000u) |
                ((color & 0x00FEFEFEu) >> 1);
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
        uint32_t color = palette_neo_to_argb(val);
        s_argb_table[offset] = color;
        s_shadow_argb_table[offset] =
            (color & 0xFF000000u) |
            ((color & 0x00FEFEFEu) >> 1);
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
