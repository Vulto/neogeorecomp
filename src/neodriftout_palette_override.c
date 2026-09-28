#include <neogeorecomp/bus.h>
#include <neogeorecomp/m68k.h>
#include <neogeorecomp/func_table.h>

extern void sub_012036_upstream(void);

/*
 * The original allocator waits until the VBlank palette DMA has consumed
 * all queued palette entries. In the native single-threaded runtime, service
 * that DMA synchronously instead of clearing the ring state by hand.
 */
void sub_012036(void) {
    if (bus_read32(0x101B18) != 0)
        func_table_call(0x011F3E);

    sub_012036_upstream();
}
