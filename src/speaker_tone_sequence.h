#pragma once

#include <cstddef>

namespace cpcexplorer::speaker_diag {

inline constexpr unsigned kSampleRate = 22050;
inline constexpr std::size_t kToneSamples = kSampleRate;
inline constexpr std::size_t kSilenceSamples = (kSampleRate + 2U) / 4U;
inline constexpr std::size_t kTotalSamples = 3U * kToneSamples + 2U * kSilenceSamples;
inline constexpr float kAmplitude = 0.4f;

class ToneSequence {
public:
    std::size_t render(float* output, std::size_t count);
    [[nodiscard]] bool finished() const { return position_ >= kTotalSamples; }
    [[nodiscard]] std::size_t position() const { return position_; }

private:
    std::size_t position_ = 0;
};

}  // namespace cpcexplorer::speaker_diag
