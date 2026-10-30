#if defined(CPCEXPLORER_DUAL_CORE_AY)
#define ay38910_init stage4h5_private_ay38910_init
#define ay38910_reset stage4h5_private_ay38910_reset
#define ay38910_tick stage4h5_private_ay38910_tick
#define ay38910_iorq stage4h5_private_ay38910_iorq
#define ay38910_set_register stage4h5_private_ay38910_set_register
#define ay38910_set_addr_latch stage4h5_private_ay38910_set_addr_latch
#define ay38910_snapshot_onsave stage4h5_private_ay38910_snapshot_onsave
#define ay38910_snapshot_onload stage4h5_private_ay38910_snapshot_onload
#endif
#define CHIPS_IMPL
#define CHIPS_NO_ASSERT
#include "realtime_ay_prototype.h"
#if defined(CPCEXPLORER_DUAL_CORE_AY)
#undef ay38910_init
#undef ay38910_reset
#undef ay38910_tick
#undef ay38910_iorq
#undef ay38910_set_register
#undef ay38910_set_addr_latch
#undef ay38910_snapshot_onsave
#undef ay38910_snapshot_onload
#endif

#include <algorithm>
#include <cstdint>
#include <limits>

namespace cpcexplorer::stage4h5 {
namespace {
bool due(std::uint32_t now, std::uint32_t target) {
    return static_cast<std::int32_t>(now - target) >= 0;
}

std::uint32_t advance_counter(std::uint16_t& counter, std::uint16_t period,
                              std::uint32_t events) {
    if (events == 0) return 0;
    const std::uint32_t first = counter >= period ? 1U : period - counter;
    if (events < first) {
        counter = static_cast<std::uint16_t>(counter + events);
        return 0;
    }
    const std::uint32_t after_first = events - first;
    counter = static_cast<std::uint16_t>(after_first % period);
    return 1U + after_first / period;
}
}

RealtimeAyPrototype::RealtimeAyPrototype(AyDcBuffer& dc_buffer,
                                         Advancement advancement)
    : dc_buffer_(&dc_buffer), advancement_(advancement) {
    initialize_state();
}

void RealtimeAyPrototype::initialize_state() {
    ay_ = {};
    dc_buffer_->fill(0.0F);
    ay_.noise.rng = 1;
    ay_.sample_period =
        (AyTicksPerSecond * AY38910_FIXEDPOINT_SCALE) / SampleRate;
    ay_.sample_counter = ay_.sample_period;
    ay_.magnitude = 0.2F;
    set_register(7, 0);
    set_register(13, 0);
    applied_events_ = 0;
    sequence_errors_ = 0;
    last_sequence_ = 0;
}

void RealtimeAyPrototype::set_register(std::uint8_t reg, std::uint8_t value) {
    if (reg >= AY38910_NUM_REGISTERS) return;
    ay_.reg[reg] = value & _ay38910_reg_mask[reg];
    for (int index = 0; index < AY38910_NUM_CHANNELS; ++index) {
        auto& channel = ay_.tone[index];
        channel.period = static_cast<std::uint16_t>(
            (ay_.reg[2 * index + 1] << 8U) | ay_.reg[2 * index]);
        if (channel.period == 0) channel.period = 1;
        channel.tone_disable = (ay_.reg[AY38910_REG_ENABLE] >> index) & 1U;
        channel.noise_disable =
            (ay_.reg[AY38910_REG_ENABLE] >> (3 + index)) & 1U;
    }
    ay_.noise.period = ay_.reg[AY38910_REG_PERIOD_NOISE];
    if (ay_.noise.period == 0) ay_.noise.period = 1;
    ay_.env.period = static_cast<std::uint16_t>(
        (ay_.reg[AY38910_REG_ENV_PERIOD_COARSE] << 8U) |
        ay_.reg[AY38910_REG_ENV_PERIOD_FINE]);
    if (ay_.env.period == 0) ay_.env.period = 1;
    if (reg == AY38910_REG_ENV_SHAPE_CYCLE) {
        ay_.env.shape_holding = false;
        ay_.env.shape_counter = 0;
        const auto shape = ay_.reg[AY38910_REG_ENV_SHAPE_CYCLE];
        ay_.env.shape_hold = !(shape & AY38910_ENV_CONTINUE) ||
                             (shape & AY38910_ENV_HOLD);
    }
}

float RealtimeAyPrototype::dc_adjust(float sample) {
    ay_.dc_sum -= (*dc_buffer_)[ay_.dc_position];
    ay_.dc_sum += sample;
    (*dc_buffer_)[ay_.dc_position] = sample;
    ay_.dc_position = (ay_.dc_position + 1U) & (AY38910_DCADJ_BUFLEN - 1U);
    return sample - (ay_.dc_sum / AY38910_DCADJ_BUFLEN);
}

void RealtimeAyPrototype::start(std::uint32_t epoch) {
    epoch_.store(epoch, std::memory_order_release);
    running_.store(true, std::memory_order_release);
}

void RealtimeAyPrototype::stop() {
    running_.store(false, std::memory_order_release);
}

void RealtimeAyPrototype::reset_while_stopped(std::uint32_t epoch) {
    if (running_.load(std::memory_order_acquire)) return;
    queue_.reset_while_stopped();
    initialize_state();
    epoch_.store(epoch, std::memory_order_release);
}

void RealtimeAyPrototype::apply_due_events() {
    PsgEvent event{};
    while (queue_.peek(event) && due(ay_.tick, event.target_ay_tick)) {
        queue_.pop(event);
        if (event.reg < AY38910_REG_IO_PORT_A) set_register(event.reg, event.value);
        if (applied_events_ != 0 &&
            event.sequence != static_cast<std::uint16_t>(last_sequence_ + 1U))
            ++sequence_errors_;
        last_sequence_ = event.sequence;
        ++applied_events_;
    }
}

std::size_t RealtimeAyPrototype::render(float* output, std::size_t sample_count) {
    if (!running_.load(std::memory_order_acquire)) {
        std::fill(output, output + sample_count, 0.0F);
        return sample_count;
    }
    return advancement_ == Advancement::Span
        ? render_span(output, sample_count)
        : render_tick(output, sample_count);
}

std::size_t RealtimeAyPrototype::render_tick(float* output, std::size_t sample_count) {
    std::size_t produced = 0;
    while (produced < sample_count) {
        apply_due_events();
        ++ay_.tick;
        if ((ay_.tick & 7U) == 0) {
            for (auto& channel : ay_.tone) {
                if (++channel.counter >= channel.period) {
                    channel.counter = 0;
                    channel.bit ^= 1U;
                }
            }
            if (++ay_.noise.counter >= ay_.noise.period) {
                ay_.noise.counter = 0;
                ay_.noise.bit ^= 1U;
                if (ay_.noise.bit != 0) {
                    ay_.noise.rng ^= (((ay_.noise.rng & 1U) ^
                                       ((ay_.noise.rng >> 3U) & 1U)) << 17U);
                    ay_.noise.rng >>= 1U;
                }
            }
        }
        if ((ay_.tick & 15U) == 0 && ++ay_.env.counter >= ay_.env.period) {
            ay_.env.counter = 0;
            if (!ay_.env.shape_holding) {
                ay_.env.shape_counter = (ay_.env.shape_counter + 1U) & 0x1FU;
                if (ay_.env.shape_hold && ay_.env.shape_counter == 0x1FU)
                    ay_.env.shape_holding = true;
            }
            ay_.env.shape_state = _ay38910_shapes[
                ay_.reg[AY38910_REG_ENV_SHAPE_CYCLE]][ay_.env.shape_counter];
        }
        ay_.sample_counter -= AY38910_FIXEDPOINT_SCALE;
        if (ay_.sample_counter <= 0) {
            ay_.sample_counter += ay_.sample_period;
            float mixed = 0.0F;
            for (int index = 0; index < AY38910_NUM_CHANNELS; ++index) {
                const auto& channel = ay_.tone[index];
                const float volume =
                    (ay_.reg[AY38910_REG_AMP_A + index] & (1U << 4U))
                    ? _ay38910_volumes[ay_.env.shape_state]
                    : _ay38910_volumes[ay_.reg[AY38910_REG_AMP_A + index] & 0x0FU];
                const int enabled = (channel.bit | channel.tone_disable) &
                    ((ay_.noise.rng & 1U) | channel.noise_disable);
                if (enabled) mixed += volume;
            }
            ay_.sample = dc_adjust(mixed) * ay_.magnitude;
            output[produced++] = ay_.sample;
        }
    }
    return produced;
}

std::size_t RealtimeAyPrototype::render_span(float* output, std::size_t sample_count) {
    std::size_t produced = 0;
    while (produced < sample_count) {
        apply_due_events();
        const auto to_sample = static_cast<std::uint32_t>(
            (ay_.sample_counter + AY38910_FIXEDPOINT_SCALE - 1) /
            AY38910_FIXEDPOINT_SCALE);
        std::uint32_t to_event = std::numeric_limits<std::uint32_t>::max();
        PsgEvent next{};
        if (queue_.peek(next)) {
            const auto delta = static_cast<std::int32_t>(next.target_ay_tick - ay_.tick);
            if (delta > 0) to_event = static_cast<std::uint32_t>(delta);
        }
        const std::uint32_t step = std::min(to_sample, to_event);
        const std::uint32_t old_tick = ay_.tick;
        ay_.tick += step;

        const std::uint32_t tone_events = ((old_tick & 7U) + step) / 8U;
        if (tone_events != 0) {
            for (auto& channel : ay_.tone) {
                channel.bit ^= advance_counter(
                    channel.counter, channel.period, tone_events) & 1U;
            }
            std::uint32_t noise_toggles =
                advance_counter(ay_.noise.counter, ay_.noise.period, tone_events);
            while (noise_toggles-- != 0) {
                ay_.noise.bit ^= 1U;
                if (ay_.noise.bit != 0) {
                    ay_.noise.rng ^= (((ay_.noise.rng & 1U) ^
                                       ((ay_.noise.rng >> 3U) & 1U)) << 17U);
                    ay_.noise.rng >>= 1U;
                }
            }
        }

        const std::uint32_t envelope_events = ((old_tick & 15U) + step) / 16U;
        std::uint32_t changes =
            advance_counter(ay_.env.counter, ay_.env.period, envelope_events);
        while (changes-- != 0) {
            if (!ay_.env.shape_holding) {
                ay_.env.shape_counter = (ay_.env.shape_counter + 1U) & 0x1FU;
                if (ay_.env.shape_hold && ay_.env.shape_counter == 0x1FU)
                    ay_.env.shape_holding = true;
            }
            ay_.env.shape_state = _ay38910_shapes[
                ay_.reg[AY38910_REG_ENV_SHAPE_CYCLE]][ay_.env.shape_counter];
        }

        ay_.sample_counter -= static_cast<int>(step * AY38910_FIXEDPOINT_SCALE);
        if (ay_.sample_counter <= 0) {
            ay_.sample_counter += ay_.sample_period;
            float mixed = 0.0F;
            for (int index = 0; index < AY38910_NUM_CHANNELS; ++index) {
                const auto& channel = ay_.tone[index];
                const float volume =
                    (ay_.reg[AY38910_REG_AMP_A + index] & (1U << 4U))
                    ? _ay38910_volumes[ay_.env.shape_state]
                    : _ay38910_volumes[ay_.reg[AY38910_REG_AMP_A + index] & 0x0FU];
                const int enabled = (channel.bit | channel.tone_disable) &
                    ((ay_.noise.rng & 1U) | channel.noise_disable);
                if (enabled) mixed += volume;
            }
            ay_.sample = dc_adjust(mixed) * ay_.magnitude;
            output[produced++] = ay_.sample;
        }
    }
    return produced;
}

}  // namespace cpcexplorer::stage4h5
