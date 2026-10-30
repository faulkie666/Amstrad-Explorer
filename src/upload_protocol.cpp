#include "upload_protocol.h"

#include "game_library.h"

#include <cstring>

namespace cpcexplorer::upload {
namespace {

std::uint16_t u16(const std::uint8_t* value) {
    return static_cast<std::uint16_t>(value[0] | (value[1] << 8));
}

std::uint32_t u32(const std::uint8_t* value) {
    return static_cast<std::uint32_t>(value[0]) |
           (static_cast<std::uint32_t>(value[1]) << 8) |
           (static_cast<std::uint32_t>(value[2]) << 16) |
           (static_cast<std::uint32_t>(value[3]) << 24);
}

void update_crc(std::uint32_t& crc, const std::uint8_t* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
}

}  // namespace

bool decode_frame(const std::uint8_t* bytes, std::size_t size, Frame& result) {
    static constexpr std::uint8_t magic[4] = {'C', 'P', 'C', 'U'};
    if (bytes == nullptr || size < kFrameHeaderBytes ||
        std::memcmp(bytes, magic, sizeof(magic)) != 0 ||
        bytes[4] != kProtocolVersion) return false;
    const std::uint16_t length = u16(bytes + 6);
    if (length > kMaximumChunkBytes || size != kFrameHeaderBytes + length ||
        library::crc32(bytes + kFrameHeaderBytes, length) != u32(bytes + 12)) return false;
    const auto command = static_cast<Command>(bytes[5]);
    if (command < Command::Hello || command > Command::MigrationRead) return false;
    result = {command, u32(bytes + 8), bytes + kFrameHeaderBytes, length};
    return true;
}

void StreamDecoder::reset() {
    count_ = 0;
    expected_ = 0;
}

DecodeResult StreamDecoder::feed(std::uint8_t byte, std::uint32_t now_ms, Frame& result) {
    static constexpr std::uint8_t magic[4] = {'C', 'P', 'C', 'U'};
    last_byte_ms_ = now_ms;
    if (count_ < 4 && byte != magic[count_]) {
        reset();
        if (byte == magic[0]) bytes_[count_++] = byte;
        return DecodeResult::None;
    }
    if (count_ >= sizeof(bytes_)) {
        reset();
        return DecodeResult::Invalid;
    }
    bytes_[count_++] = byte;
    if (count_ == kFrameHeaderBytes) {
        const std::size_t payload = u16(bytes_ + 6);
        if (payload > kMaximumChunkBytes) {
            reset();
            return DecodeResult::Invalid;
        }
        expected_ = kFrameHeaderBytes + payload;
    }
    if (expected_ != 0 && count_ == expected_) {
        const bool valid = decode_frame(bytes_, count_, result);
        reset();
        return valid ? DecodeResult::Ready : DecodeResult::Invalid;
    }
    return DecodeResult::None;
}

DecodeResult StreamDecoder::poll(std::uint32_t now_ms) {
    if (count_ != 0 && static_cast<std::uint32_t>(now_ms - last_byte_ms_) >= frame_timeout_ms) {
        reset();
        return DecodeResult::Timeout;
    }
    return DecodeResult::None;
}

Result Session::begin(std::uint32_t package_length, std::uint32_t package_crc32, Sink& sink) {
    if (active_) return Result::InvalidState;
    if (package_length == 0 || package_length > library::kLibraryBytes - library::kDataStart)
        return Result::InvalidLength;
    if (!sink.begin(package_length)) return Result::WriteFailure;
    active_ = true;
    expected_length_ = package_length;
    expected_crc32_ = package_crc32;
    received_ = 0;
    next_sequence_ = 0;
    running_crc_ = 0xffffffffU;
    return Result::Ok;
}

Result Session::chunk(std::uint32_t sequence, const std::uint8_t* data, std::size_t size,
                      Sink& sink) {
    if (!active_) return Result::InvalidState;
    if (sequence != next_sequence_) return Result::InvalidSequence;
    if (data == nullptr || size == 0 || size > kMaximumChunkBytes ||
        size > expected_length_ - received_) return Result::InvalidLength;
    if (!sink.write(received_, data, size)) return Result::WriteFailure;
    update_crc(running_crc_, data, size);
    received_ += static_cast<std::uint32_t>(size);
    ++next_sequence_;
    return Result::Ok;
}

Result Session::commit(Sink& sink) {
    if (!active_) return Result::InvalidState;
    if (received_ != expected_length_ || ~running_crc_ != expected_crc32_) {
        sink.abort();
        active_ = false;
        return Result::IntegrityFailure;
    }
    if (!sink.commit()) return Result::WriteFailure;
    active_ = false;
    return Result::Ok;
}

void Session::abort(Sink& sink) {
    if (active_) sink.abort();
    active_ = false;
}

bool Session::timed_out(std::uint32_t now_ms, std::uint32_t last_activity_ms) const {
    return active_ && static_cast<std::uint32_t>(now_ms - last_activity_ms) >=
                          transaction_timeout_ms;
}

}  // namespace cpcexplorer::upload
