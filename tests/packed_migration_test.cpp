#include "game_library.h"
#include "launcher.h"
#include "library_installer.h"
#include "packed_library.h"
#include "packed_migration.h"
#include "upload_protocol.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
using cpcexplorer::library::crc32;
void p16(std::uint8_t* p, std::uint16_t v) { p[0] = v; p[1] = v >> 8; }
void p32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
bool expect(bool value, const char* message) {
    if (!value) std::fprintf(stderr, "FAIL: %s\n", message);
    return value;
}
std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}
std::vector<std::uint8_t> protocol_frame(cpcexplorer::upload::Command command,
                                         std::uint32_t sequence,
                                         const std::uint8_t* payload,
                                         std::uint16_t length) {
    std::vector<std::uint8_t> result(cpcexplorer::upload::kFrameHeaderBytes + length);
    std::memcpy(result.data(), "CPCU", 4);
    result[4] = cpcexplorer::upload::kProtocolVersion;
    result[5] = static_cast<std::uint8_t>(command);
    p16(result.data() + 6, length);
    p32(result.data() + 8, sequence);
    p32(result.data() + 12, crc32(payload, length));
    if (length) std::memcpy(result.data() + cpcexplorer::upload::kFrameHeaderBytes, payload, length);
    return result;
}
std::vector<std::uint8_t> package(const char* id, std::uint8_t seed,
                                  const char* title = nullptr,
                                  std::uint8_t control_profile = 0) {
    std::vector<std::uint8_t> sna(cpcexplorer::library::kSnapshotBytes);
    std::memcpy(sna.data(), "MV - SNA", 8); sna[16] = 3; sna[107] = 64;
    for (std::size_t i = 256; i < sna.size(); ++i) sna[i] = static_cast<std::uint8_t>(i + seed);
    std::vector<std::uint8_t> out(96 + sna.size());
    std::memcpy(out.data(), "CPCGAME1", 8); p16(out.data() + 8, 1); p16(out.data() + 10, 96);
    p32(out.data() + 12, static_cast<std::uint32_t>(out.size()));
    auto* d = out.data() + 16; std::memcpy(d, id, std::strlen(id));
    const char* display = title == nullptr ? id : title;
    std::memcpy(d + 16, display, std::strlen(display));
    d[56] = 1; d[57] = 64; d[58] = 3; d[59] = control_profile;
    p32(d + 60, 96); p32(d + 64, static_cast<std::uint32_t>(sna.size()));
    p32(d + 68, crc32(sna.data(), sna.size())); p32(d + 72, crc32(d, 80));
    std::memcpy(out.data() + 96, sna.data(), sna.size()); return out;
}

class Flash final : public cpcexplorer::storage::Flash {
public:
    Flash() : bytes(cpcexplorer::library::kFlashBytes, 0xFF) {
        std::fill(bytes.begin(), bytes.begin() + cpcexplorer::library::kFirmwareReserveBytes, 0xA5);
        std::fill(bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset, bytes.end(), 0x5A);
    }
    bool ready() const override { return supported; }
    const char* identity() const override { return identity_value; }
    std::uint32_t capacity() const override { return capacity_value; }
    const std::uint8_t* mapped_library() const override {
        return bytes.data() + cpcexplorer::library::kLibraryFlashOffset;
    }
    bool erase(std::uint32_t offset, std::size_t size) override {
        if (!allowed(offset, size, 4096) || fail()) return false;
        std::fill(bytes.begin() + offset, bytes.begin() + offset + size, 0xFF); return true;
    }
    bool program(std::uint32_t offset, const std::uint8_t* data, std::size_t size) override {
        if (!data || !allowed(offset, size, 256) || fail()) return false;
        for (std::size_t i = 0; i < size; ++i) if ((bytes[offset + i] & data[i]) != data[i]) return false;
        for (std::size_t i = 0; i < size; ++i) bytes[offset + i] &= data[i];
        if (corrupt_at && operations == corrupt_at) bytes[offset + size / 2] ^= 1;
        return true;
    }
    void fail_next(std::size_t relative = 1) { fail_at = operations + relative; }
    void corrupt_next(std::size_t relative = 1) { corrupt_at = operations + relative; }
    bool protected_equal(const Flash& other) const {
        return std::equal(bytes.begin(), bytes.begin() + 0x400000, other.bytes.begin()) &&
               std::equal(bytes.begin() + 0x408000, bytes.begin() + 0x43B000,
                          other.bytes.begin() + 0x408000) &&
               std::equal(bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset, bytes.end(),
                          other.bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset);
    }
    bool all_old_equal(const Flash& other) const {
        return std::equal(bytes.begin(), bytes.begin() + 0x43B000, other.bytes.begin()) &&
               std::equal(bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset, bytes.end(),
                          other.bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset);
    }
    std::vector<std::uint8_t> bytes;
    bool supported = true;
    const char* identity_value = "EF-40-18";
    std::uint32_t capacity_value = cpcexplorer::library::kFlashBytes;
    std::size_t operations = 0, fail_at = 0, corrupt_at = 0;
private:
    bool allowed(std::uint32_t offset, std::size_t size, std::uint32_t alignment) const {
        return size && size <= UINT32_MAX && !(offset & (alignment - 1)) &&
               !(size & (alignment - 1)) &&
               cpcexplorer::library::is_library_flash_range(offset, static_cast<std::uint32_t>(size));
    }
    bool fail() { ++operations; return fail_at && operations == fail_at; }
};

bool install(Flash& flash, const std::vector<std::uint8_t>& bytes,
             cpcexplorer::storage::Installer::ReservedIdPredicate reserved = nullptr) {
    cpcexplorer::storage::Installer installer(flash); cpcexplorer::upload::Session session;
    installer.set_reserved_id_predicate(reserved);
    if (session.begin(bytes.size(), crc32(bytes.data(), bytes.size()), installer) !=
        cpcexplorer::upload::Result::Ok) return false;
    std::uint32_t sequence = 0;
    for (std::size_t at = 0; at < bytes.size(); at += 1024) {
        const auto count = std::min<std::size_t>(1024, bytes.size() - at);
        if (session.chunk(sequence++, bytes.data() + at, count, installer) !=
            cpcexplorer::upload::Result::Ok) return false;
    }
    return session.commit(installer) == cpcexplorer::upload::Result::Ok;
}

Flash baseline() {
    Flash flash;
    install(flash, package("manic_miner", 2));
    install(flash, package("dizzy", 3));
    return flash;
}

bool duplicate_second_descriptor_id(Flash& flash) {
    cpcexplorer::library::View view;
    if (!view.mount(flash.mapped_library(), cpcexplorer::library::kLibraryBytes) ||
        view.declared_count() != 2) return false;
    auto* catalog = flash.bytes.data() + cpcexplorer::library::kLibraryFlashOffset +
                    view.catalog_offset();
    auto* descriptor = catalog + cpcexplorer::library::kCatalogHeaderBytes +
                       cpcexplorer::library::kDescriptorBytes;
    std::memset(descriptor, 0, 16);
    std::memcpy(descriptor, "manic_miner", 11);
    p32(descriptor + 72, 0);
    p32(descriptor + 72, crc32(descriptor, cpcexplorer::library::kDescriptorBytes));
    cpcexplorer::library::View duplicate;
    return duplicate.mount(flash.mapped_library(), cpcexplorer::library::kLibraryBytes) &&
           duplicate.declared_count() == 2 && duplicate.valid_count() == 1;
}

cpcexplorer::migration::Manifest manifest() {
    cpcexplorer::migration::Manifest value{1, cpcexplorer::migration::kPackOffset,
        cpcexplorer::migration::kPackBytes, cpcexplorer::migration::kPackCrc32, {}};
    std::memcpy(value.image_sha256, cpcexplorer::migration::Transport::expected_sha256, 32);
    return value;
}

class MockProtocol {
public:
    explicit MockProtocol(cpcexplorer::migration::Transport& transport) : transport_(transport) {}
    bool send(cpcexplorer::upload::Command command, std::uint32_t sequence = 0,
              const std::uint8_t* payload = nullptr, std::uint16_t length = 0) {
        const auto bytes = protocol_frame(command, sequence, payload, length);
        cpcexplorer::upload::Frame frame{};
        if (!cpcexplorer::upload::decode_frame(bytes.data(), bytes.size(), frame)) return false;
        using cpcexplorer::upload::Command;
        if (frame.command == Command::Hello) { handshake_ = frame.payload_length == 0; return handshake_; }
        if (frame.command == Command::MigrationStatus || frame.command == Command::MigrationLayout)
            return frame.payload_length == 0;
        if (frame.command == Command::MigrationBegin) {
            if (!handshake_ || frame.payload_length != 56 ||
                std::memcmp(frame.payload, "MIGRATE\0", 8) != 0) return false;
            cpcexplorer::migration::Manifest proposed{};
            std::memcpy(&proposed.protocol_version, frame.payload + 8, 4);
            std::memcpy(&proposed.flash_offset, frame.payload + 12, 4);
            std::memcpy(&proposed.image_bytes, frame.payload + 16, 4);
            std::memcpy(&proposed.image_crc32, frame.payload + 20, 4);
            std::memcpy(proposed.image_sha256, frame.payload + 24, 32);
            handshake_ = false; return transport_.begin(proposed, true);
        }
        if (frame.command == Command::MigrationPage)
            return transport_.write_page(frame.sequence, frame.payload, frame.payload_length);
        if (frame.command == Command::MigrationVerify)
            return frame.payload_length == 0 && transport_.verify();
        if (frame.command == Command::MigrationActivate)
            return frame.payload_length == 0 && transport_.activate();
        if (frame.command == Command::MigrationAbort) {
            if (frame.payload_length != 0) return false;
            transport_.abort(); return true;
        }
        if (frame.command == Command::MigrationRead) {
            std::uint32_t offset = 0, count = 0;
            if (frame.payload_length != 8) return false;
            std::memcpy(&offset, frame.payload, 4); std::memcpy(&count, frame.payload + 4, 4);
            return cpcexplorer::migration::is_read_range(offset, count);
        }
        return false;
    }
private:
    cpcexplorer::migration::Transport& transport_;
    bool handshake_ = false;
};

bool transfer(cpcexplorer::migration::Transport& migration, const std::vector<std::uint8_t>& pack) {
    std::uint32_t sequence = 0;
    for (std::size_t at = 0; at < pack.size(); at += 256) {
        const auto count = std::min<std::size_t>(256, pack.size() - at);
        if (!migration.write_page(sequence++, pack.data() + at, count)) return false;
    }
    return true;
}

std::size_t legacy_entries(const Flash& flash) {
    cpcexplorer::library::View view;
    return view.mount(flash.mapped_library(), cpcexplorer::library::kLibraryBytes) ? view.valid_count() : 0;
}
bool packed_entries(const Flash& flash, std::size_t expected) {
    cpcexplorer::packed::View view;
    if (!view.mount(flash.bytes.data() + cpcexplorer::migration::kPackOffset,
                    cpcexplorer::migration::kPackBytes)) return expected == 0;
    if (view.launchable_count() != expected) return false;
    static constexpr const char* ids[] = {
        "columns", "dizzy", "knight_tyme", "manic_miner", "thrust"
    };
    for (std::size_t i = 0; i < expected; ++i) {
        cpcexplorer::packed::Record record{};
        if (!view.launchable(i, record) || std::strcmp(record.id, ids[i]) != 0) return false;
    }
    return true;
}
}

int main() {
    using namespace cpcexplorer;
    const auto pack = read_file(CPCEXPLORER_STAGE4I8_PACK);
    if (!expect(pack.size() == migration::kPackBytes &&
                crc32(pack.data(), pack.size()) == migration::kPackCrc32, "pack fixture")) return 1;
    if (!expect(migration::is_read_range(0x400000, 128) &&
                migration::is_read_range(0x466F80, 128) &&
                !migration::is_read_range(0x3FFFFF, 1) &&
                !migration::is_read_range(0x467000, 1) &&
                !migration::is_read_range(0xFFFFFFF0, 128),
                "readback is bounded to catalogues, legacy allocations and pack")) return 1;
    upload::Frame decoded{};
    auto encoded = protocol_frame(upload::Command::MigrationPage, 7, pack.data(), 256);
    if (!expect(upload::decode_frame(encoded.data(), encoded.size(), decoded) &&
                decoded.command == upload::Command::MigrationPage && decoded.sequence == 7 &&
                decoded.payload_length == 256, "migration protocol frame decodes")) return 1;
    auto corrupt_frame = encoded; corrupt_frame.back() ^= 1;
    if (!expect(!upload::decode_frame(corrupt_frame.data(), corrupt_frame.size(), decoded),
                "corrupt migration packet rejected")) return 1;
    auto wrong_version = encoded; wrong_version[4] = 2;
    if (!expect(!upload::decode_frame(wrong_version.data(), wrong_version.size(), decoded),
                "unknown migration protocol version rejected")) return 1;
    auto unknown_command = encoded; unknown_command[5] = 16;
    if (!expect(!upload::decode_frame(unknown_command.data(), unknown_command.size(), decoded),
                "unknown migration command rejected")) return 1;
    const Flash original = baseline();
    if (!expect(legacy_entries(original) == 2 && packed_entries(original, 0),
                "state A: physical catalogue has Manic Miner and Dizzy descriptors")) return 2;
    if (!expect(launcher::has_builtin_snapshot_id("knight_tyme") &&
                !launcher::has_builtin_snapshot_id("manic_miner") &&
                !launcher::has_builtin_snapshot_id("Knight Tyme"),
                "built-in Knight Tyme is validated by canonical snapshot ID")) return 2;

    {
        Flash rejected = original; const Flash before = rejected;
        storage::Installer ri(rejected);
        migration::Transport rt(rejected, ri, ri.scratch_buffer(), ri.scratch_bytes());
        rt.recover(); const auto operations = rejected.operations;
        if (!expect(!rt.begin(manifest(), false) &&
                    std::strcmp(rt.error(), "builtin_knight_tyme") == 0,
                    "missing authoritative built-in Knight Tyme fails closed") ||
            !expect(rejected.operations == operations && rejected.all_old_equal(before),
                    "built-in identity failure performs no flash operation")) return 2;
    }
    {
        Flash rejected; install(rejected, package("manic_miner", 2));
        const Flash before = rejected; storage::Installer ri(rejected);
        migration::Transport rt(rejected, ri, ri.scratch_buffer(), ri.scratch_bytes());
        rt.recover(); const auto operations = rejected.operations;
        if (!expect(!rt.begin(manifest(), true), "missing Dizzy descriptor rejected") ||
            !expect(rejected.operations == operations && rejected.all_old_equal(before),
                    "missing descriptor rejection performs no flash operation")) return 2;
    }
    {
        Flash rejected; install(rejected, package("manic_miner", 2));
        install(rejected, package("unexpected_game", 3));
        const Flash before = rejected; storage::Installer ri(rejected);
        migration::Transport rt(rejected, ri, ri.scratch_buffer(), ri.scratch_bytes());
        rt.recover(); const auto operations = rejected.operations;
        if (!expect(!rt.begin(manifest(), true), "conflicting descriptor identity rejected") ||
            !expect(rejected.operations == operations && rejected.all_old_equal(before),
                    "conflicting identity rejection performs no flash operation")) return 2;
    }
    {
        Flash rejected = original;
        if (!expect(duplicate_second_descriptor_id(rejected),
                    "construct valid duplicate-ID catalogue")) return 2;
        const Flash before = rejected; storage::Installer ri(rejected);
        migration::Transport rt(rejected, ri, ri.scratch_buffer(), ri.scratch_bytes());
        rt.recover(); const auto operations = rejected.operations;
        if (!expect(!rt.begin(manifest(), true), "duplicate descriptor identity rejected") ||
            !expect(rejected.operations == operations && rejected.all_old_equal(before),
                    "duplicate identity rejection performs no flash operation")) return 2;
    }
    {
        Flash rejected;
        install(rejected, package("knight_tyme", 1));
        install(rejected, package("manic_miner", 2));
        install(rejected, package("dizzy", 3));
        const Flash before = rejected; storage::Installer ri(rejected);
        migration::Transport rt(rejected, ri, ri.scratch_buffer(), ri.scratch_bytes());
        rt.recover(); const auto operations = rejected.operations;
        if (!expect(!rt.begin(manifest(), true),
                    "fabricated Knight Tyme flash descriptor is rejected") ||
            !expect(rejected.operations == operations && rejected.all_old_equal(before),
                    "three-descriptor rejection performs no flash operation")) return 2;
    }

    Flash bootstrap = original; storage::Installer bootstrap_installer(bootstrap);
    migration::Transport bootstrap_transport(
        bootstrap, bootstrap_installer, bootstrap_installer.scratch_buffer(),
        bootstrap_installer.scratch_bytes());
    if (!expect(bootstrap_transport.recover() && bootstrap_transport.state() == migration::State::Legacy &&
                legacy_entries(bootstrap) == 2 && packed_entries(bootstrap, 0),
                "state B: firmware without pack falls back to two legacy descriptors")) return 2;

    Flash partial = original; storage::Installer partial_installer(partial);
    migration::Transport partial_transport(
        partial, partial_installer, partial_installer.scratch_buffer(), partial_installer.scratch_bytes());
    partial_transport.recover();
    if (!expect(partial_transport.begin(manifest(), true), "state C begin") ) return 2;
    for (std::uint32_t sequence = 0; sequence < 64; ++sequence)
        if (!expect(partial_transport.write_page(sequence, pack.data() + sequence * 256, 256),
                    "state C partial page")) return 2;
    storage::Installer partial_reboot_installer(partial);
    migration::Transport partial_reboot(
        partial, partial_reboot_installer, partial_reboot_installer.scratch_buffer(),
        partial_reboot_installer.scratch_bytes());
    if (!expect(partial_reboot.recover() &&
                partial_reboot.state() == migration::State::ReservedIncomplete &&
                legacy_entries(partial) == 2 && packed_entries(partial, 0),
                "state C: partial pack cannot displace the old catalogue")) return 2;

    Flash verified = original; storage::Installer verified_installer(verified);
    migration::Transport verified_transport(
        verified, verified_installer, verified_installer.scratch_buffer(),
        verified_installer.scratch_bytes());
    verified_transport.recover();
    if (!expect(verified_transport.begin(manifest(), true) && transfer(verified_transport, pack) &&
                verified_transport.verify() && verified_transport.state() == migration::State::Verified &&
                legacy_entries(verified) == 2 && packed_entries(verified, 0),
                "state D: verified but unpublished pack leaves legacy authoritative")) return 2;

    Flash framed = original; storage::Installer framed_installer(framed);
    migration::Transport framed_transport(
        framed, framed_installer, framed_installer.scratch_buffer(), framed_installer.scratch_bytes());
    framed_transport.recover(); MockProtocol wire(framed_transport);
    std::array<std::uint8_t, 56> begin_payload{};
    std::memcpy(begin_payload.data(), "MIGRATE\0", 8);
    const auto exact = manifest();
    p32(begin_payload.data() + 8, exact.protocol_version);
    p32(begin_payload.data() + 12, exact.flash_offset);
    p32(begin_payload.data() + 16, exact.image_bytes);
    p32(begin_payload.data() + 20, exact.image_crc32);
    std::memcpy(begin_payload.data() + 24, exact.image_sha256, 32);
    if (!expect(wire.send(upload::Command::Hello), "mock protocol hello") ||
        !expect(wire.send(upload::Command::MigrationBegin, 0, begin_payload.data(),
                          static_cast<std::uint16_t>(begin_payload.size())), "mock protocol begin")) return 2;
    std::uint32_t framed_sequence = 0;
    for (std::size_t at = 0; at < pack.size(); at += 256) {
        const auto count = static_cast<std::uint16_t>(std::min<std::size_t>(256, pack.size() - at));
        if (!expect(wire.send(upload::Command::MigrationPage, framed_sequence++, pack.data() + at, count),
                    "mock protocol page")) return 2;
    }
    if (!expect(wire.send(upload::Command::MigrationVerify), "mock protocol verify") ||
        !expect(wire.send(upload::Command::MigrationActivate), "mock protocol activate") ||
        !expect(framed_transport.state() == migration::State::Active && packed_entries(framed, 5),
                "state E: activation exposes BASIC plus five validated packed games") ||
        !expect(legacy_entries(framed) == 2 &&
                framed_installer.packed_region_reserved(migration::kSavedHighWater),
                "state F: rollback retains the two legacy flash descriptors")) return 2;
    launcher::configure_library(framed.mapped_library(), library::kLibraryBytes);
    if (!expect(launcher::configure_packed_library(
                    framed.bytes.data() + migration::kPackOffset, migration::kPackBytes) &&
                launcher::entry_count() == 6,
                "active launcher exposes exactly BASIC plus five packed games")) return 2;
    static constexpr const char* required_ids[] = {
        "knight_tyme", "manic_miner", "dizzy", "columns", "thrust"
    };
    std::array<bool, std::size(required_ids)> found{};
    std::size_t knight_count = 0;
    for (std::size_t i = 0; i < launcher::entry_count(); ++i) {
        const auto visible = launcher::entry(i);
        if (i == 0 && !expect(visible.type == launcher::EntryType::Basic,
                              "Locomotive BASIC remains first")) return 2;
        for (std::size_t id = 0; id < std::size(required_ids); ++id)
            found[id] = found[id] || (visible.snapshot_id != nullptr &&
                                      std::strcmp(visible.snapshot_id, required_ids[id]) == 0);
        knight_count += visible.snapshot_id != nullptr &&
                        std::strcmp(visible.snapshot_id, "knight_tyme") == 0;
        if (!expect(visible.snapshot_id == nullptr ||
                    std::strcmp(visible.snapshot_id, "fruity_frank") != 0,
                    "Fruity Frank remains hidden")) return 2;
    }
    if (!expect(std::all_of(found.begin(), found.end(), [](bool value) { return value; }) &&
                knight_count == 1,
                "required games are visible and Knight Tyme appears exactly once")) return 2;

    {
        launcher::MenuController navigation;
        navigation.reset(0, 0);
        std::uint32_t time_ms = 0;
        const auto press_button = [&navigation, &time_ms](launcher::ButtonMask button) {
            navigation.update(button, time_ms += 10);
            navigation.update(button, time_ms += 30);
            navigation.update(0, time_ms += 10);
            navigation.update(0, time_ms += 30);
        };
        press_button(launcher::ButtonZ); press_button(launcher::ButtonY);  // Arcade
        press_button(launcher::ButtonZ); press_button(launcher::ButtonY);  // C games
        std::size_t observed = 0;
        const auto started = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < 200; ++iteration) {
            press_button(launcher::ButtonZ);
            const auto first = navigation.first_visible(5);
            for (std::size_t row = 0; row < 5; ++row) {
                const auto index = first + row;
                if (index >= navigation.visible_count()) break;
                observed += std::strlen(navigation.label(index));
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::printf("NAVIGATION_BENCHMARK iterations=200 elapsed_us=%lld observed=%zu\n",
                    static_cast<long long>(elapsed), observed);
        if (!expect(observed != 0, "navigation benchmark exercises visible labels")) return 2;
        press_button(launcher::ButtonY);
        if (!expect(navigation.visible_count() > 1 &&
                    std::strcmp(navigation.label(0), "< Back") == 0,
                    "leave game list for Arcade A-Z")) return 2;
        press_button(launcher::ButtonY);
        if (!expect(navigation.visible_count() == 3,
                    "leave Arcade for top-level categories")) return 2;
        press_button(launcher::ButtonZ); press_button(launcher::ButtonZ);
        press_button(launcher::ButtonY);
        if (!expect(navigation.visible_count() >= 1 &&
                    std::strcmp(navigation.label(0), "< Back") == 0,
                    "move from Arcade to Simulation hierarchy")) return 2;
    }

    if (!expect(launcher::has_protected_snapshot_id("knight_tyme") &&
                launcher::has_protected_snapshot_id("columns") &&
                launcher::has_protected_snapshot_id("fruity_frank") &&
                !launcher::has_protected_snapshot_id("ktris"),
                "built-in, launchable and hidden packed IDs are protected")) return 2;

    const std::size_t duplicate_operations = framed.operations;
    if (!expect(!install(framed, package("columns", 7),
                         launcher::has_protected_snapshot_id),
                "upload colliding with packed ID is rejected") ||
        !expect(framed.operations == duplicate_operations,
                "packed-ID rejection occurs before erase/program")) return 2;

    const auto invalid_profile = package("invalid_profile", 8, "Invalid Profile",
                                         library::kMaximumControlProfile + 1);
    const std::size_t invalid_operations = framed.operations;
    if (!expect(!install(framed, invalid_profile, launcher::has_protected_snapshot_id),
                "unknown control profile is rejected") ||
        !expect(framed.operations == invalid_operations,
                "invalid profile rejection occurs before erase/program")) return 2;

    const auto ktris = package("ktris", 9, "Ktris", 6);
    if (!expect(install(framed, ktris, launcher::has_protected_snapshot_id),
                "Ktris installs transactionally after active packed region")) return 2;
    launcher::configure_library(framed.mapped_library(), library::kLibraryBytes);
    if (!expect(launcher::entry_count() == 7, "Ktris is added without duplicating packed games") ||
        !expect(std::strcmp(launcher::entry(6).snapshot_id, "ktris") == 0 &&
                !launcher::entry(6).packed && launcher::entry(6).snapshot_data != nullptr &&
                launcher::entry(6).snapshot_size == library::kSnapshotBytes &&
                launcher::entry(6).control_profile == input::Profile::CursorQaop,
                "Ktris uses the legacy snapshot path and metadata control profile")) return 2;
    library::GameInfo ktris_info{};
    library::View merged_catalog;
    if (!expect(merged_catalog.mount(framed.mapped_library(), library::kLibraryBytes),
                "merged CPCLIB1 catalogue mounts")) return 2;
    for (std::size_t index = 0; index < merged_catalog.valid_count(); ++index) {
        library::GameInfo candidate{};
        if (merged_catalog.game(index, candidate) &&
            std::strcmp(candidate.game_id, "ktris") == 0) ktris_info = candidate;
    }
    if (!expect(ktris_info.game_id != nullptr &&
                ktris_info.data_offset >= migration::kSavedHighWater,
                "Ktris allocation begins beyond the packed reservation") ||
        !expect(library::is_game_data_flash_range(
                    library::kLibraryFlashOffset + ktris_info.data_offset,
                    17U * 4096U),
                "Ktris allocation remains outside firmware and E10 guard")) return 2;

    storage::Installer incremental_reboot_installer(framed);
    migration::Transport incremental_reboot(
        framed, incremental_reboot_installer, incremental_reboot_installer.scratch_buffer(),
        incremental_reboot_installer.scratch_bytes());
    if (!expect(incremental_reboot.recover() &&
                    incremental_reboot.state() == migration::State::Active,
                "active packed migration survives reboot after incremental upload")) return 2;

    launcher::MenuController overlay_menu;
    overlay_menu.reset(0, 0);
    const auto press = [&overlay_menu](launcher::ButtonMask button, std::uint32_t& now) {
        overlay_menu.update(button, now += 10);
        const auto result = overlay_menu.update(button, now += 30);
        overlay_menu.update(0, now += 10);
        overlay_menu.update(0, now += 30);
        return result;
    };
    std::uint32_t now = 0;
    press(launcher::ButtonZ, now); press(launcher::ButtonY, now);
    if (!expect(overlay_menu.visible_count() == 6, "Arcade A-Z hierarchy is preserved")) return 2;
    press(launcher::ButtonZ, now); press(launcher::ButtonZ, now); press(launcher::ButtonZ, now);
    if (!expect(std::strcmp(overlay_menu.label(overlay_menu.selected()), "K") == 0,
                "Ktris shares the existing K letter")) return 2;
    press(launcher::ButtonY, now);
    if (!expect(overlay_menu.visible_count() == 3 &&
                std::strcmp(overlay_menu.label(1), "Knight Tyme") == 0 &&
                std::strcmp(overlay_menu.label(2), "Ktris") == 0,
                "K menu deterministically keeps packed entry before new upload")) return 2;
    press(launcher::ButtonZ, now); press(launcher::ButtonZ, now);
    const auto launch = press(launcher::ButtonY, now);
    if (!expect(launch.launch &&
                std::strcmp(overlay_menu.selected_entry().snapshot_id, "ktris") == 0 &&
                !overlay_menu.selected_entry().packed,
                "Ktris launches through the CPCLIB1 snapshot path")) return 2;

    {
        Flash scaled = framed;
        library::View scale_view;
        if (!expect(scale_view.mount(scaled.mapped_library(), library::kLibraryBytes),
                    "scale catalogue mounts")) return 2;
        auto* catalog = scaled.bytes.data() + library::kLibraryFlashOffset +
                        scale_view.catalog_offset();
        const auto template_package = package("scale_seed", 10, "A Scale Seed", 6);
        const auto* template_descriptor = template_package.data() + 16;
        const auto* template_snapshot = template_package.data() + 96;
        constexpr std::uint32_t allocation = 17U * 4096U;
        constexpr std::uint32_t first_extra = migration::kSavedHighWater + allocation;
        for (std::uint16_t descriptor_index = 3; descriptor_index < 175;
             ++descriptor_index) {
            const std::uint32_t data_offset = first_extra +
                static_cast<std::uint32_t>(descriptor_index - 3) * allocation;
            std::memcpy(scaled.bytes.data() + library::kLibraryFlashOffset + data_offset,
                        template_snapshot, library::kSnapshotBytes);
            auto* descriptor = catalog + library::kCatalogHeaderBytes +
                               descriptor_index * library::kDescriptorBytes;
            std::memcpy(descriptor, template_descriptor, library::kDescriptorBytes);
            std::memset(descriptor, 0, 56);
            char id[16]{};
            char title[40]{};
            const char letter = static_cast<char>('A' + (descriptor_index - 3) % 26);
            std::snprintf(id, sizeof(id), "scale_%03u", descriptor_index);
            std::snprintf(title, sizeof(title), "%c Scale %03u", letter, descriptor_index);
            std::memcpy(descriptor, id, std::strlen(id));
            std::memcpy(descriptor + 16, title, std::strlen(title));
            descriptor[56] = 1; descriptor[57] = 64; descriptor[58] = 3;
            descriptor[59] = 6;
            p32(descriptor + 60, data_offset);
            p32(descriptor + 64, library::kSnapshotBytes);
            p32(descriptor + 68, crc32(template_snapshot, library::kSnapshotBytes));
            p32(descriptor + 72, 0);
            p32(descriptor + 72, crc32(descriptor, library::kDescriptorBytes));
        }
        for (const std::size_t visible_target : {25U, 50U, 100U, 179U}) {
            const auto descriptor_count = static_cast<std::uint16_t>(visible_target - 4);
            p16(catalog + 16, descriptor_count);
            p32(catalog + 40, migration::kSavedHighWater +
                                  static_cast<std::uint32_t>(descriptor_count - 2) * allocation);
            p32(catalog + 28, 0);
            p32(catalog + 28, crc32(catalog, library::kCatalogHeaderBytes));
            launcher::configure_library(scaled.mapped_library(), library::kLibraryBytes);
            if (!expect(launcher::configure_packed_library(
                            scaled.bytes.data() + migration::kPackOffset,
                            migration::kPackBytes) &&
                        launcher::entry_count() == visible_target,
                        "scaled merged catalogue count")) return 2;
            launcher::MenuController scaled_menu;
            scaled_menu.reset(0, 0);
            std::uint32_t scale_time = 0;
            const auto scale_press = [&scaled_menu, &scale_time](launcher::ButtonMask button) {
                scaled_menu.update(button, scale_time += 10);
                scaled_menu.update(button, scale_time += 30);
                scaled_menu.update(0, scale_time += 10);
                scaled_menu.update(0, scale_time += 30);
            };
            scale_press(launcher::ButtonZ); scale_press(launcher::ButtonY);
            scale_press(launcher::ButtonZ); scale_press(launcher::ButtonY);
            std::size_t scale_observed = 0;
            const auto scale_started = std::chrono::steady_clock::now();
            for (int iteration = 0; iteration < 50; ++iteration) {
                scale_press(launcher::ButtonZ);
                const auto first = scaled_menu.first_visible(5);
                for (std::size_t row = 0; row < 5; ++row) {
                    const auto index = first + row;
                    if (index >= scaled_menu.visible_count()) break;
                    scale_observed += std::strlen(scaled_menu.label(index));
                }
            }
            const auto scale_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - scale_started).count();
            std::printf("CATALOG_SCALE visible=%zu iterations=50 elapsed_us=%lld observed=%zu\n",
                        visible_target, static_cast<long long>(scale_elapsed), scale_observed);
            if (!expect(scale_observed != 0, "scaled navigation exercises labels")) return 2;
        }
        launcher::configure_library(framed.mapped_library(), library::kLibraryBytes);
        launcher::configure_packed_library(framed.bytes.data() + migration::kPackOffset,
                                           migration::kPackBytes);
    }

    Flash flash = original; storage::Installer installer(flash);
    migration::Transport transport(flash, installer, installer.scratch_buffer(), installer.scratch_bytes());
    if (!expect(transport.recover() && transport.state() == migration::State::Legacy, "legacy recovery") ||
        !expect(transport.begin(manifest(), true), "begin reserves catalogues and erases exact region") ||
        !expect(transfer(transport, pack), "bounded page transfer") ||
        !expect(transport.verify(), "staging readback") ||
        !expect(transport.activate(), "B-then-A activation") ||
        !expect(legacy_entries(flash) == 2, "legacy entries preserved") ||
        !expect(flash.protected_equal(original), "all protected/legacy bytes preserved")) return 3;
    storage::Installer reboot_installer(flash);
    migration::Transport reboot(flash, reboot_installer, reboot_installer.scratch_buffer(), reboot_installer.scratch_bytes());
    if (!expect(reboot.recover() && reboot.state() == migration::State::Active, "reboot sees active pack")) return 4;

    for (int fault : {0, 1, 2, 3, 4}) {
        Flash rejected = original; storage::Installer ri(rejected);
        migration::Transport rt(rejected, ri, ri.scratch_buffer(), ri.scratch_bytes()); rt.recover();
        auto bad = manifest();
        if (fault == 0) rejected.identity_value = "BAD";
        if (fault == 1) rejected.capacity_value = 8U * 1024U * 1024U;
        if (fault == 2) bad.flash_offset++;
        if (fault == 3) bad.image_bytes--;
        if (fault == 4) bad.image_sha256[0] ^= 1;
        const auto operations = rejected.operations;
        if (!expect(!rt.begin(bad, true), "bad manifest/identity rejected") ||
            !expect(rejected.operations == operations, "rejection writes nothing") ||
            !expect(rejected.all_old_equal(original), "rejection preserves all old bytes")) return 5;
    }

    Flash protocol = original; storage::Installer pi(protocol);
    migration::Transport pt(protocol, pi, pi.scratch_buffer(), pi.scratch_bytes()); pt.recover();
    if (!expect(pt.begin(manifest(), true), "protocol begin") ||
        !expect(!pt.begin(manifest(), true), "duplicate begin rejected") ||
        !expect(!pt.write_page(1, pack.data(), 256), "out-of-order page rejected")) return 6;

    Flash wrong_crc = original; storage::Installer wi(wrong_crc);
    migration::Transport wt(wrong_crc, wi, wi.scratch_buffer(), wi.scratch_bytes()); wt.recover();
    auto corrupt_pack = pack; corrupt_pack[20000] ^= 1;
    if (!expect(wt.begin(manifest(), true) && transfer(wt, corrupt_pack) && !wt.verify(),
                "whole-image CRC rejects corrupt transfer") ||
        !expect(wrong_crc.protected_equal(original), "CRC failure preserves protected bytes")) return 7;

    Flash readback = original; storage::Installer bi(readback);
    migration::Transport bt(readback, bi, bi.scratch_buffer(), bi.scratch_bytes()); bt.recover();
    if (!expect(bt.begin(manifest(), true), "readback begin")) return 8;
    readback.corrupt_next(1);
    if (!expect(!bt.write_page(0, pack.data(), 256) ||
                (!bt.write_page(1, pack.data() + 256, 256) && bt.state() == migration::State::Ambiguous),
                "program/readback corruption fails closed")) return 9;

    Flash interrupted = original; storage::Installer ii(interrupted);
    migration::Transport it(interrupted, ii, ii.scratch_buffer(), ii.scratch_bytes()); it.recover();
    if (!expect(it.begin(manifest(), true) && transfer(it, pack) && it.verify(), "pre-activation state") ||
        !expect(interrupted.protected_equal(original), "interruption before activation preserves old data")) return 10;
    storage::Installer power_installer(interrupted);
    migration::Transport power(interrupted, power_installer, power_installer.scratch_buffer(), power_installer.scratch_bytes());
    if (!expect(power.recover() && power.state() == migration::State::ReservedIncomplete,
                "power loss before activation recovers incomplete") ||
        !expect(legacy_entries(interrupted) == 2, "rollback remains available")) return 11;

    Flash lockout = original; storage::Installer li(lockout);
    migration::Transport lt(lockout, li, li.scratch_buffer(), li.scratch_bytes()); lt.recover();
    lockout.fail_next(1);
    if (!expect(!lt.begin(manifest(), true), "flash-safe operation failure stops migration") ||
        !expect(legacy_entries(lockout) == 2, "lockout failure preserves legacy view")) return 12;

    for (const std::size_t operation : {70U, 133U}) {
        Flash cut = original; storage::Installer ci(cut);
        migration::Transport ct(cut, ci, ci.scratch_buffer(), ci.scratch_bytes()); ct.recover();
        cut.fail_next(operation);
        if (!expect(!ct.begin(manifest(), true), "catalog/erase power cut injected") ||
            !expect(legacy_entries(cut) == 2, "catalog/erase cut keeps legacy catalogue") ||
            !expect(cut.protected_equal(original), "catalog/erase cut preserves protected data")) return 13;
    }

    Flash program_cut = original; storage::Installer pci(program_cut);
    migration::Transport pct(program_cut, pci, pci.scratch_buffer(), pci.scratch_bytes()); pct.recover();
    if (!expect(pct.begin(manifest(), true), "program-cut begin")) return 14;
    program_cut.fail_next(20);
    if (!expect(!transfer(pct, pack) && pct.state() == migration::State::Ambiguous,
                "mid-program cut becomes ambiguous") ||
        !expect(legacy_entries(program_cut) == 2 && program_cut.protected_equal(original),
                "mid-program cut preserves legacy/protected data")) return 15;

    for (const std::size_t activation_operation : {1U, 2U}) {
        Flash activation = original; storage::Installer ai(activation);
        migration::Transport at(activation, ai, ai.scratch_buffer(), ai.scratch_bytes()); at.recover();
        if (!expect(at.begin(manifest(), true) && transfer(at, pack) && at.verify(),
                    "activation-cut prepared")) return 16;
        activation.fail_next(activation_operation);
        if (!expect(!at.activate() && at.state() == migration::State::Ambiguous,
                    "header activation cut fails closed") ||
            !expect(legacy_entries(activation) == 2 && activation.protected_equal(original),
                    "header cut preserves legacy/protected data")) return 17;
        storage::Installer ari(activation);
        migration::Transport recovered_activation(
            activation, ari, ari.scratch_buffer(), ari.scratch_bytes());
        if (!expect(recovered_activation.recover(), "header-cut reboot recovery") ||
            !expect(recovered_activation.state() ==
                        (activation_operation == 1 ? migration::State::ReservedIncomplete :
                                                     migration::State::Active),
                    "header B is the atomic activation boundary")) return 18;
    }

    std::puts("PASS: bootstrap states A-F, restricted transport, interruption, rollback and protected bytes");
    return 0;
}
