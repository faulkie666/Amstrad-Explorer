#pragma once

#include <cstddef>
#include <cstdint>

namespace cpcexplorer::packed {

constexpr std::uint32_t kPackFlashOffset = 0x43B000;
constexpr std::uint32_t kPackLimitFlashOffset = 0xFF0000;
constexpr std::size_t kSnaHeaderBytes = 256;
constexpr std::size_t kSnaRamBytes = 64 * 1024;

enum class Category : std::uint8_t { Arcade = 1, Simulation = 2 };

struct Record {
    const char* id = nullptr;
    const char* title = nullptr;
    Category category = Category::Arcade;
    char letter = 'A';
    std::uint8_t control_profile = 0;
    std::uint8_t compatibility = 0;
    const std::uint8_t* payload = nullptr;
    std::uint32_t compressed_length = 0;
    std::uint32_t raw_crc32 = 0;
    std::uint32_t compressed_crc32 = 0;
};

class View {
public:
    bool mount(const std::uint8_t* bytes, std::size_t available);
    void clear();
    bool mounted() const { return image_ != nullptr; }
    std::size_t count() const { return count_; }
    bool record(std::size_t index, Record& result) const;
    std::size_t launchable_count() const;
    bool launchable(std::size_t index, Record& result) const;
    std::size_t count(Category category, char letter = 0) const;
    bool record(Category category, char letter, std::size_t index, Record& result) const;
    bool has_letter(Category category, char letter) const;
    std::uint32_t generation() const { return generation_; }
    std::size_t image_bytes() const { return image_size_; }

private:
    const std::uint8_t* image_ = nullptr;
    const std::uint8_t* records_ = nullptr;
    std::size_t image_size_ = 0;
    std::uint32_t count_ = 0;
    std::uint32_t generation_ = 0;
};

// Decode the split SNA directly into the existing emulator RAM. No history or
// temporary output buffer is allocated. The 256-byte header remains in XIP.
bool decode_snapshot(const Record& record, std::uint8_t* ram, std::size_t ram_size);

}  // namespace cpcexplorer::packed
