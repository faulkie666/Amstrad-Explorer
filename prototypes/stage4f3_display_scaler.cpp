#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "display_fit.h"

namespace {
constexpr int kInputWidth = 320;
constexpr int kInputHeight = 200;
constexpr int kOutputWidth = 320;
constexpr int kOutputHeight = 230;
constexpr int kPanelHeight = 240;

std::vector<std::uint8_t> read_ppm(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    int width = 0, height = 0, maximum = 0;
    input >> magic >> width >> height >> maximum;
    input.get();
    if (!input || magic != "P6" || width != kInputWidth ||
        height != kInputHeight || maximum != 255) return {};
    std::vector<std::uint8_t> pixels(width * height * 3);
    input.read(reinterpret_cast<char*>(pixels.data()),
               static_cast<std::streamsize>(pixels.size()));
    return input ? pixels : std::vector<std::uint8_t>{};
}

void scale(const std::vector<std::uint8_t>& source,
           std::vector<std::uint8_t>& destination) {
    static const auto plan = cpcexplorer::display::make_fit_plan({32, 0, 256, 184, true});
    static const auto x_map = [&] {
        std::array<std::uint16_t, kOutputWidth> map{};
        for (int x = 0; x < kOutputWidth; ++x)
            map[x] = static_cast<std::uint16_t>(cpcexplorer::display::source_x_for(plan, x));
        return map;
    }();
    static const auto y_map = [&] {
        std::array<std::uint16_t, kOutputHeight> map{};
        for (int y = 0; y < kOutputHeight; ++y)
            map[y] = static_cast<std::uint16_t>(cpcexplorer::display::source_y_for(plan, y));
        return map;
    }();
    for (int y = 0; y < kOutputHeight; ++y) {
        for (int x = 0; x < kOutputWidth; ++x) {
            const auto source_offset = (y_map[y] * kInputWidth + x_map[x]) * 3;
            const auto destination_offset = (y * kOutputWidth + x) * 3;
            destination[destination_offset] = source[source_offset];
            destination[destination_offset + 1] = source[source_offset + 1];
            destination[destination_offset + 2] = source[source_offset + 2];
        }
    }
}

void current_view(const std::vector<std::uint8_t>& source,
                  std::vector<std::uint8_t>& destination) {
    for (int y = 0; y < kInputHeight; ++y) {
        for (int x = 0; x < kInputWidth; ++x) {
            const auto offset = (y * kInputWidth + x) * 3;
            destination[offset] = source[offset];
            destination[offset + 1] = source[offset + 1];
            destination[offset + 2] = source[offset + 2];
        }
    }
}

bool write_panel(const char* path, const std::vector<std::uint8_t>& scaled) {
    std::vector<std::uint8_t> panel(kOutputWidth * kPanelHeight * 3);
    constexpr int top = (kPanelHeight - kOutputHeight) / 2;
    std::copy(scaled.begin(), scaled.end(), panel.begin() + top * kOutputWidth * 3);
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kOutputWidth << ' ' << kPanelHeight << "\n255\n";
    output.write(reinterpret_cast<const char*>(panel.data()),
                 static_cast<std::streamsize>(panel.size()));
    return static_cast<bool>(output);
}
}  // namespace

int main() {
    const auto input = read_ppm("artifacts/stage4f1-fast-manic-lcd.ppm");
    if (input.empty()) return 2;
    std::vector<std::uint8_t> output(kOutputWidth * kOutputHeight * 3);
    std::vector<std::uint8_t> current(kInputWidth * kInputHeight * 3);
    std::array<double, 5> samples{}, current_samples{};
    std::uint64_t checksum = 0;
    for (std::size_t run = 0; run < samples.size(); ++run) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i) scale(input, output);
        samples[run] = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count() / 1000.0;
        const auto current_start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i) current_view(input, current);
        current_samples[run] = std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - current_start).count() / 1000.0;
        checksum += output[(static_cast<std::size_t>(samples[run]) + checksum) % output.size()];
        checksum += current[(static_cast<std::size_t>(current_samples[run]) + checksum) % current.size()];
    }
    std::sort(samples.begin(), samples.end());
    std::sort(current_samples.begin(), current_samples.end());
    if (!write_panel("artifacts/stage4f5-manic-active-fit-320x230.ppm", output)) return 3;
    std::printf("DISPLAY_SCALE source=%dx%d crop=%d,%d,%dx%d output=%dx%d panel=%dx%d "
                "top_bottom=%d median_host_us=%.3f current_host_us=%.3f ratio=%.3f "
                "output_pixels=%d current_pixels=%d increase_pct=%.1f checksum=%llu\n",
                kInputWidth, kInputHeight, 32, 0, 256,
                184, kOutputWidth, kOutputHeight, kOutputWidth,
                kPanelHeight, (kPanelHeight - kOutputHeight) / 2, samples[2],
                current_samples[2], samples[2] / current_samples[2],
                kOutputWidth * kOutputHeight, kInputWidth * kInputHeight,
                100.0 * (kOutputHeight - kInputHeight) / kInputHeight,
                static_cast<unsigned long long>(checksum));
    return 0;
}
