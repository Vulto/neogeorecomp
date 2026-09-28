#include <neogeorecomp/neogeorecomp.h>

static void copy_words_to_a5(unsigned count) {
    for (unsigned i = 0; i < count; i++) {
        uint16_t value = (uint16_t)g_m68k.d[0];
        bus_write16(g_m68k.a[5], value);
        g_m68k.a[5] += 2;
    }
}

/*
 * These entry points are targets of computed jumps in the original 68k
 * sprite-data builders. The recompiler emits the containing unrolled
 * blocks as separate C functions, so the interior entry points need
 * explicit equivalents.
 */
static void jt_013202(void) { copy_words_to_a5(10); g_m68k.d[5] = g_m68k.a[5]; }
static void jt_013206(void) { copy_words_to_a5(9);  g_m68k.d[5] = g_m68k.a[5]; }
static void jt_01320E(void) { copy_words_to_a5(7);  g_m68k.d[5] = g_m68k.a[5]; }
static void jt_01321E(void) { copy_words_to_a5(3);  g_m68k.d[5] = g_m68k.a[5]; }
static void jt_013222(void) { copy_words_to_a5(2);  g_m68k.d[5] = g_m68k.a[5]; }
static void jt_013226(void) { copy_words_to_a5(1);  g_m68k.d[5] = g_m68k.a[5]; }

static void jt_01329E(void) { copy_words_to_a5(9);  g_m68k.d[6] = g_m68k.a[5]; }
static void jt_0132A4(void) { copy_words_to_a5(6);  g_m68k.d[6] = g_m68k.a[5]; }
static void jt_0132AC(void) { copy_words_to_a5(2);  g_m68k.d[6] = g_m68k.a[5]; }
static void jt_0132AE(void) { copy_words_to_a5(1);  g_m68k.d[6] = g_m68k.a[5]; }
static void jt_0132B0(void) { g_m68k.d[6] = g_m68k.a[5]; }

void neodriftout_register_missing_dispatch_targets(void) {
    func_table_register(0x013202, jt_013202);
    func_table_register(0x013206, jt_013206);
    func_table_register(0x01320E, jt_01320E);
    func_table_register(0x01321E, jt_01321E);
    func_table_register(0x013222, jt_013222);
    func_table_register(0x013226, jt_013226);
    func_table_register(0x01329E, jt_01329E);
    func_table_register(0x0132A4, jt_0132A4);
    func_table_register(0x0132AC, jt_0132AC);
    func_table_register(0x0132AE, jt_0132AE);
    func_table_register(0x0132B0, jt_0132B0);
}
