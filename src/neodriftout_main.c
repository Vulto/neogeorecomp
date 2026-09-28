/*
 * Copyright (c) 2026 sp00nznet
 * Licensed under the MIT License.
 *
 * Neo Drift Out: New Technology — Static Recompilation Entry Point
 *
 * ROM files expected in --rom-path directory (supports both MAME and
 * alternate naming conventions):
 *   drift_p1.rom OR 213-p1.p1    68000 program code (2 MB)
 *   drift_s1.rom OR 213-s1.s1    Fix layer tiles (128 KB)
 *   drift_c1.rom OR 213-c1.c1    Sprite tiles (4 MB)
 *   drift_c2.rom OR 213-c2.c2    Sprite tiles (4 MB)
 *   drift_m1.rom OR 213-m1.m1    Z80 audio program (128 KB)
 *   drift_v1.rom OR 213-v1.v1    ADPCM samples (2 MB)
 *   drift_v2.rom OR 213-v2.v2    ADPCM samples (2 MB)
 */

#include <neogeorecomp/neogeorecomp.h>
#include <neogeorecomp/io.h>
#include <stdio.h>
#include <string.h>

/* Auto-generated: 6,475 functions + recomp_register_all() */
#include "autorecomp/recomp_funcs.h"

/* Jump-table target at the end of sub_007EDA. It is a valid code
 * continuation in the original 68k program, not a standalone generated
 * function, so the function dispatcher needs an explicit terminal entry. */
static void func_007EE8(void) {
}

/* ----- ROM Path Helpers ----- */

static char s_rom_path[512] = ".";
static bool s_self_test = false;

static void make_path(char *buf, size_t size, const char *filename) {
    snprintf(buf, size, "%s/%s", s_rom_path, filename);
}

/* Try primary filename, fall back to alternate */
static int try_open(const char *primary, const char *alt, char *out, size_t out_size) {
    char path[512];
    make_path(path, sizeof(path), primary);
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); snprintf(out, out_size, "%s", path); return 0; }
    if (alt) {
        make_path(path, sizeof(path), alt);
        f = fopen(path, "rb");
        if (f) { fclose(f); snprintf(out, out_size, "%s", path); return 0; }
    }
    fprintf(stderr, "[neodriftout] ROM not found: %s", primary);
    if (alt) fprintf(stderr, " or %s", alt);
    fprintf(stderr, "\n");
    return -1;
}

/* ----- BIOS Stub Functions ----- */

/*
 * The Neo Geo BIOS handles startup, eyecatcher (SNK logo), and
 * various system services. Since we're running without a BIOS ROM,
 * we stub out the BIOS calls the game makes.
 */

/* $C00438 — BIOS VBlank handler (when game isn't active) */
static void bios_vblank_default(void) {
    /* Nothing to do — the game hasn't taken over VBlank yet */
}

/* $C00444 — BIOS: return from game to system (eyecatcher, title) */
static void bios_return_to_system(void) {
    /*
     * SYSTEM_RETURN hands control back to the MVS BIOS. The native runtime
     * has no BIOS ROM, so emulate only the state transitions that this game
     * actually observes:
     *   State 0 -> title state
     *   State 1 -> title/credit wait, then demo
     *   State 9 -> title state after game over
     * A real Start input while in title enters player mode (state 3).
     *
     * The Neo Geo development documentation describes roughly five seconds
     * for a title presentation before the attract/demo sequence continues.
     */
    enum { TITLE_TIMEOUT_FRAMES = 300 };
    static unsigned title_frames;

    uint8_t state = bus_read8(0x10FDAE);

    if (state == 0 || state == 9) {
        bus_bios_write8(0x10FDAE, 1);
        title_frames = 0;
        bus_write8(0x10FD80, bus_read8(0x10FD80) | 0x80);
        return;
    }

    if (state == 1) {
        title_frames++;

        if (bus_read16(0x10FE80) != 0) {
            bus_bios_write8(0x10FDAE, 3);
            bus_write16(0x10FE80, 0);
            title_frames = 0;
            return;
        }

        if (title_frames >= TITLE_TIMEOUT_FRAMES) {
            bus_bios_write8(0x10FDAE, 2);
            title_frames = 0;
        }

        bus_write8(0x10FD80, bus_read8(0x10FD80) | 0x80);
        return;
    }

    if (state == 2) {
        if (bus_read16(0x10FE80) != 0) {
            bus_bios_write8(0x10FDAE, 3);
            bus_write16(0x10FE80, 0);
        }
        return;
    }

    bus_bios_write8(0x10FDAE, 1);
    title_frames = 0;
}

/* $C0044A — BIOS VBlank processing (called from game's VBlank handler) */
static void bios_vblank_process(void) {
    /*
     * The real BIOS reads controller inputs into system RAM here. The PC
     * port performs that part directly, but does not synthesize a Start
     * press or advance the game state on a timer.
     * We handle input through the platform layer + io subsystem,
     * so this is mostly a no-op. Just ensure the input bytes
     * at the BIOS locations are updated. */

    /* Read raw input from I/O registers into BIOS RAM locations.
     *
     * The Neo Geo BIOS processes hardware input registers each VBlank
     * and stores processed results in system RAM for the game to read.
     *
     * Key BIOS RAM locations for input:
     *   $10FD94: P1 raw joystick (active low)
     *   $10FD95: P1 edge-detected (newly pressed, active high)
     *   $10FD96: P1 raw (alternate location used by some games)
     *   $10FD97: P2 raw
     *   $10FD98: P1 start/select status
     *   $10FD99: P2 start/select status
     *   $10FD8A-$10FD8F: soft-DIP and player config
     */
    uint8_t p1 = io_read_p1cnt();
    uint8_t p2 = io_read_p2cnt();
    uint8_t status = io_read_status_b();

    /* Store raw input */
    bus_write8(0x10FD94, p1);
    bus_write8(0x10FD96, p1);
    bus_write8(0x10FD97, p2);

    /* Edge detection: active-high bits for newly pressed buttons */
    static uint8_t prev_p1 = 0xFF, prev_p2 = 0xFF;
    uint8_t p1_edge = ~p1 & prev_p1;
    uint8_t p2_edge = ~p2 & prev_p2;
    bus_write8(0x10FD95, p1_edge);
    bus_write8(0x10FD8E, p1_edge);
    bus_write8(0x10FD8F, p2_edge);
    prev_p1 = p1;
    prev_p2 = p2;

    /* STATUS_B bit 1 = P1 Start, active low. */
    static uint8_t prev_status = 0xFF;
    uint8_t p1_start = ((~status) >> 1) & 1;
    uint8_t p1_start_edge = p1_start & ((prev_status >> 1) & 1);
    uint8_t credits = io_get_credits();

    bus_write8(0x10FD8A, p1_start);
    bus_write8(0x10FD8C, credits);
    bus_write8(0x10FD98, status);

    /*
     * MVS flow: a Start edge consumes one credit. AES does not require
     * a credit. Only then does the BIOS expose the game-start request.
     */
    if (p1_start_edge && credits != 0) {
        if (io_consume_credit()) {
            io_queue_start();
            bus_write16(0x10FE80, 1);
            uint16_t sub = bus_read16(0x100426);
            if (bus_read8(0x10FDAE) == 2 && sub == 15)
                bus_write16(0x1011AE, 1);
        }
    }

    prev_status = status;
    io_clear_coin_inputs();

}

/* $C004C2 — BIOS: clear fix layer */
static void bios_clear_fix(void) {
    /* Clear the fix layer tilemap in VRAM ($7000-$74FF) */
    bus_write16(0x3C0004, 0x0001);  /* VRAM modulo = 1 */
    bus_write16(0x3C0000, 0x7000);  /* VRAM address = fix layer start */
    for (int i = 0; i < 0x500; i++) {
        bus_write16(0x3C0002, 0x0020);  /* Space tile (empty) */
    }
}

/* $C004C8 — BIOS: process system requests */
static void bios_process_requests(void) {
    /* Handles coin counting, timer updates, etc.
     * For now, just a no-op. */
}

/* $C00450 — BIOS: hardware test / controller detection */
static void bios_hw_test(void) {
    /* Write test result to $10FDB0 */
    bus_write32(0x10FDB0, 0);  /* No special hardware detected */
}

void neodriftout_register_missing_dispatch_targets(void);

static void register_bios_stubs(void) {
    func_table_register(0xC00438, bios_vblank_default);
    func_table_register(0xC00444, bios_return_to_system);
    func_table_register(0xC0044A, bios_vblank_process);
    func_table_register(0xC004C2, bios_clear_fix);
    func_table_register(0xC004C8, bios_process_requests);
    func_table_register(0xC00450, bios_hw_test);
    printf("[neodriftout] Registered 6 BIOS stubs\n");
}

/* ----- ROM Loading ----- */

static int load_roms(void) {
    char path[512];
    int rc;

    /* P ROM — 68k program code (try both naming conventions) */
    if (try_open("drift_p1.rom", "213-p1.p1", path, sizeof(path)) != 0) return -1;
    rc = bus_load_prom(path, NULL);
    if (rc != 0) return rc;

    /* S ROM — fix layer tiles */
    if (try_open("drift_s1.rom", "213-s1.s1", path, sizeof(path)) != 0) return -1;
    rc = video_load_srom(path);
    if (rc != 0) return rc;

    /* C ROMs — sprite tiles (1 pair) */
    char c1[512], c2[512];
    if (try_open("drift_c1.rom", "213-c1.c1", c1, sizeof(c1)) != 0) return -1;
    if (try_open("drift_c2.rom", "213-c2.c2", c2, sizeof(c2)) != 0) return -1;
    const char *crom_paths[] = { c1, c2 };
    rc = video_load_crom(crom_paths, 2);
    if (rc != 0) return rc;

    /* M ROM — Z80 audio program */
    if (try_open("drift_m1.rom", "213-m1.m1", path, sizeof(path)) != 0) return -1;
    rc = z80_load_mrom(path);
    if (rc != 0) return rc;

    /* V ROMs — ADPCM audio samples */
    char v1[512], v2[512];
    if (try_open("drift_v1.rom", "213-v1.v1", v1, sizeof(v1)) != 0) return -1;
    if (try_open("drift_v2.rom", "213-v2.v2", v2, sizeof(v2)) != 0) return -1;
    const char *vrom_paths[] = { v1, v2 };
    rc = ym2610_load_vrom(vrom_paths, 2);
    if (rc != 0) return rc;

    return 0;
}

/* ----- Entry Point ----- */

int main(int argc, char *argv[]) {
    printf("===========================================\n");
    printf("  Neo Drift Out: New Technology\n");
    printf("  Static Recompilation by sp00nznet\n");
    printf("  Runtime: neogeorecomp v%s\n", neogeo_version_string());
    printf("===========================================\n\n");

    /* Parse command line */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom-path") == 0 && i + 1 < argc) {
            strncpy(s_rom_path, argv[++i], sizeof(s_rom_path) - 1);
            s_rom_path[sizeof(s_rom_path) - 1] = '\0';
        } else if (strcmp(argv[i], "--self-test") == 0) {
            s_self_test = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s --rom-path ROM_DIR\\n", argv[0]);
            printf("       %s --self-test\\n", argv[0]);
            return 0;
        } else {
            fprintf(stderr, "[neodriftout] unknown option: %s\\n", argv[i]);
            return 2;
        }
    }

    /* Initialize the Neo Geo runtime */
    int rc = neogeo_init(&(neogeo_config_t){
        .rom_path = s_self_test ? NULL : s_rom_path,
        .window_scale = s_self_test ? 1 : 3,
        .fullscreen = false,
        .vsync = s_self_test ? false : true,
        .mvs_mode = true,
        .region = 0,  /* Japan (region 0 — matches default BIOS config) */
    });
    if (rc != 0) {
        fprintf(stderr, "[neodriftout] Runtime initialization failed\n");
        return 1;
    }

    /* The self-test exercises the native executable without game ROMs. */
    if (!s_self_test) {
        rc = load_roms();
        if (rc != 0) {
            fprintf(stderr, "[neodriftout] ROM loading failed\n");
            neogeo_shutdown();
            return 1;
        }
    }

    /* Register BIOS stubs first, then auto-generated game functions */
    register_bios_stubs();
    neodriftout_register_missing_dispatch_targets();
    recomp_register_all();

    /*
     * Override specific auto-generated functions with hand-written versions.
     * The auto-generator can mis-split functions when jump table targets
     * fall inside function bodies (e.g., dbhi search loops). The hand-written
     * versions in the recomp C sources are verified correct.
     */
    extern void func_007AC4(void);  /* Sprite palette search */
    extern void func_007D98(void);  /* Palette search */
    extern void func_000C52(void);  /* Sub-state 0 standard mode path */
    extern void func_000CC4(void);  /* RTS stub at $CC4 */
    extern void func_000CBC(void);  /* Sub-state advance to 1 */
    func_table_register(0x007AC4, func_007AC4);
    func_table_register(0x007D98, func_007D98);
    func_table_register(0x000C52, func_000C52);
    func_table_register(0x000CC4, func_000CC4);
    func_table_register(0x000CBC, func_000CBC);

    extern void func_01229E(void);  /* Sprite/VRAM commit (push-return-addr fix) */
    extern void func_011C88(void);  /* Partial VRAM DMA copy (16 words) */
    extern void func_011C78(void);  /* Partial VRAM DMA copy (24 words) */
    extern void func_011C98(void);  /* Partial VRAM DMA copy (8 words) */
    extern void func_000CC6(void);  /* Sub-state 1 handler (split fix) */
    extern void func_000EFC(void);  /* Car-select transition continuation */
    extern void func_000BFA(void);
    extern void func_000D34(void);
    extern void func_000D82(void);
    extern void func_000D9A(void);
    extern void func_000DC8(void);
    extern void func_000DF6(void);
    extern void func_000E20(void);
    extern void func_000E38(void);
    extern void func_000E6E(void);
    extern void func_000E9C(void);
    extern void func_000EC8(void);
    extern void func_000EE0(void);
    extern void func_000F28(void);
    extern void func_000F3E(void);
    extern void func_000F64(void);
    extern void func_000F7C(void);
    extern void func_000F98(void);
    extern void func_000FB0(void);
    func_table_register(0x01229E, func_01229E);
    extern void func_012202(void);  /* Sprite allocator (with logging) */
    extern void func_01229E(void);  /* Sprite upload synchronization wrapper */
    func_table_register(0x012202, func_012202);
    func_table_register(0x011C78, func_011C78);
    func_table_register(0x011C88, func_011C88);
    func_table_register(0x011C98, func_011C98);
    func_table_register(0x000CC6, func_000CC6);
    func_table_register(0x000EFC, func_000EFC);
    func_table_register(0x000BFA, func_000BFA);
    func_table_register(0x000D34, func_000D34);
    func_table_register(0x000D82, func_000D82);
    func_table_register(0x000D9A, func_000D9A);
    func_table_register(0x000DC8, func_000DC8);
    func_table_register(0x000DF6, func_000DF6);
    func_table_register(0x000E20, func_000E20);
    func_table_register(0x000E38, func_000E38);
    func_table_register(0x000E6E, func_000E6E);
    func_table_register(0x000E9C, func_000E9C);
    func_table_register(0x000EC8, func_000EC8);
    func_table_register(0x000EE0, func_000EE0);
    func_table_register(0x000F28, func_000F28);
    func_table_register(0x000F3E, func_000F3E);
    func_table_register(0x000F64, func_000F64);
    func_table_register(0x000F7C, func_000F7C);
    func_table_register(0x000F98, func_000F98);
    func_table_register(0x000FB0, func_000FB0);
    func_table_register(0x007EE8, func_007EE8);

    printf("[neodriftout] Registered %u total functions (with hand-written overrides)\n",
           func_table_count());

    if (s_self_test) {
        uint16_t test16 = 0xA55A;
        uint32_t test32 = 0x1234CDEF;

        if (func_table_count() < 6000 ||
            func_table_lookup(0x00068C) == NULL ||
            func_table_lookup(0x00022C) == NULL ||
            func_table_lookup(0x007EE8) == NULL) {
            fprintf(stderr, "[neodriftout] self-test: function table validation failed\n");
            neogeo_shutdown();
            return 1;
        }

        bus_write16(0x100100, test16);
        bus_write32(0x100104, test32);
        if (bus_read16(0x100100) != test16 || bus_read32(0x100104) != test32) {
            fprintf(stderr, "[neodriftout] self-test: bus round-trip failed\n");
            neogeo_shutdown();
            return 1;
        }

        palette_write(0, 0x7FFF);
        if (palette_read(0) != 0x7FFF) {
            fprintf(stderr, "[neodriftout] self-test: palette round-trip failed\n");
            neogeo_shutdown();
            return 1;
        }
        if (palette_neo_to_argb(0x8000) != 0xFF000000u) {
            fprintf(stderr, "[neodriftout] self-test: palette black conversion failed\n");
            neogeo_shutdown();
            return 1;
        }
        palette_write(1, 0x4F00);
        {
            const uint32_t *normal = palette_get_argb_table();
            const uint32_t *shadow = palette_get_shadow_argb_table();
            if (normal[1] == 0xFF000000u || shadow[1] >= normal[1]) {
                fprintf(stderr, "[neodriftout] self-test: palette shadow conversion failed\n");
                neogeo_shutdown();
                return 1;
            }
        }

        g_m68k.d[0] = 0x80000000u;
        g_m68k.flag_x = true;
        g_m68k.flag_c = false;
        M68K_ROXL32(g_m68k.d[0], 1);
        if (g_m68k.d[0] != 0x00000001u ||
            !g_m68k.flag_x || !g_m68k.flag_c) {
            fprintf(stderr, "[neodriftout] self-test: ROXL32 failed\n");
            neogeo_shutdown();
            return 1;
        }

        g_m68k.d[0] = 0x00000001u;
        g_m68k.flag_x = true;
        g_m68k.flag_c = false;
        M68K_ROXR32(g_m68k.d[0], 1);
        if (g_m68k.d[0] != 0x80000000u ||
            !g_m68k.flag_x || !g_m68k.flag_c) {
            fprintf(stderr, "[neodriftout] self-test: ROXR32 failed\n");
            neogeo_shutdown();
            return 1;
        }

        g_m68k.d[0] = 0x00000000u;
        g_m68k.d[1] = 0x00000001u;
        g_m68k.flag_x = false;
        g_m68k.flag_z = true;
        M68K_SBCD8(g_m68k.d[0], g_m68k.d[1]);
        if ((uint8_t)g_m68k.d[0] != 0x99u ||
            !g_m68k.flag_x || !g_m68k.flag_c) {
            fprintf(stderr, "[neodriftout] self-test: SBCD8 failed\n");
            neogeo_shutdown();
            return 1;
        }

        g_m68k.d[0] = 0x00000000u;
        g_m68k.flag_x = false;
        g_m68k.flag_z = true;
        M68K_NBCD8(g_m68k.d[0]);
        if ((uint8_t)g_m68k.d[0] != 0x00u ||
            g_m68k.flag_x || g_m68k.flag_c) {
            fprintf(stderr, "[neodriftout] self-test: NBCD8 zero failed\n");
            neogeo_shutdown();
            return 1;
        }

        g_m68k.d[0] = 0x00000001u;
        g_m68k.flag_x = false;
        g_m68k.flag_z = true;
        M68K_NBCD8(g_m68k.d[0]);
        if ((uint8_t)g_m68k.d[0] != 0x99u ||
            !g_m68k.flag_x || !g_m68k.flag_c) {
            fprintf(stderr, "[neodriftout] self-test: NBCD8 failed\n");
            neogeo_shutdown();
            return 1;
        }

        bus_write8(0x100200, 0x12);
        bus_write8(0x100202, 0x34);
        bus_write8(0x100204, 0x56);
        bus_write8(0x100206, 0x78);
        g_m68k.d[0] = 0;
        M68K_MOVEP32_MEM_TO_REG(g_m68k.d[0], 0x100200);
        if (g_m68k.d[0] != 0x12345678u) {
            fprintf(stderr, "[neodriftout] self-test: MOVEP32 read failed\n");
            neogeo_shutdown();
            return 1;
        }

        g_m68k.d[0] = 0xA1B2C3D4u;
        M68K_MOVEP32_REG_TO_MEM(g_m68k.d[0], 0x100200);
        if (bus_read8(0x100200) != 0xA1 ||
            bus_read8(0x100202) != 0xB2 ||
            bus_read8(0x100204) != 0xC3 ||
            bus_read8(0x100206) != 0xD4) {
            fprintf(stderr, "[neodriftout] self-test: MOVEP32 write failed\n");
            neogeo_shutdown();
            return 1;
        }

        bus_write8(0x100300, 0x80);
        g_m68k.flag_x = true;
        M68K_BFTST_MEMORY(0x100300, 0, 4);
        if (!g_m68k.flag_n || g_m68k.flag_z ||
            g_m68k.flag_v || g_m68k.flag_c || !g_m68k.flag_x) {
            fprintf(stderr, "[neodriftout] self-test: BFTST failed\n");
            neogeo_shutdown();
            return 1;
        }

        g_m68k.d[0] = 0x00000099u;
        g_m68k.d[1] = 0x00000001u;
        g_m68k.flag_x = false;
        g_m68k.flag_z = true;
        M68K_ABCD8(g_m68k.d[0], g_m68k.d[1]);
        if ((uint8_t)g_m68k.d[0] != 0x00u ||
            !g_m68k.flag_x || !g_m68k.flag_c) {
            fprintf(stderr, "[neodriftout] self-test: ABCD8 failed\n");
            neogeo_shutdown();
            return 1;
        }

        bus_write16(0x100440, 0x0001);
        g_m68k.flag_x = false;
        {
            uint16_t _tmp = bus_read16(0x100440);
            M68K_ROXR16(_tmp, 1);
            bus_write16(0x100440, _tmp);
        }
        if (bus_read16(0x100440) != 0x0000u || !g_m68k.flag_c ||
            !g_m68k.flag_x) {
            fprintf(stderr, "[neodriftout] self-test: ROXR16 memory failed\n");
            neogeo_shutdown();
            return 1;
        }

        bus_write8(0x100410, 0x12);
        bus_write8(0x100420, 0x12);
        g_m68k.a[0] = 0x100410;
        g_m68k.a[1] = 0x100420;
        {
            uint8_t _src = bus_read8(g_m68k.a[0]);
            uint8_t _dst = bus_read8(g_m68k.a[1]);
            g_m68k.a[0] += 1;
            g_m68k.a[1] += 1;
            M68K_CMP8(_dst, _src);
        }
        if (!g_m68k.flag_z || g_m68k.a[0] != 0x100411 ||
            g_m68k.a[1] != 0x100421) {
            fprintf(stderr, "[neodriftout] self-test: CMPM8 failed\n");
            neogeo_shutdown();
            return 1;
        }

        bus_write8(0x100430, 0x12);
        bus_write8(0x100432, 0x13);
        g_m68k.a[7] = 0x100430;
        {
            uint8_t _src = bus_read8(g_m68k.a[7]);
            uint8_t _dst = bus_read8(0x100432);
            g_m68k.a[7] += 2;
            M68K_CMP8(_dst, _src);
        }
        if (g_m68k.flag_n || g_m68k.flag_z || g_m68k.flag_c ||
            g_m68k.a[7] != 0x100432) {
            fprintf(stderr, "[neodriftout] self-test: CMPM8 A7 increment failed\n");
            neogeo_shutdown();
            return 1;
        }

        io_queue_start();
        bus_write16(0x10041A, 0);
        if (bus_read16(0x10041A) != 1) {
            fprintf(stderr, "[neodriftout] self-test: queued Start event was lost\n");
            neogeo_shutdown();
            return 1;
        }
        bus_write16(0x10041A, 0);
        if (bus_read16(0x10041A) != 0) {
            fprintf(stderr, "[neodriftout] self-test: queued Start event was not consumed\n");
            neogeo_shutdown();
            return 1;
        }

        neogeo_shutdown();
        printf("Neo Drift Out native runtime self-test passed.\n");
        return 0;
    }

    /* Start execution */
    platform_set_title("Neo Drift Out: New Technology [neogeorecomp]");
    neogeo_run();

    return 0;
}
