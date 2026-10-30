#include "cpc_core.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::uint32_t state_hash() {
    std::uint32_t value = 2166136261U;
    std::vector<std::uint8_t> snapshot(cpcexplorer::sna_size());
    if (!cpcexplorer::save_sna(snapshot.data(), snapshot.size())) return 0;
    for (const auto byte : snapshot) {
        value = (value ^ byte) * 16777619U;
    }
    return value;
}

std::uint32_t run_case(const std::vector<std::uint8_t>& snapshot,
                       bool joystick, int key) {
    cpcexplorer::select_engine(cpcexplorer::EngineKind::Reference);
    cpcexplorer::init();
    if (!cpcexplorer::load_sna(snapshot.data(), snapshot.size())) return 0;
    cpcexplorer::set_digital_joystick(joystick);
    if (key != 0) cpcexplorer::key_down(key);
    cpcexplorer::run_us(100'000);
    if (key != 0) cpcexplorer::key_up(key);
    cpcexplorer::run_us(100'000);
    return state_hash();
}

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

}  // namespace

int main() {
    const auto columns = read_file(CPCEXPLORER_STAGE4I7_COLUMNS_SNA);
    const auto thrust = read_file(CPCEXPLORER_STAGE4I7_THRUST_SNA);
    if (!expect(columns.size() == cpcexplorer::sna_size(), "Columns SNA size") ||
        !expect(thrust.size() == cpcexplorer::sna_size(), "Thrust SNA size")) return 1;

    const auto columns_neutral = run_case(columns, true, 0);
    for (const int key : {0x08, 0x09, 0x0A, 0x0B, 0x20}) {
        if (!expect(run_case(columns, true, key) != columns_neutral,
                    "Columns joystick action changes deterministic CPC state")) return 2;
    }

    const auto thrust_neutral = run_case(thrust, false, 0);
    constexpr int thrust_keys[] = {'z', 'x', 0x0D, 0x02, ' '};
    for (const int key : thrust_keys) {
        if (!expect(run_case(thrust, false, key) != thrust_neutral,
                    "Thrust key changes deterministic CPC state")) return 3;
    }

    std::puts("PASS: Columns joystick and Thrust five-key actions reach the actual CPC core");
    return 0;
}
