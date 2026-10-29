#include "cpc_core.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const auto size = in.tellg();
    if (size <= 0) return {};
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool valid_amsdos(const std::vector<std::uint8_t>& data, std::uint16_t load,
                  std::uint16_t length, std::uint16_t exec) {
    if (data.size() < 128U + length) return false;
    std::uint16_t sum = 0;
    for (std::size_t i = 0; i <= 66; ++i) sum = static_cast<std::uint16_t>(sum + data[i]);
    const auto u16 = [&data](std::size_t i) {
        return static_cast<std::uint16_t>(data[i] | (data[i + 1] << 8));
    };
    return sum == u16(67) && u16(21) == load && u16(24) == length && u16(26) == exec;
}

void run_seconds(int seconds) {
    for (int i = 0; i < seconds * 50; ++i) cpcexplorer::run_us(20'000);
}

std::uint64_t frame_hash() {
    const auto* data = cpcexplorer::framebuffer();
    const auto count = static_cast<std::size_t>(cpcexplorer::framebuffer_width()) *
                       cpcexplorer::framebuffer_height();
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < count; ++i) {
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
            static_cast<char>(abgr & 0xff), static_cast<char>((abgr >> 8) & 0xff),
            static_cast<char>((abgr >> 16) & 0xff)};
        out.write(rgb.data(), rgb.size());
    }
}

double benchmark_frame_conversion() {
    constexpr int iterations = 500;
    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    const int stride = cpcexplorer::framebuffer_width();
    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int y = 0; y < 200; ++y) {
            const auto* source = frame + (36 + y) * stride + 64;
            for (int x = 0; x < 320; ++x) {
                const std::uint32_t abgr = palette[source[x * 2]];
                checksum += ((abgr & 0x000000f8u) << 8) |
                            ((abgr & 0x0000fc00u) >> 5) |
                            ((abgr & 0x00f80000u) >> 19);
            }
        }
    }
    const auto elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("RENDER host_rgb565_us=%.1f checksum=%llu pixels=64000 bytes=128000\n",
        elapsed / iterations, static_cast<unsigned long long>(checksum));
    return elapsed / iterations;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: cpc_host_knight_tyme_test GAME_DIRECTORY\n");
        return 2;
    }
    const std::filesystem::path dir = argv[1];
    const auto bootstrap = read_file(dir / "KT$.BIN");
    const auto code = read_file(dir / "KTCODE.BIN");
    const auto graphics = read_file(dir / "KTGRX.BIN");
    if (!valid_amsdos(bootstrap, 0x1388, 8192, 0x1388) ||
        !valid_amsdos(code, 0x1388, 30000, 0x1388) ||
        !valid_amsdos(graphics, 0xC000, 11110, 0x0000)) {
        std::fprintf(stderr, "FAIL: supplied game file AMSDOS metadata/checksum mismatch\n");
        return 3;
    }

    cpcexplorer::init();
    run_seconds(5);

    // Reproduce the original loader's two deliberately sequential overlays.
    // quickload strips the 128-byte AMSDOS header and BASIC executes CALL &1388.
    if (!cpcexplorer::quickload(bootstrap.data(), bootstrap.size(), true)) return 4;
    run_seconds(8);
    const auto bootstrap_hash = frame_hash();
    write_ppm("knight-tyme-bootstrap.ppm");

    // KT$ holds the illustrated title until a key is pressed. Let it return
    // before KTCODE intentionally overwrites the same &1388 region.
    cpcexplorer::key_down(' ');
    cpcexplorer::run_us(50'000);
    cpcexplorer::key_up(' ');
    run_seconds(2);

    if (!cpcexplorer::quickload(code.data(), code.size(), false) ||
        !cpcexplorer::quickload(graphics.data(), graphics.size(), false) ||
        !cpcexplorer::quickload(code.data(), code.size(), true)) return 5;
    run_seconds(12);
    const auto game_hash = frame_hash();
    write_ppm("knight-tyme-game.ppm");
    const auto diag = cpcexplorer::diagnostics();
    std::printf("KNIGHT_TYME bootstrap_hash=%016llx game_hash=%016llx ticks=%llu pc=%04x\n",
        static_cast<unsigned long long>(bootstrap_hash),
        static_cast<unsigned long long>(game_hash),
        static_cast<unsigned long long>(diag.z80_ticks), diag.pc);
    if (game_hash == bootstrap_hash || diag.z80_ticks < 107'000'000) {
        std::fprintf(stderr, "FAIL: game did not advance to a distinct executing display state\n");
        return 6;
    }

    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return 7;
    {
        std::ofstream out("knight-tyme.sna", std::ios::binary);
        out.write(reinterpret_cast<const char*>(snapshot.data()),
                  static_cast<std::streamsize>(snapshot.size()));
    }
    cpcexplorer::reset();
    if (!cpcexplorer::quickload(snapshot.data(), snapshot.size(), false)) return 8;
    run_seconds(2);
    const auto reload_hash = frame_hash();
    write_ppm("knight-tyme-reloaded.ppm");
    std::printf("SNAPSHOT bytes=%zu reload_hash=%016llx pc=%04x\n", snapshot.size(),
        static_cast<unsigned long long>(reload_hash), cpcexplorer::diagnostics().pc);
    if (reload_hash == 0 || reload_hash == 1469598103934665603ull) return 9;
    if (benchmark_frame_conversion() <= 0.0) return 10;
    return 0;
}
