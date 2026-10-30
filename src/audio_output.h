#pragma once
#include <cstddef>
#include <cstdint>
namespace cpcexplorer::audio {
enum class AmplifierPolicy : std::uint8_t {
    GateOnPlayback,
    HoldEnabled,
};
constexpr bool gate_amplifier_on_underrun(AmplifierPolicy policy) {
    return policy == AmplifierPolicy::GateOnPlayback;
}
struct Diagnostics { std::uint32_t submitted=0, played=0, underruns=0, overruns=0; };
bool init(unsigned sample_rate, AmplifierPolicy policy = AmplifierPolicy::GateOnPlayback);
bool submit(const float* samples, std::size_t count);
bool can_submit();
void shutdown();
Diagnostics diagnostics();
}
