#include "qwst_pad.h"

namespace cpcexplorer::qwst {
namespace {

constexpr std::uint8_t kInputPort0 = 0x00;
constexpr std::uint8_t kOutputPort0 = 0x02;
constexpr std::uint8_t kPolarityPort0 = 0x04;
constexpr std::uint8_t kConfigurationPort0 = 0x06;

constexpr bool due(std::uint32_t now, std::uint32_t deadline) {
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

}  // namespace

std::uint16_t Driver::decode_buttons(std::uint16_t state) {
    std::uint16_t result = 0;
    if (state & (1U << 14)) result |= A;
    if (state & (1U << 12)) result |= B;
    if (state & (1U << 15)) result |= X;
    if (state & (1U << 13)) result |= Y;
    if (state & (1U << 1)) result |= Up;
    if (state & (1U << 4)) result |= Down;
    if (state & (1U << 2)) result |= Left;
    if (state & (1U << 3)) result |= Right;
    if (state & (1U << 11)) result |= Plus;
    if (state & (1U << 5)) result |= Minus;
    return result;
}

bool Driver::write16(std::uint8_t address, std::uint8_t reg, std::uint16_t value) {
    if (!bus_.write) return false;
    const std::uint8_t data[] = {
        reg, static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8)};
    return bus_.write(bus_.context, address, data, sizeof(data));
}

bool Driver::read16(std::uint8_t address, std::uint8_t reg, std::uint16_t& value) {
    if (!bus_.write_read) return false;
    std::uint8_t data[2]{};
    if (!bus_.write_read(bus_.context, address, &reg, 1, data, sizeof(data))) return false;
    value = static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
    return true;
}

bool Driver::configure(std::uint8_t address) {
    // Values and byte order match Pimoroni's MIT-licensed reference driver.
    // The polarity register makes each active-low button read as a set bit.
    std::uint16_t ignored = 0;
    return read16(address, kInputPort0, ignored) &&
           write16(address, kConfigurationPort0, 0xF93F) &&
           write16(address, kPolarityPort0, 0xF83F) &&
           write16(address, kOutputPort0, 0x06C0);
}

bool Driver::probe() {
    for (const auto address : kAddresses) {
        if (configure(address)) {
            address_ = address;
            connected_ = true;
            candidate_ = stable_ = 0;
            candidate_samples_ = 0;
            return true;
        }
    }
    return false;
}

void Driver::disconnect(std::uint32_t now_ms) {
    connected_ = false;
    address_ = 0;
    candidate_ = stable_ = 0;
    candidate_samples_ = 0;
    next_probe_ms_ = now_ms + kReconnectMs;
}

Sample Driver::poll(std::uint32_t now_ms) {
    const auto old_buttons = stable_;
    const auto old_connected = connected_;
    if (!connected_) {
        if (first_probe_ || due(now_ms, next_probe_ms_)) {
            first_probe_ = false;
            if (!probe()) next_probe_ms_ = now_ms + kReconnectMs;
        }
        return {stable_, address_, connected_,
                stable_ != old_buttons || connected_ != old_connected};
    }

    std::uint16_t raw = 0;
    if (!read16(address_, kInputPort0, raw)) {
        disconnect(now_ms);
        return {stable_, address_, connected_, true};
    }
    raw = decode_buttons(raw);
    if (raw != candidate_) {
        candidate_ = raw;
        candidate_samples_ = 1;
    } else if (candidate_samples_ < kDebounceSamples) {
        ++candidate_samples_;
        if (candidate_samples_ == kDebounceSamples) stable_ = candidate_;
    }
    return {stable_, address_, connected_,
            stable_ != old_buttons || connected_ != old_connected};
}

}  // namespace cpcexplorer::qwst
