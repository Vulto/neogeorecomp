#include <neogeorecomp/bus.h>
#include <stdint.h>
#include <stdio.h>

extern void sub_012036_upstream(void);

/*
 * The original allocator waits for the BIOS/VBlank palette DMA to free a
 * ring slot. The native runtime has no concurrent BIOS DMA thread, so drain
 * one pending slot synchronously before entering the original allocator.
 */
void sub_012036(void) {
    uint32_t pending = bus_read32(0x101B14);
    fprintf(stderr, "[palette-override] pending=%08X next=%08X\\n", pending, bus_read32(0x101B18));
    if (pending != 0) {
        (void)bus_read16(0x101B14);
        if (bus_read32(0x101B14) != 0)
            bus_write32(0x101B14, 0);
    }

    fprintf(stderr, "[palette-override] calling upstream\\n");
    sub_012036_upstream();
    fprintf(stderr, "[palette-override] upstream returned\\n");
}
