#pragma once

#include <cstddef>
#include <cstdint>

#include "gameplay_input.h"
#include "packed_library.h"

namespace cpcexplorer::launcher {

enum class EntryType : std::uint8_t {
    Basic,
    Snapshot,
};

struct Entry {
    const char* display_name;
    EntryType type;
    const char* snapshot_id;
    const std::uint8_t* snapshot_data;
    std::size_t snapshot_size;
    input::Profile control_profile;
    bool packed = false;
    packed::Record packed_record{};
};

void configure_library(const std::uint8_t* bytes, std::size_t size);
bool configure_packed_library(const std::uint8_t* bytes, std::size_t size);
bool packed_library_mounted();
bool has_builtin_snapshot_id(const char* snapshot_id);
bool has_protected_snapshot_id(const char* snapshot_id);
Entry entry(std::size_t index);
std::size_t entry_count();

enum ButtonMask : std::uint8_t {
    ButtonX = 1U << 0,
    ButtonY = 1U << 1,
    ButtonZ = 1U << 2,
    ButtonA = 1U << 3,
    ButtonB = 1U << 4,
    ButtonC = 1U << 5,
};

struct Update {
    bool selection_changed;
    bool launch;
};

class MenuController {
public:
    static constexpr std::uint32_t debounce_ms = 30;

    void reset(std::uint8_t pressed_mask, std::uint32_t now_ms);
    Update update(std::uint8_t pressed_mask, std::uint32_t now_ms);
    std::size_t selected() const { return selected_; }
    std::size_t visible_count() const;
    const char* label(std::size_t index) const;
    Entry selected_entry() const;
    std::size_t first_visible(std::size_t visible_slots) const;

private:
    struct ButtonState {
        bool raw = false;
        bool stable = false;
        std::uint32_t changed_at = 0;
    };

    bool pressed_event(ButtonState& state, bool raw_pressed, std::uint32_t now_ms);

    std::size_t selected_ = 0;
    std::uint8_t level_ = 0;
    packed::Category category_ = packed::Category::Arcade;
    char letter_ = 'A';
    ButtonState x_{};
    ButtonState y_{};
    ButtonState z_{};
};

}  // namespace cpcexplorer::launcher
