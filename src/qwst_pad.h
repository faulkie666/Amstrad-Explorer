#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::qwst {

enum Button : std::uint16_t {
    A = 1U << 0,
    B = 1U << 1,
    X = 1U << 2,
    Y = 1U << 3,
    Up = 1U << 4,
    Down = 1U << 5,
    Left = 1U << 6,
    Right = 1U << 7,
    Plus = 1U << 8,
    Minus = 1U << 9,
};

inline constexpr std::uint8_t kAddresses[] = {0x21, 0x23, 0x25, 0x27};

struct Bus {
    void* context = nullptr;
    bool (*write)(void*, std::uint8_t, const std::uint8_t*, std::size_t) = nullptr;
    bool (*write_read)(void*, std::uint8_t, const std::uint8_t*, std::size_t,
                       std::uint8_t*, std::size_t) = nullptr;
};

struct Sample {
    std::uint16_t buttons = 0;
    std::uint8_t address = 0;
    bool connected = false;
    bool changed = false;
};

class Driver {
public:
    explicit Driver(Bus bus) : bus_(bus) {}

    Sample poll(std::uint32_t now_ms);
    Sample sample() const { return {stable_, address_, connected_, false}; }

    static std::uint16_t decode_buttons(std::uint16_t tca_state);

private:
    static constexpr std::uint32_t kReconnectMs = 1'000;
    static constexpr std::uint8_t kDebounceSamples = 2;

    bool probe();
    bool configure(std::uint8_t address);
    bool write16(std::uint8_t address, std::uint8_t reg, std::uint16_t value);
    bool read16(std::uint8_t address, std::uint8_t reg, std::uint16_t& value);
    void disconnect(std::uint32_t now_ms);

    Bus bus_{};
    std::uint32_t next_probe_ms_ = 0;
    std::uint16_t stable_ = 0;
    std::uint16_t candidate_ = 0;
    std::uint8_t address_ = 0;
    std::uint8_t candidate_samples_ = 0;
    bool connected_ = false;
    bool first_probe_ = true;
};

}  // namespace cpcexplorer::qwst
