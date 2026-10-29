#include "audio_buffer.h"

#include <array>
#include <cstdio>

int main() {
    cpcexplorer::audio::PcmBufferQueue queue;
    std::array<float, 128> positive{};
    positive.fill(1.0f);
    std::array<float, 128> negative{};
    negative.fill(-1.0f);

    if (!queue.submit(positive.data(), positive.size())) return 1;
    const auto* first = queue.acquire();
    if (first == nullptr || first[0] != 176 || queue.acquire() != nullptr) return 2;
    if (!queue.submit(negative.data(), negative.size())) return 3;
    if (queue.submit(positive.data(), positive.size())) return 4;

    queue.complete();
    const auto* second = queue.acquire();
    if (second == nullptr || second[0] != 80) return 5;
    queue.complete();
    if (queue.acquire() != nullptr) return 6;

    queue.reset();
    if (!queue.submit(positive.data(), 1)) return 7;
    const auto* padded = queue.acquire();
    if (padded == nullptr || padded[0] != 176 || padded[1] != 128) return 8;

    std::puts("PASS: bounded double buffer, overrun rejection, underrun silence, conservative conversion");
    return 0;
}
