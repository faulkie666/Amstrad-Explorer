#include "gameplay_input.h"
#include "launcher.h"

#include <cstdio>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main() {
    using namespace cpcexplorer;
    using namespace cpcexplorer::input;

    if (!expect(profile_from_metadata(2) == Profile::KeysZXSpace,
                "descriptor metadata selects reusable Z/X/Space profile") ||
        !expect(profile_from_metadata(3) == Profile::JoystickDirectionsFire,
                "descriptor metadata selects five-action joystick profile") ||
        !expect(profile_from_metadata(5) == Profile::ThrustFiveKey,
                "descriptor metadata selects Thrust five-key profile") ||
        !expect(profile_from_metadata(6) == Profile::CursorQaop,
                "descriptor metadata selects cursor/QAOP pad profile") ||
        !expect(profile_from_metadata(8) == Profile::KeysWasdSpace,
                "descriptor metadata selects custom WASD profile") ||
        !expect(profile_from_metadata(1) == Profile::KeysQWShift,
                "descriptor metadata selects Q/W/Shift profile") ||
        !expect(profile_from_metadata(99) == Profile::None,
                "invalid metadata fails closed") ||
        !expect(legacy_profile_for_payload_crc(0x4ccc3730U) == Profile::KnightTyme,
                "pre-profile Knight snapshot receives verified controls") ||
        !expect(legacy_profile_for_payload_crc(0x9584cfc4U) == Profile::KeysQWShift,
                "pre-metadata Manic snapshot retains its legacy profile") ||
        !expect(legacy_profile_for_payload_crc(0x1e76d62fU) == Profile::KeysZXSpace,
                "approved pre-metadata Dizzy snapshot selects Z/X/Space") ||
        !expect(legacy_profile_for_payload_crc(0) == Profile::None,
                "unknown payloads remain unprofiled")) return 1;

    Adapter input;
    const auto manic = bindings(Profile::KeysQWShift);
    const auto dizzy = bindings(Profile::KeysZXSpace);
    const auto joystick = bindings(Profile::JoystickDirectionsFire);
    const auto thrust = bindings(Profile::ThrustFiveKey);
    const auto qaop = bindings(Profile::CursorQaop);
    const auto knight = bindings(Profile::KnightTyme);
    if (!expect(manic.keys[0] == 'q' && manic.keys[2] == 'w' && manic.keys[1] == 0x02,
                "Manic Miner bindings remain Q/W/Shift") ||
        !expect(dizzy.keys[0] == 'z' && dizzy.keys[2] == 'x' && dizzy.keys[1] == ' ',
                "Dizzy bindings are Z/X/Space") ||
        !expect(joystick.digital_joystick && joystick.keys[0] == 0x08 &&
                joystick.keys[2] == 0x09 && joystick.keys[3] == 0x0B &&
                joystick.keys[4] == 0x0A && joystick.keys[1] == 0x20,
                "six-button surface maps direct joystick directions and fire") ||
        !expect(!thrust.digital_joystick && thrust.keys[OnboardX] == 'z' &&
                thrust.keys[2] == 'x' && thrust.keys[3] == 0x0D &&
                thrust.keys[4] == 0x02 && thrust.keys[1] == ' ',
                "Thrust exposes all five essential keys directly") ||
        !expect(qaop.keys[PadUp] == 0x0B && qaop.keys[PadDown] == 0x0A &&
                qaop.keys[PadLeft] == 0x08 && qaop.keys[PadRight] == 0x09 &&
                qaop.keys[PadX] == 'q' && qaop.keys[PadB] == 'a' &&
                qaop.keys[PadY] == 'o' && qaop.keys[PadA] == 'p' &&
                qaop.keys[PadMinus] == ' ' && qaop.keys[PadPlus] == 0x0D,
                "official ABXY orientation maps to cursor/QAOP/Space/Enter") ||
        !expect(knight.keys[PadX] == 'n' && knight.keys[PadB] == 'm' &&
                knight.keys[PadY] == 'a' && knight.keys[PadA] == 'z' &&
                knight.keys[PadMinus] == ' ',
                "default Knight bindings remain the installed Stage 4J.1 mapping")) return 2;

    input.reset(Profile::KeysQWShift, launcher::ButtonY);
    if (!expect(!input.armed(), "launch-held Y starts suppressed") ||
        !expect(input.update(launcher::ButtonY).held == 0, "held Y cannot jump") ||
        !expect(input.update(launcher::ButtonX | launcher::ButtonY).held == 0,
                "other buttons cannot bypass launch suppression") ||
        !expect(input.update(0).held == 0 && input.armed(),
                "all-buttons-up arms gameplay")) return 3;

    auto event = input.update(launcher::ButtonX);
    if (!expect(event.pressed == launcher::ButtonX && event.held == launcher::ButtonX,
                "X presses and holds Q profile key")) return 3;
    event = input.update(launcher::ButtonX);
    if (!expect(event.pressed == 0 && event.released == 0 && event.held == launcher::ButtonX,
                "held X is level-based without repeat events")) return 4;
    event = input.update(0);
    if (!expect(event.released == launcher::ButtonX && event.held == 0, "X release releases Q")) return 5;

    event = input.update(launcher::ButtonZ);
    if (!expect(event.pressed == launcher::ButtonZ && event.held == launcher::ButtonZ, "Z holds W")) return 6;
    event = input.update(launcher::ButtonY | launcher::ButtonZ);
    if (!expect(event.pressed == launcher::ButtonY && event.released == 0 &&
                event.held == (launcher::ButtonZ | launcher::ButtonY), "simultaneous Z/Y is preserved")) return 7;
    event = input.update(launcher::ButtonX);
    if (!expect(event.pressed == launcher::ButtonX &&
                event.released == (launcher::ButtonZ | launcher::ButtonY) &&
                event.held == launcher::ButtonX,
                "rapid transition releases old keys and presses new key")) return 8;
    event = input.update(0);
    if (!expect(event.released == launcher::ButtonX && event.held == 0,
                "final release leaves no stuck keys")) return 9;

    input.reset(Profile::JoystickDirectionsFire, 0);
    event = input.update(launcher::ButtonA | launcher::ButtonB | launcher::ButtonY);
    if (!expect(event.held == (launcher::ButtonA | launcher::ButtonB | launcher::ButtonY),
                "A/B/Y remain independent level-based controls")) return 10;

    // Both the old onboard Y button and Qw/ST X can fire. Releasing one while
    // the other is held must not release the shared CPC fire key.
    input.reset(Profile::JoystickDirectionsFire, 0);
    event = input.update((1U << OnboardY) | (1U << PadX));
    if (!expect(key_held(joystick, event.held, 0x20), "duplicate fire sources are held") ||
        !expect(key_held(joystick, (1U << PadX), 0x20),
                "one remaining source keeps the shared key held")) return 11;

    input.reset(Profile::CursorQaop, 0);
    event = input.update(static_cast<ButtonMask>(1U << PadX));
    if (!expect(event.pressed == static_cast<ButtonMask>(1U << PadX) &&
                event.held == static_cast<ButtonMask>(1U << PadX),
                "Qw/ST upper-bit press and hold are retained")) return 12;
    event = input.update(0);
    if (!expect(event.released == static_cast<ButtonMask>(1U << PadX),
                "Qw/ST upper-bit release is retained")) return 13;

    input.reset(Profile::None, 0);
    event = input.update(launcher::ButtonX | launcher::ButtonY | launcher::ButtonZ);
    if (!expect(event.pressed == 0 && event.released == 0 && event.held == 0,
                "unprofiled games receive no gameplay keys")) return 14;

    std::puts("PASS: metadata profiles, legacy Manic fallback, holds/releases and transitions");
    return 0;
}
