#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer {

enum class EngineKind : std::uint8_t {
    Reference,
    Fast,
};

// Host key code reserved for the physical CPC Shift key. ASCII upper-case
// characters still synthesize Shift plus their character in the normal map.
inline constexpr int KeyShift = 0x02;

struct Diagnostics {
    std::uint64_t z80_ticks;
    std::uint16_t pc;
    std::size_t state_bytes;
    std::size_t framebuffer_bytes;
    std::size_t ram_bytes;
    EngineKind engine;
    std::uint64_t instructions;
    std::uint64_t peripheral_dispatches;
    std::uint64_t reconstructed_frames;
    std::uint32_t last_emulated_us;
    std::uint32_t last_cpu_us;
    std::uint32_t last_event_us;
    std::uint32_t last_reconstruction_us;
};

struct DisplayGeometry {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
};

using AudioCallback = void (*)(const float*, std::size_t, void*);
struct AudioDiagnostics { std::uint64_t samples = 0, nonzero_samples = 0, register_writes = 0; };
inline constexpr unsigned AudioSampleRate = 22050;
void set_audio_callback(AudioCallback callback, void* user = nullptr);
AudioDiagnostics audio_diagnostics();

// The reference engine remains the deterministic default. Selection is
// explicit and takes effect on the next init/reset.
bool select_engine(EngineKind engine);
EngineKind selected_engine();
bool engine_available(EngineKind engine);
const char* engine_name(EngineKind engine);

void init();
void reset();
void run_us(std::uint32_t microseconds);
// Returns true once for each newly reconstructed/presentable framebuffer.
// Reference mode reports every completed run slice; fast mode may decouple
// presentation from the 4 MHz emulated-time budget.
bool consume_presented_frame();
// Enables bounded per-slice timing only while a diagnostic host is attached.
void set_diagnostics_enabled(bool enabled);
void key_down(int key_code);
void key_up(int key_code);
bool quickload(const std::uint8_t* data, std::size_t size, bool start);
std::size_t sna_size();
bool save_sna(std::uint8_t* destination, std::size_t size);
bool load_sna(const std::uint8_t* data, std::size_t size);
bool load_builtin_knight_tyme();
const std::uint8_t* framebuffer();
const std::uint32_t* palette();
int framebuffer_width();
int framebuffer_height();
DisplayGeometry display_geometry();
Diagnostics diagnostics();

namespace testing {
using AyWriteCallback = void (*)(std::uint64_t cycle, std::uint8_t reg,
                                 std::uint8_t value, void* user);
void set_ay_write_callback(AyWriteCallback callback, void* user = nullptr);
void ay_write_register(std::uint8_t reg, std::uint8_t value);
std::uint8_t ay_read_register(std::uint8_t reg);
void ay_advance_ticks(std::uint32_t ticks);
}

}  // namespace cpcexplorer
