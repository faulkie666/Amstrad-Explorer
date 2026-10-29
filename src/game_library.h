#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::library {

constexpr std::uint32_t kFlashBytes = 16U * 1024U * 1024U;
constexpr std::uint32_t kFirmwareReserveBytes = 4U * 1024U * 1024U;
constexpr std::uint32_t kLibraryFlashOffset = kFirmwareReserveBytes;
constexpr std::uint32_t kE10GuardBytes = 64U * 1024U;
constexpr std::uint32_t kE10GuardFlashOffset = kFlashBytes - kE10GuardBytes;
constexpr std::uint32_t kLibraryBytes = kE10GuardFlashOffset - kLibraryFlashOffset;
constexpr std::uint32_t kCatalogSlotBytes = 16U * 1024U;
constexpr std::uint32_t kCatalogSlotCount = 2;
constexpr std::uint32_t kDataStart = kCatalogSlotBytes * kCatalogSlotCount;
constexpr std::uint16_t kFormatVersion = 1;
constexpr std::uint16_t kDescriptorBytes = 80;
constexpr std::uint16_t kCatalogHeaderBytes = 64;
constexpr std::uint16_t kMaximumEntries = 192;
constexpr std::uint32_t kSnapshotBytes = 256U + 64U * 1024U;
constexpr std::uintptr_t kLibraryXipAddress = 0x10000000U + kLibraryFlashOffset;
static_assert(kLibraryFlashOffset + kLibraryBytes == kE10GuardFlashOffset);
static_assert(kDataStart < kLibraryBytes);

enum class MachineType : std::uint8_t {
    Cpc464 = 1,
};

struct GameInfo {
    const char* game_id = nullptr;
    const char* title = nullptr;
    MachineType machine = MachineType::Cpc464;
    std::uint8_t ram_kib = 0;
    std::uint8_t snapshot_version = 0;
    const std::uint8_t* data = nullptr;
    std::uint32_t data_offset = 0;
    std::uint32_t data_length = 0;
    std::uint32_t data_crc32 = 0;
};

std::uint32_t crc32(const std::uint8_t* data, std::size_t size);
bool is_supported_snapshot(const std::uint8_t* data, std::size_t size,
                           std::uint8_t expected_version = 0);
bool overlaps_firmware(std::uint32_t flash_offset, std::uint32_t length);
bool is_library_flash_range(std::uint32_t flash_offset, std::uint32_t length);
bool is_game_data_flash_range(std::uint32_t flash_offset, std::uint32_t length);

class View {
public:
    bool mount(const std::uint8_t* bytes, std::size_t size);
    bool mounted() const { return catalog_ != nullptr; }
    std::uint32_t generation() const { return generation_; }
    std::size_t declared_count() const { return declared_count_; }
    std::size_t valid_count() const { return valid_count_; }
    std::uint32_t catalog_offset() const;
    bool game(std::size_t valid_index, GameInfo& result) const;

private:
    bool descriptor(std::size_t descriptor_index, GameInfo& result) const;

    const std::uint8_t* library_ = nullptr;
    std::size_t library_size_ = 0;
    const std::uint8_t* catalog_ = nullptr;
    std::uint32_t generation_ = 0;
    std::uint16_t declared_count_ = 0;
    std::uint16_t valid_count_ = 0;
    std::uint16_t valid_indices_[kMaximumEntries]{};
};

// A .cpcgame file is a deterministic 16-byte package header, one library
// descriptor, and the validated SNA payload. It is a transport container, not
// a disk/tape image and not a raw-RAM loader.
bool parse_package(const std::uint8_t* bytes, std::size_t size, GameInfo& result);

}  // namespace cpcexplorer::library
