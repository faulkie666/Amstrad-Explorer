#include "cpc_core.h"
#include "display/st7789.h"

#include <array>
#include <cstdint>
#include <cstdio>

#include "pico/stdlib.h"

namespace {

constexpr int kLcdWidth = 320;
constexpr int kLcdHeight = 240;
constexpr int kCpcWidth = 320;
constexpr int kCpcHeight = 200;
constexpr int kTopBorder = (kLcdHeight - kCpcHeight) / 2;
constexpr int kSourceX = 64;
constexpr int kSourceY = 36;

std::array<std::uint8_t, kLcdWidth * 2> lines[2];

void fill_black() {
    std::array<std::uint8_t, kLcdWidth * 2> black{};
    st7789_set_window(0, 0, kLcdWidth, kLcdHeight);
    st7789_start_write();
    for (int y = 0; y < kLcdHeight; ++y) st7789_write_dma(black.data(), black.size());
    st7789_end_write();
}

void render_cpc() {
    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    const int stride = cpcexplorer::framebuffer_width();
    st7789_set_window(0, kTopBorder, kCpcWidth, kCpcHeight);
    st7789_start_write();
    for (int y = 0; y < kCpcHeight; ++y) {
        auto& line = lines[y & 1];
        const auto* source = frame + (kSourceY + y) * stride + kSourceX;
        for (int x = 0; x < kCpcWidth; ++x) {
            const std::uint32_t abgr = palette[source[x * 2]];
            const std::uint16_t rgb565 = static_cast<std::uint16_t>(
                ((abgr & 0x000000f8u) << 8) |
                ((abgr & 0x0000fc00u) >> 5) |
                ((abgr & 0x00f80000u) >> 19));
            line[x * 2] = static_cast<std::uint8_t>(rgb565 >> 8);
            line[x * 2 + 1] = static_cast<std::uint8_t>(rgb565);
        }
        st7789_write_dma(line.data(), line.size());
    }
    st7789_end_write();
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(1500);
    std::printf("CPC Explorer Stage 1B starting (RP2350B, 150 MHz nominal)\n");
    cpcexplorer::init();
    const auto diag = cpcexplorer::diagnostics();
    std::printf("state=%u framebuffer=%u ram=%u bytes\n",
        static_cast<unsigned>(diag.state_bytes), static_cast<unsigned>(diag.framebuffer_bytes),
        static_cast<unsigned>(diag.ram_bytes));

    st7789_init(kLcdWidth, kLcdHeight, ST7789_ROTATE_0, false, ST7789_COLOR_RGB565);
    fill_black();

    absolute_time_t next = get_absolute_time();
    while (true) {
        next = delayed_by_ms(next, 20);
        cpcexplorer::run_us(20'000);
        render_cpc();
        sleep_until(next);
    }
}
