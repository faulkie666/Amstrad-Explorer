#include "cpc_core.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
std::vector<float> pcm;
void sink(const float* samples, std::size_t count, void*) {
    pcm.insert(pcm.end(), samples, samples + count);
}
void u32(std::ofstream& stream, std::uint32_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void u16(std::ofstream& stream, std::uint16_t value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
bool write_wav(const char* path) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    const auto data_bytes = static_cast<std::uint32_t>(pcm.size() * 2U);
    output.write("RIFF", 4); u32(output, 36U + data_bytes);
    output.write("WAVE", 4); output.write("fmt ", 4); u32(output, 16);
    u16(output, 1); u16(output, 1); u32(output, cpcexplorer::AudioSampleRate);
    u32(output, cpcexplorer::AudioSampleRate * 2U); u16(output, 2); u16(output, 16);
    output.write("data", 4); u32(output, data_bytes);
    for (float sample : pcm) {
        const auto value = static_cast<std::int16_t>(
            std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f));
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    return output.good();
}
std::uint16_t read_u16(const std::array<std::uint8_t, 44>& header, std::size_t offset) {
    return static_cast<std::uint16_t>(header[offset]) |
           (static_cast<std::uint16_t>(header[offset + 1]) << 8U);
}
std::uint32_t read_u32(const std::array<std::uint8_t, 44>& header, std::size_t offset) {
    return static_cast<std::uint32_t>(header[offset]) |
           (static_cast<std::uint32_t>(header[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(header[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(header[offset + 3]) << 24U);
}
bool validate_wav(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return false;
    const auto file_bytes = input.tellg();
    if (file_bytes != static_cast<std::streamoff>(44U + pcm.size() * 2U)) return false;
    input.seekg(0);
    std::array<std::uint8_t, 44> header{};
    input.read(reinterpret_cast<char*>(header.data()), header.size());
    return input.good() &&
           std::equal(header.begin(), header.begin() + 4, "RIFF") &&
           std::equal(header.begin() + 8, header.begin() + 12, "WAVE") &&
           std::equal(header.begin() + 12, header.begin() + 16, "fmt ") &&
           std::equal(header.begin() + 36, header.begin() + 40, "data") &&
           read_u32(header, 4) == 36U + pcm.size() * 2U &&
           read_u32(header, 16) == 16U && read_u16(header, 20) == 1U &&
           read_u16(header, 22) == 1U &&
           read_u32(header, 24) == cpcexplorer::AudioSampleRate &&
           read_u32(header, 28) == cpcexplorer::AudioSampleRate * 2U &&
           read_u16(header, 32) == 2U && read_u16(header, 34) == 16U &&
           read_u32(header, 40) == pcm.size() * 2U;
}
double frequency(std::size_t begin, std::size_t end) {
    std::size_t crossings = 0;
    for (std::size_t i = begin + 1; i < end; ++i)
        if (pcm[i - 1] <= 0.0f && pcm[i] > 0.0f) ++crossings;
    return crossings * static_cast<double>(cpcexplorer::AudioSampleRate) /
           static_cast<double>(end - begin);
}
}  // namespace

int main(int argc, char** argv) {
    using namespace cpcexplorer;
    select_engine(EngineKind::Reference);
    init();
    set_audio_callback(sink);
    testing::ay_write_register(7, 0x3E);  // A tone on; noise and B/C off.
    testing::ay_write_register(8, 15);

    constexpr std::array<double, 3> requested{440.0, 660.0, 880.0};
    std::array<double, 3> measured{};
    for (double hz : requested) {
        const auto period = static_cast<std::uint16_t>(
            std::lround(1'000'000.0 / (16.0 * hz)));
        testing::ay_write_register(0, static_cast<std::uint8_t>(period));
        testing::ay_write_register(1, static_cast<std::uint8_t>(period >> 8));
        testing::ay_advance_ticks(1'000'000);
    }
    if (pcm.size() < 66'000) return 1;
    constexpr std::size_t settle = 4096;
    for (std::size_t i = 0; i < requested.size(); ++i) {
        const std::size_t begin = i * AudioSampleRate + settle;
        const std::size_t end = std::min((i + 1) * AudioSampleRate - 512U, pcm.size());
        measured[i] = frequency(begin, end);
        if (std::fabs(measured[i] - requested[i]) > 8.0) return 2;
    }
    double square_sum = 0.0;
    float peak = 0.0f;
    for (float sample : pcm) {
        square_sum += static_cast<double>(sample) * sample;
        peak = std::max(peak, std::fabs(sample));
    }
    const double rms = std::sqrt(square_sum / pcm.size());
    if (rms < 0.03 || peak < 0.08f || peak > 0.25f) return 3;
    if (argc < 2 || !write_wav(argv[1])) return 4;
    if (!validate_wav(argv[1])) return 5;
    std::printf("PASS: tones %.2f %.2f %.2f Hz samples=%zu rms=%.6f peak=%.6f\n",
                measured[0], measured[1], measured[2], pcm.size(), rms, peak);
    return 0;
}
