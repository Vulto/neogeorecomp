/*
 * platform.h — SDL3 platform abstraction layer.
 *
 * Handles windowing, input mapping, audio output, and frame timing.
 * This is the only public interface that exposes platform services;
 * the rest of the runtime remains platform-agnostic.
 */

#ifndef NEOGEORECOMP_PLATFORM_H
#define NEOGEORECOMP_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int platform_init(int window_scale, bool fullscreen, bool vsync);
void platform_shutdown(void);

void platform_present(const uint32_t *framebuffer);
void platform_toggle_fullscreen(void);

bool platform_poll_input(void);

int platform_audio_init(int sample_rate);
void platform_audio_queue(const int16_t *samples, int num_samples);

void platform_frame_sync(void);
uint64_t platform_get_ticks(void);

void platform_set_title(const char *title);

#ifdef __cplusplus
}
#endif

#endif
