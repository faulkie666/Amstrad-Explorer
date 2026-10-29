#include "fast_z80_adapter.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

#define CHIPS_HOT_CODE(name) name
#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "chips/z80.h"
#include "chips/ay38910.h"
#include "chips/i8255.h"
#include "chips/mc6845.h"
#include "chips/am40010.h"
#include "chips/upd765.h"
#include "chips/mem.h"
#include "chips/kbd.h"
#include "chips/clk.h"
#include "chips/fdd.h"
#include "chips/fdd_cpc.h"
#include "systems/cpc.h"

namespace {

using Clock = std::chrono::steady_clock;
constexpr int kBareShift = 0x02;
constexpr std::uint64_t kCpcCyclesPerSecond = 4'000'000;
constexpr std::uint64_t kCpcCyclesPerFrame = 80'000;
constexpr std::uint64_t kCpcCyclesPerScanline = 256;
constexpr std::uint64_t kGateArrayInterruptPeriod = 52 * kCpcCyclesPerScanline;

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    const auto size = file.tellg();
    if (size <= 0) return {};
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(result.data()), size);
    return file ? result : std::vector<std::uint8_t>{};
}

std::uint64_t hash_bytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool init_cpc(cpc_t& machine, std::span<const std::uint8_t> rom) {
    if (rom.size() != 32768) return false;
    cpc_desc_t desc{};
    desc.type = CPC_TYPE_464;
    desc.joystick_type = CPC_JOYSTICK_NONE;
    desc.audio.num_samples = 128;
    desc.audio.sample_rate = 22050;
    desc.audio.volume = 0.0f;
    desc.roms.cpc464.os.ptr = const_cast<std::uint8_t*>(rom.data());
    desc.roms.cpc464.os.size = 16384;
    desc.roms.cpc464.basic.ptr = const_cast<std::uint8_t*>(rom.data() + 16384);
    desc.roms.cpc464.basic.size = 16384;
    cpc_init(&machine, &desc);
    kbd_register_key(&machine.kbd, kBareShift, 2, 5, 0);
    cpc_reset(&machine);
    return true;
}

bool load_sna(cpc_t& machine, std::span<const std::uint8_t> sna) {
    if (sna.size() != sizeof(_cpc_sna_header) + 0x10000U) return false;
    chips_range_t range{const_cast<std::uint8_t*>(sna.data()), sna.size()};
    return cpc_quickload(&machine, range, false);
}

fast_z80_state cpu_state(const z80_t& cpu) {
    fast_z80_state state{};
    state.af = cpu.af; state.bc = cpu.bc; state.de = cpu.de; state.hl = cpu.hl;
    state.af2 = cpu.af2; state.bc2 = cpu.bc2; state.de2 = cpu.de2; state.hl2 = cpu.hl2;
    state.ix = cpu.ix; state.iy = cpu.iy; state.wz = cpu.wz; state.sp = cpu.sp;
    state.pc = cpu.pc; state.i = cpu.i; state.r = cpu.r; state.im = cpu.im;
    state.iff1 = cpu.iff1; state.iff2 = cpu.iff2;
    state.halted = (cpu.pins & Z80_HALT) != 0;
    return state;
}

std::uint64_t io_transaction(cpc_t& sys, std::uint16_t port, bool read,
                             std::uint8_t value) {
    std::uint64_t pins = Z80_IORQ | static_cast<std::uint64_t>(port) |
                         (read ? Z80_RD : Z80_WR);
    Z80_SET_DATA(pins, value);
    if ((pins & Z80_A11) == 0) {
        std::uint64_t ppi_pins = (pins & Z80_PIN_MASK &
            ~(I8255_PC_PINS | I8255_A1 | I8255_A0)) | I8255_CS;
        if (pins & Z80_A9) ppi_pins |= I8255_A1;
        if (pins & Z80_A8) ppi_pins |= I8255_A0;
        if ((sys.ppi.pins & (I8255_PC7 | I8255_PC6)) != 0) {
            std::uint64_t ay_pins = 0;
            if (sys.ppi.pins & I8255_PC7) ay_pins |= AY38910_BDIR;
            if (sys.ppi.pins & I8255_PC6) ay_pins |= AY38910_BC1;
            AY38910_SET_DATA(ay_pins, I8255_GET_PA(sys.ppi.pins));
            ay_pins = ay38910_iorq(&sys.psg, ay_pins);
            I8255_SET_PA(ppi_pins, AY38910_GET_DATA(ay_pins));
        }
        ppi_pins |= I8255_PB1 | I8255_PB2 | I8255_PB3 | I8255_PB4;
        if (sys.crtc.vs) ppi_pins |= I8255_PB0;
        ppi_pins = i8255_tick(&sys.ppi, ppi_pins);
        if ((ppi_pins & (I8255_CS | I8255_RD)) == (I8255_CS | I8255_RD))
            Z80_SET_DATA(pins, I8255_GET_DATA(ppi_pins));
        if ((ppi_pins & (I8255_PC7 | I8255_PC6)) != 0) {
            std::uint64_t ay_pins = 0;
            if (ppi_pins & I8255_PC7) ay_pins |= AY38910_BDIR;
            if (ppi_pins & I8255_PC6) ay_pins |= AY38910_BC1;
            AY38910_SET_DATA(ay_pins, I8255_GET_PA(ppi_pins));
            ay38910_iorq(&sys.psg, ay_pins);
        }
        kbd_set_active_columns(&sys.kbd, 1U << (I8255_GET_PC(ppi_pins) & 0x0F));
    }
    am40010_iorq(&sys.ga, pins);
    if ((pins & Z80_A14) == 0) {
        std::uint64_t crtc_pins = (pins & Z80_PIN_MASK) | MC6845_CS;
        if (pins & Z80_A9) crtc_pins |= MC6845_RW;
        if (pins & Z80_A8) crtc_pins |= MC6845_RS;
        pins = mc6845_iorq(&sys.crtc, crtc_pins) & Z80_PIN_MASK;
    }
    return pins;
}

enum class PeripheralMode { None, TimingOnly, FullVideo, FastPresentation,
                            ClockedPresentation };

struct FastMachine {
    cpc_t cpc{};
    fast_z80* cpu = nullptr;
    std::uint64_t cycles = 0;
    std::uint64_t instructions = 0;
    std::uint64_t character_events = 0;
    std::uint64_t video_events = 0;
    std::uint64_t frame_number = 0;
    std::uint64_t interrupt_acks = 0;
    std::uint64_t next_interrupt_cycle = kGateArrayInterruptPeriod;
    std::uint64_t next_frame_cycle = kCpcCyclesPerFrame;
    std::uint64_t next_crtc_line_cycle = kCpcCyclesPerScanline;
    std::uint32_t cycle_remainder = 0;
    unsigned crtc_vsync_remaining = 0;
    unsigned presentation_divisor = 1;
    PeripheralMode mode = PeripheralMode::FastPresentation;

    ~FastMachine() { fast_z80_destroy(cpu); }
};

std::uint8_t mem_read_cb(void* opaque, std::uint16_t address, bool, int) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    return mem_rd(&machine.cpc.mem, address);
}

void mem_write_cb(void* opaque, std::uint16_t address, std::uint8_t value, int) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    mem_wr(&machine.cpc.mem, address, value);
}

std::uint8_t io_read_cb(void* opaque, std::uint16_t port, int) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    return Z80_GET_DATA(io_transaction(machine.cpc, port, true, 0xFF));
}

void io_write_cb(void* opaque, std::uint16_t port, std::uint8_t value, int) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    io_transaction(machine.cpc, port, false, value);
}

void intack_cb(void* opaque, int) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    machine.cpc.ga.video.intcnt &= 0x1F;
    machine.cpc.ga.video.intr = false;
    ++machine.interrupt_acks;
}

bool init_fast(FastMachine& machine, std::span<const std::uint8_t> rom,
               std::span<const std::uint8_t> sna = {}) {
    fast_z80_destroy(machine.cpu);
    machine.cpu = nullptr;
    machine.cycles = machine.instructions = machine.character_events = 0;
    machine.video_events = machine.frame_number = machine.cycle_remainder = 0;
    machine.interrupt_acks = 0;
    machine.next_interrupt_cycle = kGateArrayInterruptPeriod;
    machine.next_frame_cycle = kCpcCyclesPerFrame;
    if (!init_cpc(machine.cpc, rom)) return false;
    if (!sna.empty() && !load_sna(machine.cpc, sna)) return false;
    if (machine.cpc.crtc.vs) {
        unsigned width = machine.cpc.crtc.sync_widths >> 4;
        if (width == 0) width = 16;
        machine.crtc_vsync_remaining = width > machine.cpc.crtc.vsync_ctr
            ? width - machine.cpc.crtc.vsync_ctr : 1;
    } else {
        machine.crtc_vsync_remaining = 0;
    }
    const unsigned line_characters = std::max<unsigned>(1, machine.cpc.crtc.h_total + 1);
    const unsigned remaining_characters = line_characters -
        std::min<unsigned>(machine.cpc.crtc.h_ctr, line_characters - 1);
    machine.next_crtc_line_cycle = static_cast<std::uint64_t>(remaining_characters) * 4;
    fast_z80_callbacks callbacks{mem_read_cb, mem_write_cb, io_read_cb, io_write_cb,
                                 intack_cb, &machine};
    machine.cpu = fast_z80_create(&callbacks);
    if (!machine.cpu) return false;
    const auto initial = cpu_state(machine.cpc.cpu);
    fast_z80_load_state(machine.cpu, &initial);
    return true;
}

void character_event(FastMachine& machine) {
    auto& ga = machine.cpc.ga;
    const auto crtc_pins = ga.cclk_cb(ga.user_data);
    const bool render = machine.mode == PeripheralMode::FullVideo &&
                        (machine.frame_number % machine.presentation_divisor) == 0;
    if (render) {
        ga.video.latch[0] = _am40010_vid_read(&ga, crtc_pins, 0);
    }
    const bool sync = _am40010_sync_irq(&ga, crtc_pins);
    _am40010_crt_tick(&ga, sync);
    ga.crtc_pins = crtc_pins;
    if (render) {
        ga.video.latch[1] = _am40010_vid_read(&ga, crtc_pins, 1);
        _am40010_decode_video(&ga, crtc_pins);
        ++machine.video_events;
    }
    ++machine.character_events;
}

// Stage 4E deliberately trades beam-exact timing for speed.  The display is
// reconstructed from CRTC registers and video RAM only when a host-visible
// frame is due; CPU and interrupt time continue independently between those
// presentation points.  This is isolated prototype code, not production CPC
// timing.
void render_presented_frame(FastMachine& machine) {
    auto& ga = machine.cpc.ga;
    auto& crtc = machine.cpc.crtc;
    std::fill(std::begin(machine.cpc.fb), std::end(machine.cpc.fb), ga.regs.border);

    const unsigned columns = std::clamp<unsigned>(crtc.h_displayed, 1, 48);
    const unsigned rows = std::clamp<unsigned>(crtc.v_displayed, 1, 34);
    const unsigned rasters = std::clamp<unsigned>(crtc.max_scanline_addr + 1, 1, 16);
    const unsigned visible_lines = std::min<unsigned>(rows * rasters, 200);
    const std::uint16_t start = static_cast<std::uint16_t>(
        ((static_cast<unsigned>(crtc.start_addr_hi) << 8) | crtc.start_addr_lo) & 0x3FFF);
    const int origin_x = 64;
    const int origin_y = 36;
    ga.video.mode = ga.regs.config & 3;

    for (unsigned y = 0; y < visible_lines; ++y) {
        const unsigned row = y / rasters;
        const unsigned raster = y % rasters;
        const std::uint16_t row_ma = static_cast<std::uint16_t>((start + row * columns) & 0x3FFF);
        for (unsigned column = 0; column < columns; ++column) {
            const std::uint16_t ma = static_cast<std::uint16_t>((row_ma + column) & 0x3FFF);
            const std::uint16_t address = static_cast<std::uint16_t>(
                ((ma & 0x3000) << 2) | ((ma & 0x03FF) << 1) | ((raster & 7) << 11));
            const int x = origin_x + static_cast<int>(column * 16);
            if (x < 0 || x + 16 > AM40010_FRAMEBUFFER_WIDTH ||
                origin_y + static_cast<int>(y) >= AM40010_FRAMEBUFFER_HEIGHT) break;
            ga.video.latch[0] = ga.ram[address];
            ga.video.latch[1] = ga.ram[static_cast<std::uint16_t>(address + 1)];
            _am40010_decode_pixels(&ga,
                &machine.cpc.fb[(origin_y + y) * AM40010_FRAMEBUFFER_WIDTH + x]);
            ++machine.video_events;
        }
    }
}

// Advance only the CRTC state observable by the CPC464 software used here.
// Whole character spans are consumed arithmetically; the slower row/VSYNC
// transition work occurs once per scanline.  Fine HSYNC skew, light pen and
// mid-line raster effects are intentionally outside this practical prototype.
void advance_crtc_spans(FastMachine& machine) {
    auto& crtc = machine.cpc.crtc;
    const unsigned line_characters = std::max<unsigned>(1, crtc.h_total + 1);
    while (machine.cycles >= machine.next_crtc_line_cycle) {
        crtc.h_ctr = 0;
        machine.next_crtc_line_cycle += static_cast<std::uint64_t>(line_characters) * 4;
        ++machine.character_events;

        if (machine.crtc_vsync_remaining != 0 &&
            --machine.crtc_vsync_remaining == 0) crtc.vs = false;
        if (crtc.r_ctr >= crtc.max_scanline_addr) {
            crtc.r_ctr = 0;
            if (crtc.v_ctr >= crtc.v_total) crtc.v_ctr = 0;
            else ++crtc.v_ctr;
        } else {
            ++crtc.r_ctr;
        }
        if (!crtc.vs && crtc.r_ctr == 0 && crtc.v_ctr == crtc.v_sync_pos) {
            unsigned width = crtc.sync_widths >> 4;
            if (width == 0) width = 16;
            machine.crtc_vsync_remaining = width;
            crtc.vs = true;
        }
    }
}

void advance_fast_presentation(FastMachine& machine, std::uint64_t previous_cycles) {
    while (machine.cycles >= machine.next_interrupt_cycle) {
        machine.cpc.ga.video.intr = true;
        machine.next_interrupt_cycle += kGateArrayInterruptPeriod;
        ++machine.character_events; // event count, retained in the common benchmark field
    }
    while (machine.cycles >= machine.next_frame_cycle) {
        const auto frame = machine.next_frame_cycle / kCpcCyclesPerFrame;
        if ((frame % machine.presentation_divisor) == 0) render_presented_frame(machine);
        machine.next_frame_cycle += kCpcCyclesPerFrame;
        ++machine.character_events;
    }
    (void)previous_cycles;
}

void advance_peripherals(FastMachine& machine, int cycles) {
    const auto previous_cycles = machine.cycles;
    machine.cycles += static_cast<std::uint64_t>(cycles);
    const auto old_frame = machine.frame_number;
    machine.frame_number = machine.cycles / kCpcCyclesPerFrame;
    if (machine.frame_number > old_frame) {
        const auto elapsed_frames = machine.frame_number - old_frame;
        kbd_update(&machine.cpc.kbd,
            static_cast<std::uint32_t>(std::min<std::uint64_t>(elapsed_frames * 20'000, 1'000'000)));
    }
    if (machine.mode == PeripheralMode::None) return;
    if (machine.mode == PeripheralMode::FastPresentation ||
        machine.mode == PeripheralMode::ClockedPresentation) {
        if (machine.mode == PeripheralMode::FastPresentation) {
            advance_crtc_spans(machine);
        } else {
            machine.cycle_remainder += static_cast<std::uint32_t>(cycles);
            while (machine.cycle_remainder >= 4) {
                machine.cycle_remainder -= 4;
                machine.cpc.ga.cclk_cb(machine.cpc.ga.user_data);
            }
        }
        advance_fast_presentation(machine, previous_cycles);
        machine.cpc.ga.seq_tick_count = static_cast<std::uint32_t>(machine.cycles);
        return;
    }
    machine.cycle_remainder += static_cast<std::uint32_t>(cycles);
    while (machine.cycle_remainder >= 4) {
        machine.cycle_remainder -= 4;
        character_event(machine);
    }
    machine.cpc.ga.seq_tick_count = static_cast<std::uint32_t>(machine.cycles);
}

int step(FastMachine& machine) {
    if (machine.cpc.ga.video.intr) fast_z80_raise_int(machine.cpu);
    const int cycles = fast_z80_step(machine.cpu);
    advance_peripherals(machine, cycles);
    ++machine.instructions;
    return cycles;
}

void run_cycles(FastMachine& machine, std::uint64_t cycles) {
    const auto target = machine.cycles + cycles;
    while (machine.cycles < target) step(machine);
}

void key(FastMachine& machine, int code, bool down) {
    if (down) cpc_key_down(&machine.cpc, code);
    else cpc_key_up(&machine.cpc, code);
}

void type_text(FastMachine& machine, std::string_view text) {
    for (unsigned char ch : text) {
        key(machine, ch, true); run_cycles(machine, 200'000);
        key(machine, ch, false); run_cycles(machine, 400'000);
    }
}

std::vector<std::uint8_t> frame_copy(const FastMachine& machine) {
    return {machine.cpc.fb, machine.cpc.fb + sizeof(machine.cpc.fb)};
}

std::size_t frame_difference(std::span<const std::uint8_t> a,
                             std::span<const std::uint8_t> b) {
    if (a.size() != b.size()) return static_cast<std::size_t>(-1);
    std::size_t count = 0;
    for (std::size_t i = 0; i < a.size(); ++i) count += a[i] != b[i];
    return count;
}

struct PlayerBounds {
    int min_x = 10000, max_x = -1, min_y = 10000, max_y = -1;
    std::size_t pixels = 0;
};

PlayerBounds player_bounds(const FastMachine& machine) {
    PlayerBounds result{};
    for (int y = 120; y < 160; ++y) {
        for (int x = 70; x < 140; ++x) {
            const auto pixel = machine.cpc.fb[y * AM40010_FRAMEBUFFER_WIDTH + x];
            const auto color = machine.cpc.ga.hw_colors[pixel];
            if ((color & 0xff) <= 200 || ((color >> 8) & 0xff) <= 200 ||
                ((color >> 16) & 0xff) <= 200) continue;
            result.min_x = std::min(result.min_x, x);
            result.max_x = std::max(result.max_x, x);
            result.min_y = std::min(result.min_y, y);
            result.max_y = std::max(result.max_y, y);
            ++result.pixels;
        }
    }
    return result;
}

bool write_ppm(const FastMachine& machine, const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << AM40010_FRAMEBUFFER_WIDTH << ' ' << AM40010_FRAMEBUFFER_HEIGHT
        << "\n255\n";
    for (std::size_t i = 0; i < sizeof(machine.cpc.fb); ++i) {
        const auto color = machine.cpc.ga.hw_colors[machine.cpc.fb[i]];
        const std::array<char, 3> rgb{static_cast<char>(color),
            static_cast<char>(color >> 8), static_cast<char>(color >> 16)};
        out.write(rgb.data(), 3);
    }
    return static_cast<bool>(out);
}

double lcd_conversion_us(const FastMachine& machine) {
    constexpr int repetitions = 500;
    std::uint64_t checksum = 0;
    const auto start = Clock::now();
    for (int n = 0; n < repetitions; ++n) {
        for (int y = 0; y < 200; ++y) {
            const auto* source = machine.cpc.fb +
                (36 + y) * AM40010_FRAMEBUFFER_WIDTH + 64;
            for (int x = 0; x < 320; ++x) {
                const auto color = machine.cpc.ga.hw_colors[source[x * 2]];
                checksum += ((color & 0x000000f8u) << 8) |
                            ((color & 0x0000fc00u) >> 5) |
                            ((color & 0x00f80000u) >> 19);
            }
        }
    }
    const auto us = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    std::printf("LCD_CONVERT host_us=%.3f checksum=%llu\n", us / repetitions,
                static_cast<unsigned long long>(checksum));
    return us / repetitions;
}

struct Bench { double ms = 0; std::uint64_t instructions = 0, chars = 0, video = 0; };

Bench benchmark_fast(std::span<const std::uint8_t> rom,
                     std::span<const std::uint8_t> sna, PeripheralMode mode,
                     unsigned presentation_divisor) {
    FastMachine machine{};
    init_fast(machine, rom, sna);
    machine.mode = mode;
    machine.presentation_divisor = presentation_divisor;
    const auto start = Clock::now();
    run_cycles(machine, kCpcCyclesPerSecond);
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    return {elapsed, machine.instructions, machine.character_events, machine.video_events};
}

double benchmark_reference(std::span<const std::uint8_t> rom,
                           std::span<const std::uint8_t> sna) {
    cpc_t machine{};
    init_cpc(machine, rom); load_sna(machine, sna);
    const auto start = Clock::now();
    cpc_exec(&machine, 1'000'000);
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

bool test_manic(std::span<const std::uint8_t> rom, std::span<const std::uint8_t> sna,
                const std::filesystem::path& evidence) {
    std::filesystem::create_directories(evidence);
    FastMachine base{};
    if (!init_fast(base, rom, sna)) return false;
    run_cycles(base, 2 * kCpcCyclesPerSecond);
    const auto base_frame = frame_copy(base);
    const auto base_hash = hash_bytes(base_frame.data(), base_frame.size());
    write_ppm(base, evidence / "manic-central-cavern.ppm");

    struct Result { std::vector<std::uint8_t> frame; PlayerBounds player; std::uint64_t ram; };
    auto scenario = [&](int key_code, const char* name) {
        FastMachine machine{}; init_fast(machine, rom, sna);
        if (key_code) key(machine, key_code, true);
        run_cycles(machine, 15 * kCpcCyclesPerFrame);
        if (key_code) key(machine, key_code, false);
        run_cycles(machine, 5 * kCpcCyclesPerFrame);
        write_ppm(machine, evidence / name);
        return Result{frame_copy(machine), player_bounds(machine),
                      hash_bytes(machine.cpc.ram, sizeof(machine.cpc.ram))};
    };
    const auto neutral = scenario(0, "manic-neutral.ppm");
    const auto left = scenario('q', "manic-left-q.ppm");
    const auto right = scenario('w', "manic-right-w.ppm");
    const auto jump = scenario(kBareShift, "manic-jump-shift.ppm");
    const auto ld = frame_difference(neutral.frame, left.frame);
    const auto rd = frame_difference(neutral.frame, right.frame);
    const auto jd = frame_difference(neutral.frame, jump.frame);
    std::printf("MANIC central_hash=%016llx neutral_player=%d,%d left=%d,%d right=%d,%d "
                "jump=%d,%d diffs=%zu/%zu/%zu ram=%016llx/%016llx/%016llx/%016llx\n",
        static_cast<unsigned long long>(base_hash), neutral.player.min_x, neutral.player.min_y,
        left.player.min_x, left.player.min_y, right.player.min_x, right.player.min_y,
        jump.player.min_x, jump.player.min_y, ld, rd, jd,
        static_cast<unsigned long long>(neutral.ram), static_cast<unsigned long long>(left.ram),
        static_cast<unsigned long long>(right.ram), static_cast<unsigned long long>(jump.ram));
    return base_hash != 0 && neutral.player.pixels >= 100 && ld && rd && jd &&
           left.ram != neutral.ram && right.ram != neutral.ram && jump.ram != neutral.ram;
}

bool test_knight(std::span<const std::uint8_t> rom, std::span<const std::uint8_t> sna,
                 const std::filesystem::path& evidence) {
    FastMachine baseline{};
    if (!init_fast(baseline, rom, sna)) return false;
    run_cycles(baseline, kCpcCyclesPerSecond);
    const auto menu = frame_copy(baseline);
    write_ppm(baseline, evidence / "knight-menu.ppm");

    std::size_t best_diff = 0;
    int best_key = 0;
    std::uint64_t best_instructions = 0;
    static constexpr std::array<int, 8> candidates{' ', 0x08, 0x09, 0x0A,
                                                   0x0B, 0x0D, '1', 'n'};
    for (const int key_code : candidates) {
        FastMachine machine{};
        init_fast(machine, rom, sna);
        run_cycles(machine, kCpcCyclesPerSecond);
        key(machine, key_code, true); run_cycles(machine, 400'000);
        key(machine, key_code, false); run_cycles(machine, 8 * kCpcCyclesPerFrame);
        const auto frame = frame_copy(machine);
        const auto diff = frame_difference(menu, frame);
        fast_z80_state state{}; fast_z80_get_state(machine.cpu, &state);
        std::printf("KNIGHT_KEY code=%02x diff=%zu ram=%016llx pc=%04x iff=%d ack=%llu\n",
            key_code, diff,
            static_cast<unsigned long long>(hash_bytes(machine.cpc.ram,
                                                        sizeof(machine.cpc.ram))), state.pc,
            state.iff1 ? 1 : 0, static_cast<unsigned long long>(machine.interrupt_acks));
        if (diff > best_diff) {
            best_diff = diff;
            best_key = key_code;
            best_instructions = machine.instructions;
            write_ppm(machine, evidence / "knight-after-input.ppm");
        }
    }
    std::printf("KNIGHT menu_hash=%016llx best_key=%02x diff=%zu instructions=%llu\n",
        static_cast<unsigned long long>(hash_bytes(menu.data(), menu.size())), best_key,
        best_diff, static_cast<unsigned long long>(best_instructions));

    FastMachine clocked{};
    init_fast(clocked, rom, sna);
    clocked.mode = PeripheralMode::FullVideo;
    run_cycles(clocked, kCpcCyclesPerSecond);
    const auto clocked_menu = frame_copy(clocked);
    key(clocked, ' ', true); run_cycles(clocked, 400'000);
    key(clocked, ' ', false); run_cycles(clocked, 8 * kCpcCyclesPerFrame);
    const auto clocked_diff = frame_difference(clocked_menu, frame_copy(clocked));
    std::printf("KNIGHT_CHARACTER_CLOCK space_diff=%zu ack=%llu\n", clocked_diff,
        static_cast<unsigned long long>(clocked.interrupt_acks));

    // Establish whether the saved menu itself responds to these keys in the
    // established per-T-state engine.  This separates a prototype regression
    // from a snapshot/menu-control assumption.
    cpc_t reference_baseline{};
    init_cpc(reference_baseline, rom); load_sna(reference_baseline, sna);
    for (int frame = 0; frame < 50; ++frame) cpc_exec(&reference_baseline, 20'000);
    const std::vector<std::uint8_t> reference_menu(
        reference_baseline.fb, reference_baseline.fb + sizeof(reference_baseline.fb));
    std::size_t reference_best = 0;
    for (const int key_code : candidates) {
        cpc_t reference{};
        init_cpc(reference, rom); load_sna(reference, sna);
        for (int frame = 0; frame < 50; ++frame) cpc_exec(&reference, 20'000);
        cpc_key_down(&reference, key_code);
        for (int frame = 0; frame < 5; ++frame) cpc_exec(&reference, 20'000);
        cpc_key_up(&reference, key_code);
        for (int frame = 0; frame < 8; ++frame) cpc_exec(&reference, 20'000);
        const std::span<const std::uint8_t> reference_frame(reference.fb,
                                                            sizeof(reference.fb));
        const auto diff = frame_difference(reference_menu, reference_frame);
        reference_best = std::max(reference_best, diff);
        std::printf("KNIGHT_REF_KEY code=%02x diff=%zu\n", key_code, diff);
    }
    std::printf("KNIGHT_REFERENCE best_diff=%zu\n", reference_best);
    return best_diff > 100 && best_instructions > 100'000;
}

bool test_basic(std::span<const std::uint8_t> rom,
                const std::filesystem::path& evidence) {
    FastMachine machine{};
    if (!init_fast(machine, rom)) return false;
    run_cycles(machine, 5 * kCpcCyclesPerSecond);
    const auto ready = frame_copy(machine);
    write_ppm(machine, evidence / "basic-ready.ppm");
    type_text(machine, "PRINT 2+2\r");
    run_cycles(machine, kCpcCyclesPerSecond);
    const auto result = frame_copy(machine);
    write_ppm(machine, evidence / "basic-print-2-plus-2.ppm");
    fast_z80_state state{}; fast_z80_get_state(machine.cpu, &state);
    const auto diff = frame_difference(ready, result);
    std::printf("BASIC ready_hash=%016llx result_hash=%016llx diff=%zu pc=%04x instructions=%llu\n",
        static_cast<unsigned long long>(hash_bytes(ready.data(), ready.size())),
        static_cast<unsigned long long>(hash_bytes(result.data(), result.size())), diff, state.pc,
        static_cast<unsigned long long>(machine.instructions));
    return diff > 100 && machine.instructions > 100'000;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::fprintf(stderr, "usage: %s ROM KNIGHT_SNA MANIC_SNA EVIDENCE_DIR\n", argv[0]);
        return 2;
    }
    const auto rom = read_file(argv[1]);
    const auto knight = read_file(argv[2]);
    const auto manic = read_file(argv[3]);
    const std::filesystem::path evidence = argv[4];
    if (rom.size() != 32768 || knight.size() != 65792 || manic.size() != 65792) return 3;
    std::filesystem::create_directories(evidence);

    const bool manic_ok = test_manic(rom, manic, evidence);
    const bool knight_ok = test_knight(rom, knight, evidence);
    const bool basic_ok = test_basic(rom, evidence);

    const double reference_ms = benchmark_reference(rom, manic);
    const auto cpu = benchmark_fast(rom, manic, PeripheralMode::None, 1);
    const auto peripheral = benchmark_fast(rom, manic, PeripheralMode::TimingOnly, 1);
    const auto full = benchmark_fast(rom, manic, PeripheralMode::FullVideo, 1);
    const auto quarter = benchmark_fast(rom, manic, PeripheralMode::FullVideo, 4);
    const auto fast = benchmark_fast(rom, manic, PeripheralMode::FastPresentation, 4);
    const auto clocked = benchmark_fast(rom, manic, PeripheralMode::ClockedPresentation, 4);
    std::printf("BENCH emulated_ms=1000 reference_ms=%.3f cpu_ms=%.3f peripheral_ms=%.3f "
                "video50_ms=%.3f video12_5_ms=%.3f fast12_5_ms=%.3f "
                "clocked12_5_ms=%.3f speedup=%.3f/%.3f/%.3f/%.3f\n",
        reference_ms, cpu.ms, peripheral.ms, full.ms, quarter.ms, fast.ms, clocked.ms,
        reference_ms / full.ms, reference_ms / quarter.ms, reference_ms / fast.ms,
        reference_ms / clocked.ms);
    std::printf("DISPATCH instructions=%llu chars=%llu video50=%llu video12_5=%llu "
                "fast_events=%llu fast_pixels=%llu\n",
        static_cast<unsigned long long>(full.instructions),
        static_cast<unsigned long long>(full.chars),
        static_cast<unsigned long long>(full.video),
        static_cast<unsigned long long>(quarter.video),
        static_cast<unsigned long long>(fast.chars),
        static_cast<unsigned long long>(fast.video));
    FastMachine conversion{}; init_fast(conversion, rom, manic);
    run_cycles(conversion, kCpcCyclesPerFrame); lcd_conversion_us(conversion);

    std::printf("RESULT manic=%s knight=%s basic=%s architecture=%s\n",
        manic_ok ? "PASS" : "FAIL", knight_ok ? "PASS" : "FAIL",
        basic_ok ? "PASS" : "FAIL",
        (manic_ok && knight_ok && basic_ok && reference_ms / fast.ms >= 5.0)
            ? "CREDIBLE" : "HOLD");
    return (manic_ok && knight_ok && basic_ok) ? 0 : 1;
}
