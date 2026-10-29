#include "game_library.h"
#include "library_installer.h"
#include "upload_protocol.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
using cpcexplorer::library::crc32;

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

void put16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
void put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

std::vector<std::uint8_t> snapshot(std::uint8_t seed) {
    std::vector<std::uint8_t> result(cpcexplorer::library::kSnapshotBytes);
    for (std::size_t i = 256; i < result.size(); ++i)
        result[i] = static_cast<std::uint8_t>(i * 17U + seed);
    std::memcpy(result.data(), "MV - SNA", 8);
    result[16] = 3;
    result[107] = 64;
    result[108] = 0;
    return result;
}

std::vector<std::uint8_t> package(const char* id, const char* title, std::uint8_t seed) {
    const auto sna = snapshot(seed);
    std::vector<std::uint8_t> result(96 + sna.size());
    std::memcpy(result.data(), "CPCGAME1", 8);
    put16(result.data() + 8, 1); put16(result.data() + 10, 96);
    put32(result.data() + 12, static_cast<std::uint32_t>(result.size()));
    auto* d = result.data() + 16;
    std::memcpy(d, id, std::min<std::size_t>(std::strlen(id), 15));
    std::memcpy(d + 16, title, std::min<std::size_t>(std::strlen(title), 39));
    d[56] = 1; d[57] = 64; d[58] = 3;
    put32(d + 60, 96); put32(d + 64, static_cast<std::uint32_t>(sna.size()));
    put32(d + 68, crc32(sna.data(), sna.size()));
    put32(d + 72, 0); put32(d + 72, crc32(d, 80));
    std::memcpy(result.data() + 96, sna.data(), sna.size());
    return result;
}

class SimFlash final : public cpcexplorer::storage::Flash {
public:
    SimFlash() : bytes(cpcexplorer::library::kFlashBytes, 0xff) {
        std::fill(bytes.begin(), bytes.begin() + cpcexplorer::library::kFirmwareReserveBytes,
                  static_cast<std::uint8_t>(0xa5));
        std::fill(bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset, bytes.end(),
                  static_cast<std::uint8_t>(0x5a));
    }
    bool ready() const override { return supported; }
    const char* identity() const override { return supported ? "EF-40-18" : "unsupported"; }
    std::uint32_t capacity() const override { return capacity_bytes; }
    const std::uint8_t* mapped_library() const override {
        return bytes.data() + cpcexplorer::library::kLibraryFlashOffset;
    }
    bool erase(std::uint32_t offset, std::size_t size) override {
        if (!allowed(offset, size, 4096) || fails()) return false;
        std::fill(bytes.begin() + offset, bytes.begin() + offset + size,
                  static_cast<std::uint8_t>(0xff));
        return true;
    }
    bool program(std::uint32_t offset, const std::uint8_t* data, std::size_t size) override {
        if (data == nullptr || !allowed(offset, size, 256) || fails()) return false;
        for (std::size_t i = 0; i < size; ++i) {
            if ((bytes[offset + i] & data[i]) != data[i]) return false;
        }
        for (std::size_t i = 0; i < size; ++i) bytes[offset + i] &= data[i];
        if (corrupt_operation != 0 && operations == corrupt_operation && size != 0)
            bytes[offset + size / 2] ^= 0x01;
        return true;
    }
    void fail_relative(std::size_t operation) { fail_operation = operations + operation; }
    void corrupt_relative(std::size_t operation) { corrupt_operation = operations + operation; }
    void clear_failure() { fail_operation = 0; }
    bool sentinels() const {
        return std::all_of(bytes.begin(), bytes.begin() + cpcexplorer::library::kFirmwareReserveBytes,
                           [](std::uint8_t v) { return v == 0xa5; }) &&
               std::all_of(bytes.begin() + cpcexplorer::library::kE10GuardFlashOffset, bytes.end(),
                           [](std::uint8_t v) { return v == 0x5a; });
    }

    std::vector<std::uint8_t> bytes;
    bool supported = true;
    std::uint32_t capacity_bytes = cpcexplorer::library::kFlashBytes;
    std::size_t operations = 0;
    std::size_t fail_operation = 0;
    std::size_t corrupt_operation = 0;

private:
    bool allowed(std::uint32_t offset, std::size_t size, std::uint32_t alignment) const {
        return size != 0 && size <= UINT32_MAX && (offset & (alignment - 1)) == 0 &&
               (size & (alignment - 1)) == 0 &&
               cpcexplorer::library::is_library_flash_range(offset,
                   static_cast<std::uint32_t>(size));
    }
    bool fails() {
        ++operations;
        return fail_operation != 0 && operations == fail_operation;
    }
};

bool install(SimFlash& flash, const std::vector<std::uint8_t>& bytes) {
    cpcexplorer::storage::Installer installer(flash);
    cpcexplorer::upload::Session session;
    if (session.begin(static_cast<std::uint32_t>(bytes.size()), crc32(bytes.data(), bytes.size()),
                      installer) != cpcexplorer::upload::Result::Ok) return false;
    std::uint32_t sequence = 0;
    for (std::size_t at = 0; at < bytes.size(); at += cpcexplorer::upload::kMaximumChunkBytes) {
        const auto count = std::min<std::size_t>(cpcexplorer::upload::kMaximumChunkBytes,
                                                 bytes.size() - at);
        if (session.chunk(sequence++, bytes.data() + at, count, installer) !=
            cpcexplorer::upload::Result::Ok) {
            session.abort(installer);
            return false;
        }
    }
    const bool ok = session.commit(installer) == cpcexplorer::upload::Result::Ok;
    if (!ok) session.abort(installer);
    return ok;
}

std::size_t entries(const SimFlash& flash, std::uint32_t* generation = nullptr) {
    cpcexplorer::library::View view;
    if (!view.mount(flash.mapped_library(), cpcexplorer::library::kLibraryBytes)) return 0;
    if (generation) *generation = view.generation();
    return view.valid_count();
}

bool game_info(const SimFlash& flash, const char* id, cpcexplorer::library::GameInfo& result) {
    cpcexplorer::library::View view;
    if (!view.mount(flash.mapped_library(), cpcexplorer::library::kLibraryBytes)) return false;
    for (std::size_t i = 0; i < view.valid_count(); ++i) {
        cpcexplorer::library::GameInfo candidate{};
        if (view.game(i, candidate) && std::strcmp(candidate.game_id, id) == 0) {
            result = candidate;
            return true;
        }
    }
    return false;
}
}  // namespace

int main() {
    using namespace cpcexplorer;
    const auto one = package("game_one", "Game One", 1);
    const auto two = package("game_two", "Game Two", 2);
    const auto three = package("game_three", "Game Three", 3);

    SimFlash unsupported;
    unsupported.capacity_bytes = 8U * 1024U * 1024U;
    storage::Installer rejected(unsupported);
    if (!expect(!rejected.begin(static_cast<std::uint32_t>(one.size())),
                "unsupported flash capacity fails closed") ||
        !expect(unsupported.operations == 0, "rejected capacity performs no writes")) return 1;

    SimFlash flash;
    if (!expect(install(flash, one), "first transactional install") ||
        !expect(entries(flash) == 1, "first catalog visible") ||
        !expect(flash.sentinels(), "firmware and E10 guard untouched after first install")) return 2;

    const std::size_t before_duplicate = flash.operations;
    if (!expect(!install(flash, one), "duplicate game ID rejected") ||
        !expect(flash.operations == before_duplicate, "duplicate rejected before erase") ||
        !expect(entries(flash) == 1, "duplicate cannot alter catalog")) return 3;

    if (!expect(install(flash, two), "second transactional install") ||
        !expect(entries(flash) == 2, "multiple installed games enumerate") ||
        !expect(flash.sentinels(), "sentinels survive multiple installs")) return 4;

    const SimFlash baseline = flash;
    for (const std::size_t failure : {50U, 259U, 300U, 324U}) {
        SimFlash interrupted = baseline;
        interrupted.fail_operation = 0;
        interrupted.fail_relative(failure);
        if (!expect(!install(interrupted, three), "injected interruption reports failure") ||
            !expect(entries(interrupted) == 2, "old catalog survives interruption") ||
            !expect(interrupted.sentinels(), "interruption cannot touch protected regions")) return 5;
    }

    SimFlash recovered = baseline;
    recovered.fail_operation = 0;
    recovered.fail_relative(324);
    if (!expect(!install(recovered, three), "commit-marker interruption injected") ||
        !expect(entries(recovered) == 2, "uncommitted catalog ignored")) return 6;
    recovered.clear_failure();
    if (!expect(install(recovered, three), "retry after interruption succeeds") ||
        !expect(entries(recovered) == 3, "recovered catalog contains all games")) return 7;

    SimFlash bad_readback = baseline;
    bad_readback.fail_operation = 0;
    bad_readback.corrupt_relative(20);
    if (!expect(!install(bad_readback, three), "payload readback corruption rejected") ||
        !expect(entries(bad_readback) == 2, "readback failure cannot publish catalog")) return 71;

    SimFlash bad_transport = baseline;
    storage::Installer transport_installer(bad_transport);
    upload::Session transport_session;
    if (!expect(transport_session.begin(static_cast<std::uint32_t>(three.size()),
                                        crc32(three.data(), three.size()) ^ 1U,
                                        transport_installer) == upload::Result::Ok,
                "begin transfer with deliberately wrong whole-package CRC")) return 72;
    std::uint32_t bad_sequence = 0;
    for (std::size_t at = 0; at < three.size(); at += upload::kMaximumChunkBytes) {
        const auto count = std::min<std::size_t>(upload::kMaximumChunkBytes, three.size() - at);
        if (transport_session.chunk(bad_sequence++, three.data() + at, count,
                                    transport_installer) != upload::Result::Ok) return 73;
    }
    if (!expect(transport_session.commit(transport_installer) == upload::Result::IntegrityFailure,
                "whole-package CRC mismatch rejected") ||
        !expect(entries(bad_transport) == 2, "transport CRC failure cannot publish catalog")) return 74;

    SimFlash damaged = baseline;
    const std::size_t newest_commit = library::kLibraryFlashOffset +
                                      library::kCatalogSlotBytes + 32;
    std::fill(damaged.bytes.begin() + newest_commit, damaged.bytes.begin() + newest_commit + 4,
              static_cast<std::uint8_t>(0xff));
    std::uint32_t fallback_generation = 0;
    if (!expect(entries(damaged, &fallback_generation) == 1 && fallback_generation == 1,
                "damaged newest catalog falls back to prior generation")) return 8;

    SimFlash persistence = baseline;
    std::fill(persistence.bytes.begin(),
              persistence.bytes.begin() + library::kFirmwareReserveBytes,
              static_cast<std::uint8_t>(0x33));
    if (!expect(entries(persistence) == 2, "library persists across simulated UF2 firmware update") ||
        !expect(std::all_of(persistence.bytes.begin() + library::kE10GuardFlashOffset,
                            persistence.bytes.end(), [](std::uint8_t v) { return v == 0x5a; }),
                "firmware update simulation preserves E10 guard")) return 9;

    std::uint8_t page[256]{};
    if (!expect(!flash.erase(library::kFirmwareReserveBytes - 4096, 4096),
                "backend rejects firmware erase") ||
        !expect(!flash.erase(library::kE10GuardFlashOffset, 4096),
                "backend rejects guard erase") ||
        !expect(!flash.program(library::kE10GuardFlashOffset, page, sizeof(page)),
                "backend rejects guard program") ||
        !expect(!flash.erase(library::kLibraryFlashOffset + 1, 4096),
                "backend rejects sector misalignment") ||
        !expect(!flash.program(library::kLibraryFlashOffset + 1, page, sizeof(page)),
                "backend rejects page misalignment") ||
        !expect(!flash.erase(0xfffff000U, 8192), "backend rejects overflow")) return 10;

    upload::StreamDecoder decoder;
    upload::Frame frame{};
    decoder.feed('C', 10, frame);
    if (!expect(decoder.poll(2010) == upload::DecodeResult::Timeout,
                "partial USB frame times out and resets")) return 11;

    SimFlash removal;
    if (!expect(install(removal, one), "install synthetic game for removal") ||
        !expect(install(removal, two), "install unrelated game for preservation")) return 20;
    library::GameInfo removed_before{}, preserved_before{};
    if (!expect(game_info(removal, "game_one", removed_before), "locate removal target") ||
        !expect(game_info(removal, "game_two", preserved_before), "locate preserved game")) return 21;
    const std::vector<std::uint8_t> removed_payload(
        removed_before.data, removed_before.data + removed_before.data_length);
    const std::size_t before_rejections = removal.operations;
    storage::Installer remover(removal);
    if (!expect(!remover.remove("locomotive_basic"), "BASIC removal rejected") ||
        !expect(!remover.remove("knight_tyme"), "Knight Tyme removal rejected") ||
        !expect(!remover.remove("does_not_exist"), "unknown removal rejected") ||
        !expect(removal.operations == before_rejections, "rejected removals perform no writes"))
        return 22;
    if (!expect(remover.remove("game_one"), "transactional removal succeeds") ||
        !expect(entries(removal) == 1, "removed game hidden") ||
        !expect(!game_info(removal, "game_one", removed_before), "removed ID absent") ||
        !expect(game_info(removal, "game_two", preserved_before), "unrelated game remains") ||
        !expect(removal.sentinels(), "removal preserves firmware and E10 guard")) return 23;
    const auto removed_flash = removal.bytes.begin() + library::kLibraryFlashOffset +
                               library::kDataStart;
    if (!expect(std::equal(removed_payload.begin(), removed_payload.end(), removed_flash),
                "removed payload is retained and not erased")) return 24;

    storage::Installer after_restart(removal);
    if (!expect(after_restart.recover_pending_removal(), "restart recovery sees completed removal") ||
        !expect(entries(removal) == 1, "removal persists after restart") ||
        !expect(!after_restart.remove("game_one"), "repeat removal is rejected")) return 25;

    SimFlash fallback = removal;
    library::View newest;
    newest.mount(fallback.mapped_library(), library::kLibraryBytes);
    const std::size_t removal_newest_commit =
        library::kLibraryFlashOffset + newest.catalog_offset() + 32;
    std::fill(fallback.bytes.begin() + removal_newest_commit,
              fallback.bytes.begin() + removal_newest_commit + 4,
              static_cast<std::uint8_t>(0xff));
    if (!expect(entries(fallback) == 1, "fallback catalog cannot resurrect removed game") ||
        !expect(!game_info(fallback, "game_one", removed_before),
                "removed ID absent from fallback catalog")) return 26;

    if (!expect(install(removal, three), "install after removal") ||
        !expect(game_info(removal, "game_three", removed_before), "locate post-removal game") ||
        !expect(removed_before.data_offset > library::kDataStart,
                "allocator does not reuse removed payload allocation")) return 27;

    SimFlash active_upload;
    storage::Installer busy(active_upload);
    if (!expect(busy.begin(static_cast<std::uint32_t>(one.size())), "start synthetic active upload") ||
        !expect(!busy.remove("game_one"), "removal during active upload rejected")) return 28;
    busy.abort();

    SimFlash interruption_base;
    if (!expect(install(interruption_base, one), "prepare removal interruption baseline")) return 29;
    for (const std::size_t failure : {1U, 20U, 65U, 66U, 67U, 80U, 131U, 132U}) {
        SimFlash interrupted = interruption_base;
        interrupted.fail_operation = 0;
        interrupted.fail_relative(failure);
        storage::Installer interrupted_remover(interrupted);
        if (!expect(!interrupted_remover.remove("game_one"),
                    "injected removal interruption reports failure")) return 30;
        interrupted.clear_failure();
        storage::Installer rebooted(interrupted);
        if (!expect(rebooted.recover_pending_removal(), "pending removal recovers on restart") ||
            !expect(interrupted.sentinels(), "interrupted removal preserves protected regions"))
            return 31;
        if (failure <= 66) {
            if (!expect(entries(interrupted) == 1,
                        "pre-commit interruption retains original game")) return 32;
        } else {
            if (!expect(entries(interrupted) == 0,
                        "post-pending-commit recovery completes removal")) return 33;
            library::View recovered_view;
            recovered_view.mount(interrupted.mapped_library(), library::kLibraryBytes);
            const std::size_t recovered_commit = library::kLibraryFlashOffset +
                                                  recovered_view.catalog_offset() + 32;
            std::fill(interrupted.bytes.begin() + recovered_commit,
                      interrupted.bytes.begin() + recovered_commit + 4,
                      static_cast<std::uint8_t>(0xff));
            if (!expect(entries(interrupted) == 0,
                        "recovered fallback cannot resurrect removed game")) return 34;
        }
    }

    std::puts("PASS: install/remove transactions, dual-catalog recovery, non-reuse, persistence, guards");
    return 0;
}
