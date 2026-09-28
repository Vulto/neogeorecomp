#include <neogeorecomp/func_table.h>
#include <neogeorecomp/neogeorecomp.h>
#include <neogeorecomp/bus.h>
#include <neogeorecomp/platform.h>

#include <stdint.h>

/*
 * Neo Drift Out keeps its own frame loop inside USER. The original BIOS
 * supplies VBlank asynchronously; in the native runtime there is no BIOS
 * interrupt source, so service one complete frame whenever the game reaches
 * its VBlank wait flag.
 */
void func_000B34(void) {
    static const uint32_t sub_state_table[20] = {
        0x000BFA, 0x000CC6, 0x000D34, 0x000D82,
        0x000D9A, 0x000DC8, 0x000DF6, 0x000E20,
        0x000E38, 0x000E6E, 0x000E9C, 0x000EC8,
        0x000EE0, 0x000EFC, 0x000F28, 0x000F3E,
        0x000F64, 0x000F7C, 0x000F98, 0x000FB0,
    };

    func_table_call(0x010F96);
    func_table_call(0x014BA8);
    func_table_call(0x001004);
    func_table_call(0x000FC8);
    func_table_call(0x0014E8);
    func_table_call(0x0013B4);
    func_table_call(0x00148A);
    func_table_call(0x001340);
    func_table_call(0x0013E6);
    func_table_call(0x0014A6);

    for (;;) {
        if (bus_read16(0x100424) == 0) {
            func_table_call(0x0010B8);
            if (!neogeo_frame_yield())
                return;
            continue;
        }

        func_table_call(0x001076);

        uint16_t sub_state = bus_read16(0x100426);
        if (sub_state < 20)
            func_table_call(sub_state_table[sub_state]);

        func_table_call(0x0013E6);
        func_table_call(0x0014A6);

        bus_write16(0x100424, 0);

        if (!platform_poll_input())
            return;
    }
}
