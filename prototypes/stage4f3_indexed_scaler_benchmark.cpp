#include "cpc_core.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto size = input.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    return input ? bytes : std::vector<std::uint8_t>{};
}

std::uint16_t rgb565(std::uint32_t abgr) {
    return static_cast<std::uint16_t>(((abgr & 0x000000f8u) << 8) |
        ((abgr & 0x0000fc00u) >> 5) | ((abgr & 0x00f80000u) >> 19));
}

void current_convert(const std::uint8_t* frame, const std::uint32_t* palette,
                     int stride, std::vector<std::uint16_t>& output) {
    for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 320; ++x)
            output[y * 320 + x] = rgb565(palette[frame[(36 + y) * stride + 64 + x * 2]]);
}

void proportional_convert(const std::uint8_t* frame, const std::uint32_t* palette,
                          int stride, std::vector<std::uint16_t>& output) {
    static const auto x_map = [] {
        std::array<std::uint16_t, 320> result{};
        for (int x = 0; x < 320; ++x) result[x] = 128 + x * 512 / 320;
        return result;
    }();
    static const auto y_map = [] {
        std::array<std::uint16_t, 230> result{};
        for (int y = 0; y < 230; ++y) result[y] = 36 + y * 184 / 230;
        return result;
    }();
    for (int y = 0; y < 230; ++y)
        for (int x = 0; x < 320; ++x)
            output[y * 320 + x] = rgb565(palette[frame[y_map[y] * stride + x_map[x]]]);
}

template<class Function>
double median(Function function) {
    std::array<double, 5> samples{};
    for (auto& sample : samples) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i) function();
        sample = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count() / 1000.0;
    }
    std::sort(samples.begin(), samples.end());
    return samples[2];
}
}  // namespace

int main() {
    const auto snapshot = read_file("artifacts/stage4c1a-manic-miner/manic-miner-native.sna");
    if (snapshot.size() != 65792) return 2;
    cpcexplorer::select_engine(cpcexplorer::EngineKind::Fast);
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 3;
    for (int i = 0; i < 50; ++i) cpcexplorer::run_us(20'000);
    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    const int stride = cpcexplorer::framebuffer_width();
    std::vector<std::uint16_t> current(320 * 200);
    std::vector<std::uint16_t> proportional(320 * 230);
    const double current_us = median([&] { current_convert(frame, palette, stride, current); });
    const double proportional_us = median([&] {
        proportional_convert(frame, palette, stride, proportional);
    });
    std::uint64_t checksum = 0;
    for (const auto pixel : current) checksum += pixel;
    for (const auto pixel : proportional) checksum += pixel;
    std::printf("INDEXED_SCALE current_us=%.3f proportional_us=%.3f delta_us=%.3f "
                "ratio=%.3f pixels_current=%zu pixels_proportional=%zu checksum=%llu\n",
                current_us, proportional_us, proportional_us - current_us,
                proportional_us / current_us, current.size(), proportional.size(),
                static_cast<unsigned long long>(checksum));
    return 0;
}
