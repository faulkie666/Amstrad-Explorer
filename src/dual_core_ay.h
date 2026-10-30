#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::dual_core_ay {

enum class State : std::uint32_t { Boot, Idle, Starting, Running, Stopping, Stopped, Failed };

struct Telemetry {
    State state = State::Boot;
    std::uint32_t producer_events = 0;
    std::uint32_t consumed_events = 0;
    std::uint32_t queue_full = 0;
    std::uint32_t sequence_errors = 0;
    std::uint32_t late_events = 0;
    std::uint32_t queue_peak = 0;
    std::uint32_t pcm_samples = 0;
    std::uint32_t render_max_us = 0;
    std::uint32_t render_p95_upper_us = 0;
    std::uint32_t deadline_misses = 0;
    std::uint32_t stack_high_water_bytes = 0;
};

bool launch_and_start(const std::uint8_t* registers, std::size_t count,
                      std::uint32_t epoch, std::uint32_t timeout_ms = 500);
bool launch_idle(std::uint32_t timeout_ms = 500);
bool stop(std::uint32_t timeout_ms = 500);
Telemetry telemetry();
bool active();

}  // namespace cpcexplorer::dual_core_ay
