#include "audio_output.h"

#include <cstdio>

int main() {
    using cpcexplorer::audio::AmplifierPolicy;
    using cpcexplorer::audio::gate_amplifier_on_underrun;
    if (!gate_amplifier_on_underrun(AmplifierPolicy::GateOnPlayback)) return 1;
    if (gate_amplifier_on_underrun(AmplifierPolicy::HoldEnabled)) return 2;
    std::puts("PASS: gameplay hold policy cannot deassert AMP_EN on DMA underrun");
    return 0;
}
