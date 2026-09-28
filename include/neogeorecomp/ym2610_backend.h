#ifndef NEOGEORECOMP_YM2610_BACKEND_H
#define NEOGEORECOMP_YM2610_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ym2610_backend_init(int sample_rate);
void ym2610_backend_shutdown(void);
int ym2610_backend_load_vrom(const uint8_t *data, uint32_t size);
void ym2610_backend_write(uint8_t port, uint8_t data);
uint8_t ym2610_backend_read(uint8_t port);
void ym2610_backend_generate(int16_t *buffer, int num_samples);
void ym2610_backend_reset(void);
bool ym2610_backend_irq_pending(void);

#ifdef __cplusplus
}
#endif

#endif
