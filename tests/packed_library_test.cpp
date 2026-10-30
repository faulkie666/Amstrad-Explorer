#include "cpc_core.h"
#include "game_library.h"
#include "launcher.h"
#include "packed_library.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
void p16(std::uint8_t* p, std::uint16_t v) { p[0] = v; p[1] = v >> 8; }
void p32(std::uint8_t* p, std::uint32_t v) {
    p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}
std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}
std::vector<std::uint8_t> literal_lz4(const std::uint8_t* data, std::size_t size) {
    std::vector<std::uint8_t> out{0xF0};
    std::size_t extra = size - 15;
    while (extra >= 255) { out.push_back(255); extra -= 255; }
    out.push_back(static_cast<std::uint8_t>(extra));
    out.insert(out.end(), data, data + size);
    return out;
}
void make_header(std::uint8_t* h, unsigned slot, std::uint32_t records_offset,
                 std::uint32_t image_size, std::uint32_t records_crc,
                 std::uint32_t payload_crc) {
    std::memcpy(h, "CPXPCK2", 7); p16(h + 8, 2); p16(h + 10, 128);
    p16(h + 12, 160); p16(h + 14, slot); p32(h + 16, 1); p32(h + 20, 1);
    p32(h + 24, records_offset); p32(h + 28, 160); p32(h + 32, 16384);
    p32(h + 36, image_size); p32(h + 40, records_crc); p32(h + 44, payload_crc);
    p32(h + 48, 0); p32(h + 48, cpcexplorer::library::crc32(h, 128));
}
std::vector<std::uint8_t> make_image(const std::vector<std::uint8_t>& sna) {
    const auto block = literal_lz4(sna.data() + 256, 65536);
    std::vector<std::uint8_t> payload(sna.begin(), sna.begin() + 256);
    payload.insert(payload.end(), block.begin(), block.end());
    const std::size_t image_size = (16384 + payload.size() + 15) & ~std::size_t(15);
    std::vector<std::uint8_t> image(image_size);
    std::array<std::uint8_t, 160> record{};
    std::memcpy(record.data(), "knight_tyme", 12);
    std::memcpy(record.data() + 32, "Knight Tyme", 11);
    record[96] = 1; record[97] = 'K'; record[98] = 0; record[99] = 64;
    record[100] = 1; record[101] = 1;
    p32(record.data() + 104, 16384); p32(record.data() + 108, payload.size());
    p32(record.data() + 112, sna.size());
    p32(record.data() + 116, cpcexplorer::library::crc32(sna.data(), sna.size()));
    p32(record.data() + 120, cpcexplorer::library::crc32(payload.data(), payload.size()));
    std::memcpy(image.data() + 8192, record.data(), record.size());
    std::memcpy(image.data() + 12288, record.data(), record.size());
    std::memcpy(image.data() + 16384, payload.data(), payload.size());
    const auto records_crc = cpcexplorer::library::crc32(record.data(), record.size());
    const auto payload_crc = cpcexplorer::library::crc32(image.data() + 16384,
                                                         image.size() - 16384);
    make_header(image.data(), 0, 8192, image.size(), records_crc, payload_crc);
    make_header(image.data() + 4096, 1, 12288, image.size(), records_crc, payload_crc);
    return image;
}
std::uint32_t frame_crc() {
    return cpcexplorer::library::crc32(cpcexplorer::framebuffer(),
                                       cpcexplorer::framebuffer_width() *
                                       cpcexplorer::framebuffer_height());
}
bool expect(bool value, const char* message) {
    if (!value) std::fprintf(stderr, "FAIL: %s\n", message);
    return value;
}
}

int main() {
    const auto sna = read_file(CPCEXPLORER_PACKED_TEST_SNA);
    if (!expect(sna.size() == 65792, "test SNA size")) return 1;
    auto image = make_image(sna);
    cpcexplorer::packed::View view;
    cpcexplorer::packed::Record record{};
    if (!expect(view.mount(image.data(), image.size()), "mount") ||
        !expect(view.count() == 1 && view.generation() == 1, "catalogue identity") ||
        !expect(view.record(0, record), "record") ||
        !expect(std::strcmp(record.id, "knight_tyme") == 0, "record ID") ||
        !expect(view.has_letter(cpcexplorer::packed::Category::Arcade, 'K'), "A-Z lookup")) return 2;
    std::array<std::uint8_t, 65536> ram{};
    if (!expect(cpcexplorer::packed::decode_snapshot(record, ram.data(), ram.size()), "decode") ||
        !expect(std::memcmp(ram.data(), sna.data() + 256, ram.size()) == 0, "round trip")) return 3;
    if (!expect(!cpcexplorer::packed::decode_snapshot(record,
                const_cast<std::uint8_t*>(record.payload + 256), ram.size()), "overlap rejected")) return 4;

    cpcexplorer::select_engine(cpcexplorer::EngineKind::Reference);
    cpcexplorer::init();
    if (!expect(cpcexplorer::load_sna(sna.data(), sna.size()), "ordinary load")) return 5;
    std::array<std::uint8_t, 16> ordinary_ay{}, packed_ay{};
    cpcexplorer::copy_ay_registers(ordinary_ay.data(), ordinary_ay.size());
    cpcexplorer::run_us(20000); const auto ordinary_pc = cpcexplorer::diagnostics().pc;
    const auto ordinary_frame = frame_crc();
    cpcexplorer::init();
    if (!expect(cpcexplorer::load_packed_sna(record), "packed load")) return 6;
    cpcexplorer::copy_ay_registers(packed_ay.data(), packed_ay.size());
    if (!expect(packed_ay == ordinary_ay, "AY register restoration")) return 7;
    cpcexplorer::run_us(20000);
    if (!expect(cpcexplorer::diagnostics().pc == ordinary_pc, "CPU restoration") ||
        !expect(frame_crc() == ordinary_frame, "framebuffer restoration")) return 8;

    auto fallback = image; fallback[0] ^= 1;
    if (!expect(view.mount(fallback.data(), fallback.size()), "header-B fallback")) return 9;
    auto corrupt = image; corrupt[20000] ^= 1;
    if (!expect(!view.mount(corrupt.data(), corrupt.size()), "payload corruption rejected")) return 10;
    auto truncated = image;
    if (!expect(!view.mount(truncated.data(), 12000), "truncation rejected")) return 11;
    if (!expect(cpcexplorer::launcher::configure_packed_library(image.data(), image.size()),
                "launcher mount")) return 12;
    cpcexplorer::launcher::MenuController menu;
    menu.reset(0, 0);
    if (!expect(menu.visible_count() == 3 && std::strcmp(menu.label(1), "Arcade") == 0,
                "top-level menu")) return 13;
    menu.update(cpcexplorer::launcher::ButtonZ, 10);
    menu.update(cpcexplorer::launcher::ButtonZ, 40);
    menu.update(0, 50); menu.update(0, 80);
    menu.update(cpcexplorer::launcher::ButtonY, 90);
    const auto category = menu.update(cpcexplorer::launcher::ButtonY, 120);
    if (!expect(category.selection_changed && menu.visible_count() == 2 &&
                std::strcmp(menu.label(1), "K") == 0, "category to A-Z")) return 14;
    menu.update(0, 130); menu.update(0, 160);
    menu.update(cpcexplorer::launcher::ButtonZ, 170);
    menu.update(cpcexplorer::launcher::ButtonZ, 200);
    menu.update(0, 210); menu.update(0, 240);
    menu.update(cpcexplorer::launcher::ButtonY, 250);
    menu.update(cpcexplorer::launcher::ButtonY, 280);
    if (!expect(menu.visible_count() == 2 && std::strcmp(menu.label(1), "Knight Tyme") == 0,
                "A-Z to games")) return 15;
    menu.update(0, 290); menu.update(0, 320);
    menu.update(cpcexplorer::launcher::ButtonZ, 330);
    menu.update(cpcexplorer::launcher::ButtonZ, 360);
    menu.update(0, 370); menu.update(0, 400);
    menu.update(cpcexplorer::launcher::ButtonY, 410);
    if (!expect(menu.update(cpcexplorer::launcher::ButtonY, 440).launch &&
                menu.selected_entry().packed, "packed game launch")) return 16;
    std::puts("PASS: CPXPCK2 A/B mount, bounds, CRC, LZ4, overlap and snapshot restoration");
    return 0;
}
