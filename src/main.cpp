#include "cpc_core.h"
#include "display/st7789.h"
#include "display_fit.h"
#include "game_library.h"
#include "gameplay_input.h"
#include "launcher.h"
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
#include "performance_metrics.h"
#endif
#if defined(CPCEXPLORER_LIBRARY_UPLOAD)
#include "library_installer.h"
#include "pico_flash_backend.h"
#include "upload_protocol.h"
#endif

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#endif

namespace {

constexpr int kLcdWidth = 320;
constexpr int kLcdHeight = 240;
constexpr int kSourceX = 64;
constexpr int kSourceY = 36;

constexpr std::uint16_t rgb565(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return static_cast<std::uint16_t>(((r & 0xf8U) << 8) | ((g & 0xfcU) << 3) | (b >> 3));
}

constexpr std::uint16_t kBlack = rgb565(0, 0, 0);
constexpr std::uint16_t kCpcBlue = rgb565(0, 0, 92);
constexpr std::uint16_t kPanelBlue = rgb565(0, 28, 120);
constexpr std::uint16_t kYellow = rgb565(255, 232, 32);
constexpr std::uint16_t kWhite = rgb565(238, 246, 255);
constexpr std::uint16_t kLightBlue = rgb565(96, 204, 255);

std::array<std::uint8_t, kLcdWidth * 2> lines[2];

#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
struct RenderTiming {
    std::uint32_t conversion_us = 0;
    std::uint32_t lcd_wait_us = 0;
    std::uint32_t total_us = 0;
};
RenderTiming last_render_timing;
bool performance_measurement_active = false;
#endif

void set_pixel(std::array<std::uint8_t, kLcdWidth * 2>& line, int x, std::uint16_t color) {
    line[x * 2] = static_cast<std::uint8_t>(color >> 8);
    line[x * 2 + 1] = static_cast<std::uint8_t>(color);
}

void fill_line(std::array<std::uint8_t, kLcdWidth * 2>& line, int width,
               std::uint16_t color) {
    for (int x = 0; x < width; ++x) set_pixel(line, x, color);
}

void fill_rect(int x, int y, int width, int height, std::uint16_t color) {
    if ((width <= 0) || (height <= 0)) return;
    fill_line(lines[0], width, color);
    fill_line(lines[1], width, color);
    st7789_set_window(static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
                      static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
    st7789_start_write();
    for (int row = 0; row < height; ++row) {
        st7789_write_dma(lines[row & 1].data(), static_cast<std::size_t>(width * 2));
    }
    st7789_end_write();
}

struct Glyph {
    char character;
    std::uint8_t rows[7];
};

constexpr Glyph kFont[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}}, {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {6, 8, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 2, 12}},
    {'=', {0, 0, 31, 0, 31, 0, 0}}, {'>', {16, 8, 4, 2, 4, 8, 16}},
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}}, {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
};

const std::uint8_t* glyph_rows(char character) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    for (const auto& glyph : kFont) {
        if (glyph.character == upper) return glyph.rows;
    }
    return kFont[0].rows;
}

int text_width(const char* text, int scale) {
    const int count = static_cast<int>(std::strlen(text));
    return count ? count * 6 * scale - scale : 0;
}

void draw_text(const char* text, int x, int y, int scale, std::uint16_t foreground,
               std::uint16_t background) {
    const int width = text_width(text, scale);
    const int height = 7 * scale;
    if ((width <= 0) || (width > kLcdWidth)) return;
    st7789_set_window(static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
                      static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height));
    st7789_start_write();
    for (int pixel_y = 0; pixel_y < height; ++pixel_y) {
        auto& line = lines[pixel_y & 1];
        fill_line(line, width, background);
        int cursor = 0;
        for (const char* current = text; *current; ++current) {
            const auto* rows = glyph_rows(*current);
            for (int pixel_x = 0; pixel_x < 5 * scale; ++pixel_x) {
                const int font_x = pixel_x / scale;
                const int font_y = pixel_y / scale;
                if ((rows[font_y] & (1U << (4 - font_x))) != 0) {
                    set_pixel(line, cursor + pixel_x, foreground);
                }
            }
            cursor += 6 * scale;
        }
        st7789_write_dma(line.data(), static_cast<std::size_t>(width * 2));
    }
    st7789_end_write();
}

void draw_centered(const char* text, int y, int scale, std::uint16_t foreground,
                   std::uint16_t background) {
    draw_text(text, (kLcdWidth - text_width(text, scale)) / 2, y, scale, foreground, background);
}

void fill_black() { fill_rect(0, 0, kLcdWidth, kLcdHeight, kBlack); }

void render_cpc() {
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
    const bool measure = performance_measurement_active;
    const std::uint64_t render_start_us = measure ? time_us_64() : 0;
    last_render_timing = {};
    std::uint64_t operation_start_us = measure ? time_us_64() : 0;
#endif
    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    const int stride = cpcexplorer::framebuffer_width();
#if defined(CPCEXPLORER_ACTIVE_DISPLAY_FIT)
    const auto geometry = cpcexplorer::display_geometry();
    const auto plan = cpcexplorer::display::make_fit_plan({
        geometry.x, geometry.y, geometry.width, geometry.height, geometry.valid});
#else
    const cpcexplorer::display::FitPlan plan{};
#endif
    static cpcexplorer::display::FitPlan previous_plan{};
    static bool have_previous_plan = false;
    static std::array<std::uint16_t, kLcdWidth> source_x{};
    static std::array<std::uint16_t, kLcdHeight> source_y{};
    if (!have_previous_plan || !(plan == previous_plan)) {
        // Clear pixels no longer covered when a title changes CRTC geometry.
        // This runs only on a geometry transition, not in the steady frame path.
        fill_black();
        for (int x = 0; x < plan.output_width; ++x)
            source_x[static_cast<std::size_t>(x)] = static_cast<std::uint16_t>(
                cpcexplorer::display::source_x_for(plan, x));
        for (int y = 0; y < plan.output_height; ++y)
            source_y[static_cast<std::size_t>(y)] = static_cast<std::uint16_t>(
                cpcexplorer::display::source_y_for(plan, y));
        previous_plan = plan;
        have_previous_plan = true;
    }
    st7789_set_window(static_cast<std::uint16_t>(plan.output_x),
                      static_cast<std::uint16_t>(plan.output_y),
                      static_cast<std::uint16_t>(plan.output_width),
                      static_cast<std::uint16_t>(plan.output_height));
    st7789_start_write();
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
    if (measure) last_render_timing.lcd_wait_us +=
        static_cast<std::uint32_t>(time_us_64() - operation_start_us);
#endif
    for (int y = 0; y < plan.output_height; ++y) {
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        if (measure) operation_start_us = time_us_64();
#endif
        auto& line = lines[y & 1];
        const auto* source = frame +
            (kSourceY + source_y[static_cast<std::size_t>(y)]) * stride + kSourceX;
        for (int x = 0; x < plan.output_width; ++x) {
            const std::uint32_t abgr = palette[
                source[source_x[static_cast<std::size_t>(x)] * 2]];
            const std::uint16_t color = static_cast<std::uint16_t>(
                ((abgr & 0x000000f8u) << 8) |
                ((abgr & 0x0000fc00u) >> 5) |
                ((abgr & 0x00f80000u) >> 19));
            set_pixel(line, x, color);
        }
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        if (measure) {
            last_render_timing.conversion_us +=
                static_cast<std::uint32_t>(time_us_64() - operation_start_us);
            operation_start_us = time_us_64();
        }
#endif
        st7789_write_dma(line.data(), static_cast<std::size_t>(plan.output_width * 2));
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        if (measure) last_render_timing.lcd_wait_us +=
            static_cast<std::uint32_t>(time_us_64() - operation_start_us);
#endif
    }
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
    if (measure) operation_start_us = time_us_64();
#endif
    st7789_end_write();
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
    if (measure) {
        last_render_timing.lcd_wait_us +=
            static_cast<std::uint32_t>(time_us_64() - operation_start_us);
        last_render_timing.total_us =
            static_cast<std::uint32_t>(time_us_64() - render_start_us);
    }
#endif
}

#if defined(CPCEXPLORER_MENU) || defined(CPCEXPLORER_LIBRARY)
constexpr int kEntryTop = 122;
constexpr int kEntryHeight = 30;
constexpr std::size_t kVisibleEntries = 3;

void draw_entry(std::size_t entry_index, std::size_t row, bool selected) {
    const int y = kEntryTop + static_cast<int>(row) * kEntryHeight;
    const std::uint16_t background = selected ? kYellow : kPanelBlue;
    const std::uint16_t foreground = selected ? kCpcBlue : kWhite;
    fill_rect(34, y, 252, 24, background);
    char label[44]{};
    std::snprintf(label, sizeof(label), "%c %s", selected ? '>' : ' ',
                  cpcexplorer::launcher::entry(entry_index).display_name);
    const int scale = text_width(label, 2) <= 224 ? 2 : 1;
    draw_text(label, 48, y + (24 - 7 * scale) / 2, scale, foreground, background);
}

void draw_entries(const cpcexplorer::launcher::MenuController& menu) {
    fill_rect(34, kEntryTop, 252, static_cast<int>(kVisibleEntries) * kEntryHeight, kCpcBlue);
    const std::size_t first = menu.first_visible(kVisibleEntries);
    for (std::size_t row = 0; row < kVisibleEntries; ++row) {
        const std::size_t index = first + row;
        if (index >= cpcexplorer::launcher::entry_count()) break;
        draw_entry(index, row, index == menu.selected());
    }
}

void draw_menu(const cpcexplorer::launcher::MenuController& menu) {
    fill_rect(0, 0, kLcdWidth, kLcdHeight, kCpcBlue);
    draw_centered("AMSTRAD CPC", 18, 3, kYellow, kCpcBlue);
    draw_centered("EXPLORER", 46, 3, kLightBlue, kCpcBlue);
    draw_centered("Select a system or game", 86, 2, kWhite, kCpcBlue);
    fill_rect(34, 110, 252, 2, kLightBlue);
    draw_entries(menu);
    draw_centered("X = UP   Z = DOWN   Y = SELECT", 224, 1, kYellow, kCpcBlue);
}

std::uint8_t read_menu_buttons() {
    std::uint8_t pressed = 0;
    if (!gpio_get(PIMORONI_EXPLORER_SWITCH_X_PIN)) pressed |= cpcexplorer::launcher::ButtonX;
    if (!gpio_get(PIMORONI_EXPLORER_SWITCH_Y_PIN)) pressed |= cpcexplorer::launcher::ButtonY;
    if (!gpio_get(PIMORONI_EXPLORER_SWITCH_Z_PIN)) pressed |= cpcexplorer::launcher::ButtonZ;
    return pressed;
}

void init_menu_buttons() {
    constexpr unsigned pins[] = {
        PIMORONI_EXPLORER_SWITCH_X_PIN,
        PIMORONI_EXPLORER_SWITCH_Y_PIN,
        PIMORONI_EXPLORER_SWITCH_Z_PIN,
    };
    for (unsigned pin : pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
}

void apply_gameplay_input(cpcexplorer::input::Profile profile,
                          const cpcexplorer::input::Transition& transition) {
    const auto keys = cpcexplorer::input::bindings(profile);
    const auto release = [&](std::uint8_t mask, int key) {
        if ((transition.released & mask) != 0) cpcexplorer::key_up(key);
    };
    const auto press = [&](std::uint8_t mask, int key) {
        if ((transition.pressed & mask) != 0) cpcexplorer::key_down(key);
    };
    release(cpcexplorer::input::KeyLeft, keys.left);
    release(cpcexplorer::input::KeyRight, keys.right);
    release(cpcexplorer::input::KeyJump, keys.action);
    press(cpcexplorer::input::KeyLeft, keys.left);
    press(cpcexplorer::input::KeyRight, keys.right);
    press(cpcexplorer::input::KeyJump, keys.action);
}

#if defined(CPCEXPLORER_LIBRARY_UPLOAD)
void draw_upload_progress(std::uint32_t received, std::uint32_t expected) {
    char status[28]{};
    const unsigned percent = expected == 0 ? 0 :
        static_cast<unsigned>((static_cast<std::uint64_t>(received) * 100U) / expected);
    std::snprintf(status, sizeof(status), "USB UPLOAD %u%%", percent);
    fill_rect(0, 212, kLcdWidth, 11, kCpcBlue);
    draw_centered(status, 214, 1, kYellow, kCpcBlue);
}
#endif
#endif

[[noreturn]] void run_emulator(const cpcexplorer::launcher::Entry& entry) {
#if defined(CPCEXPLORER_USE_FAST_ENGINE)
    if (!cpcexplorer::select_engine(cpcexplorer::EngineKind::Fast)) {
        std::printf("engine=fast unavailable; holding on reference\n");
    }
#endif
    cpcexplorer::init();
    bool loaded = true;
    if (entry.type == cpcexplorer::launcher::EntryType::Snapshot) {
        if (entry.snapshot_data != nullptr) {
            loaded = cpcexplorer::load_sna(entry.snapshot_data, entry.snapshot_size);
        } else {
            loaded = (entry.snapshot_id != nullptr) &&
                     (std::strcmp(entry.snapshot_id, "knight_tyme") == 0) &&
                     cpcexplorer::load_builtin_knight_tyme();
        }
    }
    std::printf("startup=%s loaded=%s engine=%s\n", entry.display_name, loaded ? "yes" : "no",
                cpcexplorer::engine_name(cpcexplorer::selected_engine()));
    if (!loaded) {
        fill_rect(0, 0, kLcdWidth, kLcdHeight, kCpcBlue);
        draw_centered("SNAPSHOT LOAD FAILED", 108, 2, kYellow, kCpcBlue);
        while (true) tight_loop_contents();
    }

    const auto diag = cpcexplorer::diagnostics();
    std::printf("state=%u framebuffer=%u ram=%u bytes\n",
        static_cast<unsigned>(diag.state_bytes), static_cast<unsigned>(diag.framebuffer_bytes),
        static_cast<unsigned>(diag.ram_bytes));
    fill_black();
#if defined(CPCEXPLORER_MENU) || defined(CPCEXPLORER_LIBRARY)
    cpcexplorer::input::Adapter gameplay_input;
    const auto gameplay_profile = cpcexplorer::input::profile_for_game(
        entry.snapshot_id == nullptr ? "" : entry.snapshot_id);
    gameplay_input.reset(gameplay_profile, read_menu_buttons());
#endif
    absolute_time_t next = get_absolute_time();
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
    cpcexplorer::performance::Aggregator performance;
    performance.reset(time_us_64());
    bool measurement_active = false;
#endif
    while (true) {
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        const bool usb_diagnostics_enabled = stdio_usb_connected();
        if (usb_diagnostics_enabled && !measurement_active) performance.reset(time_us_64());
        measurement_active = usb_diagnostics_enabled;
        performance_measurement_active = measurement_active;
        cpcexplorer::set_diagnostics_enabled(measurement_active);
        const std::uint64_t iteration_start_us = measurement_active ? time_us_64() : 0;
#endif
        next = delayed_by_ms(next, 20);
#if defined(CPCEXPLORER_MENU) || defined(CPCEXPLORER_LIBRARY)
        apply_gameplay_input(gameplay_profile, gameplay_input.update(read_menu_buttons()));
#endif
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        const std::uint64_t execution_start_us = measurement_active ? time_us_64() : 0;
#endif
        cpcexplorer::run_us(20'000);
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        const std::uint32_t execution_us = measurement_active
            ? static_cast<std::uint32_t>(time_us_64() - execution_start_us) : 0;
        const auto engine_timing = measurement_active
            ? cpcexplorer::diagnostics() : cpcexplorer::Diagnostics{};
#endif
        const bool presented_frame = cpcexplorer::consume_presented_frame();
        if (presented_frame) render_cpc();
        else last_render_timing = {};
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        const std::uint64_t work_end_us = measurement_active ? time_us_64() : 0;
        const bool missed_deadline = measurement_active && work_end_us > to_us_since_boot(next);
#endif
        sleep_until(next);
#if defined(CPCEXPLORER_PERF_DIAGNOSTICS)
        const std::uint64_t iteration_end_us = measurement_active ? time_us_64() : 0;
        if (measurement_active) performance.add({
            .emulated_us = engine_timing.last_emulated_us,
            .execution_us = execution_us,
            .cpu_us = engine_timing.last_cpu_us,
            .event_us = engine_timing.last_event_us,
            .reconstruction_us = engine_timing.last_reconstruction_us,
            .conversion_us = last_render_timing.conversion_us,
            .lcd_wait_us = last_render_timing.lcd_wait_us,
            .render_us = last_render_timing.total_us,
            .work_us = static_cast<std::uint32_t>(work_end_us - iteration_start_us),
            .iteration_us = static_cast<std::uint32_t>(iteration_end_us - iteration_start_us),
            .presented_frame = presented_frame,
            .missed_deadline = missed_deadline,
        });
        if (measurement_active && performance.report_due(iteration_end_us)) {
            const auto report = performance.take_report(iteration_end_us);
            // Opening the USB CDC port (DTR asserted) explicitly enables output.
            // Diagnostic-only build timeouts cap a stalled host to a few ms.
            if (stdio_usb_connected()) {
                // Keep the bounded telemetry buffer out of the 2 KiB core-0
                // stack reservation. Only this emulator loop can access it.
                static char telemetry[640]{};
                std::memset(telemetry, 0, sizeof(telemetry));
                const int length = std::snprintf(
                    telemetry, sizeof(telemetry),
                    "CPCPERF slices=%u presented=%u interval_us=%llu emu_us=%llu "
                    "exec_avg=%u exec_max=%u cpu_avg=%u cpu_max=%u "
                    "event_avg=%u event_max=%u recon_avg=%u recon_max=%u "
                    "conv_avg=%u conv_max=%u lcdwait_avg=%u lcdwait_max=%u "
                    "render_avg=%u render_max=%u "
                    "work_avg=%u work_max=%u loop_avg=%u loop_max=%u missed=%u "
                    "speed_pct_x10=%u fps_x10=%u\n",
                    static_cast<unsigned>(report.slices),
                    static_cast<unsigned>(report.presented_frames),
                    static_cast<unsigned long long>(report.interval_us),
                    static_cast<unsigned long long>(report.emulated_total_us),
                    static_cast<unsigned>(report.execution_average_us),
                    static_cast<unsigned>(report.execution_maximum_us),
                    static_cast<unsigned>(report.cpu_average_us),
                    static_cast<unsigned>(report.cpu_maximum_us),
                    static_cast<unsigned>(report.event_average_us),
                    static_cast<unsigned>(report.event_maximum_us),
                    static_cast<unsigned>(report.reconstruction_average_us),
                    static_cast<unsigned>(report.reconstruction_maximum_us),
                    static_cast<unsigned>(report.conversion_average_us),
                    static_cast<unsigned>(report.conversion_maximum_us),
                    static_cast<unsigned>(report.lcd_wait_average_us),
                    static_cast<unsigned>(report.lcd_wait_maximum_us),
                    static_cast<unsigned>(report.render_average_us),
                    static_cast<unsigned>(report.render_maximum_us),
                    static_cast<unsigned>(report.work_average_us),
                    static_cast<unsigned>(report.work_maximum_us),
                    static_cast<unsigned>(report.iteration_average_us),
                    static_cast<unsigned>(report.iteration_maximum_us),
                    static_cast<unsigned>(report.missed_deadlines),
                    static_cast<unsigned>(report.effective_speed_tenths_percent),
                    static_cast<unsigned>(report.presentation_fps_tenths));
                if (length > 0) {
                    const int bounded_length = length < static_cast<int>(sizeof(telemetry))
                        ? length : static_cast<int>(sizeof(telemetry) - 1);
                    stdio_put_string(telemetry, bounded_length, false, false);
                }
            }
        }
#endif
    }
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(1500);
    st7789_init(kLcdWidth, kLcdHeight, ST7789_ROTATE_0, false, ST7789_COLOR_RGB565);

#if defined(CPCEXPLORER_MENU) || defined(CPCEXPLORER_LIBRARY)
#if defined(CPCEXPLORER_LIBRARY)
    #if defined(CPCEXPLORER_LIBRARY_UPLOAD)
    static cpcexplorer::storage::PicoFlash physical_flash;
    const bool flash_ready = physical_flash.probe();
    static cpcexplorer::storage::Installer installer(physical_flash);
    #if defined(CPCEXPLORER_LIBRARY_REMOVE)
    const bool recovery_ready = flash_ready && installer.recover_pending_removal();
    #else
    const bool recovery_ready = true;
    #endif
    const bool storage_ready = flash_ready && recovery_ready;
    static cpcexplorer::upload::Session upload_session;
    static cpcexplorer::upload::StreamDecoder decoder;
    std::uint32_t upload_activity_ms = 0;
    std::uint32_t handshake_ms = 0;
    bool handshake = false;
    #endif
    cpcexplorer::launcher::configure_library(
        reinterpret_cast<const std::uint8_t*>(cpcexplorer::library::kLibraryXipAddress),
        cpcexplorer::library::kLibraryBytes);
    // Use the same Pico SDK program version embedded in binary_info. Keeping a
    // second preprocessor identity ladder here allowed feature macros such as
    // PERF_DIAGNOSTICS to mislabel newer firmware as 4C.2-DIAG.
    std::printf("CPC Explorer firmware %s upload library starting (entries=%u)\n",
                PICO_PROGRAM_VERSION_STRING,
                static_cast<unsigned>(cpcexplorer::launcher::entry_count()));
    #if defined(CPCEXPLORER_LIBRARY_UPLOAD)
    std::printf("flash=%s capacity=%u upload=%s\n", physical_flash.identity(),
                static_cast<unsigned>(physical_flash.capacity()), storage_ready ? "enabled" : "disabled");
    #endif
#else
    std::printf("CPC Explorer Stage 3 menu starting (RP2350B, 150 MHz nominal)\n");
#endif
    init_menu_buttons();
    cpcexplorer::launcher::MenuController menu;
    menu.reset(read_menu_buttons(), to_ms_since_boot(get_absolute_time()));
    draw_menu(menu);
    while (true) {
        #if defined(CPCEXPLORER_LIBRARY_UPLOAD)
        const std::uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        int incoming = 0;
        while ((incoming = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
            cpcexplorer::upload::Frame frame{};
            const auto decoded = decoder.feed(static_cast<std::uint8_t>(incoming), now_ms, frame);
            if (decoded == cpcexplorer::upload::DecodeResult::Invalid) {
                std::printf("CPCU1 ERROR invalid_frame\n"); std::fflush(stdout);
            } else if (decoded == cpcexplorer::upload::DecodeResult::Ready) {
                using cpcexplorer::upload::Command;
                using cpcexplorer::upload::Result;
                if (frame.command == Command::Hello) {
                    handshake = true; handshake_ms = now_ms;
                    std::printf("CPCU1 READY fw=%s flash=%s capacity=%u entries=%u\n",
                        PICO_PROGRAM_VERSION_STRING,
                        physical_flash.identity(), static_cast<unsigned>(physical_flash.capacity()),
                        static_cast<unsigned>(cpcexplorer::launcher::entry_count()));
                } else if (frame.command == Command::Status) {
                    std::printf("CPCU1 STATUS upload=%s flash=%s capacity=%u entries=%u received=%u\n",
                        upload_session.active() ? "active" : "idle", physical_flash.identity(),
                        static_cast<unsigned>(physical_flash.capacity()),
                        static_cast<unsigned>(cpcexplorer::launcher::entry_count()),
                        static_cast<unsigned>(upload_session.received()));
                    for (std::size_t i = 0; i < cpcexplorer::launcher::entry_count(); ++i) {
                        const auto item = cpcexplorer::launcher::entry(i);
                        const char* id = i == 0 ? "locomotive_basic" : item.snapshot_id;
                        std::printf("CPCU1 GAME %u id=%s title=%s\n", static_cast<unsigned>(i),
                                    id == nullptr ? "unknown" : id, item.display_name);
                    }
                    std::printf("CPCU1 END\n");
                } else if (frame.command == Command::Begin) {
                    if (!storage_ready) {
                        std::printf("CPCU1 ERROR storage_unavailable_%s\n", installer.error());
                    } else if (!handshake || now_ms - handshake_ms > 10000 || frame.payload_length != 8) {
                        std::printf("CPCU1 ERROR handshake_required\n");
                    } else {
                        std::uint32_t length = 0, checksum = 0;
                        std::memcpy(&length, frame.payload, 4); std::memcpy(&checksum, frame.payload + 4, 4);
                        const auto result = upload_session.begin(length, checksum, installer);
                        if (result == Result::Ok) {
                            upload_activity_ms = now_ms; handshake = false;
                            draw_upload_progress(0, upload_session.expected_length());
                            std::printf("CPCU1 BEGIN OK\n");
                        } else std::printf("CPCU1 ERROR begin_%s\n", installer.error());
                    }
                } else if (frame.command == Command::Chunk) {
                    const auto result = upload_session.chunk(frame.sequence, frame.payload,
                                                              frame.payload_length, installer);
                    if (result == Result::Ok) {
                        upload_activity_ms = now_ms;
                        draw_upload_progress(upload_session.received(),
                                             upload_session.expected_length());
                        std::printf("CPCU1 ACK %u %u\n", static_cast<unsigned>(frame.sequence),
                                    static_cast<unsigned>(upload_session.received()));
                    } else {
                        std::printf("CPCU1 ERROR chunk_%s code=%u\n", installer.error(),
                                    static_cast<unsigned>(result));
                        upload_session.abort(installer); handshake = false; draw_menu(menu);
                    }
                } else if (frame.command == Command::Commit) {
                    const auto result = upload_session.commit(installer);
                    if (result == Result::Ok) {
                        cpcexplorer::launcher::configure_library(
                            reinterpret_cast<const std::uint8_t*>(cpcexplorer::library::kLibraryXipAddress),
                            cpcexplorer::library::kLibraryBytes);
                        menu.reset(read_menu_buttons(), now_ms); draw_menu(menu);
                        std::printf("CPCU1 COMMITTED crc=%08lx entries=%u\n",
                                    static_cast<unsigned long>(installer.stored_crc32()),
                                    static_cast<unsigned>(cpcexplorer::launcher::entry_count()));
                    } else {
                        std::printf("CPCU1 ERROR commit_%s code=%u\n", installer.error(),
                                    static_cast<unsigned>(result));
                        upload_session.abort(installer); handshake = false; draw_menu(menu);
                    }
                } else if (frame.command == Command::Abort) {
                    upload_session.abort(installer); handshake = false; draw_menu(menu);
                    std::printf("CPCU1 ABORTED\n");
                #if defined(CPCEXPLORER_LIBRARY_REMOVE)
                } else if (frame.command == Command::Remove) {
                    if (!storage_ready) {
                        std::printf("CPCU1 ERROR storage_unavailable_%s\n", installer.error());
                    } else if (upload_session.active()) {
                        std::printf("CPCU1 ERROR remove_upload_active\n");
                    } else if (!handshake || now_ms - handshake_ms > 10000 ||
                               frame.payload_length < 8 || frame.payload_length > 22 ||
                               std::memcmp(frame.payload, "REMOVE\0", 7) != 0) {
                        std::printf("CPCU1 ERROR remove_confirmation_required\n");
                    } else {
                        char game_id[16]{};
                        const std::size_t id_length = frame.payload_length - 7;
                        if (id_length == 0 || id_length >= sizeof(game_id)) {
                            std::printf("CPCU1 ERROR remove_invalid_game_id\n");
                        } else {
                            std::memcpy(game_id, frame.payload + 7, id_length);
                            const bool removed = installer.remove(game_id);
                            cpcexplorer::launcher::configure_library(
                                reinterpret_cast<const std::uint8_t*>(cpcexplorer::library::kLibraryXipAddress),
                                cpcexplorer::library::kLibraryBytes);
                            menu.reset(read_menu_buttons(), now_ms); draw_menu(menu);
                            if (removed) {
                                std::printf("CPCU1 REMOVED id=%s entries=%u\n", game_id,
                                    static_cast<unsigned>(cpcexplorer::launcher::entry_count()));
                            } else {
                                std::printf("CPCU1 ERROR remove_%s\n", installer.error());
                            }
                        }
                        handshake = false;
                    }
                #endif
                }
                std::fflush(stdout);
            }
        }
        if (decoder.poll(now_ms) == cpcexplorer::upload::DecodeResult::Timeout) {
            std::printf("CPCU1 ERROR frame_timeout\n"); std::fflush(stdout);
        }
        if (upload_session.timed_out(now_ms, upload_activity_ms)) {
            upload_session.abort(installer); handshake = false; draw_menu(menu);
            std::printf("CPCU1 ERROR transaction_timeout\n"); std::fflush(stdout);
        }
        if (upload_session.active()) { sleep_ms(5); continue; }
        #endif
        const auto update = menu.update(read_menu_buttons(), to_ms_since_boot(get_absolute_time()));
        if (update.selection_changed) draw_entries(menu);
        if (update.launch) {
            const auto entry = cpcexplorer::launcher::entry(menu.selected());
            std::printf("menu launch=%s\n", entry.display_name);
            draw_centered("LOADING", 205, 2, kYellow, kCpcBlue);
            run_emulator(entry);
        }
        sleep_ms(5);
    }
#else
    std::printf("CPC Explorer Stage 2A starting (RP2350B, 150 MHz nominal)\n");
#if defined(CPCEXPLORER_DIRECT_KNIGHT)
    const cpcexplorer::launcher::Entry game = {
        "Knight Tyme", cpcexplorer::launcher::EntryType::Snapshot, "knight_tyme", nullptr, 0};
    run_emulator(game);
#else
    const cpcexplorer::launcher::Entry basic = {
        "CPC464 BASIC", cpcexplorer::launcher::EntryType::Basic, nullptr, nullptr, 0};
    run_emulator(basic);
#endif
#endif
}
