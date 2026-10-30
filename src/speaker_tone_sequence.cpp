#include "speaker_tone_sequence.h"

#include <algorithm>
#include <cstdint>

namespace cpcexplorer::speaker_diag {
namespace {

float sample_at(std::size_t position) {
    unsigned frequency = 0;
    std::size_t local = 0;
    if (position < kToneSamples) {
        frequency = 440;
        local = position;
    } else if (position < kToneSamples + kSilenceSamples) {
        return 0.0f;
    } else if (position < 2U * kToneSamples + kSilenceSamples) {
        frequency = 660;
        local = position - kToneSamples - kSilenceSamples;
    } else if (position < 2U * (kToneSamples + kSilenceSamples)) {
        return 0.0f;
    } else {
        frequency = 880;
        local = position - 2U * (kToneSamples + kSilenceSamples);
    }
    const auto half_cycles = (static_cast<std::uint64_t>(local) * frequency * 2U) / kSampleRate;
    return (half_cycles & 1U) == 0U ? kAmplitude : -kAmplitude;
}

}  // namespace

std::size_t ToneSequence::render(float* output, std::size_t count) {
    if (output == nullptr) return 0;
    const auto produced = std::min(count, kTotalSamples - std::min(position_, kTotalSamples));
    for (std::size_t i = 0; i < produced; ++i) output[i] = sample_at(position_ + i);
    for (std::size_t i = produced; i < count; ++i) output[i] = 0.0f;
    position_ += produced;
    return produced;
}

}  // namespace cpcexplorer::speaker_diag
