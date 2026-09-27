/*
 * platform.c — SDL3 platform layer implementation.
 *
 * Handles window creation, input polling, audio output, and frame timing.
 * This is the only file in the runtime that depends on SDL3.
 */

#include <neogeorecomp/platform.h>
#include <neogeorecomp/video.h>
#include <neogeorecomp/io.h>

#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static SDL_AudioStream *s_audio_stream = NULL;

static bool s_fullscreen = false;
static uint64_t s_frame_start = 0;

#define FRAME_TIME_US 16896

int platform_init(int window_scale, bool fullscreen, bool vsync) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "[platform] SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    int width = NEOGEO_SCREEN_WIDTH * window_scale;
    int height = NEOGEO_SCREEN_HEIGHT * window_scale;

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

    s_window = SDL_CreateWindow("neogeorecomp", width, height, flags);
    if (!s_window) {
        fprintf(stderr, "[platform] Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    s_renderer = SDL_CreateRenderer(s_window, NULL);
    if (!s_renderer) {
        fprintf(stderr, "[platform] Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        SDL_Quit();
        return -1;
    }

    if (!SDL_SetRenderLogicalPresentation(
            s_renderer,
            NEOGEO_SCREEN_WIDTH,
            NEOGEO_SCREEN_HEIGHT,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
        fprintf(stderr, "[platform] Logical presentation setup failed: %s\n",
                SDL_GetError());
        platform_shutdown();
        return -1;
    }

    if (vsync && !SDL_SetRenderVSync(s_renderer, 1)) {
        fprintf(stderr, "[platform] VSync setup failed: %s\n", SDL_GetError());
        platform_shutdown();
        return -1;
    }

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        NEOGEO_SCREEN_WIDTH,
        NEOGEO_SCREEN_HEIGHT
    );
    if (!s_texture) {
        fprintf(stderr, "[platform] Texture creation failed: %s\n", SDL_GetError());
        platform_shutdown();
        return -1;
    }

    s_fullscreen = fullscreen;
    s_frame_start = SDL_GetPerformanceCounter();

    printf("[platform] Window: %dx%d (scale %d), VSync: %s\n",
           width, height, window_scale, vsync ? "on" : "off");
    return 0;
}

void platform_shutdown(void) {
    if (s_audio_stream) SDL_DestroyAudioStream(s_audio_stream);
    if (s_texture) SDL_DestroyTexture(s_texture);
    if (s_renderer) SDL_DestroyRenderer(s_renderer);
    if (s_window) SDL_DestroyWindow(s_window);

    s_audio_stream = NULL;
    s_texture = NULL;
    s_renderer = NULL;
    s_window = NULL;

    SDL_Quit();
}

void platform_present(const uint32_t *framebuffer) {
    SDL_UpdateTexture(
        s_texture,
        NULL,
        framebuffer,
        NEOGEO_SCREEN_WIDTH * (int)sizeof(uint32_t)
    );
    SDL_RenderClear(s_renderer);
    SDL_RenderTexture(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

void platform_toggle_fullscreen(void) {
    s_fullscreen = !s_fullscreen;
    SDL_SetWindowFullscreen(s_window, s_fullscreen);
}

bool platform_poll_input(void) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                return false;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                bool pressed = event.key.down;

                switch (event.key.scancode) {
                    case SDL_SCANCODE_UP:    io_set_button(0, IO_BTN_UP, pressed); break;
                    case SDL_SCANCODE_DOWN:  io_set_button(0, IO_BTN_DOWN, pressed); break;
                    case SDL_SCANCODE_LEFT:  io_set_button(0, IO_BTN_LEFT, pressed); break;
                    case SDL_SCANCODE_RIGHT: io_set_button(0, IO_BTN_RIGHT, pressed); break;
                    case SDL_SCANCODE_Z:     io_set_button(0, IO_BTN_A, pressed); break;
                    case SDL_SCANCODE_X:     io_set_button(0, IO_BTN_B, pressed); break;
                    case SDL_SCANCODE_C:     io_set_button(0, IO_BTN_C, pressed); break;
                    case SDL_SCANCODE_V:     io_set_button(0, IO_BTN_D, pressed); break;

                    case SDL_SCANCODE_5:
                        if (pressed) io_insert_coin(0);
                        break;
                    case SDL_SCANCODE_1:
                        io_set_button(0, IO_BTN_START << 4, pressed);
                        break;
                    case SDL_SCANCODE_3:
                        io_set_button(0, IO_BTN_SELECT << 4, pressed);
                        break;

                    case SDL_SCANCODE_F11:
                        if (pressed) platform_toggle_fullscreen();
                        break;
                    case SDL_SCANCODE_ESCAPE:
                        if (pressed) return false;
                        break;

                    default:
                        break;
                }
                break;
            }

            default:
                break;
        }
    }

    return true;
}

int platform_audio_init(int sample_rate) {
    SDL_AudioSpec desired = {
        .format = SDL_AUDIO_S16,
        .channels = 2,
        .freq = sample_rate
    };

    s_audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &desired,
        NULL,
        NULL
    );

    if (!s_audio_stream) {
        fprintf(stderr, "[platform] Audio init failed: %s\n", SDL_GetError());
        return -1;
    }

    if (!SDL_ResumeAudioStreamDevice(s_audio_stream)) {
        fprintf(stderr, "[platform] Audio start failed: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(s_audio_stream);
        s_audio_stream = NULL;
        return -1;
    }

    printf("[platform] Audio: %d Hz, stereo\n", sample_rate);
    return 0;
}

void platform_audio_queue(const int16_t *samples, int num_samples) {
    if (s_audio_stream) {
        SDL_PutAudioStreamData(
            s_audio_stream,
            samples,
            num_samples * 2 * (int)sizeof(int16_t)
        );
    }
}

void platform_frame_sync(void) {
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t target = s_frame_start + (freq * FRAME_TIME_US / 1000000);
    uint64_t now;

    do {
        now = SDL_GetPerformanceCounter();
    } while (now < target);

    s_frame_start = now;
}

uint64_t platform_get_ticks(void) {
    return SDL_GetTicks();
}

void platform_set_title(const char *title) {
    if (s_window) SDL_SetWindowTitle(s_window, title);
}
