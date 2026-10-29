#include "cpc_core.h"

#include <cassert>

#include "cpc_rom.h"

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

namespace cpcexplorer {
namespace {

cpc_t machine;
std::uint64_t total_ticks;

}  // namespace

void init() {
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
    cpc_reset(&machine);
    total_ticks = 0;
}

void reset() {
    cpc_reset(&machine);
    total_ticks = 0;
}

void run_us(std::uint32_t microseconds) {
    total_ticks += cpc_exec(&machine, microseconds);
}

void key_down(int key_code) { cpc_key_down(&machine, key_code); }
void key_up(int key_code) { cpc_key_up(&machine, key_code); }

const std::uint8_t* framebuffer() { return machine.fb; }
const std::uint32_t* palette() { return machine.ga.hw_colors; }
int framebuffer_width() { return AM40010_FRAMEBUFFER_WIDTH; }
int framebuffer_height() { return AM40010_FRAMEBUFFER_HEIGHT; }

Diagnostics diagnostics() {
    return {
        .z80_ticks = total_ticks,
        .pc = machine.cpu.pc,
        .state_bytes = sizeof(machine),
        .framebuffer_bytes = sizeof(machine.fb),
        .ram_bytes = sizeof(machine.ram),
    };
}

static_assert(sizeof(machine.ram) == 64 * 1024, "CPC 464 RAM profile changed");
static_assert(sizeof(machine.fb) == 768 * 272, "compact indexed framebuffer changed");
static_assert(sizeof(machine) < 320 * 1024, "emulator state exceeds Stage 1B budget");

}  // namespace cpcexplorer
