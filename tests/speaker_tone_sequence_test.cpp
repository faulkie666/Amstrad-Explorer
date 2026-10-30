#include "speaker_tone_sequence.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
double frequency(const std::vector<float>& samples, std::size_t begin, std::size_t end) {
    unsigned upward_crossings = 0;
    for (std::size_t i = begin + 1; i < end; ++i)
        if (samples[i - 1] < 0.0f && samples[i] > 0.0f) ++upward_crossings;
    return upward_crossings * static_cast<double>(cpcexplorer::speaker_diag::kSampleRate) /
           static_cast<double>(end - begin);
}
}

int main() {
    using namespace cpcexplorer::speaker_diag;
    ToneSequence sequence;
    std::vector<float> samples;
    std::array<float, 128> block{};
    while (!sequence.finished()) {
        const auto produced = sequence.render(block.data(), block.size());
        samples.insert(samples.end(), block.begin(), block.begin() + produced);
    }
    if (samples.size() != kTotalSamples || sequence.position() != kTotalSamples) return 1;

    const std::size_t silence1 = kToneSamples;
    const std::size_t tone2 = silence1 + kSilenceSamples;
    const std::size_t silence2 = tone2 + kToneSamples;
    const std::size_t tone3 = silence2 + kSilenceSamples;
    if (!std::all_of(samples.begin() + silence1, samples.begin() + tone2,
                     [](float value) { return value == 0.0f; }) ||
        !std::all_of(samples.begin() + silence2, samples.begin() + tone3,
                     [](float value) { return value == 0.0f; })) return 2;
    if (std::any_of(samples.begin(), samples.end(), [](float value) {
            return value < -kAmplitude || value > kAmplitude;
        })) return 3;

    const std::array<double, 3> measured{
        frequency(samples, 0, kToneSamples),
        frequency(samples, tone2, tone2 + kToneSamples),
        frequency(samples, tone3, tone3 + kToneSamples),
    };
    const std::array<double, 3> expected{440.0, 660.0, 880.0};
    for (std::size_t i = 0; i < measured.size(); ++i)
        if (std::abs(measured[i] - expected[i]) > 2.0) return 4;

    std::printf("PASS: %.2f Hz, %.2f Hz, %.2f Hz; two %zu-sample silences; %zu samples total\n",
                measured[0], measured[1], measured[2], kSilenceSamples, samples.size());
    return 0;
}
