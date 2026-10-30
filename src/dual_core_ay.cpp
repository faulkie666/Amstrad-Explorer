#include "dual_core_ay.h"

#include "audio_output.h"
#include "cpc_core.h"
#include "realtime_ay_prototype.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace cpcexplorer::dual_core_ay {
namespace {
using cpcexplorer::stage4h5::AyDcBuffer;
using cpcexplorer::stage4h5::PsgEvent;
using cpcexplorer::stage4h5::RealtimeAyPrototype;

constexpr std::uint32_t kBufferSamples = 128;
constexpr std::uint32_t kDeadlineUs =
    (kBufferSamples * 1'000'000U + AudioSampleRate - 1U) / AudioSampleRate;
constexpr std::uint8_t kStackGuard = 0xA5;

struct alignas(8) Control {
    explicit Control(AyDcBuffer& history) : renderer(history) {}
    RealtimeAyPrototype renderer;
    std::atomic<std::uint32_t> state{static_cast<std::uint32_t>(State::Boot)};
    std::atomic<std::uint32_t> stop_request{0};
    std::atomic<std::uint32_t> next_sequence{0};
    std::atomic<std::uint32_t> epoch{0};
    std::atomic<std::uint32_t> consumer_tick{0};
    std::uint32_t reserved[3]{};
};
static_assert(sizeof(Control) == 1'216);

struct AtomicTelemetry {
    std::atomic<std::uint32_t> producer_events{0}, consumed_events{0};
    std::atomic<std::uint32_t> queue_full{0}, sequence_errors{0}, late_events{0};
    std::atomic<std::uint32_t> queue_peak{0}, pcm_samples{0};
    std::atomic<std::uint32_t> render_max_us{0}, render_p95_upper_us{0};
    std::atomic<std::uint32_t> deadline_misses{0}, stack_high_water_bytes{0};
    void reset() {
        producer_events.store(0); consumed_events.store(0); queue_full.store(0);
        sequence_errors.store(0); late_events.store(0); queue_peak.store(0);
        pcm_samples.store(0); render_max_us.store(0); render_p95_upper_us.store(0);
        deadline_misses.store(0); stack_high_water_bytes.store(0);
    }
};

extern "C" {
__attribute__((used, section(".scratch_y.stage4h5b_dc"), aligned(4)))
AyDcBuffer ay_dc_history{};
__attribute__((used, section(".scratch_x.stage4h5b_control"), aligned(8)))
Control control{ay_dc_history};
// Size-only linker reservation. The live stack is explicitly launched at
// __StackOneBottom..__StackOneTop, rather than at this dummy input address.
__attribute__((used, section(".stack1.stage4h5b"), aligned(8)))
std::array<std::uint8_t, 2'048> core1_stack_reservation{};
}

AtomicTelemetry stats;
std::array<std::uint32_t, 16> render_histogram{};
std::atomic<bool> core_launched{false};

extern "C" std::uint8_t __StackOneBottom;
extern "C" std::uint8_t __StackOneTop;

State state() {
    return static_cast<State>(control.state.load(std::memory_order_acquire));
}

void update_max(std::atomic<std::uint32_t>& target, std::uint32_t value) {
    auto current = target.load(std::memory_order_relaxed);
    while (value > current &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

void record_stack_watermark() {
    const auto* bottom = &__StackOneBottom;
    const auto* top = &__StackOneTop;
    const auto* cursor = bottom;
    while (cursor < top && *cursor == kStackGuard) ++cursor;
    update_max(stats.stack_high_water_bytes,
               static_cast<std::uint32_t>(top - cursor));
}

void publish_percentile() {
    std::uint32_t total = 0;
    for (const auto count : render_histogram) total += count;
    if (total == 0) return;
    const std::uint32_t threshold = (total * 95U + 99U) / 100U;
    std::uint32_t cumulative = 0;
    for (std::uint32_t bucket = 0; bucket < render_histogram.size(); ++bucket) {
        cumulative += render_histogram[bucket];
        if (cumulative >= threshold) {
            stats.render_p95_upper_us.store((bucket + 1U) * 500U,
                                             std::memory_order_relaxed);
            return;
        }
    }
}

void producer_callback(std::uint64_t cycle, std::uint8_t reg,
                       std::uint8_t value, void*) {
    if (reg >= 16 || state() != State::Running) return;
    const auto target = static_cast<std::uint32_t>(cycle / 4U);
    const auto consumer = control.consumer_tick.load(std::memory_order_acquire);
    if (static_cast<std::int32_t>(target - consumer) < 0)
        stats.late_events.fetch_add(1, std::memory_order_relaxed);
    const auto sequence = static_cast<std::uint16_t>(
        control.next_sequence.fetch_add(1, std::memory_order_relaxed));
    stats.producer_events.fetch_add(1, std::memory_order_relaxed);
    if (!control.renderer.enqueue({target, sequence, reg, value}))
        stats.queue_full.fetch_add(1, std::memory_order_relaxed);
}

void core1_entry() {
    if (!flash_safe_execute_core_init()) {
        control.state.store(static_cast<std::uint32_t>(State::Failed),
                            std::memory_order_release);
        while (true) __wfe();
    }
    control.state.store(static_cast<std::uint32_t>(State::Idle),
                        std::memory_order_release);
    while (true) {
        while (state() != State::Starting) __wfe();
        control.stop_request.store(0, std::memory_order_release);
        if (!audio::init(AudioSampleRate, audio::AmplifierPolicy::HoldEnabled)) {
            control.state.store(static_cast<std::uint32_t>(State::Failed),
                                std::memory_order_release);
            continue;
        }
        control.state.store(static_cast<std::uint32_t>(State::Running),
                            std::memory_order_release);

        std::array<float, kBufferSamples> pcm{};
        std::uint32_t buffers = 0;
#if !defined(CPCEXPLORER_STAGE4H5D_AUDIO_CORRECTION)
        auto next_deadline = get_absolute_time();
#endif
        while (control.stop_request.load(std::memory_order_acquire) == 0) {
#if defined(CPCEXPLORER_STAGE4H5D_AUDIO_CORRECTION)
            // DMA/PWM is the physical sample clock. After priming both slots,
            // render immediately when the ISR frees one instead of racing it
            // with a second nominal 5.805 ms wall-clock timer.
            while (buffers >= 2 && !audio::can_submit() &&
                   control.stop_request.load(std::memory_order_acquire) == 0)
                __wfe();
            if (control.stop_request.load(std::memory_order_acquire) != 0) break;
#endif
            const auto began = time_us_64();
            control.renderer.render(pcm.data(), pcm.size());
            const auto elapsed = static_cast<std::uint32_t>(time_us_64() - began);
            update_max(stats.render_max_us, elapsed);
            ++render_histogram[std::min<std::size_t>(elapsed / 500U,
                                                     render_histogram.size() - 1U)];
            if (!audio::submit(pcm.data(), pcm.size())) {
                // The audio backend records the overrun. Keep the cadence; do
                // not spin or overwrite a DMA-owned buffer.
            }
            stats.pcm_samples.fetch_add(kBufferSamples, std::memory_order_relaxed);
            stats.consumed_events.store(control.renderer.applied_events(),
                                        std::memory_order_relaxed);
            stats.sequence_errors.store(control.renderer.sequence_errors(),
                                         std::memory_order_relaxed);
            stats.queue_peak.store(control.renderer.queue_high_water(),
                                   std::memory_order_relaxed);
            control.consumer_tick.store(control.renderer.ay_ticks(),
                                        std::memory_order_release);
            ++buffers;
            if ((buffers & 127U) == 0) {
                publish_percentile();
                record_stack_watermark();
            }
#if defined(CPCEXPLORER_STAGE4H5D_AUDIO_CORRECTION)
            if (elapsed > kDeadlineUs)
                stats.deadline_misses.fetch_add(1, std::memory_order_relaxed);
#else
            next_deadline = delayed_by_us(next_deadline, kDeadlineUs);
            if (time_reached(next_deadline)) {
                stats.deadline_misses.fetch_add(1, std::memory_order_relaxed);
            } else {
                sleep_until(next_deadline);
            }
#endif
        }
        control.state.store(static_cast<std::uint32_t>(State::Stopping),
                            std::memory_order_release);
        audio::shutdown();
        control.renderer.stop();
        control.state.store(static_cast<std::uint32_t>(State::Stopped),
                            std::memory_order_release);
    }
}

bool wait_for(State expected, std::uint32_t timeout_ms) {
    const auto until = make_timeout_time_ms(timeout_ms);
    while (state() != expected) {
        if (state() == State::Failed || time_reached(until)) return false;
        tight_loop_contents();
    }
    return true;
}
}  // namespace

bool launch_and_start(const std::uint8_t* registers, std::size_t count,
                      std::uint32_t epoch, std::uint32_t timeout_ms) {
    if (!registers || count < 16 || active()) return false;
    if (!launch_idle(timeout_ms)) return false;
    if (state() != State::Stopped && state() != State::Idle) return false;

    stats.reset();
    render_histogram.fill(0);
    control.renderer.reset_while_stopped(epoch);
    control.epoch.store(epoch, std::memory_order_release);
    control.consumer_tick.store(0, std::memory_order_relaxed);
    control.next_sequence.store(0, std::memory_order_relaxed);
    control.renderer.start(epoch);
    for (std::uint8_t reg = 0; reg < 16; ++reg) {
        const auto sequence = static_cast<std::uint16_t>(
            control.next_sequence.fetch_add(1, std::memory_order_relaxed));
        if (!control.renderer.enqueue({0, sequence, reg, registers[reg]})) return false;
    }
    set_ay_write_callback(producer_callback);
    control.state.store(static_cast<std::uint32_t>(State::Starting),
                        std::memory_order_release);
    __sev();
    return wait_for(State::Running, timeout_ms);
}

bool launch_idle(std::uint32_t timeout_ms) {
    if (!core_launched.exchange(true)) {
        std::memset(&__StackOneBottom, kStackGuard,
                    static_cast<std::size_t>(&__StackOneTop - &__StackOneBottom));
        multicore_launch_core1_with_stack(
            core1_entry, reinterpret_cast<std::uint32_t*>(&__StackOneBottom), 2'048);
        if (!wait_for(State::Idle, timeout_ms)) return false;
    }
    return state() == State::Idle || state() == State::Stopped;
}

bool stop(std::uint32_t timeout_ms) {
    if (!active()) return state() == State::Stopped || state() == State::Idle;
    set_ay_write_callback(nullptr);
    control.stop_request.store(1, std::memory_order_release);
    __sev();
    return wait_for(State::Stopped, timeout_ms);
}

Telemetry telemetry() {
    return {
        state(), stats.producer_events.load(), stats.consumed_events.load(),
        stats.queue_full.load(), stats.sequence_errors.load(), stats.late_events.load(),
        stats.queue_peak.load(), stats.pcm_samples.load(), stats.render_max_us.load(),
        stats.render_p95_upper_us.load(), stats.deadline_misses.load(),
        stats.stack_high_water_bytes.load(),
    };
}

bool active() { return state() == State::Starting || state() == State::Running; }

}  // namespace cpcexplorer::dual_core_ay
