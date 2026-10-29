#include "game_library.h"

#include <cstring>

namespace cpcexplorer::library {
namespace {

constexpr std::uint8_t kCatalogMagic[8] = {'C', 'P', 'C', 'L', 'I', 'B', '1', 0};
constexpr std::uint8_t kPackageMagic[8] = {'C', 'P', 'C', 'G', 'A', 'M', 'E', '1'};
constexpr std::uint32_t kCommitMarker = 0x4d4d4f43U;  // "COMM", little-endian

std::uint16_t u16(const std::uint8_t* value) {
    return static_cast<std::uint16_t>(value[0] | (value[1] << 8));
}

std::uint32_t u32(const std::uint8_t* value) {
    return static_cast<std::uint32_t>(value[0]) |
           (static_cast<std::uint32_t>(value[1]) << 8) |
           (static_cast<std::uint32_t>(value[2]) << 16) |
           (static_cast<std::uint32_t>(value[3]) << 24);
}

std::uint32_t crc32_masked(const std::uint8_t* data, std::size_t size,
                           std::size_t masked_offset, std::size_t masked_size) {
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = 0; i < size; ++i) {
        const std::uint8_t byte = (i >= masked_offset && i < masked_offset + masked_size)
                                      ? 0
                                      : data[i];
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

bool terminated_printable(const std::uint8_t* text, std::size_t size) {
    if (size == 0 || text[0] == 0) return false;
    for (std::size_t i = 0; i < size; ++i) {
        if (text[i] == 0) return true;
        if (text[i] < 0x20 || text[i] > 0x7e) return false;
    }
    return false;
}

bool valid_catalog_header(const std::uint8_t* catalog, std::size_t available) {
    if (available < kCatalogHeaderBytes ||
        std::memcmp(catalog, kCatalogMagic, sizeof(kCatalogMagic)) != 0 ||
        u16(catalog + 8) != kFormatVersion ||
        u16(catalog + 10) != kCatalogHeaderBytes ||
        u16(catalog + 18) != kDescriptorBytes ||
        u16(catalog + 16) > kMaximumEntries ||
        u32(catalog + 20) < kDataStart ||
        u32(catalog + 24) > kLibraryBytes ||
        u32(catalog + 20) > u32(catalog + 24) ||
        u32(catalog + 32) != kCommitMarker) return false;
    return crc32_masked(catalog, kCatalogHeaderBytes, 28, 4) == u32(catalog + 28);
}

bool parse_descriptor(const std::uint8_t* descriptor, const std::uint8_t* storage,
                      std::size_t storage_size, std::uint32_t minimum_offset,
                      std::uint32_t maximum_offset, bool require_sector_alignment,
                      GameInfo& result) {
    if (crc32_masked(descriptor, kDescriptorBytes, 72, 4) != u32(descriptor + 72) ||
        !terminated_printable(descriptor, 16) ||
        !terminated_printable(descriptor + 16, 40) ||
        descriptor[56] != static_cast<std::uint8_t>(MachineType::Cpc464) ||
        descriptor[57] != 64 || descriptor[58] == 0 || descriptor[59] > 2) return false;
    const std::uint32_t offset = u32(descriptor + 60);
    const std::uint32_t length = u32(descriptor + 64);
    if (offset < minimum_offset || offset > maximum_offset ||
        (require_sector_alignment && (offset & 0xfffU) != 0) ||
        length != kSnapshotBytes || length > maximum_offset - offset ||
        offset > storage_size || length > storage_size - offset) return false;
    const auto* payload = storage + offset;
    if (crc32(payload, length) != u32(descriptor + 68) ||
        !is_supported_snapshot(payload, length, descriptor[58])) return false;
    result.game_id = reinterpret_cast<const char*>(descriptor);
    result.title = reinterpret_cast<const char*>(descriptor + 16);
    result.machine = MachineType::Cpc464;
    result.ram_kib = descriptor[57];
    result.snapshot_version = descriptor[58];
    result.control_profile = descriptor[59];
    result.data = payload;
    result.data_offset = offset;
    result.data_length = length;
    result.data_crc32 = u32(descriptor + 68);
    return true;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr && size != 0) return 0;
    std::uint32_t crc = 0xffffffffU;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

bool is_supported_snapshot(const std::uint8_t* data, std::size_t size,
                           std::uint8_t expected_version) {
    static constexpr std::uint8_t magic[8] = {'M', 'V', ' ', '-', ' ', 'S', 'N', 'A'};
    if (data == nullptr || size != kSnapshotBytes ||
        std::memcmp(data, magic, sizeof(magic)) != 0 ||
        data[16] == 0 || data[107] != 64 || data[108] != 0) return false;
    return expected_version == 0 || data[16] == expected_version;
}

bool overlaps_firmware(std::uint32_t flash_offset, std::uint32_t length) {
    if (length == 0) return false;
    if (flash_offset >= kFlashBytes || length > kFlashBytes - flash_offset) return true;
    return flash_offset < kFirmwareReserveBytes;
}

bool is_library_flash_range(std::uint32_t flash_offset, std::uint32_t length) {
    if (length == 0 || flash_offset < kLibraryFlashOffset ||
        flash_offset >= kE10GuardFlashOffset) return false;
    return length <= kE10GuardFlashOffset - flash_offset;
}

bool is_game_data_flash_range(std::uint32_t flash_offset, std::uint32_t length) {
    constexpr std::uint32_t first_data = kLibraryFlashOffset + kDataStart;
    return (flash_offset & 0xfffU) == 0 && flash_offset >= first_data &&
           is_library_flash_range(flash_offset, length);
}

bool View::mount(const std::uint8_t* bytes, std::size_t size) {
    library_ = bytes;
    library_size_ = size;
    catalog_ = nullptr;
    generation_ = 0;
    declared_count_ = 0;
    valid_count_ = 0;
    if (bytes == nullptr || size < kDataStart) return false;
    for (std::uint32_t slot = 0; slot < kCatalogSlotCount; ++slot) {
        const std::size_t offset = slot * kCatalogSlotBytes;
        const auto* candidate = bytes + offset;
        if (!valid_catalog_header(candidate, size - offset)) continue;
        const std::uint32_t generation = u32(candidate + 12);
        if (catalog_ == nullptr || static_cast<std::int32_t>(generation - generation_) > 0) {
            catalog_ = candidate;
            generation_ = generation;
        }
    }
    if (catalog_ == nullptr) return false;
    declared_count_ = u16(catalog_ + 16);
    for (std::uint16_t i = 0; i < declared_count_; ++i) {
        GameInfo ignored{};
        if (descriptor(i, ignored)) valid_indices_[valid_count_++] = i;
    }
    return true;
}

bool View::descriptor(std::size_t descriptor_index, GameInfo& result) const {
    if (catalog_ == nullptr || descriptor_index >= declared_count_) return false;
    const std::size_t descriptor_offset = kCatalogHeaderBytes + descriptor_index * kDescriptorBytes;
    if (descriptor_offset + kDescriptorBytes > kCatalogSlotBytes) return false;
    return parse_descriptor(catalog_ + descriptor_offset, library_, library_size_,
                            u32(catalog_ + 20), u32(catalog_ + 24), true, result);
}

bool View::game(std::size_t valid_index, GameInfo& result) const {
    if (valid_index >= valid_count_) return false;
    return descriptor(valid_indices_[valid_index], result);
}

std::uint32_t View::catalog_offset() const {
    return catalog_ == nullptr ? UINT32_MAX :
        static_cast<std::uint32_t>(catalog_ - library_);
}

bool parse_package(const std::uint8_t* bytes, std::size_t size, GameInfo& result) {
    constexpr std::size_t package_header = 16;
    constexpr std::size_t payload_offset = package_header + kDescriptorBytes;
    if (bytes == nullptr || size < payload_offset ||
        std::memcmp(bytes, kPackageMagic, sizeof(kPackageMagic)) != 0 ||
        u16(bytes + 8) != kFormatVersion || u16(bytes + 10) != payload_offset ||
        u32(bytes + 12) != size) return false;
    return parse_descriptor(bytes + package_header, bytes, size,
                            static_cast<std::uint32_t>(payload_offset),
                            static_cast<std::uint32_t>(size), false, result);
}

}  // namespace cpcexplorer::library
