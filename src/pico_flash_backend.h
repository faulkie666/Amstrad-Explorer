#pragma once

#include "library_installer.h"

#include <cstdint>

namespace cpcexplorer::storage {

class PicoFlash final : public Flash {
public:
    bool probe();
    bool ready() const override { return ready_; }
    const char* identity() const override { return identity_; }
    std::uint32_t capacity() const override { return capacity_; }
    const std::uint8_t* mapped_library() const override;
    bool erase(std::uint32_t flash_offset, std::size_t size) override;
    bool program(std::uint32_t flash_offset, const std::uint8_t* data,
                 std::size_t size) override;
    int last_sdk_error() const { return last_sdk_error_; }

private:
    bool ready_ = false;
    std::uint32_t capacity_ = 0;
    char identity_[24] = "unprobed";
    int last_sdk_error_ = 0;
};

}  // namespace cpcexplorer::storage
