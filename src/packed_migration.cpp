#include "packed_migration.h"

#include "game_library.h"
#include "packed_library.h"

#include <algorithm>
#include <cstring>

namespace cpcexplorer::migration {
namespace {
constexpr std::uint32_t kLibraryOffset = library::kLibraryFlashOffset;
constexpr std::uint32_t kPackRelative = kPackOffset - kLibraryOffset;
constexpr std::uint32_t kHeaderBOffset = 4096U;
constexpr std::uint32_t kEraseBytes = kReservedEnd - kPackOffset;

void update_crc(std::uint32_t& crc, const std::uint8_t* data, std::size_t size) {
    while (size--) {
        crc ^= *data++;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
}
}

const std::uint8_t Transport::expected_sha256[32] = {
    0x27,0xdc,0x07,0x1d,0x93,0x60,0xb0,0x8e,0x12,0x48,0xe0,0xe9,0x0a,0xff,0x48,0x4c,
    0x64,0x8c,0xc9,0x5b,0xb2,0xd4,0x36,0xdc,0x7a,0xa9,0x1b,0x1e,0xf1,0xe2,0x8f,0xbe,
};

Transport::Transport(storage::Flash& flash, storage::Installer& installer,
                     std::uint8_t* scratch, std::size_t scratch_size)
    : flash_(flash), installer_(installer), scratch_(scratch), scratch_size_(scratch_size) {}

const char* state_name(State state) {
    switch (state) {
        case State::Unavailable: return "unavailable";
        case State::Legacy: return "legacy";
        case State::ReservedIncomplete: return "reserved-incomplete";
        case State::Receiving: return "receiving";
        case State::Verified: return "verified";
        case State::Active: return "active";
        case State::Ambiguous: return "ambiguous";
    }
    return "unknown";
}

bool is_read_range(std::uint32_t flash_offset, std::uint32_t length) {
    if (length == 0 || length > 128 || flash_offset < kBackupOffset) return false;
    return flash_offset < kReservedEnd && length <= kReservedEnd - flash_offset;
}

void Transport::fail(const char* message, bool ambiguous) {
    error_ = message;
    state_ = ambiguous ? State::Ambiguous : state_;
}

bool Transport::recover() {
    received_ = next_sequence_ = 0; running_crc_ = 0xFFFFFFFFU;
    if (!flash_.ready() || flash_.capacity() != library::kFlashBytes ||
        std::strcmp(flash_.identity(), "EF-40-18") != 0 || scratch_ == nullptr ||
        scratch_size_ < 3 * kPageBytes) {
        state_ = State::Unavailable; error_ = "identity_capacity_or_scratch"; return false;
    }
    if (!installer_.packed_region_reserved(kSavedHighWater)) {
        state_ = State::Legacy; error_ = "none"; return true;
    }
    packed::View view;
    const auto* image = flash_.mapped_library() + kPackRelative;
    if (view.mount(image, kPackBytes) && view.count() == 6 && view.launchable_count() == 5) {
        state_ = State::Active; error_ = "none"; return true;
    }
    state_ = State::ReservedIncomplete; error_ = "none"; return true;
}

bool Transport::exact_manifest(const Manifest& manifest) const {
    return manifest.protocol_version == 1 && manifest.flash_offset == kPackOffset &&
           manifest.image_bytes == kPackBytes && manifest.image_crc32 == kPackCrc32 &&
           std::memcmp(manifest.image_sha256, expected_sha256, 32) == 0;
}

bool Transport::begin(const Manifest& manifest, bool builtin_knight_tyme_valid) {
    if (state_ == State::Receiving || state_ == State::Verified || state_ == State::Ambiguous) {
        fail("invalid_state"); return false;
    }
    if (!exact_manifest(manifest)) { fail("manifest"); return false; }
    if (!flash_.ready() || flash_.capacity() != library::kFlashBytes ||
        std::strcmp(flash_.identity(), "EF-40-18") != 0) {
        fail("identity_capacity"); return false;
    }
    if (!builtin_knight_tyme_valid) {
        fail("builtin_knight_tyme"); return false;
    }
    if (!installer_.reserve_for_packed_library(kSavedHighWater)) {
        fail(installer_.error()); return false;
    }
    if (!flash_.erase(kPackOffset, kEraseBytes)) {
        fail("pack_erase", true); return false;
    }
    std::memset(scratch_, 0xFF, 3 * kPageBytes);
    received_ = next_sequence_ = 0; running_crc_ = 0xFFFFFFFFU;
    state_ = State::Receiving; error_ = "none"; return true;
}

bool Transport::page_is_header(std::uint32_t offset) const {
    return offset == 0 || offset == kHeaderBOffset;
}

bool Transport::write_page(std::uint32_t sequence, const std::uint8_t* data, std::size_t size) {
    if (state_ != State::Receiving) { fail("invalid_state"); return false; }
    if (sequence != next_sequence_ || data == nullptr || received_ >= kPackBytes) {
        fail("sequence"); return false;
    }
    const std::uint32_t remaining = kPackBytes - received_;
    const std::size_t expected = std::min<std::size_t>(kPageBytes, remaining);
    if (size != expected || received_ / kPageBytes != sequence) {
        fail("page_size_or_offset"); return false;
    }
    std::uint8_t* page = scratch_ + 2 * kPageBytes;
    std::memset(page, 0xFF, kPageBytes); std::memcpy(page, data, size);
    if (page_is_header(received_)) {
        std::memcpy(scratch_ + (received_ == 0 ? 0 : kPageBytes), page, kPageBytes);
    } else {
        const std::uint32_t target = kPackOffset + received_;
        if (target < kPackOffset || target > kReservedEnd - kPageBytes ||
            !flash_.program(target, page, kPageBytes)) {
            fail("page_program", true); return false;
        }
        const auto* readback = flash_.mapped_library() + kPackRelative + received_;
        if (std::memcmp(readback, page, kPageBytes) != 0) {
            fail("page_readback", true); return false;
        }
    }
    update_crc(running_crc_, data, size);
    received_ += static_cast<std::uint32_t>(size); ++next_sequence_;
    return true;
}

std::uint32_t Transport::readback_crc(std::uint32_t offset, std::uint32_t length) const {
    if (offset > kPackBytes || length > kPackBytes - offset) return 0;
    return library::crc32(flash_.mapped_library() + kPackRelative + offset, length);
}

bool Transport::verify() {
    if (state_ != State::Receiving || received_ != kPackBytes || ~running_crc_ != kPackCrc32) {
        fail("stream_integrity"); return false;
    }
    if (readback_crc(0, kPackBytes) != kStagingCrc32) {
        fail("staging_readback", true); return false;
    }
    state_ = State::Verified; error_ = "none"; return true;
}

bool Transport::activate() {
    if (state_ != State::Verified) { fail("invalid_state"); return false; }
    if (!flash_.program(kPackOffset + kHeaderBOffset, scratch_ + kPageBytes, kPageBytes)) {
        fail("header_b_program", true); return false;
    }
    packed::View secondary;
    const auto* image = flash_.mapped_library() + kPackRelative;
    if (!secondary.mount(image, kPackBytes) || secondary.count() != 6 ||
        secondary.launchable_count() != 5) {
        fail("header_b_verify", true); return false;
    }
    if (!flash_.program(kPackOffset, scratch_, kPageBytes)) {
        fail("header_a_program", true); return false;
    }
    packed::View complete;
    if (readback_crc(0, kPackBytes) != kPackCrc32 || !complete.mount(image, kPackBytes) ||
        complete.count() != 6 || complete.launchable_count() != 5) {
        fail("complete_verify", true); return false;
    }
    state_ = State::Active; error_ = "none"; return true;
}

void Transport::abort() {
    if (state_ == State::Receiving || state_ == State::Verified) {
        state_ = State::ReservedIncomplete;
    }
    received_ = next_sequence_ = 0; running_crc_ = 0xFFFFFFFFU;
}

}  // namespace cpcexplorer::migration
