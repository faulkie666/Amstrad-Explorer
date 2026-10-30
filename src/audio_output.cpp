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

#include <atomic>

namespace cpcexplorer::audio {
namespace {
PcmBufferQueue queue;
volatile bool running = false;
int dma_channel = -1;
unsigned pwm_slice = 0;
AmplifierPolicy amplifier_policy = AmplifierPolicy::GateOnPlayback;
struct AtomicDiagnostics {
    std::atomic<std::uint32_t> submitted{0}, played{0}, underruns{0}, overruns{0};
    void reset() {
        submitted.store(0); played.store(0); underruns.store(0); overruns.store(0);
    }
};
AtomicDiagnostics stats{};

void start_dma(const std::uint16_t* buffer) {
    running = true;
    if (gate_amplifier_on_underrun(amplifier_policy))
        gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 1);
    dma_channel_set_read_addr(dma_channel, buffer, false);
    dma_channel_set_trans_count(dma_channel, PcmBufferQueue::BufferSamples, true);
}

void dma_isr() {
    const auto mask = 1U << dma_channel;
    if ((dma_hw->ints1 & mask) == 0) return;
    dma_hw->ints1 = mask;
    queue.complete();
    stats.played.fetch_add(1, std::memory_order_relaxed);
    if (const auto* next = queue.acquire()) {
        start_dma(next);
    } else {
        running = false;
        pwm_set_chan_level(pwm_slice, PWM_CHAN_A, 128);
        if (gate_amplifier_on_underrun(amplifier_policy))
            gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 0);
        stats.underruns.fetch_add(1, std::memory_order_relaxed);
    }
}
}  // namespace

bool init(unsigned sample_rate, AmplifierPolicy policy) {
    stats.reset();
    queue.reset();
    running = false;
    amplifier_policy = policy;

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
    if (amplifier_policy == AmplifierPolicy::HoldEnabled)
        gpio_put(PIMORONI_EXPLORER_SPEAKER_AMP_ENABLE_PIN, 1);
    return true;
}

bool submit(const float* samples, std::size_t count) {
    if (samples == nullptr || dma_channel < 0) return false;
    const auto irq_state = save_and_disable_interrupts();
    if (!queue.submit(samples, count)) {
        stats.overruns.fetch_add(1, std::memory_order_relaxed);
        restore_interrupts(irq_state);
        return false;
    }
    stats.submitted.fetch_add(1, std::memory_order_relaxed);
    if (!running) {
        if (const auto* next = queue.acquire()) start_dma(next);
    }
    restore_interrupts(irq_state);
    return true;
}

bool can_submit() {
    if (dma_channel < 0) return false;
    const auto irq_state = save_and_disable_interrupts();
    const bool available = queue.has_free();
    restore_interrupts(irq_state);
    return available;
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

Diagnostics diagnostics() {
    return {
        stats.submitted.load(std::memory_order_relaxed),
        stats.played.load(std::memory_order_relaxed),
        stats.underruns.load(std::memory_order_relaxed),
        stats.overruns.load(std::memory_order_relaxed),
    };
}
}  // namespace cpcexplorer::audio
