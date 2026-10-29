#include "cpc_core.h"
#include "game_library.h"
#include "launcher.h"
#include "upload_protocol.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

void put16(std::uint8_t* output, std::uint16_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::uint8_t* output, std::uint32_t value) {
    for (int index = 0; index < 4; ++index)
        output[index] = static_cast<std::uint8_t>(value >> (index * 8));
}

void make_descriptor(std::uint8_t* descriptor, const char* id, const char* title,
                     std::uint32_t offset, const std::uint8_t* snapshot) {
    std::memset(descriptor, 0, cpcexplorer::library::kDescriptorBytes);
    std::memcpy(descriptor, id, std::min<std::size_t>(std::strlen(id), 15));
    std::memcpy(descriptor + 16, title, std::min<std::size_t>(std::strlen(title), 39));
    descriptor[56] = 1;
    descriptor[57] = 64;
    descriptor[58] = snapshot[16];
    put32(descriptor + 60, offset);
    put32(descriptor + 64, cpcexplorer::library::kSnapshotBytes);
    put32(descriptor + 68,
          cpcexplorer::library::crc32(snapshot, cpcexplorer::library::kSnapshotBytes));
    put32(descriptor + 72, cpcexplorer::library::crc32(descriptor, 80));
}

std::vector<std::uint8_t> make_snapshot() {
    cpcexplorer::init();
    cpcexplorer::run_us(500'000);
    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return {};
    return snapshot;
}

std::vector<std::uint8_t> make_package(const std::vector<std::uint8_t>& snapshot) {
    constexpr std::uint32_t payload_offset = 16 + cpcexplorer::library::kDescriptorBytes;
    std::vector<std::uint8_t> package(payload_offset + snapshot.size());
    std::memcpy(package.data(), "CPCGAME1", 8);
    put16(package.data() + 8, 1);
    put16(package.data() + 10, payload_offset);
    put32(package.data() + 12, static_cast<std::uint32_t>(package.size()));
    make_descriptor(package.data() + 16, "test_game", "Test Game", payload_offset,
                    snapshot.data());
    std::memcpy(package.data() + payload_offset, snapshot.data(), snapshot.size());
    return package;
}

class SimulatedSink final : public cpcexplorer::upload::Sink {
public:
    bool begin(std::uint32_t length) override {
        staged.assign(length, 0xff);
        active = true;
        return true;
    }
    bool write(std::uint32_t offset, const std::uint8_t* data, std::size_t size) override {
        if (!active || offset > staged.size() || size > staged.size() - offset) return false;
        std::copy(data, data + size, staged.begin() + offset);
        return true;
    }
    bool commit() override {
        cpcexplorer::library::GameInfo game{};
        if (!cpcexplorer::library::parse_package(staged.data(), staged.size(), game)) return false;
        active = false;
        published = true;
        return true;
    }
    void abort() override { active = false; }

    std::vector<std::uint8_t> staged;
    bool active = false;
    bool published = false;
};

}  // namespace

int main() {
    using namespace cpcexplorer;
    const auto snapshot = make_snapshot();
    if (!expect(snapshot.size() == library::kSnapshotBytes, "create host snapshot") ||
        !expect(library::is_supported_snapshot(snapshot.data(), snapshot.size()),
                "snapshot validation")) return 1;

    std::vector<std::uint8_t> flash(library::kLibraryBytes, 0xff);
    auto* header = flash.data();
    std::memset(header, 0, library::kCatalogHeaderBytes);
    std::memcpy(header, "CPCLIB1", 7);
    put16(header + 8, library::kFormatVersion);
    put16(header + 10, library::kCatalogHeaderBytes);
    put32(header + 12, 7);
    put16(header + 16, 3);
    put16(header + 18, library::kDescriptorBytes);
    put32(header + 20, library::kDataStart);
    put32(header + 24, library::kLibraryBytes);
    put32(header + 32, 0x4d4d4f43U);
    const std::uint32_t offset0 = library::kDataStart;
    constexpr std::uint32_t allocation = 17U * 4096U;
    const std::uint32_t offset1 = offset0 + allocation;
    const std::uint32_t offset2 = offset1 + allocation;
    std::memcpy(flash.data() + offset0, snapshot.data(), snapshot.size());
    std::memcpy(flash.data() + offset1, snapshot.data(), snapshot.size());
    std::memcpy(flash.data() + offset2, snapshot.data(), snapshot.size());
    make_descriptor(header + 64, "game_one", "Game One", offset0, flash.data() + offset0);
    make_descriptor(header + 144, "game_two", "Game Two", offset1, flash.data() + offset1);
    make_descriptor(header + 224, "game_bad", "Corrupted Game", offset2,
                    flash.data() + offset2);
    flash[offset2 + 300] ^= 0x80;  // payload no longer matches its descriptor checksum
    put32(header + 28, library::crc32(header, library::kCatalogHeaderBytes));

    library::View view;
    library::GameInfo game{};
    if (!expect(view.mount(flash.data(), flash.size()), "mount valid catalog") ||
        !expect(view.generation() == 7, "catalog generation") ||
        !expect(view.declared_count() == 3, "declared entries") ||
        !expect(view.valid_count() == 2, "corrupt entry skipped") ||
        !expect(view.game(1, game) && std::strcmp(game.title, "Game Two") == 0,
                "enumerate second valid game")) return 2;

    launcher::configure_library(flash.data(), flash.size());
    if (!expect(launcher::entry_count() == 4, "built-ins plus library games") ||
        !expect(std::strcmp(launcher::entry(0).display_name, "Locomotive BASIC") == 0,
                "BASIC remains first") ||
        !expect(std::strcmp(launcher::entry(1).display_name, "Knight Tyme") == 0,
                "Knight Tyme remains second") ||
        !expect(std::strcmp(launcher::entry(2).display_name, "Game One") == 0,
                "library follows built-ins")) return 3;

    launcher::MenuController menu;
    menu.reset(0, 0);
    std::uint32_t now = 10;
    for (int press = 0; press < 3; ++press) {
        menu.update(launcher::ButtonZ, now);
        menu.update(launcher::ButtonZ, now + 30);
        menu.update(0, now + 40);
        menu.update(0, now + 70);
        now += 80;
    }
    if (!expect(menu.selected() == 3, "navigate longer list") ||
        !expect(menu.first_visible(3) == 1, "scroll visible window")) return 4;

    cpcexplorer::reset();
    if (!expect(cpcexplorer::load_sna(game.data, game.data_length),
                "load library snapshot into emulator")) return 5;

    if (!expect(library::overlaps_firmware(0, 4096), "reject firmware overlap") ||
        !expect(library::overlaps_firmware(library::kFirmwareReserveBytes - 1, 2),
                "reject boundary overlap") ||
        !expect(!library::overlaps_firmware(library::kLibraryFlashOffset, 4096),
                "allow library region") ||
        !expect(library::overlaps_firmware(library::kFlashBytes - 100, 200),
                "reject flash overflow")) return 6;

    constexpr std::uint32_t final_allocation =
        library::kE10GuardFlashOffset - 17U * 4096U;
    if (!expect(library::kE10GuardFlashOffset == 0x00ff0000U, "E10 guard location") ||
        !expect(library::kLibraryFlashOffset + library::kLibraryBytes ==
                    library::kE10GuardFlashOffset,
                "library ends before guard") ||
        !expect(library::is_library_flash_range(library::kLibraryFlashOffset, 4096),
                "catalog range allowed") ||
        !expect(!library::is_library_flash_range(library::kE10GuardFlashOffset, 1),
                "guard start rejected") ||
        !expect(!library::is_library_flash_range(library::kE10GuardFlashOffset - 1, 2),
                "guard crossing rejected") ||
        !expect(!library::is_library_flash_range(0xfffffff0U, 0x40U),
                "integer overflow rejected") ||
        !expect(library::is_game_data_flash_range(final_allocation,
                                                   library::kSnapshotBytes),
                "last sector allocation allowed") ||
        !expect(!library::is_game_data_flash_range(final_allocation + 4096U,
                                                    17U * 4096U),
                "allocation reaching guard rejected") ||
        !expect(!library::is_game_data_flash_range(0xffffffffU, 2),
                "data offset overflow rejected") ||
        !expect(!library::is_game_data_flash_range(
                    library::kLibraryFlashOffset + library::kDataStart + 1,
                    library::kSnapshotBytes),
                "unaligned data offset rejected")) return 14;

    const auto package = make_package(snapshot);
    library::GameInfo packaged{};
    if (!expect(library::parse_package(package.data(), package.size(), packaged),
                "parse deterministic game package")) return 7;
    SimulatedSink sink;
    {
        upload::Session interrupted;
        if (!expect(interrupted.begin(static_cast<std::uint32_t>(package.size()),
                                      library::crc32(package.data(), package.size()), sink) ==
                        upload::Result::Ok,
                    "begin simulated upload") ||
            !expect(interrupted.chunk(0, package.data(), 512, sink) == upload::Result::Ok,
                    "write first upload chunk")) return 8;
        // Power loss: no commit call. The active catalog remains unpublished.
    }
    if (!expect(!sink.published, "interrupted upload is not published")) return 9;
    sink.abort();
    upload::Session recovered;
    if (recovered.begin(static_cast<std::uint32_t>(package.size()),
                        library::crc32(package.data(), package.size()), sink) != upload::Result::Ok)
        return 10;
    std::uint32_t sequence = 0;
    for (std::size_t offset = 0; offset < package.size(); offset += upload::kMaximumChunkBytes) {
        const std::size_t count = std::min(upload::kMaximumChunkBytes, package.size() - offset);
        if (recovered.chunk(sequence++, package.data() + offset, count, sink) != upload::Result::Ok)
            return 11;
    }
    if (!expect(recovered.commit(sink) == upload::Result::Ok, "recovered upload commit") ||
        !expect(sink.published, "verified package becomes visible")) return 12;

    std::vector<std::uint8_t> framed(16 + 3);
    std::memcpy(framed.data(), "CPCU", 4);
    framed[4] = 1;
    framed[5] = static_cast<std::uint8_t>(upload::Command::Chunk);
    put16(framed.data() + 6, 3);
    put32(framed.data() + 8, 4);
    framed[16] = 1; framed[17] = 2; framed[18] = 3;
    put32(framed.data() + 12, library::crc32(framed.data() + 16, 3));
    upload::Frame decoded{};
    if (!expect(upload::decode_frame(framed.data(), framed.size(), decoded) &&
                    decoded.sequence == 4 && decoded.payload_length == 3,
                "decode bounded USB frame")) return 13;
    framed[5] = static_cast<std::uint8_t>(upload::Command::Remove);
    if (!expect(upload::decode_frame(framed.data(), framed.size(), decoded) &&
                    decoded.command == upload::Command::Remove,
                "decode explicit removal command")) return 15;

    std::puts("PASS: library mount/enumeration/scroll/load/corruption/overlap/upload recovery");
    return 0;
}
