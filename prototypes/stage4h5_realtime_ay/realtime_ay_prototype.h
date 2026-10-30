#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

extern "C" {
#include "chips/ay38910.h"
}

namespace cpcexplorer::stage4h5 {

// One producer (the emulation core), one consumer (the audio core). The
// producer publishes a complete slot before advancing head; the consumer does
// not release a slot until it has copied it. No lock or allocation is used.
struct PsgEvent {
    std::uint32_t target_ay_tick = 0;
    std::uint16_t sequence = 0;
    std::uint8_t reg = 0;
    std::uint8_t value = 0;
};
static_assert(sizeof(PsgEvent) == 8);

struct CompactAyState {
    std::uint32_t tick = 0;
    std::uint8_t reg[AY38910_NUM_REGISTERS]{};
    ay38910_tone_t tone[AY38910_NUM_CHANNELS]{};
    ay38910_noise_t noise{};
    ay38910_env_t env{};
    int sample_period = 0;
    int sample_counter = 0;
    float magnitude = 0.0F;
    float sample = 0.0F;
    float dc_sum = 0.0F;
    std::uint32_t dc_position = 0;
};

using AyDcBuffer = std::array<float, AY38910_DCADJ_BUFLEN>;

template <std::size_t Capacity>
class SpscPsgQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "capacity must be a power of two");
public:
    bool push(const PsgEvent& event) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1U) & mask;
        if (next == tail_.load(std::memory_order_acquire)) {
            ++overruns_;
            return false;
        }
        slots_[head] = event;
        head_.store(next, std::memory_order_release);
        const auto depth = (next - tail_.load(std::memory_order_relaxed)) & mask;
        auto high = high_water_.load(std::memory_order_relaxed);
        while (depth > high && !high_water_.compare_exchange_weak(
                   high, depth, std::memory_order_relaxed)) {}
        return true;
    }

    bool peek(PsgEvent& event) const {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        event = slots_[tail];
        return true;
    }

    bool pop(PsgEvent& event) {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        event = slots_[tail];
        tail_.store((tail + 1U) & mask, std::memory_order_release);
        return true;
    }

    std::uint32_t overruns() const { return overruns_.load(); }
    std::uint32_t high_water() const { return high_water_.load(); }
    std::uint32_t depth() const {
        return (head_.load(std::memory_order_acquire) -
                tail_.load(std::memory_order_acquire)) & mask;
    }
    void reset_while_stopped() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        overruns_.store(0, std::memory_order_relaxed);
        high_water_.store(0, std::memory_order_relaxed);
    }

private:
    static constexpr std::uint32_t mask = static_cast<std::uint32_t>(Capacity - 1);
    std::array<PsgEvent, Capacity> slots_{};
    alignas(4) std::atomic<std::uint32_t> head_{0};
    alignas(4) std::atomic<std::uint32_t> tail_{0};
    std::atomic<std::uint32_t> overruns_{0};
    std::atomic<std::uint32_t> high_water_{0};
};

// Correctness-first host prototype. Tick and span advancement use a compact
// state derived from the reviewed chips AY implementation; tests compare both
// paths sample by sample against the existing reference semantics.
class RealtimeAyPrototype {
public:
    enum class Advancement : std::uint8_t { Tick, Span };
    static constexpr std::uint32_t AyTicksPerSecond = 1'000'000;
    static constexpr std::uint32_t SampleRate = 22'050;
    static constexpr std::size_t QueueCapacity = 128;

    explicit RealtimeAyPrototype(AyDcBuffer& dc_buffer,
                                 Advancement advancement = Advancement::Span);
    bool enqueue(const PsgEvent& event) {
        return running_.load(std::memory_order_acquire) && queue_.push(event);
    }
    std::size_t render(float* output, std::size_t sample_count);
    void start(std::uint32_t epoch);
    void stop();
    void reset_while_stopped(std::uint32_t epoch);

    std::uint32_t ay_ticks() const { return ay_.tick; }
    std::uint32_t applied_events() const { return applied_events_; }
    std::uint32_t queue_overruns() const { return queue_.overruns(); }
    std::uint32_t queue_high_water() const { return queue_.high_water(); }
    std::uint32_t queue_depth() const { return queue_.depth(); }
    std::uint16_t last_sequence() const { return last_sequence_; }
    std::uint32_t sequence_errors() const { return sequence_errors_; }
    int sample_period() const { return ay_.sample_period; }
    std::uint32_t epoch() const { return epoch_.load(std::memory_order_acquire); }
    bool running() const { return running_.load(std::memory_order_acquire); }

private:
    void apply_due_events();
    std::size_t render_tick(float* output, std::size_t sample_count);
    std::size_t render_span(float* output, std::size_t sample_count);
    void initialize_state();
    void set_register(std::uint8_t reg, std::uint8_t value);
    float dc_adjust(float sample);
    CompactAyState ay_{};
    AyDcBuffer* dc_buffer_ = nullptr;
    SpscPsgQueue<QueueCapacity> queue_{};
    Advancement advancement_ = Advancement::Span;
    std::atomic<std::uint32_t> epoch_{0};
    std::atomic<bool> running_{false};
    std::uint32_t applied_events_ = 0;
    std::uint32_t sequence_errors_ = 0;
    std::uint16_t last_sequence_ = 0;
};

}  // namespace cpcexplorer::stage4h5
