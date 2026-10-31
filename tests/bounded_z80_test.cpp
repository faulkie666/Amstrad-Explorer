#include "emulation/fast_z80_adapter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
struct Bus {
    std::array<std::uint8_t, 65536> memory{};
    std::vector<std::uint32_t> io;
    unsigned acknowledgements = 0;
};

std::uint8_t mem_read(struct z80_s*, std::uint16_t address, int, void* opaque) {
    return static_cast<Bus*>(opaque)->memory[address];
}
void mem_write(struct z80_s*, std::uint16_t address, std::uint8_t value, void* opaque) {
    static_cast<Bus*>(opaque)->memory[address] = value;
}
std::uint8_t io_read(struct z80_s*, std::uint16_t port, void* opaque) {
    auto& bus = *static_cast<Bus*>(opaque);
    bus.io.push_back(0x1000000U | port);
    return 0x5a;
}
void io_write(struct z80_s*, std::uint16_t port, std::uint8_t value, void* opaque) {
    static_cast<Bus*>(opaque)->io.push_back((static_cast<std::uint32_t>(value) << 16) | port);
}
void intack(struct z80_s*, void* opaque) { ++static_cast<Bus*>(opaque)->acknowledgements; }

struct Result {
    fast_z80_state cpu{};
    Bus bus{};
    std::uint64_t cycles = 0;
};

Result run(const std::vector<std::uint8_t>& program, unsigned span,
           fast_z80_state initial, std::uint64_t interrupt_cycle,
           std::uint64_t cycle_limit) {
    Result result{};
    std::copy(program.begin(), program.end(), result.bus.memory.begin());
    // IM 1 handler and IM 2 vector/handler both return with RETI.
    result.bus.memory[0x0038] = 0xed; result.bus.memory[0x0039] = 0x4d;
    result.bus.memory[0x80ff] = 0x00; result.bus.memory[0x8100] = 0x20;
    result.bus.memory[0x2000] = 0xed; result.bus.memory[0x2001] = 0x4d;
    fast_z80_callbacks callbacks{mem_read, mem_write, io_read, io_write, intack, &result.bus};
    fast_z80* cpu = fast_z80_create(&callbacks);
    fast_z80_load_state(cpu, &initial);
    bool interrupt_raised = false;
    while (result.cycles < cycle_limit) {
        if (!interrupt_raised && result.cycles >= interrupt_cycle) {
            fast_z80_raise_int(cpu);
            interrupt_raised = true;
        }
        std::uint64_t boundary = cycle_limit;
        if (!interrupt_raised) boundary = std::min(boundary, interrupt_cycle);
        const auto remaining = boundary > result.cycles ? boundary - result.cycles : 1;
        const int target = static_cast<int>(std::min<std::uint64_t>(span, remaining));
        result.cycles += span == 1 ? fast_z80_step(cpu) : fast_z80_run_bounded(cpu, target);
    }
    fast_z80_get_state(cpu, &result.cpu);
    fast_z80_destroy(cpu);
    return result;
}

bool equal(const Result& a, const Result& b) {
    const auto& x = a.cpu; const auto& y = b.cpu;
    return a.cycles == b.cycles && x.af == y.af && x.bc == y.bc && x.de == y.de &&
        x.hl == y.hl && x.af2 == y.af2 && x.bc2 == y.bc2 && x.de2 == y.de2 &&
        x.hl2 == y.hl2 && x.ix == y.ix && x.iy == y.iy && x.wz == y.wz &&
        x.sp == y.sp && x.pc == y.pc && x.i == y.i && x.r == y.r && x.im == y.im &&
        x.iff1 == y.iff1 && x.iff2 == y.iff2 && x.halted == y.halted &&
        a.bus.memory == b.bus.memory && a.bus.io == b.bus.io &&
        a.bus.acknowledgements == b.bus.acknowledgements;
}

bool check(const char* name, const std::vector<std::uint8_t>& program,
           fast_z80_state state, std::uint64_t interrupt_cycle = UINT64_MAX) {
    const auto one = run(program, 1, state, interrupt_cycle, 20'000);
    const auto bounded = run(program, 32, state, interrupt_cycle, 20'000);
    if (!equal(one, bounded)) {
        std::fprintf(stderr, "FAIL: %s pc=%04x/%04x cycles=%llu/%llu ack=%u/%u io=%zu/%zu\n",
            name, one.cpu.pc, bounded.cpu.pc,
            static_cast<unsigned long long>(one.cycles),
            static_cast<unsigned long long>(bounded.cycles),
            one.bus.acknowledgements, bounded.bus.acknowledgements,
            one.bus.io.size(), bounded.bus.io.size());
        return false;
    }
    std::printf("PASS: %s cycles=%llu ack=%u io=%zu\n", name,
        static_cast<unsigned long long>(one.cycles), one.bus.acknowledgements,
        one.bus.io.size());
    return true;
}
} // namespace

int main() {
    fast_z80_state base{}; base.sp = 0xff00; base.pc = 0; base.im = 1;
    const std::vector<std::uint8_t> memory_io{
        0x3e,0x12, 0x32,0x00,0x40, 0x3a,0x00,0x40,
        0xd3,0x10, 0xdb,0x10, 0xc3,0x00,0x00};
    if (!check("memory and I/O", memory_io, base)) return 1;

    auto ei = base; ei.iff1 = ei.iff2 = 0;
    const std::vector<std::uint8_t> ei_di_halt{
        0xf3, 0xfb, 0x00, 0xf3, 0xfb, 0x00, 0x76, 0xc3,0x00,0x00};
    if (!check("EI/DI delay and HALT", ei_di_halt, ei, 0)) return 2;

    auto halt = base; halt.iff1 = halt.iff2 = 1;
    const std::vector<std::uint8_t> halt_wake{0x76, 0x3c, 0xc3,0x00,0x00};
    if (!check("HALT wake-up", halt_wake, halt, 40)) return 3;

    auto im2 = base; im2.iff1 = im2.iff2 = 1; im2.im = 2; im2.i = 0x80;
    const std::vector<std::uint8_t> interrupt_loop{0x00, 0x18, 0xfd};
    if (!check("IM2 interrupt priority", interrupt_loop, im2, 128)) return 4;

    std::puts("PASS: bounded Z80 synthetic differential suite");
    return 0;
}
