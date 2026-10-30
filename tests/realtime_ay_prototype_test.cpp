#include "realtime_ay_prototype.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
void require_check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
    std::abort();
}
}

#ifdef assert
#undef assert
#endif
#define assert(condition) require_check(static_cast<bool>(condition), #condition, __LINE__)

namespace {
using cpcexplorer::stage4h5::PsgEvent;
using cpcexplorer::stage4h5::RealtimeAyPrototype;

std::vector<PsgEvent> three_notes() {
    std::vector<PsgEvent> result;
    std::uint16_t sequence = 0;
    const auto write = [&](std::uint32_t tick, std::uint8_t reg, std::uint8_t value) {
        result.push_back({tick, sequence++, reg, value});
    };
    write(0, 7, 0x3E);       // tone A on; noise and B/C off
    write(0, 8, 15);         // channel A maximum fixed volume
    for (const auto [second, period] : std::array<std::pair<unsigned, unsigned>, 3>{
             {{0, 142}, {1, 95}, {2, 71}}}) {
        write(second * RealtimeAyPrototype::AyTicksPerSecond, 0,
              static_cast<std::uint8_t>(period & 0xFF));
        write(second * RealtimeAyPrototype::AyTicksPerSecond, 1,
              static_cast<std::uint8_t>(period >> 8));
    }
    return result;
}

std::vector<PsgEvent> mixed_noise_envelope() {
    std::vector<PsgEvent> result;
    std::uint16_t sequence = 0;
    const auto write = [&](std::uint32_t tick, std::uint8_t reg, std::uint8_t value) {
        result.push_back({tick, sequence++, reg, value});
    };
    write(0, 0, 0x34); write(0, 1, 0x02);
    write(0, 2, 80); write(0, 3, 0);
    write(0, 4, 125); write(0, 5, 0);
    write(0, 6, 7); write(0, 7, 0x20);
    write(0, 8, 15); write(0, 9, 12); write(0, 10, 0x10);
    write(0, 11, 20); write(0, 12, 0); write(0, 13, 10);
    write(250'000, 13, 10);  // identical data must still restart the envelope.
    write(400'000, 6, 3);
    write(500'000, 7, 0x28);
    write(600'000, 11, 40);
    write(750'000, 8, 9);
    return result;
}

double rms(const float* begin, const float* end) {
    double sum = 0.0;
    for (auto it = begin; it != end; ++it) sum += *it * *it;
    return std::sqrt(sum / static_cast<double>(end - begin));
}

unsigned zero_crossings(const float* begin, const float* end) {
    unsigned count = 0;
    for (auto it = begin + 1; it != end; ++it) {
        if ((*(it - 1) < 0.0F && *it >= 0.0F) ||
            (*(it - 1) >= 0.0F && *it < 0.0F)) ++count;
    }
    return count;
}

double upward_frequency(const std::vector<float>& pcm, std::size_t begin,
                        std::size_t end) {
    unsigned crossings = 0;
    for (std::size_t index = begin + 1; index < end; ++index) {
        if (pcm[index - 1] <= 0.0F && pcm[index] > 0.0F) ++crossings;
    }
    return crossings * static_cast<double>(RealtimeAyPrototype::SampleRate) /
           static_cast<double>(end - begin);
}

std::vector<float> render_direct(const std::vector<PsgEvent>& events,
                                 std::size_t sample_count) {
    ay38910_desc_t desc{};
    desc.type = AY38910_TYPE_8912;
    desc.tick_hz = static_cast<int>(RealtimeAyPrototype::AyTicksPerSecond);
    desc.sound_hz = static_cast<int>(RealtimeAyPrototype::SampleRate);
    desc.magnitude = 0.2F;
    ay38910_t ay{};
    ay38910_init(&ay, &desc);

    std::vector<float> result(sample_count);
    std::uint32_t tick = 0;
    std::size_t next_event = 0;
    std::size_t produced = 0;
    while (produced < sample_count) {
        while (next_event < events.size() && events[next_event].target_ay_tick <= tick) {
            ay38910_set_register(&ay, events[next_event].reg, events[next_event].value);
            ++next_event;
        }
        if (ay38910_tick(&ay)) result[produced++] = ay.sample;
        ++tick;
    }
    return result;
}
}

int main() {
    using namespace cpcexplorer::stage4h5;
    static_assert(sizeof(SpscPsgQueue<64>) <= 544);

    AyDcBuffer stopped_dc{};
    RealtimeAyPrototype stopped{stopped_dc};
    std::array<float, 128> stopped_pcm{};
    assert(!stopped.enqueue({0, 0, 0, 1}));
    assert(stopped.render(stopped_pcm.data(), stopped_pcm.size()) == stopped_pcm.size());
    assert(std::all_of(stopped_pcm.begin(), stopped_pcm.end(),
                       [](float sample) { return sample == 0.0F; }));
    stopped.start(7);
    assert(stopped.running() && stopped.epoch() == 7);
    stopped.stop();
    stopped.reset_while_stopped(8);
    assert(!stopped.running() && stopped.epoch() == 8 && stopped.queue_depth() == 0);

    // Restart must clear queued writes and all oscillator/filter phase so audio
    // from a previous game cannot leak into the next snapshot.
    stopped.start(8);
    for (const auto& event : three_notes()) assert(stopped.enqueue(event));
    std::array<float, 512> restarted_pcm{};
    stopped.render(restarted_pcm.data(), restarted_pcm.size());
    stopped.stop();
    stopped.reset_while_stopped(9);
    stopped.start(9);
    for (const auto& event : three_notes()) assert(stopped.enqueue(event));
    std::array<float, 512> reset_pcm{};
    stopped.render(reset_pcm.data(), reset_pcm.size());
    assert(restarted_pcm == reset_pcm);

    AyDcBuffer sequence_dc{};
    RealtimeAyPrototype sequence_probe{sequence_dc};
    sequence_probe.start(1);
    assert(sequence_probe.enqueue({0, 10, 7, 0x3E}));
    assert(sequence_probe.enqueue({0, 12, 8, 15}));
    std::array<float, 16> sequence_pcm{};
    sequence_probe.render(sequence_pcm.data(), sequence_pcm.size());
    assert(sequence_probe.sequence_errors() == 1);

    AyDcBuffer prototype_dc{};
    RealtimeAyPrototype prototype{prototype_dc, RealtimeAyPrototype::Advancement::Span};
    prototype.start(1);
    const auto events = three_notes();
    for (const auto& event : events) assert(prototype.enqueue(event));

    std::vector<float> output(3U * RealtimeAyPrototype::SampleRate);
    const auto span_start = std::chrono::steady_clock::now();
    assert(prototype.render(output.data(), output.size()) == output.size());
    const auto span_elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - span_start).count();

    assert(prototype.queue_overruns() == 0);
    assert(prototype.queue_high_water() == events.size());
    assert(prototype.applied_events() == events.size());
    assert(prototype.last_sequence() == events.back().sequence);
    const auto direct = render_direct(events, output.size());
    assert(output == direct);  // queue/timestamp path is sample-exact at 100% time.

    AyDcBuffer tick_dc{};
    RealtimeAyPrototype tick_prototype{tick_dc, RealtimeAyPrototype::Advancement::Tick};
    tick_prototype.start(1);
    for (const auto& event : events) assert(tick_prototype.enqueue(event));
    std::vector<float> tick_output(output.size());
    const auto tick_start = std::chrono::steady_clock::now();
    tick_prototype.render(tick_output.data(), tick_output.size());
    const auto tick_elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - tick_start).count();
    assert(tick_output == direct);

    auto port_events = events;
    port_events.push_back({500'000, 100, 14, 0x00});
    port_events.push_back({1'500'000, 101, 14, 0xFF});
    std::stable_sort(port_events.begin(), port_events.end(),
                     [](const auto& left, const auto& right) {
                         return left.target_ay_tick < right.target_ay_tick;
                     });
    AyDcBuffer port_dc{};
    RealtimeAyPrototype port_span{port_dc};
    port_span.start(4);
    for (const auto& event : port_events) assert(port_span.enqueue(event));
    std::vector<float> port_pcm(output.size());
    port_span.render(port_pcm.data(), port_pcm.size());
    assert(port_pcm == direct);  // CPC port-A/keyboard traffic is audio-neutral.
    assert(port_span.applied_events() == port_events.size());

    const auto mixed_events = mixed_noise_envelope();
    const auto mixed_direct = render_direct(mixed_events, RealtimeAyPrototype::SampleRate);
    AyDcBuffer mixed_span_dc{}, mixed_tick_dc{};
    RealtimeAyPrototype mixed_span{mixed_span_dc, RealtimeAyPrototype::Advancement::Span};
    RealtimeAyPrototype mixed_tick{mixed_tick_dc, RealtimeAyPrototype::Advancement::Tick};
    mixed_span.start(2); mixed_tick.start(2);
    for (const auto& mixed_event : mixed_events) {
        assert(mixed_span.enqueue(mixed_event));
        assert(mixed_tick.enqueue(mixed_event));
    }
    std::vector<float> mixed_span_pcm(mixed_direct.size());
    std::vector<float> mixed_tick_pcm(mixed_direct.size());
    mixed_span.render(mixed_span_pcm.data(), mixed_span_pcm.size());
    mixed_tick.render(mixed_tick_pcm.data(), mixed_tick_pcm.size());
    if (mixed_span_pcm != mixed_direct) {
        const auto mismatch = std::mismatch(mixed_span_pcm.begin(), mixed_span_pcm.end(),
                                            mixed_direct.begin());
        std::fprintf(stderr, "mixed span mismatch sample=%zu span=%.9f direct=%.9f\n",
                     static_cast<std::size_t>(mismatch.first - mixed_span_pcm.begin()),
                     *mismatch.first, *mismatch.second);
        return 2;
    }
    assert(mixed_tick_pcm == mixed_direct);

    const auto one_second = RealtimeAyPrototype::SampleRate;
    std::array<unsigned, 3> crossings{};
    std::array<double, 3> levels{};
    for (std::size_t note = 0; note < 3; ++note) {
        const auto* begin = output.data() + note * one_second + 1000;
        const auto* end = output.data() + (note + 1) * one_second;
        crossings[note] = zero_crossings(begin, end);
        levels[note] = rms(begin, end);
        assert(levels[note] > 0.005);
    }
    assert(crossings[0] < crossings[1] && crossings[1] < crossings[2]);

    // At 68% emulation, register events arrive 1/0.68 times farther apart.
    // The independent oscillator must retain pitch while note tempo follows
    // those delayed writes.
    std::array<double, 3> slowed_hz{};
    for (double ratio : std::array{0.68, 0.80, 1.00, 1.25}) {
        AyDcBuffer scaled_dc{};
        RealtimeAyPrototype scaled{scaled_dc};
        scaled.start(3);
        const double note_seconds = std::max(1.0, 1.0 / ratio);
        const auto sample_count = static_cast<std::size_t>(
            std::ceil(3.0 * RealtimeAyPrototype::SampleRate * note_seconds));
        std::vector<float> pcm(sample_count);
        std::size_t produced = 0;
        std::size_t next_event = 0;
        while (produced < pcm.size()) {
            const double wall_seconds = static_cast<double>(produced) /
                                        RealtimeAyPrototype::SampleRate;
            while (next_event < events.size() &&
                   events[next_event].target_ay_tick /
                       static_cast<double>(RealtimeAyPrototype::AyTicksPerSecond) /
                       ratio <= wall_seconds) {
                // Target remains emulated time. Slow writes arrive late and
                // apply immediately; fast writes wait for their target.
                assert(scaled.enqueue(events[next_event++]));
            }
            const auto chunk = std::min<std::size_t>(128, pcm.size() - produced);
            scaled.render(pcm.data() + produced, chunk);
            produced += chunk;
        }
        std::array<double, 3> hz{};
        for (std::size_t note = 0; note < 3; ++note) {
            const auto begin = static_cast<std::size_t>(
                (note * note_seconds + 0.20 * note_seconds) *
                RealtimeAyPrototype::SampleRate);
            const auto end = static_cast<std::size_t>(
                ((note + 1) * note_seconds - 0.10 * note_seconds) *
                RealtimeAyPrototype::SampleRate);
            hz[note] = upward_frequency(pcm, begin, end);
        }
        assert(std::fabs(hz[0] - 440.0) < 10.0);
        assert(std::fabs(hz[1] - 660.0) < 10.0);
        assert(std::fabs(hz[2] - 880.0) < 10.0);
        if (ratio == 0.68) slowed_hz = hz;
    }

    SpscPsgQueue<4> bounded;
    assert(bounded.push({0, 0, 0, 1}));
    assert(bounded.push({0, 1, 0, 2}));
    assert(bounded.push({0, 2, 0, 3}));
    assert(!bounded.push({0, 3, 0, 4}));
    assert(bounded.overruns() == 1);
    PsgEvent event{};
    for (std::uint16_t sequence = 0; sequence < 3; ++sequence) {
        assert(bounded.pop(event));
        assert(event.sequence == sequence);
    }

    SpscPsgQueue<64> knight_burst_64;
    SpscPsgQueue<128> knight_burst_128;
    for (std::uint16_t sequence = 0; sequence < 50; ++sequence) {
        const PsgEvent burst{0, sequence, static_cast<std::uint8_t>(sequence % 14),
                             static_cast<std::uint8_t>(sequence)};
        assert(knight_burst_64.push(burst));
        assert(knight_burst_128.push(burst));
    }
    assert(knight_burst_64.high_water() == 50);
    assert(knight_burst_128.high_water() == 50);

    SpscPsgQueue<128> threaded;
    constexpr std::uint32_t stress_events = 100'000;
    std::atomic<bool> producer_done{false};
    std::atomic<bool> ordering_ok{true};
    std::thread producer([&] {
        for (std::uint32_t index = 0; index < stress_events; ++index) {
            const PsgEvent item{index, static_cast<std::uint16_t>(index),
                                static_cast<std::uint8_t>(index % 14),
                                static_cast<std::uint8_t>(index)};
            while (!threaded.push(item)) std::this_thread::yield();
        }
        producer_done.store(true, std::memory_order_release);
    });
    std::thread consumer([&] {
        std::uint32_t expected = 0;
        PsgEvent item{};
        while (expected < stress_events) {
            if (threaded.pop(item)) {
                if (item.target_ay_tick != expected ||
                    item.sequence != static_cast<std::uint16_t>(expected))
                    ordering_ok.store(false, std::memory_order_relaxed);
                ++expected;
            } else if (!producer_done.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    });
    producer.join(); consumer.join();
    assert(ordering_ok.load() && threaded.depth() == 0);

    std::printf(
        "PASS stage4h5 prototype reference_state=%zu compact_state=%zu dc=%zu "
        "queue64=%zu queue128=%zu control=%zu "
        "span_3s_ms=%.3f tick_3s_ms=%.3f speedup=%.2fx "
        "rms=%.6f/%.6f/%.6f crossings=%u/%u/%u slowed_hz=%.2f/%.2f/%.2f "
        "high_water=%u\n",
        sizeof(ay38910_t), sizeof(CompactAyState), sizeof(AyDcBuffer),
        sizeof(SpscPsgQueue<64>), sizeof(SpscPsgQueue<128>),
        sizeof(RealtimeAyPrototype),
        span_elapsed, tick_elapsed, tick_elapsed / span_elapsed,
        levels[0], levels[1], levels[2], crossings[0], crossings[1],
        crossings[2], slowed_hz[0], slowed_hz[1], slowed_hz[2],
        prototype.queue_high_water());
    return 0;
}
