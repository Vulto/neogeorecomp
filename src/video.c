/*
 * video.c — Neo Geo LSPC video system implementation.
 *
 * Handles VRAM management, sprite rendering, and fix layer compositing.
 *
 * The Neo Geo has 380 usable sprites per frame; sprite #0 is the active-list filler. Each sprite is a vertical
 * strip of 16x16 tiles. Wide objects are built by chaining sprites
 * horizontally via the "sticky bit" in SCB3.
 *
 * Rendering priority: Lower sprite numbers have higher priority.
 * The fix layer is always on top of all sprites.
 *
 * Notes:
 *   - SCB2 vertical/horizontal shrinking is implemented.
 *   - Auto-animation and scanline sprite limits are implemented.
 *   - GPU acceleration remains optional and outside the hardware emulation path.
 */

#include <neogeorecomp/video.h>
#include <neogeorecomp/palette.h>
#include <neogeorecomp/timer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ----- Internal State ----- */

static uint16_t s_vram[NEOGEO_VRAM_SIZE];  /* VRAM (word-addressed) */
static uint16_t s_vram_addr = 0;               /* Current VRAM address */
static int16_t  s_vram_mod = 0;                /* Auto-increment value */
static uint16_t s_lspc_mode = 0;               /* LSPC mode register */

static uint8_t *s_crom = NULL;       /* Sprite tile data (C ROMs) */
static uint32_t s_crom_size = 0;
static uint8_t *s_srom = NULL;       /* Fix layer tile data (S ROM) */
static uint32_t s_srom_size = 0;
static uint8_t *s_sfix = NULL;       /* BIOS fix tiles (SFIX ROM) */
static uint32_t s_sfix_size = 0;
static uint8_t *s_l0 = NULL;          /* Sprite vertical shrink lookup */
static uint32_t s_l0_size = 0;

static bool s_use_bios_fix = true;   /* Fix layer source selection */
static bool s_shadow = false;        /* Shadow/darken mode */
static uint8_t s_auto_anim_counter = 0;  /* Auto-animation frame counter */
static uint8_t s_auto_anim_speed = 0;
static uint8_t s_auto_anim_frame_counter = 0;
static bool s_auto_anim_disabled = false;

/* ----- Initialization ----- */

int video_init(void) {
    memset(s_vram, 0, sizeof(s_vram));
    s_vram_addr = 0;
    s_vram_mod = 1;  /* Default auto-increment */
    s_lspc_mode = 0;
    s_auto_anim_counter = 0;
    s_auto_anim_speed = 0;
    s_auto_anim_frame_counter = 0;
    s_auto_anim_disabled = false;
    return 0;
}

void video_shutdown(void) {
    free(s_crom); s_crom = NULL; s_crom_size = 0;
    free(s_srom); s_srom = NULL; s_srom_size = 0;
    free(s_sfix); s_sfix = NULL; s_sfix_size = 0;
    free(s_l0); s_l0 = NULL; s_l0_size = 0;
}

/* ----- ROM Loading ----- */

int video_load_crom(const char **crom_paths, int num_croms) {
    /*
     * C ROMs always come in pairs (odd = bitplanes 0-1, even = bitplanes 2-3).
     * We interleave them into a single buffer for efficient tile lookup.
     */
    if (num_croms < 2 || num_croms % 2 != 0) {
        fprintf(stderr, "[video] C ROMs must be in pairs (got %d)\n", num_croms);
        return -1;
    }

    /* Determine total size from first pair */
    FILE *f = fopen(crom_paths[0], "rb");
    if (!f) { fprintf(stderr, "[video] Failed to open C ROM: %s\n", crom_paths[0]); return -1; }
    fseek(f, 0, SEEK_END);
    long per_rom_size = ftell(f);
    fclose(f);

    s_crom_size = (uint32_t)(per_rom_size * num_croms);
    s_crom = (uint8_t *)malloc(s_crom_size);
    if (!s_crom) return -1;

    /* Load each C ROM pair interleaved */
    uint32_t offset = 0;
    for (int i = 0; i < num_croms; i += 2) {
        FILE *f_odd  = fopen(crom_paths[i], "rb");
        FILE *f_even = fopen(crom_paths[i + 1], "rb");
        if (!f_odd || !f_even) {
            fprintf(stderr, "[video] Failed to open C ROM pair %d/%d\n", i, i + 1);
            if (f_odd) fclose(f_odd);
            if (f_even) fclose(f_even);
            return -1;
        }

        /* Interleave byte-by-byte: odd, even, odd, even... */
        for (long j = 0; j < per_rom_size; j++) {
            s_crom[offset++] = (uint8_t)fgetc(f_odd);
            s_crom[offset++] = (uint8_t)fgetc(f_even);
        }

        fclose(f_odd);
        fclose(f_even);
    }

    printf("[video] Loaded %d C ROMs: %u bytes total\n", num_croms, s_crom_size);
    return 0;
}

int video_load_srom(const char *srom_path) {
    FILE *f = fopen(srom_path, "rb");
    if (!f) { fprintf(stderr, "[video] Failed to open S ROM: %s\n", srom_path); return -1; }
    fseek(f, 0, SEEK_END);
    s_srom_size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    s_srom = (uint8_t *)malloc(s_srom_size);
    if (!s_srom) { fclose(f); return -1; }
    if (fread(s_srom, 1, s_srom_size, f) != s_srom_size) {
        fclose(f);
        free(s_srom);
        s_srom = NULL;
        s_srom_size = 0;
        return -1;
    }
    fclose(f);
    printf("[video] Loaded S ROM: %u bytes\n", s_srom_size);
    return 0;
}

int video_load_sfix(const char *sfix_path) {
    FILE *f = fopen(sfix_path, "rb");
    if (!f) { fprintf(stderr, "[video] Failed to open SFIX ROM: %s\n", sfix_path); return -1; }
    fseek(f, 0, SEEK_END);
    s_sfix_size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    s_sfix = (uint8_t *)malloc(s_sfix_size);
    if (!s_sfix) { fclose(f); return -1; }
    if (fread(s_sfix, 1, s_sfix_size, f) != s_sfix_size) {
        fclose(f);
        free(s_sfix);
        s_sfix = NULL;
        s_sfix_size = 0;
        return -1;
    }
    fclose(f);
    printf("[video] Loaded SFIX ROM: %u bytes\n", s_sfix_size);
    return 0;
}

int video_load_l0(const char *l0_path) {
    FILE *f = fopen(l0_path, "rb");
    if (!f) {
        fprintf(stderr, "[video] Failed to open L0 ROM: %s\\n", l0_path);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long size = ftell(f);
    if (size < 0x10000) {
        fclose(f);
        fprintf(stderr, "[video] L0 ROM is too small: %ld bytes\\n", size);
        return -1;
    }

    rewind(f);
    uint8_t *rom = malloc(0x10000);
    if (!rom) {
        fclose(f);
        return -1;
    }

    size_t got = fread(rom, 1, 0x10000, f);
    fclose(f);
    if (got != 0x10000) {
        free(rom);
        return -1;
    }

    free(s_l0);
    s_l0 = rom;
    s_l0_size = 0x10000;
    printf("[video] Loaded L0 ROM: %u bytes\\n", s_l0_size);
    return 0;
}

/* ----- VRAM Access ----- */

void video_set_vram_addr(uint16_t addr) {
    s_vram_addr = addr;
}

uint16_t video_read_vram(void) {
    uint16_t val = 0;
    if (s_vram_addr < NEOGEO_VRAM_SIZE) {
        val = s_vram[s_vram_addr];
    }
    s_vram_addr = (uint16_t)(s_vram_addr + s_vram_mod);
    return val;
}

void video_write_vram(uint16_t val) {
    if (s_vram_addr < NEOGEO_VRAM_SIZE) {
        s_vram[s_vram_addr] = val;
    }
    s_vram_addr = (uint16_t)(s_vram_addr + s_vram_mod);
}

void video_set_vram_mod(uint16_t mod) {
    s_vram_mod = (int16_t)mod;
}

void video_set_lspc_mode(uint16_t mode) {
    s_lspc_mode = mode;
    s_auto_anim_speed = (uint8_t)(mode >> 8);
    s_auto_anim_disabled = (mode & 0x0008u) != 0;
}

uint16_t video_get_lspc_mode(void) {
    /* NTSC LSPC counter spans $0F8..$1FF for the 264-line frame. */
    uint16_t raster = (uint16_t)((timer_get_scanline() + 0x00F8u) & 0x01FFu);
    return (uint16_t)((raster << 7) | (s_lspc_mode & 0x007Fu));
}

/* ----- Tile Decoding Helpers ----- */

/*
 * Decode one 16x16 4bpp sprite tile from C ROM data.
 *
 * Neo Geo C ROM tile format (128 bytes per tile):
 *   - Tiles are stored as 4 bitplanes across two C ROM chips
 *   - C1 (odd) holds bitplanes 0 and 1
 *   - C2 (even) holds bitplanes 2 and 3
 *   - After interleaving: each byte pair gives 2 bitplanes for 8 pixels
 *   - Each row = 8 bytes (16 pixels x 4bpp)
 *   - 16 rows = 128 bytes total
 *
 * The C ROM data has already been interleaved at load time (video_load_crom
 * interleaves odd/even pairs byte-by-byte), so the data is:
 *   byte 0 (from C1): bitplanes 0,1 for pixels 0-7
 *   byte 1 (from C2): bitplanes 2,3 for pixels 0-7
 *   byte 2 (from C1): bitplanes 0,1 for pixels 8-15
 *   byte 3 (from C2): bitplanes 2,3 for pixels 8-15
 *   ... repeat for 16 rows = 128 bytes
 */
static const uint16_t s_zoom_x[16] = {
    0x0080, 0x0880, 0x0888, 0x2888,
    0x288A, 0x2A8A, 0x2AAA, 0xAAAA,
    0xAAEA, 0xBAEA, 0xBAEB, 0xBBEB,
    0xBBEF, 0xFBEF, 0xFBFF, 0xFFFF
};

static uint8_t sprite_pixel(uint32_t tile_num, int x, int y) {
    uint32_t offset = tile_num * 128u;
    if (!s_crom || offset + 127u >= s_crom_size)
        return 0;

    /*
     * C ROM block order is top-right, bottom-right, top-left,
     * bottom-left. Each 8-pixel row is four interleaved bytes:
     * C1 bitplanes 0/1 and C2 bitplanes 2/3.
     */
    int block = (y >= 8 ? 1 : 0) + (x < 8 ? 2 : 0);
    int row = y & 7;
    int bit = x & 7;
    const uint8_t *p = s_crom + offset + (uint32_t)block * 32u + row * 4;

    uint8_t bp0 = p[0];
    uint8_t bp2 = p[1];
    uint8_t bp1 = p[2];
    uint8_t bp3 = p[3];

    return (uint8_t)(
        (((bp0 >> bit) & 1u) << 0) |
        (((bp1 >> bit) & 1u) << 1) |
        (((bp2 >> bit) & 1u) << 2) |
        (((bp3 >> bit) & 1u) << 3));
}

static void draw_sprite_line(
    uint32_t tile_num,
    uint8_t palette_idx,
    int screen_x,
    int screen_y,
    int source_y,
    bool h_flip,
    const uint32_t *argb_palette,
    uint32_t *framebuffer,
    uint8_t h_shrink)
{
    uint16_t mask = s_zoom_x[h_shrink & 0x0F];
    int dst_x = screen_x & 0x1FF;
    int pal_base = palette_idx * 16;

    for (int x = 0; x < 16; x++) {
        int source_x = h_flip ? 15 - x : x;
        if ((mask & (uint16_t)(1u << (15 - source_x))) == 0)
            continue;

        int px = dst_x;
        dst_x = (dst_x + 1) & 0x1FF;
        if (px >= NEOGEO_SCREEN_WIDTH)
            continue;

        uint8_t pixel = sprite_pixel(tile_num, source_x, source_y);
        if (pixel == 0)
            continue;

        uint32_t color = argb_palette[pal_base + pixel];
        if ((color & 0xFF000000u) == 0)
            continue;

        framebuffer[screen_y * NEOGEO_SCREEN_WIDTH + px] = color;
    }
}

/*
 * Decode one 8x8 4bpp fix layer tile from S ROM data.
 *
 * S ROM tiles are simpler than sprite tiles: 32 bytes per tile,
 * 4 bitplanes, stored column-by-column (top to bottom, then
 * left to right within each column).
 */
static void decode_fix_tile(
    uint16_t tile_num,
    uint8_t palette_idx,
    int screen_x, int screen_y,
    const uint32_t *argb_palette,
    uint32_t *framebuffer)
{
    const uint8_t *rom = s_use_bios_fix ? s_sfix : s_srom;
    uint32_t rom_size = s_use_bios_fix ? s_sfix_size : s_srom_size;
    if (!rom || rom_size == 0) return;

    /*
     * Neo Geo S ROM fix layer tile format: 8x8, 4bpp, nibble-packed.
     * 32 bytes per tile. Columns are scrambled per MAME charlayout:
     *   xoffsets = {33*4, 32*4, 49*4, 48*4, 1*4, 0*4, 17*4, 16*4}
     *
     * Decoded: pixel (x, y) is at byte[base_byte[x] + y], hi/lo nibble:
     *   x=0: byte 16+y, hi    x=1: byte 16+y, lo
     *   x=2: byte 24+y, hi    x=3: byte 24+y, lo
     *   x=4: byte  0+y, hi    x=5: byte  0+y, lo
     *   x=6: byte  8+y, hi    x=7: byte  8+y, lo
     */
    uint32_t offset = ((uint32_t)tile_num * 32) % rom_size;
    const uint8_t *tile = rom + offset;

    int pal_base = palette_idx * 16;

    /* Lookup: for each x column, which byte group (0,8,16,24) and which nibble */
    static const int x_byte_base[8] = {16, 16, 24, 24, 0, 0, 8, 8};
    static const int x_is_lo[8]     = { 1,  0,  1,  0, 1, 0, 1, 0};

    for (int row = 0; row < 8; row++) {
        int py = screen_y + row;
        if (py < 0 || py >= NEOGEO_SCREEN_HEIGHT) continue;

        for (int col = 0; col < 8; col++) {
            int px = screen_x + col;
            if (px < 0 || px >= NEOGEO_SCREEN_WIDTH) continue;

            uint8_t byte_val = tile[x_byte_base[col] + row];
            uint8_t pixel = x_is_lo[col] ? (byte_val & 0x0F) : ((byte_val >> 4) & 0x0F);

            if (pixel == 0) continue;

            uint32_t color = argb_palette[pal_base + pixel];
            if ((color & 0xFF000000) == 0) continue;
            framebuffer[py * NEOGEO_SCREEN_WIDTH + px] = color;
        }
    }
}

/* ----- Rendering ----- */

void video_render_frame(uint32_t *framebuffer) {
    /*
     * Neo Geo rendering pipeline:
     *   1. Fill with backdrop color (last palette entry)
     *   2. Render sprites 380 -> 0 (lower index = higher priority, drawn last)
     *   3. Render fix layer on top (always visible, highest priority)
     */

    const uint32_t *argb = s_shadow ? palette_get_shadow_argb_table() : palette_get_argb_table();
    uint32_t backdrop = argb[NEOGEO_NUM_PALETTES * NEOGEO_COLORS_PER_PAL - 1];

    /* 1. Fill with backdrop */
    for (int i = 0; i < NEOGEO_SCREEN_WIDTH * NEOGEO_SCREEN_HEIGHT; i++) {
        framebuffer[i] = backdrop;
    }

    /* 2. Render sprites (back to front: high index first, low index on top)
     *
     * Sprite chaining: when the sticky bit is set in SCB3, the sprite
     * inherits the X position of the previous sprite + 16 pixels.
     * This allows building wide objects from multiple vertical strips.
     * We track chain_x across iterations for this purpose.
     */
    typedef struct {
        int x;
        int y;
        int height;
        bool special_size_33;
        uint8_t v_shrink;
        uint8_t h_shrink;
        bool valid;
    } SpriteState;

    SpriteState sprites[NEOGEO_MAX_SPRITES + 1] = {0};

    /*
     * Resolve sticky chains in sprite-number order first. Rendering is
     * performed in reverse order afterwards so lower sprite numbers have
     * higher priority.
     */
    for (int spr = 1; spr <= NEOGEO_MAX_SPRITES; spr++) {
        uint16_t scb3 = s_vram[0x8200 + spr];
        uint16_t scb4 = s_vram[0x8400 + spr];
        uint16_t scb2 = s_vram[0x8000 + spr];

        int y_raw = (scb3 >> 7) & 0x1FF;
        int x_raw = (scb4 >> 7) & 0x1FF;
        int height = scb3 & 0x3F;
        int y = (0x1F0 - y_raw) & 0x1FF;
        int x = x_raw;

        bool sticky = (scb3 & 0x40) != 0;

        sprites[spr].x = x;
        sprites[spr].y = y;
        sprites[spr].special_size_33 = height == 33;
        sprites[spr].height = height == 33 ? 32 : height;
        sprites[spr].v_shrink = (uint8_t)(scb2 & 0xFF);
        sprites[spr].h_shrink = (uint8_t)((scb2 >> 8) & 0x0F);
        sprites[spr].valid = height != 0;

        if (sticky && spr > 0 && sprites[spr - 1].valid) {
            /*
             * Sticky sprites are placed immediately after the previous
             * sprite's displayed width. Horizontal shrinking is not
             * inherited, so use the previous sprite's own SCB2 width:
             * $0 = 1 pixel ... $F = 16 pixels.
             */
            sprites[spr].x =
                (sprites[spr - 1].x + sprites[spr - 1].h_shrink + 1) & 0x1FF;
            sprites[spr].y = sprites[spr - 1].y;
            sprites[spr].height = sprites[spr - 1].height;
            sprites[spr].special_size_33 = sprites[spr - 1].special_size_33;
            sprites[spr].v_shrink = sprites[spr - 1].v_shrink;
        }
    }

    enum { SpriteBitWords = (NEOGEO_MAX_SPRITES + 64) / 64 };
    uint64_t sprite_scanline_mask[NEOGEO_SCREEN_HEIGHT][SpriteBitWords] = {{0}};
    uint16_t sprite_scanline_count[NEOGEO_SCREEN_HEIGHT] = {0};

    /*
     * The hardware evaluates sprite entries in ascending sprite-number
     * order for the per-scanline limit. Record the entries that survive
     * first, then render them in reverse order for priority.
     */
    for (int spr = 0; spr <= NEOGEO_MAX_SPRITES; spr++) {
        SpriteState *state = &sprites[spr];
        if (!state->valid || state->height <= 0)
            continue;

        for (int sprite_line = 0; sprite_line < state->height * 16 && sprite_line < 512; sprite_line++) {
            int py = (state->y + sprite_line) & 0x1FF;
            if (py >= NEOGEO_SCREEN_HEIGHT)
                continue;

            if (sprite_scanline_count[py] >= NEOGEO_MAX_SCANLINE_SPRITES)
                continue;

            unsigned word = (unsigned)spr >> 6;
            uint64_t bit = UINT64_C(1) << ((unsigned)spr & 63u);
            sprite_scanline_mask[py][word] |= bit;
            sprite_scanline_count[py]++;
        }
    }

    for (int spr = NEOGEO_MAX_SPRITES; spr >= 1; spr--) {
        SpriteState *state = &sprites[spr];
        if (!state->valid || state->height <= 0)
            continue;
        /*
         * SCB2 values of zero are valid hardware values:
         *   vertical 0 = minimum vertical height
         *   horizontal 0 = one output pixel per source row.
         * Do not discard these sprites.
         */
        uint16_t scb1_base = (uint16_t)(spr * 64);

        for (int sprite_line = 0; sprite_line < state->height * 16 && sprite_line < 512; sprite_line++) {
            int zoom_line = sprite_line & 0xFF;
            bool invert = (sprite_line & 0x100) != 0;

            if (state->special_size_33) {
                int period = ((int)state->v_shrink + 1) << 1;
                zoom_line %= period;
                if (zoom_line > state->v_shrink) {
                    zoom_line = period - 1 - zoom_line;
                    invert = !invert;
                }
            }

            if (invert)
                zoom_line ^= 0xFF;

            uint8_t l0;
            if (s_l0 && s_l0_size >= 0x10000) {
                l0 = s_l0[((uint32_t)state->v_shrink << 8) | (uint32_t)zoom_line];
            } else {
                /*
                 * L0 is a system ROM, not a cartridge ROM. Keep the renderer
                 * functional when no dump is supplied by using proportional
                 * nearest-line sampling as the fallback.
                 */
                uint32_t source_line =
                    ((uint32_t)zoom_line * 256u) /
                    ((uint32_t)state->v_shrink + 1u);
                if (source_line > 255u)
                    source_line = 255u;
                l0 = (uint8_t)source_line;
            }
            int tile_row = l0 >> 4;
            int source_y = l0 & 0x0F;

            if (invert) {
                tile_row ^= 0x1F;
                source_y ^= 0x0F;
            }

            uint16_t scb1_even = s_vram[scb1_base + tile_row * 2];
            uint16_t scb1_odd = s_vram[scb1_base + tile_row * 2 + 1];

            uint32_t tile_num = (uint32_t)scb1_even |
                                (((uint32_t)(scb1_odd & 0x00F0)) << 12);
            uint8_t palette_idx = (uint8_t)(scb1_odd >> 8);
            bool h_flip = (scb1_odd & 0x0001) != 0;
            bool v_flip = (scb1_odd & 0x0002) != 0;

            if (scb1_odd & 0x0008)
                tile_num = (tile_num & ~0x7u) | (s_auto_anim_counter & 0x7u);
            else if (scb1_odd & 0x0004)
                tile_num = (tile_num & ~0x3u) | (s_auto_anim_counter & 0x3u);

            if (v_flip)
                source_y ^= 0x0F;

            int py = (state->y + sprite_line) & 0x1FF;
            if (py >= NEOGEO_SCREEN_HEIGHT)
                continue;

            unsigned word = (unsigned)spr >> 6;
            uint64_t bit = UINT64_C(1) << ((unsigned)spr & 63u);
            if ((sprite_scanline_mask[py][word] & bit) == 0)
                continue;

            draw_sprite_line(tile_num, palette_idx, state->x, py, source_y,
                             h_flip, argb, framebuffer, state->h_shrink);
        }
    }

    /* 3. Render fix layer (always on top).
     * The 40x32 map has two hidden rows above and below the NTSC window.
     * Visible screen row 0 maps to VRAM row 2. */
    enum { FixVisibleRows = 28, FixVisibleRowOffset = 2 };
    for (int col = 0; col < NEOGEO_FIX_COLS; col++) {
        for (int row = 0; row < FixVisibleRows; row++) {
            int map_row = row + FixVisibleRowOffset;
            uint16_t fix_entry = s_vram[0x7000 + col * NEOGEO_FIX_ROWS + map_row];

            uint16_t tile_num = fix_entry & 0x0FFF;
            uint8_t palette_idx = (fix_entry >> 12) & 0x0F;

            if (tile_num == 0) continue;

            int px = col * 8;
            int py = row * 8;

            decode_fix_tile(tile_num, palette_idx, px, py, argb, framebuffer);
        }
    }

    if (!s_auto_anim_disabled) {
        if (s_auto_anim_frame_counter == 0) {
            s_auto_anim_frame_counter = s_auto_anim_speed;
            s_auto_anim_counter++;
        } else {
            s_auto_anim_frame_counter--;
        }
    }
}

/* ----- Fix Layer Control ----- */

void video_set_fix_source(bool use_bios) {
    s_use_bios_fix = use_bios;
}

/* ----- Shadow ----- */

void video_set_shadow(bool enabled) {
    s_shadow = enabled;
}

/* ----- Auto-Animation ----- */

uint8_t video_get_auto_anim_counter(void) {
    return s_auto_anim_counter;
}

/* ----- Debug ----- */

const uint16_t *video_get_vram_ptr(void) {
    return s_vram;
}
