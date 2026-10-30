#include "realtime_ay_prototype.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using cpcexplorer::stage4h5::PsgEvent;
using cpcexplorer::stage4h5::RealtimeAyPrototype;
using cpcexplorer::stage4h5::AyDcBuffer;

struct TraceEvent { std::uint64_t cpc_cycle; std::uint8_t reg; std::uint8_t value; };

std::vector<TraceEvent> read_trace(const char* path) {
    std::ifstream input(path);
    std::vector<TraceEvent> events;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        std::array<std::string, 5> fields;
        std::istringstream row(line);
        for (auto& field : fields) std::getline(row, field, ',');
        if (fields[2] != "write") continue;
        const int reg = std::stoi(fields[3]);
        if (reg < 0 || reg > 13) continue;
        events.push_back({std::stoull(fields[0]), static_cast<std::uint8_t>(reg),
                          static_cast<std::uint8_t>(std::stoi(fields[4]))});
    }
    return events;
}

bool run_trace(const char* label, const char* path) {
    constexpr double ratio = 0.68;
    constexpr std::uint64_t emulated_seconds = 60;
    constexpr std::size_t block_samples = 128;
    const auto source = read_trace(path);
    if (source.empty()) return false;

    std::vector<PsgEvent> events;
    events.reserve(source.size());
    std::uint16_t sequence = 0;
    for (const auto& item : source) {
        const auto ay_tick = static_cast<std::uint32_t>(std::llround(
            (item.cpc_cycle / 4.0) / ratio));
        events.push_back({ay_tick, sequence++, item.reg, item.value});
    }

    AyDcBuffer dc_buffer{};
    RealtimeAyPrototype synth{dc_buffer, RealtimeAyPrototype::Advancement::Span};
    synth.start(1);
    const auto nominal_end_tick = static_cast<std::uint64_t>(std::ceil(
        (emulated_seconds / ratio) * RealtimeAyPrototype::AyTicksPerSecond));
    const auto end_tick = std::max<std::uint64_t>(
        nominal_end_tick, static_cast<std::uint64_t>(events.back().target_ay_tick) + 6'000U);
    const auto total_samples = static_cast<std::size_t>(std::ceil(
        end_tick * static_cast<double>(AY38910_FIXEDPOINT_SCALE) /
        synth.sample_period()));
    std::array<float, block_samples> block{};
    std::size_t next = 0;
    std::size_t produced = 0;
    std::uint64_t nonzero = 0;
    double square_sum = 0.0;
    float peak = 0.0F;
    const auto start = std::chrono::steady_clock::now();
    while (produced < total_samples) {
        const auto count = std::min(block_samples, total_samples - produced);
        const auto horizon = synth.ay_ticks() + static_cast<std::uint32_t>(
            std::ceil(count * static_cast<double>(RealtimeAyPrototype::AyTicksPerSecond) /
                      RealtimeAyPrototype::SampleRate));
        while (next < events.size() && events[next].target_ay_tick <= horizon) {
            if (!synth.enqueue(events[next])) return false;
            ++next;
        }
        if (synth.render(block.data(), count) != count) return false;
        for (std::size_t index = 0; index < count; ++index) {
            const float sample = block[index];
            if (std::fabs(sample) > 0.0001F) ++nonzero;
            square_sum += static_cast<double>(sample) * sample;
            peak = std::max(peak, std::fabs(sample));
        }
        produced += count;
    }
    const auto elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    const double rms = std::sqrt(square_sum / produced);
    const double output_seconds = produced /
        static_cast<double>(RealtimeAyPrototype::SampleRate);
    const double realtime_load = elapsed_ms / (1000.0 * output_seconds) * 100.0;
    std::printf(
        "TRACE label=%s writes=%zu applied=%u samples=%zu nonzero=%llu rms=%.6f "
        "peak=%.6f queue_high=%u overruns=%u enqueued=%zu ay_ticks=%u last_target=%u "
        "span_ms=%.3f desktop_load_pct=%.4f\n",
        label, events.size(), synth.applied_events(), produced,
        static_cast<unsigned long long>(nonzero), rms, peak,
        synth.queue_high_water(), synth.queue_overruns(), next, synth.ay_ticks(),
        events.back().target_ay_tick, elapsed_ms, realtime_load);
    return next == events.size() && synth.applied_events() == events.size() &&
           synth.queue_overruns() == 0 && produced == total_samples;
}
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: realtime_ay_trace_test MANIC_CSV KNIGHT_CSV DIZZY_CSV\n");
        return 64;
    }
    if (!run_trace("manic", argv[1])) return 1;
    if (!run_trace("knight", argv[2])) return 2;
    if (!run_trace("dizzy", argv[3])) return 3;
    std::puts("PASS stage4h5 game-trace continuity and bounded queue");
    return 0;
}
