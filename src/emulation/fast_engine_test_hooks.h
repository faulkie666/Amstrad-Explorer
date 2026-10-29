#pragma once

namespace cpcexplorer::testing {

// Host benchmark decomposition only. Production code must leave Full selected.
enum class FastBenchmarkMode {
    Full,
    EventsWithoutPresentation,
    CpuOnly,
};

void set_fast_benchmark_mode(FastBenchmarkMode mode);
void set_fast_audio_enabled(bool enabled);

}  // namespace cpcexplorer::testing
