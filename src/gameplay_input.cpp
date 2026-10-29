#include "gameplay_input.h"

#include "launcher.h"

namespace cpcexplorer::input {

Profile profile_from_metadata(std::uint8_t value) {
    if (value <= static_cast<std::uint8_t>(Profile::KeysZXSpace))
        return static_cast<Profile>(value);
    return Profile::None;
}

Profile legacy_profile_for_payload_crc(std::uint32_t payload_crc) {
    // The physically installed Manic Miner package predates descriptor
    // profiles, as does the already-approved first Dizzy package. Their
    // validated snapshot fingerprints are the compatibility bridge; no
    // catalog/game ID controls input selection.
    if (payload_crc == 0x9584cfc4U) return Profile::KeysQWShift;
    if (payload_crc == 0x1e76d62fU) return Profile::KeysZXSpace;
    return Profile::None;
}

KeyBindings bindings(Profile profile) {
    switch (profile) {
        case Profile::KeysQWShift: return {'q', 'w', 0x02};
        case Profile::KeysZXSpace: return {'z', 'x', ' '};
        default: return {};
    }
}

void Adapter::reset(Profile profile, std::uint8_t launch_buttons) {
    profile_ = profile;
    held_ = 0;
    // A complete all-buttons-up sample is required after launch. This keeps
    // the Y press that selected a game from becoming its first gameplay input.
    armed_ = profile_ != Profile::None && launch_buttons == 0;
}

Transition Adapter::update(std::uint8_t buttons) {
    Transition result{};
    if (profile_ == Profile::None) return result;
    if (!armed_) {
        if (buttons == 0) armed_ = true;
        return result;
    }

    std::uint8_t desired = 0;
    if ((buttons & launcher::ButtonX) != 0) desired |= KeyLeft;
    if ((buttons & launcher::ButtonZ) != 0) desired |= KeyRight;
    if ((buttons & launcher::ButtonY) != 0) desired |= KeyJump;
    result.pressed = static_cast<std::uint8_t>(desired & ~held_);
    result.released = static_cast<std::uint8_t>(held_ & ~desired);
    result.held = desired;
    held_ = desired;
    return result;
}

}  // namespace cpcexplorer::input
