#include "cpc_core.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto size = input.tellg();
    if (size <= 0) return {};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

bool valid_amsdos(const std::vector<std::uint8_t>& data, std::uint16_t load,
                  std::uint16_t length, std::uint16_t execute) {
    if (data.size() < 128U + length) return false;
    std::uint16_t checksum = 0;
    for (std::size_t i = 0; i <= 66; ++i)
        checksum = static_cast<std::uint16_t>(checksum + data[i]);
    const auto word = [&data](std::size_t offset) {
        return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
    };
    return checksum == word(67) && load == word(21) && length == word(24) &&
           execute == word(26);
}

std::uint16_t word(const std::vector<std::uint8_t>& data, std::size_t offset);

bool valid_amsdos_header(const std::vector<std::uint8_t>& data, std::uint8_t type) {
    if (data.size() < 128) return false;
    std::uint16_t checksum = 0;
    for (std::size_t i = 0; i <= 66; ++i)
        checksum = static_cast<std::uint16_t>(checksum + data[i]);
    const auto length = word(data, 24);
    return data[18] == type && length != 0 && data.size() >= 128U + length &&
           checksum == word(data, 67);
}

bool loader_runs_binary(const std::vector<std::uint8_t>& loader,
                        const std::vector<std::uint8_t>& binary) {
    std::string binary_name;
    for (std::size_t i = 1; i < 9 && binary[i] != ' '; ++i)
        binary_name.push_back(static_cast<char>(binary[i]));
    const std::size_t payload_end = 128U + word(loader, 24);
    std::size_t offset = 128;
    while (offset + 5 <= payload_end) {
        const std::uint16_t line_bytes = word(loader, offset);
        if (line_bytes < 5 || offset + line_bytes > payload_end) return false;
        const auto begin = loader.begin() + static_cast<std::ptrdiff_t>(offset + 4);
        const auto end = loader.begin() + static_cast<std::ptrdiff_t>(offset + line_bytes);
        if (*begin == 0xca) { // Locomotive BASIC RUN
            const auto first_quote = std::find(begin, end, static_cast<std::uint8_t>('"'));
            if (first_quote == end) return false;
            const auto second_quote = std::find(first_quote + 1, end,
                                                static_cast<std::uint8_t>('"'));
            if (second_quote == end) return false;
            std::string target(first_quote + 1, second_quote);
            if (!target.empty() && target.front() == '!') target.erase(target.begin());
            return target == binary_name;
        }
        offset += line_bytes;
    }
    return false;
}

std::uint16_t word(const std::vector<std::uint8_t>& data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

bool valid_manic_loader(const std::vector<std::uint8_t>& data) {
    if (!valid_amsdos(data, 0x0170, 1429, 0x0000) || data[18] != 0) return false;
    const auto payload_end = 128U + word(data, 24);
    std::size_t offset = 128;
    while (offset + 4 <= payload_end) {
        const std::uint16_t line_bytes = word(data, offset);
        if (line_bytes == 0) break;
        if (line_bytes < 5 || offset + line_bytes > payload_end) return false;
        const std::uint16_t line_number = word(data, offset + 2);
        if (line_number == 600) {
            // Tokenized Locomotive BASIC:
            // MEMORY &3FFF:LOAD"manic.bin":CALL &4000
            const std::uint8_t expected[] = {
                0x20, 0xaa, 0x20, 0x1c, 0xff, 0x3f, 0x01, 0xa8,
                '"', 'm', 'a', 'n', 'i', 'c', '.', 'b', 'i', 'n', '"',
                0x01, 0x83, 0x20, 0x1c, 0x00, 0x40, 0x01};
            const auto begin = data.begin() + static_cast<std::ptrdiff_t>(offset + 4);
            const auto end = data.begin() + static_cast<std::ptrdiff_t>(offset + line_bytes);
            return std::search(begin, end, std::begin(expected), std::end(expected)) != end;
        }
        offset += line_bytes;
    }
    return false;
}

void run_seconds(int seconds) {
    for (int frame = 0; frame < seconds * 50; ++frame) cpcexplorer::run_us(20'000);
}

void run_frames(int frames) {
    for (int frame = 0; frame < frames; ++frame) cpcexplorer::run_us(20'000);
}

void type_text(std::string_view text) {
    for (const unsigned char key : text) {
        cpcexplorer::key_down(key);
        cpcexplorer::run_us(50'000);
        cpcexplorer::key_up(key);
        cpcexplorer::run_us(100'000);
    }
}

std::uint64_t frame_hash() {
    const auto* data = cpcexplorer::framebuffer();
    const auto count = static_cast<std::size_t>(cpcexplorer::framebuffer_width()) *
                       cpcexplorer::framebuffer_height();
    std::uint64_t hash = 1469598103934665603ull;
    for (std::size_t i = 0; i < count; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::vector<std::uint8_t> frame_copy() {
    const auto* data = cpcexplorer::framebuffer();
    const auto count = static_cast<std::size_t>(cpcexplorer::framebuffer_width()) *
                       cpcexplorer::framebuffer_height();
    return {data, data + count};
}

std::size_t frame_difference(const std::vector<std::uint8_t>& lhs,
                             const std::vector<std::uint8_t>& rhs) {
    if (lhs.size() != rhs.size()) return static_cast<std::size_t>(-1);
    std::size_t different = 0;
    for (std::size_t i = 0; i < lhs.size(); ++i) different += lhs[i] != rhs[i];
    return different;
}

struct PlayerBounds {
    int min_x = 10000;
    int max_x = -1;
    int min_y = 10000;
    int max_y = -1;
    std::size_t pixels = 0;
};

PlayerBounds player_bounds(const std::vector<std::uint8_t>& frame) {
    PlayerBounds bounds{};
    const int width = cpcexplorer::framebuffer_width();
    const auto* palette = cpcexplorer::palette();
    // Central Cavern's player sprite occupies this isolated playfield area;
    // status text and the other white guardian are outside the crop.
    for (int y = 100; y < 160; ++y) {
        for (int x = 128; x < 260; ++x) {
            const std::uint32_t color = palette[frame[static_cast<std::size_t>(y * width + x)]];
            const int red = color & 0xff;
            const int green = (color >> 8) & 0xff;
            const int blue = (color >> 16) & 0xff;
            if (red <= 200 || green <= 200 || blue <= 200) continue;
            bounds.min_x = std::min(bounds.min_x, x);
            bounds.max_x = std::max(bounds.max_x, x);
            bounds.min_y = std::min(bounds.min_y, y);
            bounds.max_y = std::max(bounds.max_y, y);
            ++bounds.pixels;
        }
    }
    return bounds;
}

bool write_ppm(const std::filesystem::path& path) {
    const int width = cpcexplorer::framebuffer_width();
    const int height = cpcexplorer::framebuffer_height();
    const auto* frame = cpcexplorer::framebuffer();
    const auto* palette = cpcexplorer::palette();
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        const std::uint32_t abgr = palette[frame[i]];
        const std::array<char, 3> rgb = {
            static_cast<char>(abgr & 0xff), static_cast<char>((abgr >> 8) & 0xff),
            static_cast<char>((abgr >> 16) & 0xff)};
        out.write(rgb.data(), rgb.size());
    }
    return static_cast<bool>(out);
}

int build_knight_tyme(const std::filesystem::path& directory,
                      const std::filesystem::path& output) {
    const auto bootstrap = read_file(directory / "KT$.BIN");
    const auto code = read_file(directory / "KTCODE.BIN");
    const auto graphics = read_file(directory / "KTGRX.BIN");
    if (!valid_amsdos(bootstrap, 0x1388, 8192, 0x1388) ||
        !valid_amsdos(code, 0x1388, 30000, 0x1388) ||
        !valid_amsdos(graphics, 0xc000, 11110, 0x0000)) {
        std::fprintf(stderr, "unsupported or modified Knight Tyme source files\n");
        return 3;
    }
    cpcexplorer::init();
    run_seconds(5);
    if (!cpcexplorer::quickload(bootstrap.data(), bootstrap.size(), true)) return 4;
    run_seconds(8);
    cpcexplorer::key_down(' ');
    cpcexplorer::run_us(50'000);
    cpcexplorer::key_up(' ');
    run_seconds(2);
    if (!cpcexplorer::quickload(code.data(), code.size(), false) ||
        !cpcexplorer::quickload(graphics.data(), graphics.size(), false) ||
        !cpcexplorer::quickload(code.data(), code.size(), true)) return 5;
    run_seconds(12);
    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return 6;
    std::ofstream stream(output, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(snapshot.data()),
                 static_cast<std::streamsize>(snapshot.size()));
    if (!stream) return 7;
    std::printf("wrote validated 64 KiB CPC SNA: %s\n", output.string().c_str());
    return 0;
}

int build_usb_selftest(const std::filesystem::path& output,
                       const std::filesystem::path& evidence) {
    static constexpr std::string_view program[] = {
        "10 MODE 1:BORDER 0:INK 0,0:INK 1,24:PAPER 0:PEN 1:CLS\r",
        "20 LOCATE 14,8:PRINT \"CPC EXPLORER\"\r",
        "30 LOCATE 14,11:PRINT \"USB GAME TEST\"\r",
        "40 LOCATE 11,14:PRINT \"INSTALL SUCCESSFUL\"\r",
        "50 GOTO 50\r",
        "RUN\r",
    };
    cpcexplorer::init();
    run_seconds(5);
    for (const auto line : program) {
        type_text(line);
        run_seconds(1);
    }
    run_seconds(2);
    const std::uint64_t original_hash = frame_hash();
    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return 20;
    std::ofstream stream(output, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(snapshot.data()),
                 static_cast<std::streamsize>(snapshot.size()));
    if (!stream) return 21;
    stream.close();

    cpcexplorer::reset();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 22;
    run_seconds(2);
    const std::uint64_t reload_hash = frame_hash();
    if (!write_ppm(evidence)) return 23;
    if (original_hash != reload_hash) {
        std::fprintf(stderr, "self-test raster changed after SNA reload: %016llx != %016llx\n",
                     static_cast<unsigned long long>(original_hash),
                     static_cast<unsigned long long>(reload_hash));
        return 24;
    }
    std::printf("USB_SELFTEST snapshot=%s bytes=%zu raster=%016llx evidence=%s\n",
                output.string().c_str(), snapshot.size(),
                static_cast<unsigned long long>(reload_hash), evidence.string().c_str());
    return 0;
}

int build_manic_miner(const std::filesystem::path& directory,
                      const std::filesystem::path& output,
                      const std::filesystem::path& evidence) {
    const auto loader = read_file(directory / "MANIC");
    const auto binary = read_file(directory / "MANIC.BIN");
    if (!valid_manic_loader(loader) ||
        !valid_amsdos(binary, 0x4000, 17117, 0x0000) || binary[18] != 2) {
        std::fprintf(stderr, "unsupported or modified Manic Miner source files\n");
        return 30;
    }
    std::filesystem::create_directories(evidence);

    // Prove keyboard injection independently against an ordinary CPC464 BASIC
    // session before relying on the same matrix path for the game.
    cpcexplorer::init();
    run_seconds(5);
    const auto ready_hash = frame_hash();
    type_text("print 4\r");
    run_seconds(2);
    const auto print_hash = frame_hash();
    if (ready_hash == print_hash || !write_ppm(evidence / "basic-keyboard-print-4.ppm")) {
        std::fprintf(stderr, "CPC464 BASIC keyboard injection did not change the display\n");
        return 31;
    }
    std::printf("BASIC_INPUT ready=%016llx print_4=%016llx\n",
        static_cast<unsigned long long>(ready_hash),
        static_cast<unsigned long long>(print_hash));

    // Reproduce line 600 of the validated tokenized loader. The host has no
    // AMSDOS filesystem, so quickload supplies LOAD while BASIC supplies the
    // exact MEMORY and CALL operations used by RUN"MANIC".
    cpcexplorer::init();
    run_seconds(5);
    type_text("memory 16383\r");
    run_seconds(1);
    auto launch_binary = binary;
    launch_binary[26] = 0x00;
    launch_binary[27] = 0x40;
    if (!cpcexplorer::quickload(launch_binary.data(), launch_binary.size(), true)) return 32;
    run_seconds(2);
    if (!write_ppm(evidence / "manic-miner-trainer-prompt.ppm")) return 33;
    for (int prompt = 0; prompt < 4; ++prompt) {
        type_text("n");
        run_seconds(1);
    }
    type_text("1\r");
    run_seconds(8);
    const auto title_hash = frame_hash();
    if (!write_ppm(evidence / "manic-miner-title.ppm")) return 33;

    cpcexplorer::key_down(0x0d);
    run_seconds(1);
    cpcexplorer::key_up(0x0d);
    run_seconds(4);
    const auto gameplay_hash = frame_hash();
    if (!write_ppm(evidence / "manic-miner-gameplay.ppm") || gameplay_hash == title_hash) {
        std::fprintf(stderr, "Manic Miner did not advance from title to gameplay\n");
        return 34;
    }

    std::vector<std::uint8_t> gameplay_state(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(gameplay_state.data(), gameplay_state.size())) return 35;
    const auto scenario = [&](int key, const char* filename,
                              std::vector<std::uint8_t>& frame) {
        cpcexplorer::init();
        if (!cpcexplorer::load_sna(gameplay_state.data(), gameplay_state.size())) return false;
        cpcexplorer::key_down(key);
        run_frames(15);
        frame = frame_copy();
        const bool wrote = write_ppm(evidence / filename);
        cpcexplorer::key_up(key);
        run_frames(5);
        return wrote;
    };
    std::vector<std::uint8_t> reference, left, right, jump;
    if (!scenario(0, "manic-miner-reference.ppm", reference)) return 36;
    // Key code zero is unregistered and provides the timing-identical control.
    if (!scenario('q', "manic-miner-left.ppm", left) ||
        !scenario('w', "manic-miner-right.ppm", right) ||
        !scenario(cpcexplorer::KeyShift, "manic-miner-jump.ppm", jump)) return 37;
    const auto left_diff = frame_difference(reference, left);
    const auto right_diff = frame_difference(reference, right);
    const auto jump_diff = frame_difference(reference, jump);
    const auto reference_player = player_bounds(reference);
    const auto left_player = player_bounds(left);
    const auto right_player = player_bounds(right);
    const auto jump_player = player_bounds(jump);
    std::printf("MANIC_INPUT title=%016llx gameplay=%016llx left_diff=%zu right_diff=%zu jump_diff=%zu\n",
        static_cast<unsigned long long>(title_hash),
        static_cast<unsigned long long>(gameplay_hash), left_diff, right_diff, jump_diff);
    std::printf("MANIC_PLAYER reference=%d,%d left=%d,%d right=%d,%d jump=%d,%d\n",
        reference_player.min_x, reference_player.min_y, left_player.min_x, left_player.min_y,
        right_player.min_x, right_player.min_y, jump_player.min_x, jump_player.min_y);
    if (left_diff == 0 || right_diff == 0 || jump_diff == 0 || left == right ||
        left == jump || right == jump || reference_player.pixels < 100 ||
        left_player.min_x >= reference_player.min_x ||
        right_player.min_x <= reference_player.min_x ||
        jump_player.min_y >= reference_player.min_y) {
        std::fprintf(stderr, "Manic Miner gameplay did not respond distinctly to Q/W/Shift\n");
        return 38;
    }

    // Capture a fresh CPC464-native v1 snapshot, then prove its framebuffer
    // and controls survive a reload before publishing it to the importer.
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(gameplay_state.data(), gameplay_state.size())) return 39;
    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return 40;
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 41;
    run_frames(50);
    const auto reloaded_hash = frame_hash();
    if (!write_ppm(evidence / "manic-miner-reloaded.ppm")) return 42;
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 42;
    run_frames(50);
    const auto repeated_reload_hash = frame_hash();
    const auto reload_frame = [&](int key, std::vector<std::uint8_t>& result) {
        cpcexplorer::init();
        if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return false;
        if (key != 0) cpcexplorer::key_down(key);
        run_frames(15);
        result = frame_copy();
        if (key != 0) cpcexplorer::key_up(key);
        run_frames(5);
        return true;
    };
    std::vector<std::uint8_t> reload_reference, reload_left_frame, reload_right_frame,
                              reload_jump_frame;
    if (!reload_frame(0, reload_reference) || !reload_frame('q', reload_left_frame) ||
        !reload_frame('w', reload_right_frame) ||
        !reload_frame(cpcexplorer::KeyShift, reload_jump_frame)) return 42;
    const auto reload_left = frame_difference(reload_reference, reload_left_frame);
    const auto reload_right = frame_difference(reload_reference, reload_right_frame);
    const auto reload_jump = frame_difference(reload_reference, reload_jump_frame);
    if (reloaded_hash != repeated_reload_hash || reload_left == 0 || reload_right == 0 ||
        reload_jump == 0) {
        std::fprintf(stderr, "native Manic Miner snapshot reload/control regression\n");
        return 42;
    }
    std::ofstream stream(output, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(snapshot.data()),
                 static_cast<std::streamsize>(snapshot.size()));
    if (!stream) return 43;
    std::printf("MANIC_SNAPSHOT path=%s bytes=%zu reload_a=%016llx reload_b=%016llx controls=%zu/%zu/%zu pc=%04x\n",
        output.string().c_str(), snapshot.size(),
        static_cast<unsigned long long>(reloaded_hash),
        static_cast<unsigned long long>(repeated_reload_hash), reload_left, reload_right,
        reload_jump, cpcexplorer::diagnostics().pc);
    return 0;
}

int probe_snapshot(const std::filesystem::path& snapshot_path,
                   const std::filesystem::path& evidence,
                   const std::vector<int>& start_keys = {}) {
    const auto snapshot = read_file(snapshot_path);
    std::filesystem::create_directories(evidence);
    const auto reach_test_state = [&snapshot, &start_keys]() {
        cpcexplorer::init();
        if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return false;
        if (!start_keys.empty()) {
            run_frames(20);
            for (const int key : start_keys) {
                cpcexplorer::key_down(key);
                run_frames(5);
                cpcexplorer::key_up(key);
                run_frames(25);
            }
            run_frames(50);
        }
        return true;
    };
    if (!reach_test_state()) return 50;
    run_frames(85);
    const auto neutral = frame_copy();
    const auto neutral_hash = frame_hash();
    if (!write_ppm(evidence / "neutral.ppm")) return 51;

    if (!reach_test_state()) return 52;
    run_frames(85);
    if (neutral_hash != frame_hash()) {
        std::fprintf(stderr, "snapshot progression is not deterministic\n");
        return 53;
    }

    std::vector<int> keys;
    for (int key = 'a'; key <= 'z'; ++key) keys.push_back(key);
    for (int key = '0'; key <= '9'; ++key) keys.push_back(key);
    keys.insert(keys.end(), {' ', 0x0d, cpcexplorer::KeyShift});
    for (const int key : keys) {
        if (!reach_test_state()) return 54;
        run_frames(50);
        cpcexplorer::key_down(key);
        run_frames(25);
        cpcexplorer::key_up(key);
        run_frames(10);
        const auto result = frame_copy();
        const auto difference = frame_difference(neutral, result);
        if (difference == 0) continue;
        char filename[32]{};
        std::snprintf(filename, sizeof(filename), "key-%02x.ppm", key & 0xff);
        if (!write_ppm(evidence / filename)) return 55;
        std::printf("SNA_KEY key=0x%02x difference=%zu hash=%016llx evidence=%s\n",
                    key & 0xff, difference,
                    static_cast<unsigned long long>(frame_hash()),
                    (evidence / filename).string().c_str());
    }
    std::printf("SNA_PROBE snapshot=%s bytes=%zu neutral=%016llx evidence=%s\n",
                snapshot_path.string().c_str(), snapshot.size(),
                static_cast<unsigned long long>(neutral_hash), evidence.string().c_str());
    return 0;
}

int build_amsdos_basic_binary(const std::filesystem::path& loader_path,
                              const std::filesystem::path& binary_path,
                              const std::filesystem::path& output,
                              const std::filesystem::path& evidence) {
    const auto loader = read_file(loader_path);
    const auto binary = read_file(binary_path);
    if (!valid_amsdos_header(loader, 0) || !valid_amsdos_header(binary, 2) ||
        !loader_runs_binary(loader, binary)) {
        std::fprintf(stderr, "invalid AMSDOS BASIC/binary pair\n");
        return 60;
    }
    std::filesystem::create_directories(evidence);
    cpcexplorer::init();
    run_seconds(5);
    if (!cpcexplorer::quickload(loader.data(), loader.size(), true)) return 61;
    type_text("RUN\r");
    run_seconds(1);
    if (!write_ppm(evidence / "loader.ppm")) return 62;

    // This is the reusable host equivalent of a BASIC loader's final RUN/LOAD
    // of one AMSDOS binary. cpc_quickload validates and applies the file's own
    // load and execution addresses; no title-specific RAM address is supplied.
    if (!cpcexplorer::quickload(binary.data(), binary.size(), true)) return 63;
    run_seconds(10);
    if (!write_ppm(evidence / "running.ppm")) return 64;

    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return 65;
    std::ofstream stream(output, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(snapshot.data()),
                 static_cast<std::streamsize>(snapshot.size()));
    if (!stream) return 66;

    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 67;
    run_frames(100);
    const auto first = frame_hash();
    if (!write_ppm(evidence / "reloaded.ppm")) return 68;
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 69;
    run_frames(100);
    const auto second = frame_hash();
    if (first != second) {
        std::fprintf(stderr, "captured snapshot progression is not deterministic\n");
        return 70;
    }
    std::printf("AMSDOS_PAIR loader=%s binary=%s load=0x%04x exec=0x%04x "
                "snapshot=%s bytes=%zu reload=%016llx evidence=%s\n",
                loader_path.string().c_str(), binary_path.string().c_str(),
                word(binary, 21), word(binary, 26), output.string().c_str(),
                snapshot.size(), static_cast<unsigned long long>(first),
                evidence.string().c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 4 && std::string_view(argv[1]) == "knight-tyme")
        return build_knight_tyme(argv[2], argv[3]);
    if (argc == 4 && std::string_view(argv[1]) == "usb-selftest")
        return build_usb_selftest(argv[2], argv[3]);
    if (argc == 5 && std::string_view(argv[1]) == "manic-miner")
        return build_manic_miner(argv[2], argv[3], argv[4]);
    if (argc == 4 && std::string_view(argv[1]) == "probe-sna")
        return probe_snapshot(argv[2], argv[3]);
    if (argc == 5 && std::string_view(argv[1]) == "probe-sna-start")
        return probe_snapshot(argv[2], argv[3], {std::stoi(argv[4], nullptr, 0)});
    if (argc == 6 && std::string_view(argv[1]) == "probe-sna-start2")
        return probe_snapshot(argv[2], argv[3], {std::stoi(argv[4], nullptr, 0),
                              std::stoi(argv[5], nullptr, 0)});
    if (argc >= 5 && std::string_view(argv[1]) == "probe-sna-sequence") {
        std::vector<int> keys;
        for (int i = 4; i < argc; ++i) keys.push_back(std::stoi(argv[i], nullptr, 0));
        return probe_snapshot(argv[2], argv[3], keys);
    }
    if (argc == 6 && std::string_view(argv[1]) == "amsdos-basic-bin")
        return build_amsdos_basic_binary(argv[2], argv[3], argv[4], argv[5]);
    std::fprintf(stderr,
        "usage:\n"
        "  cpc_snapshot_builder knight-tyme SOURCE_DIR OUTPUT_SNA\n"
        "  cpc_snapshot_builder usb-selftest OUTPUT_SNA OUTPUT_PPM\n"
        "  cpc_snapshot_builder manic-miner SOURCE_DIR OUTPUT_SNA EVIDENCE_DIR\n"
        "  cpc_snapshot_builder probe-sna INPUT_SNA EVIDENCE_DIR\n"
        "  cpc_snapshot_builder probe-sna-start INPUT_SNA EVIDENCE_DIR START_KEY\n"
        "  cpc_snapshot_builder probe-sna-start2 INPUT_SNA EVIDENCE_DIR START_KEY1 START_KEY2\n"
        "  cpc_snapshot_builder probe-sna-sequence INPUT_SNA EVIDENCE_DIR START_KEY...\n"
        "  cpc_snapshot_builder amsdos-basic-bin LOADER BASIC_BIN OUTPUT_SNA EVIDENCE_DIR\n");
    return 2;
}
