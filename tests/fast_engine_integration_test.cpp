#include "cpc_core.h"
#include "display_fit.h"
#include "emulation/fast_engine_test_hooks.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const auto size = file.tellg();
    if (size <= 0) return {};
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.seekg(0); file.read(reinterpret_cast<char*>(data.data()), size);
    return file ? data : std::vector<std::uint8_t>{};
}

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

std::vector<std::uint8_t> frame() {
    const auto size = static_cast<std::size_t>(cpcexplorer::framebuffer_width()) *
                      cpcexplorer::framebuffer_height();
    return {cpcexplorer::framebuffer(), cpcexplorer::framebuffer() + size};
}

std::size_t difference(const std::vector<std::uint8_t>& a,
                       const std::vector<std::uint8_t>& b) {
    if (a.size() != b.size()) return static_cast<std::size_t>(-1);
    std::size_t count = 0;
    for (std::size_t i = 0; i < a.size(); ++i) count += a[i] != b[i];
    return count;
}

void run_frames(int count) {
    for (int i = 0; i < count; ++i) cpcexplorer::run_us(20'000);
}

void type_text(std::string_view text) {
    for (unsigned char ch : text) {
        cpcexplorer::key_down(ch); cpcexplorer::run_us(50'000);
        cpcexplorer::key_up(ch); cpcexplorer::run_us(100'000);
    }
}

struct Bounds {
    int min_x = 9999, min_y = 9999, max_x = -1, max_y = -1;
    std::size_t pixels = 0;
};

void include(Bounds& bounds, int x, int y) {
    bounds.min_x = std::min(bounds.min_x, x);
    bounds.min_y = std::min(bounds.min_y, y);
    bounds.max_x = std::max(bounds.max_x, x);
    bounds.max_y = std::max(bounds.max_y, y);
    ++bounds.pixels;
}

Bounds manic_player() {
    Bounds result{};
    const auto* pixels = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    const int stride = cpcexplorer::framebuffer_width();
    for (int y = 120; y < 160; ++y) {
        for (int x = 134; x < 204; ++x) {
            const auto colour = palette[pixels[y * stride + x]];
            if ((colour & 0xff) <= 200 || ((colour >> 8) & 0xff) <= 200 ||
                ((colour >> 16) & 0xff) <= 200) continue;
            include(result, x, y);
        }
    }
    return result;
}

Bounds lcd_non_border_bounds(const std::vector<std::uint8_t>& raster,
                             std::uint8_t border) {
    Bounds result{};
    const int stride = cpcexplorer::framebuffer_width();
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            if (raster[(36 + y) * stride + 64 + x * 2] != border)
                include(result, x, y);
        }
    }
    return result;
}

std::vector<std::uint8_t> snapshot_frame(cpcexplorer::EngineKind engine,
                                         const std::vector<std::uint8_t>& snapshot) {
    cpcexplorer::select_engine(engine);
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return {};
    run_frames(50);
    return frame();
}

struct Scenario { std::vector<std::uint8_t> raster; Bounds player; };

Scenario manic_scenario(const std::vector<std::uint8_t>& sna, int key) {
    cpcexplorer::select_engine(cpcexplorer::EngineKind::Fast);
    cpcexplorer::init();
    cpcexplorer::load_sna(sna.data(), sna.size());
    if (key) cpcexplorer::key_down(key);
    run_frames(15);
    if (key) cpcexplorer::key_up(key);
    run_frames(5);
    return {frame(), manic_player()};
}

struct BenchResult {
    double milliseconds;
    cpcexplorer::Diagnostics diagnostics;
};

BenchResult benchmark(cpcexplorer::EngineKind engine,
                      const std::vector<std::uint8_t>* snapshot,
                      cpcexplorer::testing::FastBenchmarkMode mode =
                          cpcexplorer::testing::FastBenchmarkMode::Full,
                      int frames = 50, bool audio_enabled = true) {
    cpcexplorer::select_engine(engine); cpcexplorer::init();
    if (snapshot) cpcexplorer::load_sna(snapshot->data(), snapshot->size());
    if (engine == cpcexplorer::EngineKind::Fast) {
        cpcexplorer::testing::set_fast_benchmark_mode(mode);
        cpcexplorer::testing::set_fast_audio_enabled(audio_enabled);
    }
    const auto start = Clock::now();
    run_frames(frames);
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return {elapsed, cpcexplorer::diagnostics()};
}

} // namespace

int main() {
    const auto manic = read_file(CPCEXPLORER_MANIC_SNA);
    const auto knight = read_file(CPCEXPLORER_KNIGHT_SNA);
    if (!expect(manic.size() == 65792 && knight.size() == 65792,
                "required 64 KiB CPC464 snapshots exist")) return 1;

    if (!expect(cpcexplorer::selected_engine() == cpcexplorer::EngineKind::Reference,
                "reference engine is the startup default") ||
        !expect(cpcexplorer::engine_available(cpcexplorer::EngineKind::Reference),
                "reference engine remains available") ||
        !expect(cpcexplorer::engine_available(cpcexplorer::EngineKind::Fast),
                "fast engine is compiled into integration candidate")) return 2;

    cpcexplorer::select_engine(cpcexplorer::EngineKind::Fast);
    cpcexplorer::init();
    int presentations = 0;
    for (int i = 0; i < 50; ++i) {
        cpcexplorer::run_us(20'000);
        presentations += cpcexplorer::consume_presented_frame() ? 1 : 0;
    }
    auto diag = cpcexplorer::diagnostics();
    if (!expect(diag.z80_ticks >= 4'000'000 && diag.z80_ticks <= 4'000'032,
                "50 x 20 ms preserves one second of 4 MHz CPC time") ||
        !expect(presentations == static_cast<int>(diag.reconstructed_frames),
                "presented and reconstructed frame counts agree") ||
        !expect(presentations == 12 || presentations == 25,
                "presentation is decoupled at a supported 12.5 or 25 Hz cadence") ||
        !expect(diag.peripheral_dispatches > 15'000 && diag.peripheral_dispatches < 17'000,
                "span scheduler dispatch count is bounded")) return 3;

    run_frames(200);
    const auto basic_ready = frame();
    type_text("PRINT 2+2\r"); run_frames(50);
    const auto basic_first = frame();
    type_text("PRINT 3+3\r"); run_frames(50);
    const auto basic_second = frame();
    const auto basic_geometry = cpcexplorer::display_geometry();
    const auto basic_fit = cpcexplorer::display::make_fit_plan({
        basic_geometry.x, basic_geometry.y, basic_geometry.width,
        basic_geometry.height, basic_geometry.valid});
    if (!expect(difference(basic_ready, basic_first) > 100,
                "BASIC accepts PRINT 2+2") ||
        !expect(difference(basic_first, basic_second) > 10,
                "BASIC accepts a repeated command") ||
        !expect(!basic_fit.active_fit && basic_fit.output_width == 320 &&
                basic_fit.output_height == 200,
                "BASIC retains the established 320x200 presentation")) return 4;

    const auto neutral = manic_scenario(manic, 0);
    const auto left = manic_scenario(manic, 'q');
    const auto right = manic_scenario(manic, 'w');
    const auto jump = manic_scenario(manic, cpcexplorer::KeyShift);
    const auto reference_manic_frame = snapshot_frame(cpcexplorer::EngineKind::Reference, manic);
    const auto fast_manic_frame = snapshot_frame(cpcexplorer::EngineKind::Fast, manic);
    const auto manic_geometry = cpcexplorer::display_geometry();
    const auto manic_fit = cpcexplorer::display::make_fit_plan({
        manic_geometry.x, manic_geometry.y, manic_geometry.width,
        manic_geometry.height, manic_geometry.valid});
    const auto reference_bounds = lcd_non_border_bounds(reference_manic_frame, manic[63] & 0x1f);
    const auto fast_bounds = lcd_non_border_bounds(fast_manic_frame, manic[63] & 0x1f);
    if (!expect(neutral.player.pixels >= 100, "Manic Miner Central Cavern player renders") ||
        !expect(left.player.min_x < neutral.player.min_x, "Q moves Manic Miner left") ||
        !expect(right.player.min_x > neutral.player.min_x, "W moves Manic Miner right") ||
        !expect(jump.player.min_y < neutral.player.min_y, "bare Shift makes Manic Miner jump") ||
        !expect(difference(neutral.raster, left.raster) < 1000 &&
                difference(neutral.raster, right.raster) < 1000,
                "movement does not corrupt the background") ||
        !expect(reference_bounds.min_x == 32 && reference_bounds.max_x == 287 &&
                fast_bounds.min_x == reference_bounds.min_x &&
                fast_bounds.max_x == reference_bounds.max_x,
                "fast reconstruction preserves reference Manic Miner centring") ||
        !expect(fast_bounds.max_x - fast_bounds.min_x + 1 == 256,
                "Manic Miner keeps its original 256-pixel source width") ||
        !expect(manic_fit.source_x == 32 && manic_fit.source_width == 256 &&
                manic_fit.source_height == 184 && manic_fit.output_x == 0 &&
                manic_fit.output_y == 5 && manic_fit.output_width == 320 &&
                manic_fit.output_height == 230,
                "Manic Miner active CRTC area maps to 320x230 without cropping")) return 5;

    cpcexplorer::init();
    if (!expect(cpcexplorer::load_sna(knight.data(), knight.size()),
                "Knight Tyme snapshot loads")) return 6;
    run_frames(50); const auto knight_menu = frame();
    cpcexplorer::key_down(' '); run_frames(5);
    cpcexplorer::key_up(' '); run_frames(8);
    const auto knight_after = frame();
    const auto knight_geometry = cpcexplorer::display_geometry();
    const auto knight_fit = cpcexplorer::display::make_fit_plan({
        knight_geometry.x, knight_geometry.y, knight_geometry.width,
        knight_geometry.height, knight_geometry.valid});
    if (!expect(difference(knight_menu, knight_after) > 10'000,
                "Knight Tyme Space advances its menu") ||
        !expect(knight_fit.source_x == 32 && knight_fit.source_width == 256 &&
                knight_fit.source_height == 200 && knight_fit.output_x == 6 &&
                knight_fit.output_y == 0 && knight_fit.output_width == 307 &&
                knight_fit.output_height == 240,
                "Knight Tyme current menu fits proportionally without clipping")) return 7;

    // Repeated launch/reset sequence represents returning to the host launcher:
    // no stale key or CPU state may leak into the next selected title.
    cpcexplorer::init();
    run_frames(250);
    const auto relaunched_basic = frame();
    if (!expect(relaunched_basic.size() == basic_ready.size() &&
                cpcexplorer::diagnostics().pc != 0,
                "subsequent BASIC launch is stable")) return 8;

    const auto reference_basic = benchmark(cpcexplorer::EngineKind::Reference, nullptr);
    const auto fast_basic = benchmark(cpcexplorer::EngineKind::Fast, nullptr);
    const auto reference_knight = benchmark(cpcexplorer::EngineKind::Reference, &knight);
    const auto fast_knight = benchmark(cpcexplorer::EngineKind::Fast, &knight);
    const auto reference_manic = benchmark(cpcexplorer::EngineKind::Reference, &manic);
    const auto fast_manic = benchmark(cpcexplorer::EngineKind::Fast, &manic);
    const auto component_cost = [&](const char* game,
                                    const std::vector<std::uint8_t>* snapshot) {
        constexpr int kFrames = 250;
        const auto cpu = benchmark(cpcexplorer::EngineKind::Fast, snapshot,
            cpcexplorer::testing::FastBenchmarkMode::CpuOnly, kFrames);
        const auto events = benchmark(cpcexplorer::EngineKind::Fast, snapshot,
            cpcexplorer::testing::FastBenchmarkMode::EventsWithoutPresentation, kFrames);
        const auto full = benchmark(cpcexplorer::EngineKind::Fast, snapshot,
            cpcexplorer::testing::FastBenchmarkMode::Full, kFrames);
        const double scale = 50.0 / kFrames;
        std::printf("ENGINE_COST game=%s emulated_ms=1000 cpu_ms=%.3f events_ms=%.3f reconstruct_ms=%.3f full_ms=%.3f\n",
            game, cpu.milliseconds * scale,
            std::max(0.0, events.milliseconds - cpu.milliseconds) * scale,
            std::max(0.0, full.milliseconds - events.milliseconds) * scale,
            full.milliseconds * scale);
    };
    std::printf("ENGINE_MEMORY reference_state=%zu fast_state=%zu incremental=%zu\n",
        reference_manic.diagnostics.state_bytes, fast_manic.diagnostics.state_bytes,
        fast_manic.diagnostics.state_bytes - reference_manic.diagnostics.state_bytes);
    std::printf("ENGINE_BENCH game=basic emulated_ms=1000 reference_ms=%.3f fast_ms=%.3f speedup=%.3f dispatch=%llu frames=%llu\n",
        reference_basic.milliseconds, fast_basic.milliseconds,
        reference_basic.milliseconds / fast_basic.milliseconds,
        static_cast<unsigned long long>(fast_basic.diagnostics.peripheral_dispatches),
        static_cast<unsigned long long>(fast_basic.diagnostics.reconstructed_frames));
    std::printf("ENGINE_BENCH game=knight_tyme emulated_ms=1000 reference_ms=%.3f fast_ms=%.3f speedup=%.3f dispatch=%llu frames=%llu\n",
        reference_knight.milliseconds, fast_knight.milliseconds,
        reference_knight.milliseconds / fast_knight.milliseconds,
        static_cast<unsigned long long>(fast_knight.diagnostics.peripheral_dispatches),
        static_cast<unsigned long long>(fast_knight.diagnostics.reconstructed_frames));
    std::printf("ENGINE_BENCH game=manic_miner emulated_ms=1000 reference_ms=%.3f fast_ms=%.3f speedup=%.3f dispatch=%llu frames=%llu\n",
        reference_manic.milliseconds, fast_manic.milliseconds,
        reference_manic.milliseconds / fast_manic.milliseconds,
        static_cast<unsigned long long>(fast_manic.diagnostics.peripheral_dispatches),
        static_cast<unsigned long long>(fast_manic.diagnostics.reconstructed_frames));
    component_cost("basic", nullptr);
    component_cost("knight_tyme", &knight);
    component_cost("manic_miner", &manic);
    const auto manic_without_audio = benchmark(cpcexplorer::EngineKind::Fast, &manic,
        cpcexplorer::testing::FastBenchmarkMode::Full, 250, false);
    const auto manic_with_audio = benchmark(cpcexplorer::EngineKind::Fast, &manic,
        cpcexplorer::testing::FastBenchmarkMode::Full, 250, true);
    std::printf("ENGINE_AUDIO_COST game=manic_miner emulated_ms=5000 disabled_ms=%.3f enabled_ms=%.3f overhead_pct=%.3f\n",
        manic_without_audio.milliseconds, manic_with_audio.milliseconds,
        100.0 * (manic_with_audio.milliseconds - manic_without_audio.milliseconds) /
            manic_without_audio.milliseconds);
#if !defined(CPCEXPLORER_FAST_RAM_VERIFY)
    if (!expect(reference_manic.milliseconds / fast_manic.milliseconds >= 4.5,
                "integrated fast engine retains the Stage 4E performance gain")) return 9;
#endif

    std::puts("PASS: explicit engines, 4 MHz time, decoupled presentation, three titles, repeated launches");
    return 0;
}
