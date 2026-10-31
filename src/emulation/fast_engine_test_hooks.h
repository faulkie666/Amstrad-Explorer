#pragma once

#include <cstdint>

namespace cpcexplorer::testing {

// Host benchmark decomposition only. Production code must leave Full selected.
enum class FastBenchmarkMode {
    Full,
    EventsWithoutPresentation,
    CpuOnly,
};

void set_fast_benchmark_mode(FastBenchmarkMode mode);
void set_fast_audio_enabled(bool enabled);
void set_fast_span_cycles(unsigned cycles);

struct FastStateDigest {
    std::uint64_t machine_hash;
    std::uint64_t gate_array_hash;
    std::uint64_t crtc_hash;
    std::uint64_t ppi_hash;
    std::uint64_t psg_hash;
    std::uint64_t ram_hash;
    std::uint64_t framebuffer_hash;
    std::uint64_t cycles;
    std::uint16_t af, bc, de, hl, ix, iy, sp, pc;
    std::uint8_t i, r, im, iff1, iff2;
    bool halted;
};

FastStateDigest fast_state_digest();

}  // namespace cpcexplorer::testing
