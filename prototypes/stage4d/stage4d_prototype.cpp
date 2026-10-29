#include "fast_z80_adapter.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
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
constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
constexpr int kBareShift = 0x02;

std::uint64_t hash_bytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = kFnvOffset;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

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

bool init_cpc(cpc_t& machine, std::span<const std::uint8_t> rom) {
    if (rom.size() != 32 * 1024) return false;
    cpc_desc_t desc{};
    desc.type = CPC_TYPE_464;
    desc.joystick_type = CPC_JOYSTICK_NONE;
    desc.audio.num_samples = 128;
    desc.audio.sample_rate = 22050;
    desc.audio.volume = 0.0f;
    desc.roms.cpc464.os.ptr = const_cast<std::uint8_t*>(rom.data());
    desc.roms.cpc464.os.size = 16 * 1024;
    desc.roms.cpc464.basic.ptr = const_cast<std::uint8_t*>(rom.data() + 16 * 1024);
    desc.roms.cpc464.basic.size = 16 * 1024;
    cpc_init(&machine, &desc);
    kbd_register_key(&machine.kbd, kBareShift, 2, 5, 0);
    cpc_reset(&machine);
    return true;
}

void apply_deterministic_input(std::string_view name, std::size_t instruction,
                               cpc_t& reference, cpc_t& fast) {
    if (name != "manic_miner") return;
    auto down = [&](int key) {
        cpc_key_down(&reference, key);
        cpc_key_down(&fast, key);
    };
    auto up = [&](int key) {
        cpc_key_up(&reference, key);
        cpc_key_up(&fast, key);
    };
    switch (instruction) {
        case 1000: down('q'); break;
        case 1500: up('q'); break;
        case 2000: down('w'); break;
        case 2500: up('w'); break;
        case 3000: down(kBareShift); break;
        case 3250: up(kBareShift); break;
        default: break;
    }
}

bool load_sna(cpc_t& machine, std::span<const std::uint8_t> sna) {
    if (sna.size() != sizeof(_cpc_sna_header) + 0x10000U) return false;
    chips_range_t range{const_cast<std::uint8_t*>(sna.data()), sna.size()};
    return cpc_quickload(&machine, range, false);
}

std::vector<std::uint8_t> save_sna(cpc_t& machine) {
    while (!z80_opdone(&machine.cpu)) machine.pins = _cpc_tick(&machine, machine.pins);
    std::vector<std::uint8_t> result(sizeof(_cpc_sna_header) + 0x10000U);
    _cpc_sna_header header{};
    const std::uint8_t magic[8] = {'M', 'V', ' ', '-', ' ', 'S', 'N', 'A'};
    std::memcpy(header.magic, magic, sizeof(magic));
    header.version = 1;
    header.F = machine.cpu.f; header.A = machine.cpu.a;
    header.C = machine.cpu.c; header.B = machine.cpu.b;
    header.E = machine.cpu.e; header.D = machine.cpu.d;
    header.L = machine.cpu.l; header.H = machine.cpu.h;
    header.R = machine.cpu.r; header.I = machine.cpu.i;
    header.IFF1 = machine.cpu.iff1; header.IFF2 = machine.cpu.iff2;
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
    std::memcpy(result.data(), &header, sizeof(header));
    std::memcpy(result.data() + sizeof(header), machine.ram, 0x10000U);
    return result;
}

fast_z80_state state_from_chips(const z80_t& cpu, bool boundary_pc) {
    fast_z80_state state{};
    state.af = cpu.af; state.bc = cpu.bc; state.de = cpu.de; state.hl = cpu.hl;
    state.af2 = cpu.af2; state.bc2 = cpu.bc2; state.de2 = cpu.de2; state.hl2 = cpu.hl2;
    state.ix = cpu.ix; state.iy = cpu.iy; state.wz = cpu.wz; state.sp = cpu.sp;
    state.pc = boundary_pc ? static_cast<std::uint16_t>(cpu.pc - 1) : cpu.pc;
    state.i = cpu.i; state.r = cpu.r; state.im = cpu.im;
    state.iff1 = cpu.iff1; state.iff2 = cpu.iff2;
    state.halted = (cpu.pins & Z80_HALT) != 0;
    return state;
}

struct Summary {
    fast_z80_state cpu{};
    std::uint64_t ram = 0;
    std::uint64_t framebuffer = 0;
    std::uint64_t gate_array = 0;
    std::uint64_t crtc = 0;
    std::uint64_t ppi = 0;
    std::uint64_t psg = 0;
    std::uint64_t ticks = 0;
    std::uint32_t ga_sequence = 0;
    std::uint64_t ga_pins = 0;
};

Summary summarize(const cpc_t& machine, fast_z80_state cpu, std::uint64_t ticks) {
    Summary result{};
    result.cpu = cpu;
    result.ram = hash_bytes(machine.ram, sizeof(machine.ram));
    result.framebuffer = hash_bytes(machine.fb, sizeof(machine.fb));
    result.gate_array = hash_bytes(&machine.ga.regs, sizeof(machine.ga.regs));
    result.gate_array ^= hash_bytes(&machine.ga.video, sizeof(machine.ga.video));
    result.crtc = hash_bytes(&machine.crtc, sizeof(machine.crtc));
    result.ppi = hash_bytes(&machine.ppi, sizeof(machine.ppi));
    result.psg = hash_bytes(machine.psg.reg, sizeof(machine.psg.reg));
    result.psg ^= static_cast<std::uint64_t>(machine.psg.addr) << 56;
    result.ticks = ticks;
    result.ga_sequence = machine.ga.seq_tick_count;
    result.ga_pins = machine.ga.pins &
                     (AM40010_READY | AM40010_INT | AM40010_SYNC |
                      AM40010_DE | AM40010_HS | AM40010_VS);
    return result;
}

bool same_visible_cpu(const fast_z80_state& a, const fast_z80_state& b) {
    return a.af == b.af && a.bc == b.bc && a.de == b.de && a.hl == b.hl &&
           a.af2 == b.af2 && a.bc2 == b.bc2 && a.de2 == b.de2 && a.hl2 == b.hl2 &&
           a.ix == b.ix && a.iy == b.iy && a.wz == b.wz && a.sp == b.sp &&
           a.pc == b.pc && a.i == b.i && a.r == b.r && a.im == b.im &&
           a.iff1 == b.iff1 && a.iff2 == b.iff2 && a.halted == b.halted;
}

bool same_summary(const Summary& a, const Summary& b) {
    return same_visible_cpu(a.cpu, b.cpu) && a.ram == b.ram &&
           a.framebuffer == b.framebuffer && a.gate_array == b.gate_array &&
           a.crtc == b.crtc && a.ppi == b.ppi && a.psg == b.psg &&
           a.ticks == b.ticks && a.ga_sequence == b.ga_sequence &&
           a.ga_pins == b.ga_pins;
}

std::uint64_t reference_step(cpc_t& machine, const char* trace_name = nullptr) {
    std::uint64_t ticks = 0;
    bool entered_instruction = false;
    do {
        if (trace_name) {
            std::printf("REFTRACE game=%s tick=%llu pre_step=%d pre_seq=%u pre_wait=%d "
                        "pre_bus=%llx\n", trace_name,
                        static_cast<unsigned long long>(ticks), machine.cpu.step,
                        machine.ga.seq_tick_count, (machine.pins & Z80_WAIT) != 0,
                        static_cast<unsigned long long>(machine.pins &
                            (Z80_M1 | Z80_MREQ | Z80_IORQ | Z80_RD | Z80_WR)));
        }
        machine.pins = _cpc_tick(&machine, machine.pins);
        ++ticks;
        if (trace_name) {
            std::printf("REFTRACE game=%s tick=%llu post_step=%d post_seq=%u post_wait=%d "
                        "post_bus=%llx opdone=%d\n", trace_name,
                        static_cast<unsigned long long>(ticks), machine.cpu.step,
                        machine.ga.seq_tick_count, (machine.pins & Z80_WAIT) != 0,
                        static_cast<unsigned long long>(machine.pins &
                            (Z80_M1 | Z80_MREQ | Z80_IORQ | Z80_RD | Z80_WR)),
                        z80_opdone(&machine.cpu));
        }
        if (!z80_opdone(&machine.cpu)) entered_instruction = true;
    } while (!entered_instruction || !z80_opdone(&machine.cpu));
    return ticks;
}

struct FastMachine {
    cpc_t cpc{};
    fast_z80* cpu = nullptr;
    std::uint64_t ticks = 0;
    std::uint64_t instruction_start = 0;
    std::uint64_t instructions = 0;
    std::uint64_t bus_events = 0;
    std::uint64_t peripheral_dispatches = 0;
    std::uint64_t pins = 0;
    const char* trace_name = nullptr;

    ~FastMachine() { fast_z80_destroy(cpu); }
};

void sync_to(FastMachine& machine, std::uint64_t target) {
    while (machine.ticks < target) {
        machine.pins = am40010_tick(&machine.cpc.ga, 0) & Z80_PIN_MASK;
        ++machine.ticks;
        ++machine.peripheral_dispatches;
    }
}

void prepare_bus(FastMachine& machine, int op_tstate, int cycle_length,
                 const char* bus_kind, int wait_sample_bias = 0) {
    const int sample = std::max(0, op_tstate - cycle_length + wait_sample_bias);
    sync_to(machine, machine.instruction_start + static_cast<std::uint64_t>(sample));
    const auto seq_at_sample = machine.cpc.ga.seq_tick_count;
    int waits = 0;
    while ((machine.pins & Z80_WAIT) && waits < 4) {
        sync_to(machine, machine.ticks + 1);
        ++waits;
    }
    if (waits) fast_z80_add_wait(machine.cpu, waits);
    sync_to(machine, machine.instruction_start + static_cast<std::uint64_t>(op_tstate + waits));
    ++machine.bus_events;
    if (machine.trace_name) {
        std::printf("FASTTRACE game=%s bus=%s op_tstate=%d sample=%d seq=%u waits=%d "
                    "end_tick=%llu end_seq=%u\n", machine.trace_name, bus_kind,
                    op_tstate, sample, seq_at_sample, waits,
                    static_cast<unsigned long long>(machine.ticks),
                    machine.cpc.ga.seq_tick_count);
    }
}

std::uint64_t io_transaction(cpc_t& sys, std::uint16_t port, bool read,
                             std::uint8_t value) {
    std::uint64_t pins = Z80_IORQ;
    pins |= static_cast<std::uint64_t>(port);
    pins |= read ? Z80_RD : Z80_WR;
    Z80_SET_DATA(pins, value);

    if ((pins & Z80_A11) == 0) {
        std::uint64_t ppi_pins = (pins & Z80_PIN_MASK & ~(I8255_PC_PINS|I8255_A1|I8255_A0)) | I8255_CS;
        if (pins & Z80_A9) ppi_pins |= I8255_A1;
        if (pins & Z80_A8) ppi_pins |= I8255_A0;
        if ((sys.ppi.pins & (I8255_PC7|I8255_PC6)) != 0) {
            std::uint64_t ay_pins = 0;
            if (sys.ppi.pins & I8255_PC7) ay_pins |= AY38910_BDIR;
            if (sys.ppi.pins & I8255_PC6) ay_pins |= AY38910_BC1;
            AY38910_SET_DATA(ay_pins, I8255_GET_PA(sys.ppi.pins));
            ay_pins = ay38910_iorq(&sys.psg, ay_pins);
            I8255_SET_PA(ppi_pins, AY38910_GET_DATA(ay_pins));
        }
        ppi_pins |= I8255_PB1|I8255_PB2|I8255_PB3|I8255_PB4;
        if (sys.crtc.vs) ppi_pins |= I8255_PB0;
        ppi_pins = i8255_tick(&sys.ppi, ppi_pins);
        if ((ppi_pins & (I8255_CS|I8255_RD)) == (I8255_CS|I8255_RD)) {
            Z80_SET_DATA(pins, I8255_GET_DATA(ppi_pins));
        }
        if ((ppi_pins & (I8255_PC7|I8255_PC6)) != 0) {
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

std::uint8_t fast_mem_read(void* opaque, std::uint16_t address, bool m1, int tstate) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    // A loaded chips SNA begins before the first M1/T1. Subsequent differential
    // checkpoints are at chips' overlapped M1/T2 boundary, where next M1/T1 has
    // already driven the bus. Only the first candidate fetch therefore samples
    // WAIT one tick after the common origin.
    const int m1_sample_bias = (m1 && machine.instructions == 0) ? 1 : 0;
    prepare_bus(machine, tstate, m1 ? 4 : 3, m1 ? "m1-read" : "mem-read",
                m1_sample_bias);
    return mem_rd(&machine.cpc.mem, address);
}

void fast_mem_write(void* opaque, std::uint16_t address, std::uint8_t value, int tstate) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    prepare_bus(machine, tstate, 3, "mem-write");
    mem_wr(&machine.cpc.mem, address, value);
}

std::uint8_t fast_io_read(void* opaque, std::uint16_t port, int tstate) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    prepare_bus(machine, tstate, 4, "io-read");
    return Z80_GET_DATA(io_transaction(machine.cpc, port, true, 0xFF));
}

void fast_io_write(void* opaque, std::uint16_t port, std::uint8_t value, int tstate) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    prepare_bus(machine, tstate, 4, "io-write");
    io_transaction(machine.cpc, port, false, value);
}

void fast_intack(void* opaque, int tstate) {
    auto& machine = *static_cast<FastMachine*>(opaque);
    sync_to(machine, machine.instruction_start + static_cast<std::uint64_t>(std::max(0, tstate)));
    machine.cpc.ga.seq_tick_count = 0;
    if (machine.cpc.ga.video.intr) {
        machine.cpc.ga.video.intcnt &= 0x1F;
        machine.cpc.ga.video.intr = false;
    }
}

bool init_fast(FastMachine& fast, std::span<const std::uint8_t> rom,
               std::span<const std::uint8_t> sna) {
    fast_z80_destroy(fast.cpu);
    fast.cpu = nullptr;
    fast.ticks = 0;
    fast.instruction_start = 0;
    fast.instructions = 0;
    fast.bus_events = 0;
    fast.peripheral_dispatches = 0;
    fast.pins = 0;
    fast.trace_name = nullptr;
    if (!init_cpc(fast.cpc, rom) || !load_sna(fast.cpc, sna)) return false;
    fast_z80_callbacks callbacks{
        fast_mem_read, fast_mem_write, fast_io_read, fast_io_write,
        fast_intack, &fast};
    fast.cpu = fast_z80_create(&callbacks);
    if (!fast.cpu) return false;
    auto initial = state_from_chips(fast.cpc.cpu, false);
    fast_z80_load_state(fast.cpu, &initial);
    return true;
}

int step_fast(FastMachine& machine) {
    if (machine.pins & Z80_INT) fast_z80_raise_int(machine.cpu);
    machine.instruction_start = machine.ticks;
    const int cycles = fast_z80_step(machine.cpu);
    sync_to(machine, machine.instruction_start + static_cast<std::uint64_t>(cycles));
    ++machine.instructions;
    return cycles;
}

void print_difference(std::string_view name, std::uint64_t checkpoint,
                      const Summary& reference, const Summary& fast) {
    const bool cpu = same_visible_cpu(reference.cpu, fast.cpu);
    const bool ram = reference.ram == fast.ram;
    const bool fb = reference.framebuffer == fast.framebuffer;
    const bool ga = reference.gate_array == fast.gate_array;
    const bool crtc = reference.crtc == fast.crtc;
    const bool ppi = reference.ppi == fast.ppi;
    const bool psg = reference.psg == fast.psg;
    const bool timing = reference.ticks == fast.ticks &&
                        reference.ga_sequence == fast.ga_sequence &&
                        reference.ga_pins == fast.ga_pins;
    std::printf(
        "DIFF game=%.*s instructions=%llu cpu=%s ram=%s fb=%s ga=%s crtc=%s ppi=%s psg=%s "
        "timing=%s ref_ticks=%llu fast_ticks=%llu ref_seq=%u fast_seq=%u "
        "ref_wait=%d fast_wait=%d ref_pc=%04x fast_pc=%04x\n",
        static_cast<int>(name.size()), name.data(),
        static_cast<unsigned long long>(checkpoint), cpu ? "same" : "DIFF",
        ram ? "same" : "DIFF", fb ? "same" : "DIFF", ga ? "same" : "DIFF",
        crtc ? "same" : "DIFF", ppi ? "same" : "DIFF", psg ? "same" : "DIFF",
        timing ? "same" : "DIFF",
        static_cast<unsigned long long>(reference.ticks),
        static_cast<unsigned long long>(fast.ticks), reference.ga_sequence,
        fast.ga_sequence, (reference.ga_pins & AM40010_READY) != 0,
        (fast.ga_pins & AM40010_READY) != 0, reference.cpu.pc, fast.cpu.pc);
}

struct ScenarioResult {
    double reference_ms = 0;
    double fast_ms = 0;
    std::uint64_t reference_ticks = 0;
    std::uint64_t fast_ticks = 0;
    std::uint64_t fast_bus_events = 0;
    std::size_t first_difference = 0;
};

ScenarioResult run_scenario(std::string_view name, std::span<const std::uint8_t> rom,
                            std::span<const std::uint8_t> sna,
                            std::size_t instruction_count) {
    cpc_t reference{};
    FastMachine fast{};
    if (!init_cpc(reference, rom) || !load_sna(reference, sna) ||
        !init_fast(fast, rom, sna)) {
        std::fprintf(stderr, "FAIL: unable to initialize %.*s\n",
                     static_cast<int>(name.size()), name.data());
        return {};
    }

    const std::array<std::size_t, 7> checkpoints{0, 1, 10, 100, 1000, 10000,
                                                 instruction_count};
    std::uint64_t reference_ticks = 0;
    std::size_t first_difference = 0;
    auto report = [&](std::size_t index) {
        fast_z80_state fast_cpu{};
        fast_z80_get_state(fast.cpu, &fast_cpu);
        const auto ref = summarize(reference, state_from_chips(reference.cpu, index != 0),
                                   reference_ticks);
        const auto candidate = summarize(fast.cpc, fast_cpu, fast.ticks);
        if (!first_difference && !same_summary(ref, candidate)) {
            first_difference = index;
        }
        print_difference(name, index, ref, candidate);
    };
    report(0);

    auto ref_start = Clock::now();
    std::size_t next_checkpoint = 1;
    for (std::size_t i = 1; i <= instruction_count; ++i) {
        apply_deterministic_input(name, i, reference, fast.cpc);
        const char* trace_name = (i == 1) ? name.data() : nullptr;
        reference_ticks += reference_step(reference, trace_name);
        fast.trace_name = trace_name;
        step_fast(fast);
        fast.trace_name = nullptr;
        if (!first_difference) {
            fast_z80_state fast_cpu{};
            fast_z80_get_state(fast.cpu, &fast_cpu);
            const auto ref = summarize(reference,
                state_from_chips(reference.cpu, true), reference_ticks);
            const auto candidate = summarize(fast.cpc, fast_cpu, fast.ticks);
            if (!same_summary(ref, candidate)) {
                first_difference = i;
                std::printf("FIRST_");
                print_difference(name, i, ref, candidate);
            }
        }
        if (i == checkpoints[next_checkpoint]) {
            report(i);
            ++next_checkpoint;
        }
    }
    auto ref_end = Clock::now();

    cpc_t reference_benchmark{};
    FastMachine fast_benchmark{};
    init_cpc(reference_benchmark, rom); load_sna(reference_benchmark, sna);
    init_fast(fast_benchmark, rom, sna);
    const auto baseline_start = Clock::now();
    std::uint64_t baseline_ticks = 0;
    for (std::size_t i = 0; i < instruction_count; ++i) {
        apply_deterministic_input(name, i + 1, reference_benchmark, fast_benchmark.cpc);
        baseline_ticks += reference_step(reference_benchmark);
    }
    const auto baseline_end = Clock::now();
    const auto candidate_start = Clock::now();
    // Recreate the candidate so its benchmark sees the same input schedule but
    // does not inherit the reference benchmark's keyboard matrix mutations.
    init_fast(fast_benchmark, rom, sna);
    cpc_t unused_reference{};
    init_cpc(unused_reference, rom); load_sna(unused_reference, sna);
    for (std::size_t i = 0; i < instruction_count; ++i) {
        apply_deterministic_input(name, i + 1, unused_reference, fast_benchmark.cpc);
        step_fast(fast_benchmark);
    }
    const auto candidate_end = Clock::now();

    (void)ref_start;
    (void)ref_end;
    ScenarioResult result{};
    result.reference_ms = std::chrono::duration<double, std::milli>(baseline_end - baseline_start).count();
    result.fast_ms = std::chrono::duration<double, std::milli>(candidate_end - candidate_start).count();
    result.reference_ticks = baseline_ticks;
    result.fast_ticks = fast_benchmark.ticks;
    result.fast_bus_events = fast_benchmark.bus_events;
    result.first_difference = first_difference;
    std::printf(
        "BENCH game=%.*s instructions=%zu reference_ms=%.3f fast_ms=%.3f ratio=%.3f "
        "reference_tick_dispatch=%llu fast_instruction_dispatch=%zu fast_peripheral_dispatch=%llu "
        "fast_bus_events=%llu\n",
        static_cast<int>(name.size()), name.data(), instruction_count,
        result.reference_ms, result.fast_ms,
        result.fast_ms > 0.0 ? result.reference_ms / result.fast_ms : 0.0,
        static_cast<unsigned long long>(baseline_ticks), instruction_count,
        static_cast<unsigned long long>(fast_benchmark.peripheral_dispatches),
        static_cast<unsigned long long>(fast_benchmark.bus_events));
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s CPC464_ROM KNIGHT_SNA MANIC_SNA\n", argv[0]);
        return 2;
    }
    const auto rom = read_file(argv[1]);
    const auto knight = read_file(argv[2]);
    const auto manic = read_file(argv[3]);
    if (rom.size() != 32768 || knight.size() != 65792 || manic.size() != 65792) {
        std::fprintf(stderr, "FAIL: invalid ROM or snapshot size\n");
        return 3;
    }

    cpc_t basic_boot{};
    if (!init_cpc(basic_boot, rom)) return 4;
    cpc_exec(&basic_boot, 5'000'000);
    const auto basic = save_sna(basic_boot);

    constexpr std::size_t kInstructions = 50'000;
    const auto basic_result = run_scenario("basic", rom, basic, kInstructions);
    const auto knight_result = run_scenario("knight_tyme", rom, knight, kInstructions);
    const auto manic_result = run_scenario("manic_miner", rom, manic, kInstructions);

    const bool credible_dispatch_reduction =
        basic_result.reference_ticks > kInstructions * 4ULL &&
        knight_result.reference_ticks > kInstructions * 4ULL &&
        manic_result.reference_ticks > kInstructions * 4ULL;
    const bool correct = basic_result.first_difference == 0 &&
                         knight_result.first_difference == 0 &&
                         manic_result.first_difference == 0;
    std::printf("RESULT dispatch_reduction=%s correctness=%s first_diff=%zu/%zu/%zu\n",
                credible_dispatch_reduction ? "yes" : "no",
                correct ? "pass" : "HOLD",
                basic_result.first_difference, knight_result.first_difference,
                manic_result.first_difference);
    return correct ? 0 : 1;
}

