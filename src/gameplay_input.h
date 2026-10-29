#pragma once

#include <cstdint>

namespace cpcexplorer::input {

enum class Profile : std::uint8_t {
    None,
    KeysQWShift,
    KeysZXSpace,
};

struct KeyBindings {
    int left = 0;
    int right = 0;
    int action = 0;
};

enum KeyMask : std::uint8_t {
    KeyLeft = 1U << 0,
    KeyRight = 1U << 1,
    KeyJump = 1U << 2,
};

struct Transition {
    std::uint8_t pressed = 0;
    std::uint8_t released = 0;
    std::uint8_t held = 0;
};

// New library entries carry this value in descriptor byte 59.  Only the
// pre-metadata Manic Miner entry needs the narrowly scoped legacy fallback.
Profile profile_from_metadata(std::uint8_t value);
Profile legacy_profile_for_payload_crc(std::uint32_t payload_crc);
KeyBindings bindings(Profile profile);

class Adapter {
public:
    void reset(Profile profile, std::uint8_t launch_buttons);
    Transition update(std::uint8_t buttons);
    bool armed() const { return armed_; }

private:
    Profile profile_ = Profile::None;
    bool armed_ = false;
    std::uint8_t held_ = 0;
};

}  // namespace cpcexplorer::input
