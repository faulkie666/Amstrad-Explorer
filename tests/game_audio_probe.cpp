#include "cpc_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
std::vector<float> samples;
struct BusEvent { std::uint64_t cycle; std::uint8_t reg; std::uint8_t value; };
std::vector<BusEvent> events;

void sink(const float* input, std::size_t count, void*) {
    samples.insert(samples.end(), input, input + count);
}
void trace(std::uint64_t cycle, std::uint8_t reg, std::uint8_t value, void*) {
    events.push_back({cycle, reg, value});
}
void u32(std::ofstream& output, std::uint32_t value) {
    output.put(static_cast<char>(value)); output.put(static_cast<char>(value >> 8));
    output.put(static_cast<char>(value >> 16)); output.put(static_cast<char>(value >> 24));
}
void u16(std::ofstream& output, std::uint16_t value) {
    output.put(static_cast<char>(value)); output.put(static_cast<char>(value >> 8));
}
bool write_wav(const char* path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    const auto bytes = static_cast<std::uint32_t>(samples.size() * 2U);
    output.write("RIFF", 4); u32(output, 36U + bytes);
    output.write("WAVEfmt ", 8); u32(output, 16); u16(output, 1); u16(output, 1);
    u32(output, cpcexplorer::AudioSampleRate);
    u32(output, cpcexplorer::AudioSampleRate * 2U); u16(output, 2); u16(output, 16);
    output.write("data", 4); u32(output, bytes);
    for (float sample : samples) {
        const auto pcm = static_cast<std::int16_t>(
            std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f));
        u16(output, static_cast<std::uint16_t>(pcm));
    }
    return output.good();
}
void key(bool down, int code) {
    if (down) cpcexplorer::key_down(code); else cpcexplorer::key_up(code);
}
void pulse_at(int slice, int begin, int length, int code) {
    if (slice == begin) key(true, code);
    if (slice == begin + length) key(false, code);
}
void inputs(const std::string& profile, int slice) {
    if (profile == "dizzy") {
        pulse_at(slice, 50, 5, ' ');
        if (slice >= 100) {
            const int phase = slice % 300;
            pulse_at(phase, 0, 50, 'z'); pulse_at(phase, 75, 50, 'x');
            pulse_at(slice % 150, 140, 5, ' ');
        }
    } else if (profile == "dizzy_states") {
        // Title (0..10s), gameplay, pause/resume, return to title, restart.
        pulse_at(slice, 500, 5, ' ');
        pulse_at(slice, 1500, 5, 'p'); pulse_at(slice, 1750, 5, 'p');
        pulse_at(slice, 2250, 5, 'q'); pulse_at(slice, 2500, 5, ' ');
        if (slice >= 550 && slice < 2250) {
            pulse_at(slice % 300, 0, 50, 'z');
            pulse_at(slice % 300, 75, 50, 'x');
            pulse_at(slice % 150, 140, 5, ' ');
        }
    } else if (profile == "manic") {
        const int phase = slice % 300;
        pulse_at(phase, 0, 50, 'q'); pulse_at(phase, 75, 50, 'w');
        pulse_at(slice % 150, 140, 5, cpcexplorer::KeyShift);
    } else if (profile == "chuckie" || profile == "chuckie_start") {
        pulse_at(slice, 50, 5, 's'); pulse_at(slice, 100, 5, '1');
        if (profile == "chuckie" && slice >= 150) {
            const int phase = slice % 300;
            pulse_at(phase, 0, 50, 'z'); pulse_at(phase, 75, 50, 'x');
            pulse_at(slice % 150, 140, 5, ' ');
        }
    } else if (profile == "knight" || profile == "knight_music" ||
               profile == "knight_once") {
        pulse_at(slice, 50, 5, ' '); pulse_at(slice, 200, 5, ' ');
        if (slice >= 250 && (profile != "knight_once" || slice <= 1055)) {
            const int phase = slice % 400;
            if (profile == "knight" || profile == "knight_once") {
                pulse_at(phase, 0, 50, 'n'); pulse_at(phase, 75, 50, 'm');
                pulse_at(phase, 150, 20, 'a'); pulse_at(phase, 200, 20, 'z');
            }
            pulse_at(phase, 250, 5, ' ');
        }
    }
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::fprintf(stderr, "usage: game_audio_probe SNA PROFILE SECONDS WAV CSV\n");
        return 64;
    }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> snapshot{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (snapshot.size() != 65'792 ||
        !std::equal(snapshot.begin(), snapshot.begin() + 8, "MV - SNA")) return 1;
    const int seconds = std::stoi(argv[3]);
    if (seconds < 1 || seconds > 300) return 2;

    using namespace cpcexplorer;
    if (!select_engine(EngineKind::Fast)) return 3;
    init(); set_audio_callback(sink); testing::set_ay_write_callback(trace);
    if (!quickload(snapshot.data(), snapshot.size(), false)) return 4;
    for (int slice = 0; slice < seconds * 50; ++slice) {
        inputs(argv[2], slice); run_us(20'000);
    }
    if (!write_wav(argv[4])) return 5;
    std::ofstream csv(argv[5]);
    if (!csv) return 6;
    csv << "cycle,seconds,event,register,value\n";
    for (const auto& event : events) {
        csv << event.cycle << ',' << event.cycle / 4'000'000.0 << ','
            << (event.reg == 0xFF ? "latch" : "write") << ',';
        if (event.reg == 0xFF) csv << -1; else csv << static_cast<unsigned>(event.reg);
        csv << ',' << static_cast<unsigned>(event.value) << '\n';
    }
    const auto diag = audio_diagnostics();
    std::printf("profile=%s seconds=%d samples=%zu changed_sound_regs=%llu nonzero=%llu bus_events=%zu\n",
        argv[2], seconds, samples.size(),
        static_cast<unsigned long long>(diag.register_writes),
        static_cast<unsigned long long>(diag.nonzero_samples), events.size());
    return 0;
}
