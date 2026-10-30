#pragma once

#include "library_installer.h"

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::migration {

constexpr std::uint32_t kPackOffset = 0x43B000U;
constexpr std::uint32_t kPackBytes = 179360U;
constexpr std::uint32_t kReservedEnd = 0x467000U;
constexpr std::uint32_t kBackupOffset = 0x400000U;
constexpr std::uint32_t kBackupEnd = kPackOffset;
constexpr std::uint32_t kSavedHighWater = kReservedEnd - 0x400000U;
constexpr std::uint32_t kPackCrc32 = 0xEC7FD1E9U;
constexpr std::uint32_t kStagingCrc32 = 0xC82ACE86U;
constexpr std::size_t kPageBytes = 256;

enum class State : std::uint8_t {
    Unavailable,
    Legacy,
    ReservedIncomplete,
    Receiving,
    Verified,
    Active,
    Ambiguous,
};

struct Manifest {
    std::uint32_t protocol_version = 0;
    std::uint32_t flash_offset = 0;
    std::uint32_t image_bytes = 0;
    std::uint32_t image_crc32 = 0;
    std::uint8_t image_sha256[32]{};
};

class Transport {
public:
    Transport(storage::Flash& flash, storage::Installer& installer,
              std::uint8_t* scratch, std::size_t scratch_size);
    bool recover();
    bool begin(const Manifest& manifest, bool builtin_knight_tyme_valid);
    bool write_page(std::uint32_t sequence, const std::uint8_t* data, std::size_t size);
    bool verify();
    bool activate();
    void abort();
    State state() const { return state_; }
    const char* error() const { return error_; }
    std::uint32_t received() const { return received_; }
    std::uint32_t readback_crc(std::uint32_t offset, std::uint32_t length) const;

    static const std::uint8_t expected_sha256[32];

private:
    bool exact_manifest(const Manifest& manifest) const;
    bool page_is_header(std::uint32_t offset) const;
    void fail(const char* message, bool ambiguous = false);

    storage::Flash& flash_;
    storage::Installer& installer_;
    std::uint8_t* scratch_;
    std::size_t scratch_size_;
    State state_ = State::Unavailable;
    const char* error_ = "uninitialized";
    std::uint32_t received_ = 0;
    std::uint32_t next_sequence_ = 0;
    std::uint32_t running_crc_ = 0xFFFFFFFFU;
};

const char* state_name(State state);
bool is_read_range(std::uint32_t flash_offset, std::uint32_t length);

}  // namespace cpcexplorer::migration
