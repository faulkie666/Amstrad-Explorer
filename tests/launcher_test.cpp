#include "launcher.h"
#include "menu_presentation.h"

#include <cstdio>
#include <cstring>

using cpcexplorer::launcher::ButtonX;
using cpcexplorer::launcher::ButtonY;
using cpcexplorer::launcher::ButtonZ;
using cpcexplorer::launcher::EntryType;
using cpcexplorer::launcher::MenuController;

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main() {
    if (!expect(std::strcmp(cpcexplorer::menu_presentation::Instruction, "Pick a game") == 0,
                "menu instruction text") ||
        !expect(std::strcmp(cpcexplorer::menu_presentation::Signature, "Faulkie") == 0,
                "menu signature text") ||
        !expect(cpcexplorer::menu_presentation::SignatureScale == 1,
                "menu signature uses smallest font scale") ||
        !expect(cpcexplorer::menu_presentation::SignatureX >= 0 &&
                    cpcexplorer::menu_presentation::SignatureX +
                        cpcexplorer::menu_presentation::SignatureWidth <= 316 &&
                    cpcexplorer::menu_presentation::SignatureY > 231 &&
                    cpcexplorer::menu_presentation::SignatureY +
                        cpcexplorer::menu_presentation::SignatureHeight <= 239,
                "menu signature is padded, on-screen, and below controls")) return 9;

    if (!expect(cpcexplorer::launcher::has_builtin_snapshot_id("knight_tyme"),
                "canonical Knight Tyme snapshot ID is built in") ||
        !expect(!cpcexplorer::launcher::has_builtin_snapshot_id("Knight Tyme"),
                "display name is not accepted as a built-in identity") ||
        !expect(!cpcexplorer::launcher::has_builtin_snapshot_id("manic_miner"),
                "legacy flash games are not mistaken for built-ins")) return 1;
    if (!expect(cpcexplorer::launcher::entry_count() == 2, "registry count") ||
        !expect(std::strcmp(cpcexplorer::launcher::entry(0).display_name, "Locomotive BASIC") == 0,
                "BASIC must be first") ||
        !expect(cpcexplorer::launcher::entry(0).type == EntryType::Basic, "first handler is BASIC") ||
        !expect(std::strcmp(cpcexplorer::launcher::entry(1).display_name, "Knight Tyme") == 0,
                "Knight Tyme must be second") ||
        !expect(cpcexplorer::launcher::entry(1).type == EntryType::Snapshot, "second handler is snapshot")) return 1;

    MenuController menu;
    menu.reset(0, 0);
    if (!expect(menu.selected() == 0, "menu defaults to BASIC")) return 2;

    // Z bounce and a held button produce exactly one movement.
    if (!expect(!menu.update(ButtonZ, 1).selection_changed, "press waits for debounce") ||
        !expect(!menu.update(0, 8).selection_changed, "bounce ignored") ||
        !expect(!menu.update(ButtonZ, 12).selection_changed, "second edge waits") ||
        !expect(menu.update(ButtonZ, 42).selection_changed, "stable Z moves down") ||
        !expect(menu.selected() == 1, "Knight Tyme selected") ||
        !expect(!menu.update(ButtonZ, 500).selection_changed, "held Z does not repeat")) return 3;

    // Release, press Z again and verify wrapping.
    menu.update(0, 510);
    menu.update(0, 540);
    menu.update(ButtonZ, 550);
    if (!expect(menu.update(ButtonZ, 580).selection_changed, "second Z press accepted") ||
        !expect(menu.selected() == 0, "down wraps to BASIC")) return 4;

    // X wraps in the opposite direction.
    menu.update(0, 590);
    menu.update(0, 620);
    menu.update(ButtonX, 630);
    if (!expect(menu.update(ButtonX, 660).selection_changed, "X press accepted") ||
        !expect(menu.selected() == 1, "up wraps to Knight Tyme")) return 5;

    // Y launches once, only after debounce; holding cannot launch again.
    menu.update(0, 670);
    menu.update(0, 700);
    if (!expect(!menu.update(ButtonY, 710).launch, "Y waits for debounce") ||
        !expect(menu.update(ButtonY, 740).launch, "Y launches selection") ||
        !expect(!menu.update(ButtonY, 900).launch, "held Y does not relaunch")) return 6;

    // A button held during reset must be released and pressed again.
    menu.reset(ButtonY, 1000);
    if (!expect(!menu.update(ButtonY, 2000).launch, "held-at-boot Y suppressed")) return 7;
    menu.update(0, 2010);
    menu.update(0, 2040);
    menu.update(ButtonY, 2050);
    if (!expect(menu.update(ButtonY, 2080).launch, "Y works after release")) return 8;

    std::puts("PASS: registry order, wrapping, debounce, one-shot movement and launch");
    return 0;
}
