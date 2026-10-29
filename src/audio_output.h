#pragma once
#include <cstddef>
#include <cstdint>
namespace cpcexplorer::audio {
struct Diagnostics { std::uint32_t submitted=0, played=0, underruns=0, overruns=0; };
bool init(unsigned sample_rate);
void submit(const float* samples, std::size_t count);
void shutdown();
Diagnostics diagnostics();
}
