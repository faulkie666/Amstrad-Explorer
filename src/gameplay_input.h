#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cpcexplorer::input {

enum class Profile : std::uint8_t {
    None,
    KeysQWShift,
    KeysZXSpace,
    JoystickDirectionsFire,
    FruityJoystickStart,
    ThrustFiveKey,
    CursorQaop,
    KnightTyme,
    KeysWasdSpace,
};

using ButtonMask = std::uint16_t;

// The first six positions retain the established Explorer X/Y/Z/A/B/C bit
// order. Qw/ST Pad inputs follow without changing existing descriptor values.
enum ButtonIndex : std::size_t {
    OnboardX, OnboardY, OnboardZ, OnboardA, OnboardB, OnboardC,
    PadA, PadB, PadX, PadY, PadUp, PadDown, PadLeft, PadRight, PadPlus, PadMinus,
    ButtonCount,
};

struct KeyBindings {
    std::array<int, ButtonCount> keys{};
    bool digital_joystick = false;
};

struct Transition {
    ButtonMask previous = 0;
    ButtonMask pressed = 0;
    ButtonMask released = 0;
    ButtonMask held = 0;
};

// New library entries carry this value in descriptor byte 59.  Only the
// pre-metadata Manic Miner entry needs the narrowly scoped legacy fallback.
Profile profile_from_metadata(std::uint8_t value);
Profile legacy_profile_for_payload_crc(std::uint32_t payload_crc);
const KeyBindings& bindings(Profile profile);
bool key_held(const KeyBindings& bindings, ButtonMask buttons, int key);

class Adapter {
public:
    void reset(Profile profile, ButtonMask launch_buttons);
    Transition update(ButtonMask buttons);
    bool armed() const { return armed_; }

private:
    Profile profile_ = Profile::None;
    bool armed_ = false;
    ButtonMask held_ = 0;
};

}  // namespace cpcexplorer::input
