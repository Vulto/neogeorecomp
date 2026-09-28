#include <neogeorecomp/ym2610.h>
#include <neogeorecomp/ym2610_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *s_vrom;
static uint32_t s_vrom_size;
static int s_sample_rate;

int ym2610_init(int sample_rate) {
    s_sample_rate = sample_rate;
    return ym2610_backend_init(sample_rate);
}

void ym2610_shutdown(void) {
    ym2610_backend_shutdown();
    free(s_vrom);
    s_vrom = NULL;
    s_vrom_size = 0;
}

int ym2610_load_vrom(const char **vrom_paths, int num_vroms) {
    if (!vrom_paths || num_vroms <= 0)
        return -1;

    uint32_t total = 0;
    for (int i = 0; i < num_vroms; i++) {
        FILE *f = fopen(vrom_paths[i], "rb");
        if (!f)
            return -1;
        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            return -1;
        }
        long size = ftell(f);
        fclose(f);
        if (size <= 0)
            return -1;
        total += (uint32_t)size;
    }

    uint8_t *data = malloc(total);
    if (!data)
        return -1;

    uint32_t offset = 0;
    for (int i = 0; i < num_vroms; i++) {
        FILE *f = fopen(vrom_paths[i], "rb");
        if (!f) {
            free(data);
            return -1;
        }
        if (fseek(f, 0, SEEK_END) != 0) {
            fclose(f);
            free(data);
            return -1;
        }
        long size = ftell(f);
        rewind(f);
        size_t got = fread(data + offset, 1, (size_t)size, f);
        fclose(f);
        if (got != (size_t)size) {
            free(data);
            return -1;
        }
        offset += (uint32_t)size;
    }

    free(s_vrom);
    s_vrom = data;
    s_vrom_size = total;

    if (ym2610_backend_load_vrom(s_vrom, s_vrom_size) != 0)
        return -1;

    printf("[ym2610] Loaded %d V ROMs: %u bytes total at %d Hz\n",
           num_vroms, s_vrom_size, s_sample_rate);
    return 0;
}

void ym2610_write(uint8_t port, uint8_t addr, uint8_t data) {
    if ((port & 1) == 0)
        ym2610_backend_write(port, addr);
    else
        ym2610_backend_write(port, data);
}

uint8_t ym2610_read(uint8_t port) {
    return ym2610_backend_read(port);
}

void ym2610_generate(int16_t *buffer, int num_samples) {
    ym2610_backend_generate(buffer, num_samples);
}

void ym2610_tick_timers(int cycles) {
    (void)cycles;
}

int ym2610_irq_pending(void) {
    return ym2610_backend_irq_pending() ? 1 : 0;
}

void ym2610_reset(void) {
    ym2610_backend_reset();
}
