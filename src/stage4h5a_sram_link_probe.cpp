#include "../prototypes/stage4h5_realtime_ay/realtime_ay_prototype.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace {
using namespace cpcexplorer::stage4h5;

struct alignas(8) ProbeControl {
    explicit ProbeControl(AyDcBuffer* history) : dc_history(history) {}

    alignas(8) std::array<std::byte, sizeof(RealtimeAyPrototype)> renderer;
    AyDcBuffer* dc_history;
    std::atomic<std::uint32_t> producer_epoch{0};
    std::atomic<std::uint32_t> consumer_epoch{0};
    std::atomic<std::uint32_t> shutdown_request{0};
    std::uint32_t render_average_us = 0;
    std::uint32_t render_maximum_us = 0;
    std::uint32_t late_buffers = 0;
    std::uint32_t generated_samples_low = 0;
    std::uint32_t generated_samples_high = 0;
};

static_assert(sizeof(AyDcBuffer) == 2'048);
static_assert(sizeof(PsgEvent) == 8);
static_assert(sizeof(SpscPsgQueue<128>) <= 1'040);
static_assert(sizeof(CompactAyState) <= 112);
static_assert(sizeof(ProbeControl) <= 1'224);
}

extern "C" {
__attribute__((used, section(".scratch_y.stage4h5a_dc"), aligned(4)))
AyDcBuffer stage4h5a_dc_history{};

__attribute__((used, section(".scratch_x.stage4h5a_control"), aligned(8)))
ProbeControl stage4h5a_control{&stage4h5a_dc_history};

// Matches the Pico SDK 2.2.0 default PICO_CORE1_STACK_SIZE. The normal linker
// script positions .stack1 immediately below __StackOneTop in scratch X.
__attribute__((used, section(".stack1.stage4h5a"), aligned(8)))
std::array<std::uint8_t, 2'048> stage4h5a_core1_stack{};
}
