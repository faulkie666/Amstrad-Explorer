#include "packed_library.h"

#include "game_library.h"

#include <cstring>
#include <limits>

namespace cpcexplorer::packed {
namespace {

constexpr std::uint8_t kMagic[8] = {'C','P','X','P','C','K','2',0};
constexpr std::size_t kHeaderBytes = 128;
constexpr std::size_t kHeaderSlotBytes = 4096;
constexpr std::size_t kRecordBytes = 160;
constexpr std::uint16_t kVersion = 2;
constexpr std::uint8_t kCodec = 1;

std::uint16_t u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}
std::uint32_t u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}
bool add_ok(std::size_t a, std::size_t b, std::size_t limit) {
    return a <= limit && b <= limit - a;
}
bool terminated(const std::uint8_t* p, std::size_t length) {
    return std::memchr(p, 0, length) != nullptr;
}
bool overlap(const std::uint8_t* a, std::size_t an, const std::uint8_t* b, std::size_t bn) {
    const auto ap = reinterpret_cast<std::uintptr_t>(a);
    const auto bp = reinterpret_cast<std::uintptr_t>(b);
    if (an > std::numeric_limits<std::uintptr_t>::max() - ap ||
        bn > std::numeric_limits<std::uintptr_t>::max() - bp) return true;
    return ap < bp + bn && bp < ap + an;
}

struct Header {
    const std::uint8_t* records = nullptr;
    std::uint32_t count = 0;
    std::uint32_t generation = 0;
    std::uint32_t image_size = 0;
};

bool parse_header(const std::uint8_t* image, std::size_t available,
                  std::size_t slot, Header& result) {
    const std::size_t start = slot * kHeaderSlotBytes;
    if (!add_ok(start, kHeaderBytes, available)) return false;
    const auto* h = image + start;
    if (std::memcmp(h, kMagic, sizeof(kMagic)) != 0 || u16(h + 8) != kVersion ||
        u16(h + 10) != kHeaderBytes || u16(h + 12) != kRecordBytes ||
        u16(h + 14) != slot) return false;
    std::uint8_t checked[kHeaderBytes];
    std::memcpy(checked, h, sizeof(checked));
    const auto stored_header_crc = u32(checked + 48);
    std::memset(checked + 48, 0, 4);
    if (library::crc32(checked, sizeof(checked)) != stored_header_crc) return false;
    const auto count = u32(h + 20);
    const auto records_offset = u32(h + 24);
    const auto records_length = u32(h + 28);
    const auto data_offset = u32(h + 32);
    const auto image_size = u32(h + 36);
    if (count == 0 || count > 4096 || records_length != count * kRecordBytes ||
        image_size > available || image_size > kPackLimitFlashOffset - kPackFlashOffset ||
        !add_ok(records_offset, records_length, image_size) || data_offset > image_size ||
        records_offset < 2 * kHeaderSlotBytes || records_offset + records_length > data_offset)
        return false;
    if (library::crc32(image + records_offset, records_length) != u32(h + 40) ||
        library::crc32(image + data_offset, image_size - data_offset) != u32(h + 44))
        return false;
    result = {image + records_offset, count, u32(h + 16), image_size};
    return true;
}

bool lz4_decode(const std::uint8_t* src, std::size_t src_len,
                std::uint8_t* dst, std::size_t dst_len) {
    if (!src || !dst || overlap(src, src_len, dst, dst_len)) return false;
    const auto* ip = src;
    const auto* const end = src + src_len;
    auto* op = dst;
    auto* const out_end = dst + dst_len;
    while (ip < end) {
        const std::uint8_t token = *ip++;
        std::size_t literal = token >> 4;
        if (literal == 15) {
            std::uint8_t extension;
            do {
                if (ip >= end) return false;
                extension = *ip++;
                if (literal > std::numeric_limits<std::size_t>::max() - extension) return false;
                literal += extension;
            } while (extension == 255);
        }
        if (literal > static_cast<std::size_t>(end - ip) ||
            literal > static_cast<std::size_t>(out_end - op)) return false;
        std::memcpy(op, ip, literal); ip += literal; op += literal;
        if (ip == end) break;
        if (end - ip < 2) return false;
        const std::size_t offset = ip[0] | (static_cast<std::size_t>(ip[1]) << 8);
        ip += 2;
        if (offset == 0 || offset > static_cast<std::size_t>(op - dst)) return false;
        std::size_t match = token & 15;
        if (match == 15) {
            std::uint8_t extension;
            do {
                if (ip >= end) return false;
                extension = *ip++;
                if (match > std::numeric_limits<std::size_t>::max() - extension) return false;
                match += extension;
            } while (extension == 255);
        }
        if (match > std::numeric_limits<std::size_t>::max() - 4) return false;
        match += 4;
        if (match > static_cast<std::size_t>(out_end - op)) return false;
        const auto* source = op - offset;
        for (std::size_t i = 0; i < match; ++i) op[i] = source[i];
        op += match;
    }
    return op == out_end;
}

}  // namespace

void View::clear() { image_ = records_ = nullptr; image_size_ = count_ = generation_ = 0; }

bool View::mount(const std::uint8_t* bytes, std::size_t available) {
    clear();
    if (!bytes) return false;
    Header a{}, b{};
    const bool have_a = parse_header(bytes, available, 0, a);
    const bool have_b = parse_header(bytes, available, 1, b);
    if (!have_a && !have_b) return false;
    const Header* selected = !have_a ? &b : !have_b ? &a :
        (b.generation > a.generation ? &b : &a);
    if (have_a && have_b && a.generation == b.generation &&
        (a.count != b.count || a.image_size != b.image_size ||
         std::memcmp(a.records, b.records, a.count * kRecordBytes) != 0)) return false;
    image_ = bytes; records_ = selected->records; count_ = selected->count;
    generation_ = selected->generation; image_size_ = selected->image_size;
    Record previous{};
    for (std::size_t i = 0; i < count_; ++i) {
        Record current{};
        if (!record(i, current)) { clear(); return false; }
        if (i && (static_cast<unsigned>(current.category) < static_cast<unsigned>(previous.category) ||
                  (current.category == previous.category && current.letter < previous.letter) ||
                  (current.category == previous.category && current.letter == previous.letter &&
                   std::strcmp(current.title, previous.title) < 0))) { clear(); return false; }
        for (std::size_t j = 0; j < i; ++j) {
            Record other{};
            if (!record(j, other) || std::strcmp(current.id, other.id) == 0) {
                clear(); return false;
            }
        }
        previous = current;
    }
    return true;
}

bool View::record(std::size_t index, Record& out) const {
    if (!mounted() || index >= count_) return false;
    const auto* r = records_ + index * kRecordBytes;
    if (!terminated(r, 32) || !terminated(r + 32, 64)) return false;
    const auto category = r[96];
    const char letter = static_cast<char>(r[97]);
    const auto machine = r[98], ram = r[99], codec = r[100];
    const auto compatibility = r[101], control = r[102];
    const auto offset = u32(r + 104), compressed = u32(r + 108), raw = u32(r + 112);
    if ((category != 1 && category != 2) || letter < 'A' || letter > 'Z' ||
        machine != 0 || ram != 64 || codec != kCodec ||
        (compatibility != 1 && compatibility != 2) || control > 5 ||
        raw != kSnaHeaderBytes + kSnaRamBytes ||
        compressed <= kSnaHeaderBytes || (offset & 15) != 0 ||
        !add_ok(offset, compressed, image_size_)) return false;
    const auto* payload = image_ + offset;
    if (std::memcmp(payload, "MV - SNA", 8) != 0 ||
        (payload[0x10] != 1 && payload[0x10] != 2 && payload[0x10] != 3) ||
        payload[0x6B] != 64 || (payload[0x10] >= 2 && payload[0x6D] != 0)) return false;
    out = {reinterpret_cast<const char*>(r), reinterpret_cast<const char*>(r + 32),
           static_cast<Category>(category), letter, control, compatibility, payload, compressed,
           u32(r + 116), u32(r + 120)};
    return true;
}

std::size_t View::launchable_count() const {
    std::size_t found = 0;
    for (std::size_t i = 0; i < count_; ++i) {
        Record value{};
        if (record(i, value) && value.compatibility == 1) ++found;
    }
    return found;
}
bool View::launchable(std::size_t wanted, Record& out) const {
    for (std::size_t i = 0, found = 0; i < count_; ++i) {
        Record value{};
        if (record(i, value) && value.compatibility == 1 && found++ == wanted) {
            out = value; return true;
        }
    }
    return false;
}

std::size_t View::count(Category category, char letter) const {
    std::size_t found = 0;
    for (std::size_t i = 0; i < count_; ++i) {
        Record value{};
        if (record(i, value) && value.compatibility == 1 && value.category == category &&
            (!letter || value.letter == letter))
            ++found;
    }
    return found;
}
bool View::record(Category category, char letter, std::size_t wanted, Record& out) const {
    for (std::size_t i = 0, found = 0; i < count_; ++i) {
        Record value{};
        if (record(i, value) && value.compatibility == 1 &&
            value.category == category && value.letter == letter) {
            if (found++ == wanted) { out = value; return true; }
        }
    }
    return false;
}
bool View::has_letter(Category category, char letter) const { return count(category, letter) != 0; }

bool decode_snapshot(const Record& record, std::uint8_t* ram, std::size_t ram_size) {
    if (!record.payload || ram_size != kSnaRamBytes ||
        record.compressed_length <= kSnaHeaderBytes ||
        library::crc32(record.payload, record.compressed_length) != record.compressed_crc32)
        return false;
    if (!lz4_decode(record.payload + kSnaHeaderBytes,
                    record.compressed_length - kSnaHeaderBytes, ram, ram_size)) return false;
    // Match zlib's streaming CRC by using the public implementation over a
    // tiny virtual concatenation is not possible. Compute the complete CRC
    // incrementally using the same reflected polynomial.
    std::uint32_t crc = 0xFFFFFFFFU;
    const auto update = [&crc](const std::uint8_t* p, std::size_t n) {
        while (n--) {
            crc ^= *p++;
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    };
    update(record.payload, kSnaHeaderBytes); update(ram, ram_size);
    return (crc ^ 0xFFFFFFFFU) == record.raw_crc32;
}

}  // namespace cpcexplorer::packed
