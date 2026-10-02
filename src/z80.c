#include <neogeorecomp/z80.h>
#include <neogeorecomp/ym2610.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define z80_init z80_core_init
#include "z80.h"
#undef z80_init

#define Z80_RAM_SIZE 0x800u

static z80 s_cpu;
static uint8_t *s_mrom;
static uint32_t s_mrom_size;
static uint8_t s_z80_ram[Z80_RAM_SIZE];
static uint8_t s_cmd_latch;
static uint8_t s_reply_latch;
static uint8_t s_bank[4];
static uint32_t s_bank_address_mask;
static bool s_nmi_enabled;
static bool s_command_pending;

static uint8_t rom_read(uint32_t offset) {
    if (!s_mrom || offset >= s_mrom_size)
        return 0xFF;
    return s_mrom[offset];
}

static uint32_t banked_rom_offset(unsigned region, uint8_t bank, uint32_t window_offset) {
    static const unsigned shifts[4] = { 11, 12, 13, 14 };
    uint32_t offset = ((uint32_t)bank << shifts[region]) & s_bank_address_mask;
    return offset + window_offset;
}

static uint8_t z80_mem_read(void *userdata, uint16_t address) {
    (void)userdata;

    if (address < 0x8000)
        return rom_read(address);

    if (address < 0xC000)
        return rom_read(banked_rom_offset(3, s_bank[3], address - 0x8000u));

    if (address < 0xE000)
        return rom_read(banked_rom_offset(2, s_bank[2], address - 0xC000u));

    if (address < 0xF000)
        return rom_read(banked_rom_offset(1, s_bank[1], address - 0xE000u));

    if (address < 0xF800)
        return rom_read(banked_rom_offset(0, s_bank[0], address - 0xF000u));

    return s_z80_ram[address - 0xF800u];
}

static void z80_mem_write(void *userdata, uint16_t address, uint8_t value) {
    (void)userdata;
    if (address >= 0xF800)
        s_z80_ram[address - 0xF800u] = value;
}

static uint8_t z80_port_in(z80 *cpu, uint8_t port) {
    (void)cpu;

    switch (port) {
    case 0x00: {
        uint8_t command = s_cmd_latch;
        s_cmd_latch = 0;
        s_command_pending = false;
        return command;
    }

    case 0x04:
        return ym2610_read(0);

    case 0x05:
        return ym2610_read(1);

    case 0x06:
        return ym2610_read(2);

    case 0x07:
        return ym2610_read(3);

    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
        /* Neo Geo banks are selected by the value placed on the Z80
         * address bus during an IN instruction. For IN A,(n), that
         * value is the accumulator. */
        s_bank[port - 0x08] = cpu->a;
        return 0xFF;

    default:
        return 0xFF;
    }
}

static void z80_port_out(z80 *cpu, uint8_t port, uint8_t value) {
    (void)cpu;

    switch (port) {
    case 0x00:
        s_cmd_latch = 0;
        break;

    case 0x04:
        ym2610_write(0, value, 0);
        break;

    case 0x05:
        ym2610_write(1, 0, value);
        break;

    case 0x06:
        ym2610_write(2, value, 0);
        break;

    case 0x07:
        ym2610_write(3, 0, value);
        break;

    case 0x0C:
        s_reply_latch = value;
        break;

    case 0x08:
        /* NMI enable/acknowledge. The YM2610 command path uses this
         * before accepting 68k sound commands. */
        s_nmi_enabled = true;
        break;
    case 0x18:
        s_nmi_enabled = false;
        break;

    default:
        break;
    }
}

static void z80_setup(void) {
    z80_core_init(&s_cpu);
    s_cpu.read_byte = z80_mem_read;
    s_cpu.write_byte = z80_mem_write;
    s_cpu.port_in = z80_port_in;
    s_cpu.port_out = z80_port_out;
    s_cpu.userdata = NULL;

    s_bank[0] = 0x1E;
    s_bank[1] = 0x0E;
    s_bank[2] = 0x06;
    s_bank[3] = 0x02;
    s_nmi_enabled = false;
}

int z80_init(void) {
    memset(s_z80_ram, 0, sizeof(s_z80_ram));
    s_cmd_latch = 0;
    s_reply_latch = 0;
    s_command_pending = false;
    s_mrom = NULL;
    s_mrom_size = 0;
    z80_setup();
    return 0;
}

void z80_shutdown(void) {
    free(s_mrom);
    s_mrom = NULL;
    s_mrom_size = 0;
    s_bank_address_mask = 0;
}

int z80_load_mrom(const char *mrom_path) {
    FILE *f = fopen(mrom_path, "rb");
    if (!f) {
        fprintf(stderr, "[z80] Failed to open M ROM: %s\n", mrom_path);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return -1;
    }

    rewind(f);
    uint8_t *rom = malloc((size_t)size);
    if (!rom) {
        fclose(f);
        return -1;
    }

    size_t got = fread(rom, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(rom);
        return -1;
    }

    free(s_mrom);
    s_mrom = rom;
    s_mrom_size = (uint32_t)size;
    if (s_mrom_size > 0x10000u)
        s_bank_address_mask = s_mrom_size - 1u;
    else
        s_bank_address_mask = 0;
    z80_setup();

    printf("[z80] Loaded M ROM: %u bytes\n", s_mrom_size);
    return 0;
}

void z80_execute(int cycles) {
    if (cycles <= 0 || !s_mrom)
        return;

    unsigned long start = s_cpu.cyc;
    unsigned long target = start + (unsigned long)cycles;

    while (s_cpu.cyc < target) {
        unsigned long before = s_cpu.cyc;
        z80_step(&s_cpu);
        unsigned long elapsed = s_cpu.cyc - before;
        if (elapsed != 0)
            ym2610_tick_timers((int)elapsed);
    }
}

void z80_send_command(uint8_t cmd) {
    s_cmd_latch = cmd;
    s_command_pending = true;
    if (s_nmi_enabled) {
        z80_gen_nmi(&s_cpu);
    }
}

uint8_t z80_read_reply(void) {
    return s_reply_latch;
}

bool z80_command_pending(void) {
    return s_command_pending;
}

void z80_set_nmi_enabled(bool enabled) {
    s_nmi_enabled = enabled;
}

void z80_set_irq(bool asserted) {
    s_cpu.int_pending = asserted;
}

void z80_reset(void) {
    memset(s_z80_ram, 0, sizeof(s_z80_ram));
    s_cmd_latch = 0;
    s_reply_latch = 0;
    s_command_pending = false;
    s_nmi_enabled = false;
    z80_setup();
}
