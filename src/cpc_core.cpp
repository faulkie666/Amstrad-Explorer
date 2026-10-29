#include "cpc_core.h"
#include "game_library.h"

#include <cassert>
#include <algorithm>
#include <cstring>
#include <iterator>

#include "cpc_rom.h"
#if defined(CPCEXPLORER_KNIGHT_TYME)
#include "knight_tyme_snapshot.h"
#endif

#if defined(CPCEXPLORER_SRAM_HOT_CODE)
#include "pico.h"
#define CHIPS_HOT_CODE(name) __not_in_flash_func(name)
#else
#define CHIPS_HOT_CODE(name) name
#endif

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

#if defined(CPCEXPLORER_FAST_ENGINE)
#include "emulation/fast_z80_adapter.h"
#include "emulation/fast_engine_test_hooks.h"
#endif
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
#include "pico/time.h"
#endif

namespace cpcexplorer {
namespace {

cpc_t machine;
std::uint64_t total_ticks;
bool reference_frame_ready;
EngineKind active_engine = EngineKind::Reference;

}  // namespace

static void reference_init() {
    cpc_desc_t desc{};
    desc.type = CPC_TYPE_464;
    desc.joystick_type = CPC_JOYSTICK_NONE;
    desc.audio.num_samples = 128;
    desc.audio.sample_rate = 22050;
    desc.audio.volume = 0.0f;
    desc.roms.cpc464.os.ptr = const_cast<unsigned char*>(cpc464_os_rom);
    desc.roms.cpc464.os.size = sizeof(cpc464_os_rom);
    desc.roms.cpc464.basic.ptr = const_cast<unsigned char*>(cpc464_basic_rom);
    desc.roms.cpc464.basic.size = sizeof(cpc464_basic_rom);
    cpc_init(&machine, &desc);
    // The upstream key map can synthesize Shift as a modifier but has no host
    // code for holding Shift by itself. Manic Miner uses bare Shift to jump.
    kbd_register_key(&machine.kbd, KeyShift, 2, 5, 0);
    cpc_reset(&machine);
    total_ticks = 0;
    reference_frame_ready = false;
}

static void reference_reset() {
    if (!machine.valid) { reference_init(); return; }
    cpc_reset(&machine);
    total_ticks = 0;
    reference_frame_ready = false;
}

static void reference_run_us(std::uint32_t microseconds) {
    total_ticks += cpc_exec(&machine, microseconds);
    reference_frame_ready = true;
}

static void reference_key_down(int key_code) { cpc_key_down(&machine, key_code); }
static void reference_key_up(int key_code) { cpc_key_up(&machine, key_code); }

static bool reference_quickload(const std::uint8_t* data, std::size_t size, bool start) {
    if ((data == nullptr) || (size == 0)) return false;
    chips_range_t range{};
    range.ptr = const_cast<std::uint8_t*>(data);
    range.size = size;
    return cpc_quickload(&machine, range, start);
}

std::size_t sna_size() { return sizeof(_cpc_sna_header) + 0x10000U; }

static bool reference_save_sna(std::uint8_t* destination, std::size_t size) {
    if ((destination == nullptr) || (size < sna_size())) return false;

    // A CPC SNA starts an instruction from its saved PC. Align to the
    // overlapped M1/T2 boundary, where the previous instruction is complete.
    for (int i = 0; (i < 32) && !z80_opdone(&machine.cpu); ++i) {
        machine.pins = _cpc_tick(&machine, machine.pins);
        ++total_ticks;
    }
    if (!z80_opdone(&machine.cpu)) return false;

    _cpc_sna_header header{};
    const std::uint8_t magic[8] = {'M', 'V', ' ', '-', ' ', 'S', 'N', 'A'};
    std::memcpy(header.magic, magic, sizeof(magic));
    header.version = 1;
    header.F = machine.cpu.f; header.A = machine.cpu.a;
    header.C = machine.cpu.c; header.B = machine.cpu.b;
    header.E = machine.cpu.e; header.D = machine.cpu.d;
    header.L = machine.cpu.l; header.H = machine.cpu.h;
    header.R = machine.cpu.r; header.I = machine.cpu.i;
    header.IFF1 = machine.cpu.iff1 ? 1 : 0;
    header.IFF2 = machine.cpu.iff2 ? 1 : 0;
    header.IX_l = machine.cpu.ixl; header.IX_h = machine.cpu.ixh;
    header.IY_l = machine.cpu.iyl; header.IY_h = machine.cpu.iyh;
    header.SP_l = machine.cpu.spl; header.SP_h = machine.cpu.sph;
    const std::uint16_t pc = static_cast<std::uint16_t>(machine.cpu.pc - 1);
    header.PC_l = static_cast<std::uint8_t>(pc);
    header.PC_h = static_cast<std::uint8_t>(pc >> 8);
    header.IM = machine.cpu.im;
    header.F_ = static_cast<std::uint8_t>(machine.cpu.af2);
    header.A_ = static_cast<std::uint8_t>(machine.cpu.af2 >> 8);
    header.C_ = static_cast<std::uint8_t>(machine.cpu.bc2);
    header.B_ = static_cast<std::uint8_t>(machine.cpu.bc2 >> 8);
    header.E_ = static_cast<std::uint8_t>(machine.cpu.de2);
    header.D_ = static_cast<std::uint8_t>(machine.cpu.de2 >> 8);
    header.L_ = static_cast<std::uint8_t>(machine.cpu.hl2);
    header.H_ = static_cast<std::uint8_t>(machine.cpu.hl2 >> 8);
    header.selected_pen = machine.ga.regs.inksel;
    std::memcpy(header.pens, machine.ga.regs.ink, 16);
    header.pens[16] = machine.ga.regs.border;
    header.gate_array_config = machine.ga.regs.config;
    header.ram_config = machine.ga.ram_config;
    header.crtc_selected = machine.crtc.sel;
    std::memcpy(header.crtc_regs, machine.crtc.reg, sizeof(header.crtc_regs));
    header.rom_config = machine.ga.rom_select;
    header.ppi_a = machine.ppi.pa.outp;
    header.ppi_b = machine.ppi.pb.outp;
    header.ppi_c = machine.ppi.pc.outp;
    header.ppi_control = machine.ppi.control;
    header.psg_selected = machine.psg.addr;
    std::memcpy(header.psg_regs, machine.psg.reg, sizeof(header.psg_regs));
    header.dump_size_l = 64;
    header.dump_size_h = 0;
    std::memcpy(destination, &header, sizeof(header));
    std::memcpy(destination + sizeof(header), machine.ram, 0x10000U);
    return true;
}

static const std::uint8_t* reference_framebuffer();
static const std::uint32_t* reference_palette();
static Diagnostics reference_diagnostics();

#if defined(CPCEXPLORER_FAST_ENGINE)

constexpr std::uint64_t kCpcCyclesPerFrame = 80'000;
constexpr std::uint64_t kCpcCyclesPerScanline = 256;
constexpr std::uint64_t kGateArrayInterruptPeriod = 52 * kCpcCyclesPerScanline;
#if !defined(CPCEXPLORER_FAST_PRESENTATION_DIVISOR)
#define CPCEXPLORER_FAST_PRESENTATION_DIVISOR 4
#endif
constexpr unsigned kPresentationDivisor = CPCEXPLORER_FAST_PRESENTATION_DIVISOR;
static_assert(kPresentationDivisor == 2 || kPresentationDivisor == 4,
              "fast presentation divisor must be 2 or 4");

struct FastRuntime {
    fast_z80* cpu = nullptr;
    std::uint64_t cycles = 0;
    std::uint64_t cycle_budget = 0;
    std::uint64_t instructions = 0;
    std::uint64_t peripheral_dispatches = 0;
    std::uint64_t reconstructed_frames = 0;
    std::uint64_t next_interrupt_cycle = kGateArrayInterruptPeriod;
    std::uint64_t next_frame_cycle = kCpcCyclesPerFrame;
    std::uint64_t next_crtc_line_cycle = kCpcCyclesPerScanline;
    unsigned crtc_vsync_remaining = 0;
    bool frame_ready = false;
    testing::FastBenchmarkMode benchmark_mode = testing::FastBenchmarkMode::Full;
    bool diagnostics_enabled = false;
    std::uint32_t last_emulated_us = 0;
    std::uint32_t last_cpu_us = 0;
    std::uint32_t last_event_us = 0;
    std::uint32_t last_reconstruction_us = 0;
#if defined(CPCEXPLORER_FAST_RAM_ACCESS)
    std::uint8_t* read_pages[16]{};
    std::uint8_t* write_pages[16]{};
#endif
};

FastRuntime fast;

#if defined(CPCEXPLORER_FAST_RAM_ACCESS)
static std::uint8_t* contiguous_4k_page(mem_page_t* pages, unsigned first,
                                        bool write) {
    std::uint8_t* base = write ? pages[first].write_ptr : pages[first].read_ptr;
    if (base == nullptr) return nullptr;
    for (unsigned i = 1; i < 4; ++i) {
        const auto* actual = write ? pages[first + i].write_ptr : pages[first + i].read_ptr;
        if (actual != base + i * MEM_PAGE_SIZE) return nullptr;
    }
    return base;
}

static void refresh_fast_memory_pages() {
    if (fast.cpu == nullptr) return;
    for (unsigned page = 0; page < 16; ++page) {
        const unsigned first = page * 4;
        fast.read_pages[page] = contiguous_4k_page(machine.mem.page_table, first, false);
        fast.write_pages[page] = contiguous_4k_page(machine.mem.page_table, first, true);
    }
    fast_z80_set_memory_pages(fast.cpu, fast.read_pages, fast.write_pages);
}
#else
static void refresh_fast_memory_pages() {}
#endif

static fast_z80_state state_from_chips(const z80_t& cpu) {
    fast_z80_state state{};
    state.af = cpu.af; state.bc = cpu.bc; state.de = cpu.de; state.hl = cpu.hl;
    state.af2 = cpu.af2; state.bc2 = cpu.bc2; state.de2 = cpu.de2; state.hl2 = cpu.hl2;
    state.ix = cpu.ix; state.iy = cpu.iy; state.wz = cpu.wz; state.sp = cpu.sp;
    state.pc = cpu.pc; state.i = cpu.i; state.r = cpu.r; state.im = cpu.im;
    state.iff1 = cpu.iff1; state.iff2 = cpu.iff2;
    state.halted = (cpu.pins & Z80_HALT) != 0;
    return state;
}

static void init_cpc_state(cpc_t& target) {
    cpc_desc_t desc{};
    desc.type = CPC_TYPE_464;
    desc.joystick_type = CPC_JOYSTICK_NONE;
    desc.audio.num_samples = 128;
    desc.audio.sample_rate = 22050;
    desc.audio.volume = 0.0f;
    desc.roms.cpc464.os.ptr = const_cast<unsigned char*>(cpc464_os_rom);
    desc.roms.cpc464.os.size = sizeof(cpc464_os_rom);
    desc.roms.cpc464.basic.ptr = const_cast<unsigned char*>(cpc464_basic_rom);
    desc.roms.cpc464.basic.size = sizeof(cpc464_basic_rom);
    cpc_init(&target, &desc);
    kbd_register_key(&target.kbd, KeyShift, 2, 5, 0);
    cpc_reset(&target);
}

static std::uint64_t fast_io_transaction(cpc_t& sys, std::uint16_t port,
                                         bool read, std::uint8_t value) {
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
#if defined(CPCEXPLORER_FAST_RAM_ACCESS)
    const auto previous_config = sys.ga.regs.config;
    const auto previous_ram_config = sys.ga.ram_config;
    const auto previous_rom_select = sys.ga.rom_select;
#endif
    am40010_iorq(&sys.ga, pins);
#if defined(CPCEXPLORER_FAST_RAM_ACCESS)
    // Palette, PPI, keyboard and CRTC I/O cannot alter the memory map.  Avoid
    // rescanning all sixteen direct pages unless the gate array actually
    // changed ROM visibility, RAM configuration, or the selected upper ROM.
    if (sys.ga.regs.config != previous_config ||
        sys.ga.ram_config != previous_ram_config ||
        sys.ga.rom_select != previous_rom_select)
        refresh_fast_memory_pages();
#endif
    if ((pins & Z80_A14) == 0) {
        std::uint64_t crtc_pins = (pins & Z80_PIN_MASK) | MC6845_CS;
        if (pins & Z80_A9) crtc_pins |= MC6845_RW;
        if (pins & Z80_A8) crtc_pins |= MC6845_RS;
        pins = mc6845_iorq(&sys.crtc, crtc_pins) & Z80_PIN_MASK;
    }
    return pins;
}

static std::uint8_t fast_mem_read(void* opaque, std::uint16_t address, bool, int) {
    (void)opaque;
    return mem_rd(&machine.mem, address);
}
static void fast_mem_write(void* opaque, std::uint16_t address, std::uint8_t value, int) {
    (void)opaque;
    mem_wr(&machine.mem, address, value);
}
static std::uint8_t fast_io_read(void* opaque, std::uint16_t port, int) {
    (void)opaque;
    return Z80_GET_DATA(fast_io_transaction(machine, port, true, 0xFF));
}
static void fast_io_write(void* opaque, std::uint16_t port, std::uint8_t value, int) {
    (void)opaque;
    fast_io_transaction(machine, port, false, value);
}
static void fast_intack(void* opaque, int) {
    auto& runtime = *static_cast<FastRuntime*>(opaque);
    machine.ga.video.intcnt &= 0x1F;
    machine.ga.video.intr = false;
    (void)runtime;
}

static void reset_fast_timing() {
    fast.cycles = fast.cycle_budget = fast.instructions = 0;
    fast.peripheral_dispatches = fast.reconstructed_frames = 0;
    fast.next_interrupt_cycle = kGateArrayInterruptPeriod;
    fast.next_frame_cycle = kCpcCyclesPerFrame;
    fast.frame_ready = false;
    unsigned width = machine.crtc.sync_widths >> 4;
    if (width == 0) width = 16;
    fast.crtc_vsync_remaining = machine.crtc.vs
        ? (width > machine.crtc.vsync_ctr ? width - machine.crtc.vsync_ctr : 1) : 0;
    const unsigned line_chars = std::max<unsigned>(1, machine.crtc.h_total + 1);
    const unsigned remaining = line_chars -
        std::min<unsigned>(machine.crtc.h_ctr, line_chars - 1);
    fast.next_crtc_line_cycle = static_cast<std::uint64_t>(remaining) * 4;
}

static void reload_fast_cpu() {
    refresh_fast_memory_pages();
    const auto state = state_from_chips(machine.cpu);
    fast_z80_load_state(fast.cpu, &state);
    reset_fast_timing();
}

static void fast_init() {
    if (fast.cpu) { fast_z80_destroy(fast.cpu); fast.cpu = nullptr; }
    init_cpc_state(machine);
    fast_z80_callbacks callbacks{fast_mem_read, fast_mem_write, fast_io_read,
                                 fast_io_write, fast_intack, &fast};
    fast.cpu = fast_z80_create(&callbacks);
    assert(fast.cpu != nullptr);
    fast.benchmark_mode = testing::FastBenchmarkMode::Full;
    reload_fast_cpu();
}

static void fast_reset() {
    if (!fast.cpu) { fast_init(); return; }
    cpc_reset(&machine);
    reload_fast_cpu();
}

static void advance_fast_crtc() {
    auto& crtc = machine.crtc;
    const unsigned line_chars = std::max<unsigned>(1, crtc.h_total + 1);
    while (fast.cycles >= fast.next_crtc_line_cycle) {
        crtc.h_ctr = 0;
        fast.next_crtc_line_cycle += static_cast<std::uint64_t>(line_chars) * 4;
        ++fast.peripheral_dispatches;
        if (fast.crtc_vsync_remaining && --fast.crtc_vsync_remaining == 0)
            crtc.vs = false;
        if (crtc.r_ctr >= crtc.max_scanline_addr) {
            crtc.r_ctr = 0;
            crtc.v_ctr = crtc.v_ctr >= crtc.v_total ? 0 : crtc.v_ctr + 1;
        } else {
            ++crtc.r_ctr;
        }
        if (!crtc.vs && crtc.r_ctr == 0 && crtc.v_ctr == crtc.v_sync_pos) {
            unsigned width = crtc.sync_widths >> 4;
            if (width == 0) width = 16;
            fast.crtc_vsync_remaining = width;
            crtc.vs = true;
        }
    }
}

static void reconstruct_fast_frame() {
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
    const std::uint64_t timing_start = fast.diagnostics_enabled ? time_us_64() : 0;
#endif
    auto& ga = machine.ga;
    auto& crtc = machine.crtc;
    std::fill(std::begin(machine.fb), std::end(machine.fb), ga.regs.border);
    const unsigned columns = std::clamp<unsigned>(crtc.h_displayed, 1, 48);
    const unsigned rows = std::clamp<unsigned>(crtc.v_displayed, 1, 34);
    const unsigned rasters = std::clamp<unsigned>(crtc.max_scanline_addr + 1, 1, 16);
    const unsigned lines = std::min<unsigned>(rows * rasters, 200);
    // Match the reference beam renderer's horizontal placement.  The previous
    // fixed origin of 64 was correct only for a 40-column (640-pixel) active
    // display and left narrower CRTC modes anchored to the LCD's left edge.
    const int active_width = static_cast<int>(columns * 16);
    const int active_x = (AM40010_FRAMEBUFFER_WIDTH - active_width) / 2;
    const std::uint16_t start = static_cast<std::uint16_t>(
        ((static_cast<unsigned>(crtc.start_addr_hi) << 8) | crtc.start_addr_lo) & 0x3FFF);
    ga.video.mode = ga.regs.config & 3;
    for (unsigned y = 0; y < lines; ++y) {
        const unsigned row = y / rasters;
        const unsigned raster = y % rasters;
        const std::uint16_t row_ma = static_cast<std::uint16_t>((start + row * columns) & 0x3FFF);
        for (unsigned column = 0; column < columns; ++column) {
            const std::uint16_t ma = static_cast<std::uint16_t>((row_ma + column) & 0x3FFF);
            const std::uint16_t address = static_cast<std::uint16_t>(
                ((ma & 0x3000) << 2) | ((ma & 0x03FF) << 1) | ((raster & 7) << 11));
            const int x = active_x + static_cast<int>(column * 16);
            if (x + 16 > AM40010_FRAMEBUFFER_WIDTH || 36 + static_cast<int>(y) >= AM40010_FRAMEBUFFER_HEIGHT)
                break;
            ga.video.latch[0] = ga.ram[address];
            ga.video.latch[1] = ga.ram[static_cast<std::uint16_t>(address + 1)];
            _am40010_decode_pixels(&ga, &machine.fb[(36 + y) * AM40010_FRAMEBUFFER_WIDTH + x]);
        }
    }
    ++fast.reconstructed_frames;
    fast.frame_ready = true;
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
    if (fast.diagnostics_enabled)
        fast.last_reconstruction_us += static_cast<std::uint32_t>(time_us_64() - timing_start);
#endif
}

static void advance_fast_events() {
    if (fast.benchmark_mode == testing::FastBenchmarkMode::CpuOnly) return;
    advance_fast_crtc();
    while (fast.cycles >= fast.next_interrupt_cycle) {
        machine.ga.video.intr = true;
        fast.next_interrupt_cycle += kGateArrayInterruptPeriod;
        ++fast.peripheral_dispatches;
    }
    while (fast.cycles >= fast.next_frame_cycle) {
        const auto frame = fast.next_frame_cycle / kCpcCyclesPerFrame;
        if ((frame % kPresentationDivisor) == 0 &&
            fast.benchmark_mode == testing::FastBenchmarkMode::Full)
            reconstruct_fast_frame();
        fast.next_frame_cycle += kCpcCyclesPerFrame;
        ++fast.peripheral_dispatches;
    }
    machine.ga.seq_tick_count = static_cast<std::uint32_t>(fast.cycles);
}

static void fast_run_us(std::uint32_t microseconds) {
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
    const std::uint64_t timing_start = fast.diagnostics_enabled ? time_us_64() : 0;
    std::uint64_t event_timing_us = 0;
#endif
    fast.last_emulated_us = microseconds;
    fast.last_cpu_us = fast.last_event_us = fast.last_reconstruction_us = 0;
    fast.cycle_budget += static_cast<std::uint64_t>(microseconds) * 4;
    while (fast.cycles < fast.cycle_budget) {
        if (machine.ga.video.intr) fast_z80_raise_int(fast.cpu);
        const int cycles = fast_z80_step(fast.cpu);
        fast.cycles += static_cast<std::uint64_t>(cycles);
        ++fast.instructions;
        if (fast.benchmark_mode != testing::FastBenchmarkMode::CpuOnly &&
            (fast.cycles >= fast.next_crtc_line_cycle ||
             fast.cycles >= fast.next_interrupt_cycle ||
             fast.cycles >= fast.next_frame_cycle)) {
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
            const std::uint64_t event_start = fast.diagnostics_enabled ? time_us_64() : 0;
#endif
            advance_fast_events();
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
            if (fast.diagnostics_enabled) event_timing_us += time_us_64() - event_start;
#endif
        }
    }
    kbd_update(&machine.kbd, microseconds);
#if defined(CPCEXPLORER_FAST_DIAGNOSTICS)
    if (fast.diagnostics_enabled) {
        const auto total = static_cast<std::uint32_t>(time_us_64() - timing_start);
        const auto bounded_events = static_cast<std::uint32_t>(event_timing_us);
        fast.last_event_us = bounded_events >= fast.last_reconstruction_us
            ? bounded_events - fast.last_reconstruction_us : 0;
        fast.last_cpu_us = total >= bounded_events ? total - bounded_events : 0;
    }
#endif
}

static void fast_key_down(int key_code) { cpc_key_down(&machine, key_code); }
static void fast_key_up(int key_code) { cpc_key_up(&machine, key_code); }

static bool fast_quickload(const std::uint8_t* data, std::size_t size, bool start) {
    if (!data || size == 0) return false;
    chips_range_t range{const_cast<std::uint8_t*>(data), size};
    if (!cpc_quickload(&machine, range, start)) return false;
    reload_fast_cpu();
    return true;
}

static Diagnostics fast_diagnostics() {
    fast_z80_state cpu{};
    fast_z80_get_state(fast.cpu, &cpu);
    return {
        .z80_ticks = fast.cycles,
        .pc = cpu.pc,
        .state_bytes = sizeof(machine) + sizeof(FastRuntime) + fast_z80_allocation_bytes(),
        .framebuffer_bytes = sizeof(machine.fb),
        .ram_bytes = sizeof(machine.ram),
        .engine = EngineKind::Fast,
        .instructions = fast.instructions,
        .peripheral_dispatches = fast.peripheral_dispatches,
        .reconstructed_frames = fast.reconstructed_frames,
        .last_emulated_us = fast.last_emulated_us,
        .last_cpu_us = fast.last_cpu_us,
        .last_event_us = fast.last_event_us,
        .last_reconstruction_us = fast.last_reconstruction_us,
    };
}

namespace testing {
void set_fast_benchmark_mode(FastBenchmarkMode mode) { fast.benchmark_mode = mode; }
}  // namespace testing

#endif

bool select_engine(EngineKind engine) {
    if (!engine_available(engine)) return false;
    active_engine = engine;
    return true;
}

EngineKind selected_engine() { return active_engine; }

bool engine_available(EngineKind engine) {
    if (engine == EngineKind::Reference) return true;
#if defined(CPCEXPLORER_FAST_ENGINE)
    return engine == EngineKind::Fast;
#else
    return false;
#endif
}

const char* engine_name(EngineKind engine) {
    return engine == EngineKind::Fast ? "fast" : "reference";
}

void init() {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) { fast_init(); return; }
#endif
    reference_init();
}

void reset() {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) { fast_reset(); return; }
#endif
    reference_reset();
}

void run_us(std::uint32_t microseconds) {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) { fast_run_us(microseconds); return; }
#endif
    reference_run_us(microseconds);
}

bool consume_presented_frame() {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) {
        const bool ready = fast.frame_ready; fast.frame_ready = false; return ready;
    }
#endif
    const bool ready = reference_frame_ready; reference_frame_ready = false; return ready;
}

void set_diagnostics_enabled(bool enabled) {
#if defined(CPCEXPLORER_FAST_ENGINE)
    fast.diagnostics_enabled = enabled;
#else
    (void)enabled;
#endif
}

void key_down(int key_code) {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) { fast_key_down(key_code); return; }
#endif
    reference_key_down(key_code);
}

void key_up(int key_code) {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) { fast_key_up(key_code); return; }
#endif
    reference_key_up(key_code);
}

bool quickload(const std::uint8_t* data, std::size_t size, bool start) {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) return fast_quickload(data, size, start);
#endif
    return reference_quickload(data, size, start);
}

bool save_sna(std::uint8_t* destination, std::size_t size) {
    // Snapshot creation remains on the exact reference path. The production
    // fast path is a loader/runtime, and never silently emits approximate state.
    if (active_engine != EngineKind::Reference) return false;
    return reference_save_sna(destination, size);
}

bool load_sna(const std::uint8_t* data, std::size_t size) {
    return library::is_supported_snapshot(data, size) && quickload(data, size, false);
}

bool load_builtin_knight_tyme() {
#if defined(CPCEXPLORER_KNIGHT_TYME)
    return quickload(knight_tyme_sna, sizeof(knight_tyme_sna), false);
#else
    return false;
#endif
}

const std::uint8_t* framebuffer() {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) return machine.fb;
#endif
    return reference_framebuffer();
}

const std::uint32_t* palette() {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) return machine.ga.hw_colors;
#endif
    return reference_palette();
}

Diagnostics diagnostics() {
#if defined(CPCEXPLORER_FAST_ENGINE)
    if (active_engine == EngineKind::Fast) return fast_diagnostics();
#endif
    return reference_diagnostics();
}

static const std::uint8_t* reference_framebuffer() { return machine.fb; }
static const std::uint32_t* reference_palette() { return machine.ga.hw_colors; }
int framebuffer_width() { return AM40010_FRAMEBUFFER_WIDTH; }
int framebuffer_height() { return AM40010_FRAMEBUFFER_HEIGHT; }

DisplayGeometry display_geometry() {
    const unsigned columns = machine.crtc.h_displayed;
    const unsigned rows = machine.crtc.v_displayed;
    const unsigned rasters = static_cast<unsigned>(machine.crtc.max_scanline_addr) + 1U;
    const std::uint64_t width = static_cast<std::uint64_t>(columns) * 8U;
    const std::uint64_t height = static_cast<std::uint64_t>(rows) * rasters;
    // Only crop modes whose complete active area is represented by the
    // established 320x200 viewport. Unusual CRTC values retain the fallback.
    if (width < 128 || width > 320 || height < 100 || height > 200) return {};
    return {
        .x = static_cast<int>((320U - width) / 2U),
        .y = 0,
        .width = static_cast<int>(width),
        .height = static_cast<int>(height),
        .valid = true,
    };
}

static Diagnostics reference_diagnostics() {
    return {
        .z80_ticks = total_ticks,
        .pc = machine.cpu.pc,
        .state_bytes = sizeof(machine),
        .framebuffer_bytes = sizeof(machine.fb),
        .ram_bytes = sizeof(machine.ram),
        .engine = EngineKind::Reference,
        .instructions = 0,
        .peripheral_dispatches = total_ticks,
        .reconstructed_frames = total_ticks / 80'000,
        .last_emulated_us = 0,
        .last_cpu_us = 0,
        .last_event_us = 0,
        .last_reconstruction_us = 0,
    };
}

static_assert(sizeof(machine.ram) == 64 * 1024, "CPC 464 RAM profile changed");
static_assert(sizeof(machine.fb) == 768 * 272, "compact indexed framebuffer changed");
static_assert(sizeof(machine) < 320 * 1024, "emulator state exceeds Stage 1B budget");

}  // namespace cpcexplorer
