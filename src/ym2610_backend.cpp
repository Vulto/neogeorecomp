#include <neogeorecomp/ym2610_backend.h>
#include <neogeorecomp/z80.h>

#include <cstdint>
#include <vector>

#include "ymfm_opn.h"

static bool s_irq_pending;

class ym2610_interface final : public ymfm::ymfm_interface {
public:
    std::vector<uint8_t> vrom;
    int32_t timer_clocks[2] = { -1, -1 };

    void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override {
        if (tnum < 2)
            timer_clocks[tnum] = duration_in_clocks;
    }

    void ymfm_update_irq(bool asserted) override {
        s_irq_pending = asserted;
        z80_set_irq(asserted);
    }

    void advance_timers(double clocks) {
        for (uint32_t tnum = 0; tnum < 2; tnum++) {
            if (timer_clocks[tnum] < 0)
                continue;

            timer_clocks[tnum] -= (int32_t)clocks;
            if (timer_clocks[tnum] <= 0) {
                timer_clocks[tnum] = -1;
                if (m_engine) {
                    m_engine->engine_timer_expired(tnum);
                    m_engine->engine_check_interrupts();
                }
            }
        }
    }

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
static int32_t s_prev[2];
static int32_t s_curr[2];
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
    s_prev[0] = s_prev[1] = 0;
    s_curr[0] = s_curr[1] = 0;
    s_primed = false;
    s_interface.timer_clocks[0] = -1;
    s_interface.timer_clocks[1] = -1;
    z80_set_irq(false);
    s_irq_pending = false;
    return s_chip_rate != 0 ? 0 : -1;
}

extern "C" void ym2610_backend_shutdown(void) {
    delete s_chip;
    s_chip = nullptr;
    z80_set_irq(false);
    s_irq_pending = false;
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
    s_prev[0] = s_prev[1] = 0;
    s_curr[0] = s_curr[1] = 0;
    s_primed = false;
    s_interface.timer_clocks[0] = -1;
    s_interface.timer_clocks[1] = -1;
    z80_set_irq(false);
    s_irq_pending = false;
}

extern "C" bool ym2610_backend_irq_pending(void) {
    return s_irq_pending;
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
        s_curr[0] = sample.data[0];
        s_curr[1] = sample.data[1];
        s_prev[0] = s_curr[0];
        s_prev[1] = s_curr[1];
        s_interface.advance_timers(8000000.0 / (double)s_chip_rate);
        s_primed = true;
    }

    const double step = (double)s_chip_rate / (double)s_sample_rate;

    for (int i = 0; i < num_samples; i++) {
        while (s_phase >= 1.0) {
            s_prev[0] = s_curr[0];
            s_prev[1] = s_curr[1];

            ymfm::ym2610::output_data sample;
            s_chip->generate(&sample);
            s_curr[0] = sample.data[0];
            s_curr[1] = sample.data[1];
            s_interface.advance_timers(8000000.0 / (double)s_chip_rate);

            s_phase -= 1.0;
        }

        const double left =
            (double)s_prev[0] + ((double)(s_curr[0] - s_prev[0]) * s_phase);
        const double right =
            (double)s_prev[1] + ((double)(s_curr[1] - s_prev[1]) * s_phase);
        buffer[i * 2 + 0] = clamp16((int32_t)left);
        buffer[i * 2 + 1] = clamp16((int32_t)right);

        s_phase += step;
    }
}
