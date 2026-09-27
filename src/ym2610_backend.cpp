#include <neogeorecomp/ym2610_backend.h>

#include <cstdint>
#include <vector>

#include "ymfm_opn.h"

class ym2610_interface final : public ymfm::ymfm_interface {
public:
    std::vector<uint8_t> vrom;

    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override {
        if (type != ymfm::ACCESS_ADPCM_A && type != ymfm::ACCESS_ADPCM_B)
            return 0;
        if (address >= vrom.size())
            return 0;
        return vrom[address];
    }
};

static ym2610_interface s_interface;
static ymfm::ym2610 *s_chip;
static int s_sample_rate = 48000;
static uint32_t s_chip_rate;
static double s_phase;
static int32_t s_prev;
static int32_t s_curr;
static bool s_primed;

static int16_t clamp16(int32_t value) {
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return (int16_t)value;
}

extern "C" int ym2610_backend_init(int sample_rate) {
    delete s_chip;
    s_chip = new ymfm::ym2610(s_interface);
    s_chip->set_fidelity(ymfm::OPN_FIDELITY_MED);
    s_chip->reset();

    s_sample_rate = sample_rate > 0 ? sample_rate : 48000;
    s_chip_rate = s_chip->sample_rate(8000000);
    s_phase = 0.0;
    s_prev = 0;
    s_curr = 0;
    s_primed = false;
    return s_chip_rate != 0 ? 0 : -1;
}

extern "C" void ym2610_backend_shutdown(void) {
    delete s_chip;
    s_chip = nullptr;
    s_interface.vrom.clear();
    s_interface.vrom.shrink_to_fit();
}

extern "C" int ym2610_backend_load_vrom(const uint8_t *data, uint32_t size) {
    if (!data || size == 0)
        return -1;

    s_interface.vrom.assign(data, data + size);
    return 0;
}

extern "C" void ym2610_backend_write(uint8_t port, uint8_t data) {
    if (s_chip)
        s_chip->write(port & 3, data);
}

extern "C" uint8_t ym2610_backend_read(uint8_t port) {
    return s_chip ? s_chip->read(port & 3) : 0xFF;
}

extern "C" void ym2610_backend_reset(void) {
    if (!s_chip)
        return;

    s_chip->reset();
    s_phase = 0.0;
    s_prev = 0;
    s_curr = 0;
    s_primed = false;
}

extern "C" void ym2610_backend_generate(int16_t *buffer, int num_samples) {
    if (!buffer || num_samples <= 0)
        return;

    if (!s_chip) {
        std::fill(buffer, buffer + num_samples * 2, 0);
        return;
    }

    if (!s_primed) {
        ymfm::ym2610::output_data sample;
        s_chip->generate(&sample);
        s_curr = (sample.data[0] + sample.data[1]) / 2;
        s_prev = s_curr;
        s_primed = true;
    }

    const double step = (double)s_chip_rate / (double)s_sample_rate;

    for (int i = 0; i < num_samples; i++) {
        while (s_phase >= 1.0) {
            s_prev = s_curr;

            ymfm::ym2610::output_data sample;
            s_chip->generate(&sample);
            s_curr = (sample.data[0] + sample.data[1]) / 2;

            s_phase -= 1.0;
        }

        const double value = (double)s_prev + ((double)(s_curr - s_prev) * s_phase);
        const int16_t pcm = clamp16((int32_t)value);
        buffer[i * 2 + 0] = pcm;
        buffer[i * 2 + 1] = pcm;

        s_phase += step;
    }
}
