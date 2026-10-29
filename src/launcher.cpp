#include "launcher.h"

#include "game_library.h"

#include <algorithm>
#include <iterator>

namespace cpcexplorer::launcher {
namespace {

constexpr Entry kEntries[] = {
    {"Locomotive BASIC", EntryType::Basic, nullptr, nullptr, 0},
    {"Knight Tyme", EntryType::Snapshot, "knight_tyme", nullptr, 0},
};

library::View library_view;

}  // namespace

void configure_library(const std::uint8_t* bytes, std::size_t size) {
    library_view.mount(bytes, size);
}

Entry entry(std::size_t index) {
    if (index < std::size(kEntries)) return kEntries[index];
    library::GameInfo game{};
    if (!library_view.game(index - std::size(kEntries), game)) return kEntries[0];
    return {game.title, EntryType::Snapshot, game.game_id, game.data, game.data_length};
}

std::size_t entry_count() { return std::size(kEntries) + library_view.valid_count(); }

void MenuController::reset(std::uint8_t pressed_mask, std::uint32_t now_ms) {
    selected_ = 0;
    const auto initialize = [now_ms](ButtonState& state, bool pressed) {
        state.raw = pressed;
        state.stable = pressed;
        state.changed_at = now_ms;
    };
    initialize(x_, (pressed_mask & ButtonX) != 0);
    initialize(y_, (pressed_mask & ButtonY) != 0);
    initialize(z_, (pressed_mask & ButtonZ) != 0);
}

bool MenuController::pressed_event(ButtonState& state, bool raw_pressed, std::uint32_t now_ms) {
    if (raw_pressed != state.raw) {
        state.raw = raw_pressed;
        state.changed_at = now_ms;
    }
    if ((state.stable != state.raw) &&
        (static_cast<std::uint32_t>(now_ms - state.changed_at) >= debounce_ms)) {
        state.stable = state.raw;
        return state.stable;
    }
    return false;
}

Update MenuController::update(std::uint8_t pressed_mask, std::uint32_t now_ms) {
    const bool x_pressed = pressed_event(x_, (pressed_mask & ButtonX) != 0, now_ms);
    const bool y_pressed = pressed_event(y_, (pressed_mask & ButtonY) != 0, now_ms);
    const bool z_pressed = pressed_event(z_, (pressed_mask & ButtonZ) != 0, now_ms);
    Update result{};
    if (y_pressed) {
        result.launch = true;
        return result;
    }
    if (x_pressed) {
        selected_ = (selected_ == 0) ? entry_count() - 1 : selected_ - 1;
        result.selection_changed = true;
    } else if (z_pressed) {
        selected_ = (selected_ + 1) % entry_count();
        result.selection_changed = true;
    }
    return result;
}

std::size_t MenuController::first_visible(std::size_t visible_slots) const {
    if ((visible_slots == 0) || (entry_count() <= visible_slots)) return 0;
    const std::size_t max_start = entry_count() - visible_slots;
    return std::min(selected_, max_start);
}

}  // namespace cpcexplorer::launcher
