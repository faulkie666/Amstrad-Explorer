#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace cpcexplorer::audio {

class PcmBufferQueue {
public:
    static constexpr std::size_t BufferSamples = 128;

    bool submit(const float* samples, std::size_t count) {
        if (samples == nullptr) return false;
        int slot = -1;
        for (int i = 0; i < 2; ++i) {
            if (state_[i] == State::Free) { slot = i; break; }
        }
        if (slot < 0) return false;
        const auto copied = std::min(count, BufferSamples);
        for (std::size_t i = 0; i < BufferSamples; ++i) {
            const float sample = std::clamp(i < copied ? samples[i] : 0.0f, -1.0f, 1.0f);
            buffers_[slot][i] = static_cast<std::uint16_t>(128.0f + sample * 48.0f);
        }
        state_[slot] = State::Ready;
        return true;
    }

    const std::uint16_t* acquire() {
        if (playing_ >= 0) return nullptr;
        for (int i = 0; i < 2; ++i) {
            if (state_[i] == State::Ready) {
                state_[i] = State::Playing;
                playing_ = i;
                return buffers_[i].data();
            }
        }
        return nullptr;
    }

    void complete() {
        if (playing_ >= 0) {
            state_[playing_] = State::Free;
            playing_ = -1;
        }
    }

    void reset() {
        state_.fill(State::Free);
        playing_ = -1;
        for (auto& buffer : buffers_) buffer.fill(128);
    }

private:
    enum class State : std::uint8_t { Free, Ready, Playing };
    std::array<std::array<std::uint16_t, BufferSamples>, 2> buffers_{};
    std::array<State, 2> state_{State::Free, State::Free};
    int playing_ = -1;
};

}  // namespace cpcexplorer::audio
