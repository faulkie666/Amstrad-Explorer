#pragma once

#include <cstddef>
#include <cstdint>

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
};

void configure_library(const std::uint8_t* bytes, std::size_t size);
Entry entry(std::size_t index);
std::size_t entry_count();

enum ButtonMask : std::uint8_t {
    ButtonX = 1U << 0,
    ButtonY = 1U << 1,
    ButtonZ = 1U << 2,
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
    std::size_t first_visible(std::size_t visible_slots) const;

private:
    struct ButtonState {
        bool raw = false;
        bool stable = false;
        std::uint32_t changed_at = 0;
    };

    bool pressed_event(ButtonState& state, bool raw_pressed, std::uint32_t now_ms);

    std::size_t selected_ = 0;
    ButtonState x_{};
    ButtonState y_{};
    ButtonState z_{};
};

}  // namespace cpcexplorer::launcher
