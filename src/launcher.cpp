#include "launcher.h"

#include "game_library.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace cpcexplorer::launcher {
namespace {

constexpr Entry kEntries[] = {
    {"Locomotive BASIC", EntryType::Basic, nullptr, nullptr, 0, input::Profile::None},
    {"Knight Tyme", EntryType::Snapshot, "knight_tyme", nullptr, 0, input::Profile::KnightTyme},
};

library::View library_view;
packed::View packed_view;

char legacy_letter(const library::GameInfo& game) {
    if (game.title == nullptr) return 0;
    char value = game.title[0];
    if (value >= 'a' && value <= 'z') value = static_cast<char>(value - 'a' + 'A');
    return value >= 'A' && value <= 'Z' ? value : 0;
}

bool packed_has_id(const char* snapshot_id) {
    if (snapshot_id == nullptr || !packed_view.mounted()) return false;
    for (std::size_t index = 0; index < packed_view.count(); ++index) {
        packed::Record game{};
        if (packed_view.record(index, game) && std::strcmp(game.id, snapshot_id) == 0)
            return true;
    }
    return false;
}

bool legacy_overlay_game(std::size_t valid_index, library::GameInfo& game) {
    if (!library_view.game(valid_index, game) || game.game_id == nullptr ||
        legacy_letter(game) == 0 || has_builtin_snapshot_id(game.game_id) ||
        packed_has_id(game.game_id)) return false;
    return true;
}

std::size_t legacy_overlay_count(packed::Category category, char letter = 0) {
    if (!packed_view.mounted() || category != packed::Category::Arcade) return 0;
    std::size_t count = 0;
    for (std::size_t index = 0; index < library_view.valid_count(); ++index) {
        library::GameInfo game{};
        if (legacy_overlay_game(index, game) && (!letter || legacy_letter(game) == letter))
            ++count;
    }
    return count;
}

bool legacy_overlay_at(std::size_t wanted, library::GameInfo& game,
                       packed::Category category = packed::Category::Arcade,
                       char letter = 0, bool verify_payload = false) {
    if (!packed_view.mounted() || category != packed::Category::Arcade) return false;
    for (std::size_t index = 0, found = 0; index < library_view.valid_count(); ++index) {
        library::GameInfo candidate{};
        if (!legacy_overlay_game(index, candidate) ||
            (letter && legacy_letter(candidate) != letter)) continue;
        if (found++ == wanted) {
            if (verify_payload && !library_view.verified_game(index, candidate)) return false;
            game = candidate; return true;
        }
    }
    return false;
}

Entry library_entry(const library::GameInfo& game) {
    auto profile = input::profile_from_metadata(game.control_profile);
    if (profile == input::Profile::None)
        profile = input::legacy_profile_for_payload_crc(game.data_crc32);
    return {game.title, EntryType::Snapshot, game.game_id, game.data, game.data_length,
            profile, false, {}};
}

Entry packed_entry(const packed::Record& game) {
    auto profile = input::profile_from_metadata(game.control_profile);
    if (profile == input::Profile::None)
        profile = input::legacy_profile_for_payload_crc(game.raw_crc32);
    return {game.title, EntryType::Snapshot, game.id, nullptr, 0, profile, true, game};
}

bool category_record(packed::Category category, char letter, std::size_t wanted,
                     Entry& result) {
    const std::size_t packed_count = packed_view.count(category, letter);
    if (wanted < packed_count) {
        packed::Record game{};
        if (!packed_view.record(category, letter, wanted, game)) return false;
        result = packed_entry(game);
        return true;
    }
    library::GameInfo game{};
    if (!legacy_overlay_at(wanted - packed_count, game, category, letter)) return false;
    result = library_entry(game);
    return true;
}

}  // namespace

void configure_library(const std::uint8_t* bytes, std::size_t size) {
    library_view.mount(bytes, size);
}

bool configure_packed_library(const std::uint8_t* bytes, std::size_t size) {
    return packed_view.mount(bytes, size);
}
bool packed_library_mounted() { return packed_view.mounted(); }

bool has_builtin_snapshot_id(const char* snapshot_id) {
    if (snapshot_id == nullptr) return false;
    for (const auto& candidate : kEntries) {
        if (candidate.type == EntryType::Snapshot && candidate.snapshot_id != nullptr &&
            std::strcmp(candidate.snapshot_id, snapshot_id) == 0) return true;
    }
    return false;
}

bool has_protected_snapshot_id(const char* snapshot_id) {
    return has_builtin_snapshot_id(snapshot_id) || packed_has_id(snapshot_id);
}

Entry entry(std::size_t index) {
    if (packed_view.mounted()) {
        if (index == 0) return kEntries[0];
        const std::size_t packed_count = packed_view.launchable_count();
        packed::Record packed_game{};
        if (index <= packed_count && packed_view.launchable(index - 1, packed_game))
            return packed_entry(packed_game);
        library::GameInfo game{};
        if (index > packed_count && legacy_overlay_at(index - packed_count - 1, game))
            return library_entry(game);
        return kEntries[0];
    }
    if (index < std::size(kEntries)) return kEntries[index];
    library::GameInfo game{};
    if (!library_view.game(index - std::size(kEntries), game)) return kEntries[0];
    return library_entry(game);
}

std::size_t entry_count() {
    return packed_view.mounted() ? 1 + packed_view.launchable_count() +
                                       legacy_overlay_count(packed::Category::Arcade) :
        std::size(kEntries) + library_view.valid_count();
}

void MenuController::reset(std::uint8_t pressed_mask, std::uint32_t now_ms) {
    selected_ = 0;
    level_ = 0;
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
        if (!packed_view.mounted()) {
            result.launch = true;
        } else if (level_ == 0) {
            if (selected_ == 0) result.launch = true;
            else {
                category_ = selected_ == 1 ? packed::Category::Arcade :
                                              packed::Category::Simulation;
                level_ = 1; selected_ = 0; result.selection_changed = true;
            }
        } else if (selected_ == 0) {
            --level_; selected_ = 0; result.selection_changed = true;
        } else if (level_ == 1) {
            std::size_t wanted = selected_ - 1;
            for (char letter = 'A'; letter <= 'Z'; ++letter) {
                if ((packed_view.has_letter(category_, letter) ||
                     legacy_overlay_count(category_, letter) != 0) && wanted-- == 0) {
                    letter_ = letter; level_ = 2; selected_ = 0;
                    result.selection_changed = true; break;
                }
            }
        } else {
            result.launch = true;
        }
        return result;
    }
    if (x_pressed) {
        selected_ = (selected_ == 0) ? visible_count() - 1 : selected_ - 1;
        result.selection_changed = true;
    } else if (z_pressed) {
        selected_ = (selected_ + 1) % visible_count();
        result.selection_changed = true;
    }
    return result;
}

std::size_t MenuController::visible_count() const {
    if (!packed_view.mounted()) return entry_count();
    if (level_ == 0) return 3;
    if (level_ == 2) return 1 + packed_view.count(category_, letter_) +
                              legacy_overlay_count(category_, letter_);
    std::size_t letters = 0;
    for (char letter = 'A'; letter <= 'Z'; ++letter)
        if (packed_view.has_letter(category_, letter) ||
            legacy_overlay_count(category_, letter) != 0) ++letters;
    return 1 + letters;
}

const char* MenuController::label(std::size_t index) const {
    if (!packed_view.mounted()) return entry(index).display_name;
    if (level_ == 0) {
        constexpr const char* labels[] = {"Locomotive BASIC", "Arcade", "Simulation"};
        return index < 3 ? labels[index] : "?";
    }
    if (index == 0) return "< Back";
    if (level_ == 1) {
        constexpr const char* letters[] = {
            "A","B","C","D","E","F","G","H","I","J","K","L","M",
            "N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
        std::size_t wanted = index - 1;
        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            if ((packed_view.has_letter(category_, letter) ||
                 legacy_overlay_count(category_, letter) != 0) && wanted-- == 0) {
                return letters[letter - 'A'];
            }
        }
        return "?";
    }
    Entry game{};
    return category_record(category_, letter_, index - 1, game) ? game.display_name : "?";
}

Entry MenuController::selected_entry() const {
    if (!packed_view.mounted()) {
        if (selected_ < std::size(kEntries)) return kEntries[selected_];
        library::GameInfo game{};
        return library_view.verified_game(selected_ - std::size(kEntries), game)
                   ? library_entry(game) : kEntries[0];
    }
    if (level_ == 0 && selected_ == 0) return entry(0);
    if (level_ == 2 && selected_ > 0) {
        Entry game{};
        const std::size_t packed_count = packed_view.count(category_, letter_);
        if (selected_ - 1 < packed_count) {
            if (category_record(category_, letter_, selected_ - 1, game)) return game;
        } else {
            library::GameInfo legacy{};
            if (legacy_overlay_at(selected_ - 1 - packed_count, legacy, category_,
                                  letter_, true)) return library_entry(legacy);
        }
    }
    return entry(0);
}

std::size_t MenuController::first_visible(std::size_t visible_slots) const {
    if ((visible_slots == 0) || (visible_count() <= visible_slots)) return 0;
    const std::size_t max_start = visible_count() - visible_slots;
    return std::min(selected_, max_start);
}

}  // namespace cpcexplorer::launcher
