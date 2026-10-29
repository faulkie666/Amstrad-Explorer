#include "cpc_core.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
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

struct Bounds {
    int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1;
    std::size_t pixels = 0;
    void add(int x, int y) {
        if (x < x0) x0 = x;
        if (y < y0) y0 = y;
        if (x > x1) x1 = x;
        if (y > y1) y1 = y;
        ++pixels;
    }
};

void probe(cpcexplorer::EngineKind engine, const std::vector<std::uint8_t>& sna,
           const char* label) {
    cpcexplorer::select_engine(engine);
    cpcexplorer::init();
    if (!sna.empty() && !cpcexplorer::load_sna(sna.data(), sna.size())) {
        std::printf("%s load failed\n", cpcexplorer::engine_name(engine));
        return;
    }
    for (int i = 0; i < 50; ++i) cpcexplorer::run_us(20'000);

    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    const int stride = cpcexplorer::framebuffer_width();
    const std::uint8_t border = sna.empty() ? 20 : (sna[63] & 0x1f);
    Bounds sampled;
    const std::filesystem::path output = std::filesystem::path("artifacts") /
        (std::string("stage4f1-") + cpcexplorer::engine_name(engine) + "-" + label + "-lcd.ppm");
    std::ofstream ppm(output, std::ios::binary);
    ppm << "P6\n320 200\n255\n";
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const auto index = frame[(36 + y) * stride + 64 + x * 2];
            if (index != border) sampled.add(x, y);
            const std::uint32_t abgr = palette[index];
            const std::array<char, 3> rgb = {
                static_cast<char>(abgr), static_cast<char>(abgr >> 8),
                static_cast<char>(abgr >> 16)};
            ppm.write(rgb.data(), rgb.size());
        }
    }
    std::printf("GEOMETRY title=%s engine=%s fb=%dx%d border=%u nonborder=%zu "
                "bounds=%d,%d..%d,%d size=%dx%d\n",
                label, cpcexplorer::engine_name(engine), stride,
                cpcexplorer::framebuffer_height(), border, sampled.pixels,
                sampled.x0, sampled.y0, sampled.x1, sampled.y1,
                sampled.x1 >= sampled.x0 ? sampled.x1 - sampled.x0 + 1 : 0,
                sampled.y1 >= sampled.y0 ? sampled.y1 - sampled.y0 + 1 : 0);
}
}  // namespace

int main() {
    const auto sna = read_file("artifacts/stage4c1a-manic-miner/manic-miner-native.sna");
    if (sna.size() != 65792) return 2;
    probe(cpcexplorer::EngineKind::Reference, sna, "manic");
    probe(cpcexplorer::EngineKind::Fast, sna, "manic");
    probe(cpcexplorer::EngineKind::Reference, {}, "basic");
    probe(cpcexplorer::EngineKind::Fast, {}, "basic");
    return 0;
}
