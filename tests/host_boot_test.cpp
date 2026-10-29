#include "cpc_core.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>

namespace {

std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

void write_ppm(const std::filesystem::path& path) {
    const int width = cpcexplorer::framebuffer_width();
    const int height = cpcexplorer::framebuffer_height();
    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        const std::uint32_t abgr = palette[frame[i]];
        const std::array<char, 3> rgb = {
            static_cast<char>(abgr & 0xff),
            static_cast<char>((abgr >> 8) & 0xff),
            static_cast<char>((abgr >> 16) & 0xff),
        };
        out.write(rgb.data(), rgb.size());
    }
}

void type_text(std::string_view text) {
    for (const unsigned char key : text) {
        cpcexplorer::key_down(key);
        cpcexplorer::run_us(50'000);
        cpcexplorer::key_up(key);
        cpcexplorer::run_us(100'000);
    }
}

struct RasterEvidence {
    std::uint64_t hash;
    int used_colours;
    std::size_t horizontal_edges;
};

RasterEvidence inspect_raster() {
    const int width = cpcexplorer::framebuffer_width();
    const int height = cpcexplorer::framebuffer_height();
    const auto* frame = cpcexplorer::framebuffer();
    std::array<bool, 64> colours{};
    std::size_t edges = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = frame[y * width + x];
            if (pixel < colours.size()) colours[pixel] = true;
            if (x && pixel != frame[y * width + x - 1]) ++edges;
        }
    }
    int count = 0;
    for (bool used : colours) count += used ? 1 : 0;
    return {fnv1a(frame, static_cast<std::size_t>(width * height)), count, edges};
}

}  // namespace

int main() {
    constexpr std::uint64_t kExpectedReadyRaster = 0xde41d6c8715908e3ull;
    constexpr std::uint64_t kExpectedPrintRaster = 0xfd9ba9b5fa500423ull;
    cpcexplorer::init();
    for (int i = 0; i < 250; ++i) cpcexplorer::run_us(20'000);  // 5 s emulated time

    const auto ready = inspect_raster();
    write_ppm("cpc464-ready.ppm");
    auto diag = cpcexplorer::diagnostics();
    std::printf("BOOT ticks=%llu pc=%04x state=%zu framebuffer=%zu ram=%zu colours=%d edges=%zu hash=%016llx\n",
        static_cast<unsigned long long>(diag.z80_ticks), diag.pc, diag.state_bytes,
        diag.framebuffer_bytes, diag.ram_bytes, ready.used_colours, ready.horizontal_edges,
        static_cast<unsigned long long>(ready.hash));

    if (diag.z80_ticks < 19'000'000 || ready.used_colours < 2 ||
        ready.horizontal_edges < 1000 || ready.hash != kExpectedReadyRaster) {
        std::fprintf(stderr, "FAIL: firmware did not produce a plausible CPC startup raster\n");
        return 1;
    }

    /* A shifted CPC key can overlap the immediately following synthetic key.
       Type and delete a sacrificial character after '+', then enter the final
       digit.  The command that BASIC receives is exactly PRINT 2+2. */
    type_text("PRINT 2+");
    type_text("x");
    type_text("\x01");
    type_text("2");
    type_text("\r");
    for (int i = 0; i < 50; ++i) cpcexplorer::run_us(20'000);
    const auto result = inspect_raster();
    write_ppm("cpc464-print-2-plus-2.ppm");
    diag = cpcexplorer::diagnostics();
    std::printf("PRINT ticks=%llu pc=%04x colours=%d edges=%zu hash=%016llx\n",
        static_cast<unsigned long long>(diag.z80_ticks), diag.pc, result.used_colours,
        result.horizontal_edges, static_cast<unsigned long long>(result.hash));

    if (result.hash != kExpectedPrintRaster || result.horizontal_edges <= ready.horizontal_edges) {
        std::fprintf(stderr, "FAIL: PRINT 2+2 raster does not contain the verified result 4\n");
        return 2;
    }
    std::puts("PASS: original ROM executed and both raster captures came from emulated CPC video state");
    return 0;
}
