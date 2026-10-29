#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer {

struct Diagnostics {
    std::uint64_t z80_ticks;
    std::uint16_t pc;
    std::size_t state_bytes;
    std::size_t framebuffer_bytes;
    std::size_t ram_bytes;
};

void init();
void reset();
void run_us(std::uint32_t microseconds);
void key_down(int key_code);
void key_up(int key_code);
const std::uint8_t* framebuffer();
const std::uint32_t* palette();
int framebuffer_width();
int framebuffer_height();
Diagnostics diagnostics();

}  // namespace cpcexplorer
