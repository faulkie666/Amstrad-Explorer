#include "cpc_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
std::vector<float> samples;

void sink(const float* input, std::size_t count, void*) {
    samples.insert(samples.end(), input, input + count);
}

void write_u32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void write_u16(std::ofstream& output, std::uint16_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void configure_mixed_ay() {
    using cpcexplorer::testing::ay_write_register;
    ay_write_register(0, 0x34);
    ay_write_register(1, 0x02);
    ay_write_register(2, 80);
    ay_write_register(3, 0);
    ay_write_register(6, 7);
    ay_write_register(7, 0x20);
    ay_write_register(8, 15);
    ay_write_register(9, 12);
    ay_write_register(10, 0x10);
    ay_write_register(11, 20);
    ay_write_register(12, 0);
    ay_write_register(13, 10);
}

std::vector<float> render(cpcexplorer::EngineKind engine) {
    using namespace cpcexplorer;
    if (!select_engine(engine)) return {};
    init();
    samples.clear();
    set_audio_callback(sink);
    configure_mixed_ay();
    testing::ay_advance_ticks(1'000'000);
    return samples;
}

std::vector<float> render_tone() {
    using namespace cpcexplorer;
    select_engine(EngineKind::Reference);
    init();
    samples.clear();
    set_audio_callback(sink);
    testing::ay_write_register(0, 125);
    testing::ay_write_register(1, 0);
    testing::ay_write_register(7, 0x3E);
    testing::ay_write_register(8, 15);
    testing::ay_advance_ticks(1'000'000);
    return samples;
}

std::vector<float> render_envelope(cpcexplorer::EngineKind engine) {
    using namespace cpcexplorer;
    if (!select_engine(engine)) return {};
    init();
    samples.clear();
    set_audio_callback(sink);
    testing::ay_write_register(0, 125);
    testing::ay_write_register(1, 0);
    testing::ay_write_register(7, 0x3E);
    testing::ay_write_register(8, 0x10);
    testing::ay_write_register(11, 0xE8);
    testing::ay_write_register(12, 0x03);  // 1000: visible envelope steps.
    testing::ay_write_register(13, 10);    // Repeating triangle shape.
    testing::ay_advance_ticks(250'000);
    return samples;
}

void write_wav(const char* path, const std::vector<float>& input) {
    std::ofstream output(path, std::ios::binary);
    const auto bytes = static_cast<std::uint32_t>(input.size() * sizeof(std::int16_t));
    output.write("RIFF", 4); write_u32(output, 36 + bytes);
    output.write("WAVEfmt ", 8); write_u32(output, 16);
    write_u16(output, 1); write_u16(output, 1);
    write_u32(output, cpcexplorer::AudioSampleRate);
    write_u32(output, cpcexplorer::AudioSampleRate * 2);
    write_u16(output, 2); write_u16(output, 16);
    output.write("data", 4); write_u32(output, bytes);
    for (const float value : input) {
        const auto pcm = static_cast<std::int16_t>(
            std::clamp(value, -1.0f, 1.0f) * 32767.0f);
        output.write(reinterpret_cast<const char*>(&pcm), sizeof(pcm));
    }
}
}  // namespace

int main(int argc, char** argv) {
    using namespace cpcexplorer;

    select_engine(EngineKind::Reference);
    init();
    testing::ay_write_register(1, 0xFF);
    if (testing::ay_read_register(1) != 0x0F) return 1;

    const auto tone = render_tone();
    std::size_t upward_crossings = 0;
    constexpr std::size_t settle = 4096;
    for (std::size_t i = settle + 1; i < tone.size(); ++i)
        if (tone[i - 1] <= 0.0f && tone[i] > 0.0f) ++upward_crossings;
    const double measured_hz = upward_crossings * static_cast<double>(AudioSampleRate) /
                               static_cast<double>(tone.size() - settle);
    // 1 MHz AY clock / (16 * period 125) = 500 Hz.
    if (measured_hz < 490.0 || measured_hz > 510.0) return 2;

    const auto reference = render(EngineKind::Reference);
    const auto fast = render(EngineKind::Fast);
    if (reference.size() < AudioSampleRate - 128 || reference != fast) return 3;
    if (std::none_of(reference.begin(), reference.end(),
                     [](float value) { return std::fabs(value) > 0.0001f; })) return 4;

    const auto envelope_reference = render_envelope(EngineKind::Reference);
    const auto envelope_fast = render_envelope(EngineKind::Fast);
    if (envelope_reference != envelope_fast || envelope_reference.size() < 5000) return 6;
    float quiet_peak = 0.0f, loud_peak = 0.0f;
    for (std::size_t begin = 256; begin + 256 <= envelope_reference.size(); begin += 256) {
        float window_peak = 0.0f;
        for (std::size_t i = begin; i < begin + 256; ++i)
            window_peak = std::max(window_peak, std::fabs(envelope_reference[i]));
        if (quiet_peak == 0.0f || window_peak < quiet_peak) quiet_peak = window_peak;
        loud_peak = std::max(loud_peak, window_peak);
    }
    if (quiet_peak <= 0.0f || loud_peak < quiet_peak * 3.0f) return 7;

    reset();
    samples.clear();
    set_audio_callback(sink);
    testing::ay_advance_ticks(100'000);
    float peak = 0.0f;
    for (const float value : samples) peak = std::max(peak, std::fabs(value));
    if (peak > 0.0001f) return 5;

    if (argc > 1) write_wav(argv[1], reference);
    std::printf("PASS: AY masks, tone=%.2fHz, mixer/noise, envelope dynamic range=%.2fx, exact fast batching, reset silence, samples=%zu\n",
                measured_hz, loud_peak / quiet_peak, reference.size());
    return 0;
}
