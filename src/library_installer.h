#pragma once

#include "upload_protocol.h"

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::storage {

class Flash {
public:
    virtual ~Flash() = default;
    virtual bool ready() const = 0;
    virtual const char* identity() const = 0;
    virtual std::uint32_t capacity() const = 0;
    virtual const std::uint8_t* mapped_library() const = 0;
    virtual bool erase(std::uint32_t flash_offset, std::size_t size) = 0;
    virtual bool program(std::uint32_t flash_offset, const std::uint8_t* data,
                         std::size_t size) = 0;
};

class Installer final : public upload::Sink {
public:
    explicit Installer(Flash& flash) : flash_(flash) {}
    bool begin(std::uint32_t package_length) override;
    bool write(std::uint32_t offset, const std::uint8_t* data, std::size_t size) override;
    bool commit() override;
    void abort() override;
    bool remove(const char* game_id);
    bool recover_pending_removal();
    const char* error() const { return error_; }
    std::uint32_t stored_crc32() const { return stored_crc32_; }
    std::uint32_t data_offset() const { return data_offset_; }

private:
    bool prepare_payload();
    bool publish_catalog();
    bool write_catalog(std::uint32_t slot, const char* erase_error,
                       const char* program_error, const char* commit_error);
    bool verify_catalog(std::uint32_t slot, std::uint32_t generation,
                        std::uint16_t count, const char* forbidden_id = nullptr) const;
    void fail(const char* message);

    Flash& flash_;
    const char* error_ = "none";
    bool active_ = false;
    bool prepared_ = false;
    std::uint32_t package_length_ = 0;
    std::uint32_t received_ = 0;
    std::uint32_t payload_received_ = 0;
    std::uint32_t data_offset_ = 0;
    std::uint32_t stored_crc32_ = 0;
    std::uint32_t generation_ = 0;
    std::uint32_t active_catalog_ = UINT32_MAX;
    std::uint16_t existing_count_ = 0;
    std::uint8_t package_header_[96]{};
    std::uint8_t page_[256]{};
    std::size_t page_used_ = 0;
    std::uint8_t catalog_[16 * 1024]{};
};

}  // namespace cpcexplorer::storage
