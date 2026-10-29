#include "gameplay_input.h"

#include "launcher.h"

namespace cpcexplorer::input {

Profile profile_for_game(std::string_view game_id) {
    if (game_id == "manic_miner") return Profile::KeysQWShift;
    if (game_id == "dizzy") return Profile::KeysZXSpace;
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
