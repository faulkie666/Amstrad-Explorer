#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::upload {

constexpr std::uint8_t kProtocolVersion = 1;
constexpr std::size_t kFrameHeaderBytes = 16;
constexpr std::size_t kMaximumChunkBytes = 1024;

enum class Command : std::uint8_t {
    Hello = 1,
    Begin = 2,
    Chunk = 3,
    Commit = 4,
    Abort = 5,
    Status = 6,
    Remove = 7,
    MigrationStatus = 8,
    MigrationLayout = 9,
    MigrationBegin = 10,
    MigrationPage = 11,
    MigrationVerify = 12,
    MigrationActivate = 13,
    MigrationAbort = 14,
    MigrationRead = 15,
};

struct Frame {
    Command command{};
    std::uint32_t sequence = 0;
    const std::uint8_t* payload = nullptr;
    std::uint16_t payload_length = 0;
};

bool decode_frame(const std::uint8_t* bytes, std::size_t size, Frame& result);

enum class DecodeResult { None, Ready, Invalid, Timeout };

class StreamDecoder {
public:
    static constexpr std::uint32_t frame_timeout_ms = 2000;
    DecodeResult feed(std::uint8_t byte, std::uint32_t now_ms, Frame& result);
    DecodeResult poll(std::uint32_t now_ms);
    void reset();

private:
    std::uint8_t bytes_[kFrameHeaderBytes + kMaximumChunkBytes]{};
    std::size_t count_ = 0;
    std::size_t expected_ = 0;
    std::uint32_t last_byte_ms_ = 0;
};

enum class Result {
    Ok,
    InvalidState,
    InvalidLength,
    InvalidSequence,
    IntegrityFailure,
    WriteFailure,
};

class Sink {
public:
    virtual ~Sink() = default;
    virtual bool begin(std::uint32_t package_length) = 0;
    virtual bool write(std::uint32_t offset, const std::uint8_t* data, std::size_t size) = 0;
    virtual bool commit() = 0;
    virtual void abort() = 0;
};

// Bounded-RAM transaction state. Flash publication belongs to Sink::commit(),
// which must update the inactive catalog slot only after payload verification.
class Session {
public:
    static constexpr std::uint32_t transaction_timeout_ms = 30000;
    Result begin(std::uint32_t package_length, std::uint32_t package_crc32, Sink& sink);
    Result chunk(std::uint32_t sequence, const std::uint8_t* data, std::size_t size, Sink& sink);
    Result commit(Sink& sink);
    void abort(Sink& sink);
    std::uint32_t received() const { return received_; }
    std::uint32_t expected_length() const { return expected_length_; }
    bool active() const { return active_; }
    bool timed_out(std::uint32_t now_ms, std::uint32_t last_activity_ms) const;

private:
    bool active_ = false;
    std::uint32_t expected_length_ = 0;
    std::uint32_t expected_crc32_ = 0;
    std::uint32_t received_ = 0;
    std::uint32_t next_sequence_ = 0;
    std::uint32_t running_crc_ = 0xffffffffU;
};

}  // namespace cpcexplorer::upload
