#include "audio_output.h"

#include "audio_buffer.h"
#include "pico.h"
#include "display/pimoroni_explorer.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/regs/dreq.h"
#include "hardware/sync.h"

namespace cpcexplorer::audio {
namespace {
PcmBufferQueue queue;
volatile bool running = false;
int dma_channel = -1;
unsigned pwm_slice = 0;
Diagnostics stats{};

void start_dma(const std::uint16_t* buffer) {
    running = true;
    gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 1);
    dma_channel_set_read_addr(dma_channel, buffer, false);
    dma_channel_set_trans_count(dma_channel, PcmBufferQueue::BufferSamples, true);
}

void dma_isr() {
    const auto mask = 1U << dma_channel;
    if ((dma_hw->ints1 & mask) == 0) return;
    dma_hw->ints1 = mask;
    queue.complete();
    ++stats.played;
    if (const auto* next = queue.acquire()) {
        start_dma(next);
    } else {
        running = false;
        pwm_set_chan_level(pwm_slice, PWM_CHAN_A, 128);
        gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 0);
        ++stats.underruns;
    }
}
}  // namespace

bool init(unsigned sample_rate) {
    stats = {};
    queue.reset();
    running = false;

    gpio_init(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN);
    gpio_set_dir(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, GPIO_OUT);
    gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 0);

    gpio_set_function(PIMORONI_EXPLORER_PWM_AUDIO_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(PIMORONI_EXPLORER_PWM_AUDIO_PIN);
    auto pwm_config = pwm_get_default_config();
    pwm_config_set_wrap(&pwm_config, 255);
    pwm_config_set_clkdiv(&pwm_config,
        static_cast<float>(clock_get_hz(clk_sys)) / (sample_rate * 256.0f));
    pwm_init(pwm_slice, &pwm_config, false);
    pwm_set_chan_level(pwm_slice, PWM_CHAN_A, 128);
    pwm_set_enabled(pwm_slice, true);

    dma_channel = dma_claim_unused_channel(false);
    if (dma_channel < 0) {
        pwm_set_enabled(pwm_slice, false);
        return false;
    }
    auto dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, DREQ_PWM_WRAP0 + pwm_slice);
    dma_channel_configure(dma_channel, &dma_config,
                          &pwm_hw->slice[pwm_slice].cc, nullptr, 0, false);
    dma_channel_set_irq1_enabled(dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, dma_isr);
    irq_set_enabled(DMA_IRQ_1, true);
    return true;
}

void submit(const float* samples, std::size_t count) {
    if (samples == nullptr || dma_channel < 0) return;
    const auto irq_state = save_and_disable_interrupts();
    if (!queue.submit(samples, count)) {
        ++stats.overruns;
        restore_interrupts(irq_state);
        return;
    }
    ++stats.submitted;
    if (!running) {
        if (const auto* next = queue.acquire()) start_dma(next);
    }
    restore_interrupts(irq_state);
}

void shutdown() {
    gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 0);
    if (dma_channel >= 0) {
        dma_channel_set_irq1_enabled(dma_channel, false);
        dma_channel_abort(dma_channel);
        dma_channel_unclaim(dma_channel);
        dma_channel = -1;
    }
    running = false;
    pwm_set_chan_level(pwm_slice, PWM_CHAN_A, 128);
    pwm_set_enabled(pwm_slice, false);
}

Diagnostics diagnostics() { return stats; }
}  // namespace cpcexplorer::audio
