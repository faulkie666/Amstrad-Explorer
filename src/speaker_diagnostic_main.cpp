#include "audio_buffer.h"
#include "audio_output.h"
#include "speaker_tone_sequence.h"

#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#include <array>
#include <cstdio>

namespace {
constexpr char kFirmwareIdentity[] = "0.4.14-stage4h3speakerdiag";
}

int main() {
    stdio_init_all();

    if (!cpcexplorer::audio::init(
            cpcexplorer::speaker_diag::kSampleRate,
            cpcexplorer::audio::AmplifierPolicy::HoldEnabled)) {
        while (true) {
            if (stdio_usb_connected())
                std::printf("SPKDIAG ERROR fw=%s reason=no_dma_channel amp=off\n", kFirmwareIdentity);
            sleep_ms(1000);
        }
    }

    // audio::init establishes PWM midpoint before asserting AMP_EN. Give the
    // amplifier time to settle, then leave it enabled until the test finishes.
    sleep_ms(10);

    cpcexplorer::speaker_diag::ToneSequence sequence;
    std::array<float, cpcexplorer::audio::PcmBufferQueue::BufferSamples> block{};
    constexpr std::uint32_t total_blocks = static_cast<std::uint32_t>(
        (cpcexplorer::speaker_diag::kTotalSamples + block.size() - 1U) / block.size());
    std::uint32_t queued = 0;

    while (cpcexplorer::audio::diagnostics().played < total_blocks) {
        const auto current = cpcexplorer::audio::diagnostics();
        while (queued < total_blocks && queued - current.played < 2U) {
            sequence.render(block.data(), block.size());
            cpcexplorer::audio::submit(block.data(), block.size());
            ++queued;
        }
        tight_loop_contents();
    }

    const auto result = cpcexplorer::audio::diagnostics();
    cpcexplorer::audio::shutdown();

    while (true) {
        if (stdio_usb_connected()) {
            std::printf(
                "SPKDIAG RESULT fw=%s rate=%u samples=%u submitted=%u played=%u "
                "underruns=%u overruns=%u amp=off\n",
                kFirmwareIdentity,
                cpcexplorer::speaker_diag::kSampleRate,
                static_cast<unsigned>(cpcexplorer::speaker_diag::kTotalSamples),
                static_cast<unsigned>(result.submitted),
                static_cast<unsigned>(result.played),
                static_cast<unsigned>(result.underruns),
                static_cast<unsigned>(result.overruns));
        }
        sleep_ms(1000);
    }
}
