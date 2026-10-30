#include "gameplay_input.h"

#include "launcher.h"

#include <iterator>

namespace cpcexplorer::input {
namespace {

constexpr KeyBindings make_bindings(Profile profile) {
    KeyBindings result{};
    switch (profile) {
        case Profile::KeysQWShift:
            result.keys[OnboardX] = 'q'; result.keys[OnboardY] = 0x02;
            result.keys[OnboardZ] = 'w'; result.keys[PadLeft] = 'q';
            result.keys[PadRight] = 'w'; result.keys[PadX] = 0x02;
            break;
        case Profile::KeysZXSpace:
            result.keys[OnboardX] = 'z'; result.keys[OnboardY] = ' ';
            result.keys[OnboardZ] = 'x'; result.keys[PadLeft] = 'z';
            result.keys[PadRight] = 'x'; result.keys[PadX] = ' ';
            break;
        case Profile::JoystickDirectionsFire:
            result.keys[OnboardX] = 0x08; result.keys[OnboardY] = 0x20;
            result.keys[OnboardZ] = 0x09; result.keys[OnboardA] = 0x0B;
            result.keys[OnboardB] = 0x0A; result.keys[PadUp] = 0x0B;
            result.keys[PadDown] = 0x0A; result.keys[PadLeft] = 0x08;
            result.keys[PadRight] = 0x09; result.keys[PadX] = 0x20;
            result.keys[PadA] = 0x20; result.digital_joystick = true;
            break;
        case Profile::FruityJoystickStart:
            // Preserve the existing six-button profile. No Qw/ST mapping is
            // invented for the hidden, unvalidated Fruity Frank record.
            result.keys[OnboardX] = 0x08; result.keys[OnboardY] = 0x20;
            result.keys[OnboardZ] = 0x09; result.keys[OnboardA] = 0x0B;
            result.keys[OnboardB] = 0x0A; result.keys[OnboardC] = 'f';
            result.digital_joystick = true;
            break;
        case Profile::ThrustFiveKey:
            result.keys[OnboardX] = 'z'; result.keys[OnboardY] = ' ';
            result.keys[OnboardZ] = 'x'; result.keys[OnboardA] = 0x0D;
            result.keys[OnboardB] = 0x02; result.keys[PadLeft] = 'z';
            result.keys[PadRight] = 'x'; result.keys[PadUp] = 0x02;
            result.keys[PadX] = ' '; result.keys[PadPlus] = 0x0D;
            break;
        case Profile::CursorQaop:
            result.keys[PadUp] = 0x0B; result.keys[PadDown] = 0x0A;
            result.keys[PadLeft] = 0x08; result.keys[PadRight] = 0x09;
            // Official board orientation: top X, bottom B, left Y, right A.
            result.keys[PadX] = 'q'; result.keys[PadB] = 'a';
            result.keys[PadY] = 'o'; result.keys[PadA] = 'p';
            result.keys[PadMinus] = ' '; result.keys[PadPlus] = 0x0D;
            break;
        case Profile::KnightTyme:
            result.keys[PadUp] = 0x0B; result.keys[PadDown] = 0x0A;
            result.keys[PadLeft] = 0x08; result.keys[PadRight] = 0x09;
#if defined(CPCEXPLORER_STAGE4J2C_KNIGHT)
            result.keys[PadX] = 'a'; result.keys[PadB] = 'z';
            result.keys[PadY] = 'n'; result.keys[PadA] = 'm';
#else
            result.keys[PadX] = 'n'; result.keys[PadB] = 'm';
            result.keys[PadY] = 'a'; result.keys[PadA] = 'z';
#endif
            result.keys[PadMinus] = ' '; result.keys[PadPlus] = 0x0D;
            result.keys[OnboardX] = 'n'; result.keys[OnboardZ] = 'm';
            result.keys[OnboardY] = ' '; result.keys[OnboardA] = 'a';
            result.keys[OnboardB] = 'z'; result.keys[OnboardC] = 0x0D;
            break;
        case Profile::KeysWasdSpace:
            result.keys[PadUp] = 'w'; result.keys[PadDown] = 's';
            result.keys[PadLeft] = 'a'; result.keys[PadRight] = 'd';
            result.keys[PadX] = ' '; result.keys[PadPlus] = 0x0D;
            break;
        default: break;
    }
    return result;
}

constexpr KeyBindings kBindings[] = {
    make_bindings(Profile::None),
    make_bindings(Profile::KeysQWShift),
    make_bindings(Profile::KeysZXSpace),
    make_bindings(Profile::JoystickDirectionsFire),
    make_bindings(Profile::FruityJoystickStart),
    make_bindings(Profile::ThrustFiveKey),
    make_bindings(Profile::CursorQaop),
    make_bindings(Profile::KnightTyme),
    make_bindings(Profile::KeysWasdSpace),
};

}  // namespace

Profile profile_from_metadata(std::uint8_t value) {
    if (value <= static_cast<std::uint8_t>(Profile::KeysWasdSpace))
        return static_cast<Profile>(value);
    return Profile::None;
}

Profile legacy_profile_for_payload_crc(std::uint32_t payload_crc) {
    // The physically installed Manic Miner package predates descriptor
    // profiles, as does the already-approved first Dizzy package. Their
    // validated snapshot fingerprints are the compatibility bridge; no
    // catalog/game ID controls input selection.
    if (payload_crc == 0x4ccc3730U) return Profile::KnightTyme;
    if (payload_crc == 0x9584cfc4U) return Profile::KeysQWShift;
    if (payload_crc == 0x1e76d62fU) return Profile::KeysZXSpace;
    return Profile::None;
}

const KeyBindings& bindings(Profile profile) {
    const auto index = static_cast<std::size_t>(profile);
    return index < std::size(kBindings) ? kBindings[index] : kBindings[0];
}

bool key_held(const KeyBindings& configured, ButtonMask buttons, int key) {
    for (std::size_t index = 0; index < configured.keys.size(); ++index)
        if (configured.keys[index] == key && (buttons & (1U << index)) != 0) return true;
    return false;
}

void Adapter::reset(Profile profile, ButtonMask launch_buttons) {
    profile_ = profile;
    held_ = 0;
    // A complete all-buttons-up sample is required after launch. This keeps
    // the Y press that selected a game from becoming its first gameplay input.
    armed_ = profile_ != Profile::None && launch_buttons == 0;
}

Transition Adapter::update(ButtonMask buttons) {
    Transition result{};
    if (profile_ == Profile::None) return result;
    if (!armed_) {
        if (buttons == 0) armed_ = true;
        return result;
    }

    const auto configured = bindings(profile_);
    ButtonMask supported = 0;
    for (std::size_t index = 0; index < configured.keys.size(); ++index) {
        if (configured.keys[index] != 0) supported |= static_cast<ButtonMask>(1U << index);
    }
    const ButtonMask desired = buttons & supported;
    result.previous = held_;
    result.pressed = static_cast<ButtonMask>(desired & ~held_);
    result.released = static_cast<ButtonMask>(held_ & ~desired);
    result.held = desired;
    held_ = desired;
    return result;
}

}  // namespace cpcexplorer::input
