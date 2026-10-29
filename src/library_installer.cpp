#include "library_installer.h"

#include "game_library.h"

#include <algorithm>
#include <cstring>

namespace cpcexplorer::storage {
namespace {
constexpr std::uint32_t kPackageBytes = 96U + library::kSnapshotBytes;
constexpr std::uint32_t kAllocationBytes = 17U * 4096U;
constexpr std::uint32_t kCommit = 0x4d4d4f43U;
constexpr std::uint32_t kRemovalPending = 1U;
constexpr std::size_t kFlagsOffset = 36;
constexpr std::size_t kHighWaterOffset = 40;
constexpr std::size_t kPendingIdOffset = 44;
std::uint16_t u16(const std::uint8_t* p) { return p[0] | static_cast<std::uint16_t>(p[1] << 8); }
std::uint32_t u32(const std::uint8_t* p) {
    return p[0] | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
void p16(std::uint8_t* p, std::uint16_t v) { p[0] = v; p[1] = v >> 8; }
void p32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
std::uint32_t masked_crc(const std::uint8_t* p, std::size_t n, std::size_t at) {
    std::uint8_t temp[library::kDescriptorBytes]{};
    if (n > sizeof(temp)) return 0;
    std::memcpy(temp, p, n);
    std::memset(temp + at, 0, 4);
    return library::crc32(temp, n);
}
std::uint32_t align_sector(std::uint32_t value) {
    return (value + 4095U) & ~4095U;
}
bool terminated_printable(const std::uint8_t* text, std::size_t size) {
    if (text[0] == 0) return false;
    for (std::size_t i = 0; i < size; ++i) {
        if (text[i] == 0) return true;
        if (text[i] < 0x20 || text[i] > 0x7e) return false;
    }
    return false;
}
bool valid_game_id(const char* id) {
    if (id == nullptr || id[0] == 0) return false;
    if (!((id[0] >= 'a' && id[0] <= 'z') || (id[0] >= '0' && id[0] <= '9'))) return false;
    std::size_t length = 0;
    for (; length < 16 && id[length] != 0; ++length) {
        const char c = id[length];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return length > 0 && length < 16;
}
bool valid_catalog_header(const std::uint8_t* header) {
    return std::memcmp(header, "CPCLIB1", 7) == 0 &&
           u16(header + 8) == library::kFormatVersion &&
           u16(header + 10) == library::kCatalogHeaderBytes &&
           u16(header + 16) <= library::kMaximumEntries &&
           u16(header + 18) == library::kDescriptorBytes &&
           u32(header + 20) == library::kDataStart &&
           u32(header + 24) == library::kLibraryBytes &&
           u32(header + 32) == kCommit &&
           masked_crc(header, library::kCatalogHeaderBytes, 28) == u32(header + 28) &&
           (u32(header + kFlagsOffset) & ~kRemovalPending) == 0;
}
bool valid_descriptor(const std::uint8_t* descriptor) {
    if (masked_crc(descriptor, library::kDescriptorBytes, 72) != u32(descriptor + 72) ||
        !terminated_printable(descriptor, 16) || !terminated_printable(descriptor + 16, 40) ||
        descriptor[56] != 1 || descriptor[57] != 64 || descriptor[58] == 0 ||
        descriptor[59] > 2) return false;
    const std::uint32_t offset = u32(descriptor + 60), length = u32(descriptor + 64);
    return (offset & 0xfffU) == 0 && offset >= library::kDataStart &&
           length == library::kSnapshotBytes && length <= library::kLibraryBytes - offset;
}
bool descriptor_id_is(const std::uint8_t* descriptor, const char* id) {
    return std::strncmp(reinterpret_cast<const char*>(descriptor), id, 16) == 0;
}
bool scan_catalog(const std::uint8_t* catalog, std::uint32_t& high_water,
                  const char* forbidden_id = nullptr) {
    if (!valid_catalog_header(catalog)) return false;
    std::uint32_t high = library::kDataStart;
    const std::uint32_t saved_high = u32(catalog + kHighWaterOffset);
    if (saved_high != 0 && saved_high != UINT32_MAX) {
        if ((saved_high & 0xfffU) != 0 || saved_high < library::kDataStart ||
            saved_high > library::kLibraryBytes) return false;
        high = saved_high;
    }
    const std::uint16_t count = u16(catalog + 16);
    for (std::uint16_t i = 0; i < count; ++i) {
        const auto* descriptor = catalog + library::kCatalogHeaderBytes +
                                 i * library::kDescriptorBytes;
        if (!valid_descriptor(descriptor) ||
            (forbidden_id != nullptr && descriptor_id_is(descriptor, forbidden_id))) return false;
        high = std::max(high, align_sector(u32(descriptor + 60) + u32(descriptor + 64)));
    }
    high_water = high;
    return true;
}
void make_header(std::uint8_t* header, std::uint32_t generation, std::uint16_t count,
                 std::uint32_t high_water, std::uint32_t flags, const char* pending_id) {
    std::memset(header, 0, library::kCatalogHeaderBytes);
    std::memcpy(header, "CPCLIB1", 7);
    p16(header + 8, library::kFormatVersion); p16(header + 10, library::kCatalogHeaderBytes);
    p32(header + 12, generation); p16(header + 16, count);
    p16(header + 18, library::kDescriptorBytes); p32(header + 20, library::kDataStart);
    p32(header + 24, library::kLibraryBytes); p32(header + 32, kCommit);
    p32(header + kFlagsOffset, flags); p32(header + kHighWaterOffset, high_water);
    if (pending_id != nullptr)
        std::memcpy(header + kPendingIdOffset, pending_id,
                    std::min<std::size_t>(std::strlen(pending_id), 15));
    p32(header + 28, 0); p32(header + 28, library::crc32(header, library::kCatalogHeaderBytes));
}
}  // namespace

void Installer::fail(const char* message) { error_ = message; }

bool Installer::begin(std::uint32_t package_length) {
    abort();
    error_ = "none";
    if (!flash_.ready() || flash_.capacity() != library::kFlashBytes) {
        fail("flash_identity_or_capacity"); return false;
    }
    if (package_length != kPackageBytes) { fail("package_length"); return false; }
    package_length_ = package_length;
    active_ = true;
    return true;
}

bool Installer::prepare_payload() {
    static constexpr std::uint8_t magic[8] = {'C','P','C','G','A','M','E','1'};
    if (std::memcmp(package_header_, magic, 8) != 0 || u16(package_header_ + 8) != 1 ||
        u16(package_header_ + 10) != 96 || u32(package_header_ + 12) != package_length_) {
        fail("package_header"); return false;
    }
    auto* d = package_header_ + 16;
    if (masked_crc(d, 80, 72) != u32(d + 72) || d[56] != 1 || d[57] != 64 ||
        d[58] == 0 || d[59] > 2 || u32(d + 60) != 96 ||
        u32(d + 64) != library::kSnapshotBytes ||
        !terminated_printable(d, 16) || !terminated_printable(d + 16, 40)) {
        fail("package_descriptor"); return false;
    }
    library::View view;
    const auto* mapped = flash_.mapped_library();
    view.mount(mapped, library::kLibraryBytes);
    active_catalog_ = view.catalog_offset();
    generation_ = view.mounted() ? view.generation() : 0;
    if (generation_ == UINT32_MAX) { fail("catalog_generation_exhausted"); return false; }
    existing_count_ = view.mounted() ? static_cast<std::uint16_t>(view.declared_count()) : 0;
    if (existing_count_ >= library::kMaximumEntries) { fail("catalog_full"); return false; }
    std::uint32_t high = library::kDataStart;
    for (std::uint32_t slot = 0; slot < library::kCatalogSlotCount; ++slot) {
        const auto* candidate = mapped + slot * library::kCatalogSlotBytes;
        if (valid_catalog_header(candidate)) {
            std::uint32_t slot_high = library::kDataStart;
            if (!scan_catalog(candidate, slot_high)) {
                fail("catalog_allocation_metadata"); return false;
            }
            high = std::max(high, slot_high);
        }
    }
    if (view.mounted()) {
        std::memcpy(catalog_, mapped + active_catalog_, sizeof(catalog_));
        for (std::uint16_t i = 0; i < existing_count_; ++i) {
            const auto* old = catalog_ + 64 + i * 80;
            if (!valid_descriptor(old)) {
                fail("active_descriptor_corrupt"); return false;
            }
            if (descriptor_id_is(old, reinterpret_cast<const char*>(d))) {
                fail("duplicate_game_id"); return false;
            }
        }
    } else {
        std::memset(catalog_, 0xff, sizeof(catalog_));
    }
    data_offset_ = align_sector(high);
    const std::uint32_t flash_offset = library::kLibraryFlashOffset + data_offset_;
    if (!library::is_game_data_flash_range(flash_offset, kAllocationBytes)) {
        fail("snapshot_region_full"); return false;
    }
    if (!flash_.erase(flash_offset, kAllocationBytes)) { fail("payload_erase"); return false; }
    prepared_ = true;
    return true;
}

bool Installer::write(std::uint32_t offset, const std::uint8_t* data, std::size_t size) {
    if (!active_ || offset != received_ || data == nullptr || size == 0) {
        fail("stream_sequence"); return false;
    }
    std::size_t cursor = 0;
    while (cursor < size) {
        if (received_ < sizeof(package_header_)) {
            const std::size_t take = std::min<std::size_t>(
                size - cursor, sizeof(package_header_) - received_);
            std::memcpy(package_header_ + received_, data + cursor, take);
            cursor += take; received_ += static_cast<std::uint32_t>(take);
            if (received_ == sizeof(package_header_) && !prepare_payload()) return false;
            continue;
        }
        const std::size_t take = std::min(size - cursor, sizeof(page_) - page_used_);
        std::memcpy(page_ + page_used_, data + cursor, take);
        page_used_ += take; cursor += take; received_ += static_cast<std::uint32_t>(take);
        payload_received_ += static_cast<std::uint32_t>(take);
        if (page_used_ == sizeof(page_)) {
            const std::uint32_t target = library::kLibraryFlashOffset + data_offset_ +
                                         payload_received_ - sizeof(page_);
            if (!library::is_game_data_flash_range(target & ~0xfffU, 4096) ||
                !flash_.program(target, page_, sizeof(page_))) {
                fail("payload_program"); return false;
            }
            page_used_ = 0;
        }
    }
    return true;
}

bool Installer::publish_catalog() {
    const std::uint32_t inactive = active_catalog_ == 0 ? library::kCatalogSlotBytes : 0;
    auto* d = catalog_ + 64 + existing_count_ * 80;
    std::memcpy(d, package_header_ + 16, 80);
    p32(d + 60, data_offset_); p32(d + 64, library::kSnapshotBytes);
    p32(d + 72, 0); p32(d + 72, library::crc32(d, 80));
    make_header(catalog_, generation_ + 1, existing_count_ + 1,
                data_offset_ + kAllocationBytes, 0, nullptr);
    if (!write_catalog(inactive, "catalog_erase", "catalog_program", "catalog_commit"))
        return false;
    library::View verify;
    if (!verify.mount(flash_.mapped_library(), library::kLibraryBytes) ||
        verify.generation() != generation_ + 1) { fail("catalog_verify"); return false; }
    return true;
}

bool Installer::write_catalog(std::uint32_t slot, const char* erase_error,
                              const char* program_error, const char* commit_error) {
    const std::uint32_t target = library::kLibraryFlashOffset + slot;
    if ((slot != 0 && slot != library::kCatalogSlotBytes) ||
        !library::is_library_flash_range(target, sizeof(catalog_)) ||
        !flash_.erase(target, sizeof(catalog_))) { fail(erase_error); return false; }
    std::uint8_t final_page[256];
    std::memcpy(final_page, catalog_, sizeof(final_page));
    std::memset(catalog_ + 32, 0xff, 4);
    for (std::size_t at = 0; at < sizeof(catalog_); at += 256) {
        if (!flash_.program(target + static_cast<std::uint32_t>(at), catalog_ + at, 256)) {
            fail(program_error); return false;
        }
    }
    if (!flash_.program(target, final_page, sizeof(final_page))) {
        fail(commit_error); return false;
    }
    std::memcpy(catalog_, final_page, sizeof(final_page));
    return true;
}

bool Installer::verify_catalog(std::uint32_t slot, std::uint32_t generation,
                               std::uint16_t count, const char* forbidden_id) const {
    if (slot != 0 && slot != library::kCatalogSlotBytes) return false;
    const auto* catalog = flash_.mapped_library() + slot;
    std::uint32_t ignored = 0;
    return valid_catalog_header(catalog) && u32(catalog + 12) == generation &&
           u16(catalog + 16) == count && scan_catalog(catalog, ignored, forbidden_id);
}

bool Installer::commit() {
    if (!active_ || !prepared_ || received_ != package_length_ ||
        payload_received_ != library::kSnapshotBytes || page_used_ != 0) {
        fail("incomplete_payload"); return false;
    }
    const auto* snapshot = flash_.mapped_library() + data_offset_;
    stored_crc32_ = library::crc32(snapshot, library::kSnapshotBytes);
    if (stored_crc32_ != u32(package_header_ + 16 + 68) ||
        !library::is_supported_snapshot(snapshot, library::kSnapshotBytes,
                                        package_header_[16 + 58])) {
        fail("payload_readback"); return false;
    }
    if (!publish_catalog()) return false;
    active_ = false;
    return true;
}

bool Installer::remove(const char* game_id) {
    error_ = "none";
    if (active_) { fail("upload_active"); return false; }
    if (!flash_.ready() || flash_.capacity() != library::kFlashBytes) {
        fail("flash_identity_or_capacity"); return false;
    }
    if (!valid_game_id(game_id)) { fail("invalid_game_id"); return false; }
    if (std::strcmp(game_id, "basic") == 0 || std::strcmp(game_id, "locomotive_basic") == 0 ||
        std::strcmp(game_id, "knight_tyme") == 0) {
        fail("protected_builtin"); return false;
    }
    if (!recover_pending_removal()) return false;

    library::View view;
    const auto* mapped = flash_.mapped_library();
    if (!view.mount(mapped, library::kLibraryBytes)) { fail("game_not_found"); return false; }
    if (view.generation() > UINT32_MAX - 2U) {
        fail("catalog_generation_exhausted"); return false;
    }
    const std::uint32_t source_slot = view.catalog_offset();
    const auto* source = mapped + source_slot;
    std::uint32_t high_water = library::kDataStart;
    if (!scan_catalog(source, high_water)) { fail("active_catalog_invalid"); return false; }
    const std::uint16_t source_count = u16(source + 16);
    std::memcpy(catalog_, source, sizeof(catalog_));
    std::uint16_t write_index = 0;
    bool found = false;
    for (std::uint16_t read_index = 0; read_index < source_count; ++read_index) {
        auto* descriptor = catalog_ + library::kCatalogHeaderBytes +
                           read_index * library::kDescriptorBytes;
        if (descriptor_id_is(descriptor, game_id)) {
            found = true;
            continue;
        }
        if (write_index != read_index) {
            std::memmove(catalog_ + library::kCatalogHeaderBytes +
                         write_index * library::kDescriptorBytes,
                         descriptor, library::kDescriptorBytes);
        }
        ++write_index;
    }
    if (!found) { fail("game_not_found"); return false; }
    std::memset(catalog_ + library::kCatalogHeaderBytes +
                write_index * library::kDescriptorBytes, 0xff,
                sizeof(catalog_) - library::kCatalogHeaderBytes -
                write_index * library::kDescriptorBytes);

    const std::uint32_t pending_slot = source_slot == 0 ? library::kCatalogSlotBytes : 0;
    const std::uint32_t pending_generation = view.generation() + 1;
    make_header(catalog_, pending_generation, write_index, high_water,
                kRemovalPending, game_id);
    if (!write_catalog(pending_slot, "remove_pending_erase", "remove_pending_program",
                       "remove_pending_commit") ||
        !verify_catalog(pending_slot, pending_generation, write_index, game_id)) {
        if (std::strcmp(error_, "none") == 0) fail("remove_pending_verify");
        return false;
    }

    const std::uint32_t final_generation = pending_generation + 1;
    make_header(catalog_, final_generation, write_index, high_water, 0, nullptr);
    if (!write_catalog(source_slot, "remove_mirror_erase", "remove_mirror_program",
                       "remove_mirror_commit") ||
        !verify_catalog(source_slot, final_generation, write_index, game_id) ||
        !verify_catalog(pending_slot, pending_generation, write_index, game_id)) {
        if (std::strcmp(error_, "none") == 0) fail("remove_mirror_verify");
        return false;
    }
    library::View verify;
    if (!verify.mount(mapped, library::kLibraryBytes) ||
        verify.generation() != final_generation) {
        fail("remove_final_verify"); return false;
    }
    return true;
}

bool Installer::recover_pending_removal() {
    error_ = "none";
    if (active_) { fail("upload_active"); return false; }
    if (!flash_.ready() || flash_.capacity() != library::kFlashBytes) {
        fail("flash_identity_or_capacity"); return false;
    }
    library::View view;
    const auto* mapped = flash_.mapped_library();
    if (!view.mount(mapped, library::kLibraryBytes)) return true;
    const std::uint32_t pending_slot = view.catalog_offset();
    const auto* pending = mapped + pending_slot;
    if ((u32(pending + kFlagsOffset) & kRemovalPending) == 0) return true;
    if (!terminated_printable(pending + kPendingIdOffset, 16) ||
        !valid_game_id(reinterpret_cast<const char*>(pending + kPendingIdOffset))) {
        fail("pending_removal_id"); return false;
    }
    const char* game_id = reinterpret_cast<const char*>(pending + kPendingIdOffset);
    if (view.generation() == UINT32_MAX) { fail("catalog_generation_exhausted"); return false; }
    std::uint32_t high_water = library::kDataStart;
    if (!scan_catalog(pending, high_water, game_id)) {
        fail("pending_removal_catalog"); return false;
    }
    const std::uint16_t count = u16(pending + 16);
    const std::uint32_t mirror_slot = pending_slot == 0 ? library::kCatalogSlotBytes : 0;
    const std::uint32_t mirror_generation = view.generation() + 1;
    std::memcpy(catalog_, pending, sizeof(catalog_));
    make_header(catalog_, mirror_generation, count, high_water, 0, nullptr);
    if (!write_catalog(mirror_slot, "recovery_erase", "recovery_program", "recovery_commit") ||
        !verify_catalog(mirror_slot, mirror_generation, count, game_id) ||
        !verify_catalog(pending_slot, view.generation(), count, game_id)) {
        if (std::strcmp(error_, "none") == 0) fail("recovery_verify");
        return false;
    }
    return true;
}

void Installer::abort() {
    active_ = false; prepared_ = false; package_length_ = received_ = payload_received_ = 0;
    page_used_ = 0; data_offset_ = 0; stored_crc32_ = 0; existing_count_ = 0;
    active_catalog_ = UINT32_MAX;
}

}  // namespace cpcexplorer::storage
