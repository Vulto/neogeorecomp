#include <neogeorecomp/neogeorecomp.h>
#include <neogeorecomp/bus.h>

extern void func_01229E_upstream(void);

/*
 * The original BIOS/VBlank path drains $102224 asynchronously. The native
 * static runtime is single-threaded, so a literal busy wait can deadlock
 * before the VBlank dispatcher gets a chance to run. Service one pending
 * upload synchronously, then execute the original override unchanged.
 */
void func_01229E(void) {
    if (bus_read16(0x102224) != 0)
        func_table_call(0x012188);

    func_01229E_upstream();
}
