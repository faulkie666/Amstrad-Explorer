#include "cpc_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#ifndef CPCEXPLORER_DIZZY_SNA
#error CPCEXPLORER_DIZZY_SNA must name the validated Dizzy snapshot
#endif

namespace {
std::vector<float> samples;
struct BusEvent { std::uint64_t cycle; std::uint8_t reg; std::uint8_t value; };
std::vector<BusEvent> bus_events;
void sink(const float* input, std::size_t count, void*) {
    samples.insert(samples.end(), input, input + count);
}
void trace(std::uint64_t cycle, std::uint8_t reg, std::uint8_t value, void*) {
    bus_events.push_back({cycle, reg, value});
}
void u32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void u16(std::ofstream& output, std::uint16_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void write_wav(const char* path) {
    std::ofstream output(path, std::ios::binary);
    const auto bytes = static_cast<std::uint32_t>(samples.size() * 2U);
    output.write("RIFF", 4); u32(output, 36 + bytes);
    output.write("WAVEfmt ", 8); u32(output, 16); u16(output, 1); u16(output, 1);
    u32(output, cpcexplorer::AudioSampleRate); u32(output, cpcexplorer::AudioSampleRate * 2);
    u16(output, 2); u16(output, 16); output.write("data", 4); u32(output, bytes);
    for (const float value : samples) {
        const auto pcm = static_cast<std::int16_t>(
            std::clamp(value, -1.0f, 1.0f) * 32767.0f);
        output.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }
}
}  // namespace

int main(int argc, char** argv) {
    std::ifstream input(CPCEXPLORER_DIZZY_SNA, std::ios::binary);
    const std::vector<std::uint8_t> snapshot{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (snapshot.size() != 65'792 || !std::equal(snapshot.begin(), snapshot.begin() + 8,
                                                 "MV - SNA")) return 1;

    using namespace cpcexplorer;
    if (!select_engine(EngineKind::Fast)) return 2;
    init();
    set_audio_callback(sink);
    testing::set_ay_write_callback(trace);
    if (!quickload(snapshot.data(), snapshot.size(), false)) return 3;
    // The SNA header stores the selected register at 0x5A and the 16 PSG
    // registers at 0x5B.  Confirm restoration before executing game code.
    for (std::uint8_t reg = 0; reg < 16; ++reg)
        if (testing::ay_read_register(reg) != snapshot[0x5B + reg]) return 4;

    // Start the game, then exercise movement and repeated jumping for a full
    // minute of emulated time rather than assuming the title state has music.
    for (int slice = 0; slice < 3000; ++slice) {
        if (slice == 50) key_down(' ');
        if (slice == 55) key_up(' ');
        if (slice >= 100 && slice % 300 == 0) key_down('z');
        if (slice >= 100 && slice % 300 == 50) key_up('z');
        if (slice >= 100 && slice % 300 == 75) key_down('x');
        if (slice >= 100 && slice % 300 == 125) key_up('x');
        if (slice >= 100 && slice % 150 == 140) key_down(' ');
        if (slice >= 100 && slice % 150 == 145) key_up(' ');
        run_us(20'000);
    }
    const auto diag = audio_diagnostics();
    if (diag.register_writes == 0 || diag.nonzero_samples == 0 ||
        samples.size() < AudioSampleRate * 59U || bus_events.empty()) return 5;
    if (argc > 1) write_wav(argv[1]);
    if (argc > 2) {
        std::ofstream output(argv[2]);
        output << "cycle,seconds,event,register,value\n";
        for (const auto& event : bus_events) {
            output << event.cycle << ',' << (event.cycle / 4'000'000.0) << ','
                   << (event.reg == 0xFF ? "latch" : "write") << ',';
            if (event.reg == 0xFF) output << -1;
            else output << static_cast<unsigned>(event.reg);
            output << ',' << static_cast<unsigned>(event.value) << '\n';
        }
    }
    std::printf("PASS: Dizzy PSG changes=%llu bus_events=%zu last_event_s=%.3f samples=%llu nonzero=%llu wav_samples=%zu\n",
        static_cast<unsigned long long>(diag.register_writes),
        bus_events.size(), bus_events.back().cycle / 4'000'000.0,
        static_cast<unsigned long long>(diag.samples),
        static_cast<unsigned long long>(diag.nonzero_samples), samples.size());
    return 0;
}
