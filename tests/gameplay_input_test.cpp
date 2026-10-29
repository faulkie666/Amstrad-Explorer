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
        !expect(profile_from_metadata(1) == Profile::KeysQWShift,
                "descriptor metadata selects Q/W/Shift profile") ||
        !expect(profile_from_metadata(99) == Profile::None,
                "invalid metadata fails closed") ||
        !expect(legacy_profile_for_payload_crc(0x9584cfc4U) == Profile::KeysQWShift,
                "pre-metadata Manic snapshot retains its legacy profile") ||
        !expect(legacy_profile_for_payload_crc(0x1e76d62fU) == Profile::KeysZXSpace,
                "approved pre-metadata Dizzy snapshot selects Z/X/Space") ||
        !expect(legacy_profile_for_payload_crc(0) == Profile::None,
                "unknown payloads remain unprofiled")) return 1;

    Adapter input;
    const auto manic = bindings(Profile::KeysQWShift);
    const auto dizzy = bindings(Profile::KeysZXSpace);
    if (!expect(manic.left == 'q' && manic.right == 'w' && manic.action == 0x02,
                "Manic Miner bindings remain Q/W/Shift") ||
        !expect(dizzy.left == 'z' && dizzy.right == 'x' && dizzy.action == ' ',
                "Dizzy bindings are Z/X/Space")) return 2;

    input.reset(Profile::KeysQWShift, launcher::ButtonY);
    if (!expect(!input.armed(), "launch-held Y starts suppressed") ||
        !expect(input.update(launcher::ButtonY).held == 0, "held Y cannot jump") ||
        !expect(input.update(launcher::ButtonX | launcher::ButtonY).held == 0,
                "other buttons cannot bypass launch suppression") ||
        !expect(input.update(0).held == 0 && input.armed(),
                "all-buttons-up arms gameplay")) return 3;

    auto event = input.update(launcher::ButtonX);
    if (!expect(event.pressed == KeyLeft && event.held == KeyLeft,
                "X presses and holds Q profile key")) return 3;
    event = input.update(launcher::ButtonX);
    if (!expect(event.pressed == 0 && event.released == 0 && event.held == KeyLeft,
                "held X is level-based without repeat events")) return 4;
    event = input.update(0);
    if (!expect(event.released == KeyLeft && event.held == 0, "X release releases Q")) return 5;

    event = input.update(launcher::ButtonZ);
    if (!expect(event.pressed == KeyRight && event.held == KeyRight, "Z holds W")) return 6;
    event = input.update(launcher::ButtonY | launcher::ButtonZ);
    if (!expect(event.pressed == KeyJump && event.released == 0 &&
                event.held == (KeyRight | KeyJump), "simultaneous Z/Y is preserved")) return 7;
    event = input.update(launcher::ButtonX);
    if (!expect(event.pressed == KeyLeft &&
                event.released == (KeyRight | KeyJump) && event.held == KeyLeft,
                "rapid transition releases old keys and presses new key")) return 8;
    event = input.update(0);
    if (!expect(event.released == KeyLeft && event.held == 0,
                "final release leaves no stuck keys")) return 9;

    input.reset(Profile::None, 0);
    event = input.update(launcher::ButtonX | launcher::ButtonY | launcher::ButtonZ);
    if (!expect(event.pressed == 0 && event.released == 0 && event.held == 0,
                "unprofiled games receive no gameplay keys")) return 10;

    std::puts("PASS: metadata profiles, legacy Manic fallback, holds/releases and transitions");
    return 0;
}
